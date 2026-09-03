use std::io;
use std::path::Path;
use std::process::{Child, ExitCode, ExitStatus};
use std::thread;
use std::time::{Duration, Instant};

use crate::case::TestCase;
use crate::case::just_demo::JustDemo;
use crate::log::LOGGER;
use crate::prepare::prepare;
use crate::sandbox::SandboxManager;
use crate::verification::verify_files;

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

pub fn runner(project_path: &Path) -> ExitCode {
    LOGGER.info("Running tests...");
    LOGGER.info(&format!("Project path：{}", project_path.display()));

    LOGGER.info("Initializing sandbox...");
    let mut manager = SandboxManager::new().expect("Failed to initialize sandbox manager");
    let sandbox = manager
        .create_sandbox("just_demo")
        .expect("Failed to create sandbox");

    let demo = JustDemo::new(sandbox);
    demo.prepare().expect("Failed to prepare sandbox");

    LOGGER.info("Preparing processes...");

    let server_sync_dir = sandbox
        .resolve_path(Path::new("server"))
        .expect("Invalid server directory");
    let client_sync_dir = sandbox
        .resolve_path(Path::new("client"))
        .expect("Invalid client directory");
    let (mut server, mut client) = prepare(project_path, &server_sync_dir, &client_sync_dir)
        .expect("Failed to prepare processes");
    LOGGER.info("Prepared processes.");

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

    verify_files(&server_sync_dir, &client_sync_dir).expect("Failed to verify files.");

    if matches!(client_status, Ok(status) if status.success())
        && matches!(server_status, Ok(status) if status.success())
    {
        ExitCode::SUCCESS
    } else {
        ExitCode::FAILURE
    }
}
