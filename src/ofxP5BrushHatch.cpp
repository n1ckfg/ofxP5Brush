// Hatching and massing (port of p5.brush hatch/hatch.js and hatch/mass.js).

#include "ofxP5Brush.h"

using namespace ofxP5BrushDetail;

namespace {

struct Bounds {
	double minX, minY, maxX, maxY, cx, cy, size;
};

struct Segment {
	double scanY, x1, y1, x2, y2;
};

/// Rotates every contour into scanline space, intersects horizontal scanlines
/// with one combined edge list and rotates the spans back.
std::vector<Segment> scanlineHatch(const std::vector<ofxP5Brush::Polygon> & polygons, double angle, double dist,
	double gradient) {
	std::vector<Segment> segments;
	if (!(dist > 0)) return segments;

	const double rad = (angle * PI) / 180;
	const double cosA = std::cos(rad);
	const double sinA = std::sin(rad);

	struct Edge {
		double x1, y1, x2, y2;
	};
	std::vector<Edge> edges;
	double minY = std::numeric_limits<double>::infinity();
	double maxY = -std::numeric_limits<double>::infinity();
	size_t totalVerts = 0;
	for (const auto & polygon : polygons) {
		const auto & verts = polygon.vertices;
		const size_t n = verts.size();
		totalVerts += n;
		std::vector<glm::dvec2> rotated(n);
		for (size_t i = 0; i < n; ++i) {
			rotated[i].x = verts[i].x * cosA - verts[i].y * sinA;
			rotated[i].y = verts[i].x * sinA + verts[i].y * cosA;
			minY = std::min(minY, rotated[i].y);
			maxY = std::max(maxY, rotated[i].y);
		}
		for (size_t i = 0; i < n; ++i) {
			const glm::dvec2 & a = rotated[i];
			const glm::dvec2 & b = rotated[i + 1 < n ? i + 1 : 0];
			if (a.y != b.y) edges.push_back({a.x, a.y, b.x, b.y});
		}
	}
	if (totalVerts == 0) return segments;

	std::vector<double> cx;
	double Y = minY + dist * 0.5;
	double step = dist;
	const bool useGradient = gradient != 1;
	auto emit = [&](double xi, double xj) {
		segments.push_back({Y, xi * cosA + Y * sinA, -xi * sinA + Y * cosA, xj * cosA + Y * sinA, -xj * sinA + Y * cosA});
	};
	while (Y < maxY) {
		cx.clear();
		for (const auto & e : edges) {
			if ((e.y1 <= Y) != (e.y2 <= Y)) cx.push_back(e.x1 + ((Y - e.y1) / (e.y2 - e.y1)) * (e.x2 - e.x1));
		}
		if (cx.size() == 2) {
			emit(std::min(cx[0], cx[1]), std::max(cx[0], cx[1]));
		} else if (cx.size() > 2) {
			std::sort(cx.begin(), cx.end());
			for (size_t i = 0; i + 1 < cx.size(); i += 2) emit(cx[i], cx[i + 1]);
		}
		Y += step;
		if (useGradient) step *= gradient;
	}
	return segments;
}

Bounds polygonBounds(const std::vector<ofxP5Brush::Polygon> & polys) {
	Bounds b;
	b.minX = b.minY = std::numeric_limits<double>::infinity();
	b.maxX = b.maxY = -std::numeric_limits<double>::infinity();
	for (const auto & p : polys) {
		for (const auto & v : p.vertices) {
			b.minX = std::min(b.minX, v.x);
			b.minY = std::min(b.minY, v.y);
			b.maxX = std::max(b.maxX, v.x);
			b.maxY = std::max(b.maxY, v.y);
		}
	}
	b.cx = (b.minX + b.maxX) / 2;
	b.cy = (b.minY + b.maxY) / 2;
	b.size = std::hypot(b.maxX - b.minX, b.maxY - b.minY);
	return b;
}

bool pointInRing(const std::vector<glm::dvec2> & points, double x, double y) {
	bool inside = false;
	const size_t n = points.size();
	for (size_t i = 0, j = n - 1; i < n; j = i++) {
		const double xi = points[i].x, yi = points[i].y;
		const double xj = points[j].x, yj = points[j].y;
		double dy = yj - yi;
		if (dy == 0) dy = std::numeric_limits<double>::epsilon();
		const bool intersects = ((yi > y) != (yj > y)) && (x < ((xj - xi) * (y - yi)) / dy + xi);
		if (intersects) inside = !inside;
	}
	return inside;
}

} // namespace

// =============================================================================
// Hatch state
// =============================================================================

void ofxP5Brush::hatch(double dist, double angle, const HatchOptions & options) {
	hatchState.isActive = true;
	hatchState.dist = dist;
	hatchState.angle = toDegreesSigned(angle);
	hatchState.options = options;
}

void ofxP5Brush::hatchStyle(const std::string & brushName, const Color & color, double weight) {
	hatchState.hBrush = HatchBrush {brushName, color, weight};
}

void ofxP5Brush::noHatch() {
	hatchState.isActive = false;
	hatchState.hBrush.reset();
}

void ofxP5Brush::hatchArray(const std::vector<Polygon> & polygons) {
	createHatch(polygons);
}

// =============================================================================
// Hatch lines
// =============================================================================

std::vector<ofxP5Brush::HatchLine> ofxP5Brush::getHatchLines(const std::vector<Polygon> & polygons) {
	const double dist = hatchState.dist;
	const double angle = std::fmod(std::fmod(hatchState.angle, 180.0) + 180.0, 180.0);
	const HatchOptions & options = hatchState.options;
	const double gradient = options.gradient != 0 ? mapRange(options.gradient, 0, 1, 1, 1.1, true) : 1;

	auto segs = scanlineHatch(polygons, angle, dist, gradient);
	// Scanline traversal order, so lines can be chained into a serpentine.
	std::stable_sort(segs.begin(), segs.end(),
		[](const Segment & a, const Segment & b) { return a.scanY == b.scanY ? a.x1 < b.x1 : a.scanY < b.scanY; });

	const double r = options.rand;
	std::vector<HatchLine> lines;
	lines.reserve(segs.size() * (options.continuous ? 2 : 1));
	for (size_t j = 0; j < segs.size(); ++j) {
		const Segment & s = segs[j];
		double x1 = s.x1, y1 = s.y1, x2 = s.x2, y2 = s.y2;
		if (r != 0) {
			x1 += 2 * r * dist * rr(-1, 1);
			y1 += 2 * r * dist * rr(-1, 1);
			x2 += 2 * r * dist * rr(-1, 1);
			y2 += 2 * r * dist * rr(-1, 1);
		}
		const bool reverse = options.continuous && j % 2 == 1;
		const HatchLine line = reverse ? HatchLine {x2, y2, x1, y1, s.scanY, false} : HatchLine {x1, y1, x2, y2, s.scanY, false};
		lines.push_back(line);
		if (j > 0 && options.continuous) {
			const HatchLine prev = lines[lines.size() - 2];
			lines.push_back({prev.x2, prev.y2, line.x1, line.y1, s.scanY, true});
		}
	}
	return lines;
}

void ofxP5Brush::createHatch(const std::vector<Polygon> & polygons) {
	if (!hatchState.hBrush && (!strokeState.isActive || !strokeState.hasColor)) {
		ofLogWarning("ofxP5Brush") << "hatch needs a stroke: call set() or hatchStyle() first";
		return;
	}
	const auto lines = getHatchLines(polygons);
	const StrokeState saved = strokeState;
	for (const auto & l : lines) {
		if (hatchState.hBrush) {
			set(hatchState.hBrush->brush, hatchState.hBrush->color, hatchState.hBrush->weight * rr(0.9, 1.1));
		}
		line(l.x1, l.y1, l.x2, l.y2);
	}
	strokeState = saved;
}

// =============================================================================
// Mass
// =============================================================================

void ofxP5Brush::mass(const std::string & brushName, const Color & color, const MassOptions & options) {
	massState.brush = brushName;
	massState.color = color;
	massState.options = options;
	massState.isActive = true;
}

void ofxP5Brush::noMass() {
	massState.isActive = false;
	massState.brush.clear();
	massState.color = Color();
	massState.options = MassOptions();
}

void ofxP5Brush::massArray(const std::vector<Polygon> & polygons) {
	createMass(MassShape {polygons, true});
}

void ofxP5Brush::massArray(const Polygon & polygon) {
	createMass(MassShape {{polygon}, false});
}

void ofxP5Brush::createMass(const MassShape & shape) {
	if (massState.brush.empty() || !getBrushParams(massState.brush)) {
		ofLogError("ofxP5Brush") << "mass() needs a valid brush name";
		return;
	}

	// One base polygon plus two translated copies.
	const double scatter = getBrushParams(massState.brush)->scatter;
	const double maxJitter = std::min(scatter * 2, 5.0);
	glm::dvec2 jitters[2];
	for (auto & j : jitters) {
		j.x = rr2(-maxJitter, maxJitter);
		j.y = rr2(-maxJitter, maxJitter);
	}
	MassShape pols[3] = {shape, shape, shape};
	for (int k = 1; k < 3; ++k) {
		for (auto & poly : pols[k].polys) {
			for (auto & v : poly.vertices) v += jitters[k - 1];
		}
	}

	const HatchState savedHatch = hatchState;
	const StrokeState savedStroke = strokeState;
	const FieldState savedField = fieldState;
	const MassOptions & o = massState.options;
	const double precision = o.precision;
	const double strength = o.strength;
	const double gradient = o.gradient;
	const double hatchDist = 1.6 * rr2(scatter * 0.65, scatter * 0.85) - 0.4 * gradient;
	const double baseAngle = rr2(-90, 90);

	// Diagonal corner family for the whole gesture.
	glm::dvec2 pivotBias;
	const double normalizedAngle = std::fmod(std::fmod(baseAngle + 90, 180.0) + 180.0, 180.0) - 90;
	if (normalizedAngle >= 0) pivotBias = rr2() < 0.5 ? glm::dvec2(1, 1) : glm::dvec2(-1, -1);
	else pivotBias = rr2() < 0.5 ? glm::dvec2(-1, 1) : glm::dvec2(1, -1);

	set(massState.brush, massState.color, 1);
	wiggle(2 - precision);
	if (o.outline) {
		for (auto & poly : pols[0].polys) polygonDraw(poly, nullptr, nullptr, 1);
	}

	drawMassPass(pols[0], hatchDist * 0.9, baseAngle, {2 - 2 * precision, true, gradient}, pivotBias);
	if (strength > 0.33) {
		const double angle = baseAngle + 20 * rr2(-1, 1);
		drawMassPass(pols[1], hatchDist, angle, {0.6 - 0.6 * precision, true, gradient}, pivotBias);
	}
	if (strength > 0.66) {
		const double angle = baseAngle + 15 * rr2(-1, 1);
		drawMassPass(pols[2], hatchDist * 0.8, angle, {0.6 - 0.6 * precision, true, gradient}, pivotBias);
	}

	strokeState = savedStroke;
	hatchState = savedHatch;
	fieldState = savedField;
}

void ofxP5Brush::drawMassPass(const MassShape & shape, double dist, double angleDeg, const HatchOptions & options,
	const glm::dvec2 & pivotBias) {
	// Configure a hatch pass, then redraw each hatch line as an arc around a
	// pivot projected onto the line's perpendicular bisector.
	hatchState.isActive = true;
	hatchState.dist = dist;
	hatchState.angle = angleDeg;
	hatchState.options = options;
	if (shape.isArray) hatchState.options.continuous = false;

	const Bounds bounds = polygonBounds(shape.polys);
	const double offset = bounds.size * rr2(0.6, 1.4);
	const glm::dvec2 anchor(bounds.cx + pivotBias.x * offset, bounds.cy + pivotBias.y * offset);

	auto inShape = [&](double x, double y) {
		if (!shape.isArray) return !shape.polys.empty() && pointInRing(shape.polys[0].vertices, x, y);
		bool inside = false;
		for (const auto & poly : shape.polys) {
			if (pointInRing(poly.vertices, x, y)) inside = !inside;
		}
		return inside;
	};
	auto arcFits = [&](double cx, double cy, double radius, double startDeg, double endDeg) {
		const double sweepDeg = std::fmod(std::fmod(endDeg - startDeg, 360.0) + 360.0, 360.0);
		for (double t : {0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875}) {
			const double rad = ((startDeg + sweepDeg * t) * PI) / 180;
			if (!inShape(cx + radius * std::cos(rad), cy - radius * std::sin(rad))) return false;
		}
		return true;
	};

	for (const auto & seg : getHatchLines(shape.polys)) {
		// Interrupt some gestures by splitting the line with a small gap.
		std::vector<HatchLine> parts;
		if (!seg.isConnector && rr2() < 0.35) {
			const double t = rr2(0.35, 0.65);
			const double mx = seg.x1 + (seg.x2 - seg.x1) * t;
			const double my = seg.y1 + (seg.y2 - seg.y1) * t;
			const double dx = seg.x2 - seg.x1;
			const double dy = seg.y2 - seg.y1;
			double length = std::hypot(dx, dy);
			if (length == 0) length = 1;
			const double gap = rr2(0.04, 0.1) * length;
			const double gx = (dx / length) * gap * 0.5;
			const double gy = (dy / length) * gap * 0.5;
			parts.push_back({seg.x1, seg.y1, mx - gx, my - gy, seg.scanY, false});
			parts.push_back({mx + gx, my + gy, seg.x2, seg.y2, seg.scanY, false});
		} else {
			parts.push_back(seg);
		}

		for (const auto & part : parts) {
			// Project the anchor onto the perpendicular bisector: a circle
			// centre equidistant from both endpoints.
			const double mx = (part.x1 + part.x2) / 2;
			const double my = (part.y1 + part.y2) / 2;
			const double dx = part.x2 - part.x1;
			const double dy = part.y2 - part.y1;
			const double len = std::hypot(dx, dy);
			if (!(len != 0)) continue;
			const double nx = -dy / len;
			const double ny = dx / len;
			const double off = (anchor.x - mx) * nx + (anchor.y - my) * ny;
			glm::dvec2 center(mx + nx * off, my + ny * off);

			if (shape.isArray) {
				// Pull the centre towards short chords so arcs stay inside.
				const double chord = std::hypot(dx, dy);
				const double size = std::max({bounds.size, chord, 1.0});
				const double shortness = 1 - std::min(1.0, chord / (size * 0.42));
				const double bias = 0.08 + shortness * 0.18;
				center.x += (mx - center.x) * bias;
				center.y += (my - center.y) * bias;
			}

			const double radius = ofxP5BrushDetail::dist(center.x, center.y, part.x1, part.y1);
			if (!(radius != 0) || std::isnan(radius)) continue;

			// Shortest arc between the endpoints, or the long one if only it fits.
			double startDeg = calcAngle(center.x, center.y, part.x1, part.y1);
			double endDeg = calcAngle(center.x, center.y, part.x2, part.y2);
			const double sweep = std::fmod(std::fmod(endDeg - startDeg, 360.0) + 360.0, 360.0);
			if (sweep > 180) std::swap(startDeg, endDeg);
			const bool shortFits = arcFits(center.x, center.y, radius, startDeg, endDeg);
			const bool longFits = arcFits(center.x, center.y, radius, endDeg, startDeg);
			if (shortFits) {
				arcDegrees(center.x, center.y, radius, startDeg, endDeg);
			} else if (longFits) {
				arcDegrees(center.x, center.y, radius, endDeg, startDeg);
			}
		}
	}
}
