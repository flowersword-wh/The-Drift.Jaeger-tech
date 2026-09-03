use chrono::Local;
use std::{
    fs::File,
    io,
    path::{Path, PathBuf},
    process::{Child, Command, Stdio},
};

use crate::log::LOGGER;

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

    fn prepare_stdio(&mut self, project_path: &Path) -> Result<PathBuf, std::io::Error> {
        let time = Local::now().format("%Y-%m-%d_%H-%M-%S").to_string();
        let log = project_path.join(format!("logs/{}-{time}.log", self.name));

        let (stdout, stderr) = match log_streams(&log) {
            Ok(streams) => streams,
            Err(error) => {
                LOGGER.error(&format!(
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
                LOGGER.error(&format!("Failed to run {}: {err}", self.name));
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

fn prepare_log_dir(project_path: &Path) -> std::io::Result<()> {
    let log_dir = project_path.join("logs");
    if log_dir.exists() && !log_dir.is_dir() {
        LOGGER.error(format!("{} is not a directory", log_dir.display()).as_str());
        return Err(std::io::Error::new(
            std::io::ErrorKind::NotADirectory,
            "log directory is not a directory",
        ));
    }
    if !log_dir.exists() {
        std::fs::create_dir(log_dir)?;
    }

    Ok(())
}

pub fn prepare(
    project_path: &Path,
    server_sync_dir: &Path,
    client_sync_dir: &Path,
) -> Result<PreparedProcesses, std::io::Error> {
    prepare_log_dir(project_path)?;

    let mut server = ExecutableProgram::new("server".to_string());
    let mut client = ExecutableProgram::new("client".to_string());
    let server_log = server.prepare_stdio(project_path)?;
    let client_log = client.prepare_stdio(project_path)?;

    let mut server_child = server.prepare_executable_program(project_path, server_sync_dir)?;

    let client_child = match client.prepare_executable_program(project_path, client_sync_dir) {
        Ok(child) => child,
        Err(error) => {
            LOGGER.error(&format!("Failed to start client; stopping server: {error}"));

            if let Err(kill_error) = server_child.kill() {
                LOGGER.error(&format!("Failed to stop server: {kill_error}"));
            }

            if let Err(wait_error) = server_child.wait() {
                LOGGER.error(&format!("Failed to reap server: {wait_error}"));
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
