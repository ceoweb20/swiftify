use std::{env, path::Path};

use rand::Rng;
use rand_distr::Alphanumeric;
use vergen_gitcl::{BuildBuilder, Emitter, GitclBuilder};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let gitcl = GitclBuilder::default()
        .sha(true) // outputs 'VERGEN_GIT_SHA', and sets the 'short' flag true
        .commit_date(true) // outputs 'VERGEN_GIT_COMMIT_DATE'
        .build()?;

    let build = BuildBuilder::default()
        .build_date(true) // outputs 'VERGEN_BUILD_DATE'
        .build()?;

    Emitter::default()
        .add_instructions(&build)?
        .add_instructions(&gitcl)?
        .emit()
        .expect("Unable to generate the cargo keys!");
    let build_id = match std::env::var("SOURCE_DATE_EPOCH") {
        Ok(val) => val,
        Err(_) => rand::rng()
            .sample_iter(Alphanumeric)
            .take(8)
            .map(char::from)
            .collect(),
    };

    println!("cargo:rustc-env=LIBRESPOT_BUILD_ID={build_id}");

    compile_playplay();
    compile_protos();
    Ok(())
}

/// The PlayPlay key derivation: a C++ port (native/playplay) of the algorithm
/// that turns a license response's obfuscated key into the stream AES key.
fn compile_playplay() {
    let manifest = env::var("CARGO_MANIFEST_DIR").unwrap();
    let native = Path::new(&manifest).join("native/playplay");

    println!("cargo:rerun-if-changed={}", native.join("bridge.cpp").display());
    println!("cargo:rerun-if-changed={}", native.join("decrypt_key.cpp").display());
    println!("cargo:rerun-if-changed={}", native.join("bind_key.cpp").display());
    println!("cargo:rerun-if-changed={}", native.join("constants.hpp").display());
    println!("cargo:rerun-if-changed={}", native.join("types.hpp").display());
    println!("cargo:rerun-if-changed={}", native.join("util.hpp").display());
    println!("cargo:rerun-if-changed={}", native.join("unplayplay.hpp").display());
    println!("cargo:rerun-if-changed={}", native.join("idadefs.hpp").display());

    let mut build = cc::Build::new();
    build
        .cpp(true)
        .std("c++17")
        // the derivation relies on unsigned wraparound exactly as decompiled;
        // sanitizers/UB checks would miscompile it. the decompiled body also
        // declares hundreds of variables it never uses on each switch path.
        .flag_if_supported("-fno-sanitize=undefined")
        .flag_if_supported("-fno-strict-overflow")
        .warnings(false)
        .include(&native)
        .file(native.join("bridge.cpp"))
        .file(native.join("decrypt_key.cpp"))
        .file(native.join("bind_key.cpp"))
        .compile("playplay");
}

/// The license request/response protos. librespot-protocol compiles its own
/// set; this one message pair is ours, so it is generated here next to the
/// code that uses it.
fn compile_protos() {
    let manifest = env::var("CARGO_MANIFEST_DIR").unwrap();
    let proto_dir = Path::new(&manifest).join("proto");
    let out_dir = env::var("OUT_DIR").unwrap();

    protobuf_codegen::Codegen::new()
        .pure()
        .out_dir(&out_dir)
        .inputs(&[proto_dir.join("playplay.proto")])
        .include(&proto_dir)
        .run()
        .expect("playplay protobuf codegen failed");

    // the generator emits inner attributes and doc comments (`#![...]`,
    // `//!...`), which only compile at the top of a file. strip them so the
    // module can be included from src/playplay.rs.
    let generated = Path::new(&out_dir).join("playplay.rs");
    let source = std::fs::read_to_string(&generated).expect("read generated playplay.rs");
    let stripped = source
        .lines()
        .filter(|line| {
            let trimmed = line.trim_start();
            !(trimmed.starts_with("#![") || trimmed.starts_with("//!"))
        })
        .collect::<Vec<_>>()
        .join("\n");
    std::fs::write(&generated, stripped).expect("write stripped playplay.rs");
}
