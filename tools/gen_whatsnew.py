#!/usr/bin/env python3
"""
Generate whatsnew_data.h from release/ assets.
Converts logo PNG to monochrome ZX bitmap and packs changelog text.

Usage: python tools/gen_whatsnew.py
Reads:  release/logo.png, release/changes.txt, release/version.txt
 Writes: overlay/whatsnew_data.h

For each new version: update the 3 files in release/, run make.
"""

import struct
import sys
import zlib
from pathlib import Path


def _paeth(a, b, c):
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def _png_luma8(png_path):
    """Decode a simple 8-bit PNG to a grayscale pixel matrix without Pillow."""
    data = Path(png_path).read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG file")

    pos = 8
    width = height = bit_depth = color_type = None
    palette = None
    idat = bytearray()

    while pos + 8 <= len(data):
        length = struct.unpack(">I", data[pos : pos + 4])[0]
        chunk = data[pos + 4 : pos + 8]
        payload = data[pos + 8 : pos + 8 + length]
        pos += 12 + length
        if chunk == b"IHDR":
            width, height, bit_depth, color_type, comp, filt, interlace = struct.unpack(
                ">IIBBBBB", payload
            )
            if bit_depth != 8 or comp != 0 or filt != 0 or interlace != 0:
                raise ValueError("unsupported PNG format")
        elif chunk == b"PLTE":
            palette = [payload[i : i + 3] for i in range(0, len(payload), 3)]
        elif chunk == b"IDAT":
            idat.extend(payload)
        elif chunk == b"IEND":
            break

    if width is None or height is None:
        raise ValueError("missing PNG header")
    if color_type == 0:
        channels = 1
    elif color_type == 2:
        channels = 3
    elif color_type == 3:
        channels = 1
        if palette is None:
            raise ValueError("indexed PNG without palette")
    elif color_type == 4:
        channels = 2
    elif color_type == 6:
        channels = 4
    else:
        raise ValueError("unsupported PNG color type")

    raw = zlib.decompress(bytes(idat))
    stride = width * channels
    rows = []
    prev = [0] * stride
    p = 0
    for _ in range(height):
        filter_type = raw[p]
        p += 1
        src = raw[p : p + stride]
        p += stride
        recon = [0] * stride
        for i, value in enumerate(src):
            left = recon[i - channels] if i >= channels else 0
            up = prev[i]
            up_left = prev[i - channels] if i >= channels else 0
            if filter_type == 0:
                pred = 0
            elif filter_type == 1:
                pred = left
            elif filter_type == 2:
                pred = up
            elif filter_type == 3:
                pred = (left + up) >> 1
            elif filter_type == 4:
                pred = _paeth(left, up, up_left)
            else:
                raise ValueError("bad PNG filter")
            recon[i] = (value + pred) & 0xFF
        prev = recon

        row = []
        for x in range(width):
            off = x * channels
            if color_type == 0:
                lum = recon[off]
            elif color_type == 3:
                r, g, b = palette[recon[off]]
                lum = (299 * r + 587 * g + 114 * b + 500) // 1000
            elif color_type == 4:
                lum = recon[off]
            else:
                r, g, b = recon[off], recon[off + 1], recon[off + 2]
                lum = (299 * r + 587 * g + 114 * b + 500) // 1000
            row.append(lum)
        rows.append(row)

    return rows, width, height


def _resize_nearest(rows, width, height, new_w, new_h):
    if width == new_w and height == new_h:
        return rows
    scaled = []
    for y in range(new_h):
        src_y = min(height - 1, (y * height) // new_h)
        src = rows[src_y]
        scaled.append([src[min(width - 1, (x * width) // new_w)] for x in range(new_w)])
    return scaled


def convert_logo(png_path, max_w=96, max_h=88, threshold=40):
    """Convert PNG to monochrome ZX bitmap. Returns (bytes, width_bytes, height)."""
    try:
        from PIL import Image

        img = Image.open(png_path).convert("L")
        ratio = min(max_w / img.width, max_h / img.height)
        new_w = int(img.width * ratio) & ~7  # align to 8px
        new_h = int(img.height * ratio)
        if new_w == 0:
            new_w = 8
        img = img.resize((new_w, new_h), Image.LANCZOS)
        pixels = img.load()
        get_pixel = lambda x, y: pixels[x, y]
    except ModuleNotFoundError:
        rows, width, height = _png_luma8(png_path)
        ratio = min(max_w / width, max_h / height)
        new_w = int(width * ratio) & ~7
        new_h = int(height * ratio)
        if new_w == 0:
            new_w = 8
        rows = _resize_nearest(rows, width, height, new_w, new_h)
        get_pixel = lambda x, y: rows[y][x]

    result = bytearray()
    for y in range(new_h):
        for bx in range(new_w // 8):
            byte = 0
            for bit in range(8):
                if get_pixel(bx * 8 + bit, y) > threshold:
                    byte |= 0x80 >> bit
            result.append(byte)

    return bytes(result), new_w // 8, new_h


def pack_logo_rows(raw, width_bytes, height):
    """Pack each logo row as 16-bit byte mask followed by non-zero bytes."""
    packed = bytearray()
    for y in range(height):
        row = raw[y * width_bytes : (y + 1) * width_bytes]
        mask = 0
        nz = []
        for i, b in enumerate(row):
            if b:
                mask |= 1 << i
                nz.append(b)
        packed.append(mask & 0xFF)
        packed.append((mask >> 8) & 0xFF)
        packed.extend(nz)
    return bytes(packed)


def main():
    base = Path("release")
    out = Path("overlay/whatsnew_data.h")

    # Read version
    version = (base / "version.txt").read_text().strip()

    # Read changes
    changes = [
        l.strip() for l in (base / "changes.txt").read_text().splitlines() if l.strip()
    ]

    # Convert logo
    logo_data, logo_wb, logo_h = convert_logo(base / "logo.png")
    logo_packed = pack_logo_rows(logo_data, logo_wb, logo_h)

    # Generate C header
    lines = [
        "/* AUTO-GENERATED by gen_whatsnew.py -- DO NOT EDIT */",
        "",
        f'#define WN_VERSION "{version}"',
        f"#define WN_LOGO_WB {logo_wb}   /* bytes per row ({logo_wb * 8}px) */",
        f"#define WN_LOGO_H  {logo_h}   /* rows */",
        f"#define WN_LOGO_PACKED_SIZE {len(logo_packed)}",
        f"#define WN_CHANGES {len(changes)}",
        "",
        f"static const uint8_t wn_logo_packed[{len(logo_packed)}] = {{",
    ]

    # Logo data in rows: 16-bit byte mask, then the non-zero bytes.
    for i in range(0, len(logo_packed), 16):
        chunk = logo_packed[i : i + 16]
        hex_str = ", ".join(f"0x{b:02X}" for b in chunk)
        lines.append(f"    {hex_str},")

    lines.append("};")
    lines.append("")

    # Changes as packed null-terminated strings
    lines.append("/* Packed changelog: null-terminated entries, walk with pointer */")
    lines.append("static const char wn_changes[] =")
    for c in changes:
        escaped = c.replace('"', '\\"')
        lines.append(f'    "{escaped}\\0"')
    lines.append(";")

    out.write_text("\n".join(lines) + "\n")
    print(
        f"  whatsnew_data.h: logo {logo_wb * 8}x{logo_h} "
        f"({len(logo_data)}B raw, {len(logo_packed)}B packed), "
        f'{len(changes)} changes, version "{version}"'
    )


if __name__ == "__main__":
    main()
