#pragma once

// A small anti-aliased path rasterizer for the watercolor / wash mask.
//
// p5.brush draws its fill layers into a Canvas2D alpha mask (nonzero fill,
// anti-aliased, source-over / destination-out). This class reproduces that on
// the CPU: contours are accumulated with exact signed-area coverage (the
// technique used by font-rs / stb_truetype) and the resulting coverage is
// composited into a float alpha buffer.

#include <vector>

namespace ofxP5BrushDetail {

struct PixelRect {
	int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	bool empty() const { return x1 <= x0 || y1 <= y0; }
};

class MaskRasterizer {
public:
	/// Starts a new path (a set of closed contours in pixel space).
	void beginPath();

	/// Adds a closed contour. `xy` holds x0, y0, x1, y1, ... in pixels.
	void addContour(const std::vector<double> & xy);

	/// Fills the current path with the nonzero rule and composites it into
	/// `mask` (width x height floats). `erase == false` uses source-over,
	/// `erase == true` uses destination-out. Returns the touched pixel rect.
	PixelRect fill(std::vector<float> & mask, int width, int height, float alpha, bool erase = false);

private:
	struct Edge {
		double x0, y0, x1, y1;
	};

	void accumulate(double x0, double y0, double x1, double y1, int w, int h);
	void addClipped(double x0, double y0, double x1, double y1, int w, int h);

	std::vector<Edge> edges;
	double minX = 0, minY = 0, maxX = 0, maxY = 0;
	bool hasPoints = false;
	std::vector<float> acc;
	int stride = 0;
};

} // namespace ofxP5BrushDetail
