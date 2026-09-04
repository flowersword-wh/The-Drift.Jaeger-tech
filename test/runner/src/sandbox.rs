use std::fs;
use std::io;
use std::path::{Component, Path, PathBuf};

use crate::PROJECT_ROOT;

const SANDBOX_DIR: &str = "test/sandbox";

/// Reject links and reparse points.
fn reject_link(metadata: &fs::Metadata) -> io::Result<()> {
    let mut is_link = metadata.file_type().is_symlink();
    #[cfg(windows)]
    {
        use std::os::windows::fs::MetadataExt;
        // FILE_ATTRIBUTE_REPARSE_POINT also covers Windows junctions.
        is_link |= metadata.file_attributes() & 0x400 != 0;
    }
    if is_link {
        return Err(io::Error::new(
            io::ErrorKind::PermissionDenied,
            "links and reparse points are not allowed in sandbox paths",
        ));
    }
    Ok(())
}

pub struct SandboxManager {
    dir: PathBuf,
    created: Vec<Sandbox>,
}

pub struct Sandbox {
    name: String,
    dir: PathBuf,
}

impl SandboxManager {
    pub fn new() -> io::Result<Self> {
        Self::new_in(Path::new(PROJECT_ROOT))
    }

    /// Create a new sandbox in the given project directory
    fn new_in(project: &Path) -> io::Result<Self> {
        let mut dir = project.canonicalize()?;
        // Do not let a pre-existing test/ or sandbox/ junction redirect setup.
        for component in Path::new(SANDBOX_DIR).components() {
            dir.push(component);
            match fs::create_dir(&dir) {
                Ok(()) => {}
                Err(error) if error.kind() == io::ErrorKind::AlreadyExists => {}
                Err(error) => return Err(error),
            }
            let metadata = fs::symlink_metadata(&dir)?;
            reject_link(&metadata)?;
            if !metadata.is_dir() {
                return Err(io::Error::new(
                    io::ErrorKind::NotADirectory,
                    "invalid sandbox directory",
                ));
            }
        }

        Ok(Self {
            dir: dir.canonicalize()?,
            created: Vec::new(),
        })
    }

    /// Validate a relative path or an absolute descendant of the sandbox root.
    /// Missing components are allowed for creation; existing links are rejected.
    /// This is a path check, not protection against concurrent filesystem changes.
    pub fn validate_path(&self, path: &Path) -> io::Result<PathBuf> {
        validate_path(&self.dir, path)
    }

    // Only allow a single directory name.
    fn child_path(&self, name: &str) -> io::Result<PathBuf> {
        let mut components = Path::new(name).components();
        match (components.next(), components.next()) {
            (Some(Component::Normal(_)), None) => {}
            _ => {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidInput,
                    "invalid sandbox name",
                ));
            }
        }
        self.validate_path(Path::new(name))
    }

    /// Create a fresh test sandbox, replacing its previous contents if present.
    pub fn create_sandbox(&mut self, name: &str) -> io::Result<&Sandbox> {
        let dir = self.child_path(name)?;
        if dir.try_exists()? {
            reject_links_in_tree(&dir)?;
            fs::remove_dir_all(&dir)?;
        }
        // Discard an old record even if creating the replacement subsequently fails.
        self.created.retain(|sandbox| sandbox.dir != dir);
        fs::create_dir(&dir)?;
        let sandbox = Sandbox {
            name: name.to_owned(),
            dir: dir.canonicalize()?,
        };
        sandbox.create_dir(Path::new("server"))?;
        sandbox.create_dir(Path::new("client"))?;
        self.created.push(sandbox);
        Ok(self.created.last().unwrap())
    }

    /// Open an existing sandbox or create it without removing its contents.
    pub fn open_or_create_sandbox(&mut self, name: &str) -> io::Result<&Sandbox> {
        let dir = self.child_path(name)?;
        if dir.try_exists()? {
            let metadata = fs::symlink_metadata(&dir)?;
            reject_link(&metadata)?;
            if !metadata.is_dir() {
                return Err(io::Error::new(
                    io::ErrorKind::NotADirectory,
                    "sandbox path is not a directory",
                ));
            }
        } else {
            fs::create_dir(&dir)?;
        }

        self.created.retain(|sandbox| sandbox.dir != dir);
        let sandbox = Sandbox {
            name: name.to_owned(),
            dir: dir.canonicalize()?,
        };
        sandbox.create_dir(Path::new("server"))?;
        sandbox.create_dir(Path::new("client"))?;
        self.created.push(sandbox);
        Ok(self.created.last().unwrap())
    }

    pub fn get_sandbox(&self, name: &str) -> io::Result<&Sandbox> {
        let dir = self.child_path(name)?;
        let sandbox = self
            .created
            .iter()
            .find(|sandbox| sandbox.dir == dir)
            .ok_or_else(|| io::Error::new(io::ErrorKind::NotFound, "sandbox is not registered"))?;
        if !fs::metadata(&dir)?.is_dir() {
            return Err(io::Error::new(
                io::ErrorKind::NotADirectory,
                "invalid sandbox directory",
            ));
        }
        Ok(sandbox)
    }

    /// Removing the sandbox root is reserved for the manager.
    pub fn remove_sandbox(&mut self, name: &str) -> io::Result<()> {
        let dir = self.child_path(name)?;
        reject_links_in_tree(&dir)?;
        fs::remove_dir_all(&dir)?;
        self.created.retain(|sandbox| sandbox.dir != dir);
        Ok(())
    }
}

impl Sandbox {
    pub fn name(&self) -> &str {
        &self.name
    }

    /// Validate a path that must stay inside this sandbox.
    pub fn validate_path(&self, path: &Path) -> io::Result<PathBuf> {
        validate_path(&self.dir, path)
    }

    /// Resolve a relative descendant for external tools (e.g. the C++ programs).
    /// This does not restrict the external process's filesystem permissions.
    pub fn resolve_path(&self, relative: &Path) -> io::Result<PathBuf> {
        if relative.is_absolute() {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "sandbox operations require a relative path",
            ));
        }
        validate_path(&self.dir, relative)
    }

    /// Create a directory and missing parents inside this sandbox.
    pub fn create_dir(&self, relative: &Path) -> io::Result<()> {
        fs::create_dir_all(self.resolve_path(relative)?)
    }

    /// Create an isolated run directory by copying the input directories.
    pub fn create_run(&self, run_id: &str) -> io::Result<(PathBuf, PathBuf)> {
        let run_root = Path::new("runs").join(run_id);
        let server_run = run_root.join("server");
        let client_run = run_root.join("client");

        self.create_dir(&server_run)?;
        self.create_dir(&client_run)?;
        self.copy_contents(Path::new("server"), &server_run)?;
        self.copy_contents(Path::new("client"), &client_run)?;

        Ok((
            self.resolve_path(&server_run)?,
            self.resolve_path(&client_run)?,
        ))
    }

    /// Copy a directory's contents without following links.
    pub fn copy_contents(&self, source: &Path, destination: &Path) -> io::Result<()> {
        let source = self.resolve_path(source)?;
        let destination = self.resolve_path(destination)?;
        copy_directory_contents(&source, &destination)
    }

    /// Create or overwrite a file. Its parent directory must already exist.
    pub fn write_file(&self, relative: &Path, contents: impl AsRef<[u8]>) -> io::Result<()> {
        fs::write(self.resolve_path(relative)?, contents)
    }

    pub fn read_file(&self, relative: &Path) -> io::Result<Vec<u8>> {
        fs::read(self.resolve_path(relative)?)
    }

    pub fn read_to_string(&self, relative: &Path) -> io::Result<String> {
        fs::read_to_string(self.resolve_path(relative)?)
    }

    /// A missing path returns false; invalid paths and permission errors fail.
    pub fn exists(&self, relative: &Path) -> io::Result<bool> {
        self.resolve_path(relative)?.try_exists()
    }

    /// Return sorted paths relative to this sandbox, rejecting linked entries.
    pub fn read_dir(&self, relative: &Path) -> io::Result<Vec<PathBuf>> {
        let dir = self.resolve_path(relative)?;
        let mut entries = Vec::new();
        for entry in fs::read_dir(dir)? {
            let target = validate_path(&self.dir, &entry?.path())?;
            entries.push(
                target
                    .strip_prefix(&self.dir)
                    .map_err(|_| {
                        io::Error::new(io::ErrorKind::PermissionDenied, "entry is outside sandbox")
                    })?
                    .to_path_buf(),
            );
        }
        entries.sort();
        Ok(entries)
    }

    /// Copy a file within this sandbox, overwriting the destination if present.
    pub fn copy_file(&self, source: &Path, destination: &Path) -> io::Result<u64> {
        let source = self.resolve_path(source)?;
        let destination = self.resolve_path(destination)?;
        if source == destination {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "source and destination are identical",
            ));
        }
        fs::copy(source, destination)
    }

    /// Move a file or directory within this sandbox. Destination must not exist.
    pub fn rename(&self, source: &Path, destination: &Path) -> io::Result<()> {
        let source = self.resolve_path(source)?;
        let destination = self.resolve_path(destination)?;
        reject_links_in_tree(&source)?;
        if destination.try_exists()? {
            return Err(io::Error::new(
                io::ErrorKind::AlreadyExists,
                "rename destination already exists",
            ));
        }
        fs::rename(source, destination)
    }

    pub fn remove_file(&self, relative: &Path) -> io::Result<()> {
        fs::remove_file(self.resolve_path(relative)?)
    }

    /// Recursively remove a descendant directory; the sandbox root is forbidden.
    pub fn remove_dir(&self, relative: &Path) -> io::Result<()> {
        let target = self.resolve_path(relative)?;
        reject_links_in_tree(&target)?;
        fs::remove_dir_all(target)
    }
}

fn copy_directory_contents(source: &Path, destination: &Path) -> io::Result<()> {
    reject_links_in_tree(source)?;
    fs::create_dir_all(destination)?;

    for entry in fs::read_dir(source)? {
        let entry = entry?;
        let metadata = fs::symlink_metadata(entry.path())?;
        reject_link(&metadata)?;
        let target = destination.join(entry.file_name());

        if metadata.is_dir() {
            copy_directory_contents(&entry.path(), &target)?;
        } else if metadata.is_file() {
            fs::copy(entry.path(), target)?;
        } else {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "unsupported filesystem entry in sandbox",
            ));
        }
    }

    Ok(())
}

// Check all descendants before recursive operations, not only the target itself.
fn reject_links_in_tree(path: &Path) -> io::Result<()> {
    let metadata = fs::symlink_metadata(path)?;
    reject_link(&metadata)?;
    if metadata.is_dir() {
        for entry in fs::read_dir(path)? {
            reject_links_in_tree(&entry?.path())?;
        }
    }
    Ok(())
}

fn validate_path(root_path: &Path, path: &Path) -> io::Result<PathBuf> {
    reject_link(&fs::symlink_metadata(root_path)?)?;
    let root = root_path.canonicalize()?;
    if root != root_path {
        return Err(io::Error::new(
            io::ErrorKind::PermissionDenied,
            "sandbox root has moved",
        ));
    }
    let relative = if path.is_absolute() {
        path.strip_prefix(&root).map_err(|_| {
            io::Error::new(io::ErrorKind::PermissionDenied, "path is outside sandbox")
        })?
    } else {
        path
    };
    let mut target = root.clone();
    for component in relative.components() {
        let Component::Normal(name) = component else {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "path must contain only normal components",
            ));
        };
        #[cfg(windows)]
        {
            let name = name.to_string_lossy();
            if name.contains(':') || name.ends_with('.') || name.ends_with(' ') {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidInput,
                    "ambiguous Windows path component",
                ));
            }
        }
        target.push(name);
        match fs::symlink_metadata(&target) {
            Ok(metadata) => {
                reject_link(&metadata)?;
                target = target.canonicalize()?;
                if target == root || !target.starts_with(&root) {
                    return Err(io::Error::new(
                        io::ErrorKind::PermissionDenied,
                        "resolved path is outside sandbox",
                    ));
                }
            }
            Err(error) if error.kind() == io::ErrorKind::NotFound => {}
            Err(error) => return Err(error),
        }
    }
    if target == root {
        return Err(io::Error::new(
            io::ErrorKind::PermissionDenied,
            "sandbox root cannot be a target",
        ));
    }
    Ok(target)
}
