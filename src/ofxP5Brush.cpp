// Core of ofxP5Brush: canvas / target management, the compositor that blends
// the stroke and fill masks into the target (p5.brush's `Mix`), brush state
// push / pop, angle units and seeding.

#include "ofxP5Brush.h"
#include "ofxP5BrushShaders.h"

#include <random>

using namespace ofxP5BrushDetail;

ofxP5Brush * ofxP5Brush::currentInstance = nullptr;

namespace {

/// Saves the GL state touched by the brush's direct rendering passes and
/// restores it on scope exit, so openFrameworks' own bookkeeping stays valid.
struct GLStateGuard {
	GLint framebuffer = 0;
	GLint viewport[4] = {0, 0, 0, 0};
	GLint scissorBox[4] = {0, 0, 0, 0};
	GLboolean blend = GL_FALSE, depth = GL_FALSE, scissor = GL_FALSE, cull = GL_FALSE;
	GLint srcRGB = GL_ONE, dstRGB = GL_ZERO, srcA = GL_ONE, dstA = GL_ZERO;
	GLint eqRGB = GL_FUNC_ADD, eqA = GL_FUNC_ADD;
	GLint activeTexture = GL_TEXTURE0;
	GLint tex0 = 0, tex1 = 0;

	GLStateGuard() {
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
		glGetIntegerv(GL_VIEWPORT, viewport);
		glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
		blend = glIsEnabled(GL_BLEND);
		depth = glIsEnabled(GL_DEPTH_TEST);
		scissor = glIsEnabled(GL_SCISSOR_TEST);
		cull = glIsEnabled(GL_CULL_FACE);
		glGetIntegerv(GL_BLEND_SRC_RGB, &srcRGB);
		glGetIntegerv(GL_BLEND_DST_RGB, &dstRGB);
		glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcA);
		glGetIntegerv(GL_BLEND_DST_ALPHA, &dstA);
		glGetIntegerv(GL_BLEND_EQUATION_RGB, &eqRGB);
		glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &eqA);
		glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
		glActiveTexture(GL_TEXTURE1);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex1);
		glActiveTexture(GL_TEXTURE0);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex0);
	}

	~GLStateGuard() {
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
		glScissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);
		setCap(GL_BLEND, blend);
		setCap(GL_DEPTH_TEST, depth);
		setCap(GL_SCISSOR_TEST, scissor);
		setCap(GL_CULL_FACE, cull);
		glBlendFuncSeparate(srcRGB, dstRGB, srcA, dstA);
		glBlendEquationSeparate(eqRGB, eqA);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, tex1);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex0);
		glActiveTexture(activeTexture);
	}

	static void setCap(GLenum cap, GLboolean on) {
		if (on) glEnable(cap);
		else glDisable(cap);
	}
};

glm::mat4 pixelProjection(int w, int h) {
	// Pixel coordinates (row 0 = top of the canvas) straight to clip space.
	// Rendering this way puts pixel row 0 in texture row 0, which is also how
	// openFrameworks lays out fbo textures.
	glm::mat4 proj(1.0f);
	proj[0][0] = 2.f / w;
	proj[1][1] = 2.f / h;
	proj[3][0] = -1.f;
	proj[3][1] = -1.f;
	return proj;
}

std::string randomSeedString(std::random_device & rd) {
	return jsNumberToString(rd() / 4294967296.0);
}

} // namespace

// =============================================================================
// Lifecycle
// =============================================================================

ofxP5Brush::ofxP5Brush() {
	// Unseeded, like Math.random(); call seed() for reproducible drawings.
	std::random_device rd;
	rng.seed(randomSeedString(rd));
	rng2.seed(randomSeedString(rd) + ":2");
	{
		Rng r(randomSeedString(rd));
		noiseGen = SimplexNoise2D(r);
	}
	{
		Rng r(randomSeedString(rd) + ":2");
		noiseGen2 = SimplexNoise2D(r);
	}

	addStandardBrushes();
	addStandardFields();

	// p5.brush flushes pending compositing after every p5 draw(); do the same
	// once the app's draw() has run.
	ofAddListener(ofEvents().draw, this, &ofxP5Brush::onDrawEvent, OF_EVENT_ORDER_AFTER_APP);
	if (!currentInstance) currentInstance = this;
}

ofxP5Brush::~ofxP5Brush() {
	ofRemoveListener(ofEvents().draw, this, &ofxP5Brush::onDrawEvent, OF_EVENT_ORDER_AFTER_APP);
	if (currentInstance == this) currentInstance = nullptr;
}

void ofxP5Brush::setup(int width, int height, float density) {
	if (ready) flushActiveComposite();

	canvasWidth = static_cast<float>(std::max(1, width));
	canvasHeight = static_cast<float>(std::max(1, height));
	canvasDensity = density > 0.f ? density : 1.f;
	Cwidth = canvasWidth;
	Cheight = canvasHeight;
	Density = canvasDensity;

	ofFboSettings settings;
	settings.width = pixelWidth();
	settings.height = pixelHeight();
	settings.internalformat = GL_RGBA;
	settings.textureTarget = GL_TEXTURE_2D;
	settings.useDepth = false;
	settings.useStencil = false;
	settings.numSamples = 0;
	settings.minFilter = GL_LINEAR;
	settings.maxFilter = GL_LINEAR;
	settings.wrapModeHorizontal = GL_CLAMP_TO_EDGE;
	settings.wrapModeVertical = GL_CLAMP_TO_EDGE;
	canvasFbo.allocate(settings);

	targetFbo = &canvasFbo;
	ready = true;
	ensureResources();
	resetCompositeState();
	clearFbo(canvasFbo.getId(), pixelWidth(), pixelHeight(), ofFloatColor(0, 0, 0, 0));
	currentInstance = this;
}

void ofxP5Brush::load(ofFbo & fbo, float density) {
	if (!fbo.isAllocated()) {
		ofLogError("ofxP5Brush") << "load(): the fbo must be allocated first";
		return;
	}
	if (ready) flushActiveComposite();
	targetFbo = &fbo;
	Density = density > 0.f ? density : 1.f;
	Cwidth = fbo.getWidth() / Density;
	Cheight = fbo.getHeight() / Density;
	ready = true;
	ensureResources();
	resetCompositeState();
}

void ofxP5Brush::load() {
	if (!canvasFbo.isAllocated()) {
		setup(ofGetWidth(), ofGetHeight());
		return;
	}
	if (ready) flushActiveComposite();
	targetFbo = &canvasFbo;
	Cwidth = canvasWidth;
	Cheight = canvasHeight;
	Density = canvasDensity;
	ready = true;
	ensureResources();
	resetCompositeState();
}

void ofxP5Brush::isCanvasReady() {
	if (!ready) setup(std::max(1, ofGetWidth()), std::max(1, ofGetHeight()));
}

ofFbo & ofxP5Brush::getTarget() {
	isCanvasReady();
	return *targetFbo;
}

int ofxP5Brush::pixelWidth() const {
	return std::max(1, static_cast<int>(std::round(Cwidth * Density)));
}

int ofxP5Brush::pixelHeight() const {
	return std::max(1, static_cast<int>(std::round(Cheight * Density)));
}

GLuint ofxP5Brush::targetFboId() const {
	return targetFbo ? targetFbo->getId() : 0;
}

void ofxP5Brush::ensureResources() {
	if (!shadersLoaded && !shadersFailed) {
		const GlslFlavor flavor = detectGlslFlavor();
		const std::string vert = vertexShader(flavor);
		bool ok = stampShader.setupShaderFromSource(GL_VERTEX_SHADER, vert)
			&& stampShader.setupShaderFromSource(GL_FRAGMENT_SHADER, stampFragmentShader(flavor));
		if (ok) {
			stampShader.bindDefaults();
			ok = stampShader.linkProgram();
		}
		ok = ok && blendShader.setupShaderFromSource(GL_VERTEX_SHADER, vert)
			&& blendShader.setupShaderFromSource(GL_FRAGMENT_SHADER, blendFragmentShader(flavor));
		if (ok) {
			blendShader.bindDefaults();
			ok = blendShader.linkProgram();
		}
		if (ok) {
			shadersLoaded = true;
		} else {
			shadersFailed = true;
			ofLogError("ofxP5Brush") << "could not compile the brush shaders; nothing will be drawn";
		}
	}
	allocateMasks();
}

void ofxP5Brush::allocateMasks() {
	const int pw = pixelWidth();
	const int ph = pixelHeight();
	if (pw == maskWidth && ph == maskHeight && strokeMaskFbo.isAllocated()) return;
	maskWidth = pw;
	maskHeight = ph;

	ofFboSettings settings;
	settings.width = pw;
	settings.height = ph;
	settings.internalformat = GL_RGBA;
	settings.textureTarget = GL_TEXTURE_2D;
	settings.useDepth = false;
	settings.useStencil = false;
	settings.numSamples = 0;
	settings.minFilter = GL_NEAREST;
	settings.maxFilter = GL_NEAREST;
	settings.wrapModeHorizontal = GL_CLAMP_TO_EDGE;
	settings.wrapModeVertical = GL_CLAMP_TO_EDGE;
	strokeMaskFbo.allocate(settings);
	clearFbo(strokeMaskFbo.getId(), pw, ph, ofFloatColor(0, 0, 0, 0));

	for (ofTexture * tex : {&sourceTex, &fillMaskTex}) {
		tex->allocate(pw, ph, GL_RGBA, false);
		tex->setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
		tex->setTextureWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
	}
	std::vector<unsigned char> zeros(static_cast<size_t>(pw) * ph * 4, 0);
	fillMaskTex.loadData(zeros.data(), pw, ph, GL_RGBA);

	fillMask.assign(static_cast<size_t>(pw) * ph, 0.f);
	strokeMaskInfo = MaskInfo();
	fillMaskInfo = MaskInfo();
}

void ofxP5Brush::clearFbo(GLuint fboId, int w, int h, const ofFloatColor & color) {
	GLStateGuard guard;
	GLfloat previousClear[4];
	glGetFloatv(GL_COLOR_CLEAR_VALUE, previousClear);
	glBindFramebuffer(GL_FRAMEBUFFER, fboId);
	glViewport(0, 0, w, h);
	glDisable(GL_SCISSOR_TEST);
	glClearColor(color.r, color.g, color.b, color.a);
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(previousClear[0], previousClear[1], previousClear[2], previousClear[3]);
}

ofxP5Brush::Affine ofxP5Brush::getAffineMatrix() const {
	// The brush follows the openFrameworks model matrix (ofTranslate, ofRotate,
	// ofScale), independent of the view / camera currently set up.
	const glm::mat4 modelView = ofGetCurrentMatrix(OF_MATRIX_MODELVIEW);
	const glm::mat4 view = ofGetCurrentViewMatrix();
	const glm::mat4 model = glm::inverse(view) * modelView;
	Affine m;
	m.a = model[0][0];
	m.b = model[0][1];
	m.c = model[1][0];
	m.d = model[1][1];
	m.x = model[3][0];
	m.y = model[3][1];
	if (beginDepth > 0 && Density != 1.f) {
		// begin() scales by the density so native drawing uses brush units;
		// the brush applies the density itself.
		m.a /= Density;
		m.b /= Density;
		m.c /= Density;
		m.d /= Density;
		m.x /= Density;
		m.y /= Density;
	}
	return m;
}

// =============================================================================
// Frame helpers
// =============================================================================

void ofxP5Brush::render() {
	flushActiveComposite();
}

void ofxP5Brush::clear() {
	isCanvasReady();
	resetCompositeState();
	clearFbo(targetFboId(), pixelWidth(), pixelHeight(), ofFloatColor(0, 0, 0, 0));
}

void ofxP5Brush::clear(const Color & color) {
	isCanvasReady();
	resetCompositeState();
	clearFbo(targetFboId(), pixelWidth(), pixelHeight(), ofFloatColor(color.r, color.g, color.b, 1.f));
}

void ofxP5Brush::draw(float x, float y) {
	draw(x, y, Cwidth, Cheight);
}

void ofxP5Brush::draw(float x, float y, float w, float h) {
	isCanvasReady();
	render();
	targetFbo->draw(x, y, w, h);
}

void ofxP5Brush::begin() {
	isCanvasReady();
	render();
	targetFbo->begin();
	ofPushMatrix();
	ofScale(Density, Density);
	++beginDepth;
}

void ofxP5Brush::end() {
	if (beginDepth <= 0) return;
	--beginDepth;
	ofPopMatrix();
	targetFbo->end();
}

void ofxP5Brush::onDrawEvent(ofEventArgs &) {
	if (ready) flushActiveComposite();
}

// =============================================================================
// Push / pop, transforms and angle units
// =============================================================================

void ofxP5Brush::push() {
	ofPushMatrix();
	stateStack.push_back({strokeState, fillState, washState, hatchState, massState, fieldState});
}

void ofxP5Brush::pop() {
	ofPopMatrix();
	if (stateStack.empty()) return;
	const SavedState & saved = stateStack.back();
	strokeState = saved.stroke;
	fillState = saved.fill;
	washState = saved.wash;
	hatchState = saved.hatch;
	massState = saved.mass;
	fieldState = saved.field;
	stateStack.pop_back();
}

void ofxP5Brush::rotate(double angle) {
	if (usesRadians()) ofRotateRad(angle);
	else ofRotateDeg(angle);
}

double ofxP5Brush::toDegrees(double angle, bool isRad) const {
	if (isRad || usesRadians()) {
		const double deg = std::fmod((angle * 180.0) / PI, 360.0);
		return deg < 0 ? deg + 360 : deg;
	}
	return angle;
}

double ofxP5Brush::toDegreesSigned(double angle, bool isRad) const {
	return (isRad || usesRadians()) ? (angle * 180.0) / PI : angle;
}

double ofxP5Brush::calcAngle(double x1, double y1, double x2, double y2) const {
	return toDegrees(std::atan2(-(y2 - y1), x2 - x1), true);
}

// =============================================================================
// Seeding and randomness
// =============================================================================

void ofxP5Brush::seed(double s) {
	seed(jsNumberToString(s));
}

void ofxP5Brush::seed(const std::string & s) {
	rng.seed(s);
	rng2.seed(s + ":2");
	gaussCached = false;
	onSeed();
}

void ofxP5Brush::noiseSeed(double s) {
	noiseSeed(jsNumberToString(s));
}

void ofxP5Brush::noiseSeed(const std::string & s) {
	Rng r(s);
	noiseGen = SimplexNoise2D(r);
	Rng r2(s + ":2");
	noiseGen2 = SimplexNoise2D(r2);
}

double ofxP5Brush::random() {
	return rr2(0, 1);
}

double ofxP5Brush::random(double max) {
	return rng2() * max;
}

double ofxP5Brush::random(double min, double max) {
	return rr2(min, max);
}

double ofxP5Brush::gaussian(double mean, double stdev) {
	// Box-Muller with a cached second value, using the degree lookup tables.
	if (gaussCached) {
		gaussCached = false;
		return gaussZ1 * stdev + mean;
	}
	const double u = 1 - rng();
	const double v = rng();
	const double r = std::sqrt(-2.0 * std::log(u));
	const double angle = 360 * v;
	gaussZ1 = r * sinDeg(angle);
	gaussCached = true;
	return r * cosDeg(angle) * stdev + mean;
}

void ofxP5Brush::onSeed() {
	// Same order as p5.brush's seed callbacks: stroke pool, then fill pools.
	strokeGaussians.clear();
	fillGaussianPools();
}

void ofxP5Brush::fillGaussianPools() {
	const int poolSize = 512;
	fillGaussA.resize(poolSize);
	fillGaussB.resize(poolSize);
	for (int i = 0; i < poolSize; ++i) {
		fillGaussA[i] = gaussian(0.5, 0.2);
		fillGaussB[i] = gaussian(0, 0.02);
	}
}

// =============================================================================
// Compositor (p5.brush's Mix)
// =============================================================================

std::optional<ofxP5Brush::DirtyRect> ofxP5Brush::normalizeDirtyRect(const DirtyRect & rect) const {
	const double w = pixelWidth();
	const double h = pixelHeight();
	const double minX = std::max(0.0, std::floor(std::min(rect.minX, rect.maxX)));
	const double minY = std::max(0.0, std::floor(std::min(rect.minY, rect.maxY)));
	const double maxX = std::min(w, std::ceil(std::max(rect.minX, rect.maxX)));
	const double maxY = std::min(h, std::ceil(std::max(rect.minY, rect.maxY)));
	if (!(maxX > minX) || !(maxY > minY)) return std::nullopt;
	return DirtyRect {minX, minY, maxX, maxY};
}

void ofxP5Brush::markDirtyRect(MaskInfo & mask, const DirtyRect & rect) {
	const auto normalized = normalizeDirtyRect(rect);
	if (!normalized) return;
	if (!mask.dirty) {
		mask.dirty = normalized;
	} else {
		mask.dirty->minX = std::min(mask.dirty->minX, normalized->minX);
		mask.dirty->minY = std::min(mask.dirty->minY, normalized->minY);
		mask.dirty->maxX = std::max(mask.dirty->maxX, normalized->maxX);
		mask.dirty->maxY = std::max(mask.dirty->maxY, normalized->maxY);
	}
	mask.isDrawn = true;
}

void ofxP5Brush::clearMask(bool brushMask) {
	if (brushMask) {
		if (strokeMaskFbo.isAllocated()) clearFbo(strokeMaskFbo.getId(), maskWidth, maskHeight, ofFloatColor(0, 0, 0, 0));
		strokeMaskInfo = MaskInfo();
		return;
	}
	if (!fillMask.empty() && fillMaskInfo.isDrawn) {
		if (fillMaskInfo.dirty) {
			const int x0 = static_cast<int>(fillMaskInfo.dirty->minX);
			const int y0 = static_cast<int>(fillMaskInfo.dirty->minY);
			const int x1 = static_cast<int>(fillMaskInfo.dirty->maxX);
			const int y1 = static_cast<int>(fillMaskInfo.dirty->maxY);
			for (int y = y0; y < y1; ++y) {
				std::fill(fillMask.begin() + static_cast<size_t>(y) * maskWidth + x0,
					fillMask.begin() + static_cast<size_t>(y) * maskWidth + x1, 0.f);
			}
		} else {
			std::fill(fillMask.begin(), fillMask.end(), 0.f);
		}
	}
	fillMaskInfo = MaskInfo();
}

void ofxP5Brush::blend(const Color * color, bool isLast) {
	isCanvasReady();
	// Only one mask is "active" for the current drawing mode; the other one
	// may still need flushing if the mode just changed.
	const bool isBrushMask = mixIsBrush == 1;
	std::optional<std::array<float, 4>> nextColor;
	if (color) nextColor = std::array<float, 4> {color->r, color->g, color->b, color->a};
	const bool colorChanged = nextColor && (!cachedColor || *cachedColor != *nextColor);

	if (!mixIsBlending && nextColor) {
		mixIsBlending = true;
		cachedColor = nextColor;
		// Reset the brush mask at the start of each blend cycle so stale
		// bookkeeping cannot leak an old stroke into the next color.
		clearMask(true);
	}

	if (isLast || colorChanged) {
		if (mixJustChanged) {
			applyShader(!isBrushMask);
			mixJustChanged = false;
		}
		if (mixIsBlending) applyShader(isBrushMask);
		if (nextColor) cachedColor = nextColor;
		if (isLast) {
			mixIsBlending = false;
			cachedColor.reset();
		}
	}
}

void ofxP5Brush::applyShader(bool brushMask) {
	MaskInfo & info = brushMask ? strokeMaskInfo : fillMaskInfo;
	if (!info.isDrawn) return;

	const int pw = pixelWidth();
	const int ph = pixelHeight();
	std::optional<DirtyRect> rect;
	if (!info.dirty) {
		rect = DirtyRect {0, 0, static_cast<double>(pw), static_cast<double>(ph)};
	} else {
		const double pad = brushMask ? 2 : 4;
		rect = normalizeDirtyRect({info.dirty->minX - pad, info.dirty->minY - pad, info.dirty->maxX + pad,
			info.dirty->maxY + pad});
	}
	if (!rect || !shadersLoaded || !cachedColor || !targetFbo) {
		clearMask(brushMask);
		return;
	}

	const int x = static_cast<int>(rect->minX);
	const int y = static_cast<int>(rect->minY);
	const int w = static_cast<int>(rect->maxX) - x;
	const int h = static_cast<int>(rect->maxY) - y;
	const GLuint sourceId = sourceTex.getTextureData().textureID;
	GLuint maskId = 0;

	{
		GLStateGuard guard;
		glBindFramebuffer(GL_FRAMEBUFFER, targetFboId());
		glViewport(0, 0, pw, ph);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_BLEND);
		glDisable(GL_SCISSOR_TEST);

		// 1. Snapshot the target region the shader reads from.
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, sourceId);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, x, y, x, y, w, h);

		// 2. Resolve the mask texture.
		if (brushMask) {
			maskId = strokeMaskFbo.getTexture().getTextureData().textureID;
		} else {
			// Upload the dirty part of the CPU fill mask, plus the 2 px the
			// edge-blur taps reach beyond the composited region.
			const int ux0 = std::max(0, x - 2);
			const int uy0 = std::max(0, y - 2);
			const int ux1 = std::min(pw, x + w + 2);
			const int uy1 = std::min(ph, y + h + 2);
			const int uw = ux1 - ux0;
			const int uh = uy1 - uy0;
			uploadBuffer.resize(static_cast<size_t>(uw) * uh * 4);
			for (int j = 0; j < uh; ++j) {
				const float * src = fillMask.data() + static_cast<size_t>(uy0 + j) * maskWidth + ux0;
				unsigned char * dst = uploadBuffer.data() + static_cast<size_t>(j) * uw * 4;
				for (int i = 0; i < uw; ++i) {
					const float a = std::min(1.f, std::max(0.f, src[i]));
					dst[i * 4 + 0] = 255;
					dst[i * 4 + 1] = 0;
					dst[i * 4 + 2] = 0;
					dst[i * 4 + 3] = static_cast<unsigned char>(std::lround(a * 255.f));
				}
			}
			maskId = fillMaskTex.getTextureData().textureID;
			GLint previousAlignment = 4;
			glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
			glBindTexture(GL_TEXTURE_2D, maskId);
			glTexSubImage2D(GL_TEXTURE_2D, 0, ux0, uy0, uw, uh, GL_RGBA, GL_UNSIGNED_BYTE, uploadBuffer.data());
			glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);
		}
		glBindTexture(GL_TEXTURE_2D, 0);

		// 3. Composite: one pixel-space quad, scissored to the dirty rect.
		glEnable(GL_SCISSOR_TEST);
		glScissor(x, y, w, h);
		const glm::vec3 quad[6] = {{0, 0, 0}, {static_cast<float>(pw), 0, 0},
			{static_cast<float>(pw), static_cast<float>(ph), 0}, {0, 0, 0},
			{static_cast<float>(pw), static_cast<float>(ph), 0}, {0, static_cast<float>(ph), 0}};
		quadVbo.setVertexData(quad, 6, GL_DYNAMIC_DRAW);

		const auto & c = *cachedColor;
		blendShader.begin();
		blendShader.setUniformTexture("u_source", GL_TEXTURE_2D, sourceId, 0);
		blendShader.setUniformTexture("u_mask", GL_TEXTURE_2D, maskId, 1);
		blendShader.setUniform3f("u_color", c[0], c[1], c[2]);
		blendShader.setUniform1i("u_isBrush", brushMask ? 1 : 0);
		blendShader.setUniform2f("u_texel", 1.f / pw, 1.f / ph);
		blendShader.setUniformMatrix4f("u_proj", pixelProjection(pw, ph));
		quadVbo.draw(GL_TRIANGLES, 0, 6);
		blendShader.end();
	}

	clearMask(brushMask);
}

void ofxP5Brush::flushActiveComposite() {
	if (!ready) return;
	blend(nullptr, true);
	clearMask(true);
	clearMask(false);
	mixJustChanged = false;
	mixIsBlending = false;
	mixIsBrush = -1;
	cachedColor.reset();
}

void ofxP5Brush::resetCompositeState() {
	circlePos.clear();
	circleCorner.clear();
	circleDirty.reset();
	imgPos.clear();
	imgCorner.clear();
	imgDirty.reset();
	clearMask(true);
	clearMask(false);
	mixJustChanged = false;
	mixIsBlending = false;
	mixIsBrush = -1;
	cachedColor.reset();
}
