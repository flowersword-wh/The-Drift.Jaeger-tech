use std::{fs, path::Path};

use crate::sandbox::Sandbox;

pub mod binary_file;
pub mod default;
pub mod directory_transfer;
pub mod empty_file;
pub mod just_demo;
pub mod long_filename;
pub mod multiple_files;

pub(crate) fn verify_file(
    sandbox: &Sandbox,
    server_dir: &Path,
    client_dir: &Path,
    relative: &Path,
) -> std::io::Result<()> {
    let client_file = fs::read(sandbox.validate_path(&client_dir.join(relative))?)?;
    let server_file = fs::read(sandbox.validate_path(&server_dir.join(relative))?)?;
    if server_file != client_file {
        return Err(std::io::Error::new(
            std::io::ErrorKind::InvalidData,
            format!("File contents differ: {}", relative.display()),
        ));
    }
    Ok(())
}

pub trait TestCase {
    fn description(&self) -> &'static str;
    fn prepare(&self) -> std::io::Result<()>;
    fn verify(
        &self,
        _sandbox: &Sandbox,
        _server_dir: &Path,
        _client_dir: &Path,
    ) -> std::io::Result<()> {
        Ok(())
    }
    // fn run(&self, runner: &dyn Fn(&Path) -> std::io::Result<()>) -> std::io::Result<()>;
    fn clean(&self) -> std::io::Result<()>;
    // fn verify(&self, verify: &dyn Fn(&Path) -> std::io::Result<()>) -> std::io::Result<()>;
}
