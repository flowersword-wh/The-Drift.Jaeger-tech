use std::path::PathBuf;
use std::process::ExitCode;

use crate::log::LOGGER;
use crate::verification::verify_files;

mod case;
mod log;
mod runner;
mod verification;

fn main() -> ExitCode {
    // Project root dir
    let project_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("../..")
        .canonicalize()
        .unwrap_or_else(|_| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../.."));

    runner::runner(&project_dir);

    if let Err(error) = verify_files(&project_dir) {
        LOGGER.error(&format!("Verification failed.：{error}"));
        return ExitCode::FAILURE;
    }

    LOGGER.info("Verification passed.");

    ExitCode::SUCCESS
}
