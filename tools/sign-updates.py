#!/usr/bin/env python3
"""Sign portable release manifests. Run ONLY in a protected tag-release job.

SWIFTIFY_UPDATE_SIGNING_KEY is a 32-byte Ed25519 seed encoded as 64 hex digits.
SWIFTIFY_UPDATE_PUBLIC_KEY must match its public key. Never print the seed.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import sys

from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization


def sign(directory):
    key = Ed25519PrivateKey.from_private_bytes(bytes.fromhex(os.environ["SWIFTIFY_UPDATE_SIGNING_KEY"]))
    public = key.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw).hex()
    if public != os.environ["SWIFTIFY_UPDATE_PUBLIC_KEY"].lower():
        raise ValueError("The signing key does not match the compiled public key")
    tag = os.environ["GITHUB_REF_NAME"]
    if not re.fullmatch(r"v\d+\.\d+\.\d+", tag):
        raise ValueError("Only stable version tags can publish updates")
    expected = {"macos-aarch64", "macos-x86_64", "linux-x86_64", "windows-x86_64"}
    archives = list(directory.glob("*.zip"))
    if len(archives) != len(expected):
        raise ValueError("All four platform archives must be present")
    for path in archives:
        match = re.fullmatch(r"swiftify-(\d+\.\d+\.\d+)-(macos-aarch64|macos-x86_64|linux-x86_64|windows-x86_64)\.zip", path.name)
        if not match or "v" + match[1] != tag or match[2] not in expected:
            raise ValueError("Unexpected release archive")
        expected.remove(match[2])
        with path.open("rb") as file:
            digest = hashlib.file_digest(file, "sha256").hexdigest()
        manifest = {"schema": 1, "version": match[1], "target": match[2], "archive": path.name,
                    "size": path.stat().st_size, "sha256": digest}
        data = (json.dumps(manifest, sort_keys=True, separators=(",", ":")) + "\n").encode()
        name = directory / ("swiftify-update-" + match[2])
        name.with_suffix(".json").write_bytes(data)
        name.with_suffix(".sig").write_text(key.sign(data).hex() + "\n")


if __name__ == "__main__":
    sign(Path(sys.argv[1]))
