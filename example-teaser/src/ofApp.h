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
	void scene1Mass(float t);
	void scene2MassArray(float t);
	void scene3HatchArray(float t);
	void scene4Fields(float t);
	void scene5BrushWheel(float t);
	void scene6Watercolor(float t);

	/// Generates points on a circle with optional phase offset.
	std::vector<glm::vec2> circlePoints(float cx, float cy, float r, int n = 36, float phase = 0);

	/// Generates a rounded organic shape (for the cheese/mass scenes).
	struct CurvedShape {
		std::vector<glm::vec2> points;
		double curvature;
	};
	CurvedShape rounded(float cx, float cy, float rx, float ry, float phase = 0);

	ofxP5Brush brush;
	int lastScene = -1;

	static constexpr int W = 600;
	static constexpr int H = 600;
	static constexpr float CX = 300;
	static constexpr float CY = 300;
};
