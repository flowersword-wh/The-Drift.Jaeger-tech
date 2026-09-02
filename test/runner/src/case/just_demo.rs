use std::fs;
use std::path::{Path, PathBuf};

use crate::log::LOGGER;

fn create_hello_demo_test(path: &Path) -> Result<(), String> {
    fs::write(path.join("demo.txt"), "hello world")
        .map_err(|error| format!("Create demo.txt failed: {error}"))?;

    Ok(())
}

fn create_just_demo_test(dir_path: &PathBuf) -> bool {
    if let Err(msg) = create_hello_demo_test(dir_path) {
        LOGGER.error(&msg);
        return false;
    }
    true
}
