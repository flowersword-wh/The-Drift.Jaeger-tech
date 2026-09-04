use std::path::Path;

use crate::case::TestCase;
use crate::sandbox::Sandbox;
use crate::verification::{calculate_directory_hash, verify_files};

pub struct DefaultCase<'a> {
    sandbox: &'a Sandbox,
}

impl<'a> DefaultCase<'a> {
    pub fn new(sandbox: &'a Sandbox) -> Self {
        Self { sandbox }
    }
}

impl TestCase for DefaultCase<'_> {
    fn description(&self) -> &'static str {
        "Create the default server and client directories without modifying existing user data. This case provides an empty workspace for users to add their own C++ test inputs."
    }

    fn prepare(&self) -> std::io::Result<()> {
        self.sandbox.create_dir(Path::new("server"))?;
        self.sandbox.create_dir(Path::new("client"))?;
        Ok(())
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
