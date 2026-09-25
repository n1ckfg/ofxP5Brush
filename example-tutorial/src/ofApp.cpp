#include "ofApp.h"

// Port of the p5.brush live tutorial sketch.
// Cycles through 6 scenes, each demonstrating a core feature.
// Click anywhere to pause on the current scene.

static const int W = 600;
static const int H = 600;

// Hand-picked colour palette used across all scenes.
static const std::vector<std::string> palette = {
	"#002185", "#003c32", "#fcd300", "#ff2702", "#6b9404"
};

//--------------------------------------------------------------
void ofApp::setup() {
	ofSetWindowTitle("ofxP5Brush: tutorial");
	ofSetFrameRate(30);

	brush.setup(W, H);
	brush.angleMode(ofxP5Brush::DEGREES);
	brush.scaleBrushes(3.5);

	// Register a custom brush (same as the JS "watercolor" custom brush).
	ofxP5Brush::BrushParams watercolorParams;
	watercolorParams.type = "custom";
	watercolorParams.weight = 10;
	watercolorParams.scatter = 1.05;
	watercolorParams.opacity = 9;
	watercolorParams.spacing = 0.3;
	watercolorParams.pressure = {0.8, 1.3};
	watercolorParams.rotate = "natural";
	watercolorParams.tip = [](ofxP5Brush::TipSurface & m) {
		m.fill(0, 200);
		m.rect(-20, -20, 50, 50);
		m.rect(25, 25, 20, 20);
	};
	brush.add("watercolor", watercolorParams);

	// Fill seeds for deterministic drawing.
	for (int i = 0; i < 150; i++) seeds.push_back(ofRandom(1.0));

	brush.background("#fffceb");
}

//--------------------------------------------------------------
void ofApp::draw() {
	int frameNum = ofGetFrameNum();
	float t = frameNum / 30.0f;
	int scene = static_cast<int>(t / 5.0f) % 6;

	switch (scene) {
		case 0: scene0BrushRain(t); break;
		case 1: scene1Fields(t); break;
		case 2: scene2BrushWheel(t); break;
		case 3: scene3Hatches(t); break;
		case 4: scene4Watercolor(t); break;
		case 5: scene5Splines(t); break;
	}

	lastScene = scene;

	// Flush brush canvas to screen.
	brush.draw(0, 0);
}

//--------------------------------------------------------------
// Scene 0 · Brush Rain — brush.flowLine() with a vector field
//--------------------------------------------------------------
void ofApp::scene0BrushRain(float t) {
	if (!isBackgroundDrawn) {
		brush.background("#fffceb");
		isBackgroundDrawn = true;
	}

	const std::vector<std::string> colores = {
		"#2c695a", "#4ad6af", "#7facc6", "#4e93cc", "#f6684f", "#ffd300"
	};
	const std::vector<std::string> brushes = {
		"marker", "watercolor", "spray", "charcoal", "HB", "2B", "cpencil", "2H", "rotring"
	};

	brush.field("seabed");
	brush.set(brush.random(brushes), brush.random(colores), brush.random(0.7, 1.6));
	brush.flowLine(brush.random(W), brush.random(H), brush.random(140, 240), brush.random(360));
}

//--------------------------------------------------------------
// Scene 1 · Vector Fields
//--------------------------------------------------------------
void ofApp::scene1Fields(float t) {
	isBackgroundDrawn = false;
	brush.background("#080f15");

	auto flowfields = brush.listFields();
	int fieldIdx = static_cast<int>(t) % static_cast<int>(flowfields.size());
	if (fmod(t, 1.0f) == 0) brush.field(flowfields[fieldIdx]);

	brush.seed(33213 * seeds[67]);
	brush.set("charcoal", "white", 1);
	brush.circle(300, 300, 180, 0.3);

	brush.pick("HB");
	brush.seed(33213 * seeds[97]);
	for (int i = 0; i < 30; i++) {
		brush.flowLine(brush.random(W), brush.random(H), 75, 0);
	}
}

//--------------------------------------------------------------
// Scene 2 · Brush Wheel
//--------------------------------------------------------------
void ofApp::scene2BrushWheel(float t) {
	isBackgroundDrawn = false;
	brush.background("#e2e7dc");

	const std::vector<std::string> brushes = {
		"marker", "marker", "watercolor", "watercolor", "charcoal", "HB", "2B", "rotring"
	};
	brush.field("seabed");

	const float cx = 300, cy = 300;
	for (int i = 0; i < 20; i++) {
		float angle = i * 18.0f + t * 30.0f;
		brush.seed(33213 * seeds[i]);
		brush.set(brush.random(brushes), brush.random(palette), 1);
		brush.flowLine(
			cx + 100 * cos(-angle * DEG_TO_RAD),
			cy + 100 * sin(-angle * DEG_TO_RAD),
			320, angle
		);
	}

	brush.noField();
	if (static_cast<int>(t * 10) % 5 == 0) rrr = ofRandom(23122);
	brush.seed(rrr);
	brush.set(brush.random(brushes), brush.random(palette), 1);
	brush.circle(300, 300, 100, 0.2);
}

//--------------------------------------------------------------
// Scene 3 · Hatches
//--------------------------------------------------------------
void ofApp::scene3Hatches(float t) {
	isBackgroundDrawn = false;
	brush.background("#ffe6d4");

	// First polygon with HB hatch.
	brush.seed(33213 * seeds[35]);
	brush.hatchStyle("HB", "#c76282", 1.3);
	brush.hatch(15, 45);

	// Helper: compute jittered vertex position.
	auto jitter = [&](float baseX, float baseY) -> glm::vec2 {
		float r = brush.random(0, 360);
		return {
			baseX + 20 * sin((r + t * 150) * DEG_TO_RAD),
			baseY + 20 * sin((r + t * 150) * DEG_TO_RAD)
		};
	};

	brush.polygon({
		jitter(80, 150), jitter(180, 150), jitter(420, 150),
		jitter(480, 450), jitter(280, 450), jitter(130, 450)
	});

	// Second polygon with marker hatch.
	brush.seed(33213 * seeds[75]);
	brush.hatchStyle("marker", "#e0b411", 1.3);
	brush.hatch(10, 130, {0.1});

	auto jitter2 = [&](float baseX, float baseY) -> glm::vec2 {
		float r = brush.random(0, 360);
		float sinVal = sin((r + t * 120) * DEG_TO_RAD);
		return {
			baseX + 20 * cos(360 * sinVal * DEG_TO_RAD),
			baseY + 20 * sin((r + t * 120) * DEG_TO_RAD)
		};
	};

	brush.polygon({
		jitter2(250, 250), jitter2(500, 300), jitter2(300, 520)
	});

	brush.noHatch();
}

//--------------------------------------------------------------
// Scene 4 · Watercolor Fill
//--------------------------------------------------------------
void ofApp::scene4Watercolor(float t) {
	if (!isBackgroundDrawn) {
		brush.background("#fffceb");
		isBackgroundDrawn = true;
	}

	const std::vector<std::string> colores = {
		"#7b4800", "#002185", "#003c32", "#fcd300", "#ff2702", "#6b9404"
	};
	brush.set("marker", "#e0b411", 1.1);

	if (static_cast<int>(10 * t) % 3 == 0) {
		brush.fill(brush.random(colores), brush.random(60, 110));
		brush.fillBleed(brush.random(0.1, 0.55));
		brush.fillTexture(0.4, 0.4);
		brush.rect(brush.random(W), brush.random(H),
			brush.random(50, 140), brush.random(50, 140),
			OF_RECTMODE_CENTER);
		brush.noFill();
	}
}

//--------------------------------------------------------------
// Scene 5 · Splines
//--------------------------------------------------------------
void ofApp::scene5Splines(float t) {
	isBackgroundDrawn = false;
	brush.background("#445e87");
	brush.noField();

	// A circle drawn with the "2B" brush.
	brush.seed(33213 * seeds[67]);
	brush.set("2B", "#0e2d58", 2);
	brush.circle(155, 140, 50);

	brush.seed(33213 * seeds[67]);

	// Control points as (x, y, pressure). The third point animates.
	float r1 = brush.random(0, 360);
	float r2 = brush.random(0, 360);
	float p1press = brush.random(0.8, 1.5);
	float p2press = brush.random(0.8, 1.5);

	float animX = 280 - 150 * cos(360 * sin((r1 + t * 90) * DEG_TO_RAD) * DEG_TO_RAD);
	float animY = 300 + 50 * sin((r2 + t * 90) * DEG_TO_RAD);

	std::vector<glm::vec3> points = {
		{30, 30, 1},
		{250, 100, p1press},
		{animX, animY, p2press},
		{570, 570, 1}
	};
	if (points[1].x == points[1].y) points[1].x += 1;

	// Main spine in charcoal white.
	brush.set("charcoal", "white", 1);
	brush.spline(points, 1);

	// Four offset copies in lighter pencil.
	brush.set("2H", "white", 1);
	for (int i = 1; i <= 4; i++) {
		std::vector<glm::vec3> p = {
			{points[0].x + 55.0f * i, points[0].y, 1},
			{points[1].x - 3.0f * i, points[1].y + 5.0f * i, 1},
			points[2],
			{points[3].x - 100.0f * i, points[3].y, 1}
		};
		brush.seed(33213 * seeds[62]);
		brush.spline(p, 1);
	}
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button) {
	// Click to pause (toggle frameRate between 30 and 0).
	static bool paused = false;
	paused = !paused;
	ofSetFrameRate(paused ? 0 : 30);
}
