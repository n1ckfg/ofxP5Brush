# ofxP5Brush Architecture & API

This document provides a comprehensive guide to **ofxP5Brush**, an openFrameworks C++ port of the p5.brush library by Alejandro Campos Uribe. It adapts the structure of the original JS README, outlining core concepts, features, and the C++ API with openFrameworks examples.

## Table of Contents
- [Quick Start](#quick-start)
- [Core Concepts](#core-concepts)
- [Features](#features)
- [Architecture & Rendering Pipeline](#architecture--rendering-pipeline)
- [Reference](#reference)
  - [Lifecycle & Canvas](#lifecycle--canvas)
  - [Vector Fields](#vector-fields)
  - [Brush Management](#brush-management)
  - [Stroke Operations](#stroke-operations)
  - [Fill Operations](#fill-operations)
  - [Hatch & Mass Operations](#hatch--mass-operations)
  - [Primitives & Geometry](#primitives--geometry)

---

## Quick Start

If you already know the basics of openFrameworks, this is the shortest path to drawing with ofxP5Brush:

1. Add `ofxP5Brush brush;` to your `ofApp.h`.
2. Initialize it in `setup()` with `brush.setup(width, height);`.
3. Pick a brush with `brush.set(name, color, weight)`.
4. Draw with a primitive like `brush.line()`, `brush.rect()`, or `brush.circle()`.
5. Call `brush.draw(0, 0)` in `ofApp::draw()` to flush and render to the screen.

```cpp
// ofApp.h
#include "ofxP5Brush.h"

class ofApp : public ofBaseApp {
public:
    void setup();
    void draw();
    
    ofxP5Brush brush;
};

// ofApp.cpp
void ofApp::setup() {
    ofBackgroundHex(0xf6f1e8);
    brush.setup(700, 410);
    brush.scaleBrushes(3);

    brush.set("HB", "#2f2a26", 1.4);
    brush.line(80, 120, 480, 240);

    brush.fill("#d7c3a3", 120);
    brush.noStroke();
    brush.circle(300, 200, 70);

    brush.set("rotring", "#1f4b99", 0.8);
    brush.noFill();
    brush.hatch(7, 35);
    brush.rect(50, 40, 120, 90);
}

void ofApp::draw() {
    brush.draw(0, 0);
}
```

## Core Concepts

- ofxP5Brush follows a state-machine drawing model: first you set drawing state, then you draw shapes.
- `brush.set()`, `brush.stroke()`, `brush.fill()`, `brush.hatch()`, and related functions configure how upcoming shapes should look.
- `brush.line()`, `brush.rect()`, `brush.circle()`, `brush.beginShape()` actually queue geometry.
- Internally, drawing operations evaluate stroke paths, apply vector field forces, rasterize watercolor fills on the CPU, and upload texture splats to the GPU.
- `brush.draw()` resolves pending stroke/fill compositing onto the canvas FBO with Kubelka-Munk spectral blending via a fragment shader, then draws it to the screen.

## Features

- **Kubelka-Munk Spectral Blending**: The C++ port uses a fragment shader to realistically simulate physical pigment mixing.
- **Vector Field Integration**: Direct the motion of your brush strokes with vector fields (e.g., `brush.field("waves")`).
- **Dynamic Brush System**: Includes built-in brushes like `HB`, `2B`, `charcoal`, `marker`, `spray`, `rotring`, and `cpencil`.
- **Custom Tips**: Load custom tips using images or drawing functions via `TipSurface`.
- **Hatch & Mass Patterns**: Implement hatching with precision control over density and orientation, or layered "massing".
- **Watercolor Fill System**: Simulates watercolor with diffusion, bleed edges, and texture scattering.
- **ofFbo Integration**: Route brush drawing to custom `ofFbo` targets using `brush.load()`.

---

## Architecture & Rendering Pipeline

ofxP5Brush decouples shape definition from physical compositing to enable realistic stroke blending:
1. **Geometry Generation**: Lines, splines, and polygons are evaluated into coordinate paths.
2. **Pressure & Noise**: Gaussian/custom pressure profiles are applied along stroke segments.
3. **Field Distortion**: Vertices are bent according to the active `Field` grid of angles.
4. **Rasterization**: 
   - **Strokes**: Are enqueued as instanced circles or quads (`ofVbo`) to be stamped onto a stroke mask FBO.
   - **Fills**: Are rasterized on the CPU (`ofxP5BrushRaster`) into a float array, then uploaded to a texture.
5. **Compositing**: `brush.render()` (called by `brush.draw()`) issues a fullscreen quad using `blendShader`. It reads the stroke/fill masks and blends the active color into the canvas FBO using spectral mixing.

---

## Reference

### Lifecycle & Canvas

- `brush.setup(width, height, density = 1.0f)`
  Initializes the brush canvas (`ofFbo`) and internal shaders.
  
- `brush.draw(x, y, w, h)`
  Flushes pending geometry and draws the brush canvas to the screen.
  
- `brush.load(ofFbo& fbo)`
  Redirects brush drawing to a secondary target FBO. Call `brush.load()` with no arguments to restore the main canvas.
  
- `brush.scaleBrushes(scale)`
  Scales all built-in brush parameters (weight, scatter, spacing) to match your canvas size.

- `brush.push()` / `brush.pop()`
  Saves/restores stroke, fill, wash, hatch, mass, field state and the openFrameworks matrix.

### Vector Fields

- `brush.field(name)`
  Activates a built-in vector field (e.g., `"waves"`, `"curved"`, `"hand"`).

- `brush.noField()`
  Deactivates field distortions.
  
- `brush.addField(name, generatorFunction, angleMode)`
  Register a custom field using a lambda `(double t, ofxP5Brush::Field& field) { ... }`.

### Brush Management

- `brush.box()`
  Returns an `std::vector<std::string>` of registered brush names.

- `brush.add(name, params)`
  Creates a custom brush using `ofxP5Brush::BrushParams`. Allows setting custom pressure curves, texture spacing, etc.
  ```cpp
  ofxP5Brush::BrushParams params;
  params.type = "image";
  params.image = "brush_tips/custom.png";
  params.weight = 5;
  brush.add("my_watercolor", params);
  ```

- `brush.clip(x1, y1, x2, y2)` / `brush.noClip()`
  Restricts strokes and hatches to a clipping rectangle.

### Stroke Operations

- `brush.set(brushName, color, weight)`
  Activates stroke mode with the specified brush. `color` can be hex strings `"#FF0000"` or `ofColor`.
  
- `brush.stroke(color)` / `brush.noStroke()`
  Updates stroke color or disables outlines.

- `brush.strokeWeight(weight)`
  Multiplier for the base brush size.

### Fill Operations

- `brush.fill(color, opacity)` / `brush.noFill()`
  Activates watercolor fill.
  
- `brush.wash(color, opacity)` / `brush.noWash()`
  Activates a fast, flat solid fill pass (no watercolor diffusion).

- `brush.fillBleed(strength, direction, angle)`
  Adjusts watercolor diffusion (bleed) strength and direction ("out" or "in").
  
- `brush.fillTexture(textureStrength, borderIntensity, scatter)`
  Modifies inner texture granularity and edge darkening.

### Hatch & Mass Operations

- `brush.hatch(dist, angle, options)`
  Activates internal line hatching for geometries. `options` is an `ofxP5BrushHatchOptions` struct.

- `brush.mass(brushName, color, options)`
  Builds layered hand-filled tone using procedural curved geometry. `options` is an `ofxP5BrushMassOptions` struct.

### Primitives & Geometry

The library supplies standard drawing functions that behave like openFrameworks primitives but apply brush physics:
- `brush.line(x1, y1, x2, y2)`
- `brush.rect(x, y, w, h)`
- `brush.circle(x, y, radius)`
- `brush.beginShape()`, `brush.vertex(x, y)`, `brush.endShape(close)`
- `brush.spline(points, curvature)`

The advanced `Plot` and `Polygon` classes can be extracted and modified for granular control of the stroke rendering sequence.
