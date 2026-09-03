use std::io;
use std::process::Child;
use std::thread;
use std::time::{Duration, Instant};

use crate::log::LOGGER;

pub(crate) fn terminate_child_until(child: &mut Child, deadline: Instant) -> io::Result<()> {
    match child.try_wait() {
        Ok(Some(_)) => return Ok(()),
        Ok(None) => {}
        Err(wait_error) => {
            if let Err(kill_error) = child.kill() {
                LOGGER.error(&format!("Failed to terminate process after try_wait error: {wait_error}; kill failed: {kill_error}"));
                return Err(wait_error);
            }

            while Instant::now() < deadline {
                if child.try_wait()?.is_some() {
                    return Err(wait_error);
                }
                thread::sleep(Duration::from_millis(25));
            }

            return Err(io::Error::new(
                io::ErrorKind::TimedOut,
                "timed out waiting for child process termination",
            ));
        }
    }

    if let Err(error) = child.kill() {
        if child.try_wait()?.is_some() {
            return Ok(());
        }
        return Err(error);
    }

    while Instant::now() < deadline {
        if child.try_wait()?.is_some() {
            return Ok(());
        }
        thread::sleep(Duration::from_millis(25));
    }

    LOGGER.error("Timed out waiting for child process termination.");
    Err(io::Error::new(
        io::ErrorKind::TimedOut,
        "timed out waiting for child process termination",
    ))
}
