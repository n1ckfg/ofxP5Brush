#include "ofApp.h"

// A static composition showing the core ofxP5Brush features: pencil and
// marker strokes, watercolor fills, hatching, vector fields and splines.
// Space: repaint with a new seed. S: save the canvas as a PNG.

//--------------------------------------------------------------
void ofApp::setup() {
	ofSetWindowTitle("ofxP5Brush: basic");

	// The brush paints into its own canvas (an ofFbo) that persists between
	// frames, so a drawing can be made once and displayed every frame.
	brush.setup(800, 600);
	brush.angleMode(ofxP5Brush::DEGREES);

	// Built-in brushes are defined at a small scale; scale them to the canvas.
	brush.scaleBrushes(3);

	paint();
}

//--------------------------------------------------------------
void ofApp::paint() {
	// Same seed, same drawing.
	brush.seed(seed);
	brush.noiseSeed(seed);

	brush.background("#fffceb");

	// Watercolor fills. noStroke() leaves the shapes without an outline.
	brush.noStroke();
	brush.fill("#002185", 90);
	brush.fillBleed(0.25);
	brush.fillTexture(0.5, 0.4);
	brush.circle(230, 250, 130);

	brush.fill("#fcd300", 110);
	brush.fillBleed(0.12);
	brush.rect(380, 200, 250, 170);
	brush.noFill();

	// Hatching: every shape drawn while hatch() is active gets hatched, here
	// with its own brush from hatchStyle(). The HB outline comes from set().
	brush.hatchStyle("2H", "#ff2702", 1);
	brush.hatch(8, 30, {0.1});
	brush.set("HB", "#2f2a26", 1);
	brush.polygon({{70, 430}, {300, 400}, {330, 560}, {50, 540}});
	brush.noHatch();

	// Flow lines bend with the active vector field.
	const std::vector<std::string> pencils = {"2B", "HB", "cpencil", "charcoal"};
	brush.field("seabed");
	for (int i = 0; i < 14; i++) {
		brush.set(pencils[i % pencils.size()], "#2c695a", 1);
		brush.flowLine(420 + i * 24, 420, 150, 270);
	}
	brush.noField();

	// A spline through (x, y, pressure) points.
	brush.set("charcoal", "#003c32", 1.2);
	brush.spline({{430, 80, 1}, {540, 30, 1.5}, {660, 140, 0.8}, {770, 60, 1}}, 0.8);

	// Markers and pens.
	brush.set("marker", "#ff2702", 1.2);
	brush.line(60, 80, 380, 60);
	brush.set("rotring", "#2f2a26", 1);
	brush.circle(700, 480, 50, 0.4);
}

//--------------------------------------------------------------
void ofApp::draw() {
	ofBackground(40);
	brush.draw(0, 0);
	ofDrawBitmapStringHighlight("space: new seed (" + ofToString(seed) + ")   s: save png", 10, ofGetHeight() - 10);
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
	if (key == ' ') {
		seed = static_cast<int>(ofRandom(1000000));
		paint();
	} else if (key == 's') {
		brush.render();
		ofPixels pixels;
		brush.getCanvas().readToPixels(pixels);
		ofSaveImage(pixels, "ofxP5Brush_" + ofToString(seed) + ".png");
	}
}
