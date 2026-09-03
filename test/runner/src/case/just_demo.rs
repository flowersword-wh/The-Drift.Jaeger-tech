use std::path::Path;

use crate::case::TestCase;
use crate::log::LOGGER;
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
    fn prepare(&self) -> std::io::Result<()> {
        self.sandbox
            .write_file(Path::new("client/demo.txt"), b"hello world")?;
        LOGGER.info(&format!("Prepared {}", self.sandbox.name()));
        Ok(())
    }

    fn clean(&self) -> std::io::Result<()> {
        self.sandbox.remove_file(Path::new("client/demo.txt"))
    }
}
