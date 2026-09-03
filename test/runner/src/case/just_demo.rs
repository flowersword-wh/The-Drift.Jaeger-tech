use std::path::Path;

use crate::case::TestCase;
use crate::log_info;
use crate::sandbox::Sandbox;

pub struct JustDemo<'a> {
    sandbox: &'a Sandbox,
}

impl<'a> JustDemo<'a> {
    pub fn new(sandbox: &'a Sandbox) -> Self {
        JustDemo { sandbox }
    }
}

impl TestCase for JustDemo<'_> {
    fn description(&self) -> &'static str {
        "Create one small text file on the client, transfer it to the server, and verify that the server receives the same file contents."
    }

    fn prepare(&self) -> std::io::Result<()> {
        self.sandbox
            .write_file(Path::new("client/demo.txt"), b"hello world")?;
        log_info!(&format!("Prepared {}", self.sandbox.name()));
        Ok(())
    }

    fn clean(&self) -> std::io::Result<()> {
        self.sandbox.remove_file(Path::new("client/demo.txt"))
    }
}
