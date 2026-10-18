/* Software 2D renderer: anti-aliased primitives, sprites and TrueType text into a 32-bit buffer. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#pragma GCC diagnostic ignored "-Wunused-function"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"
#include "gfx.h"

uint32_t fb[SH][SW];
static uint32_t *tg = &fb[0][0];
static int tw = SW, th = SH;
static int cx0 = 0, cy0 = 0, cx1 = SW, cy1 = SH;
static int scl[4] = {0, 0, SW, SH};  /* the screen's clip while drawing elsewhere */

void gfx_target(uint32_t *buf, int w, int h) {
	if (tg == &fb[0][0] && buf != tg) { scl[0] = cx0; scl[1] = cy0; scl[2] = cx1; scl[3] = cy1; }
	tg = buf; tw = w; th = h; gfx_noclip();
}
/* back to the screen, with the clip it had before gfx_target redirected drawing */
void gfx_screen(void) { gfx_target(&fb[0][0], SW, SH); gfx_clip(scl[0], scl[1], scl[2], scl[3]); }
void gfx_clip(int x0, int y0, int x1, int y1) {
	cx0 = clampi(x0, 0, tw); cy0 = clampi(y0, 0, th);
	cx1 = clampi(x1, 0, tw); cy1 = clampi(y1, 0, th);
}
void gfx_noclip(void) { cx0 = 0; cy0 = 0; cx1 = tw; cy1 = th; }

uint32_t mix(uint32_t a, uint32_t b, float t) {
	t = clampf(t, 0.f, 1.f);
	int ar = CR(a), ag = CG(a), ab = CB(a), br = CR(b), bg = CG(b), bb = CB(b);
	return RGB((int)(ar + (br - ar) * t), (int)(ag + (bg - ag) * t), (int)(ab + (bb - ab) * t));
}
uint32_t shade(uint32_t c, float f) {
	if (f <= 1.f) return RGB((int)(CR(c) * f), (int)(CG(c) * f), (int)(CB(c) * f));
	return mix(c, 0xFFFFFF, f - 1.f);
}

/* source-over with packed red/blue and green lanes: two multiplies per pixel */
static inline uint32_t mixpx(uint32_t d, uint32_t c, unsigned a1) {
	unsigned ia = 256 - a1;
	uint32_t rb = (((d & 0xFF00FF) * ia + (c & 0xFF00FF) * a1) >> 8) & 0xFF00FF;
	uint32_t g = (((d & 0x00FF00) * ia + (c & 0x00FF00) * a1) >> 8) & 0x00FF00;
	return rb | g;
}
static inline void bl(int x, int y, uint32_t c, int a) {
	uint32_t *p = &tg[y * tw + x];
	if (a >= 255) { *p = c; return; }
	*p = mixpx(*p, c, a + (a >> 7));
}
void blend(int x, int y, uint32_t c, int a) {
	if (x < cx0 || x >= cx1 || y < cy0 || y >= cy1 || a <= 0) return;
	bl(x, y, c, a);
}
void fill_rect(int x, int y, int w, int h, uint32_t c, int a) {
	int x0 = x < cx0 ? cx0 : x, x1 = x + w > cx1 ? cx1 : x + w;
	int y0 = y < cy0 ? cy0 : y, y1 = y + h > cy1 ? cy1 : y + h;
	if (a <= 0 || x1 <= x0) return;
	if (a >= 255) {
		for (int j = y0; j < y1; j++) { uint32_t *p = &tg[j * tw]; for (int i = x0; i < x1; i++) p[i] = c; }
		return;
	}
	unsigned a1 = a + (a >> 7), ia = 256 - a1;
	uint32_t crb = (c & 0xFF00FF) * a1, cg = (c & 0x00FF00) * a1;
	for (int j = y0; j < y1; j++) {
		uint32_t *p = &tg[j * tw];
		for (int i = x0; i < x1; i++) {
			uint32_t d = p[i];
			p[i] = ((((d & 0xFF00FF) * ia + crb) >> 8) & 0xFF00FF) | ((((d & 0x00FF00) * ia + cg) >> 8) & 0x00FF00);
		}
	}
}

/* coverage-driven fill over a bounding box: cov() returns the signed distance inside (px) */
#define BOX(bx0, by0, bx1, by1) \
	int X0 = (int)floorf(bx0), X1 = (int)ceilf(bx1), Y0 = (int)floorf(by0), Y1 = (int)ceilf(by1); \
	if (X0 < cx0) X0 = cx0; if (Y0 < cy0) Y0 = cy0; if (X1 > cx1) X1 = cx1; if (Y1 > cy1) Y1 = cy1;

void disc(float cx, float cy, float rad, uint32_t c, int a) {
	BOX(cx - rad - 1, cy - rad - 1, cx + rad + 1, cy + rad + 1)
	float in2 = rad > 1 ? (rad - 1) * (rad - 1) : 0;
	for (int y = Y0; y < Y1; y++) for (int x = X0; x < X1; x++) {
		float dx = x + .5f - cx, dy = y + .5f - cy, d2 = dx * dx + dy * dy;
		if (d2 < in2) { bl(x, y, c, a); continue; }
		float cov = rad + .5f - sqrtf(d2);
		if (cov > 0) blend(x, y, c, (int)(a * clampf(cov, 0, 1)));
	}
}
void ellipse(float cx, float cy, float rx, float ry, uint32_t c, int a) {
	BOX(cx - rx - 1, cy - ry - 1, cx + rx + 1, cy + ry + 1)
	float m = rx < ry ? rx : ry;
	for (int y = Y0; y < Y1; y++) for (int x = X0; x < X1; x++) {
		float dx = (x + .5f - cx) / rx, dy = (y + .5f - cy) / ry;
		float d = (1.f - sqrtf(dx * dx + dy * dy)) * m;
		if (d > -.5f) blend(x, y, c, (int)(a * clampf(d + .5f, 0, 1)));
	}
}
void ring(float cx, float cy, float rad, float thick, uint32_t c, int a) {
	float e = rad + thick * .5f + 1;
	BOX(cx - e, cy - e, cx + e, cy + e)
	for (int y = Y0; y < Y1; y++) for (int x = X0; x < X1; x++) {
		float dx = x + .5f - cx, dy = y + .5f - cy;
		float d = thick * .5f + .5f - fabsf(sqrtf(dx * dx + dy * dy) - rad);
		if (d > 0) blend(x, y, c, (int)(a * clampf(d, 0, 1)));
	}
}
void rrect(float x, float y, float w, float h, float r, uint32_t c, int a) {
	BOX(x - 1, y - 1, x + w + 1, y + h + 1)
	if (r > w * .5f) r = w * .5f;
	if (r > h * .5f) r = h * .5f;
	float hx = w * .5f - r, hy = h * .5f - r, mx = x + w * .5f, my = y + h * .5f;
	for (int j = Y0; j < Y1; j++) for (int i = X0; i < X1; i++) {
		float qx = fabsf(i + .5f - mx) - hx, qy = fabsf(j + .5f - my) - hy;
		float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
		float d = sqrtf(ox * ox + oy * oy) + fminf(fmaxf(qx, qy), 0.f) - r;
		float cov = .5f - d;
		if (cov > 0) blend(i, j, c, (int)(a * clampf(cov, 0, 1)));
	}
}
void line_aa(float x0, float y0, float x1, float y1, float w, uint32_t c, int a) {
	float h = w * .5f + 1.f;
	BOX(fminf(x0, x1) - h, fminf(y0, y1) - h, fmaxf(x0, x1) + h, fmaxf(y0, y1) + h)
	float dx = x1 - x0, dy = y1 - y0, l2 = dx * dx + dy * dy;
	for (int y = Y0; y < Y1; y++) for (int x = X0; x < X1; x++) {
		float px = x + .5f - x0, py = y + .5f - y0;
		float t = l2 > 0 ? clampf((px * dx + py * dy) / l2, 0.f, 1.f) : 0.f;
		float ex = px - dx * t, ey = py - dy * t;
		float cov = w * .5f + .5f - sqrtf(ex * ex + ey * ey);
		if (cov > 0) blend(x, y, c, (int)(a * clampf(cov, 0, 1)));
	}
}

/* scanline polygon fill with 4 vertical sub-samples and exact horizontal coverage */
#define MAXEDGE 128
void poly_fill(const float *pts, int n, uint32_t c, int a) {
	static float E[MAXEDGE][4];
	static float cov[2048 + 2];
	int ne = 0;
	float miny = 1e9f, maxy = -1e9f, minx = 1e9f, maxx = -1e9f;
	for (int i = 0; i < n; i++) {
		float x0 = pts[i * 2], y0 = pts[i * 2 + 1];
		float x1 = pts[(i + 1) % n * 2], y1 = pts[(i + 1) % n * 2 + 1];
		minx = fminf(minx, x0); maxx = fmaxf(maxx, x0);
		miny = fminf(miny, y0); maxy = fmaxf(maxy, y0);
		if (y0 == y1 || ne >= MAXEDGE) continue;
		if (y0 < y1) { E[ne][0] = x0; E[ne][1] = y0; E[ne][2] = x1; E[ne][3] = y1; }
		else { E[ne][0] = x1; E[ne][1] = y1; E[ne][2] = x0; E[ne][3] = y0; }
		ne++;
	}
	int ys = (int)floorf(miny), ye = (int)ceilf(maxy);
	int xs0 = (int)floorf(minx), xs1 = (int)ceilf(maxx);
	if (xs0 < cx0) xs0 = cx0;
	if (xs1 > cx1 - 1) xs1 = cx1 - 1;
	if (ys < cy0) ys = cy0;
	if (ye > cy1) ye = cy1;
	if (xs1 < xs0) return;
	for (int y = ys; y < ye; y++) {
		for (int x = xs0; x <= xs1 + 1; x++) cov[x] = 0;
		for (int s = 0; s < 4; s++) {
			float sy = y + (s + .5f) / 4.f, xs[32];
			int nx = 0;
			for (int e = 0; e < ne && nx < 32; e++)
				if (E[e][1] <= sy && sy < E[e][3])
					xs[nx++] = E[e][0] + (sy - E[e][1]) * (E[e][2] - E[e][0]) / (E[e][3] - E[e][1]);
			for (int i = 1; i < nx; i++) { float v = xs[i]; int j = i - 1; while (j >= 0 && xs[j] > v) { xs[j + 1] = xs[j]; j--; } xs[j + 1] = v; }
			for (int i = 0; i + 1 < nx; i += 2) {
				float a0 = fmaxf(xs[i], (float)xs0), a1 = fminf(xs[i + 1], (float)xs1 + 1);
				if (a1 <= a0) continue;
				int p0 = (int)floorf(a0), p1 = (int)floorf(a1);
				if (p0 == p1) { cov[p0] += (a1 - a0) * .25f; continue; }
				cov[p0] += (p0 + 1 - a0) * .25f;
				for (int p = p0 + 1; p < p1; p++) cov[p] += .25f;
				cov[p1] += (a1 - p1) * .25f;
			}
		}
		for (int x = xs0; x <= xs1; x++)
			if (cov[x] > 0.004f) bl(x, y, c, (int)(a * clampf(cov[x], 0.f, 1.f)));
	}
}

/* ---------------- sprites ---------------- */
void blit(const Sprite *s, int x, int y, int alpha) {
	int i0 = x < cx0 ? cx0 - x : 0, j0 = y < cy0 ? cy0 - y : 0;
	int i1 = x + s->w > cx1 ? cx1 - x : s->w, j1 = y + s->h > cy1 ? cy1 - y : s->h;
	for (int j = j0; j < j1; j++) {
		const uint32_t *sp = s->px + j * s->w;
		for (int i = i0; i < i1; i++) {
			uint32_t p = sp[i];
			int a = (int)(p >> 24);
			if (!a) continue;
			if (alpha < 255) a = a * alpha / 255;
			bl(x + i, y + j, p & 0xFFFFFF, a + (a >> 7));
		}
	}
}
/* bilinear scale so the sprite's width becomes `size`, centred on (cx, cy) */
void blit_scaled(const Sprite *s, float cx, float cy, float size, int alpha) {
	float k = size / s->w, hw = s->w * k * .5f, hh = s->h * k * .5f;
	BOX(cx - hw, cy - hh, cx + hw, cy + hh)
	float inv = 1.f / k;
	for (int y = Y0; y < Y1; y++) {
		float sy = (y + .5f - (cy - hh)) * inv - .5f;
		int y0 = (int)floorf(sy);
		float fy = sy - y0;
		for (int x = X0; x < X1; x++) {
			float sx = (x + .5f - (cx - hw)) * inv - .5f;
			int x0 = (int)floorf(sx);
			float fx = sx - x0;
			float acc[4] = {0, 0, 0, 0};
			for (int q = 0; q < 4; q++) {
				int xx = x0 + (q & 1), yy = y0 + (q >> 1);
				if (xx < 0 || yy < 0 || xx >= s->w || yy >= s->h) continue;
				float wgt = ((q & 1) ? fx : 1 - fx) * ((q >> 1) ? fy : 1 - fy);
				uint32_t p = s->px[yy * s->w + xx];
				float pa = (p >> 24) * wgt;
				acc[0] += pa; acc[1] += CR(p) * pa; acc[2] += CG(p) * pa; acc[3] += CB(p) * pa;
			}
			if (acc[0] < .5f) continue;
			uint32_t c = RGB((int)(acc[1] / acc[0]), (int)(acc[2] / acc[0]), (int)(acc[3] / acc[0]));
			bl(x, y, c, (int)(acc[0] * alpha / 255));
		}
	}
}

/* ---------------- fonts ---------------- */
typedef struct { uint8_t *bm; int w, h, xo, yo; float adv; } Glyph;
struct Font { Glyph g[95]; float ascent, height; };
Font *fHuge, *fBig, *fUI, *fName, *fSmall, *fTiny;

static Font *font_new(const unsigned char *ttf, float px) {
	stbtt_fontinfo info;
	Font *f = calloc(1, sizeof *f);
	stbtt_InitFont(&info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0));
	float sc = stbtt_ScaleForPixelHeight(&info, px);
	int asc, desc, gap;
	stbtt_GetFontVMetrics(&info, &asc, &desc, &gap);
	f->ascent = asc * sc; f->height = (asc - desc) * sc;
	for (int i = 0; i < 95; i++) {
		int adv, lsb;
		stbtt_GetCodepointHMetrics(&info, 32 + i, &adv, &lsb);
		f->g[i].adv = adv * sc;
		if (i == 0) continue;
		f->g[i].bm = stbtt_GetCodepointBitmap(&info, sc, sc, 32 + i, &f->g[i].w, &f->g[i].h, &f->g[i].xo, &f->g[i].yo);
	}
	return f;
}

void gfx_init(const unsigned char *ttf) {
	fHuge = font_new(ttf, 40.f);
	fBig = font_new(ttf, 30.f);
	fUI = font_new(ttf, 19.f);
	fName = font_new(ttf, 15.f);
	fSmall = font_new(ttf, 13.f);
	fTiny = font_new(ttf, 11.f);
}

float text_w(Font *f, const char *s) {
	float w = 0;
	for (; *s; s++) { int i = (unsigned char)*s - 32; if (i >= 0 && i < 95) w += f->g[i].adv; }
	return w;
}
float text_h(Font *f) { return f->height; }

void text(Font *f, float x, float y, const char *s, uint32_t c, int a, int align) {
	float w = text_w(f, s);
	if (align == AL_CENTER) x -= w * .5f;
	else if (align == AL_RIGHT) x -= w;
	float by = y + f->ascent;
	for (; *s; s++) {
		int i = (unsigned char)*s - 32;
		if (i < 0 || i >= 95) continue;
		Glyph *g = &f->g[i];
		if (g->bm) {
			int gx = (int)floorf(x + .5f) + g->xo, gy = (int)floorf(by + .5f) + g->yo;
			for (int j = 0; j < g->h; j++) for (int k = 0; k < g->w; k++) {
				int v = g->bm[j * g->w + k] * a / 255;
				if (v) blend(gx + k, gy + j, c, v + (v >> 7));
			}
		}
		x += g->adv;
	}
}
