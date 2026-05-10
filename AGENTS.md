# SpecTalkZX Size-Opt Worktree Instructions

This worktree is the isolated progressive size-optimization validation tree.

## Startup Required

- This file is the automatic bootstrap for sessions started in this directory.
- Do not wait for the user to say "ponte al dia"; immediately read `.mex/ROUTER.md` and recover the current size-opt state before answering or editing.
- Read `.mex/ROUTER.md` before doing any work.
- Current final candidate branch: `codex/size-opt-progressive-helper-attrbase`.
- Do not touch `main` from this worktree unless the user explicitly asks to promote a hardware-validated result.
- Treat all current size-opt changes as build-verified but hardware-pending.

## Current Candidate State

- Build command: `make NO_COLOR=1`
- Expected final build:
  - TAP `35156B`
  - BSS guard `0xF0F6 < 0xF500` (`1034B` free)
  - overlays `1711/1774/1803/1879/1893/1651`
  - `SPECTALK.OVL=12288B`
- Final audit status: own `audit-z80` plus external Claude audit found `0 BUG`, `0 LIKELY`, `0 SUSPICIOUS`.
- Known non-blocking notes:
  - OVL2 `jr nz, _about_close_ovl` is only future build-range fragility; do not change to `jp` unless it actually fails assembly or the user accepts the byte cost.
  - ABOUT/Earth SMC lifetime is valid while OVL2 remains resident during ABOUT mode.

## Hardware Validation Strategy

- Prefer decremental HW validation:
  1. Test the final branch first.
  2. If it passes, this is the candidate to promote.
  3. If it fails, walk backward through the recorded checkpoint branches in `.mex/ROUTER.md`.
  4. Once an OK/failing boundary is found, bisect inside that block using the existing atomic commits.
- Keep fixes in new mini-branches from the failing checkpoint or final candidate.
- Record every HW result, build result, accepted fix, and rejected hypothesis in `.mex/ROUTER.md`.

## Working Rules

- Keep one logical change per commit.
- Before any candidate fix: confirm `git status --short --branch`.
- After any candidate fix: run `git diff --check` and `make NO_COLOR=1`.
- Preserve user-visible behavior unless explicitly approved.
- Do not re-open rejected shrink proposals unless new evidence changes the byte or behavior economics.
