#pragma once

// ofxP5Brush
// An openFrameworks port of p5.brush by Alejandro Campos Uribe
// (https://github.com/acamposuribe/p5.brush, MIT License).
//
// Natural drawing tools for generative art: pencils, charcoal, markers,
// custom and image tips, watercolor fills, hatching, massing and vector fields
// that bend strokes organically. Strokes and fills are composited onto the
// canvas with spectral (Kubelka-Munk) pigment mixing.
//
// The API mirrors the JavaScript library:
//
//     ofxP5Brush brush;                         // in ofApp.h
//     brush.setup(600, 600);                    // in ofApp::setup()
//     brush.scaleBrushes(3);
//     brush.set("HB", "#2f2a26", 1.4);
//     brush.line(80, 120, 480, 240);
//     brush.fill("#d7c3a3", 120);
//     brush.circle(300, 300, 70);
//     brush.draw(0, 0);                         // in ofApp::draw()
//
// Coordinates follow openFrameworks: the origin is the top-left corner of the
// canvas and the current OF transform (ofTranslate / ofRotate / ofScale) is
// applied to every brush call.

#include "ofMain.h"

#include "ofxP5BrushRaster.h"
#include "ofxP5BrushUtils.h"

#include <array>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

/// Options for ofxP5Brush::hatch(), e.g. brush.hatch(8, 30, {0.1, true}).
struct ofxP5BrushHatchOptions {
	double rand = 0; ///< 0-1, randomness in line placement
	bool continuous = false; ///< connect each line to the next
	double gradient = 0; ///< 0-1, spacing growth between lines
};

/// Options for ofxP5Brush::mass().
struct ofxP5BrushMassOptions {
	double precision = 0.5; ///< 0-1, higher = less jitter
	double strength = 1; ///< 0-1, how many of the three layers are drawn
	double gradient = 0.1; ///< 0-1, spacing variation
	bool outline = false; ///< also outline the first layer
};

class ofxP5Brush {
public:
	enum AngleMode {
		RADIANS,
		DEGREES
	};

	// =========================================================================
	// Value types
	// =========================================================================

	/// A color, accepted anywhere p5.brush accepts a color. Converts implicitly
	/// from ofColor / ofFloatColor, from CSS strings ("#2f2a26", "#abc",
	/// "red", "rgb(10, 20, 30)", "rgba(10, 20, 30, 0.5)") and from 0-255
	/// component lists ({r, g, b} or {r, g, b, a}).
	struct Color {
		float r = 0.f, g = 0.f, b = 0.f, a = 1.f; // normalized sRGB

		Color() = default;
		Color(const ofColor & c);
		Color(const ofFloatColor & c);
		Color(const std::string & css);
		Color(const char * css);
		Color(float r255, float g255, float b255, float a255 = 255.f);

		/// Grayscale value (0-255) with optional alpha (0-255), like p5's color(v, a).
		static Color gray(float v255, float a255 = 255.f);

		/// Parses a CSS color string. Returns false (leaving `out` untouched) on failure.
		static bool parse(const std::string & css, Color & out);

		ofFloatColor toOf() const { return ofFloatColor(r, g, b, a); }
		bool operator==(const Color & o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }
		bool operator!=(const Color & o) const { return !(*this == o); }
	};

	/// How a brush's size changes along a stroke.
	///  - `{start, end}` or `{start, mid, end}`: piecewise-linear ramp
	///  - a callable `double(double t)` with t in [0, 1]: custom curve
	///  - `Pressure::gaussian({a, b}, {min, max})`: the built-in brushes' envelope
	/// Simple and custom modes add a subtle per-stroke variation automatically.
	struct Pressure {
		enum Mode {
			GAUSSIAN,
			CUSTOM
		};
		struct Variation {
			double offset = 0.08, scale = 0.08, warp = 0.06, tilt = 0.06;
		};

		Mode mode = GAUSSIAN;
		glm::dvec2 curve = {0.15, 0.2}; ///< gaussian envelope shape
		glm::dvec2 minMax = {1.1, 0.9}; ///< mapped pressure range
		std::function<double(double)> fn; ///< custom curve, t in [0,1] -> [0,1]
		Variation variation;

		Pressure() = default;
		Pressure(std::initializer_list<double> ramp);
		Pressure(const std::vector<double> & ramp);
		template <class F,
			class = std::enable_if_t<std::is_invocable_r_v<double, F, double> && !std::is_same_v<std::decay_t<F>, Pressure>>>
		Pressure(F && curveFn)
			: mode(CUSTOM)
			, minMax(0.0, 1.0)
			, fn(std::forward<F>(curveFn)) { }

		static Pressure gaussian(glm::dvec2 curve, glm::dvec2 minMax);
		static Pressure custom(std::function<double(double)> curveFn, glm::dvec2 minMax = {0.0, 1.0});
	};

	/// Drawing surface handed to custom tip functions. It mirrors the subset of
	/// p5's API that p5.brush documents for tips. The coordinate space is
	/// 100 x 100 units with the origin at the centre. Dark marks become ink,
	/// light / white stays transparent. Any openFrameworks drawing call also
	/// works inside the tip function, since the tip buffer is bound.
	class TipSurface {
	public:
		explicit TipSurface(AngleMode angleMode = RADIANS);

		void push();
		void pop();
		void translate(float x, float y);
		void rotate(float angle); ///< follows the brush's angleMode()
		void scale(float s);
		void scale(float sx, float sy);

		void fill(float gray, float alpha = 255.f);
		void fill(float r, float g, float b, float a = 255.f);
		void fill(const ofColor & c);
		void noFill();
		void stroke(float gray, float alpha = 255.f);
		void stroke(float r, float g, float b, float a = 255.f);
		void stroke(const ofColor & c);
		void noStroke();
		void strokeWeight(float weight);

		void rect(float x, float y, float w, float h);
		void square(float x, float y, float size);
		void circle(float x, float y, float diameter);
		void ellipse(float x, float y, float w, float h);
		void triangle(float x1, float y1, float x2, float y2, float x3, float y3);
		void line(float x1, float y1, float x2, float y2);
		void beginShape();
		void vertex(float x, float y);
		void endShape(bool close = false);

	private:
		struct Style {
			bool doFill = true;
			ofColor fillColor = ofColor(255);
			bool doStroke = false;
			ofColor strokeColor = ofColor(0);
			float weight = 1.f;
		};
		template <class F>
		void paint(F && drawShape);

		AngleMode angleMode;
		Style style;
		std::vector<Style> styleStack;
		std::vector<glm::vec2> shape;
	};

	/// Brush definition for add(). Defaults match the "HB" pencil.
	struct BrushParams {
		std::string type = "default"; ///< "default" | "spray" | "marker" | "custom" | "image"
		double weight = 0.3; ///< base size, in canvas units
		double scatter = 0.6; ///< sideways wobble, in canvas units
		double sharpness = 0.3; ///< 0-1, edge definition ("default" type)
		double grain = 0.7; ///< texture density ("default" and "spray" types)
		double opacity = 170; ///< 0-255
		double spacing = 0.1; ///< distance between tip stamps
		Pressure pressure = Pressure::gaussian({0.15, 0.2}, {1.1, 0.9});
		std::function<void(TipSurface &)> tip; ///< "custom" type tip shape
		std::string image; ///< "image" type tip file (relative to bin/data)
		std::string rotate = "none"; ///< "none" | "natural" | "random"
		bool markerTip = true; ///< soft build-up at stroke ends (marker/custom/image)
		double noise = 0.3; ///< 0-1, per-stroke opacity variation
	};

	using HatchOptions = ofxP5BrushHatchOptions;
	using MassOptions = ofxP5BrushMassOptions;

	/// A vector field grid of angles: field[column][row].
	using Field = std::vector<std::vector<float>>;
	using FieldGenerator = std::function<void(double t, Field & field)>;

	// =========================================================================
	// Geometry classes (brush.Polygon, brush.Plot, brush.Position)
	// =========================================================================

	class Plot;

	/// A polygon. Its drawing methods use the brush that created it, or the
	/// most recently set-up brush when it was constructed without one.
	class Polygon {
	public:
		Polygon() = default;
		Polygon(const std::vector<glm::vec2> & points, ofxP5Brush * brush = nullptr);

		std::vector<glm::dvec2> vertices;
		ofxP5Brush * brush = nullptr;

		std::vector<std::pair<glm::dvec2, glm::dvec2>> getSides() const;
		/// Intersection points between the line through p1-p2 and the polygon edges.
		std::vector<glm::dvec2> intersect(const glm::dvec2 & p1, const glm::dvec2 & p2) const;

		/// Outline with the current stroke state, or with the given brush settings.
		Polygon & draw();
		Polygon & draw(const std::string & brushName, const Color & color, double weight = 1);
		/// Watercolor fill with the current fill state, or with the given settings
		/// (unspecified optional settings keep their current values).
		Polygon & fill();
		Polygon & fill(const Color & color, double opacity = 150, std::optional<double> bleed = std::nullopt,
			std::optional<double> texture = std::nullopt, std::optional<double> border = std::nullopt,
			std::optional<std::string> direction = std::nullopt, std::optional<double> angle = std::nullopt);
		/// Flat wash with the current wash state or the given color and opacity.
		Polygon & wash();
		Polygon & wash(const Color & color, double opacity = 150);
		/// Hatch with the current hatch state or the given parameters.
		Polygon & hatch();
		Polygon & hatch(double dist, double angle, const HatchOptions & options = {});
		/// Mass with the current mass state.
		Polygon & mass();
		/// Wash, fill, mass, hatch and stroke according to the current state.
		Polygon & show();

	private:
		friend class ofxP5Brush;
		struct Raw { };
		Polygon(Raw, std::vector<glm::dvec2> points, ofxP5Brush * brush);
		ofxP5Brush * owner() const;
	};

	/// A path made of segments (angle, length, pressure).
	class Plot {
	public:
		enum Type {
			CURVE,
			SEGMENTS
		};

		explicit Plot(Type type = CURVE, ofxP5Brush * brush = nullptr);

		void addSegment(double angle = 0, double length = 0, double pressure = 1, bool degrees = false);
		void endPlot(double angle = 0, double pressure = 1, bool degrees = false);
		void rotate(double angle);
		double pressure(double d);
		double angle(double d);
		int calcIndex(double d);
		/// Traces the plot from (x, y) through the active vector field.
		Polygon genPol(double x, double y, double scale = 1, double side = 0);

		Plot & draw(double x = 0, double y = 0, double scale = 1);
		Plot & fill(double x = 0, double y = 0, double scale = 1);
		Plot & wash(double x = 0, double y = 0, double scale = 1);
		Plot & hatch(double x = 0, double y = 0, double scale = 1);
		Plot & mass(double x = 0, double y = 0, double scale = 1);
		Plot & show(double x = 0, double y = 0, double scale = 1);

		Type type = CURVE;
		std::vector<double> segments;
		std::vector<double> angles;
		std::vector<double> pres;
		double dir = 0;
		double length = 0;
		/// Set by spline(), endShape() and friends: rendering then ignores the
		/// x / y / scale arguments and uses this origin.
		bool hasOrigin = false;
		glm::dvec2 origin {0, 0};
		/// The polygon generated by the last fill / hatch / wash call.
		Polygon pol;
		ofxP5Brush * brush = nullptr;

	private:
		friend class ofxP5Brush;
		ofxP5Brush * owner() const;
		std::vector<double> cumLen;
		int index = 0;
		double suma = 0;
	};

	/// A point that can travel through the active vector field.
	class Position {
	public:
		Position() = default;
		Position(double x, double y, ofxP5Brush * brush = nullptr);

		void update(double x, double y);
		void reset() { plotted = 0; }
		bool isIn() const;
		bool isInCanvas() const;
		/// Vector-field angle at this position, in degrees.
		double angle(bool skipCheck = false) const;
		/// Moves along the field; dir follows the brush's angleMode().
		void moveTo(double dir, double length, double stepLength = 1);
		/// Moves along a plot through the field.
		void plotTo(Plot & plot, double length, double stepLength, double scale = 1);

		/// Current position in brush coordinates (read-only: move with update(),
		/// moveTo() or plotTo()).
		double x = 0, y = 0;
		/// Distance travelled since construction or reset().
		double plotted = 0;
		ofxP5Brush * brush = nullptr;

	private:
		friend class ofxP5Brush;
		void setPositionSpace(double px, double py);
		void moveToDegrees(double dir, double length, double step);
		void moveConstant(double dir, double length, double step);
		void movePos(Plot * plot, double dir, double length, double step, double scale, bool usePrecomputed,
			double precomputedAngle);
		// p5.brush tracks positions offset by half the canvas ("position
		// space"). Doing the same keeps the floating-point arithmetic, and so
		// the seeded output, identical to the JavaScript library.
		double px = 0, py = 0;
		double halfW = 0, halfH = 0;
		double mx = 0, my = 0; // translation, in p5.brush's centre-origin convention
		int colIdx = 0, rowIdx = 0;
	};

	// =========================================================================
	// Lifecycle and canvas
	// =========================================================================

	ofxP5Brush();
	~ofxP5Brush();
	ofxP5Brush(const ofxP5Brush &) = delete;
	ofxP5Brush & operator=(const ofxP5Brush &) = delete;

	/// Creates the brush canvas (an ofFbo) and makes it the draw target.
	/// `density` multiplies the backing resolution (like p5's pixelDensity):
	/// drawing coordinates stay in width x height units. If you never call
	/// setup(), the first drawing call creates a canvas the size of the window.
	void setup(int width, int height, float density = 1.f);
	/// Alias for setup(), matching the standalone build's brush.createCanvas().
	void createCanvas(int width, int height, float density = 1.f) { setup(width, height, density); }
	/// Draws into your own fbo instead (GL_RGBA, no multisampling). Pending
	/// strokes are flushed into the previous target first.
	void load(ofFbo & fbo, float density = 1.f);
	/// Switches back to the brush's own canvas.
	void load();
	bool isSetup() const { return ready; }

	/// Flushes pending stroke / fill compositing into the target. Called for
	/// you by draw(), begin(), and automatically after ofApp::draw().
	void render();
	/// Clears the target to transparent, discarding pending strokes.
	void clear();
	/// Clears the target to an opaque color, discarding pending strokes.
	void clear(const Color & color);
	void background(const Color & color) { clear(color); }
	void background(double gray) { clear(Color::gray(gray)); }
	void background(double r, double g, double b) { clear(Color(r, g, b)); }

	/// Renders pending work and draws the target at its logical size.
	void draw(float x = 0, float y = 0);
	void draw(float x, float y, float w, float h);
	/// Binds the target for native openFrameworks drawing, in brush coordinates.
	void begin();
	void end();

	/// The brush's own canvas. Call render() before reading it directly.
	ofFbo & getCanvas() { return canvasFbo; }
	/// The active target (the canvas, or the fbo passed to load()).
	ofFbo & getTarget();
	float getWidth() const { return Cwidth; }
	float getHeight() const { return Cheight; }
	float getDensity() const { return Density; }

	/// Saves / restores the brush state (stroke, fill, wash, hatch, mass, field)
	/// together with the openFrameworks matrix (ofPushMatrix / ofPopMatrix).
	void push();
	void pop();
	void translate(float x, float y) { ofTranslate(x, y); }
	void rotate(double angle); ///< follows angleMode()
	void scale(float s) { ofScale(s, s); }
	void scale(float sx, float sy) { ofScale(sx, sy); }

	/// Angle units for every brush API that takes an angle. Default: RADIANS.
	void angleMode(AngleMode mode) { angleModeValue = mode; }
	AngleMode getAngleMode() const { return angleModeValue; }

	/// Seeds the brush's random generator. Same seed, same drawing.
	void seed(double s);
	void seed(const std::string & s);
	/// Seeds the brush's noise generator.
	void noiseSeed(double s);
	void noiseSeed(const std::string & s);

	/// The brush's own seeded random helpers (the standalone build's
	/// brush.random() / brush.noise()).
	double random();
	double random(double max);
	double random(double min, double max);
	template <class T>
	const T & random(const std::vector<T> & values) {
		return values[ofxP5BrushDetail::toInt32(rng() * values.size())];
	}
	double noise(double x, double y) { return noiseGen(x, y); }
	/// Picks a key with probability proportional to its weight.
	template <class K>
	K wRand(const std::vector<std::pair<K, double>> & weights);

	// =========================================================================
	// Vector fields
	// =========================================================================

	/// Activates a field: "hand", "curved", "zigzag", "waves", "seabed",
	/// "spiral", "columns" or one added with addField().
	void field(const std::string & name);
	void noField();
	/// Regenerates the active field with time t (for animation).
	void refreshField(double t = 0);
	std::vector<std::string> listFields();
	/// Adds a custom field. The generator fills field[col][row] with angles,
	/// interpreted in `angleMode` units (DEGREES by default).
	void addField(const std::string & name, FieldGenerator generator, AngleMode angleMode = DEGREES);
	/// Activates the "hand" field with the given wobble intensity.
	void wiggle(double amount = 1);

	// =========================================================================
	// Brush management
	// =========================================================================

	/// Built-in: "2B", "HB", "2H", "cpencil", "pen", "rotring", "spray",
	/// "marker", "charcoal", "pastel", "crayon". Returns false on invalid params.
	bool add(const std::string & name, BrushParams params);
	std::vector<std::string> box() const;
	/// Multiplies weight, scatter and spacing of every registered brush.
	void scaleBrushes(double scale);
	/// Parameters of a registered brush, or nullptr.
	const BrushParams * getBrushParams(const std::string & name) const;
	/// Restricts strokes and hatches to a rectangle, in current coordinates.
	void clip(double x1, double y1, double x2, double y2);
	void noClip();

	// =========================================================================
	// Stroke
	// =========================================================================

	void set(const std::string & brushName, const Color & color, double weight = 1);
	void pick(const std::string & brushName);
	void stroke(const Color & color);
	void stroke(double gray) { stroke(Color::gray(gray)); }
	void stroke(double r, double g, double b) { stroke(Color(r, g, b)); }
	void noStroke();
	void strokeWeight(double weight);

	// =========================================================================
	// Fill and wash
	// =========================================================================

	void fill(const Color & color, double opacity = 150);
	void fill(double gray, double opacity = 150) { fill(Color::gray(gray), opacity); }
	void fill(double r, double g, double b, double opacity = 150) { fill(Color(r, g, b), opacity); }
	void noFill();
	/// Bleed strength 0-1, direction "out" or "in", optional wash angle.
	void fillBleed(double strength, const std::string & direction = "out", std::optional<double> angle = std::nullopt);
	void fillTexture(double texture = 0.4, double border = 0.4, bool scatter = true);
	void wash(const Color & color, double opacity = 150);
	void wash(double r, double g, double b, double opacity = 150) { wash(Color(r, g, b), opacity); }
	void noWash();

	// =========================================================================
	// Hatch and mass
	// =========================================================================

	void hatch(double dist = 5, double angle = 45, const HatchOptions & options = {});
	void hatchStyle(const std::string & brushName, const Color & color = Color("black"), double weight = 1);
	void noHatch();
	/// Hatches several polygons as one gesture.
	void hatchArray(const std::vector<Polygon> & polygons);
	void hatchArray(const Polygon & polygon) { hatchArray(std::vector<Polygon> {polygon}); }

	void mass(const std::string & brushName, const Color & color, const MassOptions & options = {});
	void noMass();
	/// Masses polygons as one gesture; later polygons cut holes (even-odd).
	void massArray(const std::vector<Polygon> & polygons);
	void massArray(const Polygon & polygon);

	// =========================================================================
	// Primitives
	// =========================================================================

	void line(double x1, double y1, double x2, double y2);
	/// A stroke of `length` starting at (x, y) in direction `dir`, following
	/// the active field.
	void flowLine(double x, double y, double length, double dir);

	void rect(double x, double y, double w, double h, ofRectMode mode = OF_RECTMODE_CORNER);
	/// Returns {plot, offsetX, offsetY} for reuse: plot.hatch(offsetX, offsetY).
	std::tuple<Plot, double, double> circle(double x, double y, double radius, double irregularity = 0);
	/// Stroke-only arc; returns an empty plot when start == end.
	Plot arc(double x, double y, double radius, double start, double end);

	void beginShape(double curvature = 0);
	void vertex(double x, double y, double pressure = 1);
	Plot endShape(bool close = false);

	/// type is Plot::CURVE or Plot::SEGMENTS.
	void beginStroke(Plot::Type type, double x, double y);
	void move(double angle, double length, double pressure);
	void endStroke(double angle, double pressure);

	/// Points are (x, y) or (x, y, pressure).
	Plot spline(const std::vector<glm::vec2> & points, double curvature = 0.5);
	Plot spline(const std::vector<glm::vec3> & points, double curvature = 0.5);
	Polygon polygon(const std::vector<glm::vec2> & points);

	/// The most recently set-up brush, used by geometry created without one.
	static ofxP5Brush * getCurrent() { return currentInstance; }

private:
	// ---- internal state (mirrors p5.brush's State object) -------------------
	struct Affine {
		double a = 1, b = 0, c = 0, d = 1, x = 0, y = 0;
	};
	struct ClipWindow {
		double minX, minY, maxX, maxY;
		Affine inverse;
	};
	struct StrokeState {
		Color color;
		bool hasColor = false;
		double weight = 1;
		std::string type = "HB";
		bool isActive = false;
		std::optional<ClipWindow> clipWindow;
	};
	struct FillState {
		Color color;
		bool hasColor = false;
		double opacity = 150;
		double bleedStrength = 0.07;
		double textureStrength = 0.8;
		double borderStrength = 0.5;
		std::string direction = "out";
		std::optional<double> angle;
		bool scatter = true;
		bool isActive = false;
	};
	struct WashState {
		Color color;
		bool hasColor = false;
		double opacity = 150;
		bool isActive = false;
	};
	struct HatchBrush {
		std::string brush;
		Color color;
		double weight = 1;
	};
	struct HatchState {
		bool isActive = false;
		double dist = 5;
		double angle = 45; // degrees
		HatchOptions options;
		std::optional<HatchBrush> hBrush;
	};
	struct MassState {
		bool isActive = false;
		std::string brush;
		Color color;
		MassOptions options;
	};
	struct FieldState {
		bool isActive = false;
		std::string current;
		double wiggle = 1;
	};
	struct SavedState {
		StrokeState stroke;
		FillState fill;
		WashState wash;
		HatchState hatch;
		MassState mass;
		FieldState field;
	};
	struct BrushEntry {
		BrushParams param;
		std::string tipKey; // texture key for custom / image tips
		AngleMode tipAngleMode = RADIANS; // angle units for custom tip rotate()
	};
	struct FieldEntry {
		FieldGenerator gen;
		std::optional<Field> field;
		AngleMode angleMode = DEGREES;
	};
	struct DirtyRect {
		double minX, minY, maxX, maxY;
	};
	struct MaskInfo {
		bool isDrawn = false;
		std::optional<DirtyRect> dirty;
	};
	struct SubPath {
		bool isClosed = false;
		double curvature = 0;
		std::vector<glm::dvec3> vert;
	};
	struct HatchLine {
		double x1, y1, x2, y2, scanY;
		bool isConnector;
	};
	struct MassShape {
		std::vector<Polygon> polys;
		bool isArray = false;
	};
	struct Vec2d {
		double x, y;
	};
	struct FillPoly;
	friend struct FillPoly;

	// ---- angle helpers -------------------------------------------------------
	bool usesRadians() const { return angleModeValue == RADIANS; }
	double toDegrees(double angle, bool isRad = false) const;
	double toDegreesSigned(double angle, bool isRad = false) const;
	double calcAngle(double x1, double y1, double x2, double y2) const;

	// ---- randomness ----------------------------------------------------------
	double rr(double min = 0, double max = 1) { return min + rng() * (max - min); }
	double rr2(double min = 0, double max = 1) { return min + rng2() * (max - min); }
	int randInt(double min, double max) { return ofxP5BrushDetail::toInt32(rr(min, max)); }
	int randInt2(double min, double max) { return ofxP5BrushDetail::toInt32(rr2(min, max)); }
	double gaussian(double mean = 0, double stdev = 1);
	template <class T>
	const T & rArray(const std::vector<T> & values) {
		return values[ofxP5BrushDetail::toInt32(rng() * values.size())];
	}
	void onSeed();
	void fillGaussianPools();

	// ---- target / compositing ------------------------------------------------
	void isCanvasReady();
	void ensureResources();
	void allocateMasks();
	int pixelWidth() const;
	int pixelHeight() const;
	GLuint targetFboId() const;
	Affine getAffineMatrix() const;
	std::optional<DirtyRect> normalizeDirtyRect(const DirtyRect & rect) const;
	void markDirtyRect(MaskInfo & mask, const DirtyRect & rect);
	void clearMask(bool brushMask);
	void clearFbo(GLuint fboId, int w, int h, const ofFloatColor & color);
	void blend(const Color * color, bool isLast = false);
	void applyShader(bool brushMask);
	void flushActiveComposite();
	void resetCompositeState();
	void onDrawEvent(ofEventArgs & args);

	// ---- fields --------------------------------------------------------------
	void isFieldReady();
	void addStandardFields();
	Field genField() const;
	Field generateField(FieldEntry & entry, double t);
	const Field * flowField() const;

	// ---- strokes -------------------------------------------------------------
	void addStandardBrushes();
	void initializeDrawingState(double x, double y, double length, Plot * plot);
	void drawStroke(double angleScale, bool isPlot);
	void drawPlot(Plot & p, double x, double y, double scale);
	bool saveStrokeState();
	void restoreStrokeState();
	void tip();
	double calculatePressure();
	double simPressure();
	double gauss();
	double calculateAlpha() const;
	double getImageTipOverscan() const;
	bool isInsideClippingArea() const;
	void drawSpray(double pressure);
	void drawMarker(double pressure, bool vibrate, double alpha);
	void drawImageTip(double pressure, double alpha);
	void drawDefault(double pressure);
	void markerTip();
	void queueCircle(double x, double y, double diameter, double alpha);
	void queueImage(double x, double y, double size, double angle, double alpha, double extraPadding);
	void snapshotMatrix();
	void glDrawCircles();
	void glDrawImages(const std::string & tipKey);
	void drawStamps(const std::vector<glm::vec3> & pos, const std::vector<glm::vec2> & corners, float mode,
		const ofTexture * tex);
	ofTexture * getTipTexture(const BrushEntry & entry);
	static void imageToWhite(ofPixels & pixels);

	// ---- geometry ------------------------------------------------------------
	Plot createSpline(std::vector<glm::dvec3> points, double curvature, bool close);
	Plot splineImpl(const std::vector<glm::dvec3> & points, double curvature);
	Polygon genPolFromPlot(Plot & plot, double x, double y, double scale, double side);
	void polygonDraw(Polygon & p, const std::string * brushName, const Color * color, double weight);
	void polygonFill(Polygon & p);
	void polygonWash(Polygon & p);
	void polygonHatch(Polygon & p);
	void polygonMass(Polygon & p);
	void polygonShow(Polygon & p);
	void plotDraw(Plot & p, double x, double y, double scale);
	void plotFill(Plot & p, double x, double y, double scale);
	void plotWash(Plot & p, double x, double y, double scale);
	void plotHatch(Plot & p, double x, double y, double scale);
	void plotMass(Plot & p, double x, double y, double scale);
	void plotShow(Plot & p, double x, double y, double scale);

	// ---- fill / wash ---------------------------------------------------------
	void createFill(const Polygon & polygon);
	void drawWashPolygon(const Polygon & polygon);
	void rasterPolygon(const std::vector<Vec2d> & verts, const Affine & m, float alpha);
	void rasterStroke(const std::vector<Vec2d> & verts, const Affine & m, double lineWidth, float alpha);
	void rasterCircleErase(double x, double y, double radius, const Affine & m, float alpha);
	void markFillDirty(const ofxP5BrushDetail::PixelRect & r);

	// ---- hatch / mass --------------------------------------------------------
	std::vector<HatchLine> getHatchLines(const std::vector<Polygon> & polygons);
	void createHatch(const std::vector<Polygon> & polygons);
	void createMass(const MassShape & shape);
	void drawMassPass(const MassShape & shape, double dist, double angleDeg, const HatchOptions & options,
		const glm::dvec2 & pivotBias);
	Plot arcDegrees(double x, double y, double radius, double startDeg, double endDeg);

	// ---- data ----------------------------------------------------------------
	static ofxP5Brush * currentInstance;

	bool ready = false;
	float Cwidth = 0.f, Cheight = 0.f, Density = 1.f;
	float canvasWidth = 0.f, canvasHeight = 0.f, canvasDensity = 1.f;
	ofFbo canvasFbo;
	ofFbo * targetFbo = nullptr;
	int beginDepth = 0;

	// GL resources
	bool shadersLoaded = false;
	bool shadersFailed = false;
	ofShader stampShader;
	ofShader blendShader;
	ofFbo strokeMaskFbo;
	ofTexture sourceTex;
	ofTexture fillMaskTex;
	ofVbo stampVbo;
	ofVbo quadVbo;
	int maskWidth = 0, maskHeight = 0;
	std::unordered_map<std::string, ofTexture> tipTextures;
	std::vector<unsigned char> uploadBuffer;

	// Pending stamps
	std::vector<glm::vec3> circlePos;
	std::vector<glm::vec2> circleCorner;
	std::optional<DirtyRect> circleDirty;
	std::vector<glm::vec3> imgPos;
	std::vector<glm::vec2> imgCorner;
	std::optional<DirtyRect> imgDirty;

	// CPU fill mask (Canvas2D equivalent)
	std::vector<float> fillMask;
	ofxP5BrushDetail::MaskRasterizer raster;

	// Compositor state (p5.brush's Mix object)
	MaskInfo strokeMaskInfo, fillMaskInfo;
	bool mixIsBlending = false;
	std::optional<std::array<float, 4>> cachedColor;
	int mixIsBrush = -1; // -1 unset, 0 fill, 1 brush
	bool mixJustChanged = false;

	// State
	AngleMode angleModeValue = RADIANS;
	StrokeState strokeState;
	FillState fillState;
	WashState washState;
	HatchState hatchState;
	MassState massState;
	FieldState fieldState;
	std::vector<SavedState> stateStack;

	std::vector<std::string> brushOrder;
	std::unordered_map<std::string, BrushEntry> brushes;
	std::vector<std::string> fieldOrder;
	std::unordered_map<std::string, FieldEntry> fields;
	bool fieldLoaded = false;
	double fResolution = 1, fLeftX = 0, fTopY = 0;
	int fNumColumns = 0, fNumRows = 0;

	// Randomness
	ofxP5BrushDetail::Rng rng, rng2;
	ofxP5BrushDetail::SimplexNoise2D noiseGen, noiseGen2;
	bool gaussCached = false;
	double gaussZ1 = 0;
	std::vector<double> strokeGaussians;
	std::vector<double> fillGaussA, fillGaussB;

	// Stroke engine (p5.brush's module-level drawing variables)
	Position sPosition;
	double sLength = 0;
	Plot * sPlot = nullptr;
	double sDir = 0;
	double sCachedPlotAngle = 0;
	Affine sMatrix;
	double sScale = 1;
	struct StrokeCurrent {
		const BrushEntry * entry = nullptr;
		const BrushParams * p = nullptr;
		int kind = 0; // 0 default, 1 spray, 2 marker, 3 image/custom
		double seed = 0;
		bool isCustomPressure = false;
		double a = 0, b = 0, cp = 0, ct = 0, cs = 1, ck = 0;
		double min = 1, max = 1;
		double cos = 1, sin = 0;
		double alpha = 0;
		double overscan = 8;
		int pressureCount = 10;
		bool hasCachedPressure = false;
		double cachedPressure = 1;
		bool clipActive = false;
		double clipA = 1, clipB = 0, clipC = 0, clipD = 1, clipTX = 0, clipTY = 0;
		double clipMinX = 0, clipMaxX = 0, clipMinY = 0, clipMaxY = 0;
	} cur;

	// Shapes (beginShape / beginStroke)
	std::optional<SubPath> currentShape;
	double shapeCurvature = 0;
	std::optional<Plot> strokePlot;
	glm::dvec2 strokeOrigin {0, 0};

	// Fill
	const Polygon * fillPolygon = nullptr;
	double fillBBMinX = 0, fillBBMinY = 0, fillBBMaxX = 0, fillBBMaxY = 0;
	double growCap = 0;
	Affine fillMatrix;
};

template <class K>
K ofxP5Brush::wRand(const std::vector<std::pair<K, double>> & weights) {
	double total = 0;
	for (const auto & w : weights) total += w.second;
	const double pick = rng() * total;
	double cumulative = 0;
	for (const auto & w : weights) {
		cumulative += w.second;
		if (pick < cumulative) return w.first;
	}
	return weights.empty() ? K {} : weights.back().first;
}
