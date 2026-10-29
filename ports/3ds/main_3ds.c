/* Little Alchemy 2 - Nintendo 3DS platform layer (libctru). The shared game draws into a 640x480
 * frame buffer; it is halved to 320x240 and shown on the touch screen (touch = pointer, held = A) and
 * centred on the top screen. The circle pad moves the cursor too. Icon pack, audio pack and the
 * save live in sdmc:/3ds/LittleAlchemy2/. */
#include <3ds.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "gfx.h"
#include "game.h"
#include "audio.h"
#include "audio_mixer.h"
#include "font_ttf.h"
#include "logo_rgba.h"

#define BASE "sdmc:/3ds/LittleAlchemy2"
static const char *savePath = BASE "/LittleAlchemy2.sav", *iconPath = BASE "/icons.bin", *audioPath = BASE "/audio";

const unsigned char *plat_logo(void) { return logo_rgba; }
int plat_load(char *buf, int max) {
	FILE *f = fopen(savePath, "rb");
	if (!f) return -1;
	int n = (int)fread(buf, 1, max, f);
	fclose(f);
	return n;
}
void plat_save(const char *buf, int len) {
	mkdir(BASE, 0777);
	FILE *f = fopen(savePath, "wb");
	if (!f) return;
	fwrite(buf, 1, len, f);
	fclose(f);
}
const char *plat_icon_path(void) {
	struct stat st;
	return stat(iconPath, &st) ? NULL : iconPath;
}

/* ---------------- sound: ndsp, filled by a thread ---------------- */
#define ABUF 2048
static ndspWaveBuf wbuf[3];
static int16_t *abuf;
static volatile int audioRun, packSfx, sfxOn = 1;
static Thread audioThread;

static void audio_loop(void *arg) {
	(void)arg;
	while (audioRun) {
		for (int i = 0; i < 3; i++) {
			if (wbuf[i].status != NDSP_WBUF_DONE) continue;
			audio_render(abuf + i * ABUF * 2, ABUF);
			DSP_FlushDataCache(abuf + i * ABUF * 2, ABUF * 4);
			wbuf[i].nsamples = ABUF;
			ndspChnWaveBufAdd(0, &wbuf[i]);
		}
		svcSleepThread(5 * 1000 * 1000LL);
	}
}
static void sound_init(void) {
	packSfx = audio_init(audioPath);
	if (!packSfx) return;
	if (ndspInit() < 0) return;
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	ndspChnReset(0);
	ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
	ndspChnSetRate(0, AUDIO_RATE);
	ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);
	abuf = linearAlloc(3 * ABUF * 4);
	memset(abuf, 0, 3 * ABUF * 4);
	for (int i = 0; i < 3; i++) {
		wbuf[i].data_vaddr = abuf + i * ABUF * 2;
		wbuf[i].status = NDSP_WBUF_DONE;
	}
	audioRun = 1;
	s32 prio = 0;
	svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
	audioThread = threadCreate(audio_loop, NULL, 32 * 1024, prio - 1, -2, false);
}
void plat_audio(int music, int effects) { sfxOn = effects; audio_music(music); }
void plat_sfx(int id) {
	if (id == SFX_DISCOVER) audio_note_discovery();
	if (!sfxOn) return;
	audio_sfx(id);
}
void plat_rumble(int ms) { (void)ms; }

/* ---------------- video ---------------- */
/* the 3DS frame buffers are column-major BGR8 starting at the bottom-left corner */
static void put_half(u8 *dst, int dstH, int ox) {
	for (int y = 0; y < SH / 2; y++) {
		const uint32_t *a = fb[y * 2], *b = fb[y * 2 + 1];
		for (int x = 0; x < SW / 2; x++) {
			uint32_t p = a[x * 2], q = a[x * 2 + 1], r = b[x * 2], s = b[x * 2 + 1];
			int R = (CR(p) + CR(q) + CR(r) + CR(s)) >> 2, G = (CG(p) + CG(q) + CG(r) + CG(s)) >> 2, B = (CB(p) + CB(q) + CB(r) + CB(s)) >> 2;
			u8 *o = dst + ((ox + x) * dstH + (dstH - 1 - y)) * 3;
			o[0] = B; o[1] = G; o[2] = R;
		}
	}
}

int main(int argc, char **argv) {
	(void)argc; (void)argv;
	gfxInitDefault();
	gfxSetScreenFormat(GFX_TOP, GSP_BGR8_OES);
	gfxSetScreenFormat(GFX_BOTTOM, GSP_BGR8_OES);
	gfxSetDoubleBuffering(GFX_TOP, false);
	gfxSetDoubleBuffering(GFX_BOTTOM, false);
	osSetSpeedupEnable(true);   /* New 3DS: 804 MHz */
	mkdir("sdmc:/3ds", 0777);
	mkdir(BASE, 0777);
	sound_init();
	gfx_init(font_ttf);
	game_init();

	{	/* the top screen's margins stay black */
		u16 w, h;
		memset(gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &w, &h), 0, (size_t)w * h * 3);
	}
	Input in;
	memset(&in, 0, sizeof in);
	float curX = SW / 2.f, curY = SH / 2.f;
	int haveCursor = 0;
	uint32_t prevHeld = 0;
	u64 last = osGetTime();
	while (aptMainLoop() && !game_quit()) {
		hidScanInput();
		u32 k = hidKeysHeld();
		uint32_t held = 0;
		if (k & KEY_A) held |= BTN_A;
		if (k & KEY_B) held |= BTN_B;
		if (k & KEY_DUP) held |= BTN_UP;
		if (k & KEY_DDOWN) held |= BTN_DOWN;
		if (k & KEY_DLEFT) held |= BTN_LEFT;
		if (k & KEY_DRIGHT) held |= BTN_RIGHT;
		if (k & KEY_START) held |= BTN_PLUS;
		if (k & KEY_R) held |= BTN_PLUS;
		if (k & (KEY_SELECT | KEY_L)) held |= BTN_MINUS;
		if (k & KEY_Y) held |= BTN_1;
		if (k & KEY_X) held |= BTN_2;

		u64 now = osGetTime();
		float dt = (now - last) / 1000.f;
		last = now;
		if (dt > .05f) dt = .05f;
		if (dt < .001f) dt = .001f;

		circlePosition cp;
		hidCircleRead(&cp);
		float sx = cp.dx / 154.f, sy = -cp.dy / 154.f;
		if (fabsf(sx) > .15f || fabsf(sy) > .15f) {
			haveCursor = 1;
			curX = clampf(curX + sx * fabsf(sx) * 520 * dt, 0, SW - 1);
			curY = clampf(curY + sy * fabsf(sy) * 520 * dt, 0, SH - 1);
		}
		if (k & KEY_TOUCH) {
			touchPosition t;
			hidTouchRead(&t);
			curX = clampf(t.px * 2.f, 0, SW - 1);
			curY = clampf(t.py * 2.f, 0, SH - 1);
			haveCursor = 1;
			held |= BTN_A;
		}
		in.held = held;
		in.down = held & ~prevHeld;
		in.up = prevHeld & ~held;
		in.wheel = 0;
		prevHeld = held;
		in.x = curX; in.y = curY;
		in.valid = haveCursor;

		game_update(&in, dt);
		game_draw();

		u16 w, h;
		put_half(gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &w, &h), h, 0);
		put_half(gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &w, &h), h, 40);
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}
	audioRun = 0;
	if (audioThread) { threadJoin(audioThread, U64_MAX); threadFree(audioThread); }
	audio_shutdown();
	if (abuf) { ndspExit(); linearFree(abuf); }
	gfxExit();
	return 0;
}
