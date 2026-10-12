from pathlib import Path
import struct

from PIL import Image

SCREEN_W = 640
WALLPAPER_H = 460
FIRST = 48
COLORS = 207


def main():
    img = Image.open("serpentis_logo.png").convert("RGB")
    img = img.resize((SCREEN_W, WALLPAPER_H), Image.LANCZOS)
    quantized = img.quantize(colors=COLORS, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    palette = quantized.getpalette()[:COLORS * 3]
    palette += [0] * (COLORS * 3 - len(palette))
    header = b"WPAL" + struct.pack("<HH", FIRST, COLORS)
    six_bit = bytes(round(value * 63 / 255) for value in palette)
    pixels = bytes(FIRST + index for index in quantized.getdata())
    Path("build/wallpaper.raw").write_bytes(header + six_bit + pixels)
    print("Created wallpaper: %d colors, %d bytes" % (COLORS, len(header) + len(six_bit) + len(pixels)))


if __name__ == "__main__":
    main()
