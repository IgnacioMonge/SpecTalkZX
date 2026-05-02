# About Theme Adaptive Earth

The `!about` Earth overlay should not assume theme 1's black paper and multicolor palette.

## Rule
- Clear about content rows with `ATTR_MAIN_BG`, not a literal black/white attribute.
- The row-2 one-pixel separator belongs visually to the screen background under the banner. Use `ATTR_MAIN_BG`, not `ATTR_MSG_TOPIC` or banner paper; topic yellow and theme-3 banner red both make the lower banner edge look corrupted.
- Logo attributes should come from the active theme, with theme 1 allowed to keep the deliberately white/multicolor identity and theme 3 using `ATTR_MAIN_BG` so the large SpectalkZX logo is white on blue.
- Earth attribute deltas encode ink/bright only. When rendering, combine the generated ink/bright with the active theme's PAPER bits so blank pixels show the current theme background.
- Theme 2 is terminal/green: map Earth ink to green and preserve the generated BRIGHT bit for shading.
- Theme 3 keeps the same bitmap geometry as theme 1, but generated blue ink must be remapped to cyan over the theme-3 blue paper; otherwise blue-on-blue erases the right/bottom contour.
- The current bitmap set uses the compatible `GloboTerraqueoZX\build` frames in the same playback order as the matching attribute stream. Bitmap and attribute streams must stay phase-aligned; comparing only layout or frame 0 can miss a full-sequence direction mismatch. Bitmap pixels outside `earth_attr_spans` must be baked out of the assets: the standalone demo clears every attribute cell to black ink, but SpecTalkZX leaves visible themed attrs outside the Earth attr spans, so uncovered bitmap bits become fixed edge speckles.
- Do not change bitmap deltas just to adapt a theme; adapting the PAPER bits is what makes the globe background transparent to the theme.

## Applied In
- `overlay/earth_about_render.asm`
