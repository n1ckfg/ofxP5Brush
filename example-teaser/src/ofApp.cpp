#include "ofApp.h"

// Port of the p5.brush 2.x teaser sketch.
// Cycles through 7 scenes demonstrating core and new 2.x features.
// Click anywhere to pause on the current scene.

static const std::vector<std::string> palette = {
	"#002185", "#003c32", "#fcd300", "#ff2702", "#6b9404", "#4e93cc", "#9b1d10"
};
static const std::string PAPER = "#fffceb";

//--------------------------------------------------------------
void ofApp::setup() {
	ofSetWindowTitle("ofxP5Brush: teaser");
	ofSetFrameRate(30);

	brush.setup(W, H);
	brush.angleMode(ofxP5Brush::DEGREES);
	brush.scaleBrushes(3);

	// Register custom "watercolor" brush.
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
}

//--------------------------------------------------------------
void ofApp::draw() {
	int frameNum = ofGetFrameNum();
	float t = frameNum / 30.0f;
	int scene = static_cast<int>(t / 6.0f) % 7;

	switch (scene) {
		case 0: scene0BrushRain(t); break;
		case 1: scene1Mass(t); break;
		case 2: scene2MassArray(t); break;
		case 3: scene3HatchArray(t); break;
		case 4: scene4Fields(t); break;
		case 5: scene5BrushWheel(t); break;
		case 6: scene6Watercolor(t); break;
	}

	lastScene = scene;
	brush.draw(0, 0);
}

//--------------------------------------------------------------
std::vector<glm::vec2> ofApp::circlePoints(float cx, float cy, float r, int n, float phase) {
	std::vector<glm::vec2> pts;
	for (int i = 0; i < n; i++) {
		float a = phase + (360.0f * i) / n;
		pts.push_back({cx + r * cos(a * DEG_TO_RAD), cy + r * sin(a * DEG_TO_RAD)});
	}
	return pts;
}

//--------------------------------------------------------------
ofApp::CurvedShape ofApp::rounded(float cx, float cy, float rx, float ry, float phase) {
	CurvedShape shape;
	shape.curvature = 1;
	for (int i = 0; i < 7; i++) {
		float a = phase + i * 360.0f / 7.0f;
		float r = 1 + 0.12f * sin(2 * a * DEG_TO_RAD + phase * DEG_TO_RAD);
		shape.points.push_back({cx + rx * r * cos(a * DEG_TO_RAD), cy + ry * r * sin(a * DEG_TO_RAD)});
	}
	return shape;
}

//--------------------------------------------------------------
// Scene 0 · Brush Rain
//--------------------------------------------------------------
void ofApp::scene0BrushRain(float t) {
	if (lastScene != 0) brush.background(PAPER);

	const std::vector<std::string> colores = {
		"#2c695a", "#4ad6af", "#7facc6", "#4e93cc", "#f6684f", "#ffd300"
	};
	const std::vector<std::string> brushes = {
		"marker", "watercolor", "spray", "charcoal", "HB", "2B", "cpencil", "2H", "rotring"
	};

	brush.field("seabed");
	brush.set(brush.random(brushes), brush.random(colores), brush.random(0.7, 1.6));
	brush.flowLine(brush.random(W), brush.random(H), brush.random(140, 240), brush.random(360));
	brush.noField();
}

//--------------------------------------------------------------
// Scene 1 · Mass
//--------------------------------------------------------------
void ofApp::scene1Mass(float t) {
	brush.background(PAPER);
	brush.wiggle(2);

	float sweep = 0.5f - 0.5f * cos(fmod(t, 6.0f) * 60.0f * DEG_TO_RAD);

	struct BlobDef {
		float cx, cy, R;
		std::string col;
		int seedVal;
		float rot;
		float strength;
		bool gradient;
	};

	std::vector<BlobDef> blobs = {
		{130 + 12 * sin(t * 2), 245 + 10 * cos(t * 1.6f), 245, "#fcd300", 11, t * 1.2f, 1.0f, true},
		{390 + 10 * cos(t * 1.7f), 425 + 12 * sin(t * 1.4f), 220, "#002185", 23, -t, 0.65f, false},
		{455 + 12 * sin(t * 1.5f + 2), 205 + 10 * cos(t * 1.8f), 180, "#ff2702", 37, t * 0.8f, 0.3f, false}
	};

	for (auto & b : blobs) {
		brush.seed(b.seedVal);
		float ph1 = brush.random(360), ph2 = brush.random(360), ph3 = brush.random(360);

		brush.noStroke();
		brush.mass("pastel", b.col, {
			sweep,                         // precision
			b.strength,                    // strength
			b.gradient ? (double)sweep : 0.0, // gradient
			false                          // outline
		});

		// Build 8-point organic spline shape.
		std::vector<glm::vec2> pts;
		for (int k = 0; k < 8; k++) {
			float a = (360.0f / 8.0f) * k + b.rot;
			float r = b.R * (1
				+ 0.13f * sin((a + ph1) * DEG_TO_RAD)
				+ 0.08f * sin((2 * a + ph2) * DEG_TO_RAD)
				+ 0.04f * sin((3 * a + ph3) * DEG_TO_RAD));
			pts.push_back({b.cx + r * cos(a * DEG_TO_RAD), b.cy + r * sin(a * DEG_TO_RAD)});
		}
		brush.beginShape(1);
		for (auto & p : pts) brush.vertex(p.x, p.y);
		brush.endShape(true);

		// Redraw contour with HB.
		brush.noMass();
		brush.set("HB", "#000000", 1);
		brush.beginShape(1);
		for (auto & p : pts) brush.vertex(p.x, p.y);
		brush.endShape(true);
		brush.noStroke();
	}

	brush.noField();
}

//--------------------------------------------------------------
// Scene 2 · Mass Array
//--------------------------------------------------------------
void ofApp::scene2MassArray(float t) {
	brush.seed(3030);
	brush.noiseSeed(3030);
	brush.background("#141b24");

	struct CheeseDef {
		std::string color, contour, brushName;
		int seedVal;
		float strength, gradient;
		CurvedShape outer;
		float holeCx, holeCy, holeRx, holeRy;
	};

	std::vector<CheeseDef> cheeses = {
		{palette[2], palette[6], "crayon", 101, 1.0f, 0.0f,
			rounded(140, 165, 190, 170, 12),
			140, 165, 170, 150},
		{palette[3], palette[5], "crayon", 202, 1.0f, 0.45f,
			{{{235,20},{560,55},{625,260},{440,365},{230,245}}, 0},
			425, 190, 175, 145},
		{palette[0], palette[1], "crayon", 303, 0.65f, 0.0f,
			rounded(320, 465, 235, 185, 205),
			320, 465, 210, 160}
	};

	int holeCount = static_cast<int>((fmod(ofGetFrameNum(), 180.0f) / 180.0f) * 7);

	// Helper: convert a CurvedShape to a Polygon via beginShape/endShape/genPol.
	auto toPolygon = [&](const CurvedShape & shape) -> ofxP5Brush::Polygon {
		brush.beginShape(shape.curvature);
		for (auto & p : shape.points) brush.vertex(p.x, p.y);
		auto plot = brush.endShape(true);
		return plot.genPol(plot.origin.x, plot.origin.y, 1, 0.3);
	};

	brush.noStroke();
	for (auto & cheese : cheeses) {
		// Generate random holes.
		brush.seed(cheese.seedVal + 500);
		std::vector<CurvedShape> holes;
		for (int i = 0; i < 6; i++) {
			float a = brush.random(360);
			bool edge = (i < 2);
			float distance = edge ? brush.random(0.78, 1.02) : brush.random(0.1, 0.65);
			float size = edge ? brush.random(58, 88) : brush.random(28, 52);
			holes.push_back(rounded(
				cheese.holeCx + cheese.holeRx * distance * cos(a * DEG_TO_RAD),
				cheese.holeCy + cheese.holeRy * distance * sin(a * DEG_TO_RAD),
				size,
				size * brush.random(0.75, 1.2),
				brush.random(360)
			));
		}

		// Build ring list: outer + holes (up to holeCount).
		std::vector<CurvedShape> rings;
		rings.push_back(cheese.outer);
		for (int i = 0; i < std::min(holeCount, (int)holes.size()); i++) {
			rings.push_back(holes[i]);
		}

		std::vector<ofxP5Brush::Polygon> polygons;
		for (auto & ring : rings) {
			polygons.push_back(toPolygon(ring));
		}

		brush.seed(cheese.seedVal);
		brush.mass(cheese.brushName, cheese.color, {
			0.25,                     // precision
			cheese.strength,          // strength
			(double)cheese.gradient,   // gradient
			false                     // outline
		});
		brush.massArray(polygons);
		brush.noMass();

		// Redraw outlines with HB pencil.
		brush.seed(cheese.seedVal + 1000);
		brush.set("HB", cheese.contour, 1);
		for (auto & ring : rings) {
			brush.beginShape(ring.curvature);
			for (auto & p : ring.points) brush.vertex(p.x, p.y);
			brush.endShape(true);
		}
		brush.noStroke();
	}
}

//--------------------------------------------------------------
// Scene 3 · Hatch Array
//--------------------------------------------------------------
void ofApp::scene3HatchArray(float t) {
	brush.seed(2024);
	brush.noiseSeed(2024);
	brush.background("#ffe6d4");

	// Fixed rectangle shape, rotated -12 degrees.
	const float rectAngle = -12;
	const float ra = rectAngle * DEG_TO_RAD;
	std::vector<glm::vec2> rectangle;
	std::vector<std::pair<float,float>> rectCorners = {{-125,-105},{125,-105},{125,105},{-125,105}};
	for (auto & [x, y] : rectCorners) {
		rectangle.push_back({
			345 + x * cos(ra) - y * sin(ra),
			325 + x * sin(ra) + y * cos(ra)
		});
	}

	// Three families of shapes.
	using ShapeDef = CurvedShape;

	std::vector<std::vector<ShapeDef>> families = {
		{ // rounded pebbles
			{{{-30,130},{90,45},{230,90},{255,205},{135,270},{10,230}}, 1},
			{{{220,70},{360,25},{505,95},{530,215},{405,270},{265,205}}, 1},
			{{{370,220},{520,195},{625,325},{555,475},{420,500},{340,355}}, 1},
			{{{205,380},{350,350},{455,480},{370,630},{220,605},{145,480}}, 1},
			{{{-25,305},{115,255},{245,350},{210,495},{75,555},{-25,450}}, 1},
		},
		{ // pointy forms
			{{{-35,55},{145,25},{105,210}}, 0},
			{{{135,80},{510,35},{445,285},{205,245}}, 0},
			{{{475,125},{635,245},{530,540},{405,305}}, 0},
			{{{130,315},{480,365},{315,650},{65,510}}, 0},
			{{{-60,260},{185,330},{105,585},{-30,485}}, 0},
		},
		{ // oversized charcoal ribbons
			{{{-130,40},{80,-15},{365,70},{310,205},{40,235},{-105,170}}, 1},
			{{{265,-80},{465,-55},{680,105},{585,255},{390,205},{250,90}}, 1},
			{{{430,145},{615,180},{715,390},{590,590},{420,490},{350,285}}, 1},
			{{{65,300},{300,255},{535,430},{390,720},{145,655},{5,470}}, 1},
			{{{-145,265},{70,225},{315,390},{225,635},{-15,690},{-125,500}}, 1},
		}
	};

	int styleIndex = static_cast<int>(fmod(ofGetFrameNum(), 180.0f) / 60.0f);

	std::vector<ShapeDef> shapes = families[styleIndex];
	shapes.push_back({rectangle, 0});

	// Build Polygon inputs via beginShape/endShape/genPol.
	brush.noStroke();
	brush.noHatch();
	std::vector<ofxP5Brush::Polygon> polygons;
	for (auto & shape : shapes) {
		brush.beginShape(shape.curvature);
		for (auto & p : shape.points) brush.vertex(p.x, p.y);
		auto plot = brush.endShape(true);
		polygons.push_back(plot.genPol(plot.origin.x, plot.origin.y, 1, 0.3));
	}

	// Hatch styles per family.
	struct HatchStyleDef {
		std::string brushName, color;
		float dist, angle;
		std::string field;
	};
	std::vector<HatchStyleDef> hatchStyles = {
		{"HB", palette[0], 0.75f, 25, ""},
		{"watercolor", palette[3], 9, 65, ""},
		{"charcoal", palette[1], 11, 130, "seabed"}
	};
	auto & hatchStyle = hatchStyles[styleIndex];

	if (!hatchStyle.field.empty()) brush.field(hatchStyle.field);
	else brush.noField();

	brush.hatchStyle(hatchStyle.brushName, hatchStyle.color, 1.2);
	brush.hatch(hatchStyle.dist, hatchStyle.angle, {
		0.15,   // rand
		false,  // continuous
		0.15    // gradient
	});
	brush.hatchArray(polygons);

	// Redraw contours with HB.
	brush.noHatch();
	brush.noField();
	brush.seed(9090);
	brush.noiseSeed(9090);
	brush.set("HB", palette[1], 1.2);
	for (auto & shape : shapes) {
		brush.beginShape(shape.curvature);
		for (auto & p : shape.points) brush.vertex(p.x, p.y);
		brush.endShape(true);
	}
}

//--------------------------------------------------------------
// Scene 4 · Vector Fields
//--------------------------------------------------------------
void ofApp::scene4Fields(float t) {
	brush.background("#080f15");

	auto flowfields = brush.listFields();
	int fieldIdx = static_cast<int>(t) % static_cast<int>(flowfields.size());
	if (fmod(t, 1.0f) == 0) brush.field(flowfields[fieldIdx]);

	brush.seed(33213);
	brush.set("charcoal", "white", 1);
	brush.circle(CX, CY, 180, 0.3);

	brush.pick("HB");
	brush.seed(4412);
	for (int i = 0; i < 30; i++) {
		brush.flowLine(brush.random(W), brush.random(H), 75, 0);
	}
}

//--------------------------------------------------------------
// Scene 5 · Brush Wheel
//--------------------------------------------------------------
void ofApp::scene5BrushWheel(float t) {
	brush.background("#e2e7dc");

	const std::vector<std::string> brushes = {
		"marker", "marker", "watercolor", "watercolor", "charcoal", "HB", "2B", "rotring"
	};
	brush.field("seabed");

	for (int i = 0; i < 20; i++) {
		float angle = i * 18.0f + t * 30.0f;
		brush.seed(33213 + i * 17);
		brush.set(brush.random(brushes), brush.random(palette), 1);
		brush.flowLine(
			CX + 100 * cos(-angle * DEG_TO_RAD),
			CY + 100 * sin(-angle * DEG_TO_RAD),
			320, angle
		);
	}

	brush.noField();
	brush.seed(static_cast<int>(t * 10));
	brush.set(brush.random(brushes), brush.random(palette), 1);
	brush.circle(CX, CY, 100, 0.2);
}

//--------------------------------------------------------------
// Scene 6 · Watercolor Fill
//--------------------------------------------------------------
void ofApp::scene6Watercolor(float t) {
	if (lastScene != 6) brush.background(PAPER);

	const std::vector<std::string> colores = {
		"#7b4800", "#002185", "#003c32", "#fcd300", "#ff2702", "#6b9404"
	};
	brush.set("marker", "#e0b411", 1.1);

	if (static_cast<int>(10 * t) % 3 == 0) {
		brush.fill(brush.random(colores), brush.random(60, 110));
		brush.fillBleed(brush.random(0.1, 0.55));
		brush.fillTexture(0.4, 0.4);
		brush.circle(brush.random(W), brush.random(H), brush.random(40, 90), 0.4);
		brush.noFill();
	}
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button) {
	static bool paused = false;
	paused = !paused;
	ofSetFrameRate(paused ? 0 : 30);
}
