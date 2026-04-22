# Build Target Preprocess Deps

If a direct build target depends on generated or preprocessed sources, make that target own the preprocessing step instead of assuming it ran through some larger pipeline first.

## Rule
- Track include/module files explicitly in the target dependency set when the compiler only sees a root source file.
- Put required preprocessing under the direct build target so `make build` works the same way whether it is called alone or through `make`.
- Keep bypass behavior explicit with a separate target such as `nobpe`; do not make the default build silently depend on prior side effects.
- Reflect the behavior in `help` text so the target contract is obvious.

## Applied In
- `Makefile`
