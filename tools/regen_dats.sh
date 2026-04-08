#!/bin/bash
# regen_dats.sh — Regenerate SPECTALK.DAT (with BPE dict) for each dev snapshot
# Only runs the BPE Python pipeline, no z88dk compilation needed.
# The existing .tap files are correct; they just need matching DATs.

set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

BACKUP="$ROOT/build/_regen_backup"
SNAPSHOTS=(
    "Development/22_overlay_nickcolor"
    "Development/23_gemini_g2g3"
    "Development/24_all_optimized"
    "Development/25_attr_computed"
    "Development/26_tables_computed"
    "Development/27_st_copy_n_copt"
    "Development/28_cdecl_fusion"
    "Development/29_fill_merge_demoscene"
)

# --- Backup current files ---
mkdir -p "$BACKUP"
cp src/spectalk.c src/irc_handlers.c src/user_cmds.c "$BACKUP/"
cp include/spectalk.h "$BACKUP/"
cp src/SPECTALK.DAT "$BACKUP/"

restore() {
    cp "$BACKUP/spectalk.c" src/
    cp "$BACKUP/irc_handlers.c" src/
    cp "$BACKUP/user_cmds.c" src/
    cp "$BACKUP/spectalk.h" include/
    cp "$BACKUP/SPECTALK.DAT" src/
    rm -rf build/bpe_originals build/bpe_src build/bpe_final build/bpe_dict.bin build/SPECTALK.DAT
}
trap restore EXIT

for snap in "${SNAPSHOTS[@]}"; do
    name=$(basename "$snap")
    echo "=== $name ==="

    if [ ! -f "$snap/spectalk.c" ]; then
        echo "  SKIP (no source files)"
        continue
    fi

    # Clean BPE state
    rm -rf build/bpe_originals build/bpe_src build/bpe_final build/bpe_dict.bin build/SPECTALK.DAT

    # Install snapshot sources
    cp "$snap/spectalk.c" src/
    cp "$snap/irc_handlers.c" src/
    cp "$snap/user_cmds.c" src/
    cp "$snap/spectalk.h" include/
    cp "$BACKUP/SPECTALK.DAT" src/   # always use clean 1107-byte original

    # Run BPE pipeline (Python only — generates build/SPECTALK.DAT with dict)
    mkdir -p build/bpe_originals
    cp src/spectalk.c src/irc_handlers.c src/user_cmds.c build/bpe_originals/
    cp include/spectalk.h build/bpe_originals/
    cp src/SPECTALK.DAT build/bpe_originals/

    if python tools/bpe_build.py 2>&1; then
        dat_sz=$(wc -c < "build/SPECTALK.DAT")
        old_sz=$(wc -c < "$snap/SPECTALK.DAT")
        cp build/SPECTALK.DAT "$snap/SPECTALK.DAT"
        echo "  OK: DAT $old_sz -> $dat_sz bytes"
    else
        echo "  FAILED"
    fi

    # Restore clean sources for next iteration
    cp "$BACKUP/spectalk.c" src/
    cp "$BACKUP/irc_handlers.c" src/
    cp "$BACKUP/user_cmds.c" src/
    cp "$BACKUP/spectalk.h" include/
    cp "$BACKUP/SPECTALK.DAT" src/
done

echo ""
echo "=== Done ==="
echo "Skipped: 22_pre_bpe_overlay (incompatible bpe_build.py, DAT was 1077 bytes)"
echo "Skipped: 20_pre_nickcolor, 21_pre_overlay (no BPE)"
