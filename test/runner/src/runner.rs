use std::io;
use std::path::Path;
use std::process::{Child, ExitCode, ExitStatus};
use std::thread;
use std::time::{Duration, Instant};

use crate::case::TestCase;
use crate::case::default::DefaultCase;
use crate::log::LOGGER;
use crate::prepare::prepare;
use crate::sandbox::SandboxManager;
use crate::verification::verify_server_contains_client_files;

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

fn terminate_child(child: &mut Child) -> io::Result<()> {
    match child.try_wait() {
        Ok(Some(_)) => return Ok(()),
        Ok(None) => {}
        Err(wait_error) => {
            if let Err(kill_error) = child.kill() {
                LOGGER.error(&format!("Failed to terminate process after try_wait error: {wait_error}; kill failed: {kill_error}"));
                return Err(wait_error);
            }

            child.wait()?;
            return Err(wait_error);
        }
    }

    match child.kill() {
        Ok(()) => {
            child.wait()?;
            Ok(())
        }
        Err(kill_error) => match child.try_wait()? {
            Some(_) => Ok(()),
            None => Err(kill_error),
        },
    }
}

fn wait_with_timeout(child: &mut Child, deadline: Instant) -> io::Result<ProcessOutcome> {
    loop {
        match child.try_wait() {
            Ok(Some(status)) => return Ok(ProcessOutcome::Exited(status)),
            Ok(None) => {}
            Err(error) => {
                if let Err(cleanup_error) = terminate_child(child) {
                    LOGGER.error(&format!(
                        "Failed to clean up process after try_wait error: {cleanup_error}"
                    ));
                }
                return Err(error);
            }
        }

        if Instant::now() >= deadline {
            terminate_child(child)?;
            return Ok(ProcessOutcome::TimedOut);
        }

        thread::sleep(Duration::from_millis(25));
    }
}

fn report_log(name: &str, path: &Path) {
    match std::fs::read_to_string(path) {
        Ok(content) => LOGGER.info(&format!("{name} log ({}):\n{content}", path.display())),
        Err(error) => LOGGER.error(&format!(
            "Failed to read {name} log {}: {error}",
            path.display()
        )),
    }
}

pub fn runner(project_path: &Path) -> ExitCode {
    LOGGER.info("Running tests...");
    LOGGER.info(&format!("Project path：{}", project_path.display()));

    LOGGER.info("Initializing sandbox...");
    let mut manager = SandboxManager::new().expect("Failed to initialize sandbox manager");
    let sandbox = manager
        .open_or_create_sandbox("default")
        .expect("Failed to open or create default sandbox");

    let default_case = DefaultCase::new(sandbox);
    default_case
        .prepare()
        .expect("Failed to prepare default case");

    LOGGER.info("Preparing processes...");

    let server_sync_dir = sandbox
        .resolve_path(Path::new("server"))
        .expect("Invalid server directory");
    let client_sync_dir = sandbox
        .resolve_path(Path::new("client"))
        .expect("Invalid client directory");
    let process_deadline = Instant::now() + PROCESS_TIMEOUT;
    let processes = prepare(project_path, &server_sync_dir, &client_sync_dir)
        .expect("Failed to prepare processes");
    let mut server = processes.server;
    let mut client = processes.client;
    let server_log = processes.server_log;
    let client_log = processes.client_log;
    LOGGER.info("Prepared processes.");

    LOGGER.info("Accessing...");
    let client_status = wait_with_timeout(&mut client, process_deadline);
    if !matches!(&client_status, Ok(status) if status.success()) {
        if let Err(error) = terminate_child(&mut server) {
            LOGGER.error(&format!("Failed to clean up server process: {error}"));
        }
    }

    let server_status = if matches!(&client_status, Ok(status) if status.success()) {
        wait_with_timeout(&mut server, process_deadline)
    } else {
        Err(io::Error::other("Client failed; server has terminated."))
    };

    LOGGER.info(&format!("Client status：{client_status:?}"));
    LOGGER.info(&format!("Server status：{server_status:?}"));

    let processes_succeeded = matches!(client_status, Ok(status) if status.success())
        && matches!(server_status, Ok(status) if status.success());
    if !processes_succeeded {
        report_log("Server", &server_log);
        report_log("Client", &client_log);
        return ExitCode::FAILURE;
    }

    if let Err(error) = verify_server_contains_client_files(&server_sync_dir, &client_sync_dir) {
        LOGGER.error(&format!("Server containment verification failed: {error}"));
        report_log("Server", &server_log);
        report_log("Client", &client_log);
        return ExitCode::FAILURE;
    }

    // if let Err(error) = verify_files(&server_sync_dir, &client_sync_dir) {
    //     LOGGER.error(&format!("Verification failed: {error}"));
    //     report_log("Server", &server_log);
    //     report_log("Client", &client_log);
    //     return ExitCode::FAILURE;
    // }

    ExitCode::SUCCESS
}
