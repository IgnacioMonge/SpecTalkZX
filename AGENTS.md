# SpecTalkZX Aggressive Size-Opt Worktree Instructions

This worktree is the isolated aggressive size-optimization experiment tree.

## Startup Required

- This file is the automatic bootstrap for sessions started in this directory.
- Do not wait for the user to say "ponte al dia"; immediately read `.mex/ROUTER.md` and recover the current aggressive size-opt state before answering or editing.
- Read `.mex/ROUTER.md` before doing any work.
- Current branch: `codex/size-opt-aggressive-experiments`.
- Base branch/commit: `codex/size-opt-progressive-helper-attrbase` at `c56880d`.
- Do not touch `main` from this worktree unless the user explicitly asks to promote a hardware-validated result.
- Keep the progressive candidate worktree intact; it is the soak/stable reference.

## Baseline

- Build command: `make NO_COLOR=1`
- Baseline candidate from the progressive worktree:
  - TAP `35156B`
  - BSS guard `0xF0F6 < 0xF500` (`1034B` free)
  - overlays `1711/1774/1803/1879/1893/1651`
  - `SPECTALK.OVL=12288B`
- Progressive candidate status: HW OK after corrected SD copy; soak/promotion decision remains separate.

## Experiment Scope

- More aggressive shrink is allowed here, but must stay isolated from `main` and from `size-opt-progressive`.
- Acceptable experiment classes:
  - cold resident code moved to overlays
  - deeper ASM rewrites
  - self-modifying or shared esxDOS I/O experiments
  - data/BSS tradeoff probes
  - feature-budget probes when explicitly approved
- Do not silently change user-visible behavior. Mark behavior/UX cuts as opt-in.

## Working Rules

- Keep one logical change per commit.
- Before any candidate fix or experiment: confirm `git status --short --branch`.
- After any candidate experiment: run `git diff --check` and `make NO_COLOR=1`.
- Measure TAP/resident bytes separately from overlay relief.
- Record every experiment, build result, HW result, accepted fix, and rejected hypothesis in `.mex/ROUTER.md`.
- If an aggressive experiment passes build but needs hardware, mark it **HW PENDING** until tested.
- Do not re-open rejected shrink proposals unless new evidence changes the byte or behavior economics.
