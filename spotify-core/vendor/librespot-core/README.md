# Patched librespot core

`librespot-core` 0.8.0 is vendored so Swiftify can work around three upstream
problems.

## 1. iOS AP handshake (librespot issue #1477)

Spotify currently rejects the native iOS AP handshake with `TryAnotherAP`. Spoofing only that
handshake lets the session connect but can leave subsequent authentication, HTTP, metadata, and
audio-key requests with a conflicting iOS identity, causing every track to fail loading.

The source change in `config.rs` gives iOS builds a consistent Linux protocol identity end-to-end.
This matches the working desktop/Keymaster OAuth path and does not change the iOS compilation
target or Core Audio output.

Remove this part when upstream resolves the iOS issue.

## 2. PlayPlay license keys (new accounts)

Accounts created after ~2023 are no longer served by the access-point
audio-key channel (`PacketType::RequestKey` / `AesKeyError` — librespot
issues #1649 and #1623): every key request fails with `error audio key 0 1`
and playback never starts. Official clients get their keys from the PlayPlay
license endpoint instead:

```
POST /playplay/v1/key/{file_id}   (PlayPlayLicenseRequest -> obfuscated key)
```

### What works (verified against the live endpoint, tests/verify.rs)

- The request reaches the endpoint and is answered `200` with an
  obfuscated key, using the session's own login5 bearer.
- Endpoint rules, found by probing: **no `client-token` header at all**
  (one minted for librespot gets 403, the desktop constant third-party
  clients hardcode gets 400), no `product/country/salt` query params, and a
  license token belonging to a real client build — the server encrypts the
  key for the build the token identifies. The token in use is the desktop
  1.2.94.583 one, observed on the wire.
- `src/audio_key.rs`: `AudioKeyManager::request()` falls back to the license
  endpoint when the AP channel refuses, latches the refusal off for the
  session (the service is account-wide), and caches keys per file id
  (bounded at 10000 like the official client). Old accounts keep the fast
  AP path; a cache hit costs ~5µs.

### What is still open: the derivation

Deobfuscating the returned key needs the derivation of the exact client
build the license token is from. The vendored derivation under
`native/playplay/` (a port of the re-unplayplay whitebox, MIT) matches the
old v2 wire format the server no longer serves — with the current 1.2.94
token its output does not equal the AP key (tested directly on an account
both paths still serve; XOR/permutation hypotheses rejected the same way).
Until it is replaced, `request_playplay_key` fails with a clear error
instead of handing back a wrong key, and new-account playback degrades to
that error rather than the old `audio key error 0 1` loop.

The replacement derivation is an emulation of the client build's own code
(the unplayplay/wavee approach), which needs per-build pins:

- `Spotify.dll` from Spotify **1.2.94.583 win-x64**. The offline installer
  is saved at `~/Downloads/SpotifyFullSetup-1.2.94.583-x64.exe`
  (sha256 e56ce1c2ee43ff5ccbec9a2373d394993a53e493101f7982f9f2ddc1e126dd97,
  FileVersion 1.2.94.583 confirmed). Its payload is a custom encrypted
  container: extract the DLL by running the installer on Windows (or Wine —
  macOS brew casks for wine were disabled in Sep 2026).
- Once the DLL is out, the pins to relocate for this build:
  `VM_OBJECT_TRANSFORM_VA`, `AES_KEY_HOOK.TRIGGER_RIP`,
  `VM_CONSTANTS.INIT_VALUE` (per-build, was captured at runtime for older
  builds), `RUNTIME_CONTEXT_VA`, the runtime hook VAs, and regenerated SEH
  unwind tables. Every one of these moved even between adjacent 1.2.88
  builds, so they must be re-derived for 1.2.94.583.

The verification contract to reinstate when that lands is in
`tests/verify.rs`: the derived key must equal the AP key for the same file
on an account both paths serve.

### Files

- `proto/playplay.proto` — the license request/response messages, compiled
  by `build.rs` (`compile_protos`).
- `native/playplay/` — the C++ whitebox port + C ABI bridge, compiled by
  `build.rs` (`compile_playplay`); kept for its reference tests and as the
  shape of the eventual build-matching derivation.
- `src/playplay.rs` — the license request, derivation entry point, probe
  helpers.
- `src/audio_key.rs` — fallback, refusal latch, key cache.
- `src/spclient.rs` — `RequestOptions::omit_client_token`.
- `src/http_client.rs` — a request may carry its own user agent.
- `tests/verify.rs` — live-endpoint verification (`--features
  playplay-verify`, needs `SWIFTIFY_PLAYBACK_LOGIN` with the saved playback
  login JSON).

Remove this part if upstream merges its own PlayPlay support.
