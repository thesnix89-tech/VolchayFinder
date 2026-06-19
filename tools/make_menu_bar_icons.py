"""Build dark-theme menu bar icon variants from base PNG assets."""

from __future__ import annotations

from collections import deque
import os

from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
QML_DIR = os.path.join(ROOT, "src", "MacDockShell", "qml")

STAR_SRC = os.path.join(QML_DIR, "menu_bar_icon_star.png")
STAR_DST = os.path.join(QML_DIR, "menu_bar_icon_star_white.png")


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


def remove_near_black(im: Image.Image, threshold: int = 32) -> Image.Image:
    return flood_transparent(
        im,
        lambda p: p[0] <= threshold and p[1] <= threshold and p[2] <= threshold,
    )


def make_star_white(src: Image.Image) -> Image.Image:
    """Remove dark background and recolor the star silhouette to white."""
    im = remove_near_black(src.convert("RGBA"))
    px = im.load()
    w, h = im.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a == 0:
                continue
            px[x, y] = (255, 255, 255, a)
    return im


def main() -> None:
    if not os.path.exists(STAR_SRC):
        raise FileNotFoundError(f"Missing source icon: {STAR_SRC}")

    star_white = make_star_white(Image.open(STAR_SRC))
    star_white.save(STAR_DST, "PNG")
    print("star", star_white.size, "->", STAR_DST)


if __name__ == "__main__":
    main()
