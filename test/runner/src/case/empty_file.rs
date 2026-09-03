use std::path::Path;

use crate::case::{TestCase, verify_file};
use crate::sandbox::Sandbox;

pub struct EmptyFileCase<'a> {
    sandbox: &'a Sandbox,
}

impl<'a> EmptyFileCase<'a> {
    pub fn new(sandbox: &'a Sandbox) -> Self {
        Self { sandbox }
    }
}

impl TestCase for EmptyFileCase<'_> {
    fn description(&self) -> &'static str {
        "Transfer a zero-byte file and verify that the server creates it without adding or losing data."
    }

    fn prepare(&self) -> std::io::Result<()> {
        self.sandbox.write_file(Path::new("client/empty.bin"), [])
    }

    fn verify(
        &self,
        _sandbox: &Sandbox,
        server_dir: &Path,
        client_dir: &Path,
    ) -> std::io::Result<()> {
        verify_file(self.sandbox, server_dir, client_dir, Path::new("empty.bin"))
    }

    fn clean(&self) -> std::io::Result<()> {
        Ok(())
    }
}
