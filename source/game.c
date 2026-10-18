/* Little Alchemy 2 for Wii - combine elements to discover new ones. Platform-independent game logic and drawing. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "game.h"
#include "gfx.h"
#include "icons.h"
#include "elements.h"

/* layout (640x480, kept inside the TV-safe area): tool strip | workspace | element list */
#define TOPH 0
#define TOOLW 58
#define PANX 452
#define COLS 1
#define CW (SBX - 6 - LIBX)
#define CH 58
#define LIBX (PANX + 10)
#define LIBY 16
#define LIBB (SH - 16)
#define SBX 624
/* palette */
#define C_WORK 0x260026
#define C_SIDE 0x380028
#define C_GOLD 0xD79F47
#define C_GOLDHI 0xFFC46B
#define C_DIM 0x7E5A58
#define C_POP 0xFCC46B
#define C_POPHEAD 0xFAA620
#define C_INK 0x4D4131
#define ICON 52
#define MAXITEM 64
#define MAXPART 240
#define MAXPOP 8

typedef struct { int el; float x, y, pop, shake, vx, vy; } Item;
typedef struct { float x, y, vx, vy, life, max, r; uint32_t c; } Part;
typedef struct { int el, a, b; } Pop;

static uint32_t bg[SH][SW];
static unsigned char disc_[NEL], done_[NEL];
static int ndisc, lib[NEL], nlib, hideDone;
static Item items[MAXITEM];
static int nitems;
static Part parts[MAXPART];
static int drag = -1, sbDrag, hoverItem = -1, hoverLib = -1, hoverTool = -1, target = -1;
static float dragOX, dragOY, scr, scrT, t, cursorX = 210, cursorY = 240;
static int cursorOK;
static Pop popq[MAXPOP];
static int npop;
static float popT;
static char toast[96];
static float toastT;
static int flashEl = -1;
static float flashT;
static int menuOpen, menuSel, confirmReset, quit, musicOn = 1, sfxOn = 1;
static float lastTapT = -10, holdT;
static int lastTapItem = -1;

static float rnd(void) { return (float)rand() / (float)RAND_MAX; }
static void say(const char *s) { snprintf(toast, sizeof toast, "%s", s); toastT = 3.f; }

/* ---------------- progress ---------------- */
static int cmp_name(const void *a, const void *b) { return strcasecmp(ELEMS[*(const int *)a].name, ELEMS[*(const int *)b].name); }

static void rebuild_lib(void) {
	for (int e = 0; e < NEL; e++) done_[e] = 1;
	for (int i = 0; i < NRECIPE; i++)
		if (!disc_[RECIPES[i].r]) done_[RECIPES[i].a] = done_[RECIPES[i].b] = 0;
	nlib = 0;
	for (int e = 0; e < NEL; e++)
		if (disc_[e] && !(hideDone && done_[e])) lib[nlib++] = e;
	qsort(lib, nlib, sizeof lib[0], cmp_name);
}

/* index of an unlock rule that is now met for an element not yet discovered, or -1 */
static int next_unlock(void) {
	for (int i = 0; i < NUNLOCK; i++) {
		const UnlockDef *u = &UNLOCKS[i];
		if (disc_[u->el]) continue;
		int n = 0;
		for (int k = 0; k < u->count; k++) n += disc_[UNLOCK_ELS[u->first + k]];
		if (u->progress ? ndisc >= u->progress : n >= u->min) return i;
	}
	return -1;
}

static void save(void) {
	static char buf[NEL * 32];
	int n = 0;
	n += snprintf(buf, sizeof buf, "#music=%d\n#sfx=%d\n", musicOn, sfxOn);
	for (int e = 0; e < NEL; e++)
		if (disc_[e]) n += snprintf(buf + n, sizeof buf - n, "%s\n", ELEMS[e].name);
	plat_save(buf, n);
}

static void load(void) {
	static char buf[NEL * 32 + 1];
	memset(disc_, 0, sizeof disc_);
	for (int i = 0; i < NSTART; i++) disc_[STARTERS[i]] = 1;
	int n = plat_load(buf, sizeof buf - 1);
	if (n > 0) {
		buf[n] = 0;
		for (char *ln = strtok(buf, "\r\n"); ln; ln = strtok(NULL, "\r\n")) {
			/* settings lines start with '#'; everything else is a discovered element */
			if (!strncmp(ln, "#music=", 7)) musicOn = atoi(ln + 7) != 0;
			else if (!strncmp(ln, "#sfx=", 5)) sfxOn = atoi(ln + 5) != 0;
			else for (int e = 0; e < NEL; e++)
				if (!strcmp(ELEMS[e].name, ln)) { disc_[e] = 1; break; }
		}
	}
	plat_audio(musicOn, sfxOn);
	ndisc = 0;
	for (int e = 0; e < NEL; e++) ndisc += disc_[e];
	for (int u; (u = next_unlock()) >= 0; ndisc++) disc_[UNLOCKS[u].el] = 1;
	rebuild_lib();
}

/* ---------------- workspace ---------------- */
static int add_item(int el, float x, float y) {
	if (nitems >= MAXITEM) { say("The table is full - press - to clear it"); return -1; }
	Item *it = &items[nitems];
	memset(it, 0, sizeof *it);
	it->el = el; it->x = x; it->y = y;
	return nitems++;
}
static void remove_item(int i) {
	if (i < 0 || i >= nitems) return;
	if (drag == i) drag = -1;
	else if (drag > i) drag--;
	memmove(&items[i], &items[i + 1], (nitems - i - 1) * sizeof(Item));
	nitems--;
}
static void burst(float x, float y, uint32_t c, int n, float speed) {
	for (int k = 0; k < MAXPART && n > 0; k++) {
		if (parts[k].life > 0) continue;
		float a = rnd() * 6.2832f, v = speed * (.35f + rnd() * .65f);
		parts[k] = (Part){x, y, cosf(a) * v, sinf(a) * v - 40, 0, .5f + rnd() * .5f, 2 + rnd() * 3, c};
		parts[k].life = parts[k].max;
		n--;
	}
}
static void clamp_item(Item *it) {
	it->x = clampf(it->x, TOOLW + 30, PANX - 32);
	it->y = clampf(it->y, 40, SH - 50);
}
static int item_at(float x, float y, int skip) {
	for (int i = nitems - 1; i >= 0; i--) {
		if (i == skip) continue;
		float dx = x - items[i].x, dy = y - items[i].y;
		if (dx * dx + dy * dy < 30 * 30) return i;
	}
	return -1;
}
static int lib_at(float x, float y) {
	if (x < PANX || x >= LIBX + CW || y < LIBY || y >= LIBB) return -1;
	if (x < LIBX) x = LIBX;
	int c = (int)((x - LIBX) / CW), r = (int)((y - LIBY + scr) / CH), i = r * COLS + c;
	return i < nlib ? i : -1;
}
static float lib_max_scroll(void) {
	float m = ((nlib + COLS - 1) / COLS) * CH - (LIBB - LIBY);
	return m > 0 ? m : 0;
}
static void focus_lib(int el) {
	for (int i = 0; i < nlib; i++)
		if (lib[i] == el) { scrT = clampf((i / COLS) * CH - (LIBB - LIBY) / 2.f + CH / 2.f, 0, lib_max_scroll()); break; }
	flashEl = el; flashT = 3.f;
}

/* a = b = -1: unlocked by progress rather than made from a recipe */
static void discover(int el, int a, int b) {
	disc_[el] = 1;
	ndisc++;
	if (npop < MAXPOP) popq[npop++] = (Pop){el, a, b};
	if (npop == 1) { popT = 0; plat_sfx(SFX_POP); }
	for (int u; (u = next_unlock()) >= 0;) {
		disc_[UNLOCKS[u].el] = 1;
		ndisc++;
		if (npop < MAXPOP) popq[npop++] = (Pop){UNLOCKS[u].el, -1, -1};
	}
	rebuild_lib();
	save();
	plat_sfx(SFX_DISCOVER);
	plat_rumble(120);
}

static void try_combine(int i, int j) {
	int ea = items[i].el, eb = items[j].el, a = ea < eb ? ea : eb, b = ea < eb ? eb : ea;
	int res[8], nr = 0;
	for (int k = 0; k < NRECIPE && nr < 8; k++)
		if (RECIPES[k].a == a && RECIPES[k].b == b) res[nr++] = RECIPES[k].r;
	if (!nr) {
		float dx = items[i].x - items[j].x, dy = items[i].y - items[j].y, l = sqrtf(dx * dx + dy * dy) + .01f;
		items[i].vx = dx / l * 260; items[i].vy = dy / l * 260;
		items[i].shake = .35f; items[j].shake = .35f;
		plat_sfx(SFX_FAIL);
		return;
	}
	float mx = (items[i].x + items[j].x) * .5f, my = (items[i].y + items[j].y) * .5f;
	if (i > j) { remove_item(i); remove_item(j); } else { remove_item(j); remove_item(i); }
	int fresh = 0;
	for (int k = 0; k < nr; k++) {
		float ox = nr > 1 ? (k - (nr - 1) * .5f) * 64 : 0;
		int n = add_item(res[k], mx + ox, my);
		if (n >= 0) { clamp_item(&items[n]); burst(items[n].x, items[n].y, ELEMS[res[k]].col, 18, 160); }
		if (!disc_[res[k]]) { discover(res[k], ea, eb); fresh = 1; }
	}
	if (!fresh) plat_sfx(SFX_COMBINE);
}

static void hint(void) {
	static int cand[NRECIPE];
	int n = 0;
	for (int k = 0; k < NRECIPE; k++)
		if (disc_[RECIPES[k].a] && disc_[RECIPES[k].b] && !disc_[RECIPES[k].r]) cand[n++] = k;
	if (!n) { say("You've discovered everything. Amazing!"); return; }
	const RecipeDef *r = &RECIPES[cand[rand() % n]];
	int el = rand() & 1 ? r->a : r->b;
	if (hideDone) { hideDone = 0; rebuild_lib(); }
	char b[96];
	snprintf(b, sizeof b, "Hint: %s can still make something new", ELEMS[el].name);
	say(b);
	focus_lib(el);
	plat_sfx(SFX_CLICK);
}

/* ---------------- menu ---------------- */
enum { M_RESUME, M_CLEAR, M_HINT, M_HIDE, M_MUSIC, M_SFX, M_RESET, M_QUIT, M_COUNT };
static const char *menu_label(int i) {
	switch (i) {
	case M_RESUME: return "Resume";
	case M_CLEAR: return "Clear the table";
	case M_HINT: return "Get a hint";
	case M_HIDE: return hideDone ? "Show finished elements" : "Hide finished elements";
	case M_MUSIC: return musicOn ? "Music: on" : "Music: off";
	case M_SFX: return sfxOn ? "Sound effects: on" : "Sound effects: off";
	case M_RESET: return confirmReset ? "Really erase progress? (A)" : "Reset progress";
	default: return "Exit to Homebrew Channel";
	}
}
#define MX 180
#define MY 64
#define MWW 280
#define MIH 38
static void menu_do(int i) {
	plat_sfx(SFX_CLICK);
	if (i != M_RESET) confirmReset = 0;
	switch (i) {
	case M_RESUME: menuOpen = 0; break;
	case M_CLEAR: nitems = 0; drag = -1; menuOpen = 0; break;
	case M_HINT: menuOpen = 0; hint(); break;
	case M_HIDE: hideDone = !hideDone; rebuild_lib(); scrT = clampf(scrT, 0, lib_max_scroll()); break;
	case M_RESET:
		if (!confirmReset) { confirmReset = 1; break; }
		confirmReset = 0; nitems = 0; drag = -1;
		memset(disc_, 0, sizeof disc_);
		for (int k = 0; k < NSTART; k++) disc_[STARTERS[k]] = 1;
		ndisc = NSTART; rebuild_lib(); save(); scr = scrT = 0; menuOpen = 0;
		say("Progress erased. Back to the four elements.");
		break;
	case M_MUSIC: musicOn = !musicOn; plat_audio(musicOn, sfxOn); save(); break;
	case M_SFX: sfxOn = !sfxOn; plat_audio(musicOn, sfxOn); save(); plat_sfx(SFX_CLICK); break;
	case M_QUIT: quit = 1; break;
	}
}

/* ---------------- tool strip ---------------- */
enum { T_CLEAN, T_HIDE, T_HINT, T_MENU, T_COUNT };
#define TOOLX (TOOLW / 2.f)
static float tool_y(int i) { return 44 + i * 64; }
static int tool_at(float x, float y) {
	if (x >= TOOLW) return -1;
	for (int i = 0; i < T_COUNT; i++) if (fabsf(y - tool_y(i)) < 30) return i;
	return -1;
}
static const char *tool_label(int i) {
	switch (i) {
	case T_CLEAN: return "clean up";
	case T_HIDE: return hideDone ? "show all" : "hide done";
	case T_HINT: return "hints";
	default: return "settings";
	}
}
/* line icons in the style of the tool strip: centred on (x, y), about 26px across */
static void tool_icon(int i, float x, float y, uint32_t c) {
	const float w = 1.8f;
	switch (i) {
	case T_CLEAN: {
		line_aa(x + 11, y - 12, x + 1, y - 2, w, c, 255);
		float br[] = {x - 1, y - 4, x + 5, y + 2, x - 4, y + 12, x - 13, y + 3};
		for (int k = 0; k < 4; k++) line_aa(br[k * 2], br[k * 2 + 1], br[(k + 1) % 4 * 2], br[(k + 1) % 4 * 2 + 1], w, c, 255);
		line_aa(x - 6, y + 1, x - 2, y + 5, w, c, 255);
		line_aa(x - 9, y + 4, x - 5, y + 8, w, c, 255);
		break;
	}
	case T_HIDE:
		ellipse(x, y, 13, 8, c, 255);
		ellipse(x, y, 11.2f, 6.2f, C_WORK, 255);
		ring(x, y, 4, w, c, 255);
		disc(x, y, 1.6f, c, 255);
		if (hideDone) { line_aa(x - 12, y + 10, x + 12, y - 10, w + 1.6f, C_WORK, 255); line_aa(x - 12, y + 10, x + 12, y - 10, w, c, 255); }
		break;
	case T_HINT:
		ring(x, y - 2, 7, w, c, 255);
		line_aa(x - 3, y + 7, x + 3, y + 7, w, c, 255);
		line_aa(x - 2, y + 10, x + 2, y + 10, w, c, 255);
		for (int k = 0; k < 7; k++) {
			float a = 3.1416f + k * 3.1416f / 6;
			line_aa(x + cosf(a) * 10.5f, y - 2 + sinf(a) * 10.5f, x + cosf(a) * 13.5f, y - 2 + sinf(a) * 13.5f, w, c, 255);
		}
		break;
	default:
		for (int k = 0; k < 3; k++) {
			float yy = y - 7 + k * 7, kx = x + (k == 1 ? 5 : -4);
			line_aa(x - 13, yy, kx - 3, yy, w, c, 255);
			line_aa(kx + 3, yy, x + 13, yy, w, c, 255);
			ring(kx, yy, 2.6f, w, c, 255);
		}
	}
}

static void lower(char *d, const char *s, int n) {
	int i = 0;
	for (; s[i] && i < n - 1; i++) d[i] = s[i] >= 'A' && s[i] <= 'Z' ? s[i] + 32 : s[i];
	d[i] = 0;
}

/* ---------------- init / update ---------------- */
static void build_bg(void) {
	for (int y = 0; y < SH; y++) for (int x = 0; x < SW; x++) fb[y][x] = x >= PANX ? C_SIDE : C_WORK;
	/* soft shadow where the list meets the workspace */
	for (int k = 0; k < 8; k++) fill_rect(PANX - 1 - k, 0, 1, SH, 0x1B001C, 90 - k * 11);
	/* the Little Alchemy 2 logo in the corner of the workspace */
	const unsigned char *lg = plat_logo();
	if (lg) {
		Sprite s = {lg[0] << 8 | lg[1], lg[2] << 8 | lg[3], NULL};
		s.px = malloc(s.w * s.h * 4);
		for (int i = 0; s.px && i < s.w * s.h; i++) {
			const unsigned char *p = lg + 4 + i * 4;
			s.px[i] = (uint32_t)p[3] << 24 | RGB(p[0], p[1], p[2]);
		}
		if (s.px) blit(&s, PANX - 16 - s.w, SH - 12 - s.h, 255);
		free(s.px);
	}
	memcpy(bg, fb, sizeof bg);
}

void game_init(void) {
	srand(12345);
	build_bg();
	load();
	static const float sx[4] = {160, 340, 160, 340}, sy[4] = {150, 150, 320, 320};
	for (int i = 0; i < NSTART && i < 4; i++) { int n = add_item(STARTERS[i], sx[i], sy[i]); items[n].pop = .0f; }
	if (ndisc <= NSTART) say("Drag one element onto another to combine them");
}

int game_quit(void) { return quit; }

static void update_items(float dt) {
	for (int i = 0; i < nitems; i++) {
		Item *it = &items[i];
		if (it->pop < 1) it->pop = fminf(1, it->pop + dt * 3.5f);
		if (it->shake > 0) it->shake -= dt;
		if (i != drag && (it->vx != 0 || it->vy != 0)) {
			it->x += it->vx * dt; it->y += it->vy * dt;
			float f = expf(-dt * 9);
			it->vx *= f; it->vy *= f;
			if (fabsf(it->vx) < 4 && fabsf(it->vy) < 4) it->vx = it->vy = 0;
			clamp_item(it);
		}
	}
	for (int k = 0; k < MAXPART; k++) {
		Part *p = &parts[k];
		if (p->life <= 0) continue;
		p->life -= dt; p->x += p->vx * dt; p->y += p->vy * dt; p->vy += 260 * dt;
		p->vx *= expf(-dt * 2);
	}
}

void game_update(const Input *in, float dt) {
	t += dt;
	if (toastT > 0) toastT -= dt;
	if (flashT > 0) flashT -= dt;
	cursorOK = in->valid;
	if (in->valid) { cursorX = in->x; cursorY = in->y; }
	float cx = cursorX, cy = cursorY;
	update_items(dt);
	float sm = fminf(1, dt * 14);
	scr += (scrT - scr) * sm;

	if (in->down & BTN_HOME) { quit = 1; return; }

	/* discovery card swallows input until dismissed */
	if (npop) {
		popT += dt;
		if ((in->down & (BTN_A | BTN_B)) && popT > .35f) popT = 99;
		if (popT > 3.2f) {
			memmove(popq, popq + 1, (npop - 1) * sizeof(Pop));
			npop--; popT = 0;
			if (npop) plat_sfx(SFX_POP);
		}
		return;
	}

	if (menuOpen) {
		if (in->down & (BTN_PLUS | BTN_B)) { menuOpen = 0; confirmReset = 0; plat_sfx(SFX_CLICK); return; }
		if (in->down & BTN_UP) { menuSel = (menuSel + M_COUNT - 1) % M_COUNT; plat_sfx(SFX_CLICK); }
		if (in->down & BTN_DOWN) { menuSel = (menuSel + 1) % M_COUNT; plat_sfx(SFX_CLICK); }
		int over = -1;
		if (cursorOK && cx >= MX + 16 && cx < MX + MWW - 16 && cy >= MY + 50 && cy < MY + 50 + M_COUNT * MIH) {
			over = (int)((cy - MY - 50) / MIH);
			if (over != menuSel) { if (menuSel != M_RESET) confirmReset = 0; menuSel = over; }
		}
		if (in->down & BTN_A) menu_do(menuSel);
		return;
	}

	hoverTool = cursorOK && drag < 0 && !sbDrag ? tool_at(cx, cy) : -1;
	int tool = -1;
	if ((in->down & BTN_A) && hoverTool >= 0) tool = hoverTool;
	else if (in->down & BTN_PLUS) tool = T_MENU;
	else if (in->down & BTN_MINUS) tool = T_CLEAN;
	else if (in->down & BTN_1) tool = T_HINT;
	else if (in->down & BTN_2) tool = T_HIDE;
	switch (tool) {
	case T_MENU: menuOpen = 1; menuSel = 0; confirmReset = 0; plat_sfx(SFX_CLICK); return;
	case T_CLEAN: if (nitems) plat_sfx(SFX_DELETE); nitems = 0; drag = -1; break;
	case T_HINT: hint(); break;
	case T_HIDE:
		hideDone = !hideDone; rebuild_lib(); scrT = clampf(scrT, 0, lib_max_scroll());
		say(hideDone ? "Finished elements hidden" : "Showing all elements");
		plat_sfx(SFX_CLICK);
		break;
	}
	if (tool >= 0 && tool == hoverTool && (in->down & BTN_A)) return;

	/* library scrolling: d-pad, pointer at the panel edges, scrollbar drag */
	float ms = lib_max_scroll();
	/* held d-pad scrolling speeds up the longer it is held */
	holdT = in->held & (BTN_UP | BTN_DOWN) ? holdT + dt : 0;
	float sp = 520 * (1 + fminf(holdT, 2.5f) * 3);
	if (in->held & BTN_UP) scrT -= sp * dt;
	if (in->held & BTN_DOWN) scrT += sp * dt;
	scrT += in->wheel * CH;
	if (in->down & BTN_LEFT) scrT -= (LIBB - LIBY) - CH;
	if (in->down & BTN_RIGHT) scrT += (LIBB - LIBY) - CH;
	if (cursorOK && cx >= PANX && !sbDrag) {
		if (cy < LIBY + 18 && cy > TOPH) scrT -= 420 * dt * (LIBY + 18 - cy) / 26;
		else if (cy > LIBB - 22) scrT += 420 * dt * (cy - (LIBB - 22)) / 22;
	}
	if (sbDrag) {
		if (!(in->held & BTN_A)) sbDrag = 0;
		else scrT = (cy - LIBY) / (LIBB - LIBY) * (ms + (LIBB - LIBY)) - (LIBB - LIBY) / 2.f;
	}
	scrT = clampf(scrT, 0, ms);
	if (scr > ms) scr = ms;

	hoverItem = cursorOK ? item_at(cx, cy, drag) : -1;
	hoverLib = cursorOK && drag < 0 ? lib_at(cx, cy) : -1;

	if ((in->down & BTN_A) && cursorOK && drag < 0) {
		if (cx >= SBX - 4 && cy >= LIBY && ms > 0) sbDrag = 1;
		else if (hoverItem >= 0 && cx < PANX) {
			int i = hoverItem;
			if (i == lastTapItem && t - lastTapT < .35f) {
				int n = add_item(items[i].el, items[i].x + 18, items[i].y + 18);
				if (n >= 0) { clamp_item(&items[n]); plat_sfx(SFX_CLONE); }
				lastTapItem = -1;
			} else {
				/* bring to front and grab */
				Item tmp = items[i];
				memmove(&items[i], &items[i + 1], (nitems - i - 1) * sizeof(Item));
				items[nitems - 1] = tmp;
				drag = nitems - 1;
				dragOX = tmp.x - cx; dragOY = tmp.y - cy;
				items[drag].vx = items[drag].vy = 0;
				lastTapItem = drag; lastTapT = t;
				plat_sfx(SFX_PICK);
			}
		} else if (hoverLib >= 0) {
			int n = add_item(lib[hoverLib], cx, cy);
			if (n >= 0) { drag = n; dragOX = dragOY = 0; items[n].pop = .4f; plat_sfx(SFX_PICK); }
			lastTapItem = -1;
		}
	}
	if (drag >= 0) {
		items[drag].x = cx + dragOX; items[drag].y = cy + dragOY;
		target = item_at(items[drag].x, items[drag].y, drag);
		if (items[drag].x >= PANX) target = -1;
		if (!(in->held & BTN_A)) {
			int i = drag;
			drag = -1;
			if (items[i].x >= PANX - 8) { burst(items[i].x, items[i].y, 0xB0A898, 8, 90); remove_item(i); plat_sfx(SFX_DELETE); }
			else if (target >= 0) try_combine(i, target);
			else { clamp_item(&items[i]); plat_sfx(SFX_DROP); }
			target = -1;
		}
	} else target = -1;

	if ((in->down & BTN_B) && hoverItem >= 0 && cx < PANX) {
		burst(items[hoverItem].x, items[hoverItem].y, ELEMS[items[hoverItem].el].col, 10, 110);
		remove_item(hoverItem);
		hoverItem = -1;
		plat_sfx(SFX_DELETE);
	}
}

/* ---------------- drawing ---------------- */
/* left-aligned name that wraps onto two lines (or drops to a smaller size) when it is too wide */
static void name_left(float x, float cy, const char *s, uint32_t c, float maxw) {
	Font *f = fName;
	if (text_w(f, s) <= maxw) { text(f, x, cy - text_h(f) / 2, s, c, 255, AL_LEFT); return; }
	int best = -1; float bw = 1e9f;
	char buf[64];
	snprintf(buf, sizeof buf, "%s", s);
	for (int i = 0; buf[i]; i++) if (buf[i] == ' ') {
		buf[i] = 0;
		float w = fmaxf(text_w(f, buf), text_w(f, buf + i + 1));
		buf[i] = ' ';
		if (w < bw) { bw = w; best = i; }
	}
	if (best < 0) { text(fSmall, x, cy - text_h(fSmall) / 2, s, c, 255, AL_LEFT); return; }
	buf[best] = 0;
	Font *g = bw <= maxw ? f : fSmall;
	float lh = text_h(g) - 3;
	text(g, x, cy - lh, buf, c, 255, AL_LEFT);
	text(g, x, cy - 2, buf + best + 1, c, 255, AL_LEFT);
}

static void draw_tools(void) {
	for (int i = 0; i < T_COUNT; i++) {
		float y = tool_y(i);
		uint32_t c = i == hoverTool ? C_GOLDHI : C_GOLD;
		tool_icon(i, TOOLX, y - 6, c);
		text(fTiny, TOOLX, y + 12, tool_label(i), c, 255, AL_CENTER);
	}
	char b[32];
	snprintf(b, sizeof b, "%d/%d", ndisc, NEL);
	text(fSmall, TOOLX, SH - 42, b, C_GOLD, 255, AL_CENTER);
}

static void draw_library(void) {
	gfx_clip(PANX + 1, LIBY - 4, SW, LIBB + 4);
	int r0 = (int)(scr / CH), r1 = (int)((scr + (LIBB - LIBY)) / CH) + 1;
	for (int r = r0; r <= r1; r++) for (int c = 0; c < COLS; c++) {
		int i = r * COLS + c;
		if (i >= nlib) break;
		int el = lib[i];
		float x = LIBX + c * CW, y = LIBY + r * CH - scr;
		if (el == flashEl && flashT > 0) {
			int a = (int)(70 + 50 * sinf(t * 10));
			rrect(x - 4, y + 1, CW + 4, CH - 2, 10, C_GOLD, a);
		} else if (i == hoverLib) rrect(x - 4, y + 1, CW + 4, CH - 2, 10, 0x4C0C3A, 255);
		const Sprite *s = icon_get(el, 0);
		blit(s, (int)x, (int)(y + (CH - s->h) / 2), done_[el] ? 110 : 255);
		char nm[64];
		lower(nm, ELEMS[el].name, sizeof nm);
		name_left(x + s->w + 8, y + CH / 2.f, nm, done_[el] ? C_DIM : C_GOLD, CW - s->w - 10);
	}
	gfx_noclip();
	/* fades at the edges of the scrolling list */
	for (int k = 0; k < 10; k++) {
		fill_rect(PANX, LIBY - 4 + k, SW - PANX, 1, C_SIDE, 255 - k * 25);
		fill_rect(PANX, LIBB + 3 - k, SW - PANX, 1, C_SIDE, 255 - k * 25);
	}
	fill_rect(PANX, 0, SW - PANX, LIBY - 4, C_SIDE, 255);
	fill_rect(PANX, LIBB + 4, SW - PANX, SH - LIBB - 4, C_SIDE, 255);
	float ms = lib_max_scroll();
	if (ms > 0) {
		float vh = LIBB - LIBY, th = fmaxf(30, vh * vh / (vh + ms)), ty = LIBY + (vh - th) * scr / ms;
		rrect(SBX, LIBY, 5, vh, 2.5f, 0x2C0020, 255);
		rrect(SBX, ty, 5, th, 2.5f, sbDrag ? C_GOLD : 0x7A4A5E, 255);
	}
}

static void draw_item(const Item *it, int lifted, int label) {
	float p = it->pop, sc = p < 1 ? 1 + sinf(p * 3.1416f) * .25f - (1 - p) * .4f : 1;
	float x = it->x, y = it->y;
	if (it->shake > 0) x += sinf(it->shake * 60) * 4 * it->shake / .35f;
	if (lifted) { y -= 3; sc *= 1.1f; }
	const Sprite *s = icon_get(it->el, 0);
	if (sc != 1) blit_scaled(s, x, y, s->w * sc, 255);
	else blit(s, (int)(x - s->w / 2), (int)(y - s->h / 2), 255);
	if (!label) return;
	char nm[64];
	lower(nm, ELEMS[it->el].name, sizeof nm);
	text(fName, x, y + 28, nm, C_GOLD, 255, AL_CENTER);
}

static void draw_cursor(float x, float y, int grab) {
	static const float arrow[] = {0, 0, 0, 22, 6, 17, 10, 26, 14, 24, 10, 15, 17, 15};
	float p[14], o[14];
	float k = grab ? .85f : 1;
	for (int i = 0; i < 7; i++) {
		p[i * 2] = x + arrow[i * 2] * k; p[i * 2 + 1] = y + arrow[i * 2 + 1] * k;
	}
	for (int i = 0; i < 7; i++) { o[i * 2] = p[i * 2] + 1.5f; o[i * 2 + 1] = p[i * 2 + 1] + 2.5f; }
	poly_fill(o, 7, 0x000000, 90);
	for (int dx = -1; dx <= 1; dx++) for (int dy = -1; dy <= 1; dy++) {
		if (!dx && !dy) continue;
		float q[14];
		for (int i = 0; i < 7; i++) { q[i * 2] = p[i * 2] + dx * 1.4f; q[i * 2 + 1] = p[i * 2 + 1] + dy * 1.4f; }
		poly_fill(q, 7, 0x1B001C, 255);
	}
	poly_fill(p, 7, grab ? C_GOLDHI : 0xFFFFFF, 255);
}

/* full-screen reveal: slowly turning rays around a glowing centre, name above, recipe below */
static void draw_popup(void) {
	const Pop *pp = &popq[0];
	float a = clampf(popT / .25f, 0, 1), out = popT > 2.9f ? clampf((3.2f - popT) / .3f, 0, 1) : 1;
	if (popT > 90) out = 0;
	float k = a * out;
	float cx = SW / 2.f, cy = 236;
	fill_rect(0, 0, SW, SH, 0x2A002A, (int)(255 * k));
	for (int i = 0; i < 18; i++) {
		float a0 = t * .12f + i * 3.1416f / 9, a1 = a0 + 3.1416f / 18;
		float tri[6] = {cx, cy, cx + cosf(a0) * 700, cy + sinf(a0) * 700, cx + cosf(a1) * 700, cy + sinf(a1) * 700};
		poly_fill(tri, 3, 0x1C001C, (int)(150 * k));
	}
	for (int r = 240; r > 20; r -= 10) disc(cx, cy, r, 0x5A1E50, (int)(11 * k));
	if (k < .99f) return;
	float s = popT < .35f ? .7f + .3f * sinf(fminf(popT / .35f, 1) * 1.5708f) + sinf(popT / .35f * 3.1416f) * .08f : 1;
	char nm[64];
	lower(nm, ELEMS[pp->el].name, sizeof nm);
	text(fHuge, cx, cy - 108, nm, C_GOLD, 255, AL_CENTER);
	const Sprite *ic = icon_get(pp->el, 1);
	if (s != 1) blit_scaled(ic, cx, cy, ic->w * s, 255);
	else blit(ic, (int)(cx - ic->w / 2), (int)(cy - ic->h / 2), 255);
	float by = cy + 78, bh = ELEMS[pp->el].final ? 52 : 36;
	fill_rect((int)(cx - 180), (int)by, 360, (int)bh, 0x1C001C, 150);
	char b[96], na[32], nb[32];
	if (pp->a < 0) snprintf(b, sizeof b, "unlocked by your progress!");
	else {
		lower(na, ELEMS[pp->a].name, sizeof na);
		lower(nb, ELEMS[pp->b].name, sizeof nb);
		snprintf(b, sizeof b, "%s + %s", na, nb);
	}
	text(fName, cx, by + 9, b, C_GOLD, 255, AL_CENTER);
	if (ELEMS[pp->el].final) text(fTiny, cx, by + 31, "final element - it can't be combined further", C_DIM, 255, AL_CENTER);
}

static void draw_menu(void) {
	fill_rect(0, 0, SW, SH, 0x1C001C, 170);
	int h = 50 + M_COUNT * MIH + 16;
	rrect(MX + 2, MY + 5, MWW, h, 6, 0x000000, 70);
	rrect(MX, MY, MWW, h, 6, C_POP, 255);
	rrect(MX, MY, MWW, 40, 6, C_POPHEAD, 255);
	fill_rect(MX, MY + 30, MWW, 10, C_POPHEAD, 255);
	text(fUI, MX + MWW / 2.f, MY + 10, "settings", C_INK, 255, AL_CENTER);
	for (int i = 0; i < M_COUNT; i++) {
		float y = MY + 50 + i * MIH;
		int danger = i == M_RESET && confirmReset;
		if (i == menuSel) rrect(MX + 16, y + 2, MWW - 32, MIH - 4, 6, danger ? 0xD9573E : 0xF2AE4A, 255);
		text(fName, MX + MWW / 2.f, y + 10, menu_label(i), danger ? 0xFFF4E0 : C_INK, 255, AL_CENTER);
	}
}

void game_draw(void) {
	memcpy(fb, bg, sizeof fb);
	draw_library();
	draw_tools();

	gfx_clip(0, 0, PANX, SH);
	if (target >= 0) {
		const Item *tg = &items[target];
		disc(tg->x, tg->y, 34 + sinf(t * 12) * 2, C_GOLD, 45);
		ring(tg->x, tg->y, 34 + sinf(t * 12) * 2, 2, C_GOLD, 200);
	}
	for (int i = 0; i < nitems; i++) if (i != drag) draw_item(&items[i], 0, i == hoverItem && drag < 0);
	gfx_noclip();
	if (drag >= 0) {
		int over = items[drag].x >= PANX - 8;
		draw_item(&items[drag], 1, !over);
		if (over) text(fSmall, items[drag].x, items[drag].y - 46, "release to remove", 0xFF8A70, 255, AL_CENTER);
	}
	for (int k = 0; k < MAXPART; k++) {
		const Part *p = &parts[k];
		if (p->life > 0) disc(p->x, p->y, p->r * (p->life / p->max), p->c, (int)(255 * fminf(1, p->life / p->max * 2)));
	}

	float mid = (TOOLW + PANX) / 2.f;
	if (toastT > 0) {
		int a = (int)(255 * fminf(1, toastT * 3));
		float w = text_w(fName, toast);
		rrect(mid - w / 2 - 14, SH - 132, w + 28, 28, 6, 0x1C001C, a * 220 / 255);
		text(fName, mid, SH - 127, toast, C_GOLD, a, AL_CENTER);
	} else
		text(fTiny, mid, 18, "A drag    A A copy    B remove    + settings", mix(C_WORK, C_GOLD, .45f), 255, AL_CENTER);

	if (npop) draw_popup();
	if (menuOpen) draw_menu();
	if (cursorOK) draw_cursor(cursorX, cursorY, drag >= 0 || sbDrag);
}
