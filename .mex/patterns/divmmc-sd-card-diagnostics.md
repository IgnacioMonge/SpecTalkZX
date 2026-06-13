# divMMC SD Card Diagnostics

When Windows can list an esxDOS card but divMMC cannot use it, diagnose the disk layout before changing files.

Use these checks first:

- Confirm `/SYS` and `/BIN` exist at the card root, with `ESXDOS.SYS`, `NMI.SYS`, and the `.KO` modules under `/SYS`.
- Check `Get-Volume -DriveLetter <letter>`, `Get-Partition -DriveLetter <letter>`, `Get-Disk`, `fsutil fsinfo volumeinfo`, `fsutil fsinfo sectorinfo`, and read-only `chkdsk`.
- Treat FAT errors, non-512-byte sectors, exFAT/NTFS, GPT, extended partitions, multiple odd partitions, `Offset=0` superfloppy layout, or a partition type/filesystem mismatch as stronger suspects than a normal-looking Windows directory listing.
- If the geometry is still correct but the root shows garbage names or missing `/SYS`, treat `chkdsk` reports such as invalid timestamps, nonzero directory sizes, invalid first allocation units, or lost chains as real FAT/directory corruption. Do not assume this is the old offset/type mismatch problem.
- Keep esxDOS constraints in mind: FAT16/FAT32 only, no extended partitions, 8.3 filenames for firmware-era compatibility, and matching `/SYS`/`/BIN` files for the ROM version.

If the card contents look right but the layout is suspect, rebuild the SD instead of editing individual files:

1. Back up the visible contents.
2. Recreate a single primary MBR FAT32 partition with 512-byte sectors.
3. Use a conservative cluster size, such as 8K or 16K on a 16GB card.
4. Copy fresh, version-matched esxDOS `/SYS` and `/BIN` first, then user files.
5. Retest boot, `.ls`, NMI browser, and any project-specific loaders.
