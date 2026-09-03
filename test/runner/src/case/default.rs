use std::path::Path;

use crate::case::TestCase;
use crate::sandbox::Sandbox;

pub struct DefaultCase<'a> {
    sandbox: &'a Sandbox,
}

impl<'a> DefaultCase<'a> {
    pub fn new(sandbox: &'a Sandbox) -> Self {
        Self { sandbox }
    }
}

impl TestCase for DefaultCase<'_> {
    fn prepare(&self) -> std::io::Result<()> {
        self.sandbox.create_dir(Path::new("server"))?;
        self.sandbox.create_dir(Path::new("client"))?;
        Ok(())
    }

    fn clean(&self) -> std::io::Result<()> {
        Ok(())
    }
}
