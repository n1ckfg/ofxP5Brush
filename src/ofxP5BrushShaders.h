#pragma once

// GLSL sources used by ofxP5Brush, emitted for the active OpenGL flavor
// (GL 2.1 fixed-function renderer, GL 3.2+ programmable renderer, GLES 2/3).

#include <string>

namespace ofxP5BrushDetail {

enum class GlslFlavor {
	GL2,  // #version 120 (ofGLRenderer)
	GL3,  // #version 150 (ofGLProgrammableRenderer)
	ES2,  // #version 100
	ES3   // #version 300 es
};

GlslFlavor detectGlslFlavor();

/// Shared vertex shader: pixel-space position (xy) + per-vertex alpha (z),
/// quad corner in texcoord.
std::string vertexShader(GlslFlavor flavor);

/// Brush stamps: soft circles (point-sprite emulation) or tinted image tips.
std::string stampFragmentShader(GlslFlavor flavor);

/// Kubelka-Munk spectral compositing of a mask into the target.
std::string blendFragmentShader(GlslFlavor flavor);

} // namespace ofxP5BrushDetail
