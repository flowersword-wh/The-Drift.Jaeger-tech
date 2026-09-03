use std::{
    fs::File,
    io,
    path::{Path, PathBuf},
    process::{Child, Command, Stdio},
    time::Instant,
};

use crate::log_error;
use crate::process::terminate_child_until;

fn log_streams(path: &Path) -> io::Result<(Stdio, Stdio)> {
    let stdout = File::create(path)?;
    let stderr = stdout.try_clone()?;
    Ok((Stdio::from(stdout), Stdio::from(stderr)))
}

struct ExecutableProgram {
    /// Executable program name.
    name: String,
    stdout: Stdio,
    stderr: Stdio,
}

impl ExecutableProgram {
    fn new(name: String) -> Self {
        Self {
            name,
            stdout: Stdio::null(),
            stderr: Stdio::null(),
        }
    }

    fn prepare_stdio(&mut self, log_dir: &Path) -> Result<PathBuf, std::io::Error> {
        let log = log_dir.join(format!("{}.log", self.name));

        let (stdout, stderr) = match log_streams(&log) {
            Ok(streams) => streams,
            Err(error) => {
                log_error!(&format!(
                    "Unable to create {}-side log. {}：{error}",
                    self.name,
                    log.display()
                ));
                return Err(std::io::Error::new(
                    std::io::ErrorKind::Other,
                    "Stdio Error",
                ));
            }
        };

        self.stdout = stdout;
        self.stderr = stderr;

        Ok(log)
    }

    fn prepare_executable_program(
        self,
        project_path: &Path,
        sync_dir: &Path,
    ) -> Result<Child, std::io::Error> {
        let executable_program_dir = project_path.join(format!("test/{}_test", self.name));
        let executable_program = executable_program_dir.join(format!("{}.exe", self.name));

        match Command::new(&executable_program)
            .current_dir(&executable_program_dir)
            .stdin(Stdio::null())
            .stdout(self.stdout)
            .stderr(self.stderr)
            .arg(sync_dir) // By default, tests use the test directories located within the `test` folder.
            .spawn()
        {
            Ok(process) => Ok(process),
            Err(err) => {
                log_error!(&format!("Failed to run {}: {err}", self.name));
                return Err(err);
            }
        }
    }
}

pub struct PreparedProcesses {
    pub server: Child,
    pub client: Child,
    pub server_log: PathBuf,
    pub client_log: PathBuf,
}

fn prepare_log_dir(log_dir: &Path) -> std::io::Result<()> {
    if log_dir.exists() && !log_dir.is_dir() {
        log_error!(format!("{} is not a directory", log_dir.display()).as_str());
        return Err(std::io::Error::new(
            std::io::ErrorKind::NotADirectory,
            "log directory is not a directory",
        ));
    }
    if !log_dir.exists() {
        std::fs::create_dir_all(log_dir)?;
    }

    Ok(())
}

pub fn prepare(
    project_path: &Path,
    server_sync_dir: &Path,
    client_sync_dir: &Path,
    log_dir: &Path,
    cleanup_deadline: Instant,
) -> Result<PreparedProcesses, std::io::Error> {
    prepare_log_dir(log_dir)?;

    let mut server = ExecutableProgram::new("server".to_string());
    let mut client = ExecutableProgram::new("client".to_string());
    let server_log = server.prepare_stdio(log_dir)?;
    let client_log = client.prepare_stdio(log_dir)?;

    let mut server_child = server.prepare_executable_program(project_path, server_sync_dir)?;

    let client_child = match client.prepare_executable_program(project_path, client_sync_dir) {
        Ok(child) => child,
        Err(error) => {
            log_error!(&format!("Failed to start client; stopping server: {error}"));

            if let Err(cleanup_error) = terminate_child_until(&mut server_child, cleanup_deadline) {
                log_error!(&format!("Failed to clean up server: {cleanup_error}"));
            }

            return Err(error);
        }
    };

    Ok(PreparedProcesses {
        server: server_child,
        client: client_child,
        server_log,
        client_log,
    })
}
