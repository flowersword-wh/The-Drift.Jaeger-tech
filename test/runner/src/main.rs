use std::path::PathBuf;
use std::process::ExitCode;

use clap::Parser;

mod case;
mod log;
mod prepare;
mod process;
mod runner;
mod sandbox;
mod verification;

pub(crate) const PROJECT_ROOT: &str = concat!(env!("CARGO_MANIFEST_DIR"), "/../..");

#[derive(Debug, Parser)]
#[command(author, version, about = "Run The-Drift integration test cases")]
struct Cli {
    /// Run one or more named test cases. Without this option, all cases run.
    #[arg(short, long, value_enum)]
    case: Vec<runner::CaseKind>,

    /// Enable debug logging.
    #[arg(short, long)]
    verbose: bool,
}

fn main() -> ExitCode {
    let cli = Cli::parse();
    log::LOGGER.set_verbose(cli.verbose);

    // Project root dir
    let project_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("../..")
        .canonicalize()
        .unwrap_or_else(|_| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../.."));

    runner::runner(&project_dir, &cli.case)
}
