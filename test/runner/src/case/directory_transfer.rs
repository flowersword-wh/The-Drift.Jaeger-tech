use std::path::Path;

use crate::case::{TestCase, verify_file};
use crate::sandbox::Sandbox;

pub struct DirectoryTransferCase<'a> {
    sandbox: &'a Sandbox,
}

impl<'a> DirectoryTransferCase<'a> {
    pub fn new(sandbox: &'a Sandbox) -> Self {
        Self { sandbox }
    }
}

impl TestCase for DirectoryTransferCase<'_> {
    fn description(&self) -> &'static str {
        "Place a file inside a nested client-side directory, transfer it recursively, and verify that its relative path and contents are preserved."
    }

    fn prepare(&self) -> std::io::Result<()> {
        self.sandbox.create_dir(Path::new("client/nested/deep"))?;
        self.sandbox.write_file(
            Path::new("client/nested/deep/inside.txt"),
            b"directory test",
        )?;
        Ok(())
    }

    fn verify(
        &self,
        _sandbox: &Sandbox,
        server_dir: &Path,
        client_dir: &Path,
    ) -> std::io::Result<()> {
        verify_file(
            self.sandbox,
            server_dir,
            client_dir,
            Path::new("nested/deep/inside.txt"),
        )
    }

    fn clean(&self) -> std::io::Result<()> {
        Ok(())
    }
}
