"""Small packaging/signing fixtures; never reads a real release key."""
import contextlib
import importlib.util
import io
import json
import os
import subprocess
import sys
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
            def create_image(command, check, **kwargs):
                calls.append(command)
                if command[0] == "hdiutil":
                    stage = Path(command[command.index("-srcfolder") + 1])
                    self.assertEqual(os.readlink(stage / "Applications"), "/Applications")
                    self.assertEqual(command[command.index("-fs") + 1], "HFS+")
                    Path(command[-1]).write_bytes(b"dmg")
                return subprocess.CompletedProcess(command, 0, stdout="", stderr="")
            with patch.object(installer.subprocess, "run", side_effect=create_image), \
                    patch.object(installer, "styled_dmg_available", return_value=False):
                result = installer.package(root, "macos-aarch64", output)
            self.assertEqual([call[0] for call in calls], ["ditto", "hdiutil"])
            self.assertEqual(result.suffix, ".dmg")

    @unittest.skipIf(os.name == "nt", "the styled DMG is built on macOS")
    def test_macos_release_dmg_uses_the_installer_layout(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "swiftify.app"
            (root / "Contents/MacOS").mkdir(parents=True)
            (root / "Contents/MacOS/swiftify").write_bytes(b"fixture")
            output = Path(temporary) / "dist"
            builds = []
            def build_dmg(filename, volume_name, settings, **kwargs):
                builds.append((volume_name, settings))
                # the HiDPI art is found beside the background while it builds
                self.assertTrue((Path(settings["background"]).parent / "background@2x.png").is_file())
                Path(filename).write_bytes(b"styled dmg")
            fake = type(sys)("dmgbuild")
            fake.build_dmg = build_dmg
            def copy_app(command, check, **kwargs):
                Path(command[-1], "Contents").mkdir(parents=True)
                return subprocess.CompletedProcess(command, 0, stdout="", stderr="")
            with patch.dict(sys.modules, {"dmgbuild": fake}), \
                    patch.object(installer.subprocess, "run", side_effect=copy_app):
                result = installer.package(root, "macos-aarch64", output)
            self.assertEqual(result.read_bytes(), b"styled dmg")
            volume, settings = builds[0]
            self.assertEqual(volume, "BetterSwiftify")
            self.assertEqual(settings["symlinks"], {"Applications": "/Applications"})
            self.assertEqual(settings["icon_locations"]["Applications"], installer.DMG_APPLICATIONS_SLOT)
            self.assertEqual(settings["icon_locations"]["Swiftify.app"], installer.DMG_APP_SLOT)
            self.assertEqual(Path(settings["background"]).name, "background.png")

    def test_release_build_refuses_an_unstyled_dmg(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "swiftify.app"
            (root / "Contents/MacOS").mkdir(parents=True)
            (root / "Contents/MacOS/swiftify").write_bytes(b"fixture")
            with patch.object(installer, "styled_dmg_available", return_value=False), \
                    patch.object(installer.subprocess, "run"), \
                    patch.dict(os.environ, {"SWIFTIFY_REQUIRE_STYLED_DMG": "1"}):
                with self.assertRaises(RuntimeError):
                    installer.package(root, "macos-aarch64", Path(temporary) / "dist")

    def test_busy_dmg_creation_is_retried_with_a_new_image_path(self):
        with tempfile.TemporaryDirectory() as temporary:
            scratch = Path(temporary)
            staging = scratch / "contents"
            staging.mkdir()
            destination = scratch / "output.dmg"
            images = []
            def create(command, **kwargs):
                image = Path(command[-1])
                images.append(image)
                self.assertFalse(image.is_relative_to(staging))
                if len(images) == 1:
                    raise subprocess.CalledProcessError(1, command, stderr="hdiutil: create failed - Resource busy")
                image.write_bytes(b"complete image")
                return subprocess.CompletedProcess(command, 0, stdout="", stderr="")
            with patch.object(installer.subprocess, "run", side_effect=create), patch.object(installer.time, "sleep") as sleep:
                installer.create_dmg(staging, destination, scratch)
            self.assertEqual(len(images), 2)
            self.assertNotEqual(images[0], images[1])
            self.assertEqual(destination.read_bytes(), b"complete image")
            sleep.assert_called_once_with(5)

    def test_dmg_retries_are_bounded_and_preserve_previous_output(self):
        for message, attempts in (("Resource busy", 3), ("Permission denied", 1)):
            with self.subTest(message=message), tempfile.TemporaryDirectory() as temporary:
                scratch = Path(temporary)
                destination = scratch / "output.dmg"
                destination.write_bytes(b"previous valid image")
                error = subprocess.CalledProcessError(1, "hdiutil", stderr=message)
                with patch.object(installer.subprocess, "run", side_effect=error) as run, patch.object(installer.time, "sleep"):
                    with self.assertRaises(subprocess.CalledProcessError):
                        installer.create_dmg(scratch / "contents", destination, scratch)
                self.assertEqual(run.call_count, attempts)
                self.assertEqual(destination.read_bytes(), b"previous valid image")

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
