#!/usr/bin/env python3
"""
raster_ttf.py — Rasterize a TTF pixel font to C array for test_4px.c

Usage:
  python3 tools/raster_ttf.py tools/ikkle4.ttf [pixel_size]

Renders each ASCII char (32-127) at the given pixel size, extracts the
4px-wide bitmap, and outputs a C array suitable for inclusion in test_4px.c.

Default pixel_size: 8 (adjust until the font renders at its native pixel grid)
"""

import sys
from PIL import Image, ImageFont, ImageDraw

def rasterize_font(ttf_path, px_size=8):
    """Rasterize a TTF font and extract per-character bitmaps."""
    font = ImageFont.truetype(ttf_path, px_size)

    # First pass: determine actual glyph height
    max_h = 0
    for ch in range(33, 127):
        bbox = font.getbbox(chr(ch))
        if bbox:
            h = bbox[3] - bbox[1]
            if h > max_h:
                max_h = h

    print(f"Font: {ttf_path}, render size: {px_size}px, max glyph height: {max_h}px")

    glyphs = {}
    for ch in range(32, 128):
        c = chr(ch)
        # Render to small image
        img = Image.new('1', (16, 16), 0)
        draw = ImageDraw.Draw(img)
        draw.text((0, 0), c, font=font, fill=1)

        # Find bounding box of actual pixels
        pixels = img.load()
        # Extract 4px wide column, full height
        rows = []
        for y in range(min(max_h + 2, 16)):
            row = []
            for x in range(4):
                row.append(1 if pixels[x, y] else 0)
            rows.append(tuple(row))

        glyphs[ch] = rows[:max_h] if max_h <= 8 else rows[:8]

    return glyphs, max_h

def output_c_array(glyphs, height, name="ikkle_font"):
    """Output C array: height bytes per char, pixel pattern in high nibble."""
    total = 96 * height
    print(f"\n// {name}: {height} bytes per char, {total} bytes total")
    print(f"// Each byte: pixel pattern (bits 7-4 = pixels 0-3)")
    print(f"#define IKKLE_HEIGHT {height}")
    print(f"static const uint8_t {name}[{total}] = {{")

    line = "    "
    for ch in range(32, 128):
        rows = glyphs.get(ch, [(0,0,0,0)] * height)
        for i in range(height):
            r = rows[i] if i < len(rows) else (0,0,0,0)
            byte = (r[0] << 7) | (r[1] << 6) | (r[2] << 5) | (r[3] << 4)
            line += f"0x{byte:02X},"
        if (ch - 31) % 4 == 0:
            print(line)
            line = "    "

    if line.strip():
        print(line)
    print("};")

def show_preview(glyphs, height):
    """Show ASCII art preview of the font."""
    print(f"\nPreview (height={height}):")
    for ch in range(32, 128):
        rows = glyphs.get(ch, [(0,0,0,0)] * height)
        c = chr(ch) if ch >= 33 else ' '
        print(f"  '{c}' ", end="")
        for i in range(min(height, len(rows))):
            r = rows[i]
            print("".join("#" if b else "." for b in r), end=" ")
        print()

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 tools/raster_ttf.py <font.ttf> [pixel_size]")
        print("  Tries multiple sizes to find the native pixel grid.")
        print("  Output: C array for test_4px.c")
        sys.exit(1)

    ttf_path = sys.argv[1]
    sizes = [int(sys.argv[2])] if len(sys.argv) > 2 else [4, 5, 6, 7, 8, 10, 12]

    for sz in sizes:
        print(f"\n{'='*50}")
        print(f"Trying size {sz}px:")
        try:
            glyphs, height = rasterize_font(ttf_path, sz)
            if height >= 3 and height <= 8:
                show_preview(glyphs, height)
                output_c_array(glyphs, height)
                print(f"\n>>> Size {sz}px gives height {height}px — looks usable!")
        except Exception as e:
            print(f"  Error: {e}")

if __name__ == '__main__':
    main()
