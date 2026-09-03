use std::io;
use std::path::Path;
use std::process::{Child, ExitCode, ExitStatus};
use std::thread;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use crate::case::TestCase;
use crate::case::binary_file::BinaryFileCase;
use crate::case::default::DefaultCase;
use crate::case::directory_transfer::DirectoryTransferCase;
use crate::case::empty_file::EmptyFileCase;
use crate::case::just_demo::JustDemo;
use crate::case::long_filename::LongFilenameCase;
use crate::case::multiple_files::MultipleFilesCase;
use crate::prepare::prepare;
use crate::process::terminate_child_until;
use crate::sandbox::SandboxManager;
use crate::verification::verify_server_contains_client_files;
use crate::{log_error, log_info};

const PROCESS_TIMEOUT: Duration = Duration::from_secs(10);
const CLEANUP_TIMEOUT: Duration = Duration::from_secs(2);

#[derive(Clone, Copy)]
pub enum CaseKind {
    Default,
    JustDemo,
    EmptyFile,
    MultipleFiles,
    BinaryFile,
    LongFilename,
    DirectoryTransfer,
}

impl CaseKind {
    pub fn name(self) -> &'static str {
        match self {
            Self::DirectoryTransfer => "directory_transfer",
            Self::Default => "default",
            Self::JustDemo => "just_demo",
            Self::EmptyFile => "empty_file",
            Self::MultipleFiles => "multiple_files",
            Self::BinaryFile => "binary_file",
            Self::LongFilename => "long_filename",
        }
    }

    fn expected_success(self) -> bool {
        !matches!(self, Self::DirectoryTransfer)
    }
}

impl CaseKind {
    fn build<'a>(self, sandbox: &'a crate::sandbox::Sandbox) -> Box<dyn TestCase + 'a> {
        match self {
            Self::Default => Box::new(DefaultCase::new(sandbox)),
            Self::JustDemo => Box::new(JustDemo::new(sandbox)),
            Self::EmptyFile => Box::new(EmptyFileCase::new(sandbox)),
            Self::MultipleFiles => Box::new(MultipleFilesCase::new(sandbox)),
            Self::BinaryFile => Box::new(BinaryFileCase::new(sandbox)),
            Self::LongFilename => Box::new(LongFilenameCase::new(sandbox)),
            Self::DirectoryTransfer => Box::new(DirectoryTransferCase::new(sandbox)),
        }
    }
}

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

fn wait_with_timeout(child: &mut Child, deadline: Instant) -> io::Result<ProcessOutcome> {
    loop {
        match child.try_wait() {
            Ok(Some(status)) => return Ok(ProcessOutcome::Exited(status)),
            Ok(None) => {}
            Err(error) => {
                let cleanup_deadline = Instant::now() + CLEANUP_TIMEOUT;
                if let Err(cleanup_error) = terminate_child_until(child, cleanup_deadline) {
                    log_error!(&format!(
                        "Failed to clean up process after try_wait error: {cleanup_error}"
                    ));
                }
                return Err(error);
            }
        }

        if Instant::now() >= deadline {
            let cleanup_deadline = Instant::now() + CLEANUP_TIMEOUT;
            terminate_child_until(child, cleanup_deadline)?;
            return Ok(ProcessOutcome::TimedOut);
        }

        thread::sleep(Duration::from_millis(25));
    }
}

fn create_run_id() -> io::Result<String> {
    let timestamp = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_err(io::Error::other)?
        .as_nanos();
    Ok(format!("{timestamp}-{}", std::process::id()))
}

fn report_log(name: &str, path: &Path) {
    match std::fs::read_to_string(path) {
        Ok(content) => log_info!(&format!("{name} log ({}):\n{content}", path.display())),
        Err(error) => log_error!(&format!(
            "Failed to read {name} log {}: {error}",
            path.display()
        )),
    }
}

fn report_case_description(description: &str) {
    log_error!(&format!("Test description: {description}"));
}

fn run_one(project_path: &Path, manager: &mut SandboxManager, kind: CaseKind) -> io::Result<bool> {
    let sandbox = match kind {
        CaseKind::Default => manager.open_or_create_sandbox(kind.name())?,
        CaseKind::JustDemo => manager.create_sandbox(kind.name())?,
        CaseKind::EmptyFile => manager.create_sandbox(kind.name())?,
        CaseKind::MultipleFiles => manager.create_sandbox(kind.name())?,
        CaseKind::BinaryFile => manager.create_sandbox(kind.name())?,
        CaseKind::LongFilename => manager.create_sandbox(kind.name())?,
        CaseKind::DirectoryTransfer => manager.create_sandbox(kind.name())?,
    };

    let case = kind.build(sandbox);
    let description = case.description();
    case.prepare()?;

    let run_id = create_run_id()?;
    log_info!(&format!("Run ID: {run_id}"));
    let (server_sync_dir, client_sync_dir) = sandbox.create_run(&run_id)?;
    let log_dir = project_path.join("logs").join(kind.name()).join(&run_id);
    let process_deadline = Instant::now() + PROCESS_TIMEOUT;
    let cleanup_deadline = Instant::now() + CLEANUP_TIMEOUT;
    let processes = prepare(
        project_path,
        &server_sync_dir,
        &client_sync_dir,
        &log_dir,
        cleanup_deadline,
    )?;
    let mut server = processes.server;
    let mut client = processes.client;
    let server_log = processes.server_log;
    let client_log = processes.client_log;

    let client_status = match wait_with_timeout(&mut client, process_deadline) {
        Ok(status) => status,
        Err(error) => {
            report_case_description(description);
            let cleanup_deadline = Instant::now() + CLEANUP_TIMEOUT;
            terminate_child_until(&mut server, cleanup_deadline).map_err(|cleanup_error| {
                log_error!(&format!(
                    "Failed to clean up server after client wait error in run {run_id}: {cleanup_error}"
                ));
                cleanup_error
            })?;
            return Err(error);
        }
    };
    if !client_status.success() {
        report_case_description(description);
        log_error!(&format!(
            "Client process failed in run {run_id}: {client_status:?}"
        ));
        let cleanup_deadline = Instant::now() + CLEANUP_TIMEOUT;
        terminate_child_until(&mut server, cleanup_deadline)?;
        report_log("Server", &server_log);
        report_log("Client", &client_log);
        return Ok(false);
    }

    let server_status = wait_with_timeout(&mut server, process_deadline)?;
    if !server_status.success() {
        report_case_description(description);
        log_error!(&format!(
            "Server process failed in run {run_id}: {server_status:?}"
        ));
        report_log("Server", &server_log);
        report_log("Client", &client_log);
        return Ok(false);
    }

    if let Err(error) = verify_server_contains_client_files(&server_sync_dir, &client_sync_dir) {
        report_case_description(description);
        log_error!(&format!(
            "Server containment verification failed in run {run_id}: {error}"
        ));
        report_log("Server", &server_log);
        report_log("Client", &client_log);
        return Ok(false);
    }

    if let Err(error) = case.verify(sandbox, &server_sync_dir, &client_sync_dir) {
        report_case_description(description);
        log_error!(&format!(
            "{} verification failed in run {run_id}: {error}",
            kind.name()
        ));
        report_log("Server", &server_log);
        report_log("Client", &client_log);
        return Ok(false);
    }

    case.clean()?;
    Ok(true)
}

pub fn runner(project_path: &Path) -> ExitCode {
    let mut manager = match SandboxManager::new() {
        Ok(manager) => manager,
        Err(error) => {
            log_error!(&format!("Sandbox initialization failed: {error}"));
            return ExitCode::FAILURE;
        }
    };

    let cases = [
        CaseKind::Default,
        CaseKind::JustDemo,
        CaseKind::EmptyFile,
        CaseKind::MultipleFiles,
        CaseKind::BinaryFile,
        CaseKind::LongFilename,
        CaseKind::DirectoryTransfer,
    ];
    let mut failed = 0;
    let mut expected_errors = 0;

    for kind in cases {
        log_info!(&format!("Starting case: {}", kind.name()));

        match run_one(project_path, &mut manager, kind) {
            Ok(actual_success) if actual_success == kind.expected_success() => {
                if actual_success {
                    log_info!(&format!("Passed: {}", kind.name()));
                } else {
                    expected_errors += 1;
                    log_error!(&format!("Expected failure: {}", kind.name()));
                }
            }
            Ok(actual_success) => {
                failed += 1;
                log_error!(&format!(
                    "Unexpected result for {} (actual success: {actual_success})",
                    kind.name()
                ));
            }
            Err(error) => {
                log_error!(&format!(
                    "Framework error while running {}: {error}",
                    kind.name()
                ));
                return ExitCode::FAILURE;
            }
        }
    }

    log_info!(&format!(
        "Execution completed: passed {}, expected errors {}, unexpected errors {}",
        cases.len() - failed - expected_errors,
        expected_errors,
        failed
    ));

    if failed == 0 {
        ExitCode::SUCCESS
    } else {
        ExitCode::FAILURE
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
