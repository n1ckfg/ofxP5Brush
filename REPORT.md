# ofxP5Brush port: progress report

2026-09-24. A port of [p5.brush](https://github.com/acamposuribe/p5.brush) v2.2.3 (JS source at `~/GitHub/p5.brush`) to an openFrameworks 0.12.1 addon.

## Status

- **Builds cleanly.** `example-basic` builds against OF 0.12.1 on the Raspberry Pi 4 (linuxaarch64, GCC 12). There is one GCC 12 `-Wrestrict` warning, a known false positive from `std::string` concatenation.
- **Runs.** Tested headless under Xvfb with Mesa llvmpipe on the default GL 2.1 renderer (GLSL 1.20 path). It runs with no GL or shader errors.
- **Close to the original, not yet identical.** Most test scenes match the JS library nearly pixel for pixel with the same seed. One divergence in watercolor fills is still open (see [Open issues](#open-issues)).
- **Every module is ported.** Strokes, all 11 built-in brushes, custom and image tips, pressure models, vector fields, Polygon/Plot/Position, primitives (line, flowLine, rect, circle, arc, shapes, strokes, splines), watercolor fill, wash, hatch, hatchArray, mass, massArray, clipping, push/pop, seeding and the spectral compositor.

## Layout

| File | Ports (p5.brush `src/`) |
|---|---|
| `src/ofxP5Brush.h` | public API: `ofxP5Brush` class, nested `Color`, `Pressure`, `TipSurface`, `BrushParams`, `Polygon`, `Plot`, `Position` |
| `src/ofxP5Brush.cpp` | `core/color.js` (the `Mix` compositor), `core/target.js`, `core/save.js`, the standalone adapters (canvas, transforms, angle mode) |
| `src/ofxP5BrushStroke.cpp` | `stroke/stroke.js`, `stroke/gl_draw.js`, custom/image tip rasterization |
| `src/ofxP5BrushFill.cpp` | `fill/fill.js`, `fill/wash.js`, `fill/mask.js` |
| `src/ofxP5BrushHatch.cpp` | `hatch/hatch.js`, `hatch/mass.js` |
| `src/ofxP5BrushField.cpp` | `core/flowfield.js` (fields, `Position`) |
| `src/ofxP5BrushGeometry.cpp` | `core/polygon.js`, `core/plot.js`, `core/primitives.js` |
| `src/ofxP5BrushUtils.*` | `core/utils.js`: Mulberry32 PRNG, seed hashing, simplex-noise v4, degree lookup tables |
| `src/ofxP5BrushRaster.*` | new: CPU anti-aliased path rasterizer standing in for the Canvas2D fill mask |
| `src/ofxP5BrushShaders.*` | `core/gl/shader.*`, `stroke/*.vert/frag` as GLSL 1.20 / 1.50 / ES 1.00 / ES 3.00 |
| `example-basic/` | a static composition using the main features (space: new seed, s: save PNG) |

## Design decisions

- **API shape.** `ofxP5Brush brush;` followed by `brush.set("HB", "#2f2a26", 1.4); brush.line(...)`. Most p5.brush calls port almost verbatim. Colors accept `ofColor`, CSS strings (hex, named colors, `rgb()`, `hsl()`) and `{r, g, b}`.
- **Canvas.** The brush paints into its own persistent `ofFbo` (`setup(w, h, density)`), because OF clears the screen every frame. `draw()` displays it. `load(ofFbo&)` targets your own fbo. `begin()`/`end()` allow native OF drawing into the canvas. Pending strokes flush automatically after `ofApp::draw()`, mirroring p5's post-draw hook.
- **Coordinates and transforms.** The origin is at the top left, as in OF. The current OF model matrix (`ofTranslate`/`ofRotate`/`ofScale`) applies to every call. `push()`/`pop()` save brush state together with the matrix. `angleMode()` defaults to RADIANS, as in p5.
- **Rendering.**
  - Stroke stamps: GPU quads that emulate WebGL point sprites, including the 1 px minimum size, drawn into a mask fbo.
  - Watercolor and wash mask: the CPU rasterizer. It uses nonzero fill, anti-aliasing and Canvas2D-style miter-joined strokes, and emulates Canvas's 8-bit storage.
  - Compositing: the same Kubelka-Munk spectral shader as the original.
  - GL: raw GL state is saved and restored around every pass. Works with GL 2.1 (the Pi's default renderer) and 3.2+.
- **Seed parity with JS.** The PRNG, seed hashing (including JS number-to-string formatting), simplex noise, trig lookup tables, and the order of every random draw are ported exactly. Numeric parameters are `double`. Positions are tracked in p5.brush's internal "position space" (offset by half the canvas), so floating-point results match too.

## Deliberate deviations from p5.brush

1. `clip()`/`noClip()` work again. They are empty stubs in the current JS source, although the README documents them. I restored the implementation from p5.brush commit `16282a1`.
2. `Polygon.draw(brush, color, weight)` and `Polygon.fill(color, ...)` now draw even when stroke or fill was off beforehand (JS checks the state from before the override). Omitted fill overrides keep their current values instead of becoming NaN.
3. `fill(r, g, b)` no longer uses `g` as the opacity, and an opacity of 0 is respected.
4. Image tips load synchronously. Transparent areas of PNG tips are flattened onto white, so they carry no ink (JS turns them into ink).
5. Misuse is logged with `ofLogError` instead of throwing. Degenerate cases that would hang or produce NaN in JS are guarded (spray at zero pressure, out-of-range field lookups, zero hatch spacing).
6. `rect()` takes `ofRectMode` instead of `"corner"`/`"center"`. `circle()` returns a `std::tuple<Plot, x, y>`.

## Verification

A headless harness renders the same scenes with this addon and with the original library: its standalone build, run in Chromium through Playwright on SwiftShader. Both use the same seeds. Mean absolute pixel difference, on a 0–255 scale:

| Scene | Features | Mean diff | Notes |
|---|---|---|---|
| lines | all 11 built-in brushes | not scored | visually identical, spray clusters in the same spots |
| fill | watercolor, bleed in/out, texture | 0.44 | after adding 8-bit Canvas emulation (1.18 before) |
| hatch | hatch, hatchStyle, rand | 0.77 | |
| field | seabed/curved fields, flowLine, irregular circle | 0.32 | |
| shapes | spline, beginShape curvature, arc, beginStroke/move | 1.50 | |
| mass | mass with outline | 0.90 | |
| custom | custom tip, image tip, field | 0.05 | |
| wash | wash + stroke | 0.09 | |
| transform | translate/rotate/scale, fill | 2.02 | **the random sequence diverges during the fill** |

Caveat: these scores were recorded before the last change (switching `Position` to position space) and should be re-run.

The harness lives in the session scratchpad and is not in the repo: `testapp/` (OF), `jsref/` (Playwright page), `runall.sh`, `compare.py`, `diff.py`. It is worth adding as `tests/`.

## Open issues

1. **Random-sequence divergence in some watercolor fills.**
   - What happens: filling a plain `rect(150, 220, 300, 160)` with default fill settings (bleed 0.07, texture 0.8) draws a different number of random values than JS. Everything drawn afterwards then differs, as in the transform scene. Fills with other settings (fill scene) matched.
   - Ruled out: the PRNG streams themselves. A trace of 611k draws was identical, value for value.
   - Suspects: the vertices `genPol` generates, where points exactly on axis-aligned edges make inside/outside tests sensitive to floating-point rounding, or a porting slip in `trim`/`grow` at low bleed.
   - Next step: dump `plot.genPol(...)` vertices from both implementations and compare, then do the same for the fill stages.
2. **Untested paths.** The GL 3.2 programmable renderer, GLES, and the Pi 4's real GPU (V3D) have not been exercised; only llvmpipe has. Density > 1, `load(ofFbo&)`, `begin()`/`end()` and `clip()` have not been exercised either.
3. **Not written yet.** The addon README, and ports of the two JS example sketches (`example/`, `example2/`).
4. **Performance.** Unmeasured on real hardware. Under llvmpipe a two-shape watercolor scene takes about 0.5 s. The CPU fill rasterizer and the spectral shader are the hot spots.

## Development note

OF's makefiles do not rebuild addon object files when an addon header changes. After editing any header in `src/`, delete `OF_ROOT/addons/obj/linuxaarch64/Release/ofxP5Brush` before building. Otherwise stale objects with mismatched class layouts get linked; this invalidated a round of test results during this session.
