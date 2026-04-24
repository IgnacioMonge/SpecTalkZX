# External Review Context Package

When preparing a share package for an external or separate-model technical review, keep it narrow and reproducible.

## Rule

- Include the prompt/instructions as a tracked file in the package so the review starts from the same framing every time.
- Preserve relative paths for source files; reviewers need to cross-reference ASM/C contracts and `.mex` guidance without guessing provenance.
- Prefer the active resident implementation and current map/build metadata over historical `Development/` snapshots unless the task explicitly asks for regression archaeology.
- Include the specific `.mex/patterns/` files that constrain the reviewed subsystem, not the whole pattern directory by default.
- Keep generated package directories and zips as local artifacts unless the user explicitly asks to commit or publish them.

## Applies To

- `README_DEMOSCENE_REVIEW_PROMPT.md`
- `demoscene-review-package-*.zip`
