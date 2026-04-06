#!/usr/bin/env python3
"""
ikkle_compress.py — Analyze Ikkle-4 font data and find optimal compression.
"""

ikkle_font = [
    0x00,0x00,0x00,0x00,0x00,0x88,0x88,0x00,0x88,0x00,0x00,0x88,0x00,0x00,0x00,0xFF,0x99,0xFF,0x00,0x00,
    0xFF,0x99,0xFF,0x00,0x00,0xFF,0x99,0xFF,0x00,0x00,0xFF,0x99,0xFF,0x00,0x00,0x00,0x88,0x00,0x00,0x00,
    0x44,0x88,0x88,0x44,0x00,0x88,0x44,0x44,0x88,0x00,0x00,0x88,0x00,0x00,0x00,0x00,0x44,0xEE,0x44,0x00,
    0x00,0x00,0x00,0xCC,0x00,0x00,0x00,0xCC,0x00,0x00,0x00,0x00,0x00,0x88,0x00,0x22,0x44,0x44,0x88,0x00,
    0xEE,0xAA,0xAA,0xEE,0x00,0xCC,0x44,0x44,0xEE,0x00,0xEE,0x22,0x44,0xEE,0x00,0xEE,0x66,0x22,0xEE,0x00,
    0x88,0xAA,0xEE,0x22,0x00,0xEE,0xCC,0x22,0xCC,0x00,0xEE,0xCC,0xAA,0xEE,0x00,0xEE,0x22,0x44,0x88,0x00,
    0xEE,0xEE,0xAA,0xEE,0x00,0xEE,0xAA,0x66,0xEE,0x00,0x00,0x88,0x00,0x88,0x00,0x00,0x44,0x00,0xCC,0x00,
    0x00,0x66,0x88,0x66,0x00,0x00,0xCC,0x00,0xCC,0x00,0x00,0xCC,0x22,0xCC,0x00,0xEE,0x66,0x00,0x44,0x00,
    0xFF,0x99,0xFF,0x00,0x00,0xEE,0xAA,0xEE,0xAA,0x00,0xEE,0xEE,0xAA,0xEE,0x00,0xCC,0x88,0x88,0xCC,0x00,
    0xCC,0xAA,0xAA,0xEE,0x00,0xEE,0xCC,0x88,0xEE,0x00,0xEE,0x88,0xCC,0x88,0x00,0xEE,0x88,0xAA,0xEE,0x00,
    0xAA,0xEE,0xAA,0xAA,0x00,0x88,0x88,0x88,0x88,0x00,0x44,0x44,0x44,0xCC,0x00,0xAA,0xCC,0xAA,0xAA,0x00,
    0x88,0x88,0x88,0xCC,0x00,0xAA,0xEE,0xEE,0xAA,0x00,0xCC,0xAA,0xAA,0xAA,0x00,0xEE,0xAA,0xAA,0xEE,0x00,
    0xEE,0xAA,0xEE,0x88,0x00,0xEE,0xAA,0xEE,0x22,0x00,0xEE,0xAA,0xCC,0xAA,0x00,0xEE,0xCC,0x22,0xEE,0x00,
    0xEE,0x44,0x44,0x44,0x00,0xAA,0xAA,0xAA,0xEE,0x00,0xAA,0xAA,0xAA,0x44,0x00,0xAA,0xAA,0xEE,0xEE,0x00,
    0xAA,0x44,0x44,0xAA,0x00,0xAA,0xEE,0x44,0x44,0x00,0xEE,0x66,0x88,0xEE,0x00,0xCC,0x88,0x88,0xCC,0x00,
    0x88,0x44,0x44,0x22,0x00,0xCC,0x44,0x44,0xCC,0x00,0x44,0xAA,0x00,0x00,0x00,0x00,0x00,0x00,0xCC,0x00,
    0x88,0x44,0x00,0x00,0x00,0x00,0x66,0xAA,0xEE,0x00,0x00,0x88,0xCC,0xCC,0x00,0x00,0xCC,0x88,0xCC,0x00,
    0x22,0x66,0xAA,0xEE,0x00,0x00,0xEE,0xAA,0xCC,0x00,0x00,0xCC,0xCC,0x88,0x00,0x00,0xCC,0xAA,0xEE,0x00,
    0x88,0xCC,0xAA,0xAA,0x00,0x00,0x88,0x88,0x88,0x00,0x00,0x44,0x44,0xCC,0x00,0x00,0xAA,0xCC,0xAA,0x00,
    0x88,0x88,0x88,0x88,0x00,0x00,0xEE,0xEE,0xAA,0x00,0x00,0xCC,0xAA,0xAA,0x00,0x00,0xEE,0xAA,0xEE,0x00,
    0x00,0xCC,0xCC,0x88,0x00,0x00,0xCC,0xCC,0x44,0x00,0x00,0xCC,0x88,0x88,0x00,0x00,0x66,0x44,0xCC,0x00,
    0x88,0xCC,0x88,0xCC,0x00,0x00,0xAA,0xAA,0xEE,0x00,0x00,0xAA,0xAA,0x44,0x00,0x00,0xAA,0xEE,0xEE,0x00,
    0x00,0xAA,0x44,0xAA,0x00,0x00,0xAA,0xEE,0x22,0x00,0x00,0xCC,0x44,0x66,0x00,0x44,0x88,0x88,0x44,0x00,
    0x00,0x88,0x88,0x88,0x00,0x88,0x44,0x44,0x88,0x00,0x00,0x55,0xAA,0x00,0x00,0xFF,0x99,0xFF,0x00,0x00,
]

print("="*60)
print("IKKLE-4 FONT COMPRESSION ANALYSIS")
print("="*60)
print(f"Raw data: {len(ikkle_font)} bytes (96 chars x 5 bytes)")

# =====================================================================
# 1. Verify high nibble == low nibble for all bytes
# =====================================================================
all_match = True
for i, b in enumerate(ikkle_font):
    hi = (b >> 4) & 0xF
    lo = b & 0xF
    if hi != lo:
        ch = 32 + i // 5
        sc = i % 5
        all_match = False
        print(f"  MISMATCH at char {ch} ('{chr(ch)}') scanline {sc}: 0x{b:02X} (hi={hi:X} lo={lo:X})")

if all_match:
    print("\n[OK] ALL bytes have hi_nibble == lo_nibble!")
    print("     Each scanline is just 4 bits of real data.")
    print("     → Can pack 2 scanlines per byte.")

# =====================================================================
# 2. Verify 5th byte always 0x00
# =====================================================================
gap_ok = all(ikkle_font[i*5+4] == 0 for i in range(96))
print(f"\n[{'OK' if gap_ok else 'FAIL'}] 5th byte (gap) always 0x00: {gap_ok}")

# =====================================================================
# 3. Unique nibble values
# =====================================================================
nibbles = set()
for i in range(96):
    for s in range(4):  # only 4 scanlines, skip gap
        b = ikkle_font[i*5 + s]
        nibbles.add((b >> 4) & 0xF)
unique_nibs = sorted(nibbles)
print(f"\nUnique nibble values: {len(unique_nibs)} → {[f'0x{n:X}' for n in unique_nibs]}")

# =====================================================================
# 4. Approach A: Nibble packing (2 bytes per char)
# =====================================================================
packed_a = []
for i in range(96):
    scans = [(ikkle_font[i*5+s] >> 4) & 0xF for s in range(4)]
    packed_a.append((scans[0] << 4) | scans[1])
    packed_a.append((scans[2] << 4) | scans[3])

print(f"\n--- APPROACH A: Nibble packing ---")
print(f"    Data: {len(packed_a)} bytes")
print(f"    Decoder: ~15 bytes Z80 (nibble * 0x11)")
print(f"    TOTAL: ~{len(packed_a) + 15} bytes")

# =====================================================================
# 5. Approach B: Byte-pair dictionary
# =====================================================================
# Each glyph = 2 packed bytes. Count unique packed bytes.
unique_bytes = sorted(set(packed_a))
print(f"\n--- APPROACH B: Byte-pair dictionary ---")
print(f"    Unique packed bytes: {len(unique_bytes)}")

if len(unique_bytes) <= 16:
    # Could use 4-bit index per packed byte → 1 byte per glyph!
    table_size = len(unique_bytes)
    idx_map = {v: i for i, v in enumerate(unique_bytes)}
    glyph_indices = []
    for i in range(96):
        b0 = packed_a[i*2]
        b1 = packed_a[i*2+1]
        glyph_indices.append((idx_map[b0] << 4) | idx_map[b1])
    total_b = table_size + 96  # table + indices
    print(f"    ≤16 unique → 4-bit indices possible!")
    print(f"    Table: {table_size} bytes + Indices: 96 bytes = {total_b} bytes")
    print(f"    Decoder: ~25 bytes Z80 (table lookup + nibble expand)")
    print(f"    TOTAL: ~{total_b + 25} bytes")
else:
    print(f"    >16 unique bytes → 4-bit index not possible")
    # Try with separate top/bottom halves
    top_bytes = sorted(set(packed_a[i*2] for i in range(96)))
    bot_bytes = sorted(set(packed_a[i*2+1] for i in range(96)))
    print(f"    Unique top-half bytes: {len(top_bytes)}")
    print(f"    Unique bottom-half bytes: {len(bot_bytes)}")

# =====================================================================
# 6. Approach C: Whole-glyph dictionary
# =====================================================================
# Each glyph = 16-bit value. Count unique glyphs.
glyph_words = []
for i in range(96):
    w = (packed_a[i*2] << 8) | packed_a[i*2+1]
    glyph_words.append(w)
unique_glyphs = sorted(set(glyph_words))
print(f"\n--- APPROACH C: Whole-glyph dictionary ---")
print(f"    Unique 16-bit glyphs: {len(unique_glyphs)}")
if len(unique_glyphs) <= 256:
    table_size = len(unique_glyphs) * 2  # 2 bytes per glyph in table
    idx_size = 96  # 1 byte per char
    total_c = table_size + idx_size
    print(f"    Table: {table_size} bytes + Indices: {idx_size} bytes = {total_c} bytes")

# =====================================================================
# 7. Approach D: Split tables — top nibble-pair + bottom nibble-pair
# =====================================================================
# If top-pairs ≤ 16 AND bottom-pairs ≤ 16, pack 2 indices in 1 byte per char
top_set = sorted(set(packed_a[i*2] for i in range(96)))
bot_set = sorted(set(packed_a[i*2+1] for i in range(96)))
print(f"\n--- APPROACH D: Split top/bottom pair tables ---")
print(f"    Unique top pairs: {len(top_set)} → {[f'0x{v:02X}' for v in top_set]}")
print(f"    Unique bottom pairs: {len(bot_set)} → {[f'0x{v:02X}' for v in bot_set]}")

if len(top_set) <= 16 and len(bot_set) <= 16:
    top_map = {v: i for i, v in enumerate(top_set)}
    bot_map = {v: i for i, v in enumerate(bot_set)}
    indices = []
    for i in range(96):
        t = top_map[packed_a[i*2]]
        b = bot_map[packed_a[i*2+1]]
        indices.append((t << 4) | b)
    total_d = len(top_set) + len(bot_set) + 96
    print(f"    Top table: {len(top_set)}B + Bottom table: {len(bot_set)}B + Indices: 96B = {total_d} bytes")
    print(f"    Decoder: ~30 bytes Z80")
    print(f"    TOTAL: ~{total_d + 30} bytes")

    # Show the tables
    print(f"\n    Top table ({len(top_set)} entries):")
    for i, v in enumerate(top_set):
        n0 = (v >> 4) & 0xF
        n1 = v & 0xF
        print(f"      [{i:X}] = 0x{v:02X}  → scanline0={n0:04b} scanline1={n1:04b}")
    print(f"\n    Bottom table ({len(bot_set)} entries):")
    for i, v in enumerate(bot_set):
        n0 = (v >> 4) & 0xF
        n1 = v & 0xF
        print(f"      [{i:X}] = 0x{v:02X}  → scanline2={n0:04b} scanline3={n1:04b}")

# =====================================================================
# 8. Approach E: Reuse our font's LUT scheme (3 bytes/glyph with LUT)
# =====================================================================
# Our font uses 10-entry LUT mapping nibble indices to byte patterns.
# Ikkle-4 has 11 unique patterns (including 0x9).
# If we replace placeholder chars (which use 0x9), we get exactly 10.
our_lut = [0x00,0x22,0x44,0x55,0x66,0x88,0xAA,0xCC,0xEE,0xFF]
our_nib_map = {}
for i, v in enumerate(our_lut):
    our_nib_map[(v >> 4) & 0xF] = i

print(f"\n--- APPROACH E: Reuse our font LUT (10 entries) ---")
# Check which chars use nibble 0x9 (not in our LUT)
chars_with_9 = []
for i in range(96):
    ch = 32 + i
    for s in range(4):
        n = (ikkle_font[i*5+s] >> 4) & 0xF
        if n == 9:
            chars_with_9.append(ch)
            break
chars_with_9 = sorted(set(chars_with_9))
print(f"    Chars using nibble 0x9: {[f'{c}({chr(c)})' for c in chars_with_9]}")
print(f"    These are placeholder chars — can be replaced with 0xEE pattern")
# Also check 0xF
chars_with_f = []
for i in range(96):
    ch = 32 + i
    for s in range(4):
        n = (ikkle_font[i*5+s] >> 4) & 0xF
        if n == 0xF:
            chars_with_f.append(ch)
            break
chars_with_f = sorted(set(chars_with_f))
print(f"    Chars using nibble 0xF: {[f'{c}({chr(c)})' for c in chars_with_f]}")

# =====================================================================
# SUMMARY
# =====================================================================
print(f"\n{'='*60}")
print("COMPRESSION SUMMARY")
print(f"{'='*60}")
print(f"  Original:            480 bytes")
print(f"  Drop gap byte:       384 bytes (96 saved)")
print(f"  A) Nibble pack:      192 bytes + ~15B decoder = ~207B")
if len(top_set) <= 16 and len(bot_set) <= 16:
    print(f"  D) Split pair LUT:   {total_d} bytes + ~30B decoder = ~{total_d+30}B")
print(f"  C) Glyph dictionary: {len(unique_glyphs)*2 + 96} bytes + ~20B decoder = ~{len(unique_glyphs)*2+96+20}B")

# =====================================================================
# Generate best approach data for C/ASM inclusion
# =====================================================================
print(f"\n{'='*60}")
print("GENERATED DATA — Approach A (nibble packed, 192 bytes)")
print(f"{'='*60}")
print("static const uint8_t ikkle_packed[192] = {")
line = "    "
for i, b in enumerate(packed_a):
    line += f"0x{b:02X},"
    if (i + 1) % 16 == 0:
        print(line)
        line = "    "
if line.strip():
    print(line)
print("};")
print("// Decode: scanline_byte = nibble * 0x11")
print("// Access: ikkle_packed[(ch-32)*2 + (scanline>>1)]")
print("//         nibble = (scanline & 1) ? (byte & 0xF) : (byte >> 4)")

if len(top_set) <= 16 and len(bot_set) <= 16:
    print(f"\n{'='*60}")
    print(f"GENERATED DATA — Approach D (split LUT, {total_d} bytes)")
    print(f"{'='*60}")
    print(f"static const uint8_t ikkle_top[{len(top_set)}] = {{", end="")
    print(",".join(f"0x{v:02X}" for v in top_set), end="")
    print("};")
    print(f"static const uint8_t ikkle_bot[{len(bot_set)}] = {{", end="")
    print(",".join(f"0x{v:02X}" for v in bot_set), end="")
    print("};")
    print(f"static const uint8_t ikkle_idx[96] = {{")
    line = "    "
    for i, b in enumerate(indices):
        line += f"0x{b:02X},"
        if (i + 1) % 16 == 0:
            print(line)
            line = "    "
    if line.strip():
        print(line)
    print("};")
    print("// Decode: top_pair = ikkle_top[idx >> 4]")
    print("//         bot_pair = ikkle_bot[idx & 0xF]")
    print("//         Then expand each pair's nibbles via *0x11")
