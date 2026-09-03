use std::path::Path;

use crate::case::{TestCase, verify_file};
use crate::sandbox::Sandbox;

pub struct LongFilenameCase<'a> {
    sandbox: &'a Sandbox,
}

impl<'a> LongFilenameCase<'a> {
    pub fn new(sandbox: &'a Sandbox) -> Self {
        Self { sandbox }
    }

    fn long_file_name() -> String {
        format!("{}.txt", "a".repeat(240))
    }

    fn near_limit_file_name() -> String {
        format!("{}.txt", "b".repeat(250))
    }
}

impl TestCase for LongFilenameCase<'_> {
    fn description(&self) -> &'static str {
        "Transfer files with long names, including a filename close to the platform component-length limit, and verify their contents."
    }

    fn prepare(&self) -> std::io::Result<()> {
        self.sandbox.write_file(
            &Path::new("client").join(Self::long_file_name()),
            b"long filename",
        )?;
        self.sandbox.write_file(
            &Path::new("client").join(Self::near_limit_file_name()),
            b"near limit filename",
        )
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
            Path::new(&Self::long_file_name()),
        )?;
        verify_file(
            self.sandbox,
            server_dir,
            client_dir,
            Path::new(&Self::near_limit_file_name()),
        )
    }

    fn clean(&self) -> std::io::Result<()> {
        Ok(())
    }
}
