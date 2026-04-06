#!/usr/bin/env python3
"""
ikkle_squeeze2.py — Second round: find every exploitable pattern in Ikkle-4.
Focus on structural relationships WITHIN the font itself.
"""

from collections import Counter

ikkle_raw = [
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

def get_nibs(ch):
    i = ch - 32
    return tuple((ikkle_raw[i*5+s] >> 4) & 0xF for s in range(4))

def nib_art(n):
    return ''.join('#' if (n >> (3-i)) & 1 else '.' for i in range(4))

# Build packed form
packed = []  # 192 bytes
for ch in range(32, 128):
    n = get_nibs(ch)
    packed.append((n[0] << 4) | n[1])
    packed.append((n[2] << 4) | n[3])

# =====================================================================
# 1. DUPLICATE GLYPHS — which chars share identical glyphs?
# =====================================================================
print("=" * 65)
print("1. DUPLICATE GLYPHS")
print("=" * 65)
glyph_map = {}
for ch in range(32, 128):
    g = (packed[(ch-32)*2], packed[(ch-32)*2+1])
    glyph_map.setdefault(g, []).append(ch)

dupes = {g: chars for g, chars in glyph_map.items() if len(chars) > 1}
print(f"  {len(glyph_map)} unique glyphs, {len(dupes)} shared groups:")
for g, chars in sorted(dupes.items()):
    names = [f"'{chr(c)}'({c})" for c in chars]
    print(f"    0x{g[0]:02X}{g[1]:02X} → {', '.join(names)}")

# =====================================================================
# 2. UPPER/LOWER CASE RELATIONSHIPS
# =====================================================================
print(f"\n{'=' * 65}")
print("2. UPPER → lower DERIVATION PATTERNS")
print("=" * 65)

# For each letter, check if lowercase = (0, upper[0], upper[1], upper[2])
# i.e., lowercase is uppercase shifted down by 1 scanline with blank on top
derive_shift = []  # chars where lower = 0 + upper[0:3]
derive_other = []
for letter in range(26):
    uc = 65 + letter
    lc = 97 + letter
    un = get_nibs(uc)
    ln = get_nibs(lc)

    # Pattern 1: lower = (0, upper[0], upper[1], upper[2])
    if ln == (0, un[0], un[1], un[2]):
        derive_shift.append((uc, lc, "shift_down"))
    # Pattern 2: lower = (0, upper[1], upper[2], upper[3])
    elif ln == (0, un[1], un[2], un[3]):
        derive_shift.append((uc, lc, "skip_top"))
    # Pattern 3: lower[1:] == upper[1:]  (same body, different cap)
    elif ln[1:] == un[1:]:
        derive_shift.append((uc, lc, "same_body"))
    # Pattern 4: lower[1:4] == upper[0:3]
    elif ln[1:4] == un[0:3]:
        derive_shift.append((uc, lc, "drop_top_match"))
    else:
        derive_other.append((uc, lc))

print(f"  Derivable: {len(derive_shift)}/26")
for uc, lc, pattern in derive_shift:
    un = get_nibs(uc)
    ln = get_nibs(lc)
    print(f"    {chr(uc)}→{chr(lc)} [{pattern}]  "
          f"UC: {' '.join(nib_art(n) for n in un)}  "
          f"lc: {' '.join(nib_art(n) for n in ln)}")

print(f"\n  NOT derivable: {len(derive_other)}/26")
for uc, lc in derive_other:
    un = get_nibs(uc)
    ln = get_nibs(lc)
    print(f"    {chr(uc)}→{chr(lc)}  "
          f"UC: {' '.join(nib_art(n) for n in un)}  "
          f"lc: {' '.join(nib_art(n) for n in ln)}")

# =====================================================================
# 3. SCANLINE FREQUENCY — which nibbles dominate?
# =====================================================================
print(f"\n{'=' * 65}")
print("3. NIBBLE FREQUENCY + ENTROPY")
print("=" * 65)

all_nibs = []
for ch in range(32, 128):
    all_nibs.extend(get_nibs(ch))

freq = Counter(all_nibs)
total = len(all_nibs)
print(f"  {total} nibbles total, {len(freq)} unique values")

import math
entropy = 0
for nib, count in sorted(freq.items()):
    p = count / total
    e = -p * math.log2(p)
    entropy += e
    print(f"    0x{nib:X} ({nib_art(nib)}): {count:3d} ({p*100:4.1f}%)  info={-math.log2(p):.2f} bits")

print(f"\n  Entropy: {entropy:.3f} bits/nibble")
print(f"  Theoretical min: {math.ceil(total * entropy / 8)} bytes")
print(f"  Current packed: 192 bytes ({total * 4 / 8:.0f} bytes at 4 bits/nib)")

# =====================================================================
# 4. VERTICAL SYMMETRY (top byte == bottom byte)
# =====================================================================
print(f"\n{'=' * 65}")
print("4. VERTICAL SYMMETRY (top_packed == bot_packed)")
print("=" * 65)

sym_chars = []
for ch in range(32, 128):
    top = packed[(ch-32)*2]
    bot = packed[(ch-32)*2+1]
    if top == bot:
        sym_chars.append(ch)

print(f"  {len(sym_chars)} chars with top == bottom:")
for ch in sym_chars:
    n = get_nibs(ch)
    print(f"    '{chr(ch)}' ({ch})  {' '.join(nib_art(x) for x in n)}")

# =====================================================================
# 5. SCANLINE 0 == 0x0 (blank top)
# =====================================================================
print(f"\n{'=' * 65}")
print("5. BLANK FIRST SCANLINE (nibble 0 = 0x0)")
print("=" * 65)

blank_top = [ch for ch in range(32, 128) if get_nibs(ch)[0] == 0]
print(f"  {len(blank_top)} chars start with blank scanline")
non_blank = [ch for ch in range(32, 128) if get_nibs(ch)[0] != 0]
print(f"  {len(non_blank)} chars have non-blank top")

# =====================================================================
# 6. BRACKET/SYMBOL PAIRS
# =====================================================================
print(f"\n{'=' * 65}")
print("6. MIRROR/TWIN SYMBOL PAIRS")
print("=" * 65)

pairs = [
    (40, 41, '(', ')'), (60, 62, '<', '>'), (91, 93, '[', ']'),
    (123, 125, '{', '}'), (47, 92, '/', '\\'),
    (98, 100, 'b', 'd'), (112, 113, 'p', 'q'),
]

def mirror_nib(n):
    """Horizontal mirror of 4-bit pattern: bit3,2,1,0 → bit0,1,2,3"""
    return ((n & 1) << 3) | ((n & 2) << 1) | ((n & 4) >> 1) | ((n & 8) >> 3)

for a, b, na, nb in pairs:
    ga = get_nibs(a)
    gb = get_nibs(b)
    # Check if b == mirror(a)
    ga_mir = tuple(mirror_nib(n) for n in ga)
    is_mirror = (gb == ga_mir)
    # Check if b == vertically flipped a
    ga_vflip = tuple(reversed(ga))
    is_vflip = (gb == ga_vflip)

    print(f"  {na}/{nb}: ", end="")
    print(f"  {na}: {' '.join(nib_art(n) for n in ga)}", end="")
    print(f"  {nb}: {' '.join(nib_art(n) for n in gb)}", end="")
    if is_mirror:
        print(f"  ← H-MIRROR!", end="")
    if is_vflip:
        print(f"  ← V-FLIP!", end="")
    if ga == gb:
        print(f"  ← IDENTICAL!", end="")
    print()

# =====================================================================
# 7. COMPACT APPROACH: blank-top flag + 3-nib encoding
# =====================================================================
print(f"\n{'=' * 65}")
print("7. BLANK-TOP FLAG ENCODING")
print("=" * 65)
print(f"  {len(blank_top)} chars with blank top → store 3 nibs (12 bits = 1.5 bytes)")
print(f"  {len(non_blank)} chars with non-blank → store 4 nibs (16 bits = 2 bytes)")

# Pack: 12-bit values for blank-top chars, 16-bit for non-blank
# Need a 96-bit (12 byte) bitmap to flag which chars are blank-top
# For blank-top: pack 2 chars per 3 bytes (12+12=24 bits)
# For non-blank: 2 bytes per char as before

n_blank = len(blank_top)
n_non_blank = len(non_blank)

# Blank-top: pack in 12-bit units → ceil(n_blank * 12 / 8) bytes
blank_bits = n_blank * 12
blank_bytes = (blank_bits + 7) // 8

# Non-blank: 2 bytes each
non_blank_bytes = n_non_blank * 2

bitmap_bytes = 12  # 96 bits

total_7 = blank_bytes + non_blank_bytes + bitmap_bytes
decoder_7 = 40  # more complex decoder
print(f"  Bitmap: {bitmap_bytes}B + Blank: {blank_bytes}B + Full: {non_blank_bytes}B = {total_7}B")
print(f"  Decoder: ~{decoder_7}B")
print(f"  TOTAL: ~{total_7 + decoder_7}B  (vs baseline 207B)")

# =====================================================================
# 8. RADICAL: Unique nibble-pair dictionary PER POSITION
# =====================================================================
print(f"\n{'=' * 65}")
print("8. PER-POSITION NIBBLE ANALYSIS")
print("=" * 65)

for pos in range(4):
    nibs_at_pos = [get_nibs(ch)[pos] for ch in range(32, 128)]
    f = Counter(nibs_at_pos)
    unique = sorted(f.keys())
    print(f"  Scanline {pos}: {len(unique)} unique → {[f'0x{n:X}({f[n]})' for n in unique]}")

# Can any position be encoded in 3 bits?
for pos in range(4):
    nibs_at_pos = [get_nibs(ch)[pos] for ch in range(32, 128)]
    unique = set(nibs_at_pos)
    if len(unique) <= 8:
        print(f"  → Scanline {pos} fits in 3 bits! ({len(unique)} values)")

# =====================================================================
# 9. SCANLINE-ORGANIZED ENCODING (columnar)
# =====================================================================
print(f"\n{'=' * 65}")
print("9. SCANLINE-ORGANIZED ENCODING (all scan0, all scan1, ...)")
print("=" * 65)

for pos in range(4):
    nibs = [get_nibs(ch)[pos] for ch in range(32, 128)]
    # Pack nibbles: 2 per byte = 48 bytes per scanline
    # Count runs of same value
    runs = []
    cur = nibs[0]
    count = 1
    for n in nibs[1:]:
        if n == cur:
            count += 1
        else:
            runs.append((cur, count))
            cur = n
            count = 1
    runs.append((cur, count))

    # RLE cost: each run = 1 byte (4-bit value + 4-bit length, max 15)
    rle_bytes = 0
    for val, cnt in runs:
        while cnt > 0:
            chunk = min(cnt, 15)
            rle_bytes += 1
            cnt -= chunk

    raw_bytes = 48  # 96 nibbles / 2
    print(f"  Scan {pos}: {len(runs)} runs, raw={raw_bytes}B, RLE={rle_bytes}B (save {raw_bytes-rle_bytes}B)")

print(f"\n  Raw total: 192B (4 × 48B)")
total_rle = 0
for pos in range(4):
    nibs = [get_nibs(ch)[pos] for ch in range(32, 128)]
    runs = []
    cur = nibs[0]
    count = 1
    for n in nibs[1:]:
        if n == cur: count += 1
        else: runs.append((cur, count)); cur = n; count = 1
    runs.append((cur, count))
    rle_b = 0
    for val, cnt in runs:
        while cnt > 0:
            rle_b += 1
            cnt -= min(cnt, 15)
    total_rle += rle_b

print(f"  RLE total: {total_rle}B + 4B offsets + ~35B decoder = ~{total_rle + 4 + 35}B")
print(f"  BUT: requires full decompression to RAM (192B BSS) for random access")

# =====================================================================
# 10. MOST RADICAL: Custom glyph tweaks to maximize compression
# =====================================================================
print(f"\n{'=' * 65}")
print("10. NIBBLE VALUE REDUCTION — what if we get to 8 unique nibbles?")
print("=" * 65)

# Current: 11 unique (0,2,4,5,6,8,9,A,C,E,F)
# Rarest: 0x5 (freq?), 0x9 (freq?), 0xF (freq?)
print(f"  Current unique: {sorted(freq.keys())} = {len(freq)} values")

# If we eliminate 0x5, 0x9, 0xF (rarest), what chars change?
rare_nibs = {0x5, 0x9, 0xF}
remap = {0x5: 0x4, 0x9: 0xA, 0xF: 0xE}  # closest matches
affected = []
for ch in range(32, 128):
    nibs = get_nibs(ch)
    if any(n in rare_nibs for n in nibs):
        new_nibs = tuple(remap.get(n, n) for n in nibs)
        affected.append((ch, nibs, new_nibs))

print(f"  Eliminating {rare_nibs} → remap to closest")
print(f"  Affected: {len(affected)} chars:")
for ch, old, new in affected:
    old_art = ' '.join(nib_art(n) for n in old)
    new_art = ' '.join(nib_art(n) for n in new)
    print(f"    '{chr(ch)}' ({ch})  {old_art}  →  {new_art}")

remaining = sorted(set(range(16)) - rare_nibs)
remaining = sorted(freq.keys() - rare_nibs)
print(f"\n  Remaining: {len(remaining)} unique nibbles = {remaining}")
if len(remaining) <= 8:
    print(f"  *** 8 values → 3 bits per nibble!")
    print(f"  4 scanlines × 3 bits = 12 bits per char")
    print(f"  96 chars × 12 bits = 1152 bits = 144 bytes")
    print(f"  + 8-byte LUT + ~40B decoder = ~192B")
    print(f"  Same total, more complex. Not worth it.")

    # BUT: what about packing 8 nibbles per 3 bytes (8×3=24 bits)?
    # 4 nibs per char at 3 bits = 12 bits. Pack 2 chars per 3 bytes.
    pack_bytes = (96 * 12 + 7) // 8
    print(f"\n  Bit-packed: {pack_bytes}B data + 8B LUT + ~45B decoder = ~{pack_bytes+8+45}B")

# =====================================================================
# 11. COMBINED: uppercase store + lowercase derive
# =====================================================================
print(f"\n{'=' * 65}")
print("11. COMBINED SCHEME: exploit all relationships")
print("=" * 65)

# Count saveable chars:
# - Duplicate glyphs: store once, reference for rest
# - Upper→lower derivable: store upper, compute lower
# - Blank-top chars: store only 3 nibbles

# Total unique glyphs needed
unique_needed = set()
alias_map = {}  # ch → source_ch (for duplicates)

# Find all duplicate groups
for g, chars in glyph_map.items():
    primary = chars[0]
    unique_needed.add(primary)
    for ch in chars[1:]:
        alias_map[ch] = primary

# For derivable lowercase: if uppercase is stored, lowercase is derived
for uc, lc, pattern in derive_shift:
    if lc not in alias_map:  # not already handled as duplicate
        alias_map[lc] = uc  # derive from uppercase
        # Make sure uppercase is in unique_needed
        if uc not in alias_map:
            unique_needed.add(uc)

# Remaining chars that must be stored
stored_chars = sorted(ch for ch in range(32, 128)
                      if ch not in alias_map or ch in unique_needed)

# Split stored chars into blank-top and non-blank
stored_blank = [ch for ch in stored_chars if get_nibs(ch)[0] == 0]
stored_full = [ch for ch in stored_chars if get_nibs(ch)[0] != 0]

print(f"  Duplicate aliases: {len(alias_map) - len(derive_shift)} chars → stored elsewhere")
print(f"  Upper→lower derivable: {len(derive_shift)} pairs")
print(f"  Must store: {len(stored_chars)} glyphs")
print(f"    - Blank-top: {len(stored_blank)} (3 nibs = 1.5B each)")
print(f"    - Full: {len(stored_full)} (4 nibs = 2B each)")

# Data cost
full_bytes = len(stored_full) * 2
blank_bits = len(stored_blank) * 12
blank_bytes = (blank_bits + 7) // 8

# Mapping tables
# For duplicates: need char → source mapping
# For derivation: need char → (source, transform_type)
# For blank-top: need to know which stored chars are blank-top

# Simplest: 96-byte index table where each byte encodes:
# - bit 7: 1 = derived/alias (look up source), 0 = stored directly
# - bits 6-0: index into stored-glyphs table, or source char offset

# Actually let's compute concrete sizes:
# Option A: 96-byte lookup + compressed glyph table
glyph_table_bytes = full_bytes + blank_bytes
alias_count = len([ch for ch in range(32, 128) if ch in alias_map])

print(f"\n  Glyph table: {full_bytes}B (full) + {blank_bytes}B (blank-top) = {glyph_table_bytes}B")
print(f"  Index: 96B (1 byte per char → glyph table offset or derive flag)")
print(f"  Total data: {glyph_table_bytes + 96}B")
print(f"  Decoder: ~50B (handle derive + blank-top + direct)")
print(f"  GRAND TOTAL: ~{glyph_table_bytes + 96 + 50}B")

# =====================================================================
# SUMMARY
# =====================================================================
print(f"\n{'=' * 65}")
print("FINAL SUMMARY — all roads explored")
print(f"{'=' * 65}")
print(f"  Baseline nibble-pack:           192B data + 15B code = 207B")
print(f"  Split LUT k=16 (LOSSY):        128B data + 35B code = 163B  [42 chars err]")
print(f"  Blank-top flag:                 {total_7}B data + {decoder_7}B code = {total_7+decoder_7}B")
print(f"  Scanline RLE (needs 192B BSS): {total_rle}B + 39B code = {total_rle+39}B")
print(f"  Combined scheme:               {glyph_table_bytes+96}B + 50B code = {glyph_table_bytes+96+50}B")
print(f"  Reduce to 8 nibs (LOSSY):      {pack_bytes+8}B + 45B code = {pack_bytes+8+45}B")
print(f"  Entropy floor:                 ~{math.ceil(total * entropy / 8)}B data (theoretical)")
