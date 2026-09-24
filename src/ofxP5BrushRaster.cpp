#include "ofxP5BrushRaster.h"

#include <algorithm>
#include <cmath>

namespace ofxP5BrushDetail {

void MaskRasterizer::beginPath() {
	edges.clear();
	hasPoints = false;
}

void MaskRasterizer::addContour(const std::vector<double> & xy) {
	// Canvas2D silently ignores non-finite coordinates; do the same.
	std::vector<double> pts;
	pts.reserve(xy.size());
	for (size_t i = 0; i + 1 < xy.size(); i += 2) {
		if (std::isfinite(xy[i]) && std::isfinite(xy[i + 1])) {
			pts.push_back(xy[i]);
			pts.push_back(xy[i + 1]);
		}
	}
	const size_t n = pts.size() / 2;
	if (n < 2) return;

	for (size_t i = 0; i < n; ++i) {
		const double x = pts[2 * i];
		const double y = pts[2 * i + 1];
		if (!hasPoints) {
			minX = maxX = x;
			minY = maxY = y;
			hasPoints = true;
		} else {
			minX = std::min(minX, x);
			maxX = std::max(maxX, x);
			minY = std::min(minY, y);
			maxY = std::max(maxY, y);
		}
		const size_t j = (i + 1) % n;
		edges.push_back({x, y, pts[2 * j], pts[2 * j + 1]});
	}
}

PixelRect MaskRasterizer::fill(std::vector<float> & mask, int width, int height, float alpha, bool erase) {
	PixelRect rect;
	// Canvas2D keeps fill styles and pixels at 8 bits per channel: quantize the
	// paint alpha here and every stored value below, so faint layers build up
	// (and saturate) the way they do in p5.brush.
	alpha = std::round(std::min(alpha, 1.f) * 255.f) / 255.f;
	if (!hasPoints || edges.empty() || !(alpha > 0.f)) return rect;

	const int bx0 = std::clamp(static_cast<int>(std::floor(minX)), 0, width);
	const int by0 = std::clamp(static_cast<int>(std::floor(minY)), 0, height);
	const int bx1 = std::clamp(static_cast<int>(std::ceil(maxX)), 0, width);
	const int by1 = std::clamp(static_cast<int>(std::ceil(maxY)), 0, height);
	if (bx1 <= bx0 || by1 <= by0) return rect;

	const int w = bx1 - bx0;
	const int h = by1 - by0;
	stride = w + 2;
	acc.assign(static_cast<size_t>(stride) * h, 0.f);

	for (const auto & e : edges) {
		addClipped(e.x0 - bx0, e.y0 - by0, e.x1 - bx0, e.y1 - by0, w, h);
	}

	const float minCoverage = 1.f / 1024.f;
	for (int y = 0; y < h; ++y) {
		const float * row = acc.data() + static_cast<size_t>(y) * stride;
		float * dst = mask.data() + static_cast<size_t>(by0 + y) * width + bx0;
		float sum = 0.f;
		for (int x = 0; x < w; ++x) {
			sum += row[x];
			const float coverage = std::min(1.f, std::fabs(sum));
			if (coverage > minCoverage) {
				const float sa = coverage * alpha;
				const float blended = erase ? dst[x] * (1.f - sa) : sa + dst[x] * (1.f - sa);
				dst[x] = std::round(blended * 255.f) / 255.f;
			}
		}
	}

	rect.x0 = bx0;
	rect.y0 = by0;
	rect.x1 = bx1;
	rect.y1 = by1;
	return rect;
}

void MaskRasterizer::addClipped(double x0, double y0, double x1, double y1, int w, int h) {
	if (y0 == y1) return;
	if (x0 >= w && x1 >= w) return; // only affects pixels right of the region
	if (x0 >= 0 && x0 <= w && x1 >= 0 && x1 <= w) {
		accumulate(x0, y0, x1, y1, w, h);
		return;
	}
	// Split at x = 0 and x = w. Parts left of 0 become vertical edges at 0
	// (their coverage extends to the right), parts right of w are dropped.
	double ts[4];
	int n = 0;
	ts[n++] = 0.0;
	const double dx = x1 - x0;
	const double dy = y1 - y0;
	if (dx != 0) {
		double t = (0.0 - x0) / dx;
		if (t > 0 && t < 1) ts[n++] = t;
		t = (w - x0) / dx;
		if (t > 0 && t < 1) ts[n++] = t;
	}
	ts[n++] = 1.0;
	std::sort(ts, ts + n);
	for (int i = 0; i + 1 < n; ++i) {
		double xa = x0 + dx * ts[i];
		double xb = x0 + dx * ts[i + 1];
		const double ya = y0 + dy * ts[i];
		const double yb = y0 + dy * ts[i + 1];
		xa = std::clamp(xa, 0.0, static_cast<double>(w));
		xb = std::clamp(xb, 0.0, static_cast<double>(w));
		if (xa >= w && xb >= w) continue;
		accumulate(xa, ya, xb, yb, w, h);
	}
}

void MaskRasterizer::accumulate(double x0, double y0, double x1, double y1, int w, int h) {
	if (y0 == y1) return;
	double dir = 1.0;
	if (y0 > y1) {
		dir = -1.0;
		std::swap(x0, x1);
		std::swap(y0, y1);
	}
	const double dxdy = (x1 - x0) / (y1 - y0);
	double x = x0;
	if (y0 < 0) x -= y0 * dxdy;
	const int yStart = std::max(0, static_cast<int>(std::floor(y0)));
	const int yEnd = std::min(h, static_cast<int>(std::ceil(y1)));
	const double wd = static_cast<double>(w);

	for (int y = yStart; y < yEnd; ++y) {
		float * row = acc.data() + static_cast<size_t>(y) * stride;
		const double dy = std::min(static_cast<double>(y + 1), y1) - std::max(static_cast<double>(y), y0);
		double xnext = x + dxdy * dy;
		x = std::clamp(x, 0.0, wd);
		xnext = std::clamp(xnext, 0.0, wd);
		const double d = dy * dir;
		const double xa = std::min(x, xnext);
		const double xb = std::max(x, xnext);
		const double xaFloor = std::floor(xa);
		const int xai = static_cast<int>(xaFloor);
		const double xbCeil = std::ceil(xb);
		const int xbi = static_cast<int>(xbCeil);
		if (xbi <= xai + 1) {
			const double xmf = 0.5 * (x + xnext) - xaFloor;
			row[xai] += static_cast<float>(d - d * xmf);
			row[xai + 1] += static_cast<float>(d * xmf);
		} else {
			const double s = 1.0 / (xb - xa);
			const double x0f = xa - xaFloor;
			const double a0 = 0.5 * s * (1.0 - x0f) * (1.0 - x0f);
			const double x1f = xb - xbCeil + 1.0;
			const double am = 0.5 * s * x1f * x1f;
			row[xai] += static_cast<float>(d * a0);
			if (xbi == xai + 2) {
				row[xai + 1] += static_cast<float>(d * (1.0 - a0 - am));
			} else {
				const double a1 = s * (1.5 - x0f);
				row[xai + 1] += static_cast<float>(d * (a1 - a0));
				for (int xi = xai + 2; xi < xbi - 1; ++xi) {
					row[xi] += static_cast<float>(d * s);
				}
				const double a2 = a1 + (xbi - xai - 3) * s;
				row[xbi - 1] += static_cast<float>(d * (1.0 - a2 - am));
			}
			row[xbi] += static_cast<float>(d * am);
		}
		x = xnext;
	}
}

} // namespace ofxP5BrushDetail
