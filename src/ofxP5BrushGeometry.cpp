// Polygon and Plot classes and the drawing primitives (port of p5.brush
// core/polygon.js, core/plot.js and core/primitives.js).

#include "ofxP5Brush.h"

using namespace ofxP5BrushDetail;

namespace {

inline double jsRound(double v) {
	return std::floor(v + 0.5);
}

/// Intersection of segment s1 (treated as a line) with segment s2, or of two
/// lines when includeSegmentExtension is true.
std::optional<glm::dvec2> intersectLines(const glm::dvec2 & s1a, const glm::dvec2 & s1b, const glm::dvec2 & s2a,
	const glm::dvec2 & s2b, bool includeSegmentExtension = false) {
	const double dx1 = s1b.x - s1a.x, dy1 = s1b.y - s1a.y;
	const double dx2 = s2b.x - s2a.x, dy2 = s2b.y - s2a.y;
	// Handles parallel lines and zero-length segments.
	const double denom = dy2 * dx1 - dx2 * dy1;
	if (denom == 0) return std::nullopt;
	const double dy13 = s1a.y - s2a.y, dx13 = s1a.x - s2a.x;
	const double ua = (dx2 * dy13 - dy2 * dx13) / denom;
	const double ub = (dx1 * dy13 - dy1 * dx13) / denom;
	if (!includeSegmentExtension && (ub < 0 || ub > 1)) return std::nullopt;
	return glm::dvec2(s1a.x + ua * dx1, s1a.y + ua * dy1);
}

} // namespace

// =============================================================================
// Polygon
// =============================================================================

ofxP5Brush::Polygon::Polygon(const std::vector<glm::vec2> & points, ofxP5Brush * b)
	: brush(b) {
	vertices.reserve(points.size());
	for (const auto & p : points) vertices.emplace_back(p.x, p.y);
}

ofxP5Brush::Polygon::Polygon(Raw, std::vector<glm::dvec2> points, ofxP5Brush * b)
	: vertices(std::move(points))
	, brush(b) { }

ofxP5Brush * ofxP5Brush::Polygon::owner() const {
	ofxP5Brush * b = brush ? brush : ofxP5Brush::currentInstance;
	if (!b) ofLogError("ofxP5Brush") << "Polygon has no brush to draw with";
	return b;
}

std::vector<std::pair<glm::dvec2, glm::dvec2>> ofxP5Brush::Polygon::getSides() const {
	std::vector<std::pair<glm::dvec2, glm::dvec2>> sides;
	for (size_t i = 0; i < vertices.size(); ++i) {
		sides.emplace_back(vertices[i], vertices[(i + 1) % vertices.size()]);
	}
	return sides;
}

std::vector<glm::dvec2> ofxP5Brush::Polygon::intersect(const glm::dvec2 & p1, const glm::dvec2 & p2) const {
	std::vector<glm::dvec2> points;
	const size_t n = vertices.size();
	for (size_t k = 0; k < n; ++k) {
		if (auto hit = intersectLines(p1, p2, vertices[k], vertices[(k + 1) % n])) points.push_back(*hit);
	}
	return points;
}

ofxP5Brush::Polygon & ofxP5Brush::Polygon::draw() {
	if (auto b = owner()) b->polygonDraw(*this, nullptr, nullptr, 1);
	return *this;
}

ofxP5Brush::Polygon & ofxP5Brush::Polygon::draw(const std::string & brushName, const Color & color, double weight) {
	if (auto b = owner()) b->polygonDraw(*this, &brushName, &color, weight);
	return *this;
}

ofxP5Brush::Polygon & ofxP5Brush::Polygon::fill() {
	if (auto b = owner()) b->polygonFill(*this);
	return *this;
}

ofxP5Brush::Polygon & ofxP5Brush::Polygon::fill(const Color & color, double opacity, std::optional<double> bleed,
	std::optional<double> texture, std::optional<double> border, std::optional<std::string> direction,
	std::optional<double> angle) {
	auto b = owner();
	if (!b) return *this;
	const FillState saved = b->fillState;
	b->fill(color, opacity);
	if (bleed) b->fillState.bleedStrength = constrain(*bleed, 0, 1);
	if (direction) b->fillState.direction = *direction;
	if (angle) b->fillState.angle = b->toDegreesSigned(*angle);
	if (texture) b->fillState.textureStrength = constrain(*texture, 0, 1);
	if (border) b->fillState.borderStrength = constrain(*border, 0, 1);
	b->polygonFill(*this);
	b->fillState = saved;
	return *this;
}

ofxP5Brush::Polygon & ofxP5Brush::Polygon::wash() {
	if (auto b = owner()) b->polygonWash(*this);
	return *this;
}

ofxP5Brush::Polygon & ofxP5Brush::Polygon::wash(const Color & color, double opacity) {
	auto b = owner();
	if (!b) return *this;
	const WashState saved = b->washState;
	b->wash(color, opacity);
	b->polygonWash(*this);
	b->washState = saved;
	return *this;
}

ofxP5Brush::Polygon & ofxP5Brush::Polygon::hatch() {
	if (auto b = owner()) b->polygonHatch(*this);
	return *this;
}

ofxP5Brush::Polygon & ofxP5Brush::Polygon::hatch(double dist, double angle, const HatchOptions & options) {
	auto b = owner();
	if (!b) return *this;
	const HatchState saved = b->hatchState;
	if (dist != 0) b->hatch(dist, angle, options);
	b->polygonHatch(*this);
	b->hatchState = saved;
	return *this;
}

ofxP5Brush::Polygon & ofxP5Brush::Polygon::mass() {
	if (auto b = owner()) b->polygonMass(*this);
	return *this;
}

ofxP5Brush::Polygon & ofxP5Brush::Polygon::show() {
	if (auto b = owner()) b->polygonShow(*this);
	return *this;
}

void ofxP5Brush::polygonDraw(Polygon & p, const std::string * brushName, const Color * color, double weight) {
	const StrokeState saved = strokeState;
	if (brushName && color) set(*brushName, *color, weight);
	if (strokeState.isActive) {
		const size_t n = p.vertices.size();
		for (size_t i = 0; i < n; ++i) {
			const auto & a = p.vertices[i];
			const auto & b = p.vertices[(i + 1) % n];
			line(a.x, a.y, b.x, b.y);
		}
	}
	strokeState = saved;
}

void ofxP5Brush::polygonFill(Polygon & p) {
	const FillState saved = fillState;
	if (fillState.isActive) {
		isFieldReady();
		createFill(p);
	}
	fillState = saved;
}

void ofxP5Brush::polygonWash(Polygon & p) {
	const WashState saved = washState;
	if (washState.isActive) drawWashPolygon(p);
	washState = saved;
}

void ofxP5Brush::polygonHatch(Polygon & p) {
	const HatchState saved = hatchState;
	if (hatchState.isActive) createHatch({p});
	hatchState = saved;
}

void ofxP5Brush::polygonMass(Polygon & p) {
	if (massState.isActive) createMass(MassShape {{p}, false});
}

void ofxP5Brush::polygonShow(Polygon & p) {
	if (washState.isActive) polygonWash(p);
	if (fillState.isActive) polygonFill(p);
	if (massState.isActive) polygonMass(p);
	if (hatchState.isActive) polygonHatch(p);
	if (strokeState.isActive) polygonDraw(p, nullptr, nullptr, 1);
}

// =============================================================================
// Plot
// =============================================================================

ofxP5Brush::Plot::Plot(Type t, ofxP5Brush * b)
	: type(t)
	, brush(b) { }

ofxP5Brush * ofxP5Brush::Plot::owner() const {
	ofxP5Brush * b = brush ? brush : ofxP5Brush::currentInstance;
	if (!b) ofLogError("ofxP5Brush") << "Plot has no brush to draw with";
	return b;
}

void ofxP5Brush::Plot::addSegment(double a, double len, double pressure, bool degrees) {
	if (!angles.empty()) angles.pop_back();
	if (degrees) {
		a = std::fmod(std::fmod(a, 360.0) + 360.0, 360.0);
	} else if (auto b = owner()) {
		a = b->toDegrees(a);
	}
	angles.push_back(a); // pushed twice for continuity
	angles.push_back(a);
	pres.push_back(pressure);
	cumLen.push_back(length);
	segments.push_back(len);
	length += len;
}

void ofxP5Brush::Plot::endPlot(double a, double pressure, bool degrees) {
	if (degrees) {
		a = std::fmod(std::fmod(a, 360.0) + 360.0, 360.0);
	} else if (auto b = owner()) {
		a = b->toDegrees(a);
	}
	if (!angles.empty()) angles.back() = a;
	pres.push_back(pressure);
}

void ofxP5Brush::Plot::rotate(double a) {
	if (auto b = owner()) dir = b->toDegrees(a);
}

double ofxP5Brush::Plot::pressure(double d) {
	if (pres.empty()) return 1;
	if (d > length) return pres.back();
	const size_t i = std::min(static_cast<size_t>(index), pres.size() - 1);
	const double p0 = pres[i];
	const double p1 = i + 1 < pres.size() ? pres[i + 1] : p0;
	const double seg = i < segments.size() ? segments[i] : 0;
	return seg == 0 ? p0 : p0 + (d - suma) / seg * (p1 - p0);
}

double ofxP5Brush::Plot::angle(double d) {
	if (angles.empty()) return dir;
	if (d > length) return angles.back();
	calcIndex(d);
	if (type != CURVE) return angles[index] + dir;
	double a0 = angles[index];
	double a1 = static_cast<size_t>(index) + 1 < angles.size() ? angles[index + 1] : a0;
	if (std::abs(a1 - a0) > 180) {
		if (a1 > a0) a1 = -(360 - a1);
		else a0 = -(360 - a0);
	}
	const double seg = segments[index];
	const double t = seg == 0 ? 0 : (d - suma) / seg;
	return a0 + t * (a1 - a0) + dir;
}

int ofxP5Brush::Plot::calcIndex(double d) {
	const int n = static_cast<int>(cumLen.size());
	if (n == 0) {
		index = 0;
		suma = 0;
		return 0;
	}
	int i = index < n ? index : n - 1;
	// Forward scan from the cached index (monotone access), binary search back.
	while (i + 1 < n && cumLen[i + 1] <= d) ++i;
	if (cumLen[i] > d) {
		int lo = 0, hi = i - 1;
		while (lo < hi) {
			const int mid = (lo + hi + 1) >> 1;
			if (cumLen[mid] <= d) lo = mid;
			else hi = mid - 1;
		}
		i = std::max(lo, 0);
	}
	index = i;
	suma = cumLen[i];
	return i;
}

ofxP5Brush::Polygon ofxP5Brush::Plot::genPol(double x, double y, double scale, double side) {
	if (auto b = owner()) return b->genPolFromPlot(*this, x, y, scale, side);
	return Polygon();
}

ofxP5Brush::Plot & ofxP5Brush::Plot::draw(double x, double y, double scale) {
	if (auto b = owner()) b->plotDraw(*this, x, y, scale);
	return *this;
}

ofxP5Brush::Plot & ofxP5Brush::Plot::fill(double x, double y, double scale) {
	if (auto b = owner()) b->plotFill(*this, x, y, scale);
	return *this;
}

ofxP5Brush::Plot & ofxP5Brush::Plot::wash(double x, double y, double scale) {
	if (auto b = owner()) b->plotWash(*this, x, y, scale);
	return *this;
}

ofxP5Brush::Plot & ofxP5Brush::Plot::hatch(double x, double y, double scale) {
	if (auto b = owner()) b->plotHatch(*this, x, y, scale);
	return *this;
}

ofxP5Brush::Plot & ofxP5Brush::Plot::mass(double x, double y, double scale) {
	if (auto b = owner()) b->plotMass(*this, x, y, scale);
	return *this;
}

ofxP5Brush::Plot & ofxP5Brush::Plot::show(double x, double y, double scale) {
	if (auto b = owner()) b->plotShow(*this, x, y, scale);
	return *this;
}

ofxP5Brush::Polygon ofxP5Brush::genPolFromPlot(Plot & plot, double x, double y, double, double side) {
	isFieldReady();
	const double step = side < 0 ? 4 : 1;
	std::vector<glm::dvec2> vertices;
	const int numSteps = static_cast<int>(jsRound(plot.length / step));
	Position pos(x, y, this);
	double pside = 0;
	int prevIdx = 0;

	for (int i = 0; i < numSteps; ++i) {
		pos.movePos(&plot, 0, step, step, 1, false, 0);
		const int idx = plot.index; // set by angle() inside movePos
		pside += step;
		const double maxSize = side <= 0 ? 8 : std::max(plot.segments[idx] * side * rr2(0.7, 1.3), 20.0);
		// p5.brush skips vertices whose position-space x is exactly zero or NaN.
		if ((pside >= maxSize || idx >= prevIdx) && pos.px != 0 && !std::isnan(pos.px)) {
			vertices.emplace_back(pos.x, pos.y);
			pside = 0;
			if (idx >= prevIdx) prevIdx++;
		}
	}
	return Polygon(Polygon::Raw {}, std::move(vertices), this);
}

void ofxP5Brush::plotDraw(Plot & p, double x, double y, double scale) {
	if (!strokeState.isActive) return;
	if (p.hasOrigin) {
		x = p.origin.x;
		y = p.origin.y;
		scale = 1;
	}
	drawPlot(p, x, y, scale);
}

void ofxP5Brush::plotFill(Plot & p, double x, double y, double scale) {
	if (!fillState.isActive) return;
	if (p.hasOrigin) {
		x = p.origin.x;
		y = p.origin.y;
		scale = 1;
	}
	const double side = fillState.bleedStrength < 0.06 ? 0 : mapRange(fillState.bleedStrength, 0, 0.6, 0.2, 0.6, true);
	p.pol = genPolFromPlot(p, x, y, scale, side);
	polygonFill(p.pol);
}

void ofxP5Brush::plotWash(Plot & p, double x, double y, double scale) {
	if (!washState.isActive) return;
	if (p.hasOrigin) {
		x = p.origin.x;
		y = p.origin.y;
		scale = 1;
	}
	p.pol = genPolFromPlot(p, x, y, scale, 0);
	polygonWash(p.pol);
}

void ofxP5Brush::plotHatch(Plot & p, double x, double y, double scale) {
	if (!hatchState.isActive) return;
	if (p.hasOrigin) {
		x = p.origin.x;
		y = p.origin.y;
		scale = 1;
	}
	p.pol = genPolFromPlot(p, x, y, scale, 0.3);
	polygonHatch(p.pol);
}

void ofxP5Brush::plotMass(Plot & p, double x, double y, double scale) {
	if (!massState.isActive) return;
	if (p.hasOrigin) {
		x = p.origin.x;
		y = p.origin.y;
		scale = 1;
	}
	createMass(MassShape {{genPolFromPlot(p, x, y, scale, 0.15)}, false});
}

void ofxP5Brush::plotShow(Plot & p, double x, double y, double scale) {
	plotWash(p, x, y, scale);
	plotFill(p, x, y, scale);
	plotMass(p, x, y, scale);
	plotHatch(p, x, y, scale);
	plotDraw(p, x, y, scale);
}

// =============================================================================
// Primitives
// =============================================================================

ofxP5Brush::Polygon ofxP5Brush::polygon(const std::vector<glm::vec2> & points) {
	Polygon p(points, this);
	polygonShow(p);
	return p;
}

void ofxP5Brush::rect(double x, double y, double w, double h, ofRectMode mode) {
	if (mode == OF_RECTMODE_CENTER) {
		x -= w / 2;
		y -= h / 2;
	}
	beginShape(0);
	vertex(x, y);
	vertex(x + w, y);
	vertex(x + w, y + h);
	vertex(x, y + h);
	endShape(true);
}

std::tuple<ofxP5Brush::Plot, double, double> ofxP5Brush::circle(double x, double y, double radius, double r) {
	Plot p(Plot::CURVE, this);
	const double arcLength = PI * radius;
	const double angleOffset = rr2(0, 360);
	auto randomFactor = [&]() { return r != 0 ? 1 + r * 0.2 * rr2() : 1.0; };

	// Four quarter segments
	for (int i = 0; i < 4; ++i) {
		const double angle = -90.0 * i + angleOffset;
		const double f1 = randomFactor();
		const double f2 = randomFactor();
		p.addSegment(angle * f1, (arcLength / 2) * f2, 1, true);
	}

	// Optionally overshoot or fall short of closing the loop
	if (r != 0) {
		const double randomAngle = r * randInt2(-5, 5);
		p.addSegment(angleOffset, std::abs(randomAngle) * (PI / 180) * radius, 1, true);
		p.endPlot(randomAngle + angleOffset, 1, true);
	} else {
		p.endPlot(angleOffset, 1, true);
	}

	const double offsetX = x - radius * sinDeg(angleOffset);
	const double offsetY = y - radius * cosDeg(-angleOffset);
	plotShow(p, offsetX, offsetY, 1);
	return {p, offsetX, offsetY};
}

ofxP5Brush::Plot ofxP5Brush::arc(double x, double y, double radius, double start, double end) {
	return arcDegrees(x, y, radius, toDegreesSigned(start), toDegreesSigned(end));
}

ofxP5Brush::Plot ofxP5Brush::arcDegrees(double x, double y, double radius, double startDeg, double endDeg) {
	const double sweepDeg = std::fmod(std::fmod(endDeg - startDeg, 360.0) + 360.0, 360.0);
	if (sweepDeg == 0) return Plot(Plot::CURVE, this);

	Plot p(Plot::CURVE, this);
	const int segmentCount = std::max(1, static_cast<int>(std::ceil(sweepDeg / 90)));
	const double segmentSweep = sweepDeg / segmentCount;
	const double arcLength = (PI * radius * segmentSweep) / 180;
	for (int i = 0; i < segmentCount; ++i) {
		p.addSegment(startDeg + i * segmentSweep + 90, arcLength, 1, true);
	}
	p.endPlot(startDeg + sweepDeg + 90, 1, true);

	const double startX = x + radius * cosDeg(startDeg);
	const double startY = y - radius * sinDeg(startDeg);
	plotDraw(p, startX, startY, 1);
	return p;
}

void ofxP5Brush::beginShape(double curvature) {
	shapeCurvature = constrain(curvature, 0, 1);
	currentShape = SubPath();
	currentShape->curvature = shapeCurvature;
}

void ofxP5Brush::vertex(double x, double y, double pressure) {
	if (!currentShape) {
		ofLogError("ofxP5Brush") << "vertex() called outside of beginShape()/endShape()";
		return;
	}
	currentShape->vert.emplace_back(x, y, pressure);
}

ofxP5Brush::Plot ofxP5Brush::endShape(bool close) {
	if (!currentShape) {
		ofLogError("ofxP5Brush") << "endShape() called without beginShape()";
		return Plot(Plot::CURVE, this);
	}
	if (currentShape->vert.size() < 2) {
		ofLogError("ofxP5Brush") << "endShape() requires at least 2 vertices";
		currentShape.reset();
		return Plot(Plot::CURVE, this);
	}
	if (close) {
		currentShape->vert.push_back(currentShape->vert[0]);
		currentShape->isClosed = true;
	}
	Plot plot = createSpline(currentShape->vert, currentShape->curvature, currentShape->isClosed);
	currentShape.reset();
	plotShow(plot, 0, 0, 1);
	return plot;
}

void ofxP5Brush::beginStroke(Plot::Type type, double x, double y) {
	strokeOrigin = glm::dvec2(x, y);
	strokePlot = Plot(type, this);
}

void ofxP5Brush::move(double angle, double length, double pressure) {
	if (!strokePlot) {
		ofLogError("ofxP5Brush") << "move() called without beginStroke()";
		return;
	}
	strokePlot->addSegment(angle, length, pressure);
}

void ofxP5Brush::endStroke(double angle, double pressure) {
	if (!strokePlot) {
		ofLogError("ofxP5Brush") << "endStroke() called without beginStroke()";
		return;
	}
	strokePlot->endPlot(angle, pressure);
	plotDraw(*strokePlot, strokeOrigin.x, strokeOrigin.y, 1);
	strokePlot.reset();
}

ofxP5Brush::Plot ofxP5Brush::spline(const std::vector<glm::vec2> & points, double curvature) {
	std::vector<glm::dvec3> pts;
	pts.reserve(points.size());
	for (const auto & p : points) pts.emplace_back(p.x, p.y, 1.0);
	return splineImpl(pts, curvature);
}

ofxP5Brush::Plot ofxP5Brush::spline(const std::vector<glm::vec3> & points, double curvature) {
	std::vector<glm::dvec3> pts;
	pts.reserve(points.size());
	for (const auto & p : points) pts.emplace_back(p.x, p.y, p.z);
	return splineImpl(pts, curvature);
}

ofxP5Brush::Plot ofxP5Brush::splineImpl(const std::vector<glm::dvec3> & points, double curvature) {
	if (points.size() < 2) {
		ofLogError("ofxP5Brush") << "spline() requires at least 2 points";
		return Plot(Plot::CURVE, this);
	}
	Plot p = createSpline(points, curvature, false);
	plotShow(p, 0, 0, 1);
	return p;
}

ofxP5Brush::Plot ofxP5Brush::createSpline(std::vector<glm::dvec3> points, double curvature, bool close) {
	// Straight runs between points; with curvature > 0 each corner is replaced
	// by a circular arc tangent to both neighbouring runs.
	Plot p(curvature == 0 ? Plot::SEGMENTS : Plot::CURVE, this);
	const double PI2 = PI * 2;

	if (close && curvature != 0) points.push_back(points[1]);
	if (points.empty()) return p;

	double done = 0; // excess length consumed by the previous corner
	double pep = 0, pep2 = 0;
	glm::dvec2 tep(0, 0);
	const size_t n = points.size();

	for (size_t i = 0; i + 1 < n; ++i) {
		if (curvature > 0 && i + 2 < n) {
			const glm::dvec3 & p1 = points[i];
			const glm::dvec3 & p2 = points[i + 1];
			const glm::dvec3 & p3 = points[i + 2];

			const double d1 = dist(p1.x, p1.y, p2.x, p2.y);
			const double d2 = dist(p2.x, p2.y, p3.x, p3.y);
			const double a1 = calcAngle(p1.x, p1.y, p2.x, p2.y);
			const double a2 = calcAngle(p2.x, p2.y, p3.x, p3.y);

			const double curvAdjust = curvature * std::min({d1, d2, 0.5 * std::min(d1, d2)});
			const double dmax = std::max(d1, d2);
			const double s1 = d1 - curvAdjust;
			const double s2 = d2 - curvAdjust;

			if (std::floor(a1) == std::floor(a2)) {
				// Nearly collinear: keep it straight.
				const double temp = close ? (i == 0 ? 0 : d1 - done) : d1 - done;
				const double temp2 = close ? (i == 0 ? 0 : d2 - pep2) : d2;
				p.addSegment(a1, temp, p1.z, true);
				if (i == n - 3) p.addSegment(a2, temp2, p2.z, true);
				done = 0;
				if (i == 0) {
					pep = d1;
					pep2 = curvAdjust;
					tep = glm::dvec2(points[1].x, points[1].y);
				}
			} else {
				// Corner: find the tangent arc through the two offset points.
				const glm::dvec2 point1(p2.x - curvAdjust * cosDeg(-a1), p2.y - curvAdjust * sinDeg(-a1));
				const glm::dvec2 point2(point1.x + dmax * cosDeg(-a1 + 90), point1.y + dmax * sinDeg(-a1 + 90));
				const glm::dvec2 point3(p2.x + curvAdjust * cosDeg(-a2), p2.y + curvAdjust * sinDeg(-a2));
				const glm::dvec2 point4(point3.x + dmax * cosDeg(-a2 + 90), point3.y + dmax * sinDeg(-a2 + 90));

				double arcLength = std::numeric_limits<double>::quiet_NaN();
				if (const auto intPt = intersectLines(point1, point2, point3, point4, true)) {
					const double radius = dist(point1.x, point1.y, intPt->x, intPt->y);
					const double halfDist = dist(point1.x, point1.y, point3.x, point3.y) / 2;
					const double arcAngle = 2 * std::asin(halfDist / radius) * (180 / PI);
					arcLength = (PI2 * radius * arcAngle) / 360;
				}

				const double temp = close ? (i == 0 ? 0 : s1 - done) : s1 - done;
				const double temp2 = i == n - 3 ? (close ? pep - curvAdjust : s2) : 0;

				p.addSegment(a1, temp, p1.z, true);
				p.addSegment(a1, std::isnan(arcLength) ? 0 : arcLength, p1.z, true);
				p.addSegment(a2, temp2, p2.z, true);

				done = curvAdjust;
				if (i == 0) {
					pep = s1;
					pep2 = curvAdjust;
					tep = point1;
				}
			}

			if (i == n - 3) p.endPlot(a2, p2.z, true);
		} else if (curvature == 0) {
			const glm::dvec3 & p1 = points[i];
			const glm::dvec3 & p2 = points[i + 1];
			const double d = dist(p1.x, p1.y, p2.x, p2.y);
			const double a = calcAngle(p1.x, p1.y, p2.x, p2.y);
			p.addSegment(a, d, p1.z, true);
			if (i == n - 2) p.endPlot(a, p2.z, true);
		}
	}

	p.hasOrigin = true;
	p.origin = close && curvature != 0 ? tep : glm::dvec2(points[0].x, points[0].y);
	return p;
}
