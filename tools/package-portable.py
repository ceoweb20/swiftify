#!/usr/bin/env python3
"""Package the three portable layouts in the format consumed by Updates.

No signing key is used in build jobs. macOS must be marked before codesign.
"""
import argparse
import json
import os
from pathlib import Path
import re
import stat
import zipfile


def version():
    source = (Path(__file__).resolve().parents[1] / "CMakeLists.txt").read_text()
    return re.search(r"project\(Swiftify\s+VERSION (\d+\.\d+\.\d+)", source)[1]


def package(root, target, output, mark_only=False):
    if target not in ("macos-aarch64", "macos-x86_64", "linux-x86_64", "windows-x86_64"):
        raise ValueError("Unsupported update target")
    root = root.resolve(strict=True)
    mac = target.startswith("macos-")
    executable = "Contents/MacOS/swiftify" if mac else "bin/swiftify.exe" if target.startswith("windows-") else "bin/swiftify"
    helper = str(Path(executable).with_name("swiftify-update-helper" + (".exe" if target.startswith("windows-") else "")))
    if not (root / executable).is_file() or not (root / helper).is_file():
        raise ValueError("The application or standalone update helper is missing")
    marker = root / ".swiftify-portable.json"
    identity = {"product": "Swiftify", "format": 1, "target": target}
    if marker.exists():
        if json.loads(marker.read_text()) != identity:
            raise ValueError("Installation marker does not match the target")
    elif mac and not mark_only:
        raise ValueError("Mark the macOS bundle before signing it")
    else:
        marker.write_text(json.dumps(identity, sort_keys=True) + "\n")
    if mark_only:
        return
    output.mkdir(parents=True, exist_ok=True)
    archive = output / f"swiftify-{version()}-{target}.zip"
    prefix = "Swiftify.app" if mac else "Swiftify"
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for directory, folders, files in os.walk(root, followlinks=False):
            for name in sorted(folders + files):
                path = Path(directory) / name
                if path.is_symlink():
                    target_path = path.resolve(strict=True)
                    if not target_path.is_relative_to(root):
                        raise ValueError(f"Symlink escapes the bundle: {path}")
                    info = zipfile.ZipInfo(prefix + "/" + path.relative_to(root).as_posix())
                    info.create_system = 3
                    info.external_attr = (stat.S_IFLNK | 0o777) << 16
                    z.writestr(info, os.readlink(path).encode())
                elif path.is_file():
                    z.write(path, prefix + "/" + path.relative_to(root).as_posix())
    print(archive)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--target", required=True)
    parser.add_argument("--out", type=Path, default=Path("dist"))
    parser.add_argument("--mark-only", action="store_true")
    args = parser.parse_args()
    package(args.root, args.target, args.out, args.mark_only)
