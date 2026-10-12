from pathlib import Path
import struct

from PIL import Image

SCREEN_W = 640
WALLPAPER_H = 460
FIRST = 64
COLORS = 192
DEFAULT = "day"


def convert(source):
    img = Image.open(source).convert("RGB")
    # drawn in paint, so blocky pixels. lanczos smears the edges
    img = img.resize((SCREEN_W, WALLPAPER_H), Image.NEAREST)
    quantized = img.quantize(colors=COLORS, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    palette = quantized.getpalette()[:COLORS * 3]
    palette += [0] * (COLORS * 3 - len(palette))
    header = b"WPAL" + struct.pack("<HH", FIRST, COLORS)
    six_bit = bytes(round(value * 63 / 255) for value in palette)
    pixels = bytes(FIRST + index for index in quantized.getdata())
    return header + six_bit + pixels


def main():
    out = Path("build/wallpapers")
    out.mkdir(parents=True, exist_ok=True)
    for old in out.glob("*.raw"):
        old.unlink()
    for source in sorted(Path("wallpapers").glob("*.png")):
        data = convert(source)
        (out / (source.stem + ".raw")).write_bytes(data)
        if source.stem == DEFAULT:
            Path("build/wallpaper.raw").write_bytes(data)
        print("Created wallpaper %s: %d bytes" % (source.stem, len(data)))


if __name__ == "__main__":
    main()
