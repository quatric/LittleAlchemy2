/* Little Alchemy 2 for Wii - platform layer: video, input, sound, SD card saves.
 * Input: Wii Remote (pointer, Nunchuk, Classic Controller), GameCube controller, USB keyboard
 * and USB mouse; with the optional libraries also the Wii U GamePad (Wii U VC), Wii U Pro
 * Controller and wired DualShock 3. */
#include <gccore.h>
#include <unistd.h>
#include <wiiuse/wpad.h>
#include <input_wii.h>
#include <asndlib.h>
#include <fat.h>
#include <ogc/lwp_watchdog.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <sys/stat.h>
#include "gfx.h"
#include "game.h"
#include "audio.h"
#include "font_ttf.h"
#include "logo_rgba.h"

static void *xfb[2];
static int fbi;
static GXRModeObj *rmode;
static u32 yuvrow[SW / 2];
static int sdOK;
static u64 rumbleUntil;

static inline u32 yuvpair(u32 a, u32 b) {
	int r0 = (a >> 16) & 255, g0 = (a >> 8) & 255, b0 = a & 255;
	int r1 = (b >> 16) & 255, g1 = (b >> 8) & 255, b1 = b & 255;
	int y0 = 16 + ((66 * r0 + 129 * g0 + 25 * b0) >> 8);
	int y1 = 16 + ((66 * r1 + 129 * g1 + 25 * b1) >> 8);
	int r = (r0 + r1) >> 1, g = (g0 + g1) >> 1, bl = (b0 + b1) >> 1;
	int cb = 128 + ((-38 * r - 74 * g + 112 * bl) >> 8);
	int cr = 128 + ((112 * r - 94 * g - 18 * bl) >> 8);
	return (y0 << 24) | (cb << 16) | (y1 << 8) | cr;
}

static void present(void) {
	int H2 = rmode->xfbHeight, W2 = rmode->fbWidth / 2;
	u32 *dst = xfb[fbi];
	for (int y = 0, last = -1; y < H2; y++) {
		int sy = y * SH / H2;
		if (sy != last) {
			last = sy;
			const u32 *src = fb[sy];
			for (int x = 0; x < SW / 2; x++) yuvrow[x] = yuvpair(src[x * 2], src[x * 2 + 1]);
		}
		memcpy(dst + y * W2, yuvrow, SW / 2 * 4);
	}
	VIDEO_SetNextFramebuffer(dst);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	fbi ^= 1;
}

const unsigned char *plat_logo(void) { return logo_rgba; }

/* ---------------- saves ---------------- */
/* saves are written to the first two; the last two are where versions named Alchemii saved */
static const char *SAVE_PATHS[] = {"sd:/apps/LittleAlchemy2/LittleAlchemy2.sav", "sd:/LittleAlchemy2.sav",
	"sd:/apps/alchemii/alchemii.sav", "sd:/alchemii.sav"};
int plat_load(char *buf, int max) {
	if (!sdOK) return -1;
	for (int i = 0; i < 4; i++) {
		FILE *f = fopen(SAVE_PATHS[i], "rb");
		if (!f) continue;
		int n = (int)fread(buf, 1, max, f);
		fclose(f);
		return n;
	}
	return -1;
}
void plat_save(const char *buf, int len) {
	if (!sdOK) return;
	for (int i = 0; i < 2; i++) {
		FILE *f = fopen(SAVE_PATHS[i], "wb");
		if (!f) continue;
		fwrite(buf, 1, len, f);
		fclose(f);
		return;
	}
}

const char *plat_icon_path(void) {
	static const char *const paths[] = {"sd:/apps/LittleAlchemy2/icons.bin", "sd:/apps/alchemii/icons.bin"};
	struct stat st;
	for (int i = 0; sdOK && i < 2; i++) if (!stat(paths[i], &st)) return paths[i];
	return NULL;
}

/* ---------------- sound: short synthesized effects ---------------- */
#define RATE 32000
typedef struct { s16 *pcm; int bytes; } Sfx;
static Sfx sfx[SFX_COUNT];
static int sfxOn = 1, packSfx;

static Sfx synth(float dur, float f0, float f1, float vol, int shape, float f2) {
	int n = (int)(dur * RATE);
	int bytes = (n * 2 + 31) & ~31;
	s16 *p = memalign(32, bytes);
	memset(p, 0, bytes);
	float ph = 0, ph2 = 0;
	for (int i = 0; i < n; i++) {
		float t = (float)i / n, f = f0 + (f1 - f0) * t;
		float env = (t < .04f ? t / .04f : 1.f) * powf(1.f - t, shape == 2 ? 1.2f : 2.2f);
		ph += f / RATE; ph2 += f2 / RATE;
		float v = sinf(ph * 6.2832f);
		if (shape == 1) v = v * .7f + sinf(ph * 2 * 6.2832f) * .3f;
		if (f2 > 0) v = v * .6f + sinf(ph2 * 6.2832f) * .4f;
		p[i] = (s16)(v * env * vol * 32767);
	}
	DCFlushRange(p, bytes);
	return (Sfx){p, bytes};
}
static Sfx chime(void) {
	static const float notes[4] = {659.25f, 783.99f, 987.77f, 1318.5f};
	int n = (int)(.9f * RATE), bytes = (n * 2 + 31) & ~31;
	s16 *p = memalign(32, bytes);
	memset(p, 0, bytes);
	for (int k = 0; k < 4; k++) {
		int s0 = (int)(k * .09f * RATE);
		for (int i = s0; i < n; i++) {
			float t = (float)(i - s0) / RATE, env = expf(-t * 5.f) * fminf(1, t * 200);
			float v = sinf(t * notes[k] * 6.2832f) * .7f + sinf(t * notes[k] * 2 * 6.2832f) * .15f;
			int s = p[i] + (int)(v * env * .22f * 32767);
			p[i] = (s16)clampi(s, -32767, 32767);
		}
	}
	DCFlushRange(p, bytes);
	return (Sfx){p, bytes};
}
static void sound_init(void) {
	ASND_Init();
	ASND_Pause(0);
	sfx[SFX_PICK] = synth(.07f, 520, 760, .35f, 0, 0);
	sfx[SFX_DROP] = synth(.08f, 420, 300, .30f, 0, 0);
	sfx[SFX_COMBINE] = synth(.22f, 400, 900, .40f, 1, 0);
	sfx[SFX_DISCOVER] = chime();
	sfx[SFX_FAIL] = synth(.18f, 180, 140, .35f, 2, 186);
	sfx[SFX_DELETE] = synth(.12f, 600, 200, .30f, 0, 0);
	sfx[SFX_CLICK] = synth(.04f, 900, 900, .25f, 0, 0);
	sfx[SFX_CLONE] = sfx[SFX_PICK];
	/* the Little Alchemy 2 music and effects, when the audio pack is on the card */
	if (sdOK) packSfx = audio_init("sd:/apps/LittleAlchemy2/audio");
}
void plat_audio(int music, int effects) {
	sfxOn = effects;
	audio_music(music);
}
void plat_sfx(int id) {
	if (id == SFX_DISCOVER) audio_note_discovery();
	if (!sfxOn || (packSfx && audio_sfx(id))) return;
	if (id < 0 || id >= SFX_COUNT || !sfx[id].pcm) return;
	int v = ASND_GetFirstUnusedVoice();
	if (v < 0) return;
	ASND_SetVoice(v, VOICE_MONO_16BIT, RATE, 0, sfx[id].pcm, sfx[id].bytes, 255, 255, NULL);
}
void plat_rumble(int ms) {
	WPAD_Rumble(0, 1);
	rumbleUntil = gettime() + millisecs_to_ticks(ms);
}

/* ---------------- input ---------------- */
/* Devices come from libinput-wii; this maps them onto the game's buttons and cursor. */
static u32 map_buttons(u32 b) {
	u32 o = 0;
	if (b & WPAD_BUTTON_A) o |= BTN_A;
	if (b & WPAD_BUTTON_B) o |= BTN_B;
	if (b & WPAD_BUTTON_UP) o |= BTN_UP;
	if (b & WPAD_BUTTON_DOWN) o |= BTN_DOWN;
	if (b & WPAD_BUTTON_LEFT) o |= BTN_LEFT;
	if (b & WPAD_BUTTON_RIGHT) o |= BTN_RIGHT;
	if (b & WPAD_BUTTON_PLUS) o |= BTN_PLUS;
	if (b & WPAD_BUTTON_MINUS) o |= BTN_MINUS;
	if (b & WPAD_BUTTON_1) o |= BTN_1;
	if (b & WPAD_BUTTON_2) o |= BTN_2;
	if (b & WPAD_BUTTON_HOME) o |= BTN_HOME;
	if (b & INPUT_BUTTON_Z) o |= BTN_A;
	if (b & INPUT_BUTTON_C) o |= BTN_B;
	return o;
}

/* analog stick input, combined: the strongest stick wins */
static float stickX, stickY;
static void stick(float x, float y) {
	if (fabsf(x) < .18f) x = 0;
	if (fabsf(y) < .18f) y = 0;
	if (x * x + y * y > stickX * stickX + stickY * stickY) { stickX = clampf(x, -1, 1); stickY = clampf(y, -1, 1); }
}
/* The library's stick before its length is clamped, rescaled to the divisors
 * this game always used (GameCube 80 / C-stick 70, DualShock 3 127); stick()
 * clamps each axis as before, and screen y grows downward. */
static void lib_stick(int which) {
	float x, y, k = 1;
	const char *src;
	if (INPUT_StickRaw(which, &x, &y, &src) <= 0) return;
	if (!strcmp(src, "GameCube")) k = which == INPUT_STICK_LEFT ? 70.f / 80.f : 60.f / 70.f;
	else if (!strcmp(src, "DualShock 3")) k = 110.f / 127.f;
	stick(x * k, -y * k);
}

static void input_init(void) {
	INPUT_Init();
	/* GameCube: Start / R = menu, X / L = clear, Z = 1, Y = 2 */
	INPUT_GameCubeMapClear();
	INPUT_GameCubeMapSet(PAD_BUTTON_A, WPAD_BUTTON_A);
	INPUT_GameCubeMapSet(PAD_BUTTON_B, WPAD_BUTTON_B);
	INPUT_GameCubeMapSet(PAD_BUTTON_UP, WPAD_BUTTON_UP);
	INPUT_GameCubeMapSet(PAD_BUTTON_DOWN, WPAD_BUTTON_DOWN);
	INPUT_GameCubeMapSet(PAD_BUTTON_LEFT, WPAD_BUTTON_LEFT);
	INPUT_GameCubeMapSet(PAD_BUTTON_RIGHT, WPAD_BUTTON_RIGHT);
	INPUT_GameCubeMapSet(PAD_BUTTON_START | PAD_TRIGGER_R, WPAD_BUTTON_PLUS);
	INPUT_GameCubeMapSet(PAD_BUTTON_X | PAD_TRIGGER_L, WPAD_BUTTON_MINUS);
	INPUT_GameCubeMapSet(PAD_TRIGGER_Z, WPAD_BUTTON_1);
	INPUT_GameCubeMapSet(PAD_BUTTON_Y, WPAD_BUTTON_2);
	/* USB keyboard: arrows, Enter = A, Backspace = B, X / Y = 1 / 2, + / - = menu / clear, Esc = exit */
	INPUT_KeyboardMapClear();
	INPUT_KeyboardMapSet(0x52, WPAD_BUTTON_UP);
	INPUT_KeyboardMapSet(0x51, WPAD_BUTTON_DOWN);
	INPUT_KeyboardMapSet(0x50, WPAD_BUTTON_LEFT);
	INPUT_KeyboardMapSet(0x4F, WPAD_BUTTON_RIGHT);
	INPUT_KeyboardMapSet(0x28, WPAD_BUTTON_A);     /* Enter */
	INPUT_KeyboardMapSet(0x58, WPAD_BUTTON_A);     /* keypad Enter */
	INPUT_KeyboardMapSet(0x2C, WPAD_BUTTON_A);     /* Space */
	INPUT_KeyboardMapSet(0x2A, WPAD_BUTTON_B);     /* Backspace */
	INPUT_KeyboardMapSet(0x4C, WPAD_BUTTON_B);     /* Delete */
	INPUT_KeyboardMapSet(0x1B, WPAD_BUTTON_1);     /* X */
	INPUT_KeyboardMapSet(0x0B, WPAD_BUTTON_1);     /* H (hint) */
	INPUT_KeyboardMapSet(0x1C, WPAD_BUTTON_2);     /* Y */
	INPUT_KeyboardMapSet(0x2E, WPAD_BUTTON_PLUS);  /* =/+ */
	INPUT_KeyboardMapSet(0x57, WPAD_BUTTON_PLUS);  /* keypad + */
	INPUT_KeyboardMapSet(0x10, WPAD_BUTTON_PLUS);  /* M (menu) */
	INPUT_KeyboardMapSet(0x2D, WPAD_BUTTON_MINUS); /* - */
	INPUT_KeyboardMapSet(0x56, WPAD_BUTTON_MINUS); /* keypad - */
	INPUT_KeyboardMapSet(0x06, WPAD_BUTTON_MINUS); /* C (clear) */
	INPUT_KeyboardMapSet(0x29, WPAD_BUTTON_HOME);  /* Esc */
	INPUT_KeyboardMapSet(0x4A, WPAD_BUTTON_HOME);  /* Home */
}

static void input_shutdown(void) {
	INPUT_Shutdown();
}

/* ---------------- main loop ---------------- */
int main(void) {
	VIDEO_Init();
	input_init();
	sdOK = fatInitDefault();
	if (sdOK) mkdir("sd:/apps", 0777), mkdir("sd:/apps/LittleAlchemy2", 0777);
	sound_init();
	rmode = VIDEO_GetPreferredMode(NULL);
	xfb[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	xfb[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	VIDEO_Configure(rmode);
	VIDEO_ClearFrameBuffer(rmode, xfb[0], COLOR_BLACK);
	VIDEO_ClearFrameBuffer(rmode, xfb[1], COLOR_BLACK);
	VIDEO_SetNextFramebuffer(xfb[0]);
	VIDEO_SetBlack(false);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

	INPUT_SetScreenSize(SW, SH);

	gfx_init(font_ttf);
	game_init();

	Input in;
	memset(&in, 0, sizeof in);
	/* the pointer follows whichever device moved last: a Wii Remote's IR, or the
	 * virtual cursor driven by sticks and the mouse */
	float curX = SW / 2.f, curY = SH / 2.f, irX = curX, irY = curY;
	int useIR = 0, haveCursor = 0;
	u32 prevHeld = 0;
	u64 last = gettime();
	while (!game_quit()) {
		INPUT_Update();
		u64 now = gettime();
		float dt = (float)ticks_to_microsecs(now - last) / 1e6f;
		last = now;
		if (dt > .05f) dt = .05f;
		if (dt < .001f) dt = .001f;
		if (rumbleUntil && now > rumbleUntil) { WPAD_Rumble(0, 0); rumbleUntil = 0; }

		u32 held = map_buttons(INPUT_ButtonsHeld());
		stickX = stickY = 0;
		lib_stick(INPUT_STICK_LEFT);
		lib_stick(INPUT_STICK_RIGHT);

		/* Wii Remote pointer (from the first one aimed at the screen) */
		float px, py;
		int irValid = INPUT_Pointer(&px, &py);
		if (irValid) {
			float nx = irX + (px - irX) * .6f, ny = irY + (py - irY) * .6f;
			/* take the pointer back only when the remote actually moves */
			if (!useIR && fabsf(px - irX) + fabsf(py - irY) > 6) useIR = 1;
			irX = nx; irY = ny;
		}

		/* USB mouse: moves the pointer; left = A, right = B, middle = menu, wheel scrolls */
		int mx, my, mz;
		INPUT_MouseDelta(&mx, &my, &mz);
		float mdx = mx, mdy = my;
		float wheel = mz > 0 ? -1 : mz < 0 ? 1 : 0;  /* wheel away from you: towards the start of the list */
		u8 mb = INPUT_MouseButtonsHeld();
		if (mb & INPUT_MOUSE_LEFT) held |= BTN_A;
		if (mb & INPUT_MOUSE_RIGHT) held |= BTN_B;
		if (mb & INPUT_MOUSE_MIDDLE) held |= BTN_PLUS;

		/* sticks (squared for fine control) and the mouse drive the virtual cursor */
		if (stickX || stickY || mdx || mdy) {
			if (useIR) { curX = irX; curY = irY; }
			useIR = 0;
			haveCursor = 1;
			curX = clampf(curX + stickX * fabsf(stickX) * 520 * dt + mdx, 0, SW - 1);
			curY = clampf(curY + stickY * fabsf(stickY) * 520 * dt + mdy, 0, SH - 1);
		}

		in.held = held;
		in.down = held & ~prevHeld;
		in.up = prevHeld & ~held;
		in.wheel = wheel;
		prevHeld = held;
		if (useIR && irValid) { in.x = irX; in.y = irY; in.valid = 1; }
		else if (!useIR && haveCursor) { in.x = curX; in.y = curY; in.valid = 1; }
		else { in.x = irX; in.y = irY; in.valid = irValid; }

		game_update(&in, dt);
		game_draw();
		present();
	}
	audio_shutdown();
	WPAD_Rumble(0, 0);
	input_shutdown();
	ASND_End();
	VIDEO_SetBlack(true);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	return 0;
}
