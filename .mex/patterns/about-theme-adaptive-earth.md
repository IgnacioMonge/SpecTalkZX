# About Theme Adaptive Earth

The `!about` Earth overlay should not assume theme 1's black paper and multicolor palette.

## Rule
- Clear about content rows with `ATTR_MAIN_BG`, not a literal black/white attribute.
- Separator and logo attributes should come from the active theme, with only theme 1 allowed to keep the deliberately white/multicolor identity.
- Earth attribute deltas encode ink/bright only. When rendering, combine the generated ink/bright with the active theme's PAPER bits so blank pixels show the current theme background.
- Theme 2 is terminal/green: map Earth ink to green and preserve the generated BRIGHT bit for shading.
- Keep bitmap deltas unchanged; adapting the PAPER bits is what makes the globe background transparent to the theme.

## Applied In
- `overlay/spectalk_ovl2.c`
- `overlay/earth_about_render.asm`
