#pragma once

#include "ofMain.h"
#include "ofxP5Brush.h"

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void draw() override;
	void mousePressed(int x, int y, int button) override;

private:
	void scene0BrushRain(float t);
	void scene1Fields(float t);
	void scene2BrushWheel(float t);
	void scene3Hatches(float t);
	void scene4Watercolor(float t);
	void scene5Splines(float t);

	ofxP5Brush brush;

	// Pre-generated random seeds for deterministic drawing.
	std::vector<double> seeds;

	bool isBackgroundDrawn = false;
	double rrr = 213123;
	int lastScene = -1;
};
