#include "ofxP5BrushUtils.h"

#include <cstdio>
#include <cstdlib>
#include <utility>

namespace ofxP5BrushDetail {

// =============================================================================
// Number formatting (JavaScript Number.prototype.toString semantics)
// =============================================================================

std::string jsNumberToString(double value) {
	if (std::isnan(value)) return "NaN";
	if (std::isinf(value)) return value < 0 ? "-Infinity" : "Infinity";
	if (value == 0) return "0";

	const std::string sign = value < 0 ? "-" : "";
	const double a = std::fabs(value);

	// Shortest scientific representation that round-trips.
	char buf[64];
	for (int precision = 0; precision <= 17; ++precision) {
		std::snprintf(buf, sizeof(buf), "%.*e", precision, a);
		if (std::strtod(buf, nullptr) == a) break;
	}
	std::string sci(buf);
	const auto ePos = sci.find('e');
	const std::string mantissa = sci.substr(0, ePos);
	const int exponent = std::atoi(sci.c_str() + ePos + 1);

	std::string digits;
	for (char ch : mantissa) {
		if (ch != '.') digits += ch;
	}
	while (digits.size() > 1 && digits.back() == '0') digits.pop_back();

	const int k = static_cast<int>(digits.size());
	const int n = exponent + 1;
	std::string out;
	if (k <= n && n <= 21) {
		out = digits + std::string(n - k, '0');
	} else if (0 < n && n <= 21) {
		out = digits.substr(0, n) + "." + digits.substr(n);
	} else if (-6 < n && n <= 0) {
		out = "0." + std::string(-n, '0') + digits;
	} else {
		const int e = n - 1;
		const std::string expStr = (e >= 0 ? "+" : "-") + std::to_string(std::abs(e));
		out = k == 1 ? digits + "e" + expStr : digits.substr(0, 1) + "." + digits.substr(1) + "e" + expStr;
	}
	return sign + out;
}

// =============================================================================
// Seeded randomness: Mulberry32 with a SplitMix-style seed hash
// =============================================================================

uint32_t hashSeed(const std::string & seed) {
	uint32_t h = 0;
	for (unsigned char ch : seed) {
		h = (h ^ ch) * 0x9e3779b9u;
		h ^= h >> 15;
	}
	h = (h ^ (h >> 16)) * 0x85ebca6bu;
	h = (h ^ (h >> 13)) * 0xc2b2ae35u;
	h = h ^ (h >> 16);
	return h ? h : 1u;
}

Rng::Rng() {
	seed("0");
}

Rng::Rng(const std::string & s) {
	seed(s);
}

void Rng::seed(const std::string & s) {
	state = hashSeed(s);
}

double Rng::operator()() {
	state += 0x6D2B79F5u;
	uint32_t t = (state ^ (state >> 15)) * (state | 1u);
	t ^= t + (t ^ (t >> 7)) * (t | 61u);
	return static_cast<double>(t ^ (t >> 14)) * 2.3283064365386963e-10;
}

// =============================================================================
// Simplex noise (port of simplex-noise v4, createNoise2D)
// =============================================================================

namespace {
const double SQRT3 = std::sqrt(3.0);
const double F2 = 0.5 * (SQRT3 - 1.0);
const double G2 = (3.0 - SQRT3) / 6.0;
const double GRAD2[24] = {1, 1, -1, 1, 1, -1, -1, -1, 1, 0, -1, 0, 1, 0, -1, 0, 0, 1, 0, -1, 0, 1, 0, -1};

inline int fastFloor(double x) {
	return static_cast<int>(std::floor(x));
}
} // namespace

SimplexNoise2D::SimplexNoise2D() {
	Rng r("0");
	*this = SimplexNoise2D(r);
}

SimplexNoise2D::SimplexNoise2D(Rng & random) {
	for (int i = 0; i < 256; ++i) perm[i] = static_cast<uint8_t>(i);
	for (int i = 0; i < 255; ++i) {
		const int r = i + toInt32(random() * (256 - i));
		std::swap(perm[i], perm[r]);
	}
	for (int i = 256; i < 512; ++i) perm[i] = perm[i - 256];
	for (int i = 0; i < 512; ++i) {
		gradX[i] = GRAD2[(perm[i] % 12) * 2];
		gradY[i] = GRAD2[(perm[i] % 12) * 2 + 1];
	}
}

double SimplexNoise2D::operator()(double x, double y) const {
	double n0 = 0, n1 = 0, n2 = 0;
	const double s = (x + y) * F2;
	const int i = fastFloor(x + s);
	const int j = fastFloor(y + s);
	const double t = (i + j) * G2;
	const double X0 = i - t;
	const double Y0 = j - t;
	const double x0 = x - X0;
	const double y0 = y - Y0;
	int i1, j1;
	if (x0 > y0) {
		i1 = 1;
		j1 = 0;
	} else {
		i1 = 0;
		j1 = 1;
	}
	const double x1 = x0 - i1 + G2;
	const double y1 = y0 - j1 + G2;
	const double x2 = x0 - 1.0 + 2.0 * G2;
	const double y2 = y0 - 1.0 + 2.0 * G2;
	const int ii = i & 255;
	const int jj = j & 255;
	double t0 = 0.5 - x0 * x0 - y0 * y0;
	if (t0 >= 0) {
		const int gi0 = ii + perm[jj];
		t0 *= t0;
		n0 = t0 * t0 * (gradX[gi0] * x0 + gradY[gi0] * y0);
	}
	double t1 = 0.5 - x1 * x1 - y1 * y1;
	if (t1 >= 0) {
		const int gi1 = ii + i1 + perm[jj + j1];
		t1 *= t1;
		n1 = t1 * t1 * (gradX[gi1] * x1 + gradY[gi1] * y1);
	}
	double t2 = 0.5 - x2 * x2 - y2 * y2;
	if (t2 >= 0) {
		const int gi2 = ii + 1 + perm[jj + 1];
		t2 *= t2;
		n2 = t2 * t2 * (gradX[gi2] * x2 + gradY[gi2] * y2);
	}
	return 70.0 * (n0 + n1 + n2);
}

// =============================================================================
// Trigonometry lookup tables (degrees, 4 samples per degree)
// =============================================================================

namespace {
const int TOTAL_DEGREES = 1440;

struct TrigTables {
	float c[TOTAL_DEGREES];
	float s[TOTAL_DEGREES];
	TrigTables() {
		const double radiansPerIndex = (2.0 * 3.14159265358979323846) / TOTAL_DEGREES;
		for (int i = 0; i < TOTAL_DEGREES; ++i) {
			c[i] = static_cast<float>(std::cos(i * radiansPerIndex));
			s[i] = static_cast<float>(std::sin(i * radiansPerIndex));
		}
	}
};

const TrigTables & tables() {
	static const TrigTables t;
	return t;
}

inline int wrapIdx(int idx) {
	return idx >= TOTAL_DEGREES ? idx - TOTAL_DEGREES : idx;
}

int angleToIdx(double angle) {
	if (!std::isfinite(angle)) return 0;
	if (angle < 0) {
		if (angle >= -360) return wrapIdx(static_cast<int>((angle + 360) * 4));
		angle = std::fmod(angle, 360.0);
		return wrapIdx(static_cast<int>((angle < 0 ? angle + 360 : angle) * 4));
	}
	if (angle < 360) return static_cast<int>(angle * 4);
	if (angle < 720) return static_cast<int>((angle - 360) * 4);
	if (angle < 1080) return static_cast<int>((angle - 720) * 4);
	angle = std::fmod(angle, 360.0);
	return wrapIdx(static_cast<int>((angle < 0 ? angle + 360 : angle) * 4));
}
} // namespace

double cosDeg(double angle) {
	return tables().c[angleToIdx(angle)];
}

double sinDeg(double angle) {
	return tables().s[angleToIdx(angle)];
}

void cosSinDeg(double angle, double & c, double & s) {
	const int idx = angleToIdx(angle);
	c = tables().c[idx];
	s = tables().s[idx];
}

int toInt32(double value) {
	if (!std::isfinite(value)) return 0;
	const double t = std::trunc(value);
	if (t >= -2147483648.0 && t < 2147483648.0) return static_cast<int>(t);
	double m = std::fmod(t, 4294967296.0);
	if (m < 0) m += 4294967296.0;
	return static_cast<int>(static_cast<uint32_t>(static_cast<uint64_t>(m)));
}

} // namespace ofxP5BrushDetail
