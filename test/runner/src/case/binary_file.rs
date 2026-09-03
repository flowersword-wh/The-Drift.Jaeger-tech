use std::path::Path;

use crate::case::{TestCase, verify_file};
use crate::sandbox::Sandbox;

const IMAGE_DATA: &[u8] = &[
    0x89, b'P', b'N', b'G', 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, b'I', b'H', b'D', b'R',
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4,
    0x89, 0x00, 0x00, 0x00, 0x0d, b'I', b'D', b'A', b'T', 0x08, 0xd7, 0x63, 0xf8, 0xcf, 0xc0, 0x00,
    0x00, 0x03, 0x01, 0x01, 0x00, 0x18, 0xdd, 0x8d, 0xb0, 0x00, 0x00, 0x00, 0x00, b'I', b'E', b'N',
    b'D', 0xae, 0x42, 0x60, 0x82,
];

pub struct BinaryFileCase<'a> {
    sandbox: &'a Sandbox,
}

impl<'a> BinaryFileCase<'a> {
    pub fn new(sandbox: &'a Sandbox) -> Self {
        Self { sandbox }
    }

    fn large_binary_data() -> Vec<u8> {
        (0..(1024 * 1024))
            .map(|index| (index % 251) as u8)
            .collect()
    }
}

impl TestCase for BinaryFileCase<'_> {
    fn description(&self) -> &'static str {
        "Transfer image data, embedded zero and high-bit bytes, and a 1 MiB patterned payload, then compare all files byte-for-byte."
    }

    fn prepare(&self) -> std::io::Result<()> {
        self.sandbox
            .write_file(Path::new("client/sample.png"), IMAGE_DATA)?;
        self.sandbox.write_file(
            Path::new("client/bytes.bin"),
            [0x00, 0x01, 0x7f, 0x80, 0xfe, 0xff],
        )?;
        self.sandbox
            .write_file(Path::new("client/large.bin"), Self::large_binary_data())
    }

    fn verify(
        &self,
        _sandbox: &Sandbox,
        server_dir: &Path,
        client_dir: &Path,
    ) -> std::io::Result<()> {
        for name in ["sample.png", "bytes.bin", "large.bin"] {
            verify_file(self.sandbox, server_dir, client_dir, Path::new(name))?;
        }
        Ok(())
    }

    fn clean(&self) -> std::io::Result<()> {
        Ok(())
    }
}
