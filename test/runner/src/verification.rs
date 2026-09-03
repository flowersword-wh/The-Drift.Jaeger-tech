use std::{collections::HashSet, fs, path::Path};

use crate::log::LOGGER;

const EXCLUED_FILES: [&str; 4] = ["server.exe", "client.exe", "server.log", "client.log"];

fn output_log(path: &Path) -> Option<String> {
    fs::read_to_string(path).ok()
}

fn is_excluded_file(file_name: &str) -> bool {
    EXCLUED_FILES.iter().any(|&f| f == file_name)
}

fn get_dir_files_name(dir: &Path) -> Result<HashSet<String>, std::io::Error> {
    if !dir.is_dir() {
        return Err(std::io::Error::new(
            std::io::ErrorKind::NotADirectory,
            format!("{} is not a directory", dir.display()),
        ));
    }

    let mut files = HashSet::new();

    for entry in fs::read_dir(dir)? {
        let entry = entry?;

        let file_name = entry.file_name().into_string().map_err(|_| {
            std::io::Error::new(
                std::io::ErrorKind::InvalidData,
                "file name is not valid UTF-8",
            )
        })?;

        if !is_excluded_file(&file_name) {
            files.insert(file_name);
        }
    }

    Ok(files)
}

pub fn verify_files(server_sync_dir: &Path, client_sync_dir: &Path) -> std::io::Result<()> {
    if !server_sync_dir.is_dir() || !client_sync_dir.is_dir() {
        return Err(std::io::Error::new(
            std::io::ErrorKind::NotADirectory,
            format!(
                "{} or {} is not a directory",
                server_sync_dir.display(),
                client_sync_dir.display()
            ),
        ));
    }

    let f_files = get_dir_files_name(server_sync_dir)?;
    let s_files = get_dir_files_name(client_sync_dir)?;

    let missing_files = f_files
        .symmetric_difference(&s_files)
        .map(String::as_str)
        .collect::<Vec<_>>();

    if !missing_files.is_empty() {
        let server_log = server_sync_dir.join("server.log");
        let client_log = client_sync_dir.join("client.log");

        output_log(&server_log).map(|log| LOGGER.info(&format!("Server log：\n{log}")));
        output_log(&client_log).map(|log| LOGGER.info(&format!("Client log：\n{log}")));

        return Err(std::io::Error::new(
            std::io::ErrorKind::NotFound,
            format!("Missing files: {}", missing_files.join(", ")),
        ));
    }

    Ok(())
}
