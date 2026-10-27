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
installer = load("package-installer")


class UpdateToolsTest(unittest.TestCase):
    def test_windows_installer_uses_deployed_tree_and_inno_setup(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "application with spaces"
            (root / "bin").mkdir(parents=True)
            (root / "bin/swiftify.exe").write_bytes(b"fixture")
            output = Path(temporary) / "dist"
            def compile_installer(command, check):
                self.assertTrue(check)
                self.assertEqual(command[0], "fixture-ISCC.exe")
                self.assertIn(f"/DSourceRoot={root.resolve()}", command)
                (output / f"swiftify-{installer.version()}-windows-x86_64-setup.exe").write_bytes(b"installer")
            with patch.object(installer.subprocess, "run", side_effect=compile_installer):
                result = installer.package(root, "windows-x86_64", output, iscc="fixture-ISCC.exe")
            self.assertTrue(result.is_file())
            script = (SOURCE / "packaging/windows/installer.iss").read_text()
            self.assertIn("PrivilegesRequired=lowest", script)
            self.assertIn("Swiftify-uninstall", script)
            self.assertIn("recursesubdirs", script)

    @unittest.skipIf(os.name == "nt", "DMG staging requires Unix symlink support")
    def test_macos_dmg_has_an_applications_shortcut(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "swiftify.app"
            (root / "Contents/MacOS").mkdir(parents=True)
            (root / "Contents/MacOS/swiftify").write_bytes(b"fixture")
            output = Path(temporary) / "dist"
            calls = []
            def create_image(command, check):
                calls.append(command)
                if command[0] == "hdiutil":
                    stage = Path(command[command.index("-srcfolder") + 1])
                    self.assertEqual(os.readlink(stage / "Applications"), "/Applications")
                    Path(command[-1]).write_bytes(b"dmg")
            with patch.object(installer.subprocess, "run", side_effect=create_image):
                result = installer.package(root, "macos-aarch64", output)
            self.assertEqual([call[0] for call in calls], ["ditto", "hdiutil"])
            self.assertEqual(result.suffix, ".dmg")

    def test_installers_require_a_deployed_application(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for target in ("windows-x86_64", "macos-aarch64", "linux-x86_64"):
                with self.assertRaises(ValueError):
                    installer.package(root, target, root / "out")

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
            self.assertTrue((root / "Contents/Resources/swiftify-portable.json").is_file())

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
