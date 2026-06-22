"""Build Control Center menu bar toggle icons from a source photo."""

from __future__ import annotations

from collections import deque
import os

from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
SOURCE_DIR = os.path.join(ROOT, "tools", "source")
SOURCE_PATH = os.path.join(SOURCE_DIR, "control_center_source.png")
ICONS_DIR = os.path.join(ROOT, "src", "MacDockShell", "icons")
DARK_DST = os.path.join(ICONS_DIR, "control_center_toggles_dark.png")
LIGHT_DST = os.path.join(ICONS_DIR, "control_center_toggles_light.png")

OUT_W = 40
OUT_H = 46


def flood_transparent(im: Image.Image, predicate) -> Image.Image:
    im = im.convert("RGBA")
    w, h = im.size
    px = im.load()
    seen = [[False] * w for _ in range(h)]
    q: deque[tuple[int, int]] = deque()

    def try_push(x: int, y: int) -> None:
        if 0 <= x < w and 0 <= y < h and not seen[y][x] and predicate(px[x, y]):
            seen[y][x] = True
            q.append((x, y))

    for x in range(w):
        try_push(x, 0)
        try_push(x, h - 1)
    for y in range(h):
        try_push(0, y)
        try_push(w - 1, y)

    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            try_push(x + dx, y + dy)

    for y in range(h):
        for x in range(w):
            if seen[y][x]:
                r, g, b, _a = px[x, y]
                px[x, y] = (r, g, b, 0)
    return im


def is_background_pixel(r: int, g: int, b: int, a: int) -> bool:
    if a < 8:
        return True
    if r <= 24 and g <= 24 and b <= 24:
        return True
    if r <= 72 and g <= 72 and b <= 72:
        return True
    avg = (r + g + b) / 3.0
    spread = max(r, g, b) - min(r, g, b)
    return avg <= 88 and spread <= 18


def remove_background(im: Image.Image) -> Image.Image:
    return flood_transparent(
        im,
        lambda p: is_background_pixel(p[0], p[1], p[2], p[3]),
    )


def crop_to_content(im: Image.Image, pad: int = 2) -> Image.Image:
    im = im.convert("RGBA")
    bbox = im.getbbox()
    if not bbox:
        return im
    x0, y0, x1, y1 = bbox
    x0 = max(0, x0 - pad)
    y0 = max(0, y0 - pad)
    x1 = min(im.width, x1 + pad)
    y1 = min(im.height, y1 + pad)
    return im.crop((x0, y0, x1, y1))


def resize_to_target(im: Image.Image) -> Image.Image:
    im = im.convert("RGBA")
    canvas = Image.new("RGBA", (OUT_W, OUT_H), (0, 0, 0, 0))
    im.thumbnail((OUT_W, OUT_H), Image.Resampling.LANCZOS)
    ox = (OUT_W - im.width) // 2
    oy = (OUT_H - im.height) // 2
    canvas.paste(im, (ox, oy), im)
    return canvas


def add_light_theme_stroke(im: Image.Image, stroke_alpha: int = 50) -> Image.Image:
    src = im.convert("RGBA")
    w, h = src.size
    px = src.load()
    stroke = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    spx = stroke.load()

    def is_bright(x: int, y: int) -> bool:
        r, g, b, a = px[x, y]
        return a > 16 and r >= 180 and g >= 180 and b >= 180

    for y in range(h):
        for x in range(w):
            if not is_bright(x, y):
                continue
            for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1), (-1, -1), (1, -1), (-1, 1), (1, 1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and 0 <= ny < h and not is_bright(nx, ny):
                    if spx[nx, ny][3] < stroke_alpha:
                        spx[nx, ny] = (0, 0, 0, stroke_alpha)

    return Image.alpha_composite(stroke, src)


def load_source() -> Image.Image:
    if not os.path.exists(SOURCE_PATH):
        raise FileNotFoundError(
            f"Missing source icon: {SOURCE_PATH}\n"
            "Place your removebg photo there as control_center_source.png and rerun."
        )
    return Image.open(SOURCE_PATH)


def main() -> None:
    os.makedirs(ICONS_DIR, exist_ok=True)
    src = load_source()
    dark = resize_to_target(crop_to_content(remove_background(src)))
    light = add_light_theme_stroke(dark)
    dark.save(DARK_DST, "PNG")
    light.save(LIGHT_DST, "PNG")
    print("source", SOURCE_PATH)
    print("dark", dark.size, "->", DARK_DST)
    print("light", light.size, "->", LIGHT_DST)


if __name__ == "__main__":
    main()
