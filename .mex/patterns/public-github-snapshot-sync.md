# Public GitHub Snapshot Sync

## Rule
The public GitHub clone at `C:\Users\ignac\Dropbox\Retro\Software\Github Repositories\SpecTalkZX` should be updated from the dev checkout as a snapshot, not by merging dev commits, unless the histories have first been intentionally reconciled.

## Why
The dev checkout and public clone can have unrelated or heavily diverged histories. A git merge/cherry-pick is noisy and risky for release prep. The public clone also owns public-facing files absent from dev (`README.md`, `READMEsp.md`, `LICENSE`, `images/`, `.gitattributes`) and those must be preserved.

## Process
- Confirm the public clone is clean before copying.
- Copy code/build inputs from dev: `Makefile`, `asm/`, `include/`, `overlay/`, `src/`, `tools/`, and `release/`.
- Preserve public-only docs/assets, then update `README.md`, `READMEsp.md`, and `CHANGELOG.md` in place.
- Remove tracked files that no longer exist in dev only after confirming they are obsolete.
- Keep `release/about_earth/*.bin` trackable even if `.gitignore` has a global `*.bin` rule.
- Verify in the public clone with `make release NO_COLOR=1`; record release TAP, BSS gap, and overlay sizes.
- Commit locally in the public clone when asked to advance it, but do not push to GitHub unless explicitly asked.
