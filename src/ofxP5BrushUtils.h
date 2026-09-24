#pragma once

// Internal helpers for ofxP5Brush: seeded randomness, simplex noise, the
// degree-based trig lookup tables and small numeric utilities. These mirror
// src/core/utils.js from p5.brush so that seeds reproduce the same geometry.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

namespace ofxP5BrushDetail {

/// Formats a number the way JavaScript's String(number) does, so numeric
/// seeds hash identically to p5.brush (e.g. 42 -> "42", 0.5 -> "0.5").
std::string jsNumberToString(double value);

/// Maps any seed string to a non-zero uint32 (SplitMix64-style finalizer).
uint32_t hashSeed(const std::string & seed);

/// Mulberry32 PRNG returning uniform doubles in [0, 1).
class Rng {
public:
	Rng();
	explicit Rng(const std::string & seed);
	void seed(const std::string & seed);
	double operator()();

private:
	uint32_t state = 1;
};

/// 2D simplex noise, a port of simplex-noise v4 `createNoise2D`.
class SimplexNoise2D {
public:
	SimplexNoise2D();
	explicit SimplexNoise2D(Rng & random);
	double operator()(double x, double y) const;

private:
	std::array<uint8_t, 512> perm {};
	std::array<double, 512> gradX {};
	std::array<double, 512> gradY {};
};

/// Cosine / sine of an angle in degrees, via a 1440-entry lookup table.
double cosDeg(double angle);
double sinDeg(double angle);
void cosSinDeg(double angle, double & c, double & s);

/// JavaScript's `~~value` (ToInt32 with truncation), NaN-safe.
int toInt32(double value);

inline double constrain(double n, double low, double high) {
	return std::max(std::min(n, high), low);
}

inline double mapRange(double value, double a, double b, double c, double d, bool withinBounds = false) {
	double r = c + ((value - a) / (b - a)) * (d - c);
	if (!withinBounds) return r;
	return c < d ? constrain(r, c, d) : constrain(r, d, c);
}

inline double dist(double x1, double y1, double x2, double y2) {
	return std::hypot(x2 - x1, y2 - y1);
}

/// JavaScript's `%` operator (sign follows the dividend).
inline double jsMod(double a, double b) {
	return std::fmod(a, b);
}

} // namespace ofxP5BrushDetail
