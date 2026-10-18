/* Element icons. When an icon pack (tools/fetch_icons.py) is on the SD card its pictures
 * are used; otherwise each icon is a small vector drawing in a 100x100 unit box, baked on
 * first use into an outlined, shadowed sprite. Baking draws each icon over black and over
 * white and recovers alpha from the difference, so drawing in the background colour (BG) erases. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfx.h"
#include "icons.h"
#include "elements.h"

static float OX, OY, K, RC = 1, RS = 0;
static uint32_t BG;
#define PI 3.14159265f

static void rot(float deg) { RC = cosf(deg * PI / 180); RS = sinf(deg * PI / 180); }
static float tx(float x, float y) { return OX + (50 + (x - 50) * RC - (y - 50) * RS) * K; }
static float ty(float x, float y) { return OY + (50 + (x - 50) * RS + (y - 50) * RC) * K; }

static void D(float x, float y, float r, uint32_t c) { disc(tx(x, y), ty(x, y), r * K, c, 255); }
static void Da(float x, float y, float r, uint32_t c, int a) { disc(tx(x, y), ty(x, y), r * K, c, a); }
static void E(float x, float y, float rx, float ry, uint32_t c) { ellipse(tx(x, y), ty(x, y), rx * K, ry * K, c, 255); }
static void Ea(float x, float y, float rx, float ry, uint32_t c, int a) { ellipse(tx(x, y), ty(x, y), rx * K, ry * K, c, a); }
static void L(float x0, float y0, float x1, float y1, float w, uint32_t c) { line_aa(tx(x0, y0), ty(x0, y0), tx(x1, y1), ty(x1, y1), w * K, c, 255); }
static void RR(float x, float y, float w, float h, float r, uint32_t c) { rrect(OX + x * K, OY + y * K, w * K, h * K, r * K, c, 255); }
static void Pv(uint32_t c, int a, int n, const float *p) {
	float q[128];
	for (int i = 0; i < n && i < 64; i++) { q[i * 2] = tx(p[i * 2], p[i * 2 + 1]); q[i * 2 + 1] = ty(p[i * 2], p[i * 2 + 1]); }
	poly_fill(q, n, c, a);
}
/* polygon from literal coordinates (any numeric type) */
#define P(c, n, ...) Pv((c), 255, (n), (const float[]){__VA_ARGS__})
static void R(float x, float y, float w, float h, uint32_t c) { P(c, 4, x, y, x + w, y, x + w, y + h, x, y + h); }
static void Ra(float x, float y, float w, float h, uint32_t c, int a) { float p[8] = {x, y, x + w, y, x + w, y + h, x, y + h}; Pv(c, a, 4, p); }
static void T3(float x0, float y0, float x1, float y1, float x2, float y2, uint32_t c) { P(c, 3, x0, y0, x1, y1, x2, y2); }
/* elliptical arc, degrees, y down */
static void EA(float cx, float cy, float rx, float ry, float a0, float a1, float w, uint32_t c) {
	int n = (int)(fabsf(a1 - a0) / 8) + 2;
	float px = cx + rx * cosf(a0 * PI / 180), py = cy + ry * sinf(a0 * PI / 180);
	for (int i = 1; i <= n; i++) {
		float a = (a0 + (a1 - a0) * i / n) * PI / 180, x = cx + rx * cosf(a), y = cy + ry * sinf(a);
		L(px, py, x, y, w, c);
		px = x; py = y;
	}
}
static void A(float cx, float cy, float r, float a0, float a1, float w, uint32_t c) { EA(cx, cy, r, r, a0, a1, w, c); }
/* filled upper half of an ellipse sitting on y = cy */
static void SEMI(float cx, float cy, float rx, float ry, uint32_t c) {
	float p[2 * 26];
	for (int i = 0; i <= 24; i++) { float a = PI + PI * i / 24; p[i * 2] = cx + rx * cosf(a); p[i * 2 + 1] = cy + ry * sinf(a); }
	Pv(c, 255, 25, p);
}
/* lens-shaped leaf between two points */
static void LEAF(float x0, float y0, float x1, float y1, float wd, uint32_t c) {
	float p[2 * 24], dx = x1 - x0, dy = y1 - y0, l = sqrtf(dx * dx + dy * dy), nx = -dy / l, ny = dx / l;
	for (int i = 0; i < 12; i++) {
		float t = i / 11.f, o = wd * .5f * sinf(PI * t);
		p[i * 2] = x0 + dx * t + nx * o; p[i * 2 + 1] = y0 + dy * t + ny * o;
		p[(23 - i) * 2] = x0 + dx * t - nx * o; p[(23 - i) * 2 + 1] = y0 + dy * t - ny * o;
	}
	Pv(c, 255, 24, p);
}
static void STAR(float cx, float cy, float ro, float ri, int n, float a0, uint32_t c) {
	float p[2 * 32];
	for (int i = 0; i < n * 2; i++) {
		float a = (a0 + 180.f * i / n) * PI / 180, r = (i & 1) ? ri : ro;
		p[i * 2] = cx + r * cosf(a); p[i * 2 + 1] = cy + r * sinf(a);
	}
	Pv(c, 255, n * 2, p);
}
static void cloud(float x, float y, float s, uint32_t c) {
	D(x - 15 * s, y + 6 * s, 14 * s, c); D(x + 2 * s, y - 4 * s, 18 * s, c); D(x + 18 * s, y + 6 * s, 13 * s, c);
	RR(x - 28 * s, y + 4 * s, 58 * s, 16 * s, 8 * s, c);
}
static void house(float x, float y, float s, uint32_t c) {
	R(x + 8 * s, y + 30 * s, 52 * s, 38 * s, shade(c, 1.45f));
	T3(x, y + 32 * s, x + 34 * s, y, x + 68 * s, y + 32 * s, c);
	R(x + 28 * s, y + 46 * s, 12 * s, 22 * s, 0x7A4A2A);
	R(x + 14 * s, y + 38 * s, 9 * s, 9 * s, 0x9FD3F0);
	R(x + 45 * s, y + 38 * s, 9 * s, 9 * s, 0x9FD3F0);
}
static void tree(float x, float y, float s, uint32_t c) {
	R(x - 5 * s, y + 10 * s, 10 * s, 30 * s, 0x7A4A2A);
	D(x, y - 6 * s, 22 * s, c); D(x - 15 * s, y + 6 * s, 15 * s, c); D(x + 15 * s, y + 6 * s, 15 * s, c);
	Da(x - 7 * s, y - 12 * s, 8 * s, shade(c, 1.3f), 160);
}
static void wavepoly(float y0, float amp, float y1, float ph, uint32_t c) {
	float p[2 * 24];
	for (int i = 0; i <= 20; i++) { float x = 10 + 80 * i / 20.f; p[i * 2] = x; p[i * 2 + 1] = y0 + amp * sinf(x * .16f + ph); }
	p[42] = 90; p[43] = y1; p[44] = 10; p[45] = y1;
	Pv(c, 255, 23, p);
}

static void draw_icon(int el) {
	const ElemDef *e = &ELEMS[el];
	uint32_t c = e->col, d = shade(c, .72f), l = shade(c, 1.35f), dk = 0x2A2A33, wh = 0xFFFFFF;
	const char *nm = e->name;
	rot(0);
	switch (e->shape) {
	case SH_DROP:
		D(50, 62, 25, c); T3(50, 10, 27, 54, 73, 54, c); Da(41, 62, 7, l, 200); break;
	case SH_FLAME: {
		uint32_t in = mix(c, 0xFFE066, .7f);
		D(50, 64, 25, c); P(c, 5, 50, 6, 74, 52, 70, 60, 30, 60, 26, 52); P(c, 3, 26, 52, 22, 30, 38, 46);
		D(50, 70, 13, in); T3(50, 36, 38, 66, 62, 66, in); break; }
	case SH_ROCK:
		P(c, 7, 18, 72, 26, 44, 44, 28, 66, 32, 82, 52, 80, 74, 50, 82);
		P(l, 4, 30, 46, 44, 34, 60, 38, 42, 50); P(d, 4, 50, 82, 80, 74, 82, 60, 60, 70); break;
	case SH_WIND:
		L(14, 34, 60, 34, 7, c); A(60, 24, 10, 90, -180, 7, c);
		L(14, 54, 76, 54, 7, c); A(76, 64, 10, -90, 180, 7, c);
		L(24, 74, 54, 74, 7, c); break;
	case SH_CLOUD: cloud(50, 50, 1.15f, c); Da(46, 42, 8, l, 150); break;
	case SH_PUDDLE: E(50, 64, 38, 17, c); Ea(42, 60, 14, 5, l, 220); E(74, 70, 7, 3, l); break;
	case SH_PRESS:
		RR(40, 28, 20, 44, 6, c);
		L(8, 50, 26, 50, 8, d); T3(36, 50, 24, 38, 24, 62, d);
		L(92, 50, 74, 50, 8, d); T3(64, 50, 76, 38, 76, 62, d); break;
	case SH_BOLT: P(c, 7, 60, 6, 24, 54, 46, 54, 36, 94, 76, 42, 54, 42, 66, 6); break;
	case SH_DOTS: {
		static const float pt[][3] = {{30,40,8},{52,30,7},{70,44,9},{40,62,9},{62,66,7},{24,70,6},{78,70,6},{50,50,6},{36,24,5}};
		for (int i = 0; i < 9; i++) D(pt[i][0], pt[i][1], pt[i][2], (i & 1) ? d : c);
		break; }
	case SH_WAVE:
		wavepoly(36, 6, 86, 0, d); wavepoly(52, 6, 86, 2, c);
		for (int i = 0; i < 4; i++) A(22 + i * 20, 52, 6, 200, 320, 3, l);
		break;
	case SH_HILL: SEMI(50, 80, 42, 44, c); Da(36, 56, 8, l, 120); L(10, 80, 90, 80, 4, d); break;
	case SH_CRYSTAL:
		P(c, 5, 50, 8, 78, 34, 50, 92, 22, 34, 34, 18); P(l, 4, 34, 18, 50, 8, 56, 34, 42, 34);
		P(d, 3, 56, 34, 78, 34, 50, 92); L(22, 34, 78, 34, 3, d); break;
	case SH_MOUNTAIN:
		P(c, 5, 8, 84, 42, 22, 58, 48, 68, 36, 92, 84); P(d, 4, 42, 22, 58, 48, 50, 84, 44, 84);
		P(wh, 5, 42, 22, 52, 40, 46, 37, 40, 44, 33, 38); P(wh, 4, 68, 36, 76, 50, 70, 47, 63, 46); break;
	case SH_VOLCANO:
		Da(46, 22, 8, 0x8A8A8A, 220); Da(58, 13, 7, 0xA0A0A0, 200);
		P(c, 4, 10, 86, 38, 36, 62, 36, 90, 86); P(0xFF7A1A, 6, 38, 36, 62, 36, 58, 50, 52, 44, 46, 58, 42, 44); break;
	case SH_RAIN:
		cloud(50, 32, .9f, shade(c, 1.5f));
		for (int i = 0; i < 3; i++) L(34 + i * 16, 62, 28 + i * 16, 82, 6, c);
		break;
	case SH_STORM:
		cloud(50, 32, .95f, c); P(0xFFE14D, 7, 54, 44, 38, 68, 50, 68, 42, 92, 66, 60, 54, 60, 62, 44); break;
	case SH_SKY: RR(12, 12, 76, 76, 18, c); cloud(52, 52, .7f, wh); D(32, 30, 8, 0xFFE066); break;
	case SH_SNOW:
		for (int i = 0; i < 3; i++) {
			rot(i * 60.f);
			L(50, 12, 50, 88, 7, c);
			L(50, 26, 40, 16, 5, c); L(50, 26, 60, 16, 5, c); L(50, 74, 40, 84, 5, c); L(50, 74, 60, 84, 5, c);
		}
		rot(0); D(50, 50, 8, c); break;
	case SH_ICEBERG:
		P(c, 5, 26, 60, 40, 26, 52, 34, 62, 22, 76, 60); P(d, 4, 22, 60, 78, 60, 64, 90, 36, 90);
		L(10, 60, 90, 60, 4, 0x3A8EE6); break;
	case SH_INGOT:
		P(c, 4, 18, 72, 30, 42, 70, 42, 82, 72); P(l, 4, 30, 42, 70, 42, 64, 32, 36, 32); P(d, 3, 70, 42, 82, 72, 76, 42); break;
	case SH_SUN:
		for (int i = 0; i < 10; i++) { float a = i * 36 * PI / 180; L(50 + 30 * cosf(a), 50 + 30 * sinf(a), 50 + 42 * cosf(a), 50 + 42 * sinf(a), 7, c); }
		D(50, 50, 24, c); Da(42, 42, 8, l, 180); break;
	case SH_MOON:
		D(50, 50, 34, c); D(68, 40, 28, BG);
		if (!strcmp(nm, "Night")) { STAR(70, 66, 9, 4, 5, -90, 0xFFE66B); STAR(80, 30, 6, 2.5f, 5, -90, 0xFFE66B); }
		break;
	case SH_HOURGLASS:
		RR(26, 10, 48, 8, 3, 0x7A4A2A); RR(26, 82, 48, 8, 3, 0x7A4A2A);
		P(0xE6F2F8, 6, 30, 18, 70, 18, 53, 50, 70, 82, 30, 82, 47, 50);
		P(c, 3, 38, 26, 62, 26, 50, 44); P(c, 4, 50, 58, 64, 80, 36, 80, 50, 70); L(50, 44, 50, 80, 2, c); break;
	case SH_CLOCK:
		D(50, 50, 36, c); D(50, 50, 29, 0xFFFDF5);
		for (int i = 0; i < 12; i++) { float a = i * 30 * PI / 180; L(50 + 24 * cosf(a), 50 + 24 * sinf(a), 50 + 27 * cosf(a), 50 + 27 * sinf(a), i % 3 ? 2 : 4, dk); }
		L(50, 50, 50, 30, 5, dk); L(50, 50, 64, 56, 4, dk); D(50, 50, 4, 0xE8423F); break;
	case SH_STAR: STAR(50, 54, 42, 18, 5, -90, c); Da(44, 46, 6, l, 200); break;
	case SH_SPACE:
		D(50, 50, 38, c);
		{ static const float s[][2] = {{30,34},{64,26},{70,62},{38,70},{52,48},{24,54},{78,44}};
		  for (int i = 0; i < 7; i++) D(s[i][0], s[i][1], i & 1 ? 2.5f : 1.8f, wh); }
		STAR(58, 62, 8, 3, 4, -90, 0xFFE66B); break;
	case SH_PLANET:
		EA(50, 52, 46, 13, 180, 360, 5, l); D(50, 50, 27, c); Da(42, 42, 9, l, 140); EA(50, 52, 46, 13, 0, 180, 5, l); break;
	case SH_SWIRL:
		for (int arm = 0; arm < 2; arm++) {
			float px = 50, py = 50;
			for (int i = 1; i <= 30; i++) {
				float t = i / 30.f, a = t * 4.2f + arm * PI, r = 4 + t * 36;
				float x = 50 + r * cosf(a), y = 50 + r * sinf(a) * .8f;
				L(px, py, x, y, 9 - t * 4, arm ? l : c); px = x; py = y;
			}
		}
		D(50, 50, 9, l); break;
	case SH_COMET:
		P(shade(c, 1.4f), 3, 56, 24, 76, 44, 12, 90); D(66, 34, 15, c); Da(61, 29, 5, wh, 180); break;
	case SH_RAINBOW: {
		static const uint32_t rc[5] = {0xE8423F, 0xFF9F1C, 0xFFD23F, 0x4CAF50, 0x3A8EE6};
		for (int i = 0; i < 5; i++) A(50, 76, 40 - i * 7, 180, 360, 7.5f, rc[i]);
		cloud(18, 74, .45f, wh); cloud(82, 74, .45f, wh); break; }
	case SH_CRACK:
		RR(10, 36, 80, 40, 8, c); RR(10, 36, 80, 10, 5, l);
		{ float z[] = {50,36, 44,48, 56,56, 46,66, 52,76}; for (int i = 0; i < 4; i++) L(z[i*2], z[i*2+1], z[i*2+2], z[i*2+3], 5, dk); }
		break;
	case SH_GEYSER:
		E(50, 82, 36, 9, 0x8B5A2B); R(42, 30, 16, 52, c); D(50, 28, 12, c); D(38, 32, 8, c); D(62, 32, 8, c); D(50, 16, 8, c); break;
	case SH_ISLAND:
		E(50, 76, 44, 12, 0x3A8EE6); SEMI(50, 74, 32, 16, c);
		L(52, 70, 58, 34, 5, 0x8B5A2B);
		LEAF(58, 32, 34, 34, 10, 0x3E9B4F); LEAF(58, 32, 80, 38, 10, 0x3E9B4F); LEAF(58, 32, 48, 18, 9, 0x4CAF50); LEAF(58, 32, 72, 20, 9, 0x4CAF50);
		break;
	case SH_RIVER: {
		float p[] = {14, 20, 40, 30, 58, 46, 46, 62, 60, 78, 86, 86};
		for (int i = 0; i < 5; i++) L(p[i*2], p[i*2+1], p[i*2+2], p[i*2+3], 16, c);
		for (int i = 0; i < 5; i++) L(p[i*2], p[i*2+1], p[i*2+2], p[i*2+3], 4, l);
		break; }
	case SH_HEART: D(35, 40, 19, c); D(65, 40, 19, c); T3(17, 46, 83, 46, 50, 86, c); Da(32, 36, 7, l, 200); break;
	case SH_SEED: rot(30); E(50, 52, 18, 28, c); L(50, 30, 50, 74, 3, d); Da(44, 42, 4, l, 200); break;
	case SH_SPROUT:
		E(50, 84, 22, 6, 0x8B5A2B); L(50, 84, 50, 48, 6, d);
		LEAF(50, 56, 20, 36, 18, c); LEAF(50, 48, 82, 26, 20, c); break;
	case SH_GRASS:
		for (int i = 0; i < 7; i++) {
			float x = 16 + i * 11, h = 34 + ((i * 37) % 5) * 8;
			T3(x - 6, 86, x + 6, 86, x + (i & 1 ? 6 : -4), 86 - h - 6, (i & 1) ? d : c);
		}
		break;
	case SH_TREE:
		if (!strcmp(nm, "Christmas Tree")) {
			R(45, 78, 10, 12, 0x7A4A2A);
			for (int i = 0; i < 3; i++) T3(50 - 22 - i * 8, 34 + i * 18, 50 + 22 + i * 8, 34 + i * 18, 50, 8 + i * 18, c);
			D(36, 70, 4, 0xE8423F); D(60, 56, 4, 0xFFD23F); D(46, 42, 4, 0x3A8EE6); D(66, 72, 4, 0xE8423F);
			STAR(50, 10, 9, 4, 5, -90, 0xFFD23F);
		} else {
			tree(50, 42, 1.05f, c);
			if (!strcmp(nm, "Fruit Tree")) { D(36, 42, 5, 0xE8423F); D(58, 30, 5, 0xE8423F); D(64, 50, 5, 0xE8423F); D(46, 54, 5, 0xE8423F); }
		}
		break;
	case SH_FOREST: tree(34, 46, .78f, d); tree(64, 40, .9f, c); break;
	case SH_FLOWER:
		L(50, 54, 50, 92, 5, 0x3E8E41); LEAF(50, 78, 70, 66, 10, 0x4CAF50);
		for (int i = 0; i < 6; i++) { float a = i * 60 * PI / 180; D(50 + 16 * cosf(a), 38 + 16 * sinf(a), 11, c); }
		D(50, 38, 10, 0xFFD23F); break;
	case SH_FRUIT:
		D(40, 58, 24, c); D(60, 58, 24, c); L(50, 36, 54, 18, 4, 0x6D4C41); LEAF(54, 26, 76, 18, 10, 0x4CAF50); Da(36, 50, 7, l, 200);
		if (!strcmp(nm, "Jack-o-lantern")) {
			uint32_t g = 0xFFE14D;
			T3(32, 56, 44, 56, 38, 46, g); T3(56, 56, 68, 56, 62, 46, g);
			P(g, 6, 30, 66, 70, 66, 64, 78, 56, 72, 44, 72, 36, 78);
		}
		break;
	case SH_CACTUS:
		RR(38, 18, 24, 70, 12, c); RR(18, 34, 12, 28, 6, c); RR(18, 54, 24, 10, 5, c);
		RR(70, 26, 12, 26, 6, c); RR(58, 44, 24, 10, 5, c); L(50, 26, 50, 84, 2, d);
		R(30, 84, 40, 8, 0xC2703D); break;
	case SH_LEAF: LEAF(18, 82, 82, 18, 40, c); L(18, 82, 70, 30, 3, d); break;
	case SH_CORAL:
		L(50, 88, 50, 50, 9, c); L(50, 62, 28, 40, 8, c); L(28, 40, 24, 22, 7, c); L(28, 40, 40, 26, 6, c);
		L(50, 54, 72, 34, 8, c); L(72, 34, 70, 16, 6, c); L(72, 34, 84, 26, 6, c); L(50, 50, 52, 24, 7, c);
		D(24, 22, 5, l); D(70, 16, 5, l); D(52, 24, 5, l); break;
	case SH_ANIMAL:
		L(22, 46, 12, 34, 5, c);
		R(28, 56, 8, 26, c); R(64, 56, 8, 26, c); R(38, 58, 7, 24, d); R(54, 58, 7, 24, d);
		E(48, 52, 28, 16, c); D(76, 38, 13, c); T3(70, 28, 74, 16, 80, 28, c);
		if (!strcmp(nm, "Unicorn")) T3(80, 28, 86, 8, 86, 30, 0xFFD23F);
		if (!strcmp(nm, "Pegasus")) { LEAF(46, 46, 20, 16, 16, 0xE8EEF2); LEAF(50, 46, 34, 14, 14, shade(0xE8EEF2, .9f)); }
		if (!strcmp(nm, "Cow")) { D(40, 48, 6, wh); D(56, 56, 5, wh); }
		D(80, 36, 2.5f, dk); break;
	case SH_SHEEP:
		R(32, 60, 7, 24, dk); R(60, 60, 7, 24, dk);
		for (int i = 0; i < 7; i++) D(28 + i * 7.5f, 50 + ((i & 1) ? -6 : 4), 13, c);
		E(78, 44, 10, 13, dk); D(81, 42, 2, wh); break;
	case SH_FISH:
		T3(68, 50, 92, 30, 92, 70, d); E(46, 50, 30, 19, c); T3(40, 32, 56, 22, 58, 34, d);
		D(30, 46, 4, wh); D(29, 46, 2, dk); A(44, 50, 10, -50, 50, 3, d);
		if (!strcmp(nm, "Mermaid")) { D(28, 28, 10, 0xF1C27D); L(20, 22, 36, 20, 5, 0xE8423F); }
		break;
	case SH_WHALE:
		T3(78, 52, 94, 34, 92, 58, c); E(48, 58, 36, 22, c); E(46, 68, 26, 10, l);
		D(26, 52, 3, dk);
		if (!strcmp(nm, "Narwhal")) L(16, 50, 0, 40, 3, 0xF2EAD8);
		else if (!strcmp(nm, "Whale")) { L(40, 34, 36, 22, 3, 0x8FD0F5); L(40, 34, 44, 20, 3, 0x8FD0F5); }
		break;
	case SH_EGG: E(50, 54, 25, 33, c); Ea(42, 42, 6, 10, wh, 200); break;
	case SH_BIRD:
		T3(26, 56, 8, 48, 10, 66, d); E(46, 58, 24, 18, c); D(68, 40, 13, c);
		T3(80, 38, 92, 42, 80, 46, 0xFFA726); LEAF(30, 52, 60, 62, 16, d); D(71, 37, 2.5f, dk);
		if (!strcmp(nm, "Penguin")) E(52, 62, 12, 13, wh);
		if (!strcmp(nm, "Phoenix")) { L(24, 54, 6, 30, 5, 0xFFD23F); L(24, 58, 4, 50, 5, 0xFFD23F); }
		break;
	case SH_LIZARD:
		P(c, 3, 30, 52, 30, 64, 6, 80); E(48, 58, 22, 9, c); E(74, 54, 10, 7, c);
		L(36, 62, 30, 72, 4, d); L(58, 62, 64, 72, 4, d); L(40, 54, 34, 44, 4, d); L(56, 54, 62, 44, 4, d); D(77, 52, 1.8f, dk); break;
	case SH_DRAGON:
		P(d, 3, 40, 46, 30, 14, 60, 42); P(d, 3, 50, 46, 66, 12, 70, 42);
		P(c, 3, 26, 58, 26, 68, 4, 78);
		R(32, 60, 8, 22, c); R(58, 60, 8, 22, c);
		E(46, 58, 22, 14, c); L(62, 52, 72, 38, 9, c); E(78, 34, 11, 8, c);
		for (int i = 0; i < 4; i++) T3(30 + i * 9, 46, 34 + i * 9, 38, 38 + i * 9, 46, d);
		D(80, 32, 2, 0xFFE14D); break;
	case SH_BONE:
		rot(-30); RR(24, 44, 52, 12, 6, c); D(24, 42, 9, c); D(24, 58, 9, c); D(76, 42, 9, c); D(76, 58, 9, c); break;
	case SH_CHEESE:
		P(c, 4, 12, 58, 88, 42, 88, 74, 12, 74); P(l, 3, 12, 58, 88, 42, 74, 32);
		if (!strcmp(nm, "Pizza")) { D(36, 62, 5, 0xC0392B); D(60, 58, 5, 0xC0392B); D(76, 64, 4, 0xC0392B); }
		else { D(32, 66, 4, d); D(56, 60, 5, d); D(74, 66, 3, d); }
		break;
	case SH_CLOTH: {
		float p[2 * 24];
		for (int i = 0; i <= 10; i++) { float x = 14 + 72 * i / 10.f; p[i*2] = x; p[i*2+1] = 24 + 5 * sinf(i * .9f); p[(21-i)*2] = x; p[(21-i)*2+1] = 78 + 5 * sinf(i * .9f); }
		Pv(c, 255, 22, p);
		for (int i = 1; i < 4; i++) L(14 + i * 18, 30, 14 + i * 18, 74, 3, d);
		break; }
	case SH_BUG:
		Ea(36, 38, 14, 10, wh, 190); Ea(64, 38, 14, 10, wh, 190);
		E(50, 58, 18, 24, c); L(34, 54, 66, 54, 5, dk); L(36, 66, 64, 66, 5, dk); D(50, 30, 10, dk);
		L(46, 22, 40, 12, 2, dk); L(54, 22, 60, 12, 2, dk); break;
	case SH_SHELL:
		{ float p[2 * 20]; p[0] = 50; p[1] = 84;
		  for (int i = 0; i <= 16; i++) { float a = (200 + 140 * i / 16.f) * PI / 180; p[2 + i*2] = 50 + 42 * cosf(a); p[3 + i*2] = 80 + 52 * sinf(a); }
		  Pv(c, 255, 18, p); }
		for (int i = 0; i < 5; i++) { float a = (215 + 27.5f * i) * PI / 180; L(50, 82, 50 + 38 * cosf(a), 80 + 46 * sinf(a), 3, d); }
		break;
	case SH_ORB: D(50, 52, 30, c); Da(50, 52, 30, shade(c, .85f), 60); Da(40, 42, 10, wh, 220); break;
	case SH_PERSON: {
		int child = !strcmp(nm, "Child");
		float s = child ? .8f : 1.f, by = child ? 10 : 0;
		static const char *const tinted[] = {"Golem", "Robot", "Zombie", "Frankenstein's Monster", "Statue", "Gingerbread Man", "Mummy", "Medusa", "Ninja Turtle"};
		uint32_t skin = 0xF1C27D;
		for (int i = 0; i < (int)(sizeof tinted / sizeof *tinted); i++) if (!strcmp(nm, tinted[i])) skin = shade(c, 1.15f);
		if (!strcmp(nm, "Human")) c = 0x3A8EE6;
		if (!strcmp(nm, "Angel")) { LEAF(30, 54, 6, 30, 16, 0xE8EEF2); LEAF(70, 54, 94, 30, 16, 0xE8EEF2); }
		if (!strcmp(nm, "Vampire")) P(0xC0392B, 4, 26, 44, 74, 44, 82, 88, 18, 88);
		RR(50 - 22 * s, 46 + by, 44 * s, 42 - by, 14 * s, c);
		D(50, 30 + by, 14 * s, skin);
		if (!strcmp(nm, "King") || !strcmp(nm, "Monarch")) P(0xF5C518, 5, 36, 20, 38, 8, 50, 16, 62, 8, 64, 20);
		else if (!strcmp(nm, "Wizard") || !strcmp(nm, "Alchemist")) { T3(32, 22, 68, 22, 54, 0, c); D(46, 12, 2, 0xFFE66B); }
		else if (!strcmp(nm, "Witch")) { R(28, 19, 44, 4, c); T3(38, 21, 62, 21, 58, 0, c); }
		else if (!strcmp(nm, "Gnome")) T3(34, 24, 66, 24, 50, 0, 0xE8423F);
		else if (!strcmp(nm, "Santa")) { T3(36, 22, 64, 22, 66, 6, 0xE8423F); R(35, 20, 30, 5, wh); D(67, 7, 4, wh); SEMI(50, 46, 12, 8, wh); }
		else if (!strcmp(nm, "Angel")) ring(tx(50, 11), ty(50, 11), 9 * K, 2.5f * K, 0xFFD23F, 255);
		else if (!strcmp(nm, "Pirate")) { SEMI(50, 22, 18, 9, dk); R(28, 20, 44, 4, dk); D(55, 30, 3.5f, dk); L(42, 24, 62, 34, 1.5f, dk); }
		else if (!strcmp(nm, "Ninja")) R(36, 26, 28, 7, c);
		else if (!strcmp(nm, "Cook") || !strcmp(nm, "Baker")) { R(40, 12, 20, 10, wh); D(42, 12, 7, wh); D(50, 9, 8, wh); D(58, 12, 7, wh); }
		else if (!strcmp(nm, "Firefighter")) { SEMI(50, 24, 16, 12, 0xE8423F); R(32, 22, 36, 4, 0xC0392B); }
		else if (!strcmp(nm, "Diver")) { D(50, 30, 17, 0xF5C518); D(50, 31, 11, 0x7FC8F8); }
		else if (!strcmp(nm, "Knight")) { D(50, 30, 15, 0xB0B7C0); R(38, 28, 24, 4, dk); }
		else if (!strcmp(nm, "Farmer")) { E(50, 20, 24, 5, 0xE1B84B); SEMI(50, 20, 13, 10, 0xE1B84B); }
		else if (!strcmp(nm, "Astronaut")) { D(50, 30, 17, wh); D(50, 31, 11, 0x34495E); Da(46, 27, 3, wh, 200); }
		else if (!strcmp(nm, "Blacksmith")) R(36, 54, 28, 34, 0x6D4C41);
		else if (!strcmp(nm, "Hero")) { L(30, 50, 22, 84, 8, 0xC0392B); }
		else if (!strcmp(nm, "Golem")) { D(45, 28, 2.5f, 0xFFE14D); D(55, 28, 2.5f, 0xFFE14D); }
		if (strcmp(nm, "Astronaut") && strcmp(nm, "Knight") && strcmp(nm, "Golem") && strcmp(nm, "Diver")) { D(45, 30 + by, 1.8f, dk); D(55, 30 + by, 1.8f, dk); }
		break; }
	case SH_TOOL:
		rot(-35); R(46, 34, 8, 54, 0x8D6E63); RR(28, 16, 44, 18, 4, c); rot(0); break;
	case SH_LOG:
		RR(12, 36, 66, 30, 14, c); L(20, 44, 64, 44, 2, d); L(24, 56, 60, 56, 2, d);
		E(76, 51, 12, 15, mix(c, 0xF0C88A, .6f)); ring(tx(76, 51), ty(76, 51), 7 * K, 2 * K, d, 255); break;
	case SH_WHEEL:
		ring(tx(50, 50), ty(50, 50), 30 * K, 9 * K, c, 255);
		for (int i = 0; i < 6; i++) { float a = i * 60 * PI / 180; L(50, 50, 50 + 28 * cosf(a), 50 + 28 * sinf(a), 4, d); }
		D(50, 50, 7, d); break;
	case SH_GEAR:
		for (int i = 0; i < 8; i++) { rot(i * 45.f); R(43, 12, 14, 18, c); }
		rot(0); D(50, 50, 28, c); D(50, 50, 10, BG); break;
	case SH_SWORD:
		rot(45); P(c, 5, 46, 64, 46, 16, 50, 6, 54, 16, 54, 64); L(50, 14, 50, 62, 2, l);
		R(32, 62, 36, 7, 0xD4AC0D); R(46, 69, 8, 16, 0x6D4C41); D(50, 88, 5, 0xD4AC0D); rot(0); break;
	case SH_SHIELD:
		P(c, 5, 20, 16, 80, 16, 80, 46, 50, 88, 20, 46); P(l, 5, 28, 23, 72, 23, 72, 45, 50, 78, 28, 45);
		L(50, 26, 50, 72, 6, d); L(32, 42, 68, 42, 6, d); break;
	case SH_WALL:
		R(10, 22, 80, 60, c);
		for (int r = 0; r < 4; r++) {
			float y = 22 + r * 15; L(10, y, 90, y, 2.5f, shade(c, 1.4f));
			for (int k = 0; k < 4; k++) { float x = 10 + k * 20 + (r & 1) * 10; if (x > 10 && x < 90) L(x, y, x, y + 15, 2.5f, shade(c, 1.4f)); }
		}
		break;
	case SH_BRICK:
		P(c, 4, 14, 46, 72, 46, 72, 74, 14, 74); P(l, 4, 14, 46, 72, 46, 86, 36, 28, 36); P(d, 4, 72, 46, 86, 36, 86, 64, 72, 74); break;
	case SH_HOUSE:
		house(16, 18, 1.f, c);
		if (!strcmp(nm, "Library")) { R(30, 54, 40, 6, 0x8E44AD); }
		break;
	case SH_VILLAGE: house(6, 34, .62f, d); house(46, 22, .72f, c); break;
	case SH_CITY: {
		static const float b[][3] = {{12,40,16},{28,22,16},{44,48,14},{58,14,16},{74,34,16}};
		for (int i = 0; i < 5; i++) {
			uint32_t bc = (i & 1) ? c : d;
			R(b[i][0], b[i][1], b[i][2], 88 - b[i][1], bc);
			for (float y = b[i][1] + 5; y < 82; y += 9) { R(b[i][0] + 3, y, 3.5f, 4, 0xFFE66B); R(b[i][0] + b[i][2] - 6.5f, y, 3.5f, 4, 0xFFE66B); }
		}
		break; }
	case SH_TOWER:
		if (!strcmp(nm, "Lighthouse")) {
			P(wh, 4, 38, 30, 62, 30, 68, 88, 32, 88);
			for (int i = 0; i < 3; i++) { float y = 36 + i * 18; P(c, 4, 38 + (y - 30) * .1f, y, 62 - (y - 30) * .1f, y, 62 - (y + 8 - 30) * .1f, y + 8, 38 + (y + 8 - 30) * .1f, y + 8); }
			R(40, 16, 20, 14, 0xFFE66B); T3(36, 16, 64, 16, 50, 4, c);
			L(28, 22, 10, 16, 3, 0xFFE66B); L(72, 22, 90, 16, 3, 0xFFE66B);
		} else {
			R(34, 10, 32, 80, c); R(34, 10, 10, 80, l);
			for (float y = 16; y < 84; y += 9) for (int k = 0; k < 3; k++) R(38 + k * 9, y, 5, 5, 0x9FD3F0);
		}
		break;
	case SH_CASTLE:
		R(20, 42, 60, 44, c); R(12, 28, 20, 58, d); R(68, 28, 20, 58, d);
		for (int k = 0; k < 3; k++) { R(12 + k * 7, 22, 5, 7, d); R(68 + k * 7, 22, 5, 7, d); }
		for (int k = 0; k < 4; k++) R(32 + k * 10, 36, 6, 7, c);
		SEMI(50, 86, 10, 22, dk); R(20, 14, 2, 10, dk); T3(22, 14, 32, 17, 22, 20, 0xE8423F); break;
	case SH_WHEAT:
		L(50, 92, 50, 20, 4, d);
		for (int i = 0; i < 5; i++) { float y = 26 + i * 11; LEAF(50, y + 10, 38, y, 9, c); LEAF(50, y + 10, 62, y, 9, c); }
		LEAF(50, 22, 50, 8, 8, c); break;
	case SH_SACK:
		RR(24, 36, 52, 52, 18, c); P(c, 4, 36, 40, 64, 40, 70, 22, 30, 22); L(34, 38, 66, 38, 4, d);
		Da(40, 56, 8, l, 160); break;
	case SH_BREAD:
		RR(12, 36, 76, 42, 20, c); RR(14, 36, 72, 24, 12, l);
		for (int i = 0; i < 3; i++) L(30 + i * 16, 40, 38 + i * 16, 54, 4, d); break;
	case SH_CAKE:
		R(20, 50, 60, 34, c); RR(16, 42, 68, 14, 7, wh); R(20, 66, 60, 4, wh);
		D(50, 34, 7, 0xE8423F); break;
	case SH_CUP:
		Pv(wh, 150, 4, (const float[]){26, 18, 74, 18, 66, 86, 34, 86});
		P(c, 4, 29, 40, 71, 40, 66, 86, 34, 86); L(26, 18, 74, 18, 3, wh); L(28, 30, 36, 80, 3, wh);
		if (!strcmp(nm, "Tea")) { L(44, 12, 40, 2, 3, 0xC9D2DC); L(56, 12, 60, 2, 3, 0xC9D2DC); }
		break;
	case SH_ICECREAM:
		T3(32, 48, 68, 48, 50, 92, 0xD9A066);
		L(38, 56, 56, 80, 2, 0xB07D4F); L(62, 56, 44, 80, 2, 0xB07D4F);
		D(38, 44, 13, c); D(62, 44, 13, c); D(50, 32, 15, c); D(52, 16, 5, 0xE8423F); break;
	case SH_BOTTLE:
		RR(30, 40, 40, 48, 12, c); R(42, 18, 16, 26, c); R(40, 12, 20, 8, d);
		RR(34, 56, 32, 18, 4, mix(c, wh, .7f)); Ra(34, 44, 6, 36, wh, 90); break;
	case SH_BOAT:
		P(c, 4, 10, 56, 90, 56, 76, 78, 24, 78); L(10, 56, 90, 56, 4, d);
		L(30, 50, 22, 36, 3, 0x6D4C41); break;
	case SH_SHIP:
		L(50, 14, 50, 60, 4, 0x4E342E);
		P(wh, 3, 52, 16, 52, 54, 80, 54); P(0xF2F2F2, 3, 48, 22, 48, 54, 24, 54);
		P(c, 4, 10, 60, 90, 60, 78, 82, 22, 82); break;
	case SH_CAR:
		P(c, 6, 26, 48, 34, 30, 64, 30, 76, 48, 76, 50, 26, 50); RR(10, 46, 80, 24, 8, c);
		P(0xBFE3F5, 4, 36, 34, 48, 34, 48, 46, 30, 46); P(0xBFE3F5, 4, 52, 34, 62, 34, 70, 46, 52, 46);
		D(30, 70, 10, dk); D(70, 70, 10, dk); D(30, 70, 4, 0xB0B7C0); D(70, 70, 4, 0xB0B7C0);
		R(84, 52, 6, 6, 0xFFE66B); break;
	case SH_TRAIN:
		R(12, 38, 50, 34, c); R(56, 22, 30, 50, c); R(62, 28, 18, 14, 0xBFE3F5); R(20, 22, 10, 18, d); R(16, 18, 18, 6, d);
		Da(26, 12, 6, 0xC9D2DC, 220);
		D(26, 76, 9, dk); D(48, 76, 9, dk); D(72, 76, 9, dk); break;
	case SH_BIKE:
		ring(tx(26, 64), ty(26, 64), 16 * K, 5 * K, dk, 255); ring(tx(74, 64), ty(74, 64), 16 * K, 5 * K, dk, 255);
		L(26, 64, 44, 40, 5, c); L(44, 40, 66, 40, 5, c); L(66, 40, 74, 64, 5, c); L(44, 40, 50, 64, 5, c); L(26, 64, 50, 64, 5, c);
		L(50, 64, 66, 40, 5, c); L(44, 40, 40, 30, 4, dk); L(34, 30, 46, 30, 5, dk); L(66, 40, 62, 28, 4, dk); L(60, 28, 70, 26, 4, dk); break;
	case SH_PLANE:
		P(d, 4, 44, 46, 60, 46, 46, 84, 34, 84); P(c, 3, 12, 44, 18, 22, 30, 44);
		RR(10, 40, 80, 16, 8, c); P(d, 4, 46, 46, 58, 46, 50, 18, 42, 18);
		for (int i = 0; i < 5; i++) D(36 + i * 9, 46, 2.5f, 0x3A8EE6);
		break;
	case SH_ROCKET:
		rot(35);
		T3(40, 76, 60, 76, 50, 98, 0xFF9F1C);
		T3(38, 56, 24, 80, 38, 76, 0xE8423F); T3(62, 56, 76, 80, 62, 76, 0xE8423F);
		RR(36, 24, 28, 54, 12, c); T3(36, 32, 64, 32, 50, 4, 0xE8423F);
		D(50, 44, 7, 0x3A8EE6); D(50, 44, 4, 0x9FD3F0); rot(0); break;
	case SH_ALIEN:
		L(40, 20, 32, 6, 3, c); L(60, 20, 68, 6, 3, c); D(32, 6, 4, c); D(68, 6, 4, c);
		E(50, 46, 28, 32, c); rot(20); E(38, 48, 10, 6, dk); rot(-20); E(62, 48, 10, 6, dk); rot(0);
		L(44, 66, 56, 66, 2.5f, d); break;
	case SH_UFO:
		Pv(0xFFF59D, 110, 4, (const float[]){38, 58, 62, 58, 80, 94, 20, 94});
		E(50, 40, 18, 16, 0x9FD3F0); E(50, 52, 42, 12, c); E(50, 48, 42, 6, l);
		for (int i = 0; i < 5; i++) D(22 + i * 14, 54, 3, 0xFFE14D); break;
	case SH_TELESCOPE:
		L(48, 56, 30, 90, 4, dk); L(52, 56, 70, 90, 4, dk); L(50, 56, 50, 90, 4, dk);
		rot(-25); R(14, 42, 64, 16, c); R(72, 38, 12, 24, d); R(10, 46, 8, 8, d); rot(0);
		Da(80, 28, 4, 0xFFE66B, 255); break;
	case SH_BULB:
		RR(40, 62, 20, 18, 3, 0x95A5A6); L(40, 68, 60, 68, 2, 0x6B7B7C); L(40, 74, 60, 74, 2, 0x6B7B7C); D(50, 84, 5, 0x6B7B7C);
		D(50, 40, 25, c); P(c, 4, 38, 56, 62, 56, 60, 64, 40, 64);
		L(44, 58, 46, 44, 2, 0xFF9F1C); L(46, 44, 54, 44, 2, 0xFF9F1C); L(54, 44, 56, 58, 2, 0xFF9F1C);
		Da(42, 32, 6, wh, 220); break;
	case SH_SCREEN:
		RR(12, 16, 76, 54, 6, c); RR(18, 22, 64, 40, 3, 0x7FC8F8); R(44, 70, 12, 10, d); RR(30, 80, 40, 6, 3, d);
		L(26, 32, 50, 32, 3, wh); L(26, 40, 62, 40, 3, wh); L(26, 48, 44, 48, 3, wh); break;
	case SH_ROBOT:
		L(50, 22, 50, 10, 3, d); D(50, 9, 5, 0xE8423F);
		RR(24, 22, 52, 40, 8, c); D(38, 40, 7, 0x2ECCF0); D(62, 40, 7, 0x2ECCF0);
		for (int i = 0; i < 4; i++) R(36 + i * 8, 52, 5, 5, d);
		RR(30, 66, 40, 24, 6, d); D(50, 76, 4, 0xFFE14D); break;
	case SH_PAGE:
		P(c, 5, 24, 12, 64, 12, 78, 26, 78, 88, 24, 88); P(0xD5D5D0, 3, 64, 12, 64, 26, 78, 26);
		for (int i = 0; i < 5; i++) L(32, 38 + i * 10, i == 4 ? 54 : 70, 38 + i * 10, 3, 0xA9B1BA); break;
	case SH_BOOK:
		RR(20, 14, 60, 72, 6, c); R(26, 76, 52, 6, 0xF5F0E6); R(20, 14, 9, 72, d);
		RR(38, 28, 32, 14, 3, l);
		if (!strcmp(nm, "Spellbook")) STAR(54, 58, 9, 4, 4, -90, 0xFFE66B);
		break;
	case SH_BOX:
		R(20, 42, 60, 44, c); R(16, 32, 68, 14, l); R(45, 32, 10, 54, 0xFFD23F);
		LEAF(50, 32, 32, 18, 12, 0xFFD23F); LEAF(50, 32, 68, 18, 12, 0xFFD23F); break;
	case SH_SPARKLE:
		STAR(46, 54, 36, 9, 4, -90, c); STAR(76, 22, 14, 4, 4, -90, l); STAR(24, 20, 9, 3, 4, -90, l); break;
	case SH_WAND:
		L(22, 84, 62, 42, 8, dk); L(22, 84, 30, 76, 8, wh);
		STAR(68, 34, 18, 8, 5, -90, 0xFFE14D); D(84, 56, 3, 0xFFE14D); D(46, 20, 3, 0xFFE14D); break;
	case SH_GHOST: {
		float p[2 * 20]; int n = 0;
		for (int i = 0; i <= 8; i++) { float a = PI + PI * i / 8; p[n++] = 50 + 28 * cosf(a); p[n++] = 44 + 30 * sinf(a); }
		p[n++] = 78; p[n++] = 86; p[n++] = 64; p[n++] = 76; p[n++] = 50; p[n++] = 88; p[n++] = 36; p[n++] = 76; p[n++] = 22; p[n++] = 86;
		Pv(c, 255, n / 2, p);
		E(40, 44, 5, 8, dk); E(60, 44, 5, 8, dk); break; }
	case SH_SNOWMAN:
		D(50, 68, 22, c); D(50, 36, 16, c); R(34, 16, 32, 4, dk); R(39, 2, 22, 16, dk);
		D(44, 34, 2.5f, dk); D(56, 34, 2.5f, dk); T3(50, 38, 50, 43, 64, 41, 0xFF9F1C);
		D(50, 60, 2.5f, dk); D(50, 70, 2.5f, dk); L(30, 56, 14, 44, 3, 0x6D4C41); L(70, 56, 86, 44, 3, 0x6D4C41); break;
	case SH_TORNADO:
		for (int i = 0; i < 7; i++) { float y = 16 + i * 11, w = 40 - i * 5, x = 50 + 6 * sinf(i * .9f); L(x - w, y, x + w, y, 7, i & 1 ? d : c); }
		break;
	case SH_BRIDGE:
		R(8, 46, 84, 8, c); A(50, 92, 40, 200, 340, 7, c);
		for (int i = 0; i < 5; i++) { float x = 18 + i * 16, y = 92 - sqrtf(fmaxf(0, 1600 - (x - 50) * (x - 50))); if (y < 54) L(x, 54, x, y, 3, d); }
		E(50, 88, 44, 6, 0x3A8EE6); break;
	case SH_REMOTE:
		RR(36, 6, 28, 88, 12, c); R(46, 16, 8, 22, dk); R(39, 23, 22, 8, dk);
		D(50, 48, 6, 0x3A8EE6); D(46, 64, 2.5f, 0x9AA0A6); D(54, 64, 2.5f, 0x9AA0A6);
		D(50, 76, 4, 0x9AA0A6); D(50, 85, 4, 0x9AA0A6); break;
	case SH_YARN:
		D(46, 54, 30, c); A(46, 54, 22, 200, 320, 3, d); A(46, 54, 14, 180, 340, 3, d); A(32, 60, 24, -40, 60, 3, d);
		L(72, 70, 90, 88, 3, c); break;
	case SH_CAMPFIRE: {
		uint32_t in = mix(c, 0xFFE066, .7f);
		L(18, 84, 82, 70, 9, 0x7A4A2A); L(18, 70, 82, 84, 9, 0x8D5A34);
		D(50, 56, 20, c); P(c, 3, 50, 8, 70, 50, 30, 50); D(50, 62, 10, in); T3(50, 34, 41, 60, 59, 60, in); break; }
	case SH_SKULL:
		D(50, 42, 30, c); RR(34, 56, 32, 26, 7, c);
		E(38, 44, 8, 9, dk); E(62, 44, 8, 9, dk); T3(50, 52, 45, 61, 55, 61, dk);
		for (int i = 0; i < 3; i++) L(42 + i * 8, 70, 42 + i * 8, 82, 2.5f, d);
		break;
	case SH_GRAVE:
		E(50, 88, 40, 6, 0x6FA84A);
		if (!strcmp(nm, "Coffin")) {
			P(c, 6, 38, 8, 62, 8, 72, 30, 62, 92, 38, 92, 28, 30); P(l, 4, 38, 8, 62, 8, 64, 14, 36, 14);
			L(50, 26, 50, 58, 5, d); L(40, 36, 60, 36, 5, d);
		} else if (!strcmp(nm, "Graveyard")) {
			RR(12, 36, 32, 52, 14, d); R(12, 60, 32, 28, d); RR(52, 22, 36, 66, 16, c); R(52, 50, 36, 38, c);
			L(70, 34, 70, 58, 4, d); L(62, 42, 78, 42, 4, d);
		} else {
			RR(24, 14, 52, 74, 24, c); R(24, 50, 52, 38, c); Ra(24, 14, 10, 74, l, 120);
			L(50, 28, 50, 60, 5, d); L(40, 38, 60, 38, 5, d);
		}
		break;
	case SH_NOTE:
		D(32, 74, 13, c); D(70, 66, 13, c); L(43, 74, 43, 22, 6, c); L(81, 66, 81, 14, 6, c);
		P(c, 4, 40, 18, 84, 10, 84, 24, 40, 32); Da(28, 70, 4, l, 200); break;
	case SH_ENVELOPE:
		if (!strcmp(nm, "Mailbox")) {
			L(50, 60, 50, 92, 6, 0x6D4C41); RR(18, 26, 64, 38, 18, c); R(18, 45, 64, 19, c);
			R(66, 14, 4, 22, 0xE8423F); R(70, 14, 12, 8, 0xE8423F); break;
		}
		R(12, 26, 76, 50, c); P(l, 3, 12, 26, 88, 26, 50, 56); L(12, 26, 50, 56, 3, d); L(88, 26, 50, 56, 3, d);
		L(12, 76, 40, 50, 2, d); L(88, 76, 60, 50, 2, d); break;
	case SH_GLASSES: {
		uint32_t lens = (!strcmp(nm, "Sunglasses") || !strcmp(nm, "Binoculars")) ? 0x2A2A33 : 0x9FD3F0;
		if (!strcmp(nm, "Binoculars")) {
			RR(14, 30, 30, 50, 10, c); RR(56, 30, 30, 50, 10, c); R(40, 40, 20, 14, d);
			D(29, 74, 11, lens); D(71, 74, 11, lens); Da(25, 70, 3, wh, 200); Da(67, 70, 3, wh, 200); break;
		}
		Da(30, 52, 14, lens, 200); Da(70, 52, 14, lens, 200);
		ring(tx(30, 52), ty(30, 52), 15 * K, 5 * K, c, 255); ring(tx(70, 52), ty(70, 52), 15 * K, 5 * K, c, 255);
		A(50, 54, 7, 200, 340, 4, c); L(15, 48, 6, 40, 4, c); L(85, 48, 94, 40, 4, c);
		Da(25, 47, 3.5f, wh, 200); Da(65, 47, 3.5f, wh, 200);
		break; }
	case SH_RING:
		if (!strcmp(nm, "Horseshoe")) { A(50, 44, 28, -15, 195, 12, c); for (int i = 0; i < 5; i++) { float a = (-5 + i * 47.5f) * PI / 180; D(50 + 28 * cosf(a), 44 + 28 * sinf(a), 2, d); } break; }
		if (!strcmp(nm, "Chain")) { EA(32, 50, 20, 12, 0, 360, 6, c); EA(68, 50, 20, 12, 0, 360, 6, d); EA(32, 50, 20, 12, -60, 60, 6, c); break; }
		if (!strcmp(nm, "Stethoscope")) { A(50, 34, 22, 0, 180, 5, c); L(28, 34, 28, 18, 4, c); L(72, 34, 72, 18, 4, c); L(50, 56, 50, 70, 4, c); D(50, 76, 9, 0xB0B7C0); D(50, 76, 5, d); break; }
		ring(tx(50, 62), ty(50, 62), 24 * K, 7 * K, c, 255); EA(50, 62, 20, 20, 200, 260, 3, l);
		if (strcmp(nm, "The One Ring")) { P(0x9FD3F0, 4, 50, 18, 62, 30, 50, 42, 38, 30); P(wh, 3, 50, 18, 56, 24, 46, 26); }
		break;
	case SH_SNAKE: {
		float px = 12, py = 64;
		for (int i = 1; i <= 24; i++) { float x = 12 + i * 2.9f, y = 60 + 16 * sinf(i * .5f); L(px, py, x, y, 12 - i * .2f, i & 1 ? c : d); px = x; py = y; }
		E(84, 46, 11, 8, c); D(87, 43, 2.2f, dk); L(94, 48, 99, 46, 1.5f, 0xE8423F);
		if (!strcmp(nm, "Electric Eel")) P(0xFFE14D, 6, 44, 18, 34, 34, 44, 34, 38, 48, 52, 30, 42, 30);
		break; }
	case SH_UMBRELLA:
		if (!strcmp(nm, "Parachute") || !strcmp(nm, "Paraglider")) {
			SEMI(50, 40, 42, 30, c); for (int i = 0; i < 4; i++) D(19 + i * 20.7f, 41, 9.5f, BG);
			L(12, 40, 46, 76, 2, dk); L(88, 40, 54, 76, 2, dk); RR(44, 72, 12, 18, 5, 0x34495E); D(50, 68, 5, 0xF1C27D); break;
		}
		SEMI(50, 48, 42, 34, c); SEMI(50, 48, 14, 34, l);
		for (int i = 0; i < 4; i++) D(19 + i * 20.7f, 50, 9.5f, BG);
		L(50, 46, 50, 84, 4, dk); A(43, 84, 7, 0, 180, 4, dk); L(50, 14, 50, 8, 3, dk); break;
	case SH_KNIFE:
		if (!strcmp(nm, "Scissors")) {
			rot(-45); P(0xB0B7C0, 4, 46, 50, 54, 50, 52, 8, 48, 8); rot(45); P(0xB0B7C0, 4, 46, 50, 54, 50, 52, 8, 48, 8); rot(0);
			ring(tx(36, 68), ty(36, 68), 10 * K, 5 * K, c, 255); ring(tx(64, 68), ty(64, 68), 10 * K, 5 * K, c, 255); D(50, 50, 3, dk); break;
		}
		if (!strcmp(nm, "Scythe")) { L(70, 10, 40, 92, 6, 0x8D6E63); EA(46, 20, 36, 14, 180, 330, 8, c); break; }
		rot(-40); P(c, 5, 8, 50, 60, 42, 60, 58, 18, 58, 8, 52); L(14, 50, 58, 46, 2, l);
		RR(60, 43, 30, 14, 5, !strcmp(nm, "Bayonet") ? 0x5D4037 : 0x6D4C41); D(66, 50, 2, 0xB0B7C0); D(82, 50, 2, 0xB0B7C0); rot(0);
		break;
	case SH_GUN:
		if (!strcmp(nm, "Cannon")) {
			rot(-20); RR(14, 36, 70, 24, 12, c); R(78, 34, 8, 28, d); rot(0);
			D(40, 66, 16, 0x8D6E63); D(40, 66, 6, 0x5D4037); E(50, 86, 30, 4, 0x5D4037); break;
		}
		RR(12, 32, 64, 16, 4, c); R(70, 35, 20, 10, d); P(c, 4, 18, 46, 40, 46, 34, 82, 14, 82);
		A(46, 50, 7, 0, 180, 3, d); R(14, 30, 6, 4, d);
		if (!strcmp(nm, "Water Gun")) { D(50, 26, 10, 0xFFA726); Da(92, 40, 4, 0x9FD3F0, 220); }
		if (!strcmp(nm, "Flamethrower")) { D(94, 38, 6, 0xFF9F1C); D(98, 32, 4, 0xFFE14D); }
		if (!strcmp(nm, "Stun Gun")) P(0x3A8EE6, 4, 92, 30, 84, 42, 92, 42, 86, 54);
		break;
	case SH_BOMB:
		if (!strcmp(nm, "Atomic Bomb")) {
			E(46, 52, 34, 20, c); T3(76, 52, 94, 34, 94, 70, d); D(40, 52, 9, 0xFFD23F);
			for (int i = 0; i < 3; i++) { float a = (i * 120 - 90) * PI / 180; D(40 + 6 * cosf(a), 52 + 6 * sinf(a), 3, dk); }
			break;
		}
		D(46, 60, 30, c); R(52, 22, 16, 14, d); L(62, 24, 72, 12, 3, 0xC9A26B);
		STAR(76, 9, 9, 3, 6, 0, 0xFFD23F); Da(34, 48, 8, wh, 140); break;
	case SH_BOOM:
		STAR(50, 50, 46, 24, 11, -90, c); STAR(50, 50, 28, 14, 9, -70, mix(c, 0xFFE14D, .6f)); D(50, 50, 9, 0xFFF59D); break;
	case SH_COIN:
		if (!strcmp(nm, "Treasure")) {
			R(16, 46, 68, 40, 0x8D5A34); SEMI(50, 46, 34, 22, 0x8D5A34); R(16, 54, 68, 6, c); R(46, 52, 8, 14, c);
			D(32, 36, 6, c); D(46, 30, 6, c); D(62, 34, 6, c); D(56, 26, 4, l); break;
		}
		E(50, 72, 32, 11, d); R(18, 62, 64, 10, d); E(50, 62, 32, 11, c);
		D(52, 40, 26, d); D(50, 38, 26, c); D(50, 38, 19, l);
		L(46, 28, 46, 48, 4, d); A(48, 33, 6, 90, 270, 3, d); A(48, 43, 6, -90, 90, 3, d); break;
	case SH_TENT:
		T3(8, 86, 50, 14, 92, 86, c); T3(50, 14, 92, 86, 66, 86, d);
		if (!strcmp(nm, "Circus")) { T3(50, 14, 22, 86, 36, 86, wh); T3(50, 14, 56, 86, 70, 86, wh); L(50, 14, 50, 4, 3, dk); T3(50, 4, 62, 7, 50, 10, 0xFFD23F); }
		T3(50, 44, 38, 86, 62, 86, dk); break;
	case SH_CANDLE:
		R(36, 40, 28, 50, c); E(50, 40, 14, 4, l); RR(36, 40, 7, 16, 3, l); RR(54, 40, 6, 10, 3, l);
		L(50, 40, 50, 32, 2, dk); D(50, 25, 7, 0xFF9F1C); T3(50, 8, 43, 25, 57, 25, 0xFF9F1C); D(50, 27, 3.5f, 0xFFE14D); break;
	case SH_PHONE:
		if (!strcmp(nm, "Tablet")) { RR(12, 16, 76, 68, 8, c); RR(18, 22, 64, 56, 3, 0x7FC8F8); Ra(18, 22, 30, 56, wh, 50); break; }
		RR(30, 8, 40, 84, 8, c); RR(34, 16, 32, 64, 3, 0x7FC8F8); Ra(34, 16, 14, 64, wh, 50); R(44, 11, 12, 2, d); D(50, 86, 3, 0x9AA0A6); break;
	case SH_WEB:
		if (!strcmp(nm, "Butterfly Net")) { L(76, 92, 50, 52, 5, 0x8D6E63); E(40, 34, 26, 22, c); Ea(40, 34, 20, 16, wh, 120); }
		for (int i = 0; i < 8; i++) { float a = i * 45 * PI / 180; L(50, 50, 50 + 42 * cosf(a), 50 + 42 * sinf(a), 2.5f, c); }
		for (int r = 12; r <= 40; r += 9)
			for (int i = 0; i < 8; i++) { float a = i * 45 * PI / 180, b = (i + 1) * 45 * PI / 180; L(50 + r * cosf(a), 50 + r * sinf(a), 50 + r * cosf(b), 50 + r * sinf(b), 2.5f, c); }
		break;
	case SH_SPIDER:
		for (int i = 0; i < 4; i++) {
			float y = 42 + i * 8;
			L(42, 52, 20, y - 10, 3.5f, c); L(20, y - 10, 10, y + 12, 3.5f, c);
			L(58, 52, 80, y - 10, 3.5f, c); L(80, y - 10, 90, y + 12, 3.5f, c);
		}
		E(50, 60, 16, 18, c); D(50, 38, 11, c); D(45, 36, 2.5f, 0xE8423F); D(55, 36, 2.5f, 0xE8423F); break;
	case SH_POT:
		if (!strcmp(nm, "Barrel")) {
			E(50, 50, 30, 40, c); R(20, 12, 60, 76, c); E(50, 12, 26, 6, l);
			L(20, 30, 80, 30, 5, 0x5A5A66); L(20, 70, 80, 70, 5, 0x5A5A66); L(38, 14, 36, 86, 2, d); L(62, 14, 64, 86, 2, d); break;
		}
		if (!strcmp(nm, "Bucket")) {
			A(50, 40, 30, 180, 360, 3, dk); P(c, 4, 20, 40, 80, 40, 72, 88, 28, 88); E(50, 40, 30, 7, d); L(22, 52, 78, 52, 3, l); break;
		}
		if (!strcmp(nm, "Drum")) {
			R(18, 34, 64, 44, c); E(50, 78, 32, 9, c); E(50, 34, 32, 9, 0xF2EAD8);
			for (int i = 0; i < 4; i++) L(18 + i * 21, 40, 28 + i * 21, 76, 2.5f, 0xFFD23F);
			L(56, 28, 82, 6, 4, 0x8D6E63); L(44, 28, 18, 6, 4, 0x8D6E63); break;
		}
		if (!strcmp(nm, "Vase")) { E(50, 64, 26, 24, c); P(c, 4, 40, 44, 60, 44, 56, 18, 44, 18); E(50, 16, 10, 4, d); L(30, 60, 70, 60, 3, l); break; }
		if (!strcmp(nm, "Cauldron")) { L(30, 78, 24, 92, 5, c); L(70, 78, 76, 92, 5, c); D(30, 34, 6, 0x8BC34A); D(54, 26, 8, 0x8BC34A); }
		E(50, 60, 34, 28, c); R(18, 32, 64, 10, d); E(50, 32, 32, 6, !strcmp(nm, "Cauldron") ? 0x8BC34A : dk); Da(36, 56, 7, l, 160); break;
	case SH_BURGER:
		if (!strcmp(nm, "Sandwich")) {
			P(c, 3, 10, 70, 90, 70, 50, 20); R(10, 70, 80, 6, 0x4CAF50); R(12, 76, 76, 6, 0xE88A8A); RR(10, 80, 80, 10, 3, c);
			L(10, 70, 50, 20, 3, d); L(90, 70, 50, 20, 3, d); break;
		}
		SEMI(50, 44, 36, 26, c); R(12, 44, 76, 8, 0x4CAF50); RR(12, 50, 76, 12, 6, 0x6D3B1F);
		if (!strcmp(nm, "Cheeseburger")) P(0xFFD23F, 4, 12, 50, 88, 50, 76, 60, 30, 60);
		RR(14, 62, 72, 16, 7, c); D(38, 30, 2, wh); D(52, 26, 2, wh); D(62, 34, 2, wh); D(44, 38, 2, wh); break;
	case SH_BOWL: {
		uint32_t bw = !strcmp(nm, "Nest") ? c : 0xE8EEF2;
		if (!strcmp(nm, "Nest")) { E(36, 46, 9, 11, 0xBFE3F5); E(56, 44, 9, 11, 0xBFE3F5); }
		else SEMI(50, 52, 34, 18, c);
		if (!strcmp(nm, "Spaghetti") || !strcmp(nm, "Pasta")) { D(42, 40, 6, 0xC0392B); D(58, 38, 5, 0xC0392B); }
		{ float p[2 * 26]; for (int i = 0; i <= 24; i++) { float a = PI * i / 24; p[i * 2] = 50 + 40 * cosf(a); p[i * 2 + 1] = 52 + 32 * sinf(a); } Pv(bw, 255, 25, p); }
		L(10, 52, 90, 52, 4, shade(bw, .8f)); L(22, 66, 78, 66, 3, shade(bw, .85f));
		if (!strcmp(nm, "Nest")) for (int i = 0; i < 5; i++) L(14 + i * 4, 56 + i * 5, 86 - i * 4, 58 + i * 5, 2, d);
		break; }
	case SH_ATOM:
		for (int i = 0; i < 3; i++) { rot(i * 60.f); EA(50, 50, 42, 15, 0, 360, 4, c); }
		rot(0); D(50, 50, 9, 0xE8423F); D(92, 50, 4, c); break;
	case SH_BULLET:
		R(36, 44, 28, 44, c); SEMI(50, 45, 14, 34, l); R(34, 82, 32, 8, d); L(40, 48, 40, 80, 3, l); break;
	case SH_MEAT:
		if (!strcmp(nm, "Bacon")) {
			for (int k = 0; k < 2; k++) {
				float px = 12, py = 36 + k * 26;
				for (int i = 1; i <= 16; i++) { float x = 12 + i * 4.75f, y = 36 + k * 26 + 6 * sinf(i * .8f); L(px, py, x, y, 14, c); L(px, py + 2, x, y + 2, 4, 0xF5D0C5); px = x; py = y; }
			}
			break;
		}
		if (!strcmp(nm, "Steak") || !strcmp(nm, "Jerky")) {
			P(c, 8, 14, 50, 26, 26, 56, 18, 82, 28, 88, 54, 74, 78, 40, 82, 18, 70);
			A(50, 50, 36, 140, 260, 5, 0xF5E0D0); D(60, 48, 8, 0xF5E0D0); break;
		}
		L(60, 60, 82, 82, 9, 0xF2EAD8); D(84, 76, 6, 0xF2EAD8); D(78, 86, 6, 0xF2EAD8);
		rot(-40); E(42, 46, 32, 24, c); Ea(36, 38, 12, 6, l, 200); rot(0); break;
	case SH_WINDMILL:
		if (!strcmp(nm, "Wind Turbine")) {
			L(50, 40, 50, 94, 5, c);
			for (int i = 0; i < 3; i++) { float a = (i * 120 - 90) * PI / 180; L(50, 40, 50 + 36 * cosf(a), 40 + 36 * sinf(a), 6, c); }
			D(50, 40, 6, d); break;
		}
		P(c, 4, 38, 40, 62, 40, 70, 92, 30, 92); T3(36, 42, 64, 42, 50, 26, 0xC0392B); R(45, 76, 10, 16, 0x6D4C41);
		for (int i = 0; i < 4; i++) {
			float a = (45 + i * 90) * PI / 180, cx = cosf(a), sy = sinf(a);
			L(50, 38, 50 + 40 * cx, 38 + 40 * sy, 3, 0x8D6E63);
			float q[8] = {50 + 14 * cx - 6 * sy, 38 + 14 * sy + 6 * cx, 50 + 40 * cx - 6 * sy, 38 + 40 * sy + 6 * cx, 50 + 40 * cx, 38 + 40 * sy, 50 + 14 * cx, 38 + 14 * sy};
			Pv(0xE8DCC8, 255, 4, q);
		}
		D(50, 38, 5, 0x5D4037); break;
	case SH_BUILDING:
		R(16, 26, 68, 62, c);
		if (!strcmp(nm, "Bank")) {
			T3(10, 30, 90, 30, 50, 8, shade(c, .85f)); R(12, 28, 76, 6, l);
			for (int i = 0; i < 4; i++) R(22 + i * 16, 38, 8, 44, l);
			R(10, 82, 80, 6, shade(c, .85f)); break;
		}
		if (!strcmp(nm, "Factory")) {
			R(66, 8, 12, 30, d); Da(72, 4, 6, 0xC9D2DC, 220);
			for (int i = 0; i < 3; i++) T3(16 + i * 17, 26, 33 + i * 17, 26, 33 + i * 17, 14, d);
		} else R(12, 20, 76, 8, d);
		for (int r = 0; r < 3; r++) for (int k = 0; k < 4; k++) if (!(r == 2 && (k == 1 || k == 2))) R(22 + k * 15, 34 + r * 16, 9, 9, 0x9FD3F0);
		R(42, 66, 16, 22, shade(c, .6f));
		if (!strcmp(nm, "Hospital")) { R(46, 30, 8, 22, 0xE8423F); R(39, 37, 22, 8, 0xE8423F); }
		break;
	case SH_CART:
		if (!strcmp(nm, "Sleigh")) {
			P(c, 5, 16, 34, 70, 34, 80, 62, 22, 62, 14, 44); A(84, 64, 10, -90, 90, 4, 0xF5C518);
			L(18, 74, 84, 74, 4, 0xF5C518); L(30, 62, 30, 74, 3, 0xF5C518); L(66, 62, 66, 74, 3, 0xF5C518); break;
		}
		if (!strcmp(nm, "Lawn Mower")) { L(70, 48, 92, 14, 4, dk); L(86, 12, 96, 18, 5, dk); }
		R(12, 36, 66, 26, c); L(12, 46, 78, 46, 2, d);
		if (!strcmp(nm, "Cart") || !strcmp(nm, "Wagon")) L(78, 52, 96, 44, 4, 0x6D4C41);
		D(28, 70, 13, dk); D(64, 70, 13, dk); D(28, 70, 5, 0xB0B7C0); D(64, 70, 5, 0xB0B7C0); break;
	case SH_PENCIL:
		rot(-45);
		if (!strcmp(nm, "Syringe")) {
			R(24, 40, 44, 20, c); Ra(24, 40, 44, 20, wh, 100); R(36, 44, 32, 12, 0xE8423F); R(68, 47, 22, 6, 0x9AA0A6);
			R(8, 46, 16, 8, dk); R(4, 38, 6, 24, dk); rot(0); break;
		}
		if (!strcmp(nm, "Thermometer")) { RR(16, 42, 64, 16, 8, 0xE8EEF2); D(78, 50, 11, c); R(40, 47, 38, 6, c); rot(0); break; }
		if (!strcmp(nm, "Needle")) { P(c, 4, 10, 48, 84, 49, 92, 50, 10, 52); E(18, 50, 5, 1.5f, BG); rot(0); break; }
		R(22, 40, 50, 20, c); R(22, 40, 50, 6, l); T3(72, 40, 72, 60, 92, 50, 0xF2D2A0); T3(86, 47, 86, 53, 92, 50, !strcmp(nm, "Crayon") ? c : dk);
		R(10, 40, 12, 20, !strcmp(nm, "Crayon") ? d : 0xF48FB1); R(20, 40, 4, 20, 0xB0B7C0); rot(0); break;
	case SH_BAT:
		P(c, 12, 50, 40, 70, 28, 94, 32, 84, 48, 74, 46, 64, 60, 50, 52, 36, 60, 26, 46, 16, 48, 6, 32, 30, 28);
		E(50, 48, 9, 13, c); T3(42, 38, 46, 26, 50, 36, c); T3(58, 38, 54, 26, 50, 36, c);
		D(46, 42, 2, 0xFFE14D); D(54, 42, 2, 0xFFE14D); break;
	case SH_COOKIE:
		if (!strcmp(nm, "Donut")) {
			D(50, 52, 36, 0xD9A066); D(50, 50, 31, c); D(50, 50, 11, BG);
			static const float sp[][2] = {{30,36},{62,28},{72,52},{40,70},{28,56},{60,72},{48,26}};
			for (int i = 0; i < 7; i++) L(sp[i][0], sp[i][1], sp[i][0] + 4, sp[i][1] + 3, 3, i % 3 == 0 ? 0x3A8EE6 : i % 3 == 1 ? 0xFFE14D : wh);
			break;
		}
		D(50, 50, 36, c); Da(40, 40, 12, l, 120);
		{ static const float ch[][2] = {{34,36},{60,30},{70,54},{42,62},{56,72},{28,58},{50,46}};
		  for (int i = 0; i < 7; i++) D(ch[i][0], ch[i][1], 4.5f, 0x5D3A1A); }
		break;
	case SH_KITE:
		P(c, 4, 50, 8, 78, 38, 50, 74, 22, 38); P(l, 3, 50, 8, 78, 38, 50, 38); P(d, 3, 22, 38, 50, 74, 50, 38);
		{ float px = 50, py = 74; for (int i = 1; i <= 4; i++) { float x = 50 + 8 * sinf(i * 1.4f), y = 74 + i * 5; L(px, py, x, y, 2, dk); px = x; py = y; } }
		T3(46, 82, 54, 82, 50, 88, 0xFFD23F); break;
	case SH_PYRAMID:
		T3(6, 86, 50, 16, 94, 86, c); T3(50, 16, 94, 86, 62, 86, d);
		for (int i = 1; i < 4; i++) { float y = 16 + i * 17.5f, hw = (y - 16) * 44 / 70; L(50 - hw, y, 50 + hw * .3f, y, 2, d); }
		break;
	case SH_APPLIANCE:
		RR(24, 8, 52, 84, 6, c); Ra(24, 8, 10, 84, l, 120);
		if (!strcmp(nm, "Safe") || !strcmp(nm, "Vault")) {
			RR(18, 16, 64, 68, 6, c); RR(24, 22, 52, 56, 4, d); D(50, 50, 14, l); D(50, 50, 9, c);
			for (int i = 0; i < 6; i++) { float a = i * 60 * PI / 180; L(50, 50, 50 + 12 * cosf(a), 50 + 12 * sinf(a), 2, dk); }
			R(70, 40, 4, 20, l); break;
		}
		if (!strcmp(nm, "Printer")) { R(30, 10, 40, 30, wh); RR(14, 34, 72, 40, 6, c); R(28, 64, 44, 26, wh); L(34, 74, 64, 74, 2, 0xA9B1BA); D(76, 44, 3, 0x4CAF50); break; }
		if (!strcmp(nm, "Blender")) { P(0xBFE3F5, 4, 30, 14, 70, 14, 62, 60, 38, 60); Pv(c, 160, 4, (const float[]){33, 34, 67, 34, 62, 60, 38, 60}); R(30, 60, 40, 30, dk); D(50, 76, 6, c); break; }
		L(24, 36, 76, 36, 3, d); R(64, 16, 5, 14, d); R(64, 44, 5, 22, d); break;
	case SH_CANE:
		L(58, 92, 58, 34, 11, wh); A(42, 34, 16, 180, 360, 11, wh);
		for (int i = 0; i < 5; i++) L(53, 84 - i * 12, 63, 78 - i * 12, 4, c);
		L(28, 34, 30, 44, 4, c); L(36, 20, 44, 26, 4, c); L(54, 22, 50, 30, 4, c); break;
	case SH_SHIRT:
		P(c, 13, 30, 14, 42, 14, 50, 22, 58, 14, 70, 14, 90, 32, 78, 44, 70, 38, 70, 88, 30, 88, 30, 38, 22, 44, 10, 32);
		if (!strcmp(nm, "Sweater")) { for (int i = 0; i < 3; i++) L(30, 50 + i * 12, 70, 50 + i * 12, 3, wh); }
		else if (!strcmp(nm, "Apron")) { R(36, 40, 28, 40, mix(c, 0xE8423F, .4f)); }
		else if (!strcmp(nm, "Bulletproof Vest")) { R(34, 40, 32, 40, d); L(34, 54, 66, 54, 3, c); L(34, 66, 66, 66, 3, c); }
		A(50, 14, 8, 0, 180, 3, d); break;
	case SH_BOARD:
		rot(-20);
		RR(8, 40, 84, 18, 9, c); L(18, 49, 82, 49, 3, l);
		if (!strcmp(nm, "Skateboard")) { D(24, 64, 6, dk); D(76, 64, 6, dk); }
		if (!strcmp(nm, "Ruler")) for (int i = 0; i < 9; i++) L(16 + i * 8.5f, 40, 16 + i * 8.5f, i & 1 ? 46 : 50, 2, dk);
		if (!strcmp(nm, "Cutting Board")) D(82, 49, 3.5f, BG);
		rot(0); break;
	case SH_BANANA:
		EA(46, 24, 36, 46, 25, 150, 13, c); EA(46, 24, 36, 46, 40, 100, 4, l);
		D(78, 40, 3.5f, 0x6D4C41); L(12, 44, 6, 34, 5, 0x6D4C41); break;
	case SH_FENCE:
		for (int i = 0; i < 5; i++) { float x = 10 + i * 17; P(c, 5, x, 30, x + 6.5f, 20, x + 13, 30, x + 13, 88, x, 88); }
		R(6, 42, 88, 7, d); R(6, 68, 88, 7, d); break;
	case SH_CARROT:
		LEAF(50, 28, 36, 6, 10, 0x4CAF50); LEAF(50, 28, 52, 4, 10, 0x4CAF50); LEAF(50, 28, 66, 8, 10, 0x4CAF50);
		E(50, 30, 15, 6, c); T3(35, 30, 65, 30, 50, 94, c);
		L(42, 44, 50, 44, 2, d); L(52, 58, 58, 58, 2, d); L(44, 70, 50, 70, 2, d); break;
	case SH_BUTTERFLY:
		E(32, 38, 20, 16, c); E(68, 38, 20, 16, c); E(34, 64, 14, 12, l); E(66, 64, 14, 12, l);
		Da(30, 38, 6, wh, 150); Da(70, 38, 6, wh, 150);
		RR(46, 26, 8, 52, 4, dk); L(48, 28, 40, 12, 2, dk); L(52, 28, 60, 12, 2, dk); break;
	case SH_FRIES:
		for (int i = 0; i < 6; i++) R(26 + i * 8, 14 + (i * 7) % 12, 7, 44, (i & 1) ? c : shade(c, 1.15f));
		P(0xE8423F, 4, 22, 44, 78, 44, 70, 90, 30, 90); A(50, 44, 12, 0, 180, 3, 0xFFD23F); break;
	case SH_CAVE:
		SEMI(50, 86, 44, 58, c); Da(36, 46, 10, l, 120); SEMI(50, 86, 18, 30, dk); L(6, 86, 94, 86, 4, d); break;
	case SH_BOW:
		A(34, 50, 40, -62, 62, 7, c); L(53, 15, 53, 85, 2, dk); R(68, 44, 8, 12, 0x5D4037); break;
	case SH_ARROW:
		rot(-45); L(10, 50, 78, 50, 4, c); T3(76, 41, 96, 50, 76, 59, 0x9AA0A6);
		T3(10, 50, 24, 38, 28, 50, 0xE8423F); T3(10, 50, 24, 62, 28, 50, 0xE8423F); rot(0); break;
	case SH_CRATE:
		if (!strcmp(nm, "Toolbox")) {
			RR(12, 38, 76, 48, 6, c); R(12, 52, 76, 4, d); A(50, 38, 14, 180, 360, 6, dk); R(46, 50, 8, 10, 0xB0B7C0); break;
		}
		if (!strcmp(nm, "Container")) { R(8, 30, 84, 50, c); for (int i = 0; i < 9; i++) L(14 + i * 9, 34, 14 + i * 9, 76, 3, d); break; }
		R(16, 24, 68, 64, c); R(16, 24, 68, 64 / 3.f, l);
		L(16, 24, 84, 24, 4, d); L(16, 88, 84, 88, 4, d); L(16, 24, 16, 88, 4, d); L(84, 24, 84, 88, 4, d);
		L(16, 46, 84, 46, 3, d); L(16, 66, 84, 66, 3, d); L(20, 84, 80, 28, 5, d);
		if (!strcmp(nm, "Ant Farm")) { R(22, 30, 56, 52, 0xBFE3F5); Ra(22, 50, 56, 32, c, 255); L(30, 52, 44, 66, 3, d); L(44, 66, 66, 60, 3, d); D(36, 58, 2.5f, dk); D(58, 62, 2.5f, dk); }
		break;
	case SH_BATTERY:
		RR(30, 22, 40, 68, 6, c); RR(30, 22, 40, 22, 6, 0xD4AC0D); R(30, 34, 40, 10, 0xD4AC0D); R(42, 12, 16, 12, 0x9AA0A6);
		L(50, 54, 50, 72, 4, wh); L(41, 63, 59, 63, 4, wh); break;
	case SH_CAGE:
		A(50, 40, 32, 180, 360, 4, c);
		for (int i = 0; i < 7; i++) { float x = 22 + i * 9.33f, y = 40 - sqrtf(fmaxf(0, 1024 - (x - 50) * (x - 50))); L(x, y, x, 86, 3, c); }
		L(18, 40, 18, 86, 4, c); L(82, 40, 82, 86, 4, c); R(12, 84, 76, 6, d); ring(tx(50, 5), ty(50, 5), 4 * K, 2 * K, c, 255);
		if (!strcmp(nm, "Birdcage")) { E(50, 64, 10, 8, 0xFFD23F); D(56, 56, 6, 0xFFD23F); T3(61, 55, 67, 57, 61, 59, 0xFF9F1C); }
		break;
	case SH_FRAME:
		if (!strcmp(nm, "Canvas")) { L(30, 70, 20, 94, 4, 0x8D6E63); L(70, 70, 80, 94, 4, 0x8D6E63); L(50, 10, 50, 94, 4, 0x8D6E63); }
		R(14, 16, 72, 58, !strcmp(nm, "Canvas") ? 0xD9C9A8 : c); R(20, 22, 60, 46, wh);
		if (!strcmp(nm, "Painting")) {
			R(20, 22, 60, 46, 0x9FD3F0); SEMI(40, 68, 26, 22, 0x6FA84A); SEMI(66, 68, 20, 14, 0x4C8F3A); D(66, 34, 6, 0xFFD23F);
		}
		break;
	case SH_SPOON:
		if (!strcmp(nm, "Fork")) {
			for (int i = 0; i < 4; i++) L(38 + i * 8, 8, 38 + i * 8, 30, 4, c);
			RR(36, 26, 28, 14, 6, c); L(50, 36, 50, 92, 7, c); break;
		}
		E(50, 28, 15, 20, c); Ea(46, 24, 5, 9, wh, 150); L(50, 44, 50, 92, 7, c); break;
	case SH_BELL:
		SEMI(50, 66, 30, 50, c); R(14, 62, 72, 10, c); RR(10, 66, 80, 8, 4, d);
		Da(38, 40, 6, l, 180); D(50, 80, 8, d); ring(tx(50, 12), ty(50, 12), 6 * K, 3 * K, d, 255); break;
	default: D(50, 50, 30, c); break;
	}
	rot(0);
}

/* ---------------- baking ---------------- */
static Sprite cache[NEL];  /* small icons */
static const int SIZES[2] = {52, 104};

static void render_pass(int el, uint32_t *buf, int w, uint32_t bg, int margin) {
	for (int i = 0; i < w * w; i++) buf[i] = bg;
	gfx_target(buf, w, w);
	BG = bg; OX = OY = (float)margin; K = (w - 2.f * margin) / 100.f;
	draw_icon(el);
}

static Sprite bake(int el, int big) {
	int w = SIZES[big], m = big ? 7 : 4, rad = big ? 2 : 1;
	uint32_t *b = malloc(w * w * 4), *wt = malloc(w * w * 4);
	uint8_t *al = malloc(w * w), *ol = malloc(w * w);
	Sprite s = {w, w, malloc(w * w * 4)};
	render_pass(el, b, w, 0x000000, m);
	render_pass(el, wt, w, 0xFFFFFF, m);
	gfx_screen();
	for (int i = 0; i < w * w; i++) {
		int a = 255 - ((int)(CG(wt[i]) - CG(b[i])) + (int)(CR(wt[i]) - CR(b[i])) + (int)(CB(wt[i]) - CB(b[i]))) / 3;
		al[i] = (uint8_t)clampi(a, 0, 255);
	}
	/* outline: dilated alpha */
	for (int y = 0; y < w; y++) for (int x = 0; x < w; x++) {
		int mx = 0;
		for (int j = -rad; j <= rad; j++) for (int i = -rad; i <= rad; i++) {
			if (i * i + j * j > rad * rad + 1) continue;
			int xx = x + i, yy = y + j;
			if (xx < 0 || yy < 0 || xx >= w || yy >= w) continue;
			if (al[yy * w + xx] > mx) mx = al[yy * w + xx];
		}
		ol[y * w + x] = (uint8_t)mx;
	}
	uint32_t oc = shade(ELEMS[el].col, .42f);
	if (CR(oc) + CG(oc) + CB(oc) < 60) oc = 0x15151C;
	int sh = rad + 1;
	for (int y = 0; y < w; y++) for (int x = 0; x < w; x++) {
		int i = y * w + x;
		/* layers bottom-up: shadow, outline, icon; composite in premultiplied floats */
		float A = 0, Rr = 0, G = 0, Bb = 0;
		int sy = y - sh;
		float sa = sy >= 0 ? ol[sy * w + x] / 255.f * .28f : 0;
		A = sa;
		float oa = ol[i] / 255.f;
		Rr = Rr * (1 - oa) + CR(oc) * oa; G = G * (1 - oa) + CG(oc) * oa; Bb = Bb * (1 - oa) + CB(oc) * oa; A = A * (1 - oa) + oa;
		float ia = al[i] / 255.f;
		if (ia > 0) {
			uint32_t p = b[i];
			float r = CR(p) / ia, g = CG(p) / ia, bl = CB(p) / ia;
			Rr = Rr * (1 - ia) + fminf(r, 255) * ia; G = G * (1 - ia) + fminf(g, 255) * ia; Bb = Bb * (1 - ia) + fminf(bl, 255) * ia;
			A = A * (1 - ia) + ia;
		}
		if (A < 1.f / 255) { s.px[i] = 0; continue; }
		s.px[i] = ((uint32_t)(A * 255 + .5f) << 24) | RGB((int)(Rr / A), (int)(G / A), (int)(Bb / A));
	}
	free(b); free(wt); free(al); free(ol);
	return s;
}

/* ---------------- icon pack ---------------- */
static FILE *pack;
static long packData;
static int packN, packTried, packEnt[NEL];

static unsigned be16(const unsigned char *p) { return p[0] << 8 | p[1]; }

static void pack_open(void) {
	packTried = 1;
	for (int e = 0; e < NEL; e++) packEnt[e] = -1;
	const char *path = plat_icon_path();
	if (!path || !(pack = fopen(path, "rb"))) return;
	unsigned char h[12], nm[32];
	if (fread(h, 1, 12, pack) != 12 || memcmp(h, "AL2I", 4) || (int)be16(h + 6) != SIZES[0] || (int)be16(h + 8) != SIZES[1]) {
		fclose(pack); pack = NULL; return;
	}
	packN = be16(h + 4);
	for (int i = 0; i < packN && fread(nm, 1, 32, pack) == 32; i++) {
		nm[31] = 0;
		for (int e = 0; e < NEL; e++) if (packEnt[e] < 0 && !strcmp(ELEMS[e].name, (const char *)nm)) { packEnt[e] = i; break; }
	}
	packData = 12 + 32L * packN;
}

/* the pack's picture for an element, or a sprite with no pixels when it has none */
static Sprite pack_load(int el, int big) {
	Sprite s = {0, 0, NULL};
	if (!packTried) pack_open();
	if (!pack || packEnt[el] < 0) return s;
	int a = SIZES[0] * SIZES[0], b = SIZES[1] * SIZES[1], n = big ? b : a, w = SIZES[big], seen = 0;
	unsigned char *raw = malloc(n * 4);
	if (!raw) return s;
	if (fseek(pack, packData + (long)packEnt[el] * (a + b) * 4 + (big ? a * 4 : 0), SEEK_SET) || (int)fread(raw, 4, n, pack) != n) { free(raw); return s; }
	s.w = s.h = w;
	if (!(s.px = malloc(n * 4))) { free(raw); return s; }
	for (int i = 0; i < n; i++) {
		const unsigned char *p = raw + i * 4;
		s.px[i] = (uint32_t)p[3] << 24 | RGB(p[0], p[1], p[2]);
		seen |= p[3];
	}
	free(raw);
	if (!seen) { free(s.px); s.px = NULL; }
	return s;
}

/* small icons stay cached; only the latest big one is kept, as all of them would not fit in MEM1 */
const Sprite *icon_get(int el, int big) {
	static int bigEl = -1;
	static Sprite bigS;
	Sprite *s = big ? &bigS : &cache[el];
	if (big && bigEl != el) { free(bigS.px); bigS.px = NULL; bigEl = el; }
	if (!s->px) *s = pack_load(el, big);
	if (!s->px) *s = bake(el, big);
	return s;
}
int icon_size(int big) { return SIZES[big]; }
