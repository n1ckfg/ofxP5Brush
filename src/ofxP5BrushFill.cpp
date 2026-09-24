// Watercolor fill and wash (port of p5.brush fill/fill.js, fill/wash.js and
// fill/mask.js). The watercolor technique follows Tyler Hobbs' essay:
// https://tylerxhobbs.com/essays/2017/a-generative-approach-to-simulating-watercolor-paints

#include "ofxP5Brush.h"

using namespace ofxP5BrushDetail;

namespace {
const double GROW_MAX_VERTS = 2024;
} // namespace

// =============================================================================
// Fill state
// =============================================================================

void ofxP5Brush::fill(const Color & color, double opacity) {
	fillState.opacity = opacity;
	fillState.color = color;
	fillState.hasColor = true;
	fillState.isActive = true;
}

void ofxP5Brush::fillBleed(double strength, const std::string & direction, std::optional<double> angle) {
	fillState.bleedStrength = constrain(strength, 0, 1);
	fillState.direction = direction;
	fillState.angle = angle ? std::optional<double>(toDegreesSigned(*angle)) : std::nullopt;
}

void ofxP5Brush::fillTexture(double texture, double border, bool scatter) {
	fillState.textureStrength = constrain(texture, 0, 1);
	fillState.borderStrength = constrain(border, 0, 1);
	fillState.scatter = scatter;
}

void ofxP5Brush::noFill() {
	fillState.isActive = false;
}

void ofxP5Brush::wash(const Color & color, double opacity) {
	washState.opacity = opacity;
	washState.color = color;
	washState.hasColor = true;
	washState.isActive = true;
}

void ofxP5Brush::noWash() {
	washState.isActive = false;
}

// =============================================================================
// FillPoly: the growing / deforming polygon behind the watercolor effect
// =============================================================================

struct ofxP5Brush::FillPoly {
	ofxP5Brush * b;
	std::vector<Vec2d> v; // vertices
	std::vector<double> m; // bleed multiplier per vertex
	std::vector<char> dir; // bleed direction per vertex
	Vec2d midP {0, 0};
	double sizeX = 0, sizeY = 0;

	FillPoly(ofxP5Brush * brush, std::vector<Vec2d> verts, std::vector<double> mods, Vec2d center,
		std::vector<char> dirs, bool isFirst, double sx = 0, double sy = 0)
		: b(brush)
		, v(std::move(verts))
		, m(std::move(mods))
		, dir(std::move(dirs))
		, midP(center) {
		if (!isFirst) {
			sizeX = sx;
			sizeY = sy;
			return;
		}
		const size_t n = v.size();
		double maxX = 0, maxY = 0;
		struct Ray {
			Vec2d v1, v2, p1, p2;
		};
		std::vector<Ray> rays;
		rays.reserve(n);
		for (size_t i = 0; i < n; ++i) {
			maxX = std::max(maxX, std::abs(center.x - v[i].x));
			maxY = std::max(maxY, std::abs(center.y - v[i].y));
			const Vec2d v1 = v[i];
			const Vec2d v2 = v[(i + 1) % n];
			const Vec2d side {v2.x - v1.x, v2.y - v1.y};
			// utils.rotate(0, 0, side.x, side.y, 90) with the lookup tables
			double c, s;
			cosSinDeg(90, c, s);
			const Vec2d rt {c * side.x + s * side.y, c * side.y - s * side.x};
			const Vec2d mid {v1.x + side.x / 2, v1.y + side.y / 2};
			rays.push_back({v1, v2, mid, {mid.x + rt.x, mid.y + rt.y}});
		}
		sizeX = maxX;
		sizeY = maxY;

		// Bleed direction per edge: cast a ray from the edge midpoint along its
		// normal and count crossings with the original polygon.
		const auto & poly = b->fillPolygon->vertices;
		const size_t np = poly.size();
		dir.assign(n, 0);
		for (size_t i = 0; i < n; ++i) {
			const Ray & rc = rays[i];
			const double r1x = rc.p1.x, r1y = rc.p1.y;
			const double rdx = rc.p2.x - r1x, rdy = rc.p2.y - r1y;
			const double eSx = rc.v2.x - rc.v1.x, eSy = rc.v2.y - rc.v1.y;
			const double eNeg = -(eSx * eSx + eSy * eSy);
			int count = 0;
			for (size_t j = 0; j < np; ++j) {
				const glm::dvec2 & sa = poly[j];
				const glm::dvec2 & sb = poly[(j + 1) % np];
				const double sdx = sb.x - sa.x, sdy = sb.y - sa.y;
				const double denom = sdy * rdx - sdx * rdy;
				if (denom == 0) continue;
				const double ub = (rdx * (r1y - sa.y) - rdy * (r1x - sa.x)) / denom;
				if (ub < 0 || ub > 1) continue;
				const double ua = (sdx * (r1y - sa.y) - sdy * (r1x - sa.x)) / denom;
				if (ua * eNeg <= 0.01) continue;
				count++;
			}
			dir[i] = count % 2 == 0;
		}

		// Randomize the centre
		const double rx = b->rr(-0.6, 0.6) * maxX;
		const double ry = b->rr(-0.6, 0.6) * maxY;
		midP = {center.x + rx, center.y + ry};
	}

	struct Trimmed {
		std::vector<Vec2d> v;
		std::vector<double> m;
		std::vector<char> dir;
	};

	/// Removes a run of vertices and bridges the gap with a few jittered ones.
	Trimmed trim(double f) const {
		if (f >= 1 || f < 0 || v.size() <= 8) return {v, m, dir};

		const int totalN = static_cast<int>(v.size());
		const int nTrim = toInt32((1 - f) * totalN);
		const int s = toInt32(totalN / 2.0 - nTrim / 2.0);
		const int trimStart = s, trimEnd = s + nTrim;
		const Vec2d eStart = v[(trimStart - 1 + totalN) % totalN];
		const Vec2d eEnd = v[trimEnd % totalN];
		const double evx = eEnd.x - eStart.x, evy = eEnd.y - eStart.y;
		const double edgeLen = std::hypot(evx, evy);

		// Typical vertex spacing from one random kept pair, then insert at
		// least 0.2x that density along the bridge.
		const int sampleIdx = s >= 2 ? toInt32(b->rr(0, s - 1)) : (trimEnd < totalN - 1 ? trimEnd : 0);
		const Vec2d sa = v[sampleIdx], sb = v[(sampleIdx + 1) % totalN];
		const double typicalSpacing = std::max(1.0, std::hypot(sb.x - sa.x, sb.y - sa.y));
		const int nInsert = std::max(2, static_cast<int>(std::ceil(edgeLen / typicalSpacing * 0.05)));

		Trimmed out;
		const size_t finalLen = static_cast<size_t>(totalN - nTrim + nInsert);
		out.v.reserve(finalLen);
		out.m.reserve(finalLen);
		out.dir.reserve(finalLen);
		for (int i = 0; i < s; ++i) {
			out.v.push_back(v[i]);
			out.m.push_back(m[i]);
			out.dir.push_back(dir[i]);
		}
		const double jitterAmt = edgeLen * 0.06;
		const char dirBase = dir[trimStart % dir.size()];
		for (int k = 0; k < nInsert; ++k) {
			const double t = (k + 1.0) / (nInsert + 1.0);
			const double jx = b->rr(-jitterAmt, jitterAmt);
			const double jy = b->rr(-jitterAmt, jitterAmt);
			out.v.push_back({eStart.x + evx * t + jx, eStart.y + evy * t + jy});
			out.m.push_back(b->rr(0.3, 0.5));
			out.dir.push_back(dirBase);
		}
		for (int i = trimEnd; i < totalN; ++i) {
			out.v.push_back(v[i]);
			out.m.push_back(m[i]);
			out.dir.push_back(dir[i]);
		}
		return out;
	}

	/// Keeps a fraction of the vertices in order; ones outside the original
	/// polygon are pulled towards the centre.
	FillPoly scatter(double ratio) const {
		const size_t L = v.size();
		if (L == 0) return *this;
		const int keep = std::max(3, toInt32(L * ratio));
		const double step = static_cast<double>(L) / keep;
		const double stepRand = step * 0.8;
		const auto & sides = b->fillPolygon->vertices;
		const size_t np = sides.size();

		std::vector<Vec2d> sv;
		std::vector<double> sm;
		std::vector<char> sd;
		for (int i = 0; i < keep; ++i) {
			const int j = toInt32(i * step + b->rr(0, stepRand)) % static_cast<int>(L);
			Vec2d p = v[j];
			bool outside = false;
			if (p.x < b->fillBBMinX || p.x > b->fillBBMaxX || p.y < b->fillBBMinY || p.y > b->fillBBMaxY) {
				outside = true;
			} else {
				int crossings = 0;
				for (size_t k = 0; k < np; ++k) {
					const glm::dvec2 & a = sides[k];
					const glm::dvec2 & c = sides[(k + 1) % np];
					if ((a.y > p.y) == (c.y > p.y)) continue;
					const double t = (p.y - a.y) / (c.y - a.y);
					if (p.x < a.x + t * (c.x - a.x)) crossings++;
				}
				outside = crossings % 2 == 0;
			}
			if (outside) {
				const double fx = b->rr(0.3, 0.6);
				const double fy = b->rr(0.3, 0.6);
				p = {midP.x + (p.x - midP.x) * fx, midP.y + (p.y - midP.y) * fy};
			}
			sv.push_back(p);
			sm.push_back(m[j]);
			sd.push_back(!dir[j]);
		}
		return FillPoly(b, std::move(sv), std::move(sm), midP, std::move(sd), false, sizeX, sizeY);
	}

	FillPoly flipDirs() const {
		std::vector<char> flipped(dir.size());
		for (size_t i = 0; i < dir.size(); ++i) flipped[i] = !dir[i];
		return FillPoly(b, v, m, midP, std::move(flipped), false, sizeX, sizeY);
	}

	/// Inserts a displaced midpoint on every edge (or trims first, for f < 1).
	FillPoly grow(double f = 1) const {
		Trimmed tr = trim(f);
		const size_t len = tr.v.size();
		const double bleedDirDeg = b->fillState.direction == "out" ? -90 : 90;

		if (b->fillGaussA.empty()) b->fillGaussianPools();
		const auto & gPool = b->fillGaussA;
		const auto & g2Pool = b->fillGaussB;
		const double gLen = static_cast<double>(gPool.size());
		const double g2Len = static_cast<double>(g2Pool.size());

		double mod = f == 999 ? b->rr(0.6, 0.8) : b->fillState.bleedStrength;
		const double cap = b->growCap;

		// If the vertex cap will keep only every other vertex, the inserted
		// ones are all discarded: skip the math but keep the RNG sequence.
		const double preStep = cap > 0 && len * 2 > cap ? std::ceil(len * 2 / cap) : 1;
		const bool skipInserted = preStep >= 2 && (toInt32(preStep) & 1) == 0;
		if (skipInserted) {
			for (size_t i = 0; i < len; ++i) {
				if (f < 997) mod = tr.m[i];
				if (mod >= 0.05) {
					b->rr(-1, 1);
					b->rr(0, 1);
					b->rr(0.65, 1.35);
					b->rr(0, 1);
				}
			}
			return FillPoly(b, std::move(tr.v), std::move(tr.m), midP, std::move(tr.dir), false, sizeX, sizeY);
		}

		std::vector<Vec2d> inserted(len);
		std::vector<double> newMods(len * 2);
		std::vector<char> newDirs(len * 2);
		size_t idx = 0;
		for (size_t i = 0; i < len; ++i) {
			const Vec2d cv = tr.v[i];
			const Vec2d nv = tr.v[i + 1 < len ? i + 1 : 0];
			const double mi = tr.m[i];
			const char di = tr.dir[i];
			if (f < 997) mod = mi;

			if (mod < 0.05) {
				newMods[idx] = mi;
				newDirs[idx] = di;
				idx++;
				inserted[i] = {(cv.x + nv.x) / 2, (cv.y + nv.y) / 2};
				newMods[idx] = mi;
				newDirs[idx] = di;
				idx++;
				continue;
			}

			const double rotDeg = (di ? bleedDirDeg : -bleedDirDeg) + b->rr(-1, 1) * 5;
			double c, s;
			cosSinDeg(rotDeg, c, s);
			const double sideX = nv.x - cv.x;
			const double sideY = nv.y - cv.y;
			const double dirX = c * sideX + s * sideY;
			const double dirY = c * sideY - s * sideX;

			const double g = gPool[toInt32(b->rr(0, 1) * gLen)];
			const double d = g * b->rr(0.65, 1.35) * mod;
			const double nextMod = mi + g2Pool[toInt32(b->rr(0, 1) * g2Len)];

			newMods[idx] = mi;
			newDirs[idx] = di;
			idx++;
			inserted[i] = {cv.x + sideX * 0.5 + dirX * d, cv.y + sideY * 0.5 + dirY * d};
			newMods[idx] = nextMod;
			newDirs[idx] = di;
			idx++;
		}

		std::vector<Vec2d> fv;
		std::vector<double> fm;
		std::vector<char> fd;
		const size_t step = cap > 0 && idx > cap ? static_cast<size_t>(std::ceil(idx / cap)) : 1;
		fv.reserve(idx / step + 1);
		fm.reserve(idx / step + 1);
		fd.reserve(idx / step + 1);
		for (size_t j = 0; j < idx; j += step) {
			fv.push_back(j % 2 == 0 ? tr.v[j >> 1] : inserted[j >> 1]);
			fm.push_back(newMods[j]);
			fd.push_back(newDirs[j]);
		}
		return FillPoly(b, std::move(fv), std::move(fm), midP, std::move(fd), false, sizeX, sizeY);
	}

	/// Draws many translucent, growing layers and erodes them with texture.
	void fill(const Color & color, double intensity, double tex) {
		const int numLayers = 20;
		const double texture = tex * 3;
		const double layerIntensity = 2 * intensity * (1 + tex / 2);

		const bool switchingToFill = b->mixIsBrush != 0;
		b->mixIsBrush = 0;
		if (switchingToFill) b->mixJustChanged = true;
		b->blend(&color);

		const Affine m0 = b->getAffineMatrix();
		const double D = b->Density;
		b->fillMatrix = {D * m0.a, D * m0.b, D * m0.c, D * m0.d, D * m0.x, D * m0.y};
		b->growCap = GROW_MAX_VERTS * std::max(0.2, 2 * b->fillState.bleedStrength);

		const double size = std::max(sizeX, sizeY);
		const double darker = b->rr(0.15, 0.7);
		FillPoly pol = grow();
		const FillPoly sparse = scatter(0.1).grow().scatter(0.75).flipDirs();
		std::vector<FillPoly> pols;

		for (int i = 0; i < numLayers; ++i) {
			if (i % 4 == 0) pol = pol.grow();

			if (i % 2 == 0) {
				pols.clear();
				pols.push_back(pol.grow(1 - 0.0125 * i));
				pols.push_back(pol.grow(0.7 - 0.0125 * i));
				pols.push_back(pol.grow(0.4 - 0.0125 * i));
			}

			for (const auto & p : pols) {
				p.grow(999).grow(997).layer(i, size, layerIntensity);
			}
			if (b->fillState.scatter) {
				sparse.grow(999).flipDirs().grow(997).layer(i, size, layerIntensity * texture);
			}
			if (i % 2 == 0) {
				pol.grow(darker).grow(999).layer(i, size, layerIntensity * 2);
			}

			if (i % 8 == 0 || i == numLayers - 1) {
				if (texture != 0) pol.erase(texture * 3, intensity);
				b->blend(&color, true);
			}
		}
	}

	/// One layer: a faint fill plus an even fainter border stroke.
	void layer(int i, double size, double intensity) const {
		const double lineWidth = mapRange(i, 0, 24, size / 25, size / 30, true) * b->fillState.borderStrength;
		const float fillAlpha = static_cast<float>(constrain(intensity / 100.0, 0, 1)); // "int%" in p5.brush
		b->rasterPolygon(v, b->fillMatrix, fillAlpha);
		const float strokeAlpha = static_cast<float>(constrain(b->fillState.borderStrength * 0.010, 0, 1));
		if (strokeAlpha > 0 && lineWidth > 0) b->rasterStroke(v, b->fillMatrix, lineWidth, strokeAlpha);
	}

	/// Punches soft holes in the accumulated mask for a paper-like texture.
	void erase(double texture, double intensity) const {
		const int numCircles = toInt32(b->rr(80, 110) * mapRange(texture, 0, 1, 2, 3.5));
		const double halfSizeX = sizeX / 1.3;
		const double halfSizeY = sizeY / 1.3;
		const double minSize = std::min(sizeX, sizeY) * 1.3;
		const double minSizeFactor = 0.03 * minSize;
		const double maxSizeFactor = 0.45 * minSize;
		const float alpha = static_cast<float>(
			constrain(((5 - mapRange(intensity, 80, 100, 0.3, 0.7, true)) * texture) / 255, 0, 1));
		for (int i = 0; i < numCircles; ++i) {
			const double x = midP.x + b->gaussian(0, halfSizeX);
			const double y = midP.y + b->gaussian(0, halfSizeY);
			const double diameter = b->rr(minSizeFactor, maxSizeFactor);
			if (i % 5 != 0) b->rasterCircleErase(x, y, diameter / 2, b->fillMatrix, alpha);
		}
	}
};

// =============================================================================
// createFill
// =============================================================================

void ofxP5Brush::createFill(const Polygon & polygon) {
	if (!fillState.isActive || !fillState.hasColor) {
		ofLogError("ofxP5Brush") << "no fill color set. Call fill(color) before drawing shapes";
		return;
	}
	const size_t n = polygon.vertices.size();
	if (n == 0) return;

	fillPolygon = &polygon;
	fillBBMinX = fillBBMinY = std::numeric_limits<double>::infinity();
	fillBBMaxX = fillBBMaxY = -std::numeric_limits<double>::infinity();
	std::vector<Vec2d> v;
	v.reserve(n);
	for (const auto & p : polygon.vertices) {
		fillBBMinX = std::min(fillBBMinX, p.x);
		fillBBMaxX = std::max(fillBBMaxX, p.x);
		fillBBMinY = std::min(fillBBMinY, p.y);
		fillBBMaxY = std::max(fillBBMaxY, p.y);
		v.push_back({p.x, p.y});
	}

	const double wr = rr(0, 75);
	const int fluid = toInt32(n * 0.25 * (wr < 5 ? 1 : wr < 15 ? 2 : 3));
	const double strength = fillState.bleedStrength;
	std::vector<double> modifiers(n);
	for (size_t i = 0; i < n; ++i) {
		modifiers[i] = (static_cast<int>(i) > fluid ? 1 : 0.3) * rr(0.85, 1.4) * strength;
	}

	size_t shift = 0;
	if (!fillState.angle) {
		shift = static_cast<size_t>(std::max(0, randInt(0, static_cast<double>(n))));
	} else {
		// Start from the vertex furthest "behind" the wash direction.
		double c, s;
		cosSinDeg(*fillState.angle, c, s);
		const double dx = c, dy = -s;
		double bestDot = std::numeric_limits<double>::infinity();
		for (size_t i = 0; i < n; ++i) {
			const double dot = v[i].x * dx + v[i].y * dy;
			if (dot < bestDot) {
				bestDot = dot;
				shift = i;
			}
		}
	}
	std::vector<Vec2d> shifted(n);
	for (size_t i = 0; i < n; ++i) shifted[i] = v[(i + shift) % n];

	// Centroid: plain average for small polygons, shoelace otherwise.
	Vec2d center {0, 0};
	if (n < 8) {
		for (const auto & p : shifted) {
			center.x += p.x;
			center.y += p.y;
		}
		center.x /= n;
		center.y /= n;
	} else {
		double area = 0, cx = 0, cy = 0;
		for (size_t i = 0; i < n; ++i) {
			const size_t j = i + 1 < n ? i + 1 : 0;
			const double cross = shifted[i].x * shifted[j].y - shifted[j].x * shifted[i].y;
			area += cross;
			cx += (shifted[i].x + shifted[j].x) * cross;
			cy += (shifted[i].y + shifted[j].y) * cross;
		}
		area *= 0.5;
		center = area != 0 ? Vec2d {cx / (6 * area), cy / (6 * area)} : shifted[0];
	}

	FillPoly(this, std::move(shifted), std::move(modifiers), center, {}, true)
		.fill(fillState.color, mapRange(fillState.opacity, 0, 255, 0, 1, true), fillState.textureStrength);
	fillPolygon = nullptr;
}

// =============================================================================
// Wash
// =============================================================================

void ofxP5Brush::drawWashPolygon(const Polygon & polygon) {
	if (!washState.isActive || !washState.hasColor || polygon.vertices.size() < 3) return;

	const bool switchingToWash = mixIsBrush != 0;
	mixIsBrush = 0;
	if (switchingToWash) mixJustChanged = true;
	// Composite any pending mask of another color before drawing.
	blend(&washState.color);

	const Affine m0 = getAffineMatrix();
	const double D = Density;
	const Affine m {D * m0.a, D * m0.b, D * m0.c, D * m0.d, D * m0.x, D * m0.y};
	std::vector<Vec2d> v;
	v.reserve(polygon.vertices.size());
	for (const auto & p : polygon.vertices) v.push_back({p.x, p.y});
	rasterPolygon(v, m, static_cast<float>(constrain(washState.opacity / 255, 0, 1)));
}

// =============================================================================
// Mask rasterization (Canvas2D equivalents)
// =============================================================================

void ofxP5Brush::markFillDirty(const PixelRect & r) {
	if (r.empty()) return;
	markDirtyRect(fillMaskInfo, {static_cast<double>(r.x0), static_cast<double>(r.y0), static_cast<double>(r.x1),
									static_cast<double>(r.y1)});
}

void ofxP5Brush::rasterPolygon(const std::vector<Vec2d> & verts, const Affine & m, float alpha) {
	if (verts.size() < 2 || fillMask.empty()) return;
	std::vector<double> xy;
	xy.reserve(verts.size() * 2);
	for (const auto & p : verts) {
		xy.push_back(m.a * p.x + m.c * p.y + m.x);
		xy.push_back(m.b * p.x + m.d * p.y + m.y);
	}
	raster.beginPath();
	raster.addContour(xy);
	markFillDirty(raster.fill(fillMask, maskWidth, maskHeight, alpha));
}

void ofxP5Brush::rasterStroke(const std::vector<Vec2d> & verts, const Affine & m, double lineWidth, float alpha) {
	// Canvas2D stroke of a closed path: the union of one quad per edge plus a
	// miter join (limit 10, bevel beyond) at every vertex. Each piece is
	// oriented the same way so the nonzero fill of all pieces is their union.
	if (fillMask.empty()) return;
	std::vector<Vec2d> pts;
	pts.reserve(verts.size());
	for (const auto & p : verts) {
		if (!std::isfinite(p.x) || !std::isfinite(p.y)) continue;
		if (pts.empty() || p.x != pts.back().x || p.y != pts.back().y) pts.push_back(p);
	}
	while (pts.size() > 1 && pts.back().x == pts.front().x && pts.back().y == pts.front().y) pts.pop_back();
	const size_t n = pts.size();
	if (n < 2) return;

	const double hw = lineWidth / 2;
	const double miterLimit = 10;
	std::vector<double> xy;
	raster.beginPath();
	auto addPiece = [&](std::initializer_list<Vec2d> piece) {
		xy.clear();
		for (const auto & p : piece) {
			xy.push_back(m.a * p.x + m.c * p.y + m.x);
			xy.push_back(m.b * p.x + m.d * p.y + m.y);
		}
		double area = 0;
		const size_t k = xy.size() / 2;
		for (size_t i = 0; i < k; ++i) {
			const size_t j = (i + 1) % k;
			area += xy[2 * i] * xy[2 * j + 1] - xy[2 * j] * xy[2 * i + 1];
		}
		if (area < 0) {
			for (size_t i = 0, j = k - 1; i < j; ++i, --j) {
				std::swap(xy[2 * i], xy[2 * j]);
				std::swap(xy[2 * i + 1], xy[2 * j + 1]);
			}
		}
		if (area != 0) raster.addContour(xy);
	};
	auto unit = [](double x, double y) {
		const double l = std::hypot(x, y);
		return l > 0 ? Vec2d {x / l, y / l} : Vec2d {0, 0};
	};

	// Edges
	for (size_t i = 0; i < n; ++i) {
		const Vec2d & a = pts[i];
		const Vec2d & c = pts[(i + 1) % n];
		const Vec2d d = unit(c.x - a.x, c.y - a.y);
		const Vec2d nrm {-d.y * hw, d.x * hw};
		addPiece({{a.x + nrm.x, a.y + nrm.y}, {c.x + nrm.x, c.y + nrm.y}, {c.x - nrm.x, c.y - nrm.y},
			{a.x - nrm.x, a.y - nrm.y}});
	}
	// Joins
	if (n > 2) {
		for (size_t i = 0; i < n; ++i) {
			const Vec2d & p = pts[i];
			const Vec2d & prev = pts[(i + n - 1) % n];
			const Vec2d & next = pts[(i + 1) % n];
			const Vec2d d0 = unit(p.x - prev.x, p.y - prev.y);
			const Vec2d d1 = unit(next.x - p.x, next.y - p.y);
			const double cross = d0.x * d1.y - d0.y * d1.x;
			const double dot = d0.x * d1.x + d0.y * d1.y;
			if (std::abs(cross) < 1e-12 && dot > 0) continue; // straight through
			const double side = cross > 0 ? -1 : 1; // outer side of the turn
			const Vec2d o0 {p.x - d0.y * hw * side, p.y + d0.x * hw * side};
			const Vec2d o1 {p.x - d1.y * hw * side, p.y + d1.x * hw * side};
			const double halfCos = std::sqrt(std::max(0.0, (1 + dot) / 2)); // sin of half the corner angle
			if (halfCos > 1e-9 && 1 / halfCos <= miterLimit) {
				const Vec2d bis = unit(o0.x + o1.x - 2 * p.x, o0.y + o1.y - 2 * p.y);
				const double reach = hw / halfCos;
				const Vec2d tip {p.x + bis.x * reach, p.y + bis.y * reach};
				addPiece({p, o0, tip, o1});
			} else {
				addPiece({p, o0, o1});
			}
		}
	}
	markFillDirty(raster.fill(fillMask, maskWidth, maskHeight, alpha));
}

void ofxP5Brush::rasterCircleErase(double x, double y, double radius, const Affine & m, float alpha) {
	if (fillMask.empty() || !(radius > 0)) return;
	const double scale = std::sqrt(std::abs(m.a * m.d - m.b * m.c));
	const double pixelRadius = radius * scale;
	const int segments = static_cast<int>(ofClamp(std::ceil(TWO_PI * pixelRadius / 3.0), 16, 128));
	std::vector<double> xy;
	xy.reserve(segments * 2);
	for (int i = 0; i < segments; ++i) {
		const double a = TWO_PI * i / segments;
		const double px = x + radius * std::cos(a);
		const double py = y + radius * std::sin(a);
		xy.push_back(m.a * px + m.c * py + m.x);
		xy.push_back(m.b * px + m.d * py + m.y);
	}
	raster.beginPath();
	raster.addContour(xy);
	raster.fill(fillMask, maskWidth, maskHeight, alpha, true);
}
