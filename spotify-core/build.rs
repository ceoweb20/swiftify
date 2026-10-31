/*
 * Swiftify, a spotify player for desktop
 * Copyright (C) 2026  shred0s
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

fn main() {
    println!("cargo:rerun-if-changed=build.rs");

    #[cfg(feature = "swift")]
    generate_swift_bridge();
}

/// writes the swift glue the xcode project compiles. only the apple apps need
/// it, so a linux build with the c abi alone leaves that directory alone.
#[cfg(feature = "swift")]
fn generate_swift_bridge() {
    use std::path::PathBuf;

    let bridge_files = ["src/swift_ffi.rs"];

    for bridge_file in bridge_files {
        println!("cargo:rerun-if-changed={bridge_file}");
    }

    let generated_directory = PathBuf::from("../Swiftify/Swiftify/Bridge/Generated");
    std::fs::create_dir_all(&generated_directory)
        .expect("failed to create the swift-bridge output directory");

    swift_bridge_build::parse_bridges(bridge_files)
        .write_all_concatenated(generated_directory, env!("CARGO_PKG_NAME"));
}
