use std::path::Path;

use crate::case::TestCase;
use crate::sandbox::Sandbox;
use crate::verification::{calculate_directory_hash, verify_files};

const LARGE_FILE_SIZE: usize = 4 * 1024 * 1024;
const SOCKET_BUFFER_SIZE: usize = 256;

pub struct ComprehensiveCase<'a> {
    sandbox: &'a Sandbox,
}

impl<'a> ComprehensiveCase<'a> {
    pub fn new(sandbox: &'a Sandbox) -> Self {
        Self { sandbox }
    }

    fn patterned_data(size: usize) -> Vec<u8> {
        (0..size)
            .map(|index| ((index * 31 + index / 251) % 256) as u8)
            .collect()
    }
}

impl TestCase for ComprehensiveCase<'_> {
    fn description(&self) -> &'static str {
        "Synchronize a mixed directory tree containing large, binary, empty, buffer-boundary, nested, unchanged, and outdated files, then verify the complete result."
    }

    fn prepare(&self) -> std::io::Result<()> {
        for directory in [
            "client/large/archive",
            "client/binary/raw",
            "client/boundaries/empty",
            "client/boundaries/socket_buffer",
            "client/nested/level-1/level-2/level-3",
            "client/names/with spaces",
            "client/existing",
            "server/existing",
        ] {
            self.sandbox.create_dir(Path::new(directory))?;
        }

        self.sandbox.write_file(
            Path::new("client/large/archive/payload.bin"),
            Self::patterned_data(LARGE_FILE_SIZE),
        )?;
        self.sandbox.write_file(
            Path::new("client/binary/raw/all-bytes.bin"),
            Self::patterned_data(64 * 1024),
        )?;
        self.sandbox
            .write_file(Path::new("client/boundaries/empty/zero.bin"), [])?;
        self.sandbox
            .write_file(Path::new("client/boundaries/empty/one-byte.bin"), [0xff])?;

        for size in [
            SOCKET_BUFFER_SIZE - 1,
            SOCKET_BUFFER_SIZE,
            SOCKET_BUFFER_SIZE + 1,
        ] {
            self.sandbox.write_file(
                &Path::new("client/boundaries/socket_buffer").join(format!("{size}-bytes.bin")),
                Self::patterned_data(size),
            )?;
        }

        self.sandbox.write_file(
            Path::new("client/nested/level-1/level-2/level-3/deep.txt"),
            b"deeply nested text",
        )?;
        self.sandbox.write_file(
            Path::new("client/names/with spaces/mixed name [v1].txt"),
            b"path with spaces and punctuation",
        )?;

        self.sandbox.write_file(
            Path::new("client/existing/unchanged.bin"),
            b"already synchronized",
        )?;
        self.sandbox.write_file(
            Path::new("server/existing/unchanged.bin"),
            b"already synchronized",
        )?;
        self.sandbox.write_file(
            Path::new("client/existing/outdated.bin"),
            b"new client contents",
        )?;
        self.sandbox.write_file(
            Path::new("server/existing/outdated.bin"),
            b"old server contents",
        )
    }

    fn verify(
        &self,
        _sandbox: &Sandbox,
        server_dir: &Path,
        client_dir: &Path,
    ) -> std::io::Result<()> {
        verify_files(server_dir, client_dir)?;

        let server_hash = calculate_directory_hash(server_dir)?;
        let client_hash = calculate_directory_hash(client_dir)?;
        if server_hash != client_hash {
            return Err(std::io::Error::new(
                std::io::ErrorKind::InvalidData,
                format!("Directory hashes differ: server={server_hash}, client={client_hash}"),
            ));
        }

        Ok(())
    }

    fn clean(&self) -> std::io::Result<()> {
        Ok(())
    }
}
