use std::fs;
use std::io;
use std::path::{Component, Path, PathBuf};

use crate::{PROJECT_ROOT, log::LOGGER};

const SANDBOX_DIR: &str = "test/sandbox";

pub struct Sandbox {
    pub dir: PathBuf,
}

impl Sandbox {
    pub fn new() -> io::Result<Self> {
        let dir = PathBuf::from(PROJECT_ROOT).join(SANDBOX_DIR);

        fs::create_dir_all(&dir)?;

        Ok(Self {
            dir: dir.canonicalize()?,
        })
    }

    // only just allow a single level
    fn child_path(&self, name: &str) -> io::Result<PathBuf> {
        let mut components = Path::new(name).components();

        match (components.next(), components.next()) {
            (Some(Component::Normal(_)), None) => {}
            _ => {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidInput,
                    "invalid sandbox name",
                ));
            }
        }

        let path = self.dir.join(name);

        if !path.starts_with(&self.dir) {
            return Err(io::Error::new(
                io::ErrorKind::PermissionDenied,
                "sandbox path is outside the sandbox root",
            ));
        }

        Ok(path)
    }

    pub fn create(&self, name: &str) -> io::Result<()> {
        let dir = self.child_path(name)?;

        if dir.exists() {
            fs::remove_dir_all(&dir)?;
        }
        let server_dir = dir.join("server");
        let client_dir = dir.join("client");
        if server_dir.exists() {
            fs::remove_dir_all(&server_dir)?;
        }
        if client_dir.exists() {
            fs::remove_dir_all(&client_dir)?;
        }

        fs::create_dir(&dir)?;
        fs::create_dir(&server_dir)?;
        fs::create_dir(&client_dir)?;

        Ok(())
    }

    pub fn remove(&self, name: &str) -> io::Result<()> {
        let dir = self.child_path(name)?;

        if !dir.exists() {
            LOGGER.error("Sandbox does not exist.");

            return Err(io::Error::new(
                io::ErrorKind::NotFound,
                "sandbox does not exist",
            ));
        }

        let target = dir.canonicalize()?;

        if target == self.dir || !target.starts_with(&self.dir) {
            return Err(io::Error::new(
                io::ErrorKind::PermissionDenied,
                "refusing to remove path outside sandbox",
            ));
        }

        fs::remove_dir_all(target)?;
        Ok(())
    }
}
