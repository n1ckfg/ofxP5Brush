// Brushes and strokes (port of p5.brush stroke/stroke.js and stroke/gl_draw.js).

#include "ofxP5Brush.h"

using namespace ofxP5BrushDetail;

namespace {

inline double jsRound(double v) {
	return std::floor(v + 0.5);
}

const glm::vec2 QUAD_CORNERS[6] = {{-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1}};

glm::mat4 pixelProjection(int w, int h) {
	glm::mat4 proj(1.0f);
	proj[0][0] = 2.f / w;
	proj[1][1] = 2.f / h;
	proj[3][0] = -1.f;
	proj[3][1] = -1.f;
	return proj;
}

struct StrokeGLGuard {
	GLint framebuffer = 0;
	GLint viewport[4] = {0, 0, 0, 0};
	GLboolean blend = GL_FALSE, depth = GL_FALSE, scissor = GL_FALSE, cull = GL_FALSE;
	GLint srcRGB = GL_ONE, dstRGB = GL_ZERO, srcA = GL_ONE, dstA = GL_ZERO;
	GLint eqRGB = GL_FUNC_ADD, eqA = GL_FUNC_ADD;
	GLint tex0 = 0;
	StrokeGLGuard() {
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
		glGetIntegerv(GL_VIEWPORT, viewport);
		blend = glIsEnabled(GL_BLEND);
		depth = glIsEnabled(GL_DEPTH_TEST);
		scissor = glIsEnabled(GL_SCISSOR_TEST);
		cull = glIsEnabled(GL_CULL_FACE);
		glGetIntegerv(GL_BLEND_SRC_RGB, &srcRGB);
		glGetIntegerv(GL_BLEND_DST_RGB, &dstRGB);
		glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcA);
		glGetIntegerv(GL_BLEND_DST_ALPHA, &dstA);
		glGetIntegerv(GL_BLEND_EQUATION_RGB, &eqRGB);
		glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &eqA);
		glActiveTexture(GL_TEXTURE0);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex0);
	}
	~StrokeGLGuard() {
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
		set(GL_BLEND, blend);
		set(GL_DEPTH_TEST, depth);
		set(GL_SCISSOR_TEST, scissor);
		set(GL_CULL_FACE, cull);
		glBlendFuncSeparate(srcRGB, dstRGB, srcA, dstA);
		glBlendEquationSeparate(eqRGB, eqA);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex0);
	}
	static void set(GLenum cap, GLboolean on) {
		if (on) glEnable(cap);
		else glDisable(cap);
	}
};

} // namespace

// =============================================================================
// Pressure
// =============================================================================

ofxP5Brush::Pressure::Pressure(std::initializer_list<double> ramp)
	: Pressure(std::vector<double>(ramp)) { }

ofxP5Brush::Pressure::Pressure(const std::vector<double> & ramp) {
	// [start, end] or [start, mid, end], normalized into a [0, 1] curve.
	mode = CUSTOM;
	double s = 1, m = 1, e = 1;
	if (ramp.size() == 1) {
		s = m = e = ramp[0];
	} else if (ramp.size() == 2) {
		s = ramp[0];
		m = (ramp[0] + ramp[1]) / 2;
		e = ramp[1];
	} else if (ramp.size() >= 3) {
		s = ramp[0];
		m = ramp[1];
		e = ramp[2];
	}
	const double lo = std::min({s, m, e});
	const double hi = std::max({s, m, e});
	double range = hi - lo;
	if (range == 0) range = 1;
	const double ns = (s - lo) / range;
	const double nm = (m - lo) / range;
	const double ne = (e - lo) / range;
	minMax = {lo, hi};
	fn = [ns, nm, ne](double t) { return t < 0.5 ? ns + (nm - ns) * t * 2 : nm + (ne - nm) * (t - 0.5) * 2; };
}

ofxP5Brush::Pressure ofxP5Brush::Pressure::gaussian(glm::dvec2 curveParams, glm::dvec2 range) {
	Pressure p;
	p.mode = GAUSSIAN;
	p.curve = curveParams;
	p.minMax = range;
	return p;
}

ofxP5Brush::Pressure ofxP5Brush::Pressure::custom(std::function<double(double)> curveFn, glm::dvec2 range) {
	Pressure p;
	p.mode = CUSTOM;
	p.fn = std::move(curveFn);
	p.minMax = range;
	return p;
}

// =============================================================================
// Brush manager
// =============================================================================

void ofxP5Brush::addStandardBrushes() {
	auto make = [](double weight, double scatter, double sharpness, double grain, double opacity, double spacing,
					Pressure pressure, const std::string & type = "default", const std::string & rotate = "none",
					double noise = 0.3) {
		BrushParams p;
		p.weight = weight;
		p.scatter = scatter;
		p.sharpness = sharpness;
		p.grain = grain;
		p.opacity = opacity;
		p.spacing = spacing;
		p.pressure = std::move(pressure);
		p.type = type;
		p.rotate = rotate;
		p.markerTip = true;
		p.noise = noise;
		return p;
	};
	add("pen", make(0.3, 0.15, 0.9, 0.7, 150, 0.1, Pressure::gaussian({0.15, 0.2}, {1.2, 1})));
	add("rotring", make(0.15, 0.05, 0.7, 0.9, 210, 0.1, Pressure::gaussian({0.35, 0.2}, {1.3, 1})));
	add("2B", make(0.3, 0.75, 0.45, 0.8, 180, 0.1, Pressure::gaussian({0.1, 0.3}, {1.1, 0.9})));
	add("HB", make(0.3, 0.6, 0.3, 0.7, 170, 0.1, Pressure::gaussian({0.15, 0.2}, {1.1, 0.9})));
	add("2H", make(0.2, 0.6, 0.3, 0.75, 120, 0.1, Pressure::gaussian({0.15, 0.2}, {1.1, 0.9})));
	add("cpencil", make(0.35, 0.55, 0.8, 0.7, 75, 0.1, Pressure::gaussian({0.15, 0.2}, {0.95, 1.1})));
	add("pastel", make(0.7, 5, 0.91, 1, 30, 0.085 / 3, Pressure::gaussian({0.4, 0.05}, {1.09, 0.93}), "default",
					  "natural", 1));
	add("crayon", make(0.33, 1.9, 0.75, 2, 159, 0.07, Pressure {1.1, 0.9}, "default", "natural", 1));
	add("charcoal", make(0.35, 1.5, 0.68, 2, 120, 0.03, Pressure::gaussian({0.15, 0.4}, {1.1, 0.95})));
	add("spray", make(0.2, 6, 15, 40, 90, 0.5, Pressure::gaussian({0.2, 0.35}, {0.7, 1}), "spray"));
	add("marker", make(2, 0.2, 0, 0, 1, 0.03, Pressure::gaussian({0.35, 0.25}, {1.2, 0.85}), "marker"));
}

bool ofxP5Brush::add(const std::string & name, BrushParams params) {
	if (params.type != "marker" && params.type != "custom" && params.type != "image" && params.type != "spray") {
		params.type = "default";
	}
	params.noise = constrain(params.noise, 0, 1);
	if (params.pressure.mode == Pressure::CUSTOM && !params.pressure.fn) {
		ofLogError("ofxP5Brush") << "brush \"" << name << "\" has a custom pressure without a curve function";
		return false;
	}

	BrushEntry entry;
	entry.tipAngleMode = angleModeValue;
	if (params.type == "custom") {
		if (!params.tip) {
			ofLogError("ofxP5Brush") << "brush \"" << name << "\" is type \"custom\" but is missing a tip function";
			return false;
		}
		entry.tipKey = "custom::" + name;
		tipTextures.erase(entry.tipKey); // discard a stale tip if re-added
	} else if (params.type == "image") {
		if (params.image.empty()) {
			ofLogError("ofxP5Brush") << "brush \"" << name
									 << "\" is type \"image\" but is missing params.image (a file in bin/data)";
			return false;
		}
		entry.tipKey = params.image;
	}
	entry.param = std::move(params);

	if (brushes.find(name) == brushes.end()) brushOrder.push_back(name);
	auto & stored = brushes[name] = std::move(entry);

	// Rasterize / load the tip now if a GL context exists; otherwise on first use.
	if (!stored.tipKey.empty() && ofGetGLRenderer()) getTipTexture(stored);
	return true;
}

std::vector<std::string> ofxP5Brush::box() const {
	return brushOrder;
}

const ofxP5Brush::BrushParams * ofxP5Brush::getBrushParams(const std::string & name) const {
	const auto it = brushes.find(name);
	return it == brushes.end() ? nullptr : &it->second.param;
}

void ofxP5Brush::scaleBrushes(double scale) {
	for (auto & kv : brushes) {
		kv.second.param.weight *= scale;
		kv.second.param.scatter *= scale;
		kv.second.param.spacing *= scale;
	}
}

void ofxP5Brush::pick(const std::string & brushName) {
	if (brushes.find(brushName) == brushes.end()) {
		std::string available;
		for (const auto & n : brushOrder) {
			if (!available.empty()) available += ", ";
			available += n;
		}
		ofLogError("ofxP5Brush") << "brush \"" << brushName << "\" not found. Available brushes: " << available;
		return;
	}
	strokeState.type = brushName;
}

void ofxP5Brush::stroke(const Color & color) {
	strokeState.color = color;
	strokeState.hasColor = true;
	strokeState.isActive = true;
}

void ofxP5Brush::strokeWeight(double weight) {
	strokeState.weight = weight;
}

void ofxP5Brush::set(const std::string & brushName, const Color & color, double weight) {
	if (brushes.find(brushName) == brushes.end()) {
		pick(brushName); // logs the error
		return;
	}
	pick(brushName);
	stroke(color);
	strokeWeight(weight);
}

void ofxP5Brush::noStroke() {
	strokeState.isActive = false;
}

void ofxP5Brush::clip(double x1, double y1, double x2, double y2) {
	// The region lives in the coordinate space active when clip() is called.
	const Affine t = getAffineMatrix();
	const double det = t.a * t.d - t.b * t.c;
	if (std::abs(det) < 1e-12) {
		ofLogError("ofxP5Brush") << "clip() cannot be used with a non-invertible transform";
		return;
	}
	ClipWindow window;
	window.minX = std::min(x1, x2);
	window.minY = std::min(y1, y2);
	window.maxX = std::max(x1, x2);
	window.maxY = std::max(y1, y2);
	window.inverse.a = t.d / det;
	window.inverse.b = -t.b / det;
	window.inverse.c = -t.c / det;
	window.inverse.d = t.a / det;
	window.inverse.x = (t.c * t.y - t.d * t.x) / det;
	window.inverse.y = (t.b * t.x - t.a * t.y) / det;
	strokeState.clipWindow = window;
}

void ofxP5Brush::noClip() {
	strokeState.clipWindow.reset();
}

// =============================================================================
// Strokes
// =============================================================================

void ofxP5Brush::line(double x1, double y1, double x2, double y2) {
	if (!strokeState.isActive || !strokeState.hasColor) {
		ofLogError("ofxP5Brush") << "no brush or color set. Call set(\"brushName\", color) before drawing";
		return;
	}
	isFieldReady();
	const double d = dist(x1, y1, x2, y2);
	if (d == 0) return;
	initializeDrawingState(x1, y1, d, nullptr);
	drawStroke(calcAngle(x1, y1, x2, y2), false);
}

void ofxP5Brush::flowLine(double x, double y, double length, double dir) {
	if (!strokeState.isActive || !strokeState.hasColor) {
		ofLogError("ofxP5Brush") << "no brush or color set. Call set(\"brushName\", color) before drawing";
		return;
	}
	isFieldReady();
	initializeDrawingState(x, y, length, nullptr);
	drawStroke(toDegrees(dir), false);
}

void ofxP5Brush::drawPlot(Plot & p, double x, double y, double scale) {
	isFieldReady();
	initializeDrawingState(x, y, p.length, &p);
	drawStroke(scale, true);
}

void ofxP5Brush::initializeDrawingState(double x, double y, double length, Plot * plot) {
	snapshotMatrix();
	sPosition = Position(x, y, this);
	sLength = length;
	sPlot = plot;
	if (sPlot) sPlot->calcIndex(0);
}

void ofxP5Brush::snapshotMatrix() {
	sMatrix = getAffineMatrix();
	sScale = std::sqrt(sMatrix.a * sMatrix.a + sMatrix.b * sMatrix.b);
}

void ofxP5Brush::drawStroke(double angleScale, bool isPlot) {
	if (!isPlot) sDir = angleScale;
	if (!saveStrokeState()) {
		sPlot = nullptr;
		return;
	}

	const double stepSize = cur.p->spacing;
	const double total = jsRound((sLength * (isPlot ? angleScale : 1)) / stepSize);
	const int totalSteps = std::isfinite(total) ? static_cast<int>(std::max(0.0, total)) : 0;
	cur.pressureCount = 10;
	cur.hasCachedPressure = false;

	const size_t neededGaussians = static_cast<size_t>(totalSteps) * 2;
	while (strokeGaussians.size() < neededGaussians) strokeGaussians.push_back(gaussian());

	for (int i = 0; i < totalSteps; ++i) {
		if (isPlot) sCachedPlotAngle = sPlot->angle(sPosition.plotted);
		tip();
		if (isPlot) {
			sPosition.movePos(sPlot, 0, stepSize, stepSize, angleScale, true, sCachedPlotAngle);
		} else {
			sPosition.moveToDegrees(angleScale, stepSize, stepSize);
		}
	}
	restoreStrokeState();
	sPlot = nullptr;
}

bool ofxP5Brush::saveStrokeState() {
	cur.seed = rr() * 999999;
	const auto it = brushes.find(strokeState.type);
	if (it == brushes.end()) return false;
	cur.entry = &it->second;
	cur.p = &it->second.param;
	const BrushParams & p = *cur.p;

	// Per-stroke pressure parameters
	const Pressure & pressure = p.pressure;
	cur.isCustomPressure = pressure.mode == Pressure::CUSTOM;
	cur.a = !cur.isCustomPressure ? rr(-1, 1) : 0;
	cur.b = !cur.isCustomPressure ? rr(1, 1.5) : 0;
	if (!cur.isCustomPressure) {
		cur.cp = rr(3, 3.5);
		cur.ct = 0;
		cur.cs = 1;
		cur.ck = 0;
	} else {
		const auto & v = pressure.variation;
		cur.cp = rr(-v.offset, v.offset);
		cur.ct = rr(-v.warp, v.warp);
		cur.cs = rr(1 - v.scale, 1 + v.scale);
		cur.ck = rr(-v.tilt, v.tilt);
	}
	cur.min = pressure.minMax.x;
	cur.max = pressure.minMax.y;

	// Stroke direction for direction-aware dispersion (lines only)
	if (!sPlot) {
		cur.cos = cosDeg(sDir);
		cur.sin = sinDeg(sDir);
	}

	// Switch the compositor to the brush mask
	const bool switchingToBrush = mixIsBrush != 1;
	mixIsBrush = 1;
	if (switchingToBrush) mixJustChanged = true;
	blend(&strokeState.color);

	// Stroke-level noise: whole strokes come out subtly lighter or darker.
	const double baseAlpha = calculateAlpha();
	const double noiseStrength = 0.1 * p.noise;
	cur.alpha = noiseStrength > 0 ? std::max(0.0, baseAlpha * (1 + gaussian(0, noiseStrength))) : baseAlpha;
	cur.overscan = getImageTipOverscan();
	cur.kind = p.type == "spray" ? 1 : p.type == "marker" ? 2 : (p.type == "custom" || p.type == "image") ? 3 : 0;

	// Clip-window test coefficients: brush coords -> screen -> clip space.
	cur.clipActive = strokeState.clipWindow.has_value();
	if (cur.clipActive) {
		const Affine & I = strokeState.clipWindow->inverse;
		const Affine & T = sMatrix;
		cur.clipA = I.a * T.a + I.c * T.b;
		cur.clipC = I.a * T.c + I.c * T.d;
		cur.clipB = I.b * T.a + I.d * T.b;
		cur.clipD = I.b * T.c + I.d * T.d;
		// Fold the position-space offset (half the canvas) into the translation.
		cur.clipTX = I.a * T.x + I.c * T.y + I.x - cur.clipA * (Cwidth / 2) - cur.clipC * (Cheight / 2);
		cur.clipTY = I.b * T.x + I.d * T.y + I.y - cur.clipB * (Cwidth / 2) - cur.clipD * (Cheight / 2);
		cur.clipMinX = strokeState.clipWindow->minX;
		cur.clipMaxX = strokeState.clipWindow->maxX;
		cur.clipMinY = strokeState.clipWindow->minY;
		cur.clipMaxY = strokeState.clipWindow->maxY;
	}

	markerTip();
	return true;
}

void ofxP5Brush::restoreStrokeState() {
	markerTip();
	glDrawCircles();
	if (cur.kind == 3 && cur.entry) glDrawImages(cur.entry->tipKey);
}

bool ofxP5Brush::isInsideClippingArea() const {
	if (!cur.clipActive) return true;
	const double lx = cur.clipA * sPosition.px + cur.clipC * sPosition.py + cur.clipTX;
	if (lx < cur.clipMinX || lx > cur.clipMaxX) return false;
	const double ly = cur.clipB * sPosition.px + cur.clipD * sPosition.py + cur.clipTY;
	return ly >= cur.clipMinY && ly <= cur.clipMaxY;
}

void ofxP5Brush::tip() {
	if (!isInsideClippingArea()) return;
	const double pressure = calculatePressure();
	switch (cur.kind) {
	case 1: drawSpray(pressure); break;
	case 2: drawMarker(pressure, true, cur.alpha); break;
	case 3: drawImageTip(pressure, cur.alpha); break;
	default: drawDefault(pressure); break;
	}
}

double ofxP5Brush::calculatePressure() {
	if (cur.pressureCount >= 10 || !cur.hasCachedPressure) {
		cur.cachedPressure = sPlot ? simPressure() * sPlot->pressure(sPosition.plotted) : simPressure();
		cur.hasCachedPressure = true;
		cur.pressureCount = 0;
	}
	cur.pressureCount++;
	return cur.cachedPressure;
}

double ofxP5Brush::simPressure() {
	if (!cur.isCustomPressure) return gauss();
	if (!cur.p->pressure.fn) return 1;
	const double t = sPosition.plotted / sLength;
	return mapRange(cur.p->pressure.fn(std::max(0.0, std::min(1.0, 0.5 + (t - 0.5 + cur.ct) * cur.cs))) + cur.cp
			+ cur.ck * (t - 0.5),
		0, 1, cur.min, cur.max, true);
}

double ofxP5Brush::gauss() {
	// Asymmetric bell envelope along the stroke.
	const double a = 0.5 + cur.p->pressure.curve.x * cur.a;
	const double b = 1 - cur.p->pressure.curve.y * cur.b;
	const double c = cur.cp;
	const double peakPos = a * sLength;
	const double halfWidth = (sPosition.plotted < peakPos ? b * 1.2 : b * 0.8) * (sLength / 2);
	return mapRange(1 / (1 + std::pow(std::abs((sPosition.plotted - peakPos) / halfWidth), 2 * c)), 0, 1, cur.min,
		cur.max);
}

double ofxP5Brush::calculateAlpha() const {
	const std::string & type = cur.p->type;
	return (type == "default" || type == "spray") ? cur.p->opacity : cur.p->opacity / std::min(strokeState.weight, 1.3);
}

double ofxP5Brush::getImageTipOverscan() const {
	const double maxPressure = std::max(1.0, cur.max);
	const double scatterReach = strokeState.weight * cur.p->scatter;
	const double tipReach = strokeState.weight * cur.p->weight * maxPressure;
	return std::max(8.0, scatterReach * 1.5 + tipReach * 0.75);
}

// ---------------------------------------------------------------------------
// Tips
// ---------------------------------------------------------------------------

void ofxP5Brush::drawSpray(double pressure) {
	const BrushParams & p = *cur.p;
	const double gaussianPick = rng();
	const double g = strokeGaussians.empty() ? 0 : strokeGaussians[toInt32(gaussianPick * strokeGaussians.size())];
	const double vibration = strokeState.weight * p.scatter * pressure + (strokeState.weight * g * p.scatter) / 3;
	const double sw = p.weight * rr(0.9, 1.1);
	const double iterationsD = std::ceil(p.grain / pressure);
	const int iterations = std::isfinite(iterationsD) ? static_cast<int>(std::min(std::max(iterationsD, 0.0), 1e5)) : 0;
	for (int j = 0; j < iterations; ++j) {
		const double r = rr(0.9, 1.1);
		const double rX = r * vibration * rr(-1, 1);
		const double yRandomFactor = rr(-1, 1);
		const double sqrtPart = std::sqrt((r * vibration) * (r * vibration) - rX * rX);
		queueCircle(sPosition.px + rX, sPosition.py + yRandomFactor * sqrtPart, sw, cur.alpha);
	}
}

void ofxP5Brush::drawMarker(double pressure, bool vibrate, double alpha) {
	const BrushParams & p = *cur.p;
	const double vibration = vibrate ? strokeState.weight * p.scatter : 0;
	const double rx = vibrate ? vibration * rr(-1, 1) : 0;
	const double ry = vibrate ? vibration * rr(-1, 1) : 0;
	queueCircle(sPosition.px + rx, sPosition.py + ry, strokeState.weight * p.weight * pressure,
		alpha * std::max(0.8, pressure) * rr(0.9, 1.1));
}

void ofxP5Brush::drawImageTip(double pressure, double alpha) {
	const BrushParams & p = *cur.p;
	const double vibration = strokeState.weight * p.scatter;
	const double rx = vibration * rr(-1, 1);
	const double ry = vibration * rr(-1, 1);
	const double size = p.weight * strokeState.weight * pressure;
	double angle = 0;
	if (p.rotate == "random") {
		angle = randInt(0, 360) * (PI / 180);
	} else if (p.rotate == "natural") {
		angle = ((sPlot ? -sCachedPlotAngle : -sDir) + sPosition.angle()) * (PI / 180);
	}
	queueImage(sPosition.px + rx, sPosition.py + ry, size, angle, alpha * std::max(0.8, pressure) * rr(0.9, 1.1),
		cur.overscan);
}

void ofxP5Brush::drawDefault(double pressure) {
	const BrushParams & p = *cur.p;
	if (rr(0, 1) >= p.grain * pressure) return;
	const double gaussianPick = rng();
	const double g = strokeGaussians.empty() ? 0 : strokeGaussians[toInt32(gaussianPick * strokeGaussians.size())];
	const double vibration = strokeState.weight * p.scatter * (p.sharpness + ((1 - p.sharpness) * g) / pressure);
	double dx, dy;
	if (sPlot) {
		const double plotCos = cosDeg(sCachedPlotAngle);
		const double plotSin = sinDeg(sCachedPlotAngle);
		const double perp = vibration * rr(-1, 1);
		const double along = 0.3 * vibration * rr(-1, 1);
		dx = perp * plotSin + along * plotCos;
		dy = perp * plotCos - along * plotSin;
	} else {
		const double perp = vibration * rr(-1, 1);
		const double along = 0.3 * vibration * rr(-1, 1);
		dx = perp * cur.sin + along * cur.cos;
		dy = perp * cur.cos - along * cur.sin;
	}
	const double diameter = pressure * pressure * p.weight * rr(0.85, 1.15) * strokeState.weight;
	const double alpha = std::max(0.9, pressure) * cur.alpha * rr(0.75, 1.1);
	queueCircle(sPosition.px + dx, sPosition.py + dy, diameter, alpha);
}

void ofxP5Brush::markerTip() {
	if (!cur.p || !cur.p->markerTip) return;
	if (!isInsideClippingArea()) return;
	const double pressure = calculatePressure();
	const double alpha = cur.alpha;
	if (cur.kind == 2) {
		for (int s = 1; s < 10; ++s) drawMarker((pressure * s) / 10, true, alpha * 8);
	} else if (cur.kind == 3) {
		for (int s = 1; s < 5; ++s) drawImageTip((pressure * s) / 10, alpha * 2);
	}
}

// ---------------------------------------------------------------------------
// GL stamp batching (gl_draw.js)
// ---------------------------------------------------------------------------

void ofxP5Brush::queueCircle(double x, double y, double diameter, double alpha) {
	// x / y arrive in position space; back to brush coordinates, then screen.
	const double ux = x - sPosition.halfW;
	const double uy = y - sPosition.halfH;
	const double sx = Density * (sMatrix.a * ux + sMatrix.c * uy + sMatrix.x);
	const double sy = Density * (sMatrix.b * ux + sMatrix.d * uy + sMatrix.y);
	const double radius = (Density * diameter * sScale) / 2;
	if (!std::isfinite(sx) || !std::isfinite(sy) || !std::isfinite(radius)) return;
	// WebGL point sprites are at least one pixel wide; emulate that.
	const double half = std::max(radius, 0.5);
	const float a = static_cast<float>(alpha / 255);
	for (const auto & corner : QUAD_CORNERS) {
		circlePos.emplace_back(static_cast<float>(sx + corner.x * half), static_cast<float>(sy + corner.y * half), a);
		circleCorner.push_back(corner);
	}
	const DirtyRect r {sx - radius - 1, sy - radius - 1, sx + radius + 1, sy + radius + 1};
	if (!circleDirty) {
		circleDirty = r;
	} else {
		circleDirty->minX = std::min(circleDirty->minX, r.minX);
		circleDirty->minY = std::min(circleDirty->minY, r.minY);
		circleDirty->maxX = std::max(circleDirty->maxX, r.maxX);
		circleDirty->maxY = std::max(circleDirty->maxY, r.maxY);
	}
}

void ofxP5Brush::queueImage(double x, double y, double size, double angle, double alpha, double extraPadding) {
	const double ux = x - sPosition.halfW;
	const double uy = y - sPosition.halfH;
	const double sx = Density * (sMatrix.a * ux + sMatrix.c * uy + sMatrix.x);
	const double sy = Density * (sMatrix.b * ux + sMatrix.d * uy + sMatrix.y);
	const double halfSize = (Density * size * sScale) / 2;
	const double extraRadius = Density * extraPadding * sScale;
	if (!std::isfinite(sx) || !std::isfinite(sy) || !std::isfinite(halfSize) || !std::isfinite(angle)) return;
	const double c = std::cos(angle);
	const double s = std::sin(angle);
	const float a = static_cast<float>(alpha / 255);
	for (const auto & corner : QUAD_CORNERS) {
		const double rx = c * corner.x - s * corner.y;
		const double ry = s * corner.x + c * corner.y;
		imgPos.emplace_back(static_cast<float>(sx + rx * halfSize), static_cast<float>(sy + ry * halfSize), a);
		imgCorner.push_back(corner);
	}
	// Rotated square worst case: a circle of radius halfSize * sqrt(2).
	const double bounds = halfSize * 1.42 + extraRadius;
	const DirtyRect r {sx - bounds - 1, sy - bounds - 1, sx + bounds + 1, sy + bounds + 1};
	if (!imgDirty) {
		imgDirty = r;
	} else {
		imgDirty->minX = std::min(imgDirty->minX, r.minX);
		imgDirty->minY = std::min(imgDirty->minY, r.minY);
		imgDirty->maxX = std::max(imgDirty->maxX, r.maxX);
		imgDirty->maxY = std::max(imgDirty->maxY, r.maxY);
	}
}

void ofxP5Brush::glDrawCircles() {
	if (circlePos.empty()) return;
	strokeMaskInfo.isDrawn = true;
	drawStamps(circlePos, circleCorner, 0.f, nullptr);
	if (circleDirty) markDirtyRect(strokeMaskInfo, *circleDirty);
	circlePos.clear();
	circleCorner.clear();
	circleDirty.reset();
}

void ofxP5Brush::glDrawImages(const std::string &) {
	if (imgPos.empty()) return;
	ofTexture * tex = cur.entry ? getTipTexture(*cur.entry) : nullptr;
	if (tex) {
		strokeMaskInfo.isDrawn = true;
		drawStamps(imgPos, imgCorner, 1.f, tex);
		if (imgDirty) markDirtyRect(strokeMaskInfo, *imgDirty);
	}
	imgPos.clear();
	imgCorner.clear();
	imgDirty.reset();
}

void ofxP5Brush::drawStamps(const std::vector<glm::vec3> & pos, const std::vector<glm::vec2> & corners, float mode,
	const ofTexture * tex) {
	if (!shadersLoaded || !strokeMaskFbo.isAllocated() || pos.empty()) return;
	const int n = static_cast<int>(pos.size());
	StrokeGLGuard guard;
	glBindFramebuffer(GL_FRAMEBUFFER, strokeMaskFbo.getId());
	glViewport(0, 0, maskWidth, maskHeight);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);

	stampVbo.setVertexData(pos.data(), n, GL_STREAM_DRAW);
	stampVbo.setTexCoordData(corners.data(), n, GL_STREAM_DRAW);

	const Color & c = strokeState.color;
	stampShader.begin();
	stampShader.setUniformMatrix4f("u_proj", pixelProjection(maskWidth, maskHeight));
	stampShader.setUniform4f("u_color", c.r, c.g, c.b, c.a);
	stampShader.setUniform1f("u_mode", mode);
	if (tex) stampShader.setUniformTexture("u_tex", GL_TEXTURE_2D, tex->getTextureData().textureID, 0);
	stampVbo.draw(GL_TRIANGLES, 0, n);
	stampShader.end();
}

// ---------------------------------------------------------------------------
// Tip textures
// ---------------------------------------------------------------------------

void ofxP5Brush::imageToWhite(ofPixels & pixels) {
	// Dark -> opaque ink, light -> transparent; RGB becomes white so the tip
	// can be tinted with the stroke color.
	const size_t count = static_cast<size_t>(pixels.getWidth()) * pixels.getHeight();
	unsigned char * data = pixels.getData();
	for (size_t i = 0; i < count; ++i) {
		unsigned char * px = data + i * 4;
		const double average = (px[0] + px[1] + px[2]) / 3.0;
		px[0] = px[1] = px[2] = 255;
		px[3] = static_cast<unsigned char>(ofClamp(std::round(255 - average), 0, 255));
	}
}

ofTexture * ofxP5Brush::getTipTexture(const BrushEntry & entry) {
	if (entry.tipKey.empty()) return nullptr;
	const auto found = tipTextures.find(entry.tipKey);
	if (found != tipTextures.end()) return found->second.isAllocated() ? &found->second : nullptr;

	const BrushParams & param = entry.param;
	ofPixels pixels;
	if (param.type == "custom") {
		// Rasterize the tip once into a 500 x 500 buffer. Users draw in a
		// 100 x 100 space with the origin at the centre.
		ofFboSettings settings;
		settings.width = 500;
		settings.height = 500;
		settings.internalformat = GL_RGBA;
		settings.textureTarget = GL_TEXTURE_2D;
		settings.useDepth = false;
		settings.useStencil = false;
		settings.numSamples = 0;
		ofFbo tipFbo;
		tipFbo.allocate(settings);
		tipFbo.begin();
		ofPushStyle();
		ofPushMatrix();
		ofClear(255, 255, 255, 255);
		ofSetRectMode(OF_RECTMODE_CORNER);
		ofEnableAlphaBlending();
		ofFill();
		ofSetColor(255);
		ofTranslate(250, 250);
		ofScale(5, 5);
		TipSurface surface(entry.tipAngleMode);
		param.tip(surface);
		ofPopMatrix();
		ofPopStyle();
		tipFbo.end();
		tipFbo.readToPixels(pixels);
	} else if (param.type == "image") {
		if (!ofLoadImage(pixels, param.image)) {
			ofLogError("ofxP5Brush") << "failed to load image tip \"" << param.image << "\"";
			tipTextures[entry.tipKey] = ofTexture(); // remember the failure
			return nullptr;
		}
	} else {
		return nullptr;
	}

	// Flatten onto white so transparent areas of an image carry no ink.
	pixels.setImageType(OF_IMAGE_COLOR_ALPHA);
	const size_t count = static_cast<size_t>(pixels.getWidth()) * pixels.getHeight();
	unsigned char * data = pixels.getData();
	for (size_t i = 0; i < count; ++i) {
		unsigned char * px = data + i * 4;
		const float a = px[3] / 255.f;
		for (int ch = 0; ch < 3; ++ch) px[ch] = static_cast<unsigned char>(std::round(px[ch] * a + 255 * (1 - a)));
		px[3] = 255;
	}
	imageToWhite(pixels);

	ofTexture & tex = tipTextures[entry.tipKey];
	tex.allocate(pixels.getWidth(), pixels.getHeight(), GL_RGBA, false);
	tex.loadData(pixels);
	tex.setTextureMinMagFilter(GL_LINEAR, GL_LINEAR);
	tex.setTextureWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
	return &tex;
}

// =============================================================================
// TipSurface
// =============================================================================

ofxP5Brush::TipSurface::TipSurface(AngleMode mode)
	: angleMode(mode) { }

void ofxP5Brush::TipSurface::push() {
	styleStack.push_back(style);
	ofPushMatrix();
}

void ofxP5Brush::TipSurface::pop() {
	if (!styleStack.empty()) {
		style = styleStack.back();
		styleStack.pop_back();
	}
	ofPopMatrix();
}

void ofxP5Brush::TipSurface::translate(float x, float y) {
	ofTranslate(x, y);
}

void ofxP5Brush::TipSurface::rotate(float angle) {
	if (angleMode == RADIANS) ofRotateRad(angle);
	else ofRotateDeg(angle);
}

void ofxP5Brush::TipSurface::scale(float s) {
	ofScale(s, s);
}

void ofxP5Brush::TipSurface::scale(float sx, float sy) {
	ofScale(sx, sy);
}

void ofxP5Brush::TipSurface::fill(float gray, float alpha) {
	style.doFill = true;
	style.fillColor = ofColor(gray, alpha);
}

void ofxP5Brush::TipSurface::fill(float r, float g, float b, float a) {
	style.doFill = true;
	style.fillColor = ofColor(r, g, b, a);
}

void ofxP5Brush::TipSurface::fill(const ofColor & c) {
	style.doFill = true;
	style.fillColor = c;
}

void ofxP5Brush::TipSurface::noFill() {
	style.doFill = false;
}

void ofxP5Brush::TipSurface::stroke(float gray, float alpha) {
	style.doStroke = true;
	style.strokeColor = ofColor(gray, alpha);
}

void ofxP5Brush::TipSurface::stroke(float r, float g, float b, float a) {
	style.doStroke = true;
	style.strokeColor = ofColor(r, g, b, a);
}

void ofxP5Brush::TipSurface::stroke(const ofColor & c) {
	style.doStroke = true;
	style.strokeColor = c;
}

void ofxP5Brush::TipSurface::noStroke() {
	style.doStroke = false;
}

void ofxP5Brush::TipSurface::strokeWeight(float weight) {
	style.weight = weight;
}

template <class F>
void ofxP5Brush::TipSurface::paint(F && drawShape) {
	if (style.doFill) {
		ofFill();
		ofSetColor(style.fillColor);
		drawShape();
	}
	if (style.doStroke) {
		ofNoFill();
		ofSetLineWidth(style.weight);
		ofSetColor(style.strokeColor);
		drawShape();
		ofFill();
	}
}

void ofxP5Brush::TipSurface::rect(float x, float y, float w, float h) {
	paint([&] { ofDrawRectangle(x, y, w, h); });
}

void ofxP5Brush::TipSurface::square(float x, float y, float size) {
	rect(x, y, size, size);
}

void ofxP5Brush::TipSurface::circle(float x, float y, float diameter) {
	paint([&] { ofDrawCircle(x, y, diameter / 2); });
}

void ofxP5Brush::TipSurface::ellipse(float x, float y, float w, float h) {
	paint([&] { ofDrawEllipse(x, y, w, h); });
}

void ofxP5Brush::TipSurface::triangle(float x1, float y1, float x2, float y2, float x3, float y3) {
	paint([&] { ofDrawTriangle(x1, y1, x2, y2, x3, y3); });
}

void ofxP5Brush::TipSurface::line(float x1, float y1, float x2, float y2) {
	if (!style.doStroke) return;
	ofSetLineWidth(style.weight);
	ofSetColor(style.strokeColor);
	ofDrawLine(x1, y1, x2, y2);
}

void ofxP5Brush::TipSurface::beginShape() {
	shape.clear();
}

void ofxP5Brush::TipSurface::vertex(float x, float y) {
	shape.emplace_back(x, y);
}

void ofxP5Brush::TipSurface::endShape(bool close) {
	paint([&] {
		ofBeginShape();
		for (const auto & v : shape) ofVertex(v.x, v.y);
		ofEndShape(close);
	});
	shape.clear();
}
