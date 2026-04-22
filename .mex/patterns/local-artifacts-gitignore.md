# Local Artifacts Gitignore

If a file or directory is local-only, generated, or just workflow scaffolding, hide it with `.gitignore` instead of leaving it as recurring untracked noise.

## Rule
- Ignore build outputs such as `build/`, top-level binaries/maps, generated DAT files, and cache directories.
- Ignore local tool folders and notes that are not part of the shipped program, such as `.claude/`, `Development/`, or local assistant markdown files.
- If an asset is obsolete and not needed anymore, remove it from the workspace and keep it ignored so it does not come back into status noise.
- Do not ignore real source files or assets that are part of the release just to get a clean `git status`.

## Applied In
- `.gitignore`
