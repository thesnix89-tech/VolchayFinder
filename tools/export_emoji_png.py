#!/usr/bin/env python3
"""Export selected glyphs from AppleColorEmoji-Windows.ttf to color PNG icons."""

from __future__ import annotations

import io
import sys
from pathlib import Path

try:
    from fontTools.ttLib import TTFont
    from PIL import Image
except ImportError:
    print("Missing dependencies: pip install pillow fonttools")
    sys.exit(1)

REPO_ROOT = Path(__file__).resolve().parent.parent
FONT_PATH = REPO_ROOT / "src" / "MacDockShell" / "fonts" / "emoji" / "AppleColorEmoji-Windows.ttf"
OUTPUT_DIR = REPO_ROOT / "src" / "MacDockShell" / "icons"

EXPORTS = [
    ("emoji_laptop.png", 0x1F4BB),
    ("emoji_folder.png", 0x1F4C1),
    ("emoji_trash.png", 0x1F5D1),
    ("emoji_downloads.png", 0x2B07),
]

OUTPUT_SIZE = 64


def extract_png_blob(data: bytes) -> Image.Image | None:
    png_start = data.find(b"\x89PNG\r\n\x1a\n")
    if png_start < 0:
        return None
    return Image.open(io.BytesIO(data[png_start:]))


def render_glyph(font: TTFont, codepoint: int) -> Image.Image:
    cmap = font.getBestCmap()
    glyph_name = cmap.get(codepoint)
    if not glyph_name:
        raise RuntimeError(f"No glyph for U+{codepoint:04X}")

    best_image: Image.Image | None = None
    best_pixels = 0
    for strike in font["CBDT"].strikeData:
        if glyph_name not in strike:
            continue
        candidate = extract_png_blob(strike[glyph_name].data)
        if candidate is None:
            continue
        pixels = candidate.size[0] * candidate.size[1]
        if pixels > best_pixels:
            best_image = candidate.convert("RGBA")
            best_pixels = pixels

    if best_image is None:
        raise RuntimeError(f"No CBDT bitmap for U+{codepoint:04X}")

    fitted = Image.new("RGBA", (OUTPUT_SIZE, OUTPUT_SIZE), (0, 0, 0, 0))
    scale = min(OUTPUT_SIZE / best_image.width, OUTPUT_SIZE / best_image.height)
    new_w = max(1, int(best_image.width * scale))
    new_h = max(1, int(best_image.height * scale))
    resized = best_image.resize((new_w, new_h), Image.Resampling.LANCZOS)
    offset = ((OUTPUT_SIZE - new_w) // 2, (OUTPUT_SIZE - new_h) // 2)
    fitted.paste(resized, offset, resized)
    return fitted


def main() -> int:
    if not FONT_PATH.is_file():
        print(f"Font not found: {FONT_PATH}")
        print("Run: powershell -ExecutionPolicy Bypass -File tools\\setup_apple_emoji.ps1")
        return 1

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    font = TTFont(FONT_PATH)

    for filename, codepoint in EXPORTS:
        output_path = OUTPUT_DIR / filename
        image = render_glyph(font, codepoint)
        image.save(output_path, format="PNG")
        print(f"Wrote {output_path} (U+{codepoint:04X}, {image.mode})")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
