// Vector fields and the Position class (port of p5.brush core/flowfield.js).

#include "ofxP5Brush.h"

using namespace ofxP5BrushDetail;

namespace {
/// JavaScript's Math.round (halves round towards +infinity).
inline double jsRound(double v) {
	return std::floor(v + 0.5);
}
} // namespace

// =============================================================================
// Field setup
// =============================================================================

void ofxP5Brush::isFieldReady() {
	if (fieldLoaded) return;
	isCanvasReady();
	// A grid twice the canvas size, centred on it, one cell per 1% of width.
	fResolution = Cwidth * 0.01;
	fLeftX = -0.5 * Cwidth;
	fTopY = -0.5 * Cheight;
	fNumColumns = static_cast<int>(jsRound((2.0 * Cwidth) / fResolution));
	fNumRows = static_cast<int>(jsRound((2.0 * Cheight) / fResolution));
	fieldLoaded = true;
}

ofxP5Brush::Field ofxP5Brush::genField() const {
	return Field(fNumColumns, std::vector<float>(fNumRows, 0.f));
}

ofxP5Brush::Field ofxP5Brush::generateField(FieldEntry & entry, double t) {
	Field f = genField();
	if (entry.gen) entry.gen(t, f);
	if (entry.angleMode == RADIANS) {
		for (auto & column : f) {
			for (auto & v : column) v = static_cast<float>(toDegreesSigned(v, true));
		}
	}
	return f;
}

const ofxP5Brush::Field * ofxP5Brush::flowField() const {
	const auto it = fields.find(fieldState.current);
	if (it == fields.end() || !it->second.field) return nullptr;
	return &*it->second.field;
}

void ofxP5Brush::refreshField(double t) {
	if (!fieldState.isActive || fieldState.current.empty()) {
		ofLogError("ofxP5Brush") << "refreshField(): no field is active. Call field(\"name\") first.";
		return;
	}
	auto & entry = fields[fieldState.current];
	entry.field = generateField(entry, t);
}

void ofxP5Brush::field(const std::string & name) {
	if (fieldState.wiggle == 0) fieldState.wiggle = 1;
	isFieldReady();
	const auto it = fields.find(name);
	if (it == fields.end()) {
		std::string available;
		for (const auto & n : fieldOrder) {
			if (!available.empty()) available += ", ";
			available += n;
		}
		ofLogError("ofxP5Brush") << "field \"" << name << "\" does not exist. Available fields: " << available;
		return;
	}
	fieldState.isActive = true;
	fieldState.current = name;
	if (!it->second.field) it->second.field = generateField(it->second, 0);
}

void ofxP5Brush::noField() {
	isFieldReady();
	fieldState.isActive = false;
}

void ofxP5Brush::addField(const std::string & name, FieldGenerator generator, AngleMode mode) {
	if (fields.find(name) == fields.end()) fieldOrder.push_back(name);
	FieldEntry entry;
	entry.gen = std::move(generator);
	entry.angleMode = mode;
	fields[name] = std::move(entry);
}

std::vector<std::string> ofxP5Brush::listFields() {
	isFieldReady();
	return fieldOrder;
}

void ofxP5Brush::wiggle(double amount) {
	field("hand");
	fieldState.wiggle = amount;
}

void ofxP5Brush::addStandardFields() {
	auto fillField = [this](Field & f, const std::function<double(int, int)> & fn) {
		for (int c = 0; c < fNumColumns; ++c) {
			for (int r = 0; r < fNumRows; ++r) f[c][r] = static_cast<float>(fn(c, r));
		}
	};

	// Organic noise — basis for wiggle()
	addField("hand", [this, fillField](double t, Field & f) {
		const double bs = rr2(0.2, 0.8);
		const int ba = randInt2(5, 10);
		fillField(f, [&](int c, int r) {
			const double angle = 0.5 * ba * sinDeg(bs * r * c + randInt2(15, 25));
			return 0.2 * angle * cosDeg(t) + noiseGen2(c, r) * ba * 0.7;
		});
	});

	// Smooth large-scale noise curves
	addField("curved", [this, fillField](double t, Field & f) {
		int ar = randInt2(-10, 10);
		if (randInt2(0, 100) % 2 == 0) ar *= -1;
		fillField(f, [&](int c, int r) {
			return 3 * mapRange(noiseGen2(c * 0.02 + t * 0.03, r * 0.02 + t * 0.03), 0, 1, -ar, ar);
		});
	});

	// Sharp alternating angles per cell — herringbone / wicker look
	addField("zigzag", [this](double t, Field & f) {
		double ar = randInt2(-30, -15) + std::abs(44 * sinDeg(t));
		if (randInt2(0, 100) % 2 == 0) ar *= -1;
		double dif = ar;
		double angle = 0;
		for (int c = 0; c < fNumColumns; ++c) {
			for (int r = 0; r < fNumRows; ++r) {
				f[c][r] = static_cast<float>(angle);
				angle += dif;
				dif *= -1;
			}
			angle += dif;
			dif *= -1;
		}
	});

	// Sinusoidal wave bands
	addField("waves", [this, fillField](double t, Field & f) {
		const double sr = randInt2(10, 15) + 5 * sinDeg(t);
		const double cr = randInt2(3, 6) + 3 * cosDeg(t);
		const int ba = randInt2(20, 35);
		fillField(f, [&](int c, int r) { return sinDeg(sr * c) * ba * cosDeg(r * cr) + randInt2(-3, 3); });
	});

	// Dense oscillation from row x column product
	addField("seabed", [this, fillField](double t, Field & f) {
		const double bs = rr2(0.4, 0.8);
		const int ba = randInt2(18, 26);
		fillField(f, [&](int c, int r) { return 1.1 * ba * sinDeg(bs * r * c + randInt2(15, 20)) * cosDeg(t); });
	});

	// Radial vortex — angles spiral around a few attractors
	addField("spiral", [this, fillField](double, Field & f) {
		const int n = randInt2(5, 10);
		const int dir = randInt2(0, 2) * 2 - 1;
		const int offset = randInt2(65, 80); // < 90 = inward spiral
		std::vector<glm::dvec2> attractors;
		for (int i = 0; i < n; ++i) {
			const double ax = rr2(0.1, 0.9) * fNumColumns;
			const double ay = rr2(0.1, 0.9) * fNumRows;
			attractors.emplace_back(ax, ay);
		}
		fillField(f, [&](int c, int r) {
			double wx = 0, wy = 0;
			for (const auto & att : attractors) {
				const double dx = c - att.x;
				const double dy = r - att.y;
				const double w = 1 / (dx * dx + dy * dy + 1);
				const double a = std::atan2(dy, dx) * (180 / PI);
				const double angle = (dir * (a + offset) * PI) / 180;
				wx += w * std::cos(angle);
				wy += w * std::sin(angle);
			}
			return std::atan2(wy, wx) * (180 / PI);
		});
	});

	// Column-banded stripes — parallel rake marks
	addField("columns", [this, fillField](double, Field & f) {
		const int freq = randInt2(3, 8);
		const int amp = randInt2(25, 45);
		fillField(f, [&](int c, int) { return sinDeg(c * freq) * amp; });
	});
}

// =============================================================================
// Position
// =============================================================================

ofxP5Brush::Position::Position(double x_, double y_, ofxP5Brush * b)
	: brush(b ? b : ofxP5Brush::currentInstance) {
	if (!brush) {
		ofLogError("ofxP5Brush") << "Position needs a brush (pass one, or set up an ofxP5Brush first)";
		x = x_;
		y = y_;
		return;
	}
	brush->isFieldReady();
	halfW = brush->Cwidth / 2;
	halfH = brush->Cheight / 2;
	// p5.brush expresses the translation relative to the canvas centre.
	const Affine m = brush->getAffineMatrix();
	mx = m.x - halfW;
	my = m.y - halfH;
	update(x_, y_);
	plotted = 0;
}

void ofxP5Brush::Position::update(double x_, double y_) {
	setPositionSpace(x_ + halfW, y_ + halfH);
}

void ofxP5Brush::Position::setPositionSpace(double px_, double py_) {
	px = px_;
	py = py_;
	x = px - halfW;
	y = py - halfH;
	if (brush && brush->fieldState.isActive) {
		colIdx = static_cast<int>(jsRound((px + mx - brush->fLeftX) / brush->fResolution));
		rowIdx = static_cast<int>(jsRound((py + my - brush->fTopY) / brush->fResolution));
	}
}

bool ofxP5Brush::Position::isIn() const {
	if (!brush) return false;
	if (brush->fieldState.isActive) {
		return colIdx >= 0 && rowIdx >= 0 && colIdx < brush->fNumColumns && rowIdx < brush->fNumRows;
	}
	return isInCanvas();
}

bool ofxP5Brush::Position::isInCanvas() const {
	if (!brush) return false;
	const double margin = 0.5;
	const double w = brush->Cwidth;
	const double h = brush->Cheight;
	const double cx = px + mx;
	const double cy = py + my;
	return cx >= -margin * w && cx <= (1 + margin) * w && cy >= -margin * h && cy <= (1 + margin) * h;
}

double ofxP5Brush::Position::angle(bool skipCheck) const {
	if (!brush || !brush->fieldState.isActive) return 0;
	if (!skipCheck && !isIn()) return 0;
	const Field * f = brush->flowField();
	if (!f || colIdx < 0 || colIdx >= static_cast<int>(f->size())) return 0;
	const auto & column = (*f)[colIdx];
	if (rowIdx < 0 || rowIdx >= static_cast<int>(column.size())) return 0;
	return column[rowIdx] * brush->fieldState.wiggle;
}

void ofxP5Brush::Position::moveTo(double dir, double length, double stepLength) {
	if (!brush) return;
	moveToDegrees(brush->toDegreesSigned(dir), length, stepLength);
}

void ofxP5Brush::Position::moveToDegrees(double dir, double length, double step) {
	if (brush && brush->fieldState.isActive) {
		movePos(nullptr, dir, length, step, 0, false, 0);
	} else {
		moveConstant(dir, length, step);
	}
}

void ofxP5Brush::Position::moveConstant(double dir, double length, double step) {
	if (!isIn()) {
		plotted += step;
		return;
	}
	const double steps = length / step;
	double c, s;
	cosSinDeg(-dir, c, s);
	const double dx = step * c;
	const double dy = step * s;
	for (int i = 0; i < steps; ++i) {
		px += dx;
		py += dy;
		plotted += step;
	}
	x = px - halfW;
	y = py - halfH;
}

void ofxP5Brush::Position::plotTo(Plot & plot, double length, double stepLength, double scale) {
	movePos(&plot, 0, length, stepLength, scale == 0 ? 1 : scale, false, 0);
}

void ofxP5Brush::Position::movePos(Plot * plot, double dir, double length, double step, double scale,
	bool usePrecomputed, double precomputedAngle) {
	const double scaleFactor = scale != 0 ? scale : 1;
	if (!isIn()) {
		plotted += step / scaleFactor;
		return;
	}
	const double steps = length / step;
	const bool fieldActive = brush && brush->fieldState.isActive;
	const bool usePlot = scale != 0 && plot;
	for (int i = 0; i < steps; ++i) {
		const double plotAngle = usePlot && usePrecomputed && i == 0 ? precomputedAngle
			: usePlot                                                ? plot->angle(plotted)
																	 : dir;
		const double a = (fieldActive ? angle(true) : 0) - plotAngle;
		double c, s;
		cosSinDeg(a, c, s);
		setPositionSpace(px + step * c, py + step * s);
		plotted += step / scaleFactor;
	}
}
