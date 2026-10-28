#!/usr/bin/env python3
"""Build end-user installers from an already deployed application tree.

Keep the portable ZIPs for the signed updater; installers are separate release
assets. No credentials, signing secrets or Soloist binaries are bundled here.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time


SOURCE = Path(__file__).resolve().parents[1]


def version():
    text = (SOURCE / "CMakeLists.txt").read_text()
    return re.search(r"project\(Swiftify\s+VERSION (\d+\.\d+\.\d+)", text)[1]


def create_dmg(staging, destination, scratch):
    # Explicit HFS+ avoids the default APFS creation path on newer runners.
    # Use a fresh intermediate image on each attempt; a failed diskimages
    # helper may still hold the previous image open for a short time.
    for attempt in range(1, 4):
        image = scratch / f"installer-{attempt}.dmg"
        command = ["hdiutil", "create", "-volname", "Swiftify", "-srcfolder", str(staging),
                   "-fs", "HFS+", "-format", "UDZO", "-ov", str(image)]
        try:
            result = subprocess.run(command, check=True, capture_output=True, text=True, timeout=180)
        except subprocess.CalledProcessError as error:
            detail = (error.stdout or "") + (error.stderr or "")
            if attempt == 3 or "resource busy" not in detail.lower():
                if detail:
                    print(detail, file=sys.stderr)
                raise
            print(f"DMG device temporarily busy; retrying ({attempt}/3).", file=sys.stderr)
            time.sleep(5 * attempt)
        else:
            if result.stdout:
                print(result.stdout, end="")
            os.replace(image, destination)
            return


DMG_ART = SOURCE / "packaging/macos/dmg"
# Finder's window content and where it puts the two icons, in points; the
# background art leaves these spots and the label strip under them clear.
DMG_WINDOW = (660, 420)
DMG_APP_SLOT = (180, 210)
DMG_APPLICATIONS_SLOT = (480, 210)


def create_styled_dmg(app, destination, scratch):
    """The drag-to-install window: our background, the app on the left, an
    Applications alias on the right and the app icon on the volume. dmgbuild
    writes Finder's layout directly, so no Finder scripting is needed on CI."""
    import dmgbuild

    background = scratch / "background.png"
    shutil.copy(DMG_ART / "background.png", background)
    # found beside it and merged into a HiDPI background
    shutil.copy(DMG_ART / "background@2x.png", scratch / "background@2x.png")
    settings = {
        "format": "UDZO",
        "filesystem": "HFS+",
        "files": [str(app)],
        "symlinks": {"Applications": "/Applications"},
        "icon": str(app / "Contents/Resources/swiftify.icns"),
        "background": str(background),
        "window_rect": ((200, 160), DMG_WINDOW),
        "default_view": "icon-view",
        "show_status_bar": False,
        "show_tab_view": False,
        "show_toolbar": False,
        "show_pathbar": False,
        "show_sidebar": False,
        "show_icon_preview": False,
        "arrange_by": None,
        "icon_size": 128,
        "text_size": 13,
        "label_pos": "bottom",
        "hide_extensions": [app.name],
        "icon_locations": {app.name: DMG_APP_SLOT, "Applications": DMG_APPLICATIONS_SLOT},
    }
    for attempt in range(1, 4):
        image = scratch / f"installer-{attempt}.dmg"
        try:
            dmgbuild.build_dmg(str(image), "BetterSwiftify", settings=settings, detach_retries=10)
        except Exception as error:
            if attempt == 3 or "busy" not in str(error).lower():
                raise
            print(f"DMG device temporarily busy; retrying ({attempt}/3).", file=sys.stderr)
            time.sleep(5 * attempt)
        else:
            os.replace(image, destination)
            return


def styled_dmg_available():
    if not (DMG_ART / "background.png").is_file():
        return False
    try:
        import dmgbuild  # noqa: F401
    except ImportError:
        return False
    return True


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
            scratch = Path(temporary)
            staging = scratch / "contents"
            staging.mkdir()
            app = staging / "Swiftify.app"
            subprocess.run(["ditto", str(root), str(app)], check=True)
            if styled_dmg_available():
                create_styled_dmg(app, destination, scratch)
            elif os.environ.get("SWIFTIFY_REQUIRE_STYLED_DMG"):
                raise RuntimeError("dmgbuild and the installer art are required for the release DMG")
            else:
                (staging / "Applications").symlink_to("/Applications")
                create_dmg(staging, destination, scratch)
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
