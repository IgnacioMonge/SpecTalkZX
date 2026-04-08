#!/usr/bin/env python3
"""
ikkle_squeeze.py — Deep compression analysis for Ikkle-4 font.
Explores every trick to get below the 192-byte nibble-packed baseline.
"""

from collections import Counter
import itertools

# Raw Ikkle-4 font (480 bytes)
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

# Our font LUT + packed data (from SPECTALK.DAT format)
our_font_data = [
    0x00,0x22,0x44,0x55,0x66,0x88,0xAA,0xCC,0xEE,0xFF,0x00,0x00,0x00,0x55,0x55,0x05,
    0x06,0x60,0x00,0x06,0x86,0x86,0x28,0x58,0x18,0x61,0x47,0x56,0x26,0x26,0x74,0x25,
    0x00,0x00,0x25,0x55,0x52,0x52,0x22,0x25,0x06,0x28,0x26,0x02,0x28,0x22,0x00,0x02,
    0x25,0x00,0x08,0x00,0x00,0x00,0x77,0x11,0x22,0x55,0x26,0x68,0x62,0x27,0x22,0x28,
    0x26,0x12,0x58,0x81,0x21,0x62,0x56,0x68,0x11,0x85,0x71,0x17,0x45,0x76,0x62,0x81,
    0x12,0x22,0x26,0x26,0x62,0x26,0x64,0x17,0x00,0x50,0x05,0x02,0x02,0x25,0x01,0x25,
    0x21,0x00,0x80,0x80,0x05,0x21,0x25,0x26,0x12,0x02,0x26,0x88,0x52,0x26,0x68,0x66,
    0x76,0x76,0x67,0x45,0x55,0x54,0x76,0x66,0x67,0x85,0x75,0x58,0x85,0x75,0x55,0x26,
    0x58,0x62,0x66,0x86,0x66,0x82,0x22,0x28,0x41,0x11,0x62,0x66,0x87,0x66,0x55,0x55,
    0x58,0x68,0x88,0x66,0x86,0x66,0x66,0x26,0x66,0x62,0x76,0x67,0x55,0x26,0x66,0x74,
    0x76,0x67,0x66,0x45,0x21,0x62,0x82,0x22,0x22,0x66,0x66,0x68,0x66,0x66,0x62,0x66,
    0x88,0x82,0x66,0x26,0x66,0x66,0x62,0x22,0x81,0x22,0x58,0x75,0x55,0x57,0x55,0x22,
    0x11,0x72,0x22,0x27,0x26,0x00,0x00,0x00,0x00,0x09,0x55,0x20,0x00,0x07,0x14,0x64,
    0x55,0x76,0x67,0x04,0x55,0x54,0x11,0x46,0x64,0x02,0x68,0x54,0x26,0x57,0x55,0x46,
    0x64,0x17,0x55,0x76,0x66,0x20,0x72,0x28,0x10,0x41,0x62,0x55,0x67,0x66,0x55,0x55,
    0x62,0x06,0x88,0x66,0x07,0x66,0x66,0x02,0x66,0x62,0x07,0x67,0x55,0x04,0x64,0x11,
    0x04,0x55,0x55,0x04,0x52,0x17,0x28,0x22,0x21,0x06,0x66,0x68,0x06,0x66,0x62,0x06,
    0x88,0x82,0x06,0x26,0x66,0x06,0x64,0x17,0x08,0x12,0x58,0x42,0x52,0x24,0x22,0x22,
    0x22,0x72,0x12,0x27,0x36,0x00,0x00,0x99,0x96,0x06,
]
our_lut = our_font_data[:10]
our_packed = our_font_data[10:298]

# =====================================================================
# Extract nibble-packed representations
# =====================================================================
def get_ikkle_nibs(ch):
    """Return 4 nibbles (scanlines 0-3) for Ikkle-4 char."""
    i = ch - 32
    return tuple((ikkle_raw[i*5+s] >> 4) & 0xF for s in range(4))

def get_our_nibs(ch):
    """Return 6 nibbles (scanlines 0-5) for our font char."""
    i = ch - 32
    nibs = []
    for s in range(6):
        byte = our_packed[i*3 + s//2]
        nib_idx = (byte >> 4) if (s % 2 == 0) else (byte & 0xF)
        nibs.append((our_lut[nib_idx] >> 4) & 0xF)
    return tuple(nibs)

# Build packed half-glyph values
packed_tops = []  # (scanline0 << 4) | scanline1
packed_bots = []  # (scanline2 << 4) | scanline3
ikkle_glyphs = {} # ch -> (top, bot)
for ch in range(32, 128):
    nibs = get_ikkle_nibs(ch)
    top = (nibs[0] << 4) | nibs[1]
    bot = (nibs[2] << 4) | nibs[3]
    packed_tops.append(top)
    packed_bots.append(bot)
    ikkle_glyphs[ch] = (top, bot)

# =====================================================================
# Helpers
# =====================================================================
def nib_to_art(n):
    return ''.join('#' if (n >> (3-i)) & 1 else '.' for i in range(4))

def show_glyph(ch, nibs):
    """ASCII art for a 4-scanline glyph."""
    lines = [nib_to_art(n) for n in nibs]
    return f"'{chr(ch)}' " + " ".join(lines)

def half_hamming(a, b):
    """Pixel-level Hamming distance between two packed half-bytes."""
    d = 0
    for shift in [4, 0]:
        na = (a >> shift) & 0xF
        nb = (b >> shift) & 0xF
        d += bin(na ^ nb).count('1')
    return d

# =====================================================================
# APPROACH F: Derive from our font (transform + corrections)
# =====================================================================
print("=" * 65)
print("APPROACH F: Derive Ikkle-4 from our font via transformation")
print("=" * 65)

# Build all possible 4-scanline outputs from our 6-scanline font
# Operations: SELECT(i), OR(i,j), AND(i,j)
def build_ops(our_6):
    """All possible output scanlines from 6 input scanlines."""
    ops = list(our_6)  # 6 SELECT
    for i in range(6):
        for j in range(i+1, 6):
            ops.append(our_6[i] | our_6[j])    # 15 OR
    for i in range(6):
        for j in range(i+1, 6):
            ops.append(our_6[i] & our_6[j])    # 15 AND
    return ops  # 36 total

# For each combo of 4 ops, count how many chars match Ikkle-4 perfectly
best_transform = None
best_match_count = 0
best_total_dist = float('inf')

# Optimization: precompute per-char, per-row best op
# For each char and each target scanline, find all ops that produce exact match
# Then try all combos of (op_for_row0, op_for_row1, op_for_row2, op_for_row3)

# Since rows are independent, we can optimize per-row
print("\nSearching for best transformation (4 independent row selectors)...")
row_ops_dist = [[0]*36 for _ in range(4)]  # [row][op] = total distance across all chars

for ch in range(33, 128):  # skip space
    our_6 = get_our_nibs(ch)
    ikkle_4 = get_ikkle_nibs(ch)
    ops = build_ops(our_6)
    for row in range(4):
        target = ikkle_4[row]
        for oi, op_val in enumerate(ops):
            row_ops_dist[row][oi] += (0 if op_val == target else 1)

# Best combo: minimize total mismatches
best_ops = []
total_mismatches = 0
for row in range(4):
    best_oi = min(range(36), key=lambda oi: row_ops_dist[row][oi])
    best_ops.append(best_oi)
    total_mismatches += row_ops_dist[row][best_oi]

def op_name(oi):
    if oi < 6: return f"S{oi}"
    oi2 = oi - 6
    if oi2 < 15:
        pairs = [(i,j) for i in range(6) for j in range(i+1,6)]
        return f"{pairs[oi2][0]}|{pairs[oi2][1]}"
    oi3 = oi2 - 15
    pairs = [(i,j) for i in range(6) for j in range(i+1,6)]
    return f"{pairs[oi3][0]}&{pairs[oi3][1]}"

print(f"  Best: [{', '.join(op_name(o) for o in best_ops)}]")
print(f"  Row mismatches: {[row_ops_dist[r][best_ops[r]] for r in range(4)]}")

# Count per-char: how many chars are PERFECT, how many need corrections?
perfect = []
need_fix = []
for ch in range(32, 128):
    if ch == 32:
        perfect.append(ch)
        continue
    our_6 = get_our_nibs(ch)
    ikkle_4 = get_ikkle_nibs(ch)
    ops = build_ops(our_6)
    derived = tuple(ops[best_ops[r]] for r in range(4))
    if derived == ikkle_4:
        perfect.append(ch)
    else:
        need_fix.append(ch)

print(f"\n  Perfect matches: {len(perfect)}/96")
print(f"  Need correction: {len(need_fix)}/96")

# Cost: transformation code (~40B) + correction table
corr_size = len(need_fix) * 3  # char_id(1) + data(2) per correction
transform_code = 50  # estimated Z80 code for runtime transform
total_f = transform_code + corr_size
print(f"  Correction table: {len(need_fix)} × 3 = {corr_size}B")
print(f"  Transform code: ~{transform_code}B")
print(f"  TOTAL: ~{total_f}B")

# Show which chars need fixes
if len(need_fix) <= 30:
    print(f"\n  Chars needing correction:")
    for ch in need_fix:
        our_6 = get_our_nibs(ch)
        ikkle_4 = get_ikkle_nibs(ch)
        ops = build_ops(our_6)
        derived = tuple(ops[best_ops[r]] for r in range(4))
        diff_mark = ""
        for r in range(4):
            if derived[r] != ikkle_4[r]:
                diff_mark += f" r{r}:{nib_to_art(derived[r])}→{nib_to_art(ikkle_4[r])}"
        print(f"    '{chr(ch)}' ({ch}){diff_mark}")

# What if we also optimize per-char (pick best ops independently per char)?
per_char_perfect = 0
per_char_fixable = 0
per_char_impossible = []
for ch in range(33, 128):
    our_6 = get_our_nibs(ch)
    ikkle_4 = get_ikkle_nibs(ch)
    ops = build_ops(our_6)
    all_match = True
    for row in range(4):
        if ikkle_4[row] not in ops:
            all_match = False
            break
    if all_match:
        per_char_perfect += 1
    else:
        per_char_impossible.append(ch)

print(f"\n  Per-char optimal (any op per char per row): {per_char_perfect + 1}/96 derivable")
print(f"  Truly impossible (scanline not achievable): {len(per_char_impossible)}")
if per_char_impossible:
    for ch in per_char_impossible:
        our_6 = get_our_nibs(ch)
        ikkle_4 = get_ikkle_nibs(ch)
        ops = build_ops(our_6)
        missing = [r for r in range(4) if ikkle_4[r] not in ops]
        print(f"    '{chr(ch)}' ({ch}) — missing rows: {missing}")

# =====================================================================
# APPROACH J: Split top/bottom LUT with k=16
# =====================================================================
print(f"\n{'=' * 65}")
print("APPROACH J: Split top/bottom LUT (reduce to 16 entries each)")
print("=" * 65)

def greedy_reduce(all_vals, target_k):
    """Reduce to target_k representatives, minimizing total Hamming error."""
    freq = Counter(all_vals)
    unique = sorted(set(all_vals))
    kept = set(unique)

    removals = []
    while len(kept) > target_k:
        best_cost = float('inf')
        best_remove = None
        best_target = None

        for v in sorted(kept):
            others = kept - {v}
            nearest = min(others, key=lambda o: half_hamming(v, o))
            cost = freq[v] * half_hamming(v, nearest)
            if cost < best_cost or (cost == best_cost and freq.get(v, 0) < freq.get(best_remove, 0)):
                best_cost = cost
                best_remove = v
                best_target = nearest

        kept.remove(best_remove)
        removals.append((best_remove, best_target, best_cost, freq[best_remove]))

    return sorted(kept), removals

top_unique = sorted(set(packed_tops))
bot_unique = sorted(set(packed_bots))

print(f"\n  Top halves: {len(top_unique)} unique → reduce to 16")
top_kept, top_removals = greedy_reduce(packed_tops, 16)
print(f"  Removed {len(top_removals)} top entries:")
for rem, tgt, cost, freq in top_removals:
    n0r, n1r = (rem>>4)&0xF, rem&0xF
    n0t, n1t = (tgt>>4)&0xF, tgt&0xF
    print(f"    0x{rem:02X} ({nib_to_art(n0r)} {nib_to_art(n1r)}) → 0x{tgt:02X} ({nib_to_art(n0t)} {nib_to_art(n1t)})  freq={freq} cost={cost}")

print(f"\n  Bottom halves: {len(bot_unique)} unique → reduce to 16")
bot_kept, bot_removals = greedy_reduce(packed_bots, 16)
print(f"  Removed {len(bot_removals)} bottom entries:")
for rem, tgt, cost, freq in bot_removals:
    n0r, n1r = (rem>>4)&0xF, rem&0xF
    n0t, n1t = (tgt>>4)&0xF, tgt&0xF
    print(f"    0x{rem:02X} ({nib_to_art(n0r)} {nib_to_art(n1r)}) → 0x{tgt:02X} ({nib_to_art(n0t)} {nib_to_art(n1t)})  freq={freq} cost={cost}")

# Build mapping
top_map = {}
for v in sorted(set(packed_tops)):
    if v in top_kept:
        top_map[v] = v
    else:
        top_map[v] = min(top_kept, key=lambda k: half_hamming(v, k))

bot_map = {}
for v in sorted(set(packed_bots)):
    if v in bot_kept:
        bot_map[v] = v
    else:
        bot_map[v] = min(bot_kept, key=lambda k: half_hamming(v, k))

# Encode
top_idx = {v: i for i, v in enumerate(top_kept)}
bot_idx = {v: i for i, v in enumerate(bot_kept)}

indices = []
affected_chars = []
total_pixel_err = 0
for ch in range(32, 128):
    top_orig = packed_tops[ch-32]
    bot_orig = packed_bots[ch-32]
    top_enc = top_map[top_orig]
    bot_enc = bot_map[bot_orig]
    indices.append((top_idx[top_enc] << 4) | bot_idx[bot_enc])
    err = half_hamming(top_orig, top_enc) + half_hamming(bot_orig, bot_enc)
    total_pixel_err += err
    if err > 0:
        affected_chars.append((ch, err, top_orig, top_enc, bot_orig, bot_enc))

data_size = 16 + 16 + 96  # top_lut + bot_lut + indices
decoder_size = 35
print(f"\n  Data: {data_size}B (16 top + 16 bot + 96 idx)")
print(f"  Decoder: ~{decoder_size}B")
print(f"  TOTAL: ~{data_size + decoder_size}B")
print(f"  Total pixel errors: {total_pixel_err} across {len(affected_chars)} chars")

print(f"\n  Affected characters ({len(affected_chars)}):")
for ch, err, to, te, bo, be in affected_chars:
    orig_nibs = get_ikkle_nibs(ch)
    enc_top_nibs = ((te>>4)&0xF, te&0xF)
    enc_bot_nibs = ((be>>4)&0xF, be&0xF)
    enc_nibs = enc_top_nibs + enc_bot_nibs
    orig_art = " ".join(nib_to_art(n) for n in orig_nibs)
    enc_art = " ".join(nib_to_art(n) for n in enc_nibs)
    print(f"    '{chr(ch)}' err={err}  orig: {orig_art}  →  enc: {enc_art}")

# =====================================================================
# APPROACH K: Lossy font tweaks to enable 16-entry LUT
# =====================================================================
print(f"\n{'=' * 65}")
print("APPROACH K: Placeholder chars replaced + re-analyze")
print("=" * 65)

# Replace placeholder chars (using 0x9/0xF nibbles) with simple patterns
PLACEHOLDER_CHARS = {35, 36, 37, 38, 64, 127}  # #$%&@, DEL
# Replace with a simple box: 0xEE, 0xAA, 0xAA, 0xEE (like '0' but smaller)
modified_tops = list(packed_tops)
modified_bots = list(packed_bots)
for ch in PLACEHOLDER_CHARS:
    i = ch - 32
    modified_tops[i] = 0xEA  # E, A
    modified_bots[i] = 0xAE  # A, E

mod_top_unique = sorted(set(modified_tops))
mod_bot_unique = sorted(set(modified_bots))
print(f"  After replacing {len(PLACEHOLDER_CHARS)} placeholders:")
print(f"  Top unique: {len(mod_top_unique)} (was {len(top_unique)})")
print(f"  Bot unique: {len(mod_bot_unique)} (was {len(bot_unique)})")

# Re-run split-LUT analysis with modified data
if len(mod_top_unique) <= 16 and len(mod_bot_unique) <= 16:
    print(f"\n  *** BOTH FIT IN 16! No lossy merging needed! ***")
    mod_data = 16 + 16 + 96
    print(f"  Data: {mod_data}B")
else:
    print(f"\n  Still need reduction. Running greedy merge...")
    mod_top_kept, _ = greedy_reduce(modified_tops, 16)
    mod_bot_kept, _ = greedy_reduce(modified_bots, 16)

    mod_top_map = {}
    for v in sorted(set(modified_tops)):
        if v in mod_top_kept:
            mod_top_map[v] = v
        else:
            mod_top_map[v] = min(mod_top_kept, key=lambda k: half_hamming(v, k))

    mod_bot_map = {}
    for v in sorted(set(modified_bots)):
        if v in mod_bot_kept:
            mod_bot_map[v] = v
        else:
            mod_bot_map[v] = min(mod_bot_kept, key=lambda k: half_hamming(v, k))

    mod_affected = 0
    mod_err = 0
    for ch in range(32, 128):
        i = ch - 32
        te = mod_top_map[modified_tops[i]]
        be = mod_bot_map[modified_bots[i]]
        err = half_hamming(modified_tops[i], te) + half_hamming(modified_bots[i], be)
        if err > 0:
            mod_affected += 1
        mod_err += err

    print(f"  After merge: {mod_affected} chars affected, {mod_err} pixel errors")
    print(f"  Data: 128B + ~35B decoder = ~163B")

# =====================================================================
# APPROACH N: XOR delta encoding
# =====================================================================
print(f"\n{'=' * 65}")
print("APPROACH N: XOR delta encoding (sequential)")
print("=" * 65)

nibble_packed = []
for ch in range(32, 128):
    nibs = get_ikkle_nibs(ch)
    nibble_packed.append((nibs[0] << 4) | nibs[1])
    nibble_packed.append((nibs[2] << 4) | nibs[3])

# XOR delta
xor_bytes = [nibble_packed[0]]
for i in range(1, len(nibble_packed)):
    xor_bytes.append(nibble_packed[i] ^ nibble_packed[i-1])

zero_count = sum(1 for b in xor_bytes if b == 0)
unique_xor = len(set(xor_bytes))
print(f"  XOR delta: {zero_count}/{len(xor_bytes)} zero bytes ({zero_count*100//len(xor_bytes)}%)")
print(f"  Unique XOR values: {unique_xor}")
print(f"  (Zeros indicate repeated bytes — compressible with RLE)")

# Simple RLE on XOR: count runs of zeros
rle_size = 0
i = 0
while i < len(xor_bytes):
    if xor_bytes[i] == 0:
        run = 0
        while i < len(xor_bytes) and xor_bytes[i] == 0 and run < 127:
            run += 1
            i += 1
        rle_size += 2  # escape + count
    else:
        rle_size += 1
        i += 1
print(f"  XOR+RLE size: {rle_size}B (+ ~20B decoder)")

# =====================================================================
# SUMMARY
# =====================================================================
print(f"\n{'=' * 65}")
print("COMPRESSION SUMMARY")
print(f"{'=' * 65}")
print(f"  {'Approach':<40} {'Data':>6} {'Code':>6} {'Total':>6} {'Lossy':>6}")
print(f"  {'-'*40} {'-'*6} {'-'*6} {'-'*6} {'-'*6}")
print(f"  {'Baseline (nibble pack)':<40} {'192':>6} {'~15':>6} {'~207':>6} {'No':>6}")
print(f"  {'F: Transform + corrections':<40} {corr_size:>6} {'~50':>6} {f'~{total_f}':>6} {'No':>6}")
print(f"  {'J: Split LUT k=16':<40} {'128':>6} {'~35':>6} {'~163':>6} {'Yes':>6}")
print(f"  {'N: XOR + RLE':<40} {f'{rle_size}':>6} {'~20':>6} {f'~{rle_size+20}':>6} {'No':>6}")
print(f"  {'K: Placeholder fix + split LUT':<40} {'128':>6} {'~35':>6} {'~163':>6} {'Min':>6}")
