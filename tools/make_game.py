# turns game/scenes.json and the paint drawings in game/ into what snakehouse reads
#
# build/game/<scene>.pic  624x404 bytes, one fixed ui color (48-63) per pixel, rows top down
# build/game/game.dat     all little endian, offsets from the start of the file
#   header 32 bytes: 'SNKH', u16 version, u16 scenes, u16 start, u16 hotspots,
#                    u32 lines, u32 scene table, u32 hotspot table, u32 line table, u32 strings
#   scene   8 bytes: u32 path, u16 first hotspot, u16 hotspot count
#   hotspot 16 bytes: u16 x0 y0 x1 y1 (x1 y1 not included), u16 first line, u16 line count,
#                     u16 goto scene or 0xffff, u16 flags (1 = stay on the last line)
#   line    4 bytes: u32 offset of a zero terminated string, rows split by \n

from pathlib import Path
import json
import re
import struct
import sys
import textwrap

from PIL import Image

WIDTH = 624
HEIGHT = 404
ROW_CHARS = 73
HOTSPOT_MAX = 512
NO_SCENE = 0xFFFF
# PALETTE_FIXED in config.inc, the ui colors that don't change with the theme
FIXED = 48

# the default theme, 6 bit like the kernel table in vga12.asm
UI_PALETTE = [
    (0, 0, 0), (0, 0, 42), (0, 42, 0), (0, 42, 42), (42, 0, 0), (42, 0, 42), (42, 21, 0), (42, 42, 42),
    (21, 21, 21), (21, 21, 63), (21, 63, 21), (21, 63, 63), (63, 21, 21), (63, 21, 63), (63, 63, 21), (63, 63, 63),
]
RGB = [tuple((c << 2) | (c >> 4) for c in color) for color in UI_PALETTE]

SOURCE = Path("game")
OUT = Path("build/game")


def nearest(color):
    r, g, b = color[:3]
    return min(range(16), key=lambda i: (RGB[i][0] - r) ** 2 + (RGB[i][1] - g) ** 2 + (RGB[i][2] - b) ** 2)


def fixed_pixels(img):
    table = {bytes(color): nearest(color) for _, color in img.getcolors(img.width * img.height)}
    raw = img.tobytes()
    return bytes(FIXED + table[raw[i:i + 3]] for i in range(0, len(raw), 3))


def picture(path):
    img = Image.open(path).convert("RGB")
    if img.size != (WIDTH, HEIGHT):
        print("warning: %s is %dx%d, scaled to %dx%d" % (path, *img.size, WIDTH, HEIGHT))
        img = img.resize((WIDTH, HEIGHT), Image.NEAREST)
    return fixed_pixels(img)


# build/drawings/<name>.pic for paint and Files: 'SPIC', u16 width, u16 height, then the pixels
def drawings():
    out = Path("build/drawings")
    out.mkdir(parents=True, exist_ok=True)
    for old in out.glob("*"):
        old.unlink()
    for source in sorted(Path("drawings").glob("*.png")):
        img = Image.open(source).convert("RGB")
        img.thumbnail((WIDTH, HEIGHT), Image.NEAREST)
        header = b"SPIC" + struct.pack("<HH", img.width, img.height)
        (out / (source.stem + ".pic")).write_bytes(header + fixed_pixels(img))


def rows(text):
    if not text.isascii():
        raise SystemExit("only plain ascii in the lines: %r" % text)
    out = []
    for part in text.split("\n"):
        out += textwrap.wrap(part, ROW_CHARS) or [""]
    return "\n".join(out)


def main():
    spec = json.loads((SOURCE / "scenes.json").read_text())
    names = list(spec["scenes"])
    for name in names:
        if not re.fullmatch(r"[a-z0-9_]+", name):
            raise SystemExit("scene names are lowercase letters, digits and _: %r" % name)
    index = {name: i for i, name in enumerate(names)}
    if spec["start"] not in index:
        raise SystemExit("start scene %r does not exist" % spec["start"])

    OUT.mkdir(parents=True, exist_ok=True)
    for old in OUT.glob("*"):
        old.unlink()

    strings = bytearray()

    def string(text):
        at = len(strings)
        strings.extend(text.encode() + b"\0")
        return at

    scenes, hotspots, lines = [], [], []
    for name in names:
        scene = spec["scenes"][name]
        (OUT / (name + ".pic")).write_bytes(picture(SOURCE / scene["image"]))
        first = len(hotspots)
        for spot in scene.get("hotspots", []):
            x0, y0, x1, y1 = spot["rect"]
            if not (0 <= x0 < x1 <= WIDTH and 0 <= y0 < y1 <= HEIGHT):
                raise SystemExit("%s: rect %r is outside the picture" % (name, spot["rect"]))
            target = spot.get("goto")
            if target is not None and target not in index:
                raise SystemExit("%s: goto %r does not exist" % (name, target))
            said = [rows(text) for text in spot.get("say", [])]
            hotspots.append((x0, y0, x1, y1, len(lines), len(said),
                             NO_SCENE if target is None else index[target], 1 if spot.get("once") else 0))
            lines += said
        scenes.append(("/share/game/%s.pic" % name, first, len(hotspots) - first))
    if len(hotspots) > HOTSPOT_MAX:
        raise SystemExit("too many hotspots, snakehouse keeps %d" % HOTSPOT_MAX)

    scene_at = 32
    hotspot_at = scene_at + 8 * len(scenes)
    line_at = hotspot_at + 16 * len(hotspots)
    string_at = line_at + 4 * len(lines)
    scene_paths = [string(path) for path, _, _ in scenes]
    line_offsets = [string(text) for text in lines]

    data = bytearray(struct.pack("<4sHHHHIIIII", b"SNKH", 1, len(scenes), index[spec["start"]], len(hotspots),
                                 len(lines), scene_at, hotspot_at, line_at, string_at))
    for (path, first, count), at in zip(scenes, scene_paths):
        data += struct.pack("<IHH", string_at + at, first, count)
    for spot in hotspots:
        data += struct.pack("<8H", *spot)
    for at in line_offsets:
        data += struct.pack("<I", string_at + at)
    data += strings
    if len(data) > 0x10000:
        raise SystemExit("game.dat is %d bytes, snakehouse reads 64 KB" % len(data))
    (OUT / "game.dat").write_bytes(data)
    drawings()
    print("Created game: %d scenes, %d hotspots, %d lines, %d bytes of data"
          % (len(scenes), len(hotspots), len(lines), len(data)))


if __name__ == "__main__":
    sys.exit(main())
