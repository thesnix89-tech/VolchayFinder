from PIL import Image
from collections import deque
import os

dst = r'D:\Src\Hardest\Megushell\src\MacDockShell\icons'
os.makedirs(dst, exist_ok=True)


def edge_flood_transparent(im, thr=246):
    im = im.convert('RGBA')
    w, h = im.size
    px = im.load()
    seen = [[False] * w for _ in range(h)]
    q = deque()

    def near_white(p):
        return p[0] >= thr and p[1] >= thr and p[2] >= thr

    for x in range(w):
        for y in (0, h - 1):
            if not seen[y][x] and near_white(px[x, y]):
                seen[y][x] = True
                q.append((x, y))
    for y in range(h):
        for x in (0, w - 1):
            if not seen[y][x] and near_white(px[x, y]):
                seen[y][x] = True
                q.append((x, y))
    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < w and 0 <= ny < h and not seen[ny][nx] and near_white(px[nx, ny]):
                seen[ny][nx] = True
                q.append((nx, ny))
    for y in range(h):
        for x in range(w):
            if seen[y][x]:
                r, g, b, a = px[x, y]
                px[x, y] = (r, g, b, 0)
    return im


def trim(im, pad_frac=0.06):
    bbox = im.split()[3].getbbox()
    if bbox:
        im = im.crop(bbox)
    w, h = im.size
    s = max(w, h)
    pad = int(s * pad_frac)
    canvas = Image.new('RGBA', (s + 2 * pad, s + 2 * pad), (0, 0, 0, 0))
    canvas.paste(im, ((canvas.width - w) // 2, (canvas.height - h) // 2), im)
    return canvas


home = os.path.expanduser('~')
full = edge_flood_transparent(Image.open(os.path.join(home, 'Downloads', 'trash-can-macos.jpg')))
full = trim(full)
full.save(os.path.join(dst, 'trash_macos_full.png'))

empty = Image.open(os.path.join(
    home, 'Downloads',
    'png-clipart-white-bucket-illustration-trash-computer-icons-os-x-yosemite-trash-can-glass-white-thumbnail.png')).convert('RGBA')
empty = trim(empty)
empty.save(os.path.join(dst, 'trash_macos_empty.png'))
print('full', full.size, 'empty', empty.size)
