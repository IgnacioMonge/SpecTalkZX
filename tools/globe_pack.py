#!/usr/bin/env python3
"""
globe_pack.py — Pack globe.asm data for SpecTalkZX overlay

Output format (embedded in overlay or standalone):
  [0..N-1]        world_map bit-packed (only USED rows, 8 bytes/row)
  [N..N+10]       map_row_lut (11 bytes: world_map data index for screen rows 7-17)
  [N+11..end]     cell_table packed (M x 2B + 1B terminator)

Cell entry (2 bytes):
  byte 0: (screen_row - 7) << 4 | (screen_col - COL_BASE)
  byte 1: lon_offset with shadow bit 7 preserved

Transformations applied:
  - Remove center column (col 16) for aspect ratio correction
  - Close gap: right-half columns shift left by 1
  - Right-align globe to screen edge (rightmost col = 31)
"""

import re, sys, os

# Right-align: after removing col 16 and closing gap, cols are 11-20 (10 wide)
# Shift to right edge: +11 → cols 22-31
COL_BASE = 22  # first col of right-aligned globe
CENTER_COL = 16  # column to remove


def parse_globe_asm(path):
    with open(path, 'r') as f:
        lines = f.readlines()

    # --- Parse world_map ---
    world_map = []
    in_map = False
    for line in lines:
        if 'world_map:' in line:
            in_map = True
            continue
        if in_map:
            stripped = line.strip()
            if not stripped or stripped.startswith(';'):
                continue
            m = re.search(r'db\s+([\d,\s]+)', line)
            if m:
                row = [int(x.strip()) for x in m.group(1).split(',')]
                if len(row) == 64:
                    world_map.append(row)
            else:
                break

    assert len(world_map) == 32, f"Expected 32 map rows, got {len(world_map)}"

    # --- Parse cell_table (new format with shadow bit) ---
    cells = []
    in_cells = False
    i = 0
    while i < len(lines):
        line = lines[i]
        if 'cell_table:' in line:
            in_cells = True
            i += 1
            continue
        if in_cells:
            if '$0000' in line and 'dw' in line:
                break
            # New format: dw attr_addr, map_base  ; scr(R,C) lon=X D/N
            m_dw = re.search(r'dw\s+\$([0-9A-Fa-f]+),\s*world_map\+(\d+)', line)
            if m_dw:
                attr_addr = int(m_dw.group(1), 16)
                map_offset = int(m_dw.group(2))
                # Extract row,col from attr_addr
                offset_from_5800 = attr_addr - 0x5800
                scr_row = offset_from_5800 >> 5  # // 32
                scr_col = offset_from_5800 & 0x1F  # % 32
                map_row = map_offset >> 6  # // 64
                # Next line: db lon_with_shadow
                i += 1
                m_db = re.search(r'db\s+\$([0-9A-Fa-f]+)', lines[i])
                if not m_db:
                    m_db = re.search(r'db\s+(\d+)', lines[i])
                    if m_db:
                        lon_shadow = int(m_db.group(1))
                    else:
                        i += 1; continue
                else:
                    lon_shadow = int(m_db.group(1), 16)
                cells.append((scr_row, scr_col, map_row, lon_shadow))
        i += 1

    print(f"  Parsed: {len(cells)} cells")
    return world_map, cells


def transform_cells(cells):
    """Remove center column, close gap, right-align."""
    out = []
    removed = 0
    for sr, sc, mr, lon_sh in cells:
        if sc == CENTER_COL:
            removed += 1
            continue
        # Close gap: right half shifts left by 1
        if sc > CENTER_COL:
            sc -= 1
        # Right-align: shift so rightmost = 31
        # After gap close: cols are 11-20 (10 wide)
        # Shift = 31 - 20 = 11
        new_col = sc + 11
        out.append((sr, new_col, mr, lon_sh))
    print(f"  Removed {removed} cells at center col {CENTER_COL}, {len(out)} remaining")
    return out


def pack_world_map_used(world_map, cells):
    """Bit-pack only the world_map rows actually referenced by cells."""
    used_rows = sorted(set(mr for _, _, mr, _ in cells))
    # Build index: map_row → data_index
    row_to_idx = {mr: i for i, mr in enumerate(used_rows)}

    packed = bytearray()
    for mr in used_rows:
        row = world_map[mr]
        for byte_idx in range(8):
            val = 0
            for bit in range(8):
                col = byte_idx * 8 + bit
                if row[col]:
                    val |= (0x80 >> bit)
            packed.append(val)

    return packed, used_rows, row_to_idx


def build_lut(cells, row_to_idx):
    """Map screen row (7-17) → index into packed world_map data."""
    lut = {}
    for sr, _, mr, _ in cells:
        idx = row_to_idx[mr]
        if sr in lut:
            assert lut[sr] == idx
        else:
            lut[sr] = idx

    result = bytearray()
    for sr in range(7, 18):
        result.append(lut.get(sr, 0))
    return result


def pack_cells(cells):
    """Pack cells to 2 bytes each + terminator."""
    packed = bytearray()
    for sr, sc, _, lon_sh in cells:
        byte0 = ((sr - 7) << 4) | (sc - COL_BASE)
        byte1 = lon_sh  # shadow bit 7 preserved
        packed.append(byte0)
        packed.append(byte1)
    packed.append(0)  # terminator
    return packed


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.dirname(script_dir)

    globe_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(script_dir, 'globe.asm')
    out_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(project_dir, 'build', 'GLOBE.DAT')

    print(f"  Packing {globe_path}")
    world_map, cells = parse_globe_asm(globe_path)

    # Transform: remove center col, right-align
    cells = transform_cells(cells)

    min_row = min(c[0] for c in cells)
    max_row = max(c[0] for c in cells)
    min_col = min(c[1] for c in cells)
    max_col = max(c[1] for c in cells)
    print(f"  Screen rows {min_row}-{max_row}, cols {min_col}-{max_col} ({max_col-min_col+1} wide)")

    # Pack
    map_packed, used_rows, row_to_idx = pack_world_map_used(world_map, cells)
    lut = build_lut(cells, row_to_idx)
    cell_packed = pack_cells(cells)

    dat = map_packed + lut + cell_packed
    map_sz = len(map_packed)
    lut_sz = len(lut)
    cell_sz = len(cell_packed)
    print(f"  Data: {len(dat)} bytes (map:{map_sz} lut:{lut_sz} cells:{cell_sz})")
    print(f"  Used map rows: {len(used_rows)} ({used_rows})")

    # Output as C array header for embedding in overlay
    h_path = os.path.join(project_dir, 'overlay', 'globe_data.h')
    with open(h_path, 'w') as f:
        f.write("/* AUTO-GENERATED by globe_pack.py — DO NOT EDIT */\n")
        f.write(f"/* {len(cells)} cells, {len(used_rows)} map rows, shadow terminator */\n\n")
        f.write(f"#define GLOBE_DATA_SIZE {len(dat)}\n")
        f.write(f"#define GLOBE_MAP_OFF   0\n")
        f.write(f"#define GLOBE_MAP_SIZE  {map_sz}\n")
        f.write(f"#define GLOBE_LUT_OFF   {map_sz}\n")
        f.write(f"#define GLOBE_CELL_OFF  {map_sz + lut_sz}\n")
        f.write(f"#define GLOBE_COL_BASE  {COL_BASE}\n")
        f.write(f"#define GLOBE_NCELLS    {len(cells)}\n\n")
        f.write("static const uint8_t globe_data[] = {\n")
        for i in range(0, len(dat), 16):
            chunk = dat[i:i+16]
            f.write("    " + ",".join(f"0x{b:02X}" for b in chunk) + ",\n")
        f.write("};\n")
    print(f"  Written: {h_path}")

    # Also write binary (for standalone testing)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, 'wb') as f:
        f.write(dat)
    print(f"  Written: {out_path}")


if __name__ == '__main__':
    main()
