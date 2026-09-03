use std::sync::LazyLock;
use std::time::Instant;

pub(crate) const COLOR_TS: &str = "\x1b[90m";
pub(crate) const COLOR_INFO: &str = "\x1b[32m";
pub(crate) const COLOR_DEBUG: &str = "\x1b[1;34m";
pub(crate) const COLOR_WARN: &str = "\x1b[1;33m";
pub(crate) const COLOR_ERROR: &str = "\x1b[1;31m";
pub(crate) const COLOR_RESET: &str = "\x1b[0m";

pub static LOGGER: LazyLock<Log> = LazyLock::new(Log::new);

pub struct Log {
    start_time: Instant,
}
impl Log {
    fn new() -> Self {
        Self {
            start_time: Instant::now(),
        }
    }

    pub(crate) fn write(&self, color: &str, level: &str, file: &str, content: &str) {
        let elapsed_ms = self.start_time.elapsed().as_secs_f64() * 1000.0;
        let file_name = file.rsplit(['/', '\\']).next().unwrap_or(file);
        let module = file_name.strip_suffix(".rs").unwrap_or(file_name);

        println!(
            "{COLOR_TS}[{:>8.3} ms] {COLOR_RESET}{color}[{level}] \
             [{module}] {content}{COLOR_RESET}",
            elapsed_ms,
        );
    }
}

#[macro_export]
macro_rules! log_info {
    ($content:expr) => {
        $crate::log::LOGGER.write($crate::log::COLOR_INFO, "INFO", file!(), $content)
    };
}

#[macro_export]
macro_rules! log_debug {
    ($content:expr) => {
        $crate::log::LOGGER.write($crate::log::COLOR_DEBUG, "DEBUG", file!(), $content)
    };
}

#[macro_export]
macro_rules! log_warn {
    ($content:expr) => {
        $crate::log::LOGGER.write($crate::log::COLOR_WARN, "WARN", file!(), $content)
    };
}

#[macro_export]
macro_rules! log_error {
    ($content:expr) => {
        $crate::log::LOGGER.write($crate::log::COLOR_ERROR, "ERROR", file!(), $content)
    };
}
