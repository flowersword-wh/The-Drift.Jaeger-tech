use std::path::PathBuf;
use std::process::ExitCode;

mod case;
mod log;
mod prepare;
mod runner;
mod sandbox;
mod verification;

pub(crate) const PROJECT_ROOT: &str = concat!(env!("CARGO_MANIFEST_DIR"), "/../..");

fn main() -> ExitCode {
    // Project root dir
    let project_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("../..")
        .canonicalize()
        .unwrap_or_else(|_| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../.."));

    runner::runner(&project_dir)
}
