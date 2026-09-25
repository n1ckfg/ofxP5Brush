# ofxP5Brush

ofxP5Brush is an openFrameworks C++ port of the p5.brush library — a natural drawing library for pencils, charcoal, markers, watercolor fills, hatch patterns, and vector fields. 

---

## Setup & Lifecycle

Unlike the JavaScript version, ofxP5Brush integrates directly with openFrameworks. There is only one build type.

1. Include the header and declare the brush in your `ofApp.h`:
```cpp
#include "ofxP5Brush.h"

class ofApp : public ofBaseApp {
public:
    void setup();
    void draw();
    
    ofxP5Brush brush;
};
```

2. Initialize in `setup()` and draw to screen in `draw()`:
```cpp
void ofApp::setup() {
    ofBackgroundHex(0xf6f1e8);
    // Initialize with canvas dimensions
    brush.setup(ofGetWidth(), ofGetHeight());
    brush.scaleBrushes(3); // Scale built-in brushes to canvas size
}

void ofApp::draw() {
    // ⚠️ MANDATORY: Call brush.draw() to flush pending geometry 
    // and render the shader composited result to the screen.
    brush.draw(0, 0); 
}
```

### Configuration & State
- `brush.setup(width, height, density = 1.0f)` — Initializes the canvas and shaders.
- `brush.scaleBrushes(scale)` — Scales all built-in brush parameters to match canvas size.
- `brush.push()` / `brush.pop()` — Saves/restores stroke, fill, hatch, mass, field state, and the openFrameworks matrix. You can use standard OF transforms (e.g. `ofTranslate`, `ofRotateDeg`) between push and pop.

### Offscreen Targets

Redirect drawing to a secondary `ofFbo`:
```cpp
ofFbo myFbo;
myFbo.allocate(300, 200, GL_RGBA);

brush.load(myFbo); // Redirect drawing to myFbo
brush.set("HB", ofColor::black, 1.0);
brush.circle(150, 100, 70);
brush.load(); // Call with no arguments to restore the main canvas
```

---

## Shared API

ofxP5Brush follows a state-machine drawing model: first you set drawing state, then you draw shapes.

### Stroke Operations

Built-in brushes include: `HB`, `2B`, `charcoal`, `marker`, `spray`, `rotring`, and `cpencil`.

- `brush.set(brushName, color, weight)` — Activates stroke mode with the specified brush. `color` can be hex strings (e.g., `"#FF0000"`) or `ofColor`.
- `brush.stroke(color)` / `brush.noStroke()` — Updates stroke color or disables outlines.
- `brush.strokeWeight(weight)` — Multiplier for the base brush size.

### Fill Operations

Fill simulates watercolor — soft edges, bleed, texture layering. For better performance, group shapes by fill color/opacity.

- `brush.fill(color, opacity)` / `brush.noFill()` — Activates watercolor fill. Opacity usually 0-255.
- `brush.wash(color, opacity)` / `brush.noWash()` — Activates a fast, flat solid fill pass (no watercolor diffusion).
- `brush.noWash()` — Disable wash.
- `brush.fillBleed(strength, direction, angle)` — Adjusts watercolor diffusion (bleed) strength and direction (`"out"` or `"in"`).
- `brush.fillTexture(textureStrength, borderIntensity, scatter)` — Modifies inner texture granularity and edge darkening.

### Hatch & Mass Operations

- `brush.hatch(dist, angle, options)` — Activates internal line hatching for geometries. `options` is an `ofxP5BrushHatchOptions` struct.
- `brush.mass(brushName, color, options)` — Builds layered hand-filled tone using procedural curved geometry. `options` is an `ofxP5BrushMassOptions` struct.

### Vector Fields

Vector fields direct the motion of brush strokes.

- `brush.field(name)` — Activates a built-in vector field (e.g., `"waves"`, `"curved"`, `"hand"`).
- `brush.noField()` — Deactivates field distortions.
- `brush.addField(name, generatorFunction, angleMode)` — Register a custom field using a lambda `(double t, ofxP5Brush::Field& field) { ... }`.

### Primitives & Geometry

The library supplies drawing functions that behave like openFrameworks primitives but apply brush physics. These evaluate paths, apply forces, and queue geometry for the shader.

- `brush.line(x1, y1, x2, y2)`
- `brush.rect(x, y, w, h)`
- `brush.circle(x, y, radius)`
- `brush.beginShape()` / `brush.vertex(x, y)` / `brush.endShape(close)`
- `brush.spline(points, curvature)`

The advanced `Plot` and `Polygon` classes can be extracted and modified for granular control of the stroke rendering sequence.

### Brush Management

- `brush.box()` — Returns an `std::vector<std::string>` of registered brush names.
- `brush.clip(x1, y1, x2, y2)` / `brush.noClip()` — Restricts strokes and hatches to a clipping rectangle.
- `brush.add(name, params)` — Creates a custom brush using `ofxP5Brush::BrushParams`. Allows setting custom pressure curves, texture spacing, etc.

```cpp
ofxP5Brush::BrushParams params;
params.type = "image";
params.image = "brush_tips/custom.png";
params.weight = 5;
brush.add("my_watercolor", params);
```

---

## Key Gotchas for openFrameworks

- **Mandatory Frame Flush**: You **must** call `brush.draw(x, y)` at the end of your drawing routines to actually render anything to the screen. Omitting this means nothing appears.
- **Structs instead of JS Objects**: Options for hatching, massing, and custom brushes are passed via C++ structs (e.g., `ofxP5Brush::BrushParams`), not inline objects.
- **Color Types**: You can pass standard `ofColor` or hex strings formatted exactly as `"#RRGGBB"`.
- **Compositing**: Internally, strokes are enqueued as instanced geometry (`ofVbo`) and fills are rasterized on CPU (`ofxP5BrushRaster`). `brush.draw()` blends these onto the canvas FBO using a fragment shader simulating Kubelka-Munk spectral mixing.
- **Draw Ordering**: Draw native openFrameworks elements (like text or other UI) *after* `brush.draw(0, 0)` if they need to be visibly on top.
