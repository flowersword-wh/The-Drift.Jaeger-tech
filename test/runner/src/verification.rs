use std::{
    collections::HashSet,
    fs::{self, File, FileType},
    io::{self, Read},
    path::{Path, PathBuf},
};

use sha2::{Digest, Sha256};

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

/// Calculate the SHA-256 hash of a single file and return it as lowercase hex.
pub fn calculate_file_hash(path: &Path) -> io::Result<String> {
    let mut file = fs::File::open(path)?;
    let mut hasher = Sha256::new();
    let mut buffer = [0u8; 64 * 1024];

    loop {
        let read = file.read(&mut buffer)?;
        if read == 0 {
            break;
        }
        hasher.update(&buffer[..read]);
    }

    Ok(format!("{:x}", hasher.finalize()))
}

/// Calculate a deterministic SHA-256 hash for a complete directory tree.
///
/// Relative paths, entry types, and file contents are included. Entries are
/// sorted before hashing so filesystem iteration order does not affect the
/// result. Symbolic links and other unsupported entry types are rejected.
pub fn calculate_directory_hash(dir: &Path) -> io::Result<String> {
    if !dir.is_dir() {
        return Err(io::Error::new(
            io::ErrorKind::NotADirectory,
            format!("{} is not a directory", dir.display()),
        ));
    }

    let mut entries = Vec::new();
    collect_hash_entries(dir, Path::new(""), &mut entries)?;
    entries.sort_by(|left, right| left.0.cmp(&right.0));

    let mut hasher = Sha256::new();
    for (relative_path, file_type) in entries {
        let normalized_path = relative_path.to_string_lossy().replace('\\', "/");
        update_hash_part(&mut hasher, normalized_path.as_bytes());

        if file_type.is_dir() {
            hasher.update([b'D']);
        } else if file_type.is_file() {
            hasher.update([b'F']);
            let file_path = dir.join(&relative_path);
            let mut file = File::open(file_path)?;
            let mut buffer = [0u8; 64 * 1024];
            loop {
                let read = file.read(&mut buffer)?;
                if read == 0 {
                    break;
                }
                hasher.update(&buffer[..read]);
            }
        } else {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                format!("unsupported filesystem entry: {}", relative_path.display()),
            ));
        }
    }

    Ok(format!("{:x}", hasher.finalize()))
}

fn collect_hash_entries(
    dir: &Path,
    relative_dir: &Path,
    entries: &mut Vec<(PathBuf, FileType)>,
) -> io::Result<()> {
    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        let relative_path = relative_dir.join(entry.file_name());
        let file_type = entry.file_type()?;
        entries.push((relative_path.clone(), file_type));

        if file_type.is_dir() {
            collect_hash_entries(&entry.path(), &relative_path, entries)?;
        }
    }

    Ok(())
}

fn update_hash_part(hasher: &mut Sha256, part: &[u8]) {
    hasher.update((part.len() as u64).to_le_bytes());
    hasher.update(part);
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

#[cfg(test)]
mod tests {
    use std::{
        fs,
        path::PathBuf,
        time::{SystemTime, UNIX_EPOCH},
    };

    use super::{calculate_directory_hash, calculate_file_hash};

    fn temporary_directory(name: &str) -> PathBuf {
        let suffix = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_nanos();
        std::env::temp_dir().join(format!("runner-verification-{name}-{suffix}"))
    }

    #[test]
    fn file_hash_matches_sha256() {
        let dir = temporary_directory("file");
        fs::create_dir_all(&dir).unwrap();
        let file = dir.join("empty");
        fs::write(&file, []).unwrap();

        assert_eq!(
            calculate_file_hash(&file).unwrap(),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
        );
        fs::remove_dir_all(dir).unwrap();
    }

    #[test]
    fn directory_hash_includes_nested_file_contents() {
        let dir = temporary_directory("directory");
        fs::create_dir_all(dir.join("nested")).unwrap();
        let file = dir.join("nested").join("value.txt");
        fs::write(&file, b"before").unwrap();
        let before = calculate_directory_hash(&dir).unwrap();

        fs::write(&file, b"after").unwrap();
        let after = calculate_directory_hash(&dir).unwrap();

        assert_ne!(before, after);
        fs::remove_dir_all(dir).unwrap();
    }
}
