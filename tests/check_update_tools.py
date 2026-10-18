"""Small packaging/signing fixtures; never reads a real release key."""
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile


SOURCE = Path(__file__).resolve().parents[1]


def load(name):
    spec = importlib.util.spec_from_file_location(name, SOURCE / "tools" / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


packager = load("package-portable")


class UpdateToolsTest(unittest.TestCase):
    def test_portable_root_marker_and_executable_are_packaged(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary)
            root = path / "installation"
            (root / "bin").mkdir(parents=True)
            for binary in ("swiftify", "swiftify-update-helper"):
                (root / "bin" / binary).write_bytes(b"fixture, never executed")
            with contextlib.redirect_stdout(io.StringIO()):
                packager.package(root, "linux-x86_64", path / "out")
            archive = next((path / "out").glob("*.zip"))
            with zipfile.ZipFile(archive) as z:
                marker = json.loads(z.read("Swiftify/.swiftify-portable.json"))
                self.assertEqual(marker["target"], "linux-x86_64")
                self.assertIn("Swiftify/bin/swiftify", z.namelist())

    def test_macos_marker_must_precede_code_signing(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "Swiftify.app"
            (root / "Contents/MacOS").mkdir(parents=True)
            for binary in ("swiftify", "swiftify-update-helper"):
                (root / "Contents/MacOS" / binary).write_bytes(b"fixture")
            with self.assertRaises(ValueError):
                packager.package(root, "macos-aarch64", root.parent / "out")
            packager.package(root, "macos-aarch64", root.parent / "out", mark_only=True)
            self.assertTrue((root / ".swiftify-portable.json").is_file())

    def test_signed_manifests_cover_exact_bytes_and_require_matching_key(self):
        try:
            from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
            from cryptography.hazmat.primitives import serialization as s
        except ImportError:
            self.skipTest("cryptography is only required in the release signing job")
        signer = load("sign-updates")
        private = Ed25519PrivateKey.from_private_bytes(bytes([42]) * 32)
        public = private.public_key().public_bytes(s.Encoding.Raw, s.PublicFormat.Raw).hex()
        environment = {"SWIFTIFY_UPDATE_SIGNING_KEY": (bytes([42]) * 32).hex(),
                       "SWIFTIFY_UPDATE_PUBLIC_KEY": public, "GITHUB_REF_NAME": "v1.2.3"}
        with tempfile.TemporaryDirectory() as temporary, patch.dict(os.environ, environment):
            path = Path(temporary)
            for target in ("macos-aarch64", "macos-x86_64", "windows-x86_64", "linux-x86_64"):
                (path / f"swiftify-1.2.3-{target}.zip").write_bytes(b"fixture archive")
            signer.sign(path)
            for manifest in path.glob("*.json"):
                signature = bytes.fromhex(manifest.with_suffix(".sig").read_text().strip())
                private.public_key().verify(signature, manifest.read_bytes())
                self.assertEqual(json.loads(manifest.read_bytes())["size"], len(b"fixture archive"))
            os.environ["SWIFTIFY_UPDATE_PUBLIC_KEY"] = "00" * 32
            with self.assertRaises(ValueError):
                signer.sign(path)


if __name__ == "__main__":
    unittest.main()
