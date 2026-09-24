#include "ofMain.h"
#include "ofApp.h"

//========================================================================
int main() {
	ofGLWindowSettings settings;
	settings.setSize(800, 600);
	// ofxP5Brush works with the default GL 2.1 renderer (e.g. on a Raspberry
	// Pi 4) and with the programmable renderer:
	// settings.setGLVersion(3, 2);
	settings.windowMode = OF_WINDOW;

	auto window = ofCreateWindow(settings);
	ofRunApp(window, std::make_shared<ofApp>());
	ofRunMainLoop();
}
