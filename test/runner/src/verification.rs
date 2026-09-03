use std::{collections::HashSet, fs, path::Path};

const EXCLUED_FILES: [&str; 4] = ["server.exe", "client.exe", "server.log", "client.log"];

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

pub fn verify_server_contains_client_files(
    server_sync_dir: &Path,
    client_sync_dir: &Path,
) -> std::io::Result<()> {
    let server_files = get_dir_files_name(server_sync_dir)?;
    let client_files = get_dir_files_name(client_sync_dir)?;

    let missing_files = client_files
        .difference(&server_files)
        .cloned()
        .collect::<Vec<_>>();

    if !missing_files.is_empty() {
        return Err(std::io::Error::new(
            std::io::ErrorKind::NotFound,
            format!(
                "Server is missing client files: {}",
                missing_files.join(", ")
            ),
        ));
    }

    Ok(())
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
        return Err(std::io::Error::new(
            std::io::ErrorKind::NotFound,
            format!("Missing files: {}", missing_files.join(", ")),
        ));
    }

    Ok(())
}
