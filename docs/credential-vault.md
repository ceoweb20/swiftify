# Encrypted Spotify logins (0.17.0 and newer)

Swiftify no longer uses QtKeychain, macOS Keychain, Windows Credential Manager or
Linux Secret Service for Spotify credentials. A DMG or a self-signed certificate
does not reliably solve Keychain approval prompts; removing that backend does.

## First launch and migration

1. Create a **separate vault password** (at least 12 characters), not your Spotify
   password. Prefer a long, unique passphrase and keep it in your password manager.
2. Sign in to Spotify playback/library once again. Existing client IDs/settings,
   collections and library data are kept, but the old OS credential entries are
   **not read, imported, modified or deleted**, so migration never asks for Keychain
   approval. You may remove those old entries manually if desired.
3. On subsequent launches, enter the vault password once. Reopening the window,
   refreshing tokens or signing out/in within the same process does not ask again.
   The crypto key is kept in memory until the app quits, not saved beside the file.

The startup dialog blocks access while locked. A wrong password, failed write or
damaged file does not silently recreate the vault or overwrite the old logins.
Closing the app before unlocking cancels restoration safely. Logging out writes
an encrypted empty vault; it does not discard your vault password.

There is no password recovery or plaintext fallback. If you forget it, quit
Swiftify, back up/remove **only** `credentials/logins.vault`, restart and create a
new vault, then sign in to Spotify again. Do not remove the whole data directory.
Password changes/export/import are not implemented in this version.

## File and encryption

The file is under `Swiftify/credentials/logins.vault` in Qt's user data directory
(macOS: `~/Library/Application Support/Swiftify/credentials/logins.vault`; Linux:
`~/.local/share/Swiftify/credentials/logins.vault`; Windows: the user's local app
data directory). It is outside the app bundle, so OTA replacement preserves it.

- Argon2id v1.3 derives a 256-bit key with 64 MiB memory, three passes and one lane.
  Derivation runs on a worker thread, not on the UI thread. Costs are fixed in code,
  never trusted from an on-disk header.
- XChaCha20-Poly1305 authenticates/encrypts the credentials. A random 128-bit salt
  is generated for a new vault and a fresh random 192-bit nonce for every write.
- The base64 envelope contains an eight-byte `SWFTVLT1` magic, salt, nonce and
  ciphertext/tag. The entire header is authenticated as associated data.
- Both Spotify accounts are updated in one atomic `QSaveFile` replacement, with no
  direct-write fallback. File size is bounded; Unix permissions are 0700 for the
  credential directory and 0600 for the file. A lock prevents concurrent writers.
- The derived key and temporary Rust password/decryption buffers are wiped when
  dropped. The live app necessarily holds decrypted credentials in memory; Qt
  strings/JSON, process dumps and swap are not guaranteed to be erased securely.

Encryption protects the closed vault against reading its file without the
password, subject to password strength and offline guessing. It does **not**
protect an unlocked process against malware, debuggers, keyloggers or a compromised
OS/user account. This is not a hardware-backed vault or an independently audited
password manager. No password, token or secret key belongs in logs, preferences,
command-line arguments or release artifacts.

## Focused checks

```sh
cargo test --locked --manifest-path spotify-core/Cargo.toml --no-default-features --features capi vault::tests
ctest --test-dir build --output-on-failure -R '^(tst_vault|tst_session)$'
QT_QPA_PLATFORM=offscreen build/tests/tst_views vaultPasswordDialog
```

Fixtures cover round-trip decryption, wrong passwords, header/ciphertext tampering,
truncation, random nonces, two-account persistence/logout, failed writes, concurrent
locking, deferred session restoration and create/unlock UI password masking.
