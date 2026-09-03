use std::path::Path;

use crate::case::TestCase;
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
        "Place a file inside a client-side directory and confirm that directory transfer is currently rejected by the server containment verification. This is an expected failure until directory transfer support is implemented."
    }

    fn prepare(&self) -> std::io::Result<()> {
        self.sandbox.create_dir(Path::new("client/nested"))?;
        self.sandbox
            .write_file(Path::new("client/nested/inside.txt"), b"directory test")?;
        Ok(())
    }

    fn clean(&self) -> std::io::Result<()> {
        Ok(())
    }
}
