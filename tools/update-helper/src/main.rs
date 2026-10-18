#![cfg_attr(windows, windows_subsystem = "windows")]

use ed25519_dalek::{Signature, VerifyingKey};
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::{
    collections::HashSet,
    env, fs,
    fs::File,
    io::{self, Read},
    path::{Component, Path, PathBuf},
    process::{self, Command},
};
#[cfg(unix)]
use std::{thread, time::{Duration, Instant}};

type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;
const MAX_PACKAGE: u64 = 2 * 1024 * 1024 * 1024;
const MAX_EXPANDED: u64 = 4 * MAX_PACKAGE;
const VERSION: &str = match option_env!("SWIFTIFY_RELEASE_VERSION") {
    Some(value) => value,
    None => "0.0.0",
};
const PUBLIC_KEY: &str = match option_env!("SWIFTIFY_UPDATE_PUBLIC_KEY") {
    Some(value) => value,
    None => "",
};

#[derive(Debug, Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
struct Manifest {
    schema: u32,
    version: String,
    target: String,
    archive: String,
    sha256: String,
    size: u64,
}

fn target() -> String {
    format!("{}-{}", env::consts::OS, env::consts::ARCH)
}

fn root_name() -> &'static str {
    if cfg!(target_os = "macos") { "Swiftify.app" } else { "Swiftify" }
}

fn executable() -> &'static str {
    if cfg!(target_os = "macos") {
        "Contents/MacOS/swiftify"
    } else if cfg!(windows) {
        "bin/swiftify.exe"
    } else {
        "bin/swiftify"
    }
}

fn hex<const N: usize>(text: &str) -> Result<[u8; N]> {
    if text.len() != N * 2 || !text.bytes().all(|b| b.is_ascii_hexdigit()) {
        return Err("Invalid hexadecimal data".into());
    }
    let mut result = [0; N];
    for (i, byte) in result.iter_mut().enumerate() {
        *byte = u8::from_str_radix(&text[i * 2..i * 2 + 2], 16)?;
    }
    Ok(result)
}

fn verify(data: &[u8], signature: &str, key: &str, current: &str, platform: &str) -> Result<Manifest> {
    if data.len() > 16384 { return Err("Update manifest is too large".into()); }
    let key = VerifyingKey::from_bytes(&hex::<32>(key)?)?;
    key.verify_strict(data, &Signature::from_bytes(&hex::<64>(signature.trim())?))?;
    let manifest: Manifest = serde_json::from_slice(data)?;
    let version = semver::Version::parse(&manifest.version)?;
    let current = semver::Version::parse(current)?;
    if manifest.schema != 1 || manifest.target != platform || !version.pre.is_empty()
        || !version.build.is_empty() || version <= current
        || manifest.archive != format!("swiftify-{}-{}.zip", manifest.version, platform)
        || manifest.size == 0 || manifest.size > MAX_PACKAGE
    {
        return Err("Update version, platform or package is not supported".into());
    }
    hex::<32>(&manifest.sha256)?;
    Ok(manifest)
}

fn inspect(work: &Path) -> Result<Manifest> {
    verify(&read_limited(&work.join("manifest.json"), 16384)?,
        &String::from_utf8(read_limited(&work.join("manifest.sig"), 256)?)?,
        PUBLIC_KEY, VERSION, &target())
}

fn read_limited(path: &Path, limit: u64) -> Result<Vec<u8>> {
    let file = File::open(path)?;
    if file.metadata()?.len() > limit { return Err("Update file is too large".into()); }
    let mut bytes = Vec::new();
    file.take(limit + 1).read_to_end(&mut bytes)?;
    if bytes.len() as u64 > limit { return Err("Update file is too large".into()); }
    Ok(bytes)
}

fn verify_package(path: &Path, manifest: &Manifest) -> Result<()> {
    let mut file = File::open(path)?;
    if file.metadata()?.len() != manifest.size { return Err("Update package size mismatch".into()); }
    let mut hash = Sha256::new();
    let mut buffer = [0; 65536];
    loop {
        let count = file.read(&mut buffer)?;
        if count == 0 { break; }
        hash.update(&buffer[..count]);
    }
    if hash.finalize().as_slice() != hex::<32>(&manifest.sha256)? {
        return Err("Update package checksum mismatch".into());
    }
    Ok(())
}

// Reject traversal, Windows aliases/streams and duplicate/case-folded names on
// every platform. Symlinks are created last, and may only point inside the root.
fn safe_path(name: &str) -> Result<PathBuf> {
    if name.is_empty() || name.contains(['\\', ':', '\0'])
        || name.split('/').any(|part| part.is_empty() || part == "." || part == "..")
    { return Err("Unsafe archive path".into()); }
    let path = Path::new(name);
    for part in path.components() {
        let Component::Normal(part) = part else { return Err("Unsafe archive path".into()); };
        let part = part.to_str().ok_or("Invalid archive filename")?;
        let stem = part.split('.').next().unwrap_or("").to_ascii_uppercase();
        if part.ends_with(['.', ' ']) || part.chars().any(|c| c.is_control())
            || ["CON", "PRN", "AUX", "NUL"].contains(&stem.as_str())
            || ((stem.starts_with("COM") || stem.starts_with("LPT"))
                && stem.len() == 4 && stem.as_bytes()[3].is_ascii_digit())
        { return Err("Unsafe archive filename".into()); }
    }
    Ok(path.to_path_buf())
}

fn link_stays_inside(path: &Path, destination: &str) -> bool {
    if destination.contains(['\\', ':', '\0']) || Path::new(destination).is_absolute() { return false; }
    let mut depth = path.parent().map_or(0, |p| p.components().count());
    for component in Path::new(destination).components() {
        match component {
            Component::Normal(_) => depth += 1,
            Component::CurDir => (),
            Component::ParentDir if depth > 1 => depth -= 1,
            _ => return false,
        }
    }
    depth >= 1
}

fn extract(package: &Path, directory: &Path) -> Result<PathBuf> {
    let mut archive = zip::ZipArchive::new(File::open(package)?)?;
    if archive.len() > 100_000 { return Err("Too many archive entries".into()); }
    let mut total = 0u64;
    let mut names = HashSet::new();
    let mut links = Vec::new();
    for i in 0..archive.len() {
        let mut entry = archive.by_index(i)?;
        let relative = safe_path(entry.name().trim_end_matches('/'))?;
        if relative.components().next() != Some(Component::Normal(root_name().as_ref())) {
            return Err("Unexpected archive root".into());
        }
        if !names.insert(relative.to_string_lossy().to_lowercase()) {
            return Err("Duplicate archive entry".into());
        }
        total = total.checked_add(entry.size()).ok_or("Archive size overflow")?;
        if total > MAX_EXPANDED { return Err("Expanded update is too large".into()); }
        let destination = directory.join(&relative);
        let mode = entry.unix_mode().unwrap_or(0o644);
        if mode & 0o170000 == 0o120000 {
            let mut text = String::new();
            entry.by_ref().take(4097).read_to_string(&mut text)?;
            if cfg!(windows) || text.len() > 4096 || !link_stays_inside(&relative, &text) {
                return Err("Unsafe archive symlink".into());
            }
            links.push((destination, text));
        } else if entry.is_dir() {
            fs::create_dir_all(&destination)?;
        } else {
            if mode & 0o170000 != 0 && mode & 0o170000 != 0o100000 {
                return Err("Unsupported archive entry".into());
            }
            fs::create_dir_all(destination.parent().ok_or("Missing parent")?)?;
            let mut output = fs::OpenOptions::new().write(true).create_new(true).open(&destination)?;
            let expected = entry.size();
            if io::copy(&mut entry.by_ref().take(expected + 1), &mut output)? != expected {
                return Err("Archive entry size mismatch".into());
            }
            output.sync_all()?;
            #[cfg(unix)] {
                use std::os::unix::fs::PermissionsExt;
                fs::set_permissions(&destination, fs::Permissions::from_mode(mode & 0o755))?;
            }
        }
    }
    // No symlink may be an ancestor of another entry. In particular, creating
    // a later symlink must never follow an earlier one outside the staging tree.
    for (path, _) in &links {
        let relative = path.strip_prefix(directory)?.to_string_lossy().to_lowercase() + "/";
        if names.iter().any(|name| name.starts_with(&relative)) {
            return Err("Archive symlink is a parent of another entry".into());
        }
    }
    #[cfg(unix)]
    for (path, text) in &links {
        fs::create_dir_all(path.parent().ok_or("Missing symlink parent")?)?;
        std::os::unix::fs::symlink(text, path)?;
    }
    let root = directory.join(root_name());
    let canonical_root = root.canonicalize()?;
    for (path, _) in &links {
        if !path.canonicalize()?.starts_with(&canonical_root) {
            return Err("Archive symlink escapes the installation".into());
        }
    }
    validate_installation(&root)?;
    if !root.join(executable()).is_file() { return Err("Update executable is missing".into()); }
    Ok(root)
}

fn validate_installation(root: &Path) -> Result<()> {
    if fs::symlink_metadata(root)?.file_type().is_symlink() || !root.is_dir() {
        return Err("Installation must be a real directory".into());
    }
    let marker: serde_json::Value = serde_json::from_slice(&read_limited(&root.join(".swiftify-portable.json"), 1024)?)?;
    if marker["product"] != "Swiftify" || marker["format"] != 1 || marker["target"] != target() {
        return Err("Only official portable installations support in-app updates".into());
    }
    Ok(())
}

#[cfg(unix)]
fn wait_for_exit(pid: u32) -> Result<()> {
    if pid < 2 || pid > i32::MAX as u32 { return Err("Invalid application process".into()); }
    let deadline = Instant::now() + Duration::from_secs(120);
    loop {
        if unsafe { libc::kill(pid as i32, 0) } != 0 {
            if io::Error::last_os_error().raw_os_error() == Some(libc::ESRCH) { return Ok(()); }
            return Err("Cannot inspect application process".into());
        }
        if Instant::now() > deadline { return Err("Application did not exit; installation unchanged".into()); }
        thread::sleep(Duration::from_millis(100));
    }
}

#[cfg(windows)]
fn wait_for_exit(pid: u32) -> Result<()> {
    use windows_sys::Win32::{Foundation::{CloseHandle, GetLastError, ERROR_INVALID_PARAMETER, WAIT_OBJECT_0}, System::Threading::{OpenProcess, WaitForSingleObject, PROCESS_SYNCHRONIZE}};
    if pid < 2 { return Err("Invalid application process".into()); }
    let handle = unsafe { OpenProcess(PROCESS_SYNCHRONIZE, 0, pid) };
    if handle.is_null() {
        // The approved application may have exited between writing readiness
        // and opening its handle. Other errors (especially access denied) fail.
        if unsafe { GetLastError() } == ERROR_INVALID_PARAMETER { return Ok(()); }
        return Err("Cannot open application process".into());
    }
    let result = unsafe { WaitForSingleObject(handle, 120_000) };
    unsafe { CloseHandle(handle); }
    if result != WAIT_OBJECT_0 { return Err("Application did not exit; installation unchanged".into()); }
    Ok(())
}

fn launch(root: &Path) -> Result<()> {
    #[cfg(target_os = "macos")]
    {
        if !Command::new("/usr/bin/open").arg("-n").arg(root).status()?.success() {
            return Err("macOS refused to launch the updated application".into());
        }
    }
    #[cfg(not(target_os = "macos"))]
    { Command::new(root.join(executable())).current_dir(root).spawn()?; }
    Ok(())
}

fn replace_and_launch(destination: &Path, replacement: &Path, backup: &Path,
    launch_app: impl Fn(&Path) -> Result<()>) -> Result<()> {
    if let Err(error) = fs::rename(destination, backup) {
        let _ = launch_app(destination);
        return Err(error.into());
    }
    if let Err(error) = fs::rename(replacement, destination) {
        fs::rename(backup, destination)?;
        let _ = launch_app(destination);
        return Err(error.into());
    }
    if let Err(error) = launch_app(destination) {
        fs::rename(destination, replacement)?;
        fs::rename(backup, destination)?;
        let _ = launch_app(destination);
        return Err(error);
    }
    Ok(())
}

fn install(work: &Path, destination: &Path, pid: u32) -> Result<()> {
    let manifest = inspect(work)?;
    let package = work.join("package.zip");
    verify_package(&package, &manifest)?;
    validate_installation(destination)?;
    let destination = destination.canonicalize()?;
    let parent = destination.parent().ok_or("Installation has no parent")?;
    // Same-volume staging/backup; no elevation, shell execution or deletion of
    // arbitrary installation paths. System-managed installations have no marker.
    let stage = tempfile::Builder::new().prefix(".swiftify-update-").tempdir_in(parent)?;
    let replacement = extract(&package, stage.path())?;
    #[cfg(target_os = "macos")]
    if !Command::new("/usr/bin/codesign").args(["--verify", "--deep", "--strict"]).arg(&replacement).status()?.success() {
        return Err("The downloaded macOS bundle has an invalid code signature".into());
    }
    let backup = stage.path().join("previous");
    fs::write(work.join("ready"), b"ready")?;
    wait_for_exit(pid)?;
    if read_limited(&work.join("approved"), 32)? != b"approved" {
        return Err("Application did not approve installation; installation unchanged".into());
    }
    if let Err(error) = replace_and_launch(&destination, &replacement, &backup, launch) {
        // If rollback itself fails, never delete the remaining backup.
        if backup.exists() { let _ = stage.keep(); }
        return Err(error);
    }
    // Keep the previous version for manual recovery; starting a process is not
    // proof that the new application's startup completed successfully.
    let saved = stage.keep();
    fs::write(work.join("result.json"), serde_json::to_vec(&serde_json::json!({
        "installed": manifest.version, "backup": saved.join("previous")
    }))?)?;
    Ok(())
}

fn run() -> Result<()> {
    let arguments: Vec<_> = env::args_os().collect();
    let command = arguments.get(1).and_then(|v| v.to_str()).ok_or("Missing command")?;
    let work = PathBuf::from(arguments.get(2).ok_or("Missing update directory")?);
    match command {
        "inspect" if arguments.len() == 3 => println!("{}", serde_json::to_string(&inspect(&work)?)?),
        "verify" if arguments.len() == 3 => verify_package(&work.join("package.zip"), &inspect(&work)?)?,
        "install" if arguments.len() == 5 => {
            let result = install(&work, Path::new(&arguments[3]), arguments[4].to_str().ok_or("Invalid PID")?.parse()?);
            if let Err(error) = &result { let _ = fs::write(work.join("error.txt"), error.to_string()); }
            result?;
        }
        _ => return Err("Invalid updater command".into()),
    }
    Ok(())
}

fn main() {
    if let Err(error) = run() { eprintln!("{error}"); process::exit(1); }
}

#[cfg(test)]
mod tests {
    use super::*;
    use ed25519_dalek::{Signer, SigningKey};
    use std::io::Write;

    fn encoded(bytes: &[u8]) -> String { bytes.iter().map(|b| format!("{b:02x}")).collect() }

    #[test]
    fn signature_platform_and_rollback_are_checked() {
        let key = SigningKey::from_bytes(&[42; 32]);
        let manifest = Manifest { schema: 1, version: "1.2.3".into(), target: target(),
            archive: format!("swiftify-1.2.3-{}.zip", target()), sha256: "00".repeat(32), size: 10 };
        let bytes = serde_json::to_vec(&manifest).unwrap();
        let signature = encoded(&key.sign(&bytes).to_bytes());
        let public = encoded(&key.verifying_key().to_bytes());
        assert!(verify(&bytes, &signature, &public, "1.2.2", &target()).is_ok());
        assert!(verify(&bytes, &signature, &public, "1.2.3", &target()).is_err());
        assert!(verify(&bytes, &signature, &public, "2.0.0", &target()).is_err());
        assert!(verify(&bytes, &signature, &public, "1.2.2", "wrong-target").is_err());
        assert!(verify(&bytes, &signature, "", "1.2.2", &target()).is_err());
        let mut tampered = bytes.clone(); tampered[1] ^= 1;
        assert!(verify(&tampered, &signature, &public, "1.2.2", &target()).is_err());
    }

    #[test]
    fn paths_and_links_cannot_escape_or_alias() {
        for path in ["../evil", "/evil", "Swiftify/../../evil", "Swiftify/./file", "Swiftify//file", "Swiftify/C:/evil", "Swiftify\\evil", "Swiftify/NUL.txt", "Swiftify/file."] {
            assert!(safe_path(path).is_err(), "{path}");
        }
        assert!(safe_path("Swiftify.app/Contents/Frameworks/QtCore.framework/Versions/A/QtCore").is_ok());
        assert!(link_stays_inside(Path::new("Swiftify.app/Contents/F.framework/F"), "Versions/Current/F"));
        assert!(!link_stays_inside(Path::new("Swiftify.app/link"), "../outside"));
        assert!(!link_stays_inside(Path::new("Swiftify.app/link"), "/tmp/outside"));
    }

    #[test]
    fn altered_or_truncated_package_is_rejected() {
        let temp = tempfile::tempdir().unwrap(); let path = temp.path().join("package.zip");
        fs::write(&path, b"fixture").unwrap();
        let mut manifest = Manifest { schema: 1, version: "1.0.0".into(), target: target(), archive: "fixture.zip".into(),
            sha256: encoded(&Sha256::digest(b"fixture")), size: 7 };
        assert!(verify_package(&path, &manifest).is_ok());
        manifest.sha256 = "00".repeat(32); assert!(verify_package(&path, &manifest).is_err());
        manifest.size = 8; assert!(verify_package(&path, &manifest).is_err());
    }

    fn archive(directory: &Path, entries: &[(&str, &[u8])]) -> PathBuf {
        let path = directory.join("fixture.zip");
        let mut zip = zip::ZipWriter::new(File::create(&path).unwrap());
        for (name, data) in entries {
            zip.start_file(*name, zip::write::SimpleFileOptions::default().unix_permissions(0o755)).unwrap();
            zip.write_all(data).unwrap();
        }
        zip.finish().unwrap(); path
    }

    #[test]
    fn complete_portable_archive_is_extracted_without_touching_installation() {
        let temporary = tempfile::tempdir().unwrap();
        let marker = serde_json::to_vec(&serde_json::json!({"product":"Swiftify", "format":1, "target":target()})).unwrap();
        let binary = format!("{}/{}", root_name(), executable());
        let marker_path = format!("{}/.swiftify-portable.json", root_name());
        let zip = archive(temporary.path(), &[(&binary, b"fixture, never executed"), (&marker_path, &marker)]);
        let output = temporary.path().join("output");
        let root = extract(&zip, &output).unwrap();
        assert_eq!(fs::read(root.join(executable())).unwrap(), b"fixture, never executed");
        assert!(validate_installation(&root).is_ok());
    }

    #[test]
    fn unexpected_roots_and_missing_markers_are_rejected() {
        let temporary = tempfile::tempdir().unwrap();
        let zip = archive(temporary.path(), &[("other-root/evil", b"x")]);
        assert!(extract(&zip, &temporary.path().join("first")).is_err());
        let name = format!("{}/{}", root_name(), executable());
        let zip = archive(temporary.path(), &[(&name, b"x")]);
        assert!(extract(&zip, &temporary.path().join("second")).is_err());
    }

    #[cfg(unix)]
    #[test]
    fn escaping_or_parent_symlinks_are_rejected() {
        let temporary = tempfile::tempdir().unwrap();
        for target in ["../../outside", "/tmp/outside"] {
            let path = temporary.path().join("symlink.zip");
            let mut zip = zip::ZipWriter::new(File::create(&path).unwrap());
            zip.add_symlink(format!("{}/link", root_name()), target, zip::write::SimpleFileOptions::default()).unwrap();
            zip.finish().unwrap();
            assert!(extract(&path, &temporary.path().join("output")).is_err());
        }
    }

    #[test]
    fn replacement_keeps_backup_and_rolls_back_on_launch_failure() {
        let temporary = tempfile::tempdir().unwrap();
        let old = temporary.path().join("installed");
        let new = temporary.path().join("new");
        let backup = temporary.path().join("backup");
        for (path, value) in [(&old, "old"), (&new, "new")] {
            fs::create_dir(path).unwrap(); fs::write(path.join("version"), value).unwrap();
        }
        assert!(replace_and_launch(&old, &new, &backup, |path| {
            if fs::read_to_string(path.join("version"))? == "new" { Err("fixture launch denied".into()) }
            else { Ok(()) }
        }).is_err());
        assert_eq!(fs::read_to_string(old.join("version")).unwrap(), "old");
        assert_eq!(fs::read_to_string(new.join("version")).unwrap(), "new");
        assert!(replace_and_launch(&old, &new, &backup, |_| Ok(())).is_ok());
        assert_eq!(fs::read_to_string(old.join("version")).unwrap(), "new");
        assert_eq!(fs::read_to_string(backup.join("version")).unwrap(), "old");
    }

    #[test]
    fn missing_replacement_restores_the_original_directory() {
        let temporary = tempfile::tempdir().unwrap();
        let old = temporary.path().join("installed");
        fs::create_dir(&old).unwrap(); fs::write(old.join("version"), "old").unwrap();
        let result = replace_and_launch(&old, &temporary.path().join("missing"), &temporary.path().join("backup"), |_| Ok(()));
        assert!(result.is_err());
        assert_eq!(fs::read_to_string(old.join("version")).unwrap(), "old");
    }
}
