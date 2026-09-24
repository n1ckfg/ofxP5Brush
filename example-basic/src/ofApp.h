#pragma once

#include "ofMain.h"
#include "ofxP5Brush.h"

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void draw() override;
	void keyPressed(int key) override;

	void paint();

	ofxP5Brush brush;
	int seed = 42;
};
