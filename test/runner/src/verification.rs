use std::{collections::HashSet, fs, path::Path};

const EXCLUED_FILES: [&str; 4] = ["server.exe", "client.exe", "server.log", "client.log"];

fn is_excluded_file(file_name: &str) -> bool {
    EXCLUED_FILES.iter().any(|&f| f == file_name)
}

fn get_dir_entries(dir: &Path) -> Result<HashSet<std::path::PathBuf>, std::io::Error> {
    if !dir.is_dir() {
        return Err(std::io::Error::new(
            std::io::ErrorKind::NotADirectory,
            format!("{} is not a directory", dir.display()),
        ));
    }

    let mut entries = HashSet::new();
    collect_dir_entries(dir, Path::new(""), &mut entries)?;
    Ok(entries)
}

fn collect_dir_entries(
    dir: &Path,
    relative_dir: &Path,
    entries: &mut HashSet<std::path::PathBuf>,
) -> Result<(), std::io::Error> {
    for entry in fs::read_dir(dir)? {
        let entry = entry?;

        let file_name = entry.file_name().into_string().map_err(|_| {
            std::io::Error::new(
                std::io::ErrorKind::InvalidData,
                "file name is not valid UTF-8",
            )
        })?;

        if !is_excluded_file(&file_name) {
            let relative_path = relative_dir.join(&file_name);
            entries.insert(relative_path.clone());

            if entry.file_type()?.is_dir() {
                collect_dir_entries(&entry.path(), &relative_path, entries)?;
            }
        }
    }

    Ok(())
}

fn format_paths(paths: &[std::path::PathBuf]) -> String {
    paths
        .iter()
        .map(|path| path.display().to_string())
        .collect::<Vec<_>>()
        .join(", ")
}

pub fn verify_server_contains_client_files(
    server_sync_dir: &Path,
    client_sync_dir: &Path,
) -> std::io::Result<()> {
    let server_files = get_dir_entries(server_sync_dir)?;
    let client_files = get_dir_entries(client_sync_dir)?;

    let mut missing_files = client_files
        .difference(&server_files)
        .cloned()
        .collect::<Vec<_>>();
    missing_files.sort();

    if !missing_files.is_empty() {
        return Err(std::io::Error::new(
            std::io::ErrorKind::NotFound,
            format!(
                "Server is missing client files: {}",
                format_paths(&missing_files)
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

    let f_files = get_dir_entries(server_sync_dir)?;
    let s_files = get_dir_entries(client_sync_dir)?;

    let mut missing_files = f_files
        .symmetric_difference(&s_files)
        .cloned()
        .collect::<Vec<_>>();
    missing_files.sort();

    if !missing_files.is_empty() {
        return Err(std::io::Error::new(
            std::io::ErrorKind::NotFound,
            format!("Missing files: {}", format_paths(&missing_files)),
        ));
    }

    Ok(())
}
