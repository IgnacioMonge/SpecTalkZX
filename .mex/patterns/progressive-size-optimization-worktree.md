# Progressive Size Optimization Worktree

Use for `codex/size-opt-progressive` at `C:\tmp\SpecTalkZX-size-opt-progressive`.

## Baseline

- Base commit: `7608b3e Apply audit fixes and shrink overlay headroom`
- Build command: `make NO_COLOR=1`
- Baseline build: TAP `35846B`
- BSS guard: `0xF3A8 < 0xF500` (`344B` free)
- Overlays: `1784/1822/1860/1949/1958/1733`
- Packed overlay: `SPECTALK.OVL=12288B`
- HW status: pending until Spectrum access returns.

## Work Rules

- Keep `AGENTS.md` in the worktree root as the automatic startup anchor for
  future sessions. It must point agents to `.mex/ROUTER.md`, the final
  candidate branch, expected build sizes, audit status, and the decremental HW
  validation strategy.
- Do not touch `main`; all experiments stay in this worktree and branch.
- Keep one logical optimization per commit.
- Before each candidate, start from a clean `git status --short`.
- After each candidate, run `git diff --check` and `make NO_COLOR=1`.
- Record TAP size, BSS guard/free bytes, overlay sizes, and whether behavior is HW pending.
- If a candidate fails build or grows unexpectedly, revert it before moving on or commit it only as explicit rejected evidence.
- Prefer `SAFE` changes first; mark `AGGRESSIVE` or `EXPERIMENTAL` in notes and commits.
- Avoid user-visible behavior changes unless explicitly approved.

## Local Build Notes

- `src/SPECTALK.DAT` is ignored by Git but required by BPE. A fresh worktree needs a local `1517B` copy from the main checkout.
- If sandboxed `make NO_COLOR=1` fails with BPE restore or `mv` permission errors, rerun the same build outside the sandbox; do not diagnose that as a source regression.
- The revert path for accepted work is `git revert <commit>`; keep commits small enough that this remains practical.
