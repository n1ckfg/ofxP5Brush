// ofxP5Brush::Color: conversions from openFrameworks colors, 0-255 channel
// values and CSS color strings (what p5's color() accepts).

#include "ofxP5Brush.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <unordered_map>

namespace {

const std::unordered_map<std::string, uint32_t> & namedColors() {
	static const std::unordered_map<std::string, uint32_t> colors = {
		{"aliceblue", 0xf0f8ff}, {"antiquewhite", 0xfaebd7}, {"aqua", 0x00ffff}, {"aquamarine", 0x7fffd4},
		{"azure", 0xf0ffff}, {"beige", 0xf5f5dc}, {"bisque", 0xffe4c4}, {"black", 0x000000},
		{"blanchedalmond", 0xffebcd}, {"blue", 0x0000ff}, {"blueviolet", 0x8a2be2}, {"brown", 0xa52a2a},
		{"burlywood", 0xdeb887}, {"cadetblue", 0x5f9ea0}, {"chartreuse", 0x7fff00}, {"chocolate", 0xd2691e},
		{"coral", 0xff7f50}, {"cornflowerblue", 0x6495ed}, {"cornsilk", 0xfff8dc}, {"crimson", 0xdc143c},
		{"cyan", 0x00ffff}, {"darkblue", 0x00008b}, {"darkcyan", 0x008b8b}, {"darkgoldenrod", 0xb8860b},
		{"darkgray", 0xa9a9a9}, {"darkgreen", 0x006400}, {"darkgrey", 0xa9a9a9}, {"darkkhaki", 0xbdb76b},
		{"darkmagenta", 0x8b008b}, {"darkolivegreen", 0x556b2f}, {"darkorange", 0xff8c00}, {"darkorchid", 0x9932cc},
		{"darkred", 0x8b0000}, {"darksalmon", 0xe9967a}, {"darkseagreen", 0x8fbc8f}, {"darkslateblue", 0x483d8b},
		{"darkslategray", 0x2f4f4f}, {"darkslategrey", 0x2f4f4f}, {"darkturquoise", 0x00ced1},
		{"darkviolet", 0x9400d3}, {"deeppink", 0xff1493}, {"deepskyblue", 0x00bfff}, {"dimgray", 0x696969},
		{"dimgrey", 0x696969}, {"dodgerblue", 0x1e90ff}, {"firebrick", 0xb22222}, {"floralwhite", 0xfffaf0},
		{"forestgreen", 0x228b22}, {"fuchsia", 0xff00ff}, {"gainsboro", 0xdcdcdc}, {"ghostwhite", 0xf8f8ff},
		{"gold", 0xffd700}, {"goldenrod", 0xdaa520}, {"gray", 0x808080}, {"green", 0x008000},
		{"greenyellow", 0xadff2f}, {"grey", 0x808080}, {"honeydew", 0xf0fff0}, {"hotpink", 0xff69b4},
		{"indianred", 0xcd5c5c}, {"indigo", 0x4b0082}, {"ivory", 0xfffff0}, {"khaki", 0xf0e68c},
		{"lavender", 0xe6e6fa}, {"lavenderblush", 0xfff0f5}, {"lawngreen", 0x7cfc00}, {"lemonchiffon", 0xfffacd},
		{"lightblue", 0xadd8e6}, {"lightcoral", 0xf08080}, {"lightcyan", 0xe0ffff},
		{"lightgoldenrodyellow", 0xfafad2}, {"lightgray", 0xd3d3d3}, {"lightgreen", 0x90ee90},
		{"lightgrey", 0xd3d3d3}, {"lightpink", 0xffb6c1}, {"lightsalmon", 0xffa07a}, {"lightseagreen", 0x20b2aa},
		{"lightskyblue", 0x87cefa}, {"lightslategray", 0x778899}, {"lightslategrey", 0x778899},
		{"lightsteelblue", 0xb0c4de}, {"lightyellow", 0xffffe0}, {"lime", 0x00ff00}, {"limegreen", 0x32cd32},
		{"linen", 0xfaf0e6}, {"magenta", 0xff00ff}, {"maroon", 0x800000}, {"mediumaquamarine", 0x66cdaa},
		{"mediumblue", 0x0000cd}, {"mediumorchid", 0xba55d3}, {"mediumpurple", 0x9370db},
		{"mediumseagreen", 0x3cb371}, {"mediumslateblue", 0x7b68ee}, {"mediumspringgreen", 0x00fa9a},
		{"mediumturquoise", 0x48d1cc}, {"mediumvioletred", 0xc71585}, {"midnightblue", 0x191970},
		{"mintcream", 0xf5fffa}, {"mistyrose", 0xffe4e1}, {"moccasin", 0xffe4b5}, {"navajowhite", 0xffdead},
		{"navy", 0x000080}, {"oldlace", 0xfdf5e6}, {"olive", 0x808000}, {"olivedrab", 0x6b8e23},
		{"orange", 0xffa500}, {"orangered", 0xff4500}, {"orchid", 0xda70d6}, {"palegoldenrod", 0xeee8aa},
		{"palegreen", 0x98fb98}, {"paleturquoise", 0xafeeee}, {"palevioletred", 0xdb7093},
		{"papayawhip", 0xffefd5}, {"peachpuff", 0xffdab9}, {"peru", 0xcd853f}, {"pink", 0xffc0cb},
		{"plum", 0xdda0dd}, {"powderblue", 0xb0e0e6}, {"purple", 0x800080}, {"rebeccapurple", 0x663399},
		{"red", 0xff0000}, {"rosybrown", 0xbc8f8f}, {"royalblue", 0x4169e1}, {"saddlebrown", 0x8b4513},
		{"salmon", 0xfa8072}, {"sandybrown", 0xf4a460}, {"seagreen", 0x2e8b57}, {"seashell", 0xfff5ee},
		{"sienna", 0xa0522d}, {"silver", 0xc0c0c0}, {"skyblue", 0x87ceeb}, {"slateblue", 0x6a5acd},
		{"slategray", 0x708090}, {"slategrey", 0x708090}, {"snow", 0xfffafa}, {"springgreen", 0x00ff7f},
		{"steelblue", 0x4682b4}, {"tan", 0xd2b48c}, {"teal", 0x008080}, {"thistle", 0xd8bfd8},
		{"tomato", 0xff6347}, {"turquoise", 0x40e0d0}, {"violet", 0xee82ee}, {"wheat", 0xf5deb3},
		{"white", 0xffffff}, {"whitesmoke", 0xf5f5f5}, {"yellow", 0xffff00}, {"yellowgreen", 0x9acd32},
	};
	return colors;
}

std::string trimLower(const std::string & s) {
	size_t start = 0, end = s.size();
	while (start < end && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
	while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
	std::string out = s.substr(start, end - start);
	std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
	return out;
}

int hexDigit(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return -1;
}

bool parseHex(const std::string & s, float out[4]) {
	const std::string h = s.substr(1);
	for (char c : h) {
		if (hexDigit(c) < 0) return false;
	}
	auto channel = [&](int i, int len) {
		if (len == 1) return (hexDigit(h[i]) * 17) / 255.f;
		return (hexDigit(h[i]) * 16 + hexDigit(h[i + 1])) / 255.f;
	};
	switch (h.size()) {
	case 3:
	case 4:
		for (size_t i = 0; i < h.size(); ++i) out[i] = channel(static_cast<int>(i), 1);
		if (h.size() == 3) out[3] = 1.f;
		return true;
	case 6:
	case 8:
		for (size_t i = 0; i < h.size() / 2; ++i) out[i] = channel(static_cast<int>(i * 2), 2);
		if (h.size() == 6) out[3] = 1.f;
		return true;
	default:
		return false;
	}
}

// Splits "a, b, c / d" or "a b c / d" into tokens.
std::vector<std::string> splitArgs(const std::string & args) {
	std::vector<std::string> tokens;
	std::string current;
	for (char c : args) {
		if (c == ',' || c == ' ' || c == '/' || c == '\t') {
			if (!current.empty()) tokens.push_back(current);
			current.clear();
		} else {
			current += c;
		}
	}
	if (!current.empty()) tokens.push_back(current);
	return tokens;
}

bool parseNumber(const std::string & token, float & value, bool & isPercent) {
	isPercent = !token.empty() && token.back() == '%';
	const std::string num = isPercent ? token.substr(0, token.size() - 1) : token;
	if (num.empty()) return false;
	char * end = nullptr;
	value = std::strtof(num.c_str(), &end);
	return end && *end == '\0';
}

float hueToRgb(float p, float q, float t) {
	if (t < 0) t += 1;
	if (t > 1) t -= 1;
	if (t < 1.f / 6) return p + (q - p) * 6 * t;
	if (t < 1.f / 2) return q;
	if (t < 2.f / 3) return p + (q - p) * (2.f / 3 - t) * 6;
	return p;
}

bool parseFunctional(const std::string & s, float out[4]) {
	const auto open = s.find('(');
	const auto close = s.rfind(')');
	if (open == std::string::npos || close == std::string::npos || close < open) return false;
	const std::string fn = s.substr(0, open);
	const auto tokens = splitArgs(s.substr(open + 1, close - open - 1));
	if (tokens.size() != 3 && tokens.size() != 4) return false;

	float v[4] = {0, 0, 0, 1};
	bool pct[4] = {false, false, false, false};
	for (size_t i = 0; i < tokens.size(); ++i) {
		if (!parseNumber(tokens[i], v[i], pct[i])) return false;
	}
	float alpha = tokens.size() == 4 ? (pct[3] ? v[3] / 100.f : v[3]) : 1.f;
	alpha = ofClamp(alpha, 0.f, 1.f);

	if (fn == "rgb" || fn == "rgba") {
		for (int i = 0; i < 3; ++i) out[i] = ofClamp(pct[i] ? v[i] / 100.f : v[i] / 255.f, 0.f, 1.f);
		out[3] = alpha;
		return true;
	}
	if (fn == "hsl" || fn == "hsla") {
		const float h = std::fmod(std::fmod(v[0], 360.f) + 360.f, 360.f) / 360.f;
		const float sat = ofClamp(v[1] / 100.f, 0.f, 1.f);
		const float light = ofClamp(v[2] / 100.f, 0.f, 1.f);
		if (sat == 0) {
			out[0] = out[1] = out[2] = light;
		} else {
			const float q = light < 0.5f ? light * (1 + sat) : light + sat - light * sat;
			const float p = 2 * light - q;
			out[0] = hueToRgb(p, q, h + 1.f / 3);
			out[1] = hueToRgb(p, q, h);
			out[2] = hueToRgb(p, q, h - 1.f / 3);
		}
		out[3] = alpha;
		return true;
	}
	return false;
}

} // namespace

ofxP5Brush::Color::Color(const ofColor & c)
	: r(c.r / 255.f)
	, g(c.g / 255.f)
	, b(c.b / 255.f)
	, a(c.a / 255.f) { }

ofxP5Brush::Color::Color(const ofFloatColor & c)
	: r(c.r)
	, g(c.g)
	, b(c.b)
	, a(c.a) { }

ofxP5Brush::Color::Color(const std::string & css) {
	if (!parse(css, *this)) {
		ofLogError("ofxP5Brush") << "invalid color value \"" << css << "\", using black";
		r = g = b = 0.f;
		a = 1.f;
	}
}

ofxP5Brush::Color::Color(const char * css)
	: Color(std::string(css ? css : "")) { }

ofxP5Brush::Color::Color(float r255, float g255, float b255, float a255)
	: r(ofClamp(r255, 0.f, 255.f) / 255.f)
	, g(ofClamp(g255, 0.f, 255.f) / 255.f)
	, b(ofClamp(b255, 0.f, 255.f) / 255.f)
	, a(ofClamp(a255, 0.f, 255.f) / 255.f) { }

ofxP5Brush::Color ofxP5Brush::Color::gray(float v255, float a255) {
	return Color(v255, v255, v255, a255);
}

bool ofxP5Brush::Color::parse(const std::string & css, Color & out) {
	const std::string s = trimLower(css);
	float v[4] = {0, 0, 0, 1};
	bool ok = false;
	if (s.empty()) {
		ok = false;
	} else if (s[0] == '#') {
		ok = parseHex(s, v);
	} else if (s.find('(') != std::string::npos) {
		ok = parseFunctional(s, v);
	} else if (s == "transparent") {
		v[3] = 0.f;
		ok = true;
	} else {
		const auto & names = namedColors();
		const auto it = names.find(s);
		if (it != names.end()) {
			v[0] = ((it->second >> 16) & 0xff) / 255.f;
			v[1] = ((it->second >> 8) & 0xff) / 255.f;
			v[2] = (it->second & 0xff) / 255.f;
			ok = true;
		}
	}
	if (!ok) return false;
	out.r = v[0];
	out.g = v[1];
	out.b = v[2];
	out.a = v[3];
	return true;
}
