# Portable releases and in-app updates

The `prebuilt` workflow builds on main, relevant pull requests, manual dispatch
and stable version tags:

| Package target | Runner | Installation layout |
| --- | --- | --- |
| `macos-aarch64` | macos-15 | `Swiftify.app` |
| `macos-x86_64` | macos-15-intel | `Swiftify.app` |
| `windows-x86_64` | windows-2022 / MSVC | `Swiftify/bin/swiftify.exe` |
| `linux-x86_64` | ubuntu-22.04 | `Swiftify/bin/swiftify` |

Builds bundle Qt and the standalone Rust update helper. They produce ZIP files
as Actions artifacts. A pushed `v<version>` tag builds all four targets, signs the
update manifests and publishes a GitHub release in the repository that ran the
workflow. The tag must exactly match `v<version>` from `CMakeLists.txt`. Drafts,
prereleases and Actions artifacts are never offered as OTA updates.

Each build updates from the repository that built it (`SWIFTIFY_UPDATE_REPOSITORY`,
set from `github.repository` in CI, default `ceoweb20/swiftify`).

## Signing key (already configured for this repository)

OTA relies on an Ed25519 key that is independent of any Apple identity:

- the public verification key is the repository **variable** `SWIFTIFY_UPDATE_PUBLIC_KEY`
  and is compiled into the app and its installer helper;
- the private seed is the `release` environment **secret** `SWIFTIFY_UPDATE_SIGNING_KEY`,
  limited to `v*` tags. A copy lives only in `~/.swiftify-update-signing-key` (mode 0600)
  on the maintainer's computer. Keep a backup; never commit it or paste it in logs.

To create a new key pair, run in a trusted Python environment with `cryptography`:

```python
import os
from pathlib import Path
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization as s

key = Ed25519PrivateKey.generate()
private = key.private_bytes(s.Encoding.Raw, s.PrivateFormat.Raw, s.NoEncryption()).hex()
path = Path.home() / "swiftify-update-signing-key.private"
with open(os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600), "w") as f:
    f.write(private)
print("Public verification key:", key.public_key().public_bytes(s.Encoding.Raw, s.PublicFormat.Raw).hex())
```

then update the variable and the secret. Installed copies keep trusting the old
key, so changing it means a manual reinstall (see below).

To release: bump `project(Swiftify VERSION x.y.z)` and push `main`. Once all four
platform builds pass, the release job signs the manifests and publishes `vx.y.z`
(creating the tag), unless that version already has a release. Pushing a
`vx.y.z` tag by hand does the same. If neither key is set,
archives are published without OTA manifests and the app disables installation.
A mismatched or incomplete key pair fails the release job.

The public key is also the default of `SWIFTIFY_UPDATE_PUBLIC_KEY` in `CMakeLists.txt`; to use another, pass `-DSWIFTIFY_UPDATE_PUBLIC_KEY=<public-key>`; the helper is built
by default, or can be omitted with `-DSWIFTIFY_BUILD_UPDATER=OFF`. Existing users
must install one updater-enabled portable release manually first. This cannot
retrofit an updater into older binaries.

Automatic key rotation is not implemented. Changing the verification key, or
losing its private key, requires manually installing a release built with the
new key. Keep the original key backed up; verification is never silently bypassed.

## User flow

Open **Settings → About → Updates**. Two switches are on by default: *Check for
updates automatically* (once a day, at launch and while the app stays open) and
*Install updates automatically*. A found update is downloaded, verified, and
installed by itself 20 seconds after nothing has been playing, then Swiftify
restarts. It never installs over a song. With the second switch off, the app
tells you and **Download and install** asks for confirmation.

Builds made on a developer machine (not a portable package) cannot update
themselves; they have no portable marker, so the updater stays off and never
touches a development tree.

The app downloads metadata over HTTPS from this repository's releases, then uses
the local helper to verify its Ed25519 signature before trusting package details.
Downloads are bounded, redirects are restricted to GitHub's asset CDNs, and no
Spotify credentials, cookies or authorization headers are used. SHA-256, exact
file size, platform and strictly newer stable semantic version are checked again
by the installer before any replacement.

The installer extracts into a private sibling directory on the same filesystem.
It rejects traversal, unsafe/escaping symlinks, duplicate paths and Windows
filename aliases. Only marked portable installations are eligible. It signals
readiness after extraction, waits for application approval **and process exit**,
then renames the old installation to a backup and the new one into place. It
does not elevate privileges, run downloaded shell scripts or touch settings,
Keychain entries, library data or music files.

On replacement/launch-command failure it attempts to restore/relaunch the old
installation. A successful process launch is **not** proof of successful app
startup: the previous version is intentionally retained in a sibling
`.swiftify-update-*` directory for manual recovery. Logs and `result.json` with
the backup path are kept in the app cache's `swiftify-update-*` directory.

## Platform limitations

- **macOS:** releases are signed with a certificate of this project's own
  (`MACOS_SIGNING_P12`, `MACOS_SIGNING_PASSWORD`, `MACOS_SIGNING_IDENTITY` in the
  repository settings; a backup is in `~/.swiftify-macos-signing/` on the maintainer's
   computer), not with an Apple identity. The certificate keeps the designated
   requirement stable, but is not Apple Developer ID/notarization and does not
   guarantee the absence of OS trust prompts. A DMG does not change that.
   **From 0.17.0, Spotify credentials do not use Keychain at all**: see the
   [password-protected credential vault](credential-vault.md). Updates preserve
   that file and ask for the vault password on the next launch.
   The certificate is self-signed: Gatekeeper warnings may remain on downloads.
  To get the same stable identity for builds made on your own Mac, import
  `signing.p12` into your login keychain and pass
  `-DSWIFTIFY_CODESIGN_IDENTITY=<its SHA-1>` to cmake.
- **Windows:** use a portable per-user installation, e.g. under LocalAppData.
  Authenticode signing is not configured; SmartScreen warnings can remain.
  The app exits before the helper replaces DLLs/executables. Locked files or a
  non-writable installation may prevent replacement; no UAC bypass is attempted.
- **Linux:** unzip under a user-owned directory. System installations (`/usr`,
  distribution packages, Flatpak/Snap/AppImage managers) must use their package
  manager and have no portable marker. The x86_64 bundle still requires the
   baseline system audio libraries described in README. No ARM
  Linux or ARM Windows packages are currently produced.

## Focused checks

```sh
cargo test --locked --manifest-path tools/update-helper/Cargo.toml
ctest --test-dir build --output-on-failure -R '^tst_updates$'
QT_QPA_PLATFORM=offscreen build/tests/tst_views everySettingsPageDraws
python3 tests/check_translations.py .
```

CI exercises helper tests natively on all four build runners. Passing local
macOS unit/build checks does not establish real Windows/Linux installation or
Gatekeeper behavior. Validate an actual old-to-new signed release on all target
systems before enabling the public update channel.
