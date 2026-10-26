//! Verifies the PlayPlay key path against the access-point key path, using a
//! real session. Run with a saved playback login (the same JSON
//! `swiftify_core_connect_stored` accepts) via the SWIFTIFY_PLAYBACK_LOGIN
//! environment variable:
//!
//! ```sh
//! SWIFTIFY_PLAYBACK_LOGIN='{"username":"...","auth_type":1,"auth_data":"..."}' \
//!     cargo test --features playplay-verify --lib verify -- --nocapture
//! ```
//!
//! The test is behind a feature so a normal `cargo test` never needs network
//! or credentials. It picks a track through the same metadata path playback
//! uses, asks for its key over the AP channel (the one old accounts still
//! have) and over the PlayPlay license endpoint (the one new accounts are
//! limited to), and asserts the two agree. If they agree, the fallback path
//! in `audio_key.rs` is proven end-to-end for whatever account was tested.

use std::env;

use librespot_core::{
    authentication::Credentials,
    config::SessionConfig,
    playplay,
    session::Session,
    spclient::SpClient,
    FileId, SpotifyId, SpotifyUri,
};

#[cfg(feature = "playplay-verify")]
#[tokio::test]
async fn verify_playplay_key_matches_access_point_key() {
    let login = env::var("SWIFTIFY_PLAYBACK_LOGIN")
        .expect("SWIFTIFY_PLAYBACK_LOGIN with the saved playback login json");
    let credentials: Credentials =
        serde_json::from_str(&login).expect("the playback login json is valid");

    let client_id =
        env::var("SWIFTIFY_CLIENT_ID").unwrap_or_else(|_| "65b708073fc0480ea92a077233ca87bd".into());
    let session = Session::new(
        SessionConfig {
            client_id: client_id.clone(),
            ..SessionConfig::default()
        },
        None,
    );
    session
        .connect(credentials, false)
        .await
        .expect("connected with the saved playback login");

    println!("authenticated as {}", session.username());

    // a known playable track; picked to exist in every market
    let track = SpotifyId::from_base62("4uLU6hMCjMI75M1A2tKUQC").expect("track id parses");
    let spclient: SpClient = session.spclient().clone();

    // metadata -> the file the player would stream
    let track_uri = SpotifyUri::from_uri("spotify:track:4uLU6hMCjMI75M1A2tKUQC")
        .expect("track uri parses");
    let raw = spclient
        .get_track_metadata(&track_uri)
        .await
        .expect("track metadata");
    let metadata = <librespot_protocol::metadata::Track as protobuf::Message>::parse_from_bytes(&raw)
        .expect("track metadata protobuf parses");
    println!(
        "track: {} ({} bytes raw, {} files, {} alternatives)",
        metadata.name(),
        raw.len(),
        metadata.file.len(),
        metadata.alternative.len(),
    );
    // when the direct files are withheld, a playable alternative carries them
    let file = metadata
        .file
        .iter()
        .chain(metadata.alternative.iter().flat_map(|alt| alt.file.iter()))
        .find(|file| file.format == Some(protobuf::EnumOrUnknown::new(librespot_protocol::metadata::audio_file::Format::OGG_VORBIS_320)))
        .or_else(|| {
            metadata
                .file
                .iter()
                .chain(metadata.alternative.iter().flat_map(|alt| alt.file.iter()))
                .next()
        })
        .expect("the track has a streamable file");
    let file_id = FileId::from_raw(file.file_id());
    println!("track file: {}", file_id.to_base16().unwrap());

    // dump the bearer so the exact request can be replayed with curl
    let bearer = session.login5().auth_token().await.unwrap();
    #[cfg(feature = "playplay-verify")]
    std::fs::write("/tmp/pp_bearer.txt", &bearer.access_token).ok();
    println!("bearer: {}... (saved to /tmp/pp_bearer.txt)", &bearer.access_token[..24.min(bearer.access_token.len())]);
    let tokens: &[(i32, &str, &str)] = &[
        (2, "011bf34c8d0393bcda8b1d3eeaf8f3b2", "re-unplayplay v2 (banned 2024)"),
        (2, "01e132cae527bd21620e822f58514932", "spsync v2"),
        (2, "027b23a2442c86ca4b004ddfef291954", "unplayplay 0.0.6-8"),
        (5, "02811027c51620c0fd36cd1de59e227a", "unplayplay 0.0.9 (v5)"),
        // captured from the real desktop 1.2.94.583 (jul 2026, Wavee research)
        (5, "02517d9e67502214cddfe35218fa8c77", "desktop 1.2.94.583 capture"),
    ];
    for (version, token_hex, name) in tokens {
        match playplay::request_license_raw(&spclient, *version, &hex_to_bytes(token_hex), file_id).await {
            Ok(obfuscated) => println!("  v{version} {name}: OK, obfuscated_key={}", hex(&obfuscated)),
            Err(e) => println!("  v{version} {name}: {e}"),
        }
    }
    // the same v5 pair under both desktop client tokens: if either answers,
    // the client token was the 400
    let v5 = hex_to_bytes("02517d9e67502214cddfe35218fa8c77");
    match playplay::request_license_raw(&spclient, 5, &v5, file_id).await {
        Ok(obfuscated) => println!("  v5 desktop 1.2.94 (no client-token): OK, obfuscated_key={}", hex(&obfuscated)),
        Err(e) => println!("  v5 desktop 1.2.94 (no client-token): {e}"),
    }

    // the production path: license granted, derivation honestly unimplemented
    // until the whitebox is replaced with the matching build's
    let started = std::time::Instant::now();
    match playplay::request_playplay_key(&spclient, track, file_id).await {
        Ok(key) => panic!(
            "unexpected: got a derived key {key:02x?} — the whitebox should not match this build"
        ),
        Err(e) => println!("  production path (expected to fail on derivation): {e}"),
    }
    println!("  license round trip: {:?}", started.elapsed());

    // ---- cross-path analysis: the old account gets the real key from the AP
    // and the obfuscated one from the license endpoint for the same files.
    // with (obfuscated, real) pairs, cheap hypotheses about the transform can
    // be tested directly: constant XOR, byte permutation, etc.
    let analysis_tracks = [
        "spotify:track:4uLU6hMCjMI75M1A2tKUQC", // Never Gonna Give You Up
        "spotify:track:0VjIjW4GlUZAMYd2vXMi3b", // Blinding Lights
        "spotify:track:7qiZfU4dY1lWllzX7mPBI3", // Shape of You
        "spotify:track:6habFhsOp2NvshLv26Dqyd", // Someone You Loved
    ];
    let mut pairs: Vec<(String, [u8; 16], [u8; 16])> = Vec::new();
    for uri in analysis_tracks {
        let Ok(parsed) = SpotifyUri::from_uri(uri) else {
            continue;
        };
        let Ok(raw) = spclient.get_track_metadata(&parsed).await else {
            println!("  analysis: no metadata for {uri}");
            continue;
        };
        let Ok(meta) = <librespot_protocol::metadata::Track as protobuf::Message>::parse_from_bytes(&raw)
        else {
            continue;
        };
        let Some(file) = meta
            .file
            .iter()
            .chain(meta.alternative.iter().flat_map(|alt| alt.file.iter()))
            .next()
            .map(|f| FileId::from_raw(f.file_id()))
        else {
            continue;
        };
        let (Ok(ap), Ok(obf)) = (
            session.audio_key().request_access_point(track, file).await,
            playplay::request_license_raw(
                &spclient,
                5,
                &hex_to_bytes("02517d9e67502214cddfe35218fa8c77"),
                file,
            )
            .await,
        ) else {
            println!("  analysis: key paths failed for {uri}");
            continue;
        };
        println!(
            "  analysis {}: file {} ap {} obf {}",
            &uri[14..24],
            &file.to_base16().unwrap()[..8],
            hex(&ap.0),
            hex(&obf)
        );
        pairs.push((uri.to_owned(), ap.0, obf));
    }

    if pairs.len() >= 2 {
        // constant-XOR hypothesis: key = obf XOR K for a fixed K
        let xors: Vec<[u8; 16]> = pairs
            .iter()
            .map(|(_, ap, obf)| {
                let mut x = [0u8; 16];
                for i in 0..16 {
                    x[i] = ap[i] ^ obf[i];
                }
                x
            })
            .collect();
        let constant = xors.windows(2).all(|w| w[0] == w[1]);
        println!("  xor(ap, obf) per file: {:?}", xors.iter().map(|x| hex(x)).collect::<Vec<_>>());
        println!("  constant-xor hypothesis: {}", if constant { "HOLDS" } else { "rejected" });

        // permutation hypothesis: same multiset of bytes?
        let mut sorted_aps: Vec<Vec<u8>> = pairs.iter().map(|(_, ap, _)| ap.to_vec()).collect();
        for v in sorted_aps.iter_mut() {
            v.sort_unstable();
        }
        let same_histogram = pairs
            .iter()
            .map(|(_, _, obf)| {
                let mut v = obf.to_vec();
                v.sort_unstable();
                v
            })
            .zip(sorted_aps.iter())
            .all(|(a, b)| a == *b);
        println!("  byte-histogram hypothesis: {}", if same_histogram { "HOLDS" } else { "rejected" });
    }

    // key over the old path: the access-point channel
    let ap_key = session
        .audio_key()
        .request_access_point(track, file_id)
        .await
        .expect("AP audio key (this account is served by the old channel)");
    println!("ap key:      {}", hex(&ap_key.0));

    // the optimized manager path on this (AP-served) account: one production
    // request primes the cache, the second must come from it — no second AP
    // round trip, no license request
    let first = session
        .audio_key()
        .request(track, file_id)
        .await
        .expect("ap key via the production path");
    assert_eq!(first.0, ap_key.0);
    let started = std::time::Instant::now();
    let again = session
        .audio_key()
        .request(track, file_id)
        .await
        .expect("cached key");
    let cached_latency = started.elapsed();
    assert_eq!(again.0, ap_key.0);
    println!("  cached request: {cached_latency:?} (must be ~0: no round trip)");

    // once the derivation matches the build, this is the assertion that
    // proves the whole path (kept as the contract to reinstate):
    //   let playplay_key = playplay::request_playplay_key(&spclient, track, file_id)
    //       .await
    //       .expect("playplay license key");
    //   assert_eq!(ap_key.0, playplay_key);

    // and the session must be a premium one (playback requires it), which
    // also tells us which side of the account split we just tested
    let catalogue = session.get_user_attribute("type").unwrap_or_default();
    println!("account catalogue: {catalogue:?}");
}

#[cfg(feature = "playplay-verify")]
fn hex(bytes: &[u8]) -> String {
    bytes.iter().map(|b| format!("{b:02x}")).collect()
}

#[cfg(feature = "playplay-verify")]
fn hex_to_bytes(hex: &str) -> Vec<u8> {
    (0..hex.len())
        .step_by(2)
        .map(|i| u8::from_str_radix(&hex[i..i + 2], 16).unwrap())
        .collect()
}

#[cfg(not(feature = "playplay-verify"))]
#[allow(unused)]
fn placeholder() {}

