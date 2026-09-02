use chrono::Local;
use std::fs::File;
use std::io::{self, Write};
use std::path::Path;
use std::process::{Child, Command, ExitCode, ExitStatus, Stdio};
use std::thread;
use std::time::{Duration, Instant};

use crate::log::LOGGER;
use crate::sandbox::Sandbox;

const PROCESS_TIMEOUT: Duration = Duration::from_secs(10);

#[derive(Debug)]
enum ProcessOutcome {
    Exited(ExitStatus),
    TimedOut,
}

impl ProcessOutcome {
    fn success(&self) -> bool {
        matches!(self, Self::Exited(status) if status.success())
    }
}

fn wait_with_timeout(child: &mut Child, timeout: Duration) -> io::Result<ProcessOutcome> {
    let deadline = Instant::now() + timeout;

    loop {
        if let Some(status) = child.try_wait()? {
            return Ok(ProcessOutcome::Exited(status));
        }

        if Instant::now() >= deadline {
            let _ = child.kill();
            let _ = child.wait();
            return Ok(ProcessOutcome::TimedOut);
        }

        thread::sleep(Duration::from_millis(25));
    }
}

fn log_streams(path: &Path) -> io::Result<(Stdio, Stdio)> {
    let stdout = File::create(path)?;
    let stderr = stdout.try_clone()?;

    Ok((Stdio::from(stdout), Stdio::from(stderr)))
}

pub fn runner(project_path: &Path) -> ExitCode {
    LOGGER.info("Running tests...");
    LOGGER.info(&format!("Project path：{}", project_path.display()));

    LOGGER.info("Initializing sandbox...");
    let sandbox = Sandbox::new();
    let sandbox_dir = &sandbox.unwrap().dir;
    LOGGER.info(format!("Initialized Sandbox path：{}", sandbox_dir.display()).as_str());

    let server_dir = project_path.join("test/server_test");
    let client_dir = project_path.join("test/client_test");

    let server_exe = server_dir.join("server.exe");
    let client_exe = client_dir.join("client.exe");

    let time = Local::now().format("%Y-%m-%d_%H-%M-%S").to_string();
    let log_dir = project_path.join("logs");
    if !log_dir.exists() {
        std::fs::create_dir(log_dir).expect("Failed to create log directory");
    }
    let server_log = project_path.join(format!("logs/server-{time}.log"));
    let client_log = project_path.join(format!("logs/client-{time}.log"));

    let (server_stdout, server_stderr) = match log_streams(&server_log) {
        Ok(streams) => streams,
        Err(error) => {
            LOGGER.error(&format!(
                "Unable to create server-side log. {}：{error}",
                server_log.display()
            ));
            return ExitCode::FAILURE;
        }
    };

    let (client_stdout, client_stderr) = match log_streams(&client_log) {
        Ok(streams) => streams,
        Err(error) => {
            LOGGER.error(&format!(
                "Unable to create client log. {}：{error}",
                client_log.display()
            ));
            return ExitCode::FAILURE;
        }
    };

    LOGGER.info(&format!("server: {}", server_exe.display()));
    LOGGER.info(&format!("client: {}", client_exe.display()));
    LOGGER.info(&format!("server log: {}", server_log.display()));
    LOGGER.info(&format!("client log: {}", client_log.display()));

    let mut server = match Command::new(&server_exe)
        .current_dir(&server_dir)
        .stdin(Stdio::piped())
        .stdout(server_stdout)
        .stderr(server_stderr)
        .arg(".") // By default, tests use the test directories located within the `test` folder.
        .spawn()
    {
        Ok(process) => process,
        Err(err) => {
            LOGGER.error(&format!("Failed to run server: {err}"));
            return ExitCode::FAILURE;
        }
    };

    if let Some(mut stdin) = server.stdin.take() {
        if let Err(error) = writeln!(stdin, "{}", project_path.display()) {
            LOGGER.error(&format!(
                "Failed to write the test directory to the server.：{error}"
            ));
            let _ = server.kill();
            let _ = server.wait();
            return ExitCode::FAILURE;
        }
    }

    let mut client = match Command::new(&client_exe)
        .current_dir(&client_dir)
        .stdin(Stdio::null())
        .stdout(client_stdout)
        .stderr(client_stderr)
        .arg(".") // By default, tests use the test directories located within the `test` folder.
        .spawn()
    {
        Ok(process) => process,
        Err(error) => {
            LOGGER.error(&format!("Failed to launch the client.：{error}"));
            let _ = server.kill();
            let _ = server.wait();
            return ExitCode::FAILURE;
        }
    };

    LOGGER.info("Accessing...");
    let client_status = wait_with_timeout(&mut client, PROCESS_TIMEOUT);
    if !matches!(&client_status, Ok(status) if status.success()) {
        let _ = server.kill();
        let _ = server.wait();
    }

    let server_status = if matches!(&client_status, Ok(status) if status.success()) {
        wait_with_timeout(&mut server, PROCESS_TIMEOUT)
    } else {
        Err(io::Error::other("Client failed; server has terminated."))
    };

    LOGGER.info(&format!("Client status：{client_status:?}"));
    LOGGER.info(&format!("Server status：{server_status:?}"));

    if matches!(client_status, Ok(status) if status.success())
        && matches!(server_status, Ok(status) if status.success())
    {
        ExitCode::SUCCESS
    } else {
        ExitCode::FAILURE
    }
}
