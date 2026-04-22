# ASM Root Include Split

When the resident ASM grows too large to edit safely as one file, keep `asm/spectalk_asm.asm` as a thin root that only includes contiguous domain modules in a fixed order.

## Rule
- Split by contiguous logical blocks, not by reordering routines, so relative branches and layout-sensitive code keep the same assembled placement.
- Keep shared constants, globals, section declarations, and fixed-address aliases in the preamble module so the rest of the tree stays focused on code domains.
- Add new routines to the nearest domain module; if a new area becomes large enough, create another ordered include instead of rebuilding the monolith.
- Treat the root file as an index, not as a place for real implementation.

## Applied In
- `asm/spectalk_asm.asm`
- `asm/spectalk_asm/`
