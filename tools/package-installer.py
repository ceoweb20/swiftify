#!/usr/bin/env python3
"""Build end-user installers from an already deployed application tree.

Keep the portable ZIPs for the signed updater; installers are separate release
assets. No credentials, signing secrets or Soloist binaries are bundled here.
"""
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


SOURCE = Path(__file__).resolve().parents[1]


def version():
    text = (SOURCE / "CMakeLists.txt").read_text()
    return re.search(r"project\(Swiftify\s+VERSION (\d+\.\d+\.\d+)", text)[1]


def package(root, target, output, iscc=None):
    root = root.resolve(strict=True)
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    release = version()
    if target in ("macos-aarch64", "macos-x86_64"):
        if not (root / "Contents/MacOS/swiftify").is_file():
            raise ValueError("A deployed macOS application bundle is required")
        destination = output / f"swiftify-{release}-{target}.dmg"
        with tempfile.TemporaryDirectory(prefix="swiftify-dmg-") as temporary:
            staging = Path(temporary)
            subprocess.run(["ditto", str(root), str(staging / "Swiftify.app")], check=True)
            (staging / "Applications").symlink_to("/Applications")
            subprocess.run(["hdiutil", "create", "-volname", "Swiftify", "-srcfolder", str(staging),
                            "-format", "UDZO", "-ov", str(destination)], check=True)
    elif target == "windows-x86_64":
        if not (root / "bin/swiftify.exe").is_file():
            raise ValueError("A deployed Windows application tree is required")
        compiler = iscc or shutil.which("ISCC.exe")
        if not compiler:
            candidate = Path(r"C:\Program Files (x86)\Inno Setup 6\ISCC.exe")
            if candidate.is_file():
                compiler = str(candidate)
        if not compiler:
            raise ValueError("Inno Setup 6 is required to build the Windows installer")
        destination = output / f"swiftify-{release}-windows-x86_64-setup.exe"
        subprocess.run([str(compiler), f"/DSourceRoot={root}", f"/DOutputRoot={output}",
                        f"/DAppVersion={release}", str(SOURCE / "packaging/windows/installer.iss")], check=True)
    else:
        raise ValueError("Installers are supported for macOS and Windows only")
    if not destination.is_file():
        raise RuntimeError("The installer tool did not produce its output")
    print(destination)
    return destination


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--target", required=True)
    parser.add_argument("--out", type=Path, default=Path("dist"))
    parser.add_argument("--iscc", help="Path to the Inno Setup compiler")
    args = parser.parse_args()
    package(args.root, args.target, args.out, args.iscc)
