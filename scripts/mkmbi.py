#!/usr/bin/env python3
"""Convert simple images to Monarch Bitmap Image (MBI1).

MBI1 header:
    u8  magic[4] = "MBI1"
    u32 width
    u32 height
    u32 pitch       # bytes per row of encoded pixel/index data
    u32 format      # 1=XRGB8888, 2=INDEX8, 3=INDEX4
    u32 data_size   # payload bytes after this 24-byte header

Payloads:
    XRGB8888: raw little-endian u32 pixels 0x00RRGGBB
    INDEX8:   u32 colors + u32 palette[colors] + u8 indices[pitch*height]
    INDEX4:   u32 colors + u32 palette[colors] + packed nibbles[pitch*height]
              high nibble is the even x pixel, low nibble is the odd x pixel.

Supported input formats without third-party dependencies:
- BMP BI_RGB: 1/4/8-bit paletted, 24-bit BGR, 32-bit BGRX/BGRA
- PPM P3 ASCII
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

FORMAT_XRGB8888 = 1
FORMAT_INDEX8 = 2
FORMAT_INDEX4 = 3


def le16(data: bytes, off: int) -> int:
    return struct.unpack_from("<H", data, off)[0]


def le32(data: bytes, off: int) -> int:
    return struct.unpack_from("<I", data, off)[0]


def sle32(data: bytes, off: int) -> int:
    return struct.unpack_from("<i", data, off)[0]


def load_bmp(path: Path) -> tuple[int, int, list[int]]:
    data = path.read_bytes()
    if len(data) < 54 or data[:2] != b"BM":
        raise SystemExit(f"not a BMP file: {path}")

    pixel_offset = le32(data, 10)
    dib_size = le32(data, 14)
    if dib_size < 40 or 14 + dib_size > len(data) or pixel_offset > len(data):
        raise SystemExit("unsupported or corrupt BMP header")

    width = sle32(data, 18)
    height_signed = sle32(data, 22)
    planes = le16(data, 26)
    bpp = le16(data, 28)
    compression = le32(data, 30)
    colors_used = le32(data, 46)

    if planes != 1 or compression != 0 or width <= 0 or height_signed == 0:
        raise SystemExit("only uncompressed BI_RGB BMP images are supported")

    topdown = height_signed < 0
    height = -height_signed if topdown else height_signed
    if bpp not in (1, 4, 8, 24, 32):
        raise SystemExit(f"unsupported BMP bpp: {bpp}")

    row_bytes = ((width * bpp + 31) // 32) * 4
    if pixel_offset + row_bytes * height > len(data):
        raise SystemExit("BMP pixel data is truncated")

    palette: list[int] = []
    if bpp <= 8:
        colors = colors_used or (1 << bpp)
        pal_off = 14 + dib_size
        if pal_off + colors * 4 > pixel_offset:
            raise SystemExit("BMP palette overlaps pixel data")
        for i in range(colors):
            b, g, r, _ = data[pal_off + i * 4: pal_off + i * 4 + 4]
            palette.append((r << 16) | (g << 8) | b)

    pixels = [0] * (width * height)
    base = pixel_offset

    for y in range(height):
        sy = y if topdown else height - 1 - y
        row = data[base + sy * row_bytes: base + (sy + 1) * row_bytes]
        for x in range(width):
            if bpp == 32:
                b, g, r, _ = row[x * 4: x * 4 + 4]
                color = (r << 16) | (g << 8) | b
            elif bpp == 24:
                b, g, r = row[x * 3: x * 3 + 3]
                color = (r << 16) | (g << 8) | b
            elif bpp == 8:
                color = palette[row[x]]
            elif bpp == 4:
                v = row[x >> 1]
                idx = (v & 0x0F) if (x & 1) else (v >> 4)
                color = palette[idx]
            else:  # 1 bpp
                v = row[x >> 3]
                idx = (v >> (7 - (x & 7))) & 1
                color = palette[idx]
            pixels[y * width + x] = color

    return width, height, pixels


def ppm_tokens(text: str):
    for raw in text.splitlines():
        line = raw.split("#", 1)[0]
        for token in line.split():
            yield token


def load_ppm(path: Path) -> tuple[int, int, list[int]]:
    tokens = iter(ppm_tokens(path.read_text()))
    try:
        magic = next(tokens)
        if magic != "P3":
            raise SystemExit("only PPM P3 is supported")
        width = int(next(tokens))
        height = int(next(tokens))
        maxval = int(next(tokens))
    except StopIteration as exc:
        raise SystemExit("truncated PPM header") from exc

    if width <= 0 or height <= 0 or maxval <= 0:
        raise SystemExit("invalid PPM dimensions")

    pixels: list[int] = []
    for _ in range(width * height):
        try:
            r = int(next(tokens))
            g = int(next(tokens))
            b = int(next(tokens))
        except StopIteration as exc:
            raise SystemExit("truncated PPM pixel data") from exc
        r = max(0, min(maxval, r)) * 255 // maxval
        g = max(0, min(maxval, g)) * 255 // maxval
        b = max(0, min(maxval, b)) * 255 // maxval
        pixels.append((r << 16) | (g << 8) | b)

    return width, height, pixels


def palette_for(pixels: list[int]) -> tuple[list[int], dict[int, int]]:
    palette: list[int] = []
    lookup: dict[int, int] = {}
    for color in pixels:
        if color not in lookup:
            lookup[color] = len(palette)
            palette.append(color)
    return palette, lookup


def choose_format(pixels: list[int], requested: str) -> int:
    if requested == "xrgb8888":
        return FORMAT_XRGB8888
    palette, _ = palette_for(pixels)
    if requested == "index4":
        if len(palette) > 16:
            raise SystemExit("index4 requested but image has more than 16 colors")
        return FORMAT_INDEX4
    if requested == "index8":
        if len(palette) > 256:
            raise SystemExit("index8 requested but image has more than 256 colors")
        return FORMAT_INDEX8
    # auto
    if len(palette) <= 16:
        return FORMAT_INDEX4
    if len(palette) <= 256:
        return FORMAT_INDEX8
    return FORMAT_XRGB8888


def write_mbi(path: Path, width: int, height: int, pixels: list[int], requested_format: str) -> int:
    fmt = choose_format(pixels, requested_format)
    payload = bytearray()

    if fmt == FORMAT_XRGB8888:
        pitch = width * 4
        for color in pixels:
            payload += struct.pack("<I", color & 0x00FFFFFF)
    else:
        palette, lookup = palette_for(pixels)
        payload += struct.pack("<I", len(palette))
        for color in palette:
            payload += struct.pack("<I", color & 0x00FFFFFF)

        if fmt == FORMAT_INDEX8:
            pitch = width
            for color in pixels:
                payload.append(lookup[color])
        else:
            pitch = (width + 1) // 2
            for y in range(height):
                row = pixels[y * width: (y + 1) * width]
                for x in range(0, width, 2):
                    hi = lookup[row[x]] & 0x0F
                    lo = lookup[row[x + 1]] & 0x0F if x + 1 < width else 0
                    payload.append((hi << 4) | lo)

    header = b"MBI1" + struct.pack("<IIIII", width, height, pitch, fmt, len(payload))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + payload)
    return fmt


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert BMP/PPM images to MBI1")
    parser.add_argument("input", type=Path)
    parser.add_argument("-o", "--output", type=Path, required=True)
    parser.add_argument("--format", choices=["auto", "xrgb8888", "index8", "index4"], default="auto")
    args = parser.parse_args()

    suffix = args.input.suffix.lower()
    if suffix == ".bmp":
        width, height, pixels = load_bmp(args.input)
    elif suffix in (".ppm", ".pnm"):
        width, height, pixels = load_ppm(args.input)
    else:
        raise SystemExit(f"unsupported input extension: {suffix}")

    fmt = write_mbi(args.output, width, height, pixels, args.format)
    names = {FORMAT_XRGB8888: "xrgb8888", FORMAT_INDEX8: "index8", FORMAT_INDEX4: "index4"}
    print(f"mkmbi: wrote {args.output} ({width}x{height}, {names[fmt]}, {args.output.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
