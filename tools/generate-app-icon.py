#!/usr/bin/env python3
"""Export the original SVG to app/platform icons (Pillow and CairoSVG required).

Run: python3 tools/generate-app-icon.py [--preview /path/to/contact-sheet.png]
These are development-only dependencies; the application needs neither package.
"""

import argparse
from copy import deepcopy
import io
import json
from pathlib import Path
import xml.etree.ElementTree as ET

import cairosvg
from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
ICONS = ROOT / "packaging/icons"
MASTER = ICONS / "betterswiftify.svg"
SIZES = (32, 48, 64, 128, 256, 512)


def raster(source: bytes, size: int) -> Image.Image:
    # Supersampling keeps the small note geometry legible without hard pixel edges.
    png = cairosvg.svg2png(bytestring=source, output_width=size * 4, output_height=size * 4)
    return Image.open(io.BytesIO(png)).convert("RGBA").resize((size, size), Image.Resampling.LANCZOS)


def export_native_layer(source: bytes) -> None:
    ET.register_namespace("", "http://www.w3.org/2000/svg")
    svg = ET.fromstring(source)
    background = next(node for node in svg if node.get("id") == "background")
    native = ICONS / "swiftify.icon"
    layers = []
    for identifier, filename, name in (
        ("mark", "betterswiftify-mark.svg", "BetterSwiftify expressive star"),
        ("notes", "betterswiftify-notes.svg", "BetterSwiftify joined notes"),
    ):
        mark = next(node for node in svg if node.get("id") == identifier)
        layer = ET.Element(svg.tag, svg.attrib)
        layer.append(deepcopy(mark))
        (native / "Assets" / filename).write_bytes(ET.tostring(layer, encoding="utf-8", xml_declaration=True))
        layers.append({
            "image-name": filename,
            "name": name,
            "fill": {"solid": mark.get("fill")},
            "glass": False,
            "position": {"scale": 1, "translation-in-points": [0, 0]},
        })
    # Icon Composer supplies the platform mask. Separate foreground layers keep
    # its per-layer fill override from painting the dark notes lavender.
    manifest = {
        "fill": {"solid": background.get("fill")},
        "groups": [{
            "layers": layers,
            "shadow": {"kind": "neutral", "opacity": 0},
            "specular": False,
            "translucency": {"enabled": False, "value": 0},
            "refractivity": {"enabled": False, "depth": 0, "strength": 0},
        }],
        "supported-platforms": {"squares": "shared"},
    }
    # Icon Composer's color encoding is explicit sRGB, not Display P3.
    for fill in (manifest["fill"], *(layer["fill"] for layer in layers)):
        color = fill["solid"].lstrip("#")
        components = [int(color[i:i + 2], 16) / 255 for i in (0, 2, 4)]
        fill["solid"] = "srgb:" + ",".join(f"{value:.5f}" for value in (*components, 1))
    (native / "icon.json").write_text(json.dumps(manifest, indent=2) + "\n")


def contact_sheet(source: bytes, destination: Path) -> None:
    sizes = (16, 32, 64, 128, 512)
    widths = (100, 100, 120, 184, 576)
    sheet = Image.new("RGB", (sum(widths) + 64, 1400), "#efedf1")
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default(size=18)
    title_font = ImageFont.load_default(size=25)
    for row, (background, foreground, label) in enumerate((
        ("#efedf1", "#29232f", "LIGHT NEUTRAL"),
        ("#242129", "#ede6f7", "DARK NEUTRAL"),
    )):
        y = row * 700
        draw.rectangle((0, y, sheet.width, y + 700), fill=background)
        draw.text((32, y + 26), "BetterSwiftify / expressive star and notes", font=title_font, fill=foreground)
        draw.text((32, y + 68), label + "  ·  actual pixel sizes", font=font, fill=foreground)
        x = 32
        for size, width in zip(sizes, widths):
            icon = raster(source, size)
            sheet.paste(icon, (x + (width - size) // 2, y + 118 + (512 - size) // 2), icon)
            draw.text((x + width // 2, y + 670), f"{size} px", font=font, fill=foreground, anchor="mm")
            x += width
    destination.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(destination)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preview", type=Path)
    args = parser.parse_args()
    source = MASTER.read_bytes()
    for size in SIZES:
        raster(source, size).save(ICONS / f"swiftify-{size}.png")
    raster(source, 1024).save(ROOT / "resources/swiftify-icon.png")
    ico_sizes = (16, 24, 32, 48, 64, 128, 256)
    # Supply individually supersampled frames instead of resizing the 256px
    # bitmap again inside Pillow's ICO writer.
    raster(source, 256).save(
        ICONS / "swiftify.ico",
        sizes=[(size, size) for size in ico_sizes],
        append_images=[raster(source, size) for size in ico_sizes[:-1]],
    )
    export_native_layer(source)
    if args.preview:
        contact_sheet(source, args.preview)


if __name__ == "__main__":
    main()
