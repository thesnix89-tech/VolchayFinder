"""Build dock trash icons from macOS-style source renders."""

from __future__ import annotations

from collections import deque
import os

from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
DST = os.path.join(ROOT, "src", "MacDockShell", "icons")
HOME = os.path.expanduser("~")
DOWNLOADS = os.path.join(HOME, "Downloads")

FULL_SOURCE = os.path.join(DOWNLOADS, "Image.png")
EMPTY_SOURCE = os.path.join(DOWNLOADS, "Image321.png")


def trim(im: Image.Image, pad_frac: float = 0.06) -> Image.Image:
    bbox = im.split()[3].getbbox()
    if bbox:
        im = im.crop(bbox)
    w, h = im.size
    size = max(w, h)
    pad = int(size * pad_frac)
    canvas = Image.new("RGBA", (size + 2 * pad, size + 2 * pad), (0, 0, 0, 0))
    canvas.paste(im, ((canvas.width - w) // 2, (canvas.height - h) // 2), im)
    return canvas


def flood_transparent(
    im: Image.Image,
    predicate,
) -> Image.Image:
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


def remove_near_black(im: Image.Image, threshold: int = 32) -> Image.Image:
    return flood_transparent(
        im,
        lambda p: p[0] <= threshold and p[1] <= threshold and p[2] <= threshold,
    )


def remove_blue_halo(im: Image.Image) -> Image.Image:
    """Drop low-alpha blue fringe left over from keyed black backgrounds."""
    im = im.convert("RGBA")
    px = im.load()
    w, h = im.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a == 0:
                continue
            if a < 24:
                px[x, y] = (0, 0, 0, 0)
                continue
            if b > r + 18 and b > g + 12 and (r + g + b) / 3 < 120:
                px[x, y] = (0, 0, 0, 0)
    return im


def load_empty_source() -> Image.Image:
    if not os.path.exists(EMPTY_SOURCE):
        raise FileNotFoundError(f"Missing empty trash source: {EMPTY_SOURCE}")
    return remove_blue_halo(remove_near_black(Image.open(EMPTY_SOURCE).convert("RGBA")))


def load_full_source() -> Image.Image:
    if not os.path.exists(FULL_SOURCE):
        raise FileNotFoundError(f"Missing full trash source: {FULL_SOURCE}")
    return remove_near_black(Image.open(FULL_SOURCE).convert("RGBA"))


def main() -> None:
    os.makedirs(DST, exist_ok=True)

    empty = trim(load_empty_source())
    full = trim(load_full_source())

    empty_path = os.path.join(DST, "trash_macos_empty.png")
    full_path = os.path.join(DST, "trash_macos_full.png")
    empty.save(empty_path, "PNG")
    full.save(full_path, "PNG")
    print("empty", empty.size, "->", empty_path)
    print("full", full.size, "->", full_path)


if __name__ == "__main__":
    main()
