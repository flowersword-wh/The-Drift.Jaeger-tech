use std::path::Path;

use crate::case::{TestCase, verify_file};
use crate::sandbox::Sandbox;

pub struct MultipleFilesCase<'a> {
    sandbox: &'a Sandbox,
}

impl<'a> MultipleFilesCase<'a> {
    pub fn new(sandbox: &'a Sandbox) -> Self {
        Self { sandbox }
    }
}

impl TestCase for MultipleFilesCase<'_> {
    fn description(&self) -> &'static str {
        "Transfer several files in one run, including a filename with spaces and punctuation, and verify each file independently."
    }

    fn prepare(&self) -> std::io::Result<()> {
        for (name, contents) in [
            ("multiple-1.txt", b"first".as_slice()),
            ("multiple-2.txt", b"second".as_slice()),
            ("multiple-3.txt", b"third".as_slice()),
            (
                "name with spaces (v1)-[test].txt",
                b"special filename".as_slice(),
            ),
        ] {
            self.sandbox
                .write_file(&Path::new("client").join(name), contents)?;
        }
        Ok(())
    }

    fn verify(
        &self,
        _sandbox: &Sandbox,
        server_dir: &Path,
        client_dir: &Path,
    ) -> std::io::Result<()> {
        for name in [
            "multiple-1.txt",
            "multiple-2.txt",
            "multiple-3.txt",
            "name with spaces (v1)-[test].txt",
        ] {
            verify_file(self.sandbox, server_dir, client_dir, Path::new(name))?;
        }
        Ok(())
    }

    fn clean(&self) -> std::io::Result<()> {
        Ok(())
    }
}
