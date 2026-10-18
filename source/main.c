/* Little Alchemy 2 for Wii - platform layer: video, input, sound, SD card saves.
 * Input: Wii Remote (pointer, Nunchuk, Classic Controller), GameCube controller, USB keyboard
 * and USB mouse; with the optional libraries also the Wii U GamePad (Wii U VC), Wii U Pro
 * Controller and wired DualShock 3. */
#include <gccore.h>
#include <unistd.h>
#include <wiiuse/wpad.h>
#include <ogc/usb.h>
#include <ogc/usbmouse.h>
#include <wiikeyboard/usbkeyboard.h>
#ifdef HAVE_WIIDRC
#include <wiidrc/wiidrc.h>
#endif
#ifdef HAVE_WUPC
#include <wupc/wupc.h>
#endif
#ifdef HAVE_SICKSAXIS
#include <sicksaxis.h>
#endif
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
static Sfx sfx[SFX_CLICK + 1];

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
}
void plat_sfx(int id) {
	if (id < 0 || id > SFX_CLICK || !sfx[id].pcm) return;
	int v = ASND_GetFirstUnusedVoice();
	if (v < 0) return;
	ASND_SetVoice(v, VOICE_MONO_16BIT, RATE, 0, sfx[id].pcm, sfx[id].bytes, 255, 255, NULL);
}
void plat_rumble(int ms) {
	WPAD_Rumble(0, 1);
	rumbleUntil = gettime() + millisecs_to_ticks(ms);
}

/* ---------------- input ---------------- */
/* Classic Controller and Wii U Pro Controller buttons (they share the bit layout) */
static u32 map_classic(u32 b) {
	u32 o = 0;
	if (b & WPAD_CLASSIC_BUTTON_A) o |= BTN_A;
	if (b & WPAD_CLASSIC_BUTTON_B) o |= BTN_B;
	if (b & WPAD_CLASSIC_BUTTON_Y) o |= BTN_1;
	if (b & WPAD_CLASSIC_BUTTON_X) o |= BTN_2;
	if (b & (WPAD_CLASSIC_BUTTON_PLUS | WPAD_CLASSIC_BUTTON_FULL_R | WPAD_CLASSIC_BUTTON_ZR)) o |= BTN_PLUS;
	if (b & (WPAD_CLASSIC_BUTTON_MINUS | WPAD_CLASSIC_BUTTON_FULL_L | WPAD_CLASSIC_BUTTON_ZL)) o |= BTN_MINUS;
	if (b & WPAD_CLASSIC_BUTTON_HOME) o |= BTN_HOME;
	if (b & WPAD_CLASSIC_BUTTON_UP) o |= BTN_UP;
	if (b & WPAD_CLASSIC_BUTTON_DOWN) o |= BTN_DOWN;
	if (b & WPAD_CLASSIC_BUTTON_LEFT) o |= BTN_LEFT;
	if (b & WPAD_CLASSIC_BUTTON_RIGHT) o |= BTN_RIGHT;
	return o;
}

/* analog stick input, combined: the strongest stick wins */
static float stickX, stickY;
static void stick(float x, float y) {
	if (fabsf(x) < .18f) x = 0;
	if (fabsf(y) < .18f) y = 0;
	if (x * x + y * y > stickX * stickX + stickY * stickY) { stickX = clampf(x, -1, 1); stickY = clampf(y, -1, 1); }
}
static void stick_js(const joystick_t *js) {
	float a = js->ang * 3.14159265f / 180;
	stick(js->mag * sinf(a), -js->mag * cosf(a));
}
#if defined(HAVE_WIIDRC) || defined(HAVE_WUPC)
/* Wii U GamePad and Pro Controller sticks report roughly -900..900 */
static void stick_wiiu(s16 x, s16 y) { stick(x / 900.f, -y / 900.f); }
#endif

#ifdef HAVE_WIIDRC
static u32 map_drc(u32 b) {
	u32 o = 0;
	if (b & WIIDRC_BUTTON_A) o |= BTN_A;
	if (b & WIIDRC_BUTTON_B) o |= BTN_B;
	if (b & WIIDRC_BUTTON_Y) o |= BTN_1;
	if (b & WIIDRC_BUTTON_X) o |= BTN_2;
	if (b & (WIIDRC_BUTTON_PLUS | WIIDRC_BUTTON_R | WIIDRC_BUTTON_ZR)) o |= BTN_PLUS;
	if (b & (WIIDRC_BUTTON_MINUS | WIIDRC_BUTTON_L | WIIDRC_BUTTON_ZL)) o |= BTN_MINUS;
	if (b & WIIDRC_BUTTON_HOME) o |= BTN_HOME;
	if (b & WIIDRC_BUTTON_UP) o |= BTN_UP;
	if (b & WIIDRC_BUTTON_DOWN) o |= BTN_DOWN;
	if (b & WIIDRC_BUTTON_LEFT) o |= BTN_LEFT;
	if (b & WIIDRC_BUTTON_RIGHT) o |= BTN_RIGHT;
	return o;
}
#endif

#ifdef HAVE_SICKSAXIS
static struct ss_device ds3;
static int ds3OK;
static u64 ds3Retry;
static u32 poll_ds3(void) {
	if (!ds3OK) return 0;
	if (!ss_is_connected(&ds3)) {
		/* look for a newly plugged-in controller every couple of seconds */
		u64 now = gettime();
		if (now >= ds3Retry) {
			ds3Retry = now + secs_to_ticks(2);
			if (ss_open(&ds3) > 0) ss_start_reading(&ds3);
		}
		return 0;
	}
	const struct SS_BUTTONS *b = &ds3.pad.buttons;
	u32 o = 0;
	if (b->cross) o |= BTN_A;
	if (b->circle) o |= BTN_B;
	if (b->square) o |= BTN_1;
	if (b->triangle) o |= BTN_2;
	if (b->start || b->R1 || b->R2) o |= BTN_PLUS;
	if (b->select || b->L1 || b->L2) o |= BTN_MINUS;
	if (b->PS) o |= BTN_HOME;
	if (b->up) o |= BTN_UP;
	if (b->down) o |= BTN_DOWN;
	if (b->left) o |= BTN_LEFT;
	if (b->right) o |= BTN_RIGHT;
	stick((ds3.pad.left_analog.x - 128) / 127.f, (ds3.pad.left_analog.y - 128) / 127.f);
	stick((ds3.pad.right_analog.x - 128) / 127.f, (ds3.pad.right_analog.y - 128) / 127.f);
	return o;
}
#endif

/* USB keyboard: arrows, Enter = A, Backspace = B, X / Y = 1 / 2, + / - = menu / clear, Esc = exit */
static volatile u32 kbdHeld;
static lwp_t kbdThread = LWP_THREAD_NULL;
static volatile int kbdRun;
static void kbd_event(USBKeyboard_event ev) {
	if (ev.type != USBKEYBOARD_PRESSED && ev.type != USBKEYBOARD_RELEASED) return;
	u32 b = 0;
	switch (ev.keyCode) {
	case 0x52: b = BTN_UP; break;
	case 0x51: b = BTN_DOWN; break;
	case 0x50: b = BTN_LEFT; break;
	case 0x4F: b = BTN_RIGHT; break;
	case 0x28: case 0x58: case 0x2C: b = BTN_A; break;   /* Enter, keypad Enter, Space */
	case 0x2A: case 0x4C: b = BTN_B; break;              /* Backspace, Delete */
	case 0x1B: case 0x0B: b = BTN_1; break;              /* X, H (hint) */
	case 0x1C: b = BTN_2; break;                         /* Y */
	case 0x2E: case 0x57: case 0x10: b = BTN_PLUS; break; /* =/+, keypad +, M (menu) */
	case 0x2D: case 0x56: case 0x06: b = BTN_MINUS; break; /* -, keypad -, C (clear) */
	case 0x29: case 0x4A: b = BTN_HOME; break;           /* Esc, Home */
	}
	if (ev.type == USBKEYBOARD_PRESSED) kbdHeld |= b;
	else kbdHeld &= ~b;
}
static void *kbd_thread(void *arg) {
	(void)arg;
	while (kbdRun) {
		if (!USBKeyboard_IsConnected() && USBKeyboard_Open(kbd_event)) {
			/* blink the keyboard's lights to show it was picked up */
			kbdHeld = 0;
			for (int i = 0; i < 3; i++) { USBKeyboard_SetLed(i, 1); usleep(250000); }
			for (int i = 0; i < 3; i++) USBKeyboard_SetLed(i, 0);
		}
		USBKeyboard_Scan();
		usleep(400);
	}
	return NULL;
}

/* USB mouse: moves the pointer; left = A, right = B, middle = menu, wheel scrolls */
static int mouseOK;
static u64 mouseRetry;
static u32 poll_mouse(float *dx, float *dy, float *wheel) {
	static u32 buttons;
	u64 now = gettime();
	if (!mouseOK || !MOUSE_IsConnected()) {
		buttons = 0;
		if (now < mouseRetry) return 0;
		mouseRetry = now + secs_to_ticks(1);
		if (mouseOK) MOUSE_Deinit();
		mouseOK = MOUSE_Init() >= 0;
		return 0;
	}
	mouse_event ev;
	for (int n = 0; n < 64 && MOUSE_GetEvent(&ev) > 0; n++) {
		*dx += ev.rx; *dy += ev.ry;
		if (ev.rz > 0) *wheel -= 1;  /* wheel away from you: towards the start of the list */
		if (ev.rz < 0) *wheel += 1;
		buttons = 0;
		if (ev.button & 1) buttons |= BTN_A;
		if (ev.button & 2) buttons |= BTN_B;
		if (ev.button & 4) buttons |= BTN_PLUS;
	}
	return buttons;
}

static void input_init(void) {
	PAD_Init();
#ifdef HAVE_WUPC
	WUPC_Init();
#endif
	WPAD_Init();
#ifdef HAVE_WIIDRC
	WiiDRC_Init();
#endif
	USB_Initialize();
#ifdef HAVE_SICKSAXIS
	if (ss_init() >= 0 && ss_initialize(&ds3) >= 0) {
		ds3OK = 1;
		if (ss_open(&ds3) > 0) ss_start_reading(&ds3);
	}
#endif
	USBKeyboard_Initialize();
	kbdRun = 1;
	LWP_CreateThread(&kbdThread, kbd_thread, NULL, NULL, 0x4000, 0x7F);
	mouseOK = MOUSE_Init() >= 0;
}

static void input_shutdown(void) {
	kbdRun = 0;
	if (kbdThread != LWP_THREAD_NULL) LWP_JoinThread(kbdThread, NULL);
	USBKeyboard_Close();
	USBKeyboard_Deinitialize();
	if (mouseOK) MOUSE_Deinit();
#ifdef HAVE_SICKSAXIS
	if (ds3OK) { if (ss_is_connected(&ds3)) ss_stop_reading(&ds3); ss_close(&ds3); }
#endif
#ifdef HAVE_WUPC
	WUPC_Shutdown();
#endif
}

/* ---------------- main loop ---------------- */
int main(void) {
	VIDEO_Init();
	input_init();
	sound_init();
	sdOK = fatInitDefault();
	if (sdOK) mkdir("sd:/apps", 0777), mkdir("sd:/apps/LittleAlchemy2", 0777);
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

	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetVRes(WPAD_CHAN_ALL, SW, SH);

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
		WPAD_ScanPads();
		PAD_ScanPads();
		u64 now = gettime();
		float dt = (float)ticks_to_microsecs(now - last) / 1e6f;
		last = now;
		if (dt > .05f) dt = .05f;
		if (dt < .001f) dt = .001f;
		if (rumbleUntil && now > rumbleUntil) { WPAD_Rumble(0, 0); rumbleUntil = 0; }

		u32 held = 0;
		stickX = stickY = 0;

		/* Wii Remotes (pointer from the first one aimed at the screen) and their expansions */
		int irValid = 0;
		for (int ch = 0; ch < 4; ch++) {
			WPADData *wd = WPAD_Data(ch);
			u32 wh = WPAD_ButtonsHeld(ch);
			if (wh & WPAD_BUTTON_A) held |= BTN_A;
			if (wh & WPAD_BUTTON_B) held |= BTN_B;
			if (wh & WPAD_BUTTON_UP) held |= BTN_UP;
			if (wh & WPAD_BUTTON_DOWN) held |= BTN_DOWN;
			if (wh & WPAD_BUTTON_LEFT) held |= BTN_LEFT;
			if (wh & WPAD_BUTTON_RIGHT) held |= BTN_RIGHT;
			if (wh & WPAD_BUTTON_PLUS) held |= BTN_PLUS;
			if (wh & WPAD_BUTTON_MINUS) held |= BTN_MINUS;
			if (wh & WPAD_BUTTON_1) held |= BTN_1;
			if (wh & WPAD_BUTTON_2) held |= BTN_2;
			if (wh & WPAD_BUTTON_HOME) held |= BTN_HOME;
			if (!wd) continue;
			if (wd->exp.type == WPAD_EXP_CLASSIC) { held |= map_classic(wh); stick_js(&wd->exp.classic.ljs); stick_js(&wd->exp.classic.rjs); }
			if (wd->exp.type == WPAD_EXP_NUNCHUK) {
				stick_js(&wd->exp.nunchuk.js);
				if (wh & WPAD_NUNCHUK_BUTTON_Z) held |= BTN_A;
				if (wh & WPAD_NUNCHUK_BUTTON_C) held |= BTN_B;
			}
			if (!irValid && wd->ir.valid) {
				irValid = 1;
				float nx = irX + (wd->ir.x - irX) * .6f, ny = irY + (wd->ir.y - irY) * .6f;
				/* take the pointer back only when the remote actually moves */
				if (!useIR && fabsf(wd->ir.x - irX) + fabsf(wd->ir.y - irY) > 6) useIR = 1;
				irX = nx; irY = ny;
			}
		}

		/* GameCube controllers */
		for (int ch = 0; ch < 4; ch++) {
			u16 ph = PAD_ButtonsHeld(ch);
			if (ph & PAD_BUTTON_A) held |= BTN_A;
			if (ph & PAD_BUTTON_B) held |= BTN_B;
			if (ph & PAD_BUTTON_UP) held |= BTN_UP;
			if (ph & PAD_BUTTON_DOWN) held |= BTN_DOWN;
			if (ph & PAD_BUTTON_LEFT) held |= BTN_LEFT;
			if (ph & PAD_BUTTON_RIGHT) held |= BTN_RIGHT;
			if (ph & (PAD_BUTTON_START | PAD_TRIGGER_R)) held |= BTN_PLUS;
			if (ph & (PAD_BUTTON_X | PAD_TRIGGER_L)) held |= BTN_MINUS;
			if (ph & PAD_TRIGGER_Z) held |= BTN_1;
			if (ph & PAD_BUTTON_Y) held |= BTN_2;
			stick(PAD_StickX(ch) / 80.f, -PAD_StickY(ch) / 80.f);
			stick(PAD_SubStickX(ch) / 70.f, -PAD_SubStickY(ch) / 70.f);
		}

#ifdef HAVE_WUPC
		WUPC_UpdateButtonStats();
		for (int ch = 0; ch < 4; ch++) {
			if (!WUPC_Data(ch)) continue;
			held |= map_classic(WUPC_ButtonsHeld(ch));
			stick_wiiu(WUPC_lStickX(ch), WUPC_lStickY(ch));
			stick_wiiu(WUPC_rStickX(ch), WUPC_rStickY(ch));
		}
#endif
#ifdef HAVE_WIIDRC
		if (WiiDRC_Inited() && WiiDRC_Connected() && WiiDRC_ScanPads()) {
			held |= map_drc(WiiDRC_ButtonsHeld());
			stick_wiiu(WiiDRC_lStickX(), WiiDRC_lStickY());
			stick_wiiu(WiiDRC_rStickX(), WiiDRC_rStickY());
		}
#endif
#ifdef HAVE_SICKSAXIS
		held |= poll_ds3();
#endif
		held |= kbdHeld;

		float mdx = 0, mdy = 0, wheel = 0;
		held |= poll_mouse(&mdx, &mdy, &wheel);

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
	input_shutdown();
	WPAD_Rumble(0, 0);
	ASND_End();
	VIDEO_SetBlack(true);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	return 0;
}
