use std::sync::LazyLock;
use std::time::Instant;

const COLOR_TS: &str = "\x1b[90m";
const COLOR_INFO: &str = "\x1b[32m";
const COLOR_DEBUG: &str = "\x1b[1;34m";
const COLOR_WARN: &str = "\x1b[1;33m";
const COLOR_ERROR: &str = "\x1b[1;31m";
const COLOR_RESET: &str = "\x1b[0m";

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

    pub fn info(&self, content: &str) {
        self.write(COLOR_INFO, "INFO", content);
    }

    pub fn debug(&self, content: &str) {
        self.write(COLOR_DEBUG, "DEBUG", content);
    }

    pub fn warn(&self, content: &str) {
        self.write(COLOR_WARN, "WARN", content);
    }

    pub fn error(&self, content: &str) {
        self.write(COLOR_ERROR, "ERROR", content);
    }

    fn write(&self, color: &str, level: &str, content: &str) {
        let elapsed_ms = self.start_time.elapsed().as_secs_f64() * 1000.0;

        println!(
            "{COLOR_TS}[{:>8.3} ms] {COLOR_RESET}{color}[{level}] \
             {content}{COLOR_RESET}",
            elapsed_ms
        );
    }
}
