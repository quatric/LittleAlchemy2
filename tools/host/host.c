/* Desktop test harness: runs the game with scripted input and writes PPM screenshots.
 * usage: host <outdir> [all|unlock|icons]
 *   all:    start with every element discovered
 *   unlock: start one discovery short of the first progress unlock, then make Steam */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../source/game.h"
#include "../../source/gfx.h"
#include "../../source/icons.h"
#include "../../source/elements.h"

static const char *outdir;
static int allMode, unlockMode;
static unsigned char *ttf;

int plat_load(char *buf, int max) {
	int n = 0;
	if (allMode)
		for (int e = 0; e < NEL; e++) n += snprintf(buf + n, max - n, "%s\n", ELEMS[e].name);
	if (unlockMode) {
		/* NSTART + picked = first progress threshold - 1, leaving out Steam and every unlockable element */
		int need = 0, picked = 0;
		for (int u = 0; u < NUNLOCK; u++) if (UNLOCKS[u].progress && (!need || UNLOCKS[u].progress < need)) need = UNLOCKS[u].progress;
		for (int e = 0; e < NEL && picked < need - 1 - NSTART; e++) {
			int skip = !strcmp(ELEMS[e].name, "Steam");
			for (int k = 0; k < NSTART; k++) skip |= STARTERS[k] == e;
			for (int u = 0; u < NUNLOCK; u++) skip |= UNLOCKS[u].el == e;
			for (int u = 0; u < NUNLOCK; u++) for (int k = 0; k < UNLOCKS[u].count; k++) skip |= UNLOCK_ELS[UNLOCKS[u].first + k] == e;
			if (skip) continue;
			n += snprintf(buf + n, max - n, "%s\n", ELEMS[e].name);
			picked++;
		}
	}
	return n ? n : -1;
}
void plat_save(const char *buf, int len) { (void)buf; (void)len; }
/* LA2_ICONS overrides the pack; set it to "" to see the drawn icons */
const char *plat_icon_path(void) { const char *p = getenv("LA2_ICONS"); return p ? (*p ? p : NULL) : "apps/LittleAlchemy2/icons.bin"; }
void plat_sfx(int id) { (void)id; }
void plat_rumble(int ms) { (void)ms; }

static void shot(const char *name) {
	char p[512];
	snprintf(p, sizeof p, "%s/%s.ppm", outdir, name);
	FILE *f = fopen(p, "wb");
	fprintf(f, "P6\n%d %d\n255\n", SW, SH);
	for (int y = 0; y < SH; y++) for (int x = 0; x < SW; x++) {
		uint32_t c = fb[y][x];
		unsigned char px[3] = {CR(c), CG(c), CB(c)};
		fwrite(px, 1, 3, f);
	}
	fclose(f);
}

static Input in;
static void step(int n) {
	for (int i = 0; i < n; i++) {
		game_update(&in, 1 / 60.f);
		in.down = in.up = 0;
	}
	game_draw();
}
static void moveto(float x, float y, int frames) {
	float x0 = in.x, y0 = in.y;
	for (int i = 1; i <= frames; i++) { in.x = x0 + (x - x0) * i / frames; in.y = y0 + (y - y0) * i / frames; step(1); }
}
static void press(uint32_t b) { in.held |= b; in.down |= b; step(1); }
static void release(uint32_t b) { in.held &= ~b; in.up |= b; step(1); }
static void dragto(float x0, float y0, float x1, float y1) {
	moveto(x0, y0, 8); press(BTN_A); moveto(x1, y1, 20); release(BTN_A);
}

static void icon_sheets(void) {
	for (int page = 0; page * 48 < NEL; page++) {
		for (int y = 0; y < SH; y++) for (int x = 0; x < SW; x++) fb[y][x] = 0xF4EFE6;
		for (int i = 0; i < 48 && page * 48 + i < NEL; i++) {
			int el = page * 48 + i, cx = 40 + (i % 8) * 80, cy = 10 + (i / 8) * 78;
			const Sprite *s = icon_get(el, 0);
			blit(s, cx - s->w / 2, cy, 255);
			text(fSmall, cx, cy + 54, ELEMS[el].name, 0x3E342B, 255, AL_CENTER);
		}
		char n[32]; snprintf(n, sizeof n, "icons%d", page); shot(n);
	}
}

int main(int argc, char **argv) {
	outdir = argc > 1 ? argv[1] : ".";
	allMode = argc > 2 && !strcmp(argv[2], "all");
	unlockMode = argc > 2 && !strcmp(argv[2], "unlock");
	FILE *f = fopen("data/font.ttf", "rb");
	if (!f) { fprintf(stderr, "run from project root\n"); return 1; }
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	ttf = malloc(n); fread(ttf, 1, n, f); fclose(f);
	gfx_init(ttf);
	if (argc > 2 && !strcmp(argv[2], "icons")) { icon_sheets(); return 0; }
	if (argc > 2 && !strcmp(argv[2], "banner")) {
		/* 128x48 Homebrew Channel icon in the top-left corner of the screen buffer */
		for (int y = 0; y < 48; y++) for (int x = 0; x < 128; x++) fb[y][x] = mix(0x380028, 0x260026, y / 48.f);
		static const char *pick[2] = {"Water", "Fire"};
		for (int k = 0; k < 2; k++) for (int e = 0; e < NEL; e++) if (!strcmp(ELEMS[e].name, pick[k]))
			blit_scaled(icon_get(e, 0), 15 + k * 14, 16 + k * 16, 30, 255);
		text(fSmall, 84, 15, "Little Alchemy 2", 0xD79F47, 255, AL_CENTER);
		FILE *o = fopen(argv[1], "wb");
		fprintf(o, "P6\n128 48\n255\n");
		for (int y = 0; y < 48; y++) for (int x = 0; x < 128; x++) { unsigned char px[3] = {CR(fb[y][x]), CG(fb[y][x]), CB(fb[y][x])}; fwrite(px, 1, 3, o); }
		fclose(o);
		return 0;
	}
	game_init();
	in.valid = 1; in.x = 200; in.y = 250;
	if (unlockMode) {
		step(30); shot("u1_start");
		dragto(160, 150, 340, 150);
		step(20); shot("u2_steam");
		press(BTN_A); release(BTN_A); step(30); shot("u3_unlocked");
		press(BTN_A); release(BTN_A); step(30); shot("u4_after");
		printf("ok\n");
		return 0;
	}
	step(30); shot("01_start");
	/* water (160,150) onto fire (340,150) */
	dragto(160, 150, 340, 150);
	step(20); shot("02_discovery");
	press(BTN_A); release(BTN_A); step(30);
	/* earth (160,320) onto air (340,320) */
	moveto(160, 320, 8); press(BTN_A); moveto(330, 318, 20); shot("03_dragging");
	release(BTN_A); step(40); press(BTN_A); release(BTN_A); step(30);
	/* grab Fire from the library onto the dust */
	moveto(530, 100, 10); step(2); shot("04_hover");
	press(BTN_1); release(BTN_1); step(10); shot("05_hint");
	press(BTN_PLUS); release(BTN_PLUS); step(10); shot("06_menu");
	press(BTN_PLUS); release(BTN_PLUS);
	in.held |= BTN_DOWN; step(40); in.held = 0; step(10); shot("07_scrolled");
	printf("ok\n");
	return 0;
}
