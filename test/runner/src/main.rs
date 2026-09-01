use std::fs::{self, File};
use std::io::{self, Write};
use std::path::{Path, PathBuf};
use std::process::{Child, Command, ExitCode, ExitStatus, Stdio};
use std::thread;
use std::time::{Duration, Instant};

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

fn output_log(path: &Path) -> Option<String> {
    fs::read_to_string(path).ok()
}

fn create_demo_file(path: PathBuf) -> bool {
    fs::write(path, "hello world").is_ok()
}

fn log_streams(path: &Path) -> io::Result<(Stdio, Stdio)> {
    let stdout = File::create(path)?;
    let stderr = stdout.try_clone()?;

    Ok((Stdio::from(stdout), Stdio::from(stderr)))
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

fn main() -> ExitCode {
    // Project root dir
    let project_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("../..")
        .canonicalize()
        .unwrap_or_else(|_| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../.."));

    let server_dir = project_dir.join("test/server_test");
    let client_dir = project_dir.join("test/client_test");

    if !create_demo_file(client_dir.join("demo.txt")) {
        eprintln!("Create demo.txt failed");
        return ExitCode::FAILURE;
    }

    let server_exe = server_dir.join("server.exe");
    let client_exe = client_dir.join("client.exe");
    let server_log = server_dir.join("server.log");
    let client_log = client_dir.join("client.log");

    let (server_stdout, server_stderr) = match log_streams(&server_log) {
        Ok(streams) => streams,
        Err(error) => {
            eprintln!(
                "Unable to create server-side log. {}：{error}",
                server_log.display()
            );
            return ExitCode::FAILURE;
        }
    };

    let (client_stdout, client_stderr) = match log_streams(&client_log) {
        Ok(streams) => streams,
        Err(error) => {
            eprintln!(
                "Unable to create client log. {}：{error}",
                client_log.display()
            );
            return ExitCode::FAILURE;
        }
    };

    println!("server: {}", server_exe.display());
    println!("client: {}", client_exe.display());
    println!("server log: {}", server_log.display());
    println!("client log: {}", client_log.display());

    let mut server = match Command::new(&server_exe)
        .current_dir(&server_dir)
        .stdin(Stdio::piped())
        .stdout(server_stdout)
        .stderr(server_stderr)
        .spawn()
    {
        Ok(process) => process,
        Err(err) => {
            eprintln!("Failed to run server: {}", err);
            return ExitCode::FAILURE;
        }
    };

    if let Some(mut stdin) = server.stdin.take() {
        if let Err(error) = writeln!(stdin, "{}", project_dir.display()) {
            eprintln!("Failed to write the test directory to the server.：{error}");
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
        .spawn()
    {
        Ok(process) => process,
        Err(error) => {
            eprintln!("Failed to launch the client.：{error}");
            let _ = server.kill();
            let _ = server.wait();
            return ExitCode::FAILURE;
        }
    };

    println!("Accessing...");
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

    println!("Client status：{client_status:?}");
    println!("Server status：{server_status:?}");

    output_log(&server_log)
        .map(|log| println!("Server log：\n{log}"))
        .unwrap();
    output_log(&client_log)
        .map(|log| println!("Client log：\n{log}"))
        .unwrap();

    if matches!(client_status, Ok(status) if status.success())
        && matches!(server_status, Ok(status) if status.success())
    {
        ExitCode::SUCCESS
    } else {
        ExitCode::FAILURE
    }
}
