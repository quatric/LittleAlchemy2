/* Little Alchemy 2 - SDL2 platform layer for the Switch, the Wii U and desktop test builds: video,
 * input, sound and SD card saves. The game itself (source/game.c, gfx.c, icons.c) is shared with
 * the Wii version and draws into a 640x480 frame buffer, which is scaled onto the screen here.
 *
 * Input: gamepads (Joy-Con, Pro Controller; Wii U GamePad, Pro Controller, Classic Controller) with
 * the left or right stick as the cursor, the touch screen (Switch, Wii U GamePad) or a mouse as the
 * pointer, and on desktop builds the keyboard. */
#include <SDL.h>
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

#ifdef __WIIU__
#include <whb/sdcard.h>
#endif

/* where the icon pack, audio pack and save live: the app's own folder on the SD card */
static char baseDir[256];
static char savePath[300], iconPath[300], audioPath[300];

static void init_paths(int argc, char **argv) {
	const char *env = getenv("LA2_DIR");
#if defined(__SWITCH__)
	/* hbmenu passes sdmc:/switch/<folder>/<name>.nro as argv[0] */
	snprintf(baseDir, sizeof baseDir, "sdmc:/switch/LittleAlchemy2");
	if (argc > 0 && !strncmp(argv[0], "sdmc:/", 6)) {
		snprintf(baseDir, sizeof baseDir, "%s", argv[0]);
		char *slash = strrchr(baseDir, '/');
		if (slash) *slash = 0;
	}
#elif defined(__WIIU__)
	(void)argc; (void)argv;
	snprintf(baseDir, sizeof baseDir, "fs:/vol/external01/wiiu/apps/LittleAlchemy2");
#else
	(void)argc; (void)argv;
	snprintf(baseDir, sizeof baseDir, "%s", env ? env : "apps/LittleAlchemy2");
#endif
	if (env && *env) snprintf(baseDir, sizeof baseDir, "%s", env);
	snprintf(savePath, sizeof savePath, "%s/LittleAlchemy2.sav", baseDir);
	snprintf(iconPath, sizeof iconPath, "%s/icons.bin", baseDir);
	snprintf(audioPath, sizeof audioPath, "%s/audio", baseDir);
}

/* ---------------- game <-> platform ---------------- */
const unsigned char *plat_logo(void) { return logo_rgba; }

int plat_load(char *buf, int max) {
	FILE *f = fopen(savePath, "rb");
	if (!f) return -1;
	int n = (int)fread(buf, 1, max, f);
	fclose(f);
	return n;
}
void plat_save(const char *buf, int len) {
	FILE *f = fopen(savePath, "wb");
	if (!f) return;
	fwrite(buf, 1, len, f);
	fclose(f);
}
const char *plat_icon_path(void) {
	struct stat st;
	return stat(iconPath, &st) ? NULL : iconPath;
}

/* ---------------- sound ---------------- */
static SDL_AudioDeviceID audioDev;
static int sfxOn = 1, packSfx;

static void audio_cb(void *ud, Uint8 *stream, int len) {
	(void)ud;
	audio_render((int16_t *)stream, len / 4);
}
static void sound_init(void) {
	SDL_AudioSpec want, have;
	SDL_zero(want);
	want.freq = AUDIO_RATE;
	want.format = AUDIO_S16SYS;
	want.channels = 2;
	want.samples = 1024;
	want.callback = audio_cb;
	packSfx = audio_init(audioPath);
	if (!packSfx) return;
	audioDev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
	if (audioDev) SDL_PauseAudioDevice(audioDev, 0);
}
void plat_audio(int music, int effects) {
	sfxOn = effects;
	audio_music(music);
}
void plat_sfx(int id) {
	if (id == SFX_DISCOVER) audio_note_discovery();
	if (!sfxOn) return;
	audio_sfx(id);
}

static SDL_GameController *pads[8];
void plat_rumble(int ms) {
#if SDL_VERSION_ATLEAST(2, 0, 9)
	for (int i = 0; i < 8; i++) if (pads[i]) SDL_GameControllerRumble(pads[i], 0x8000, 0x8000, ms);
#else
	(void)ms;
#endif
}

/* ---------------- input ---------------- */
static float stickX, stickY;
static void stick(float x, float y) {
	if (fabsf(x) < .18f) x = 0;
	if (fabsf(y) < .18f) y = 0;
	if (x * x + y * y > stickX * stickX + stickY * stickY) { stickX = clampf(x, -1, 1); stickY = clampf(y, -1, 1); }
}

static void pad_added(int idx) {
	for (int i = 0; i < 8; i++) {
		if (pads[i]) continue;
		pads[i] = SDL_GameControllerOpen(idx);
		return;
	}
}
static void pad_removed(SDL_JoystickID id) {
	for (int i = 0; i < 8; i++)
		if (pads[i] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pads[i])) == id) {
			SDL_GameControllerClose(pads[i]);
			pads[i] = NULL;
		}
}

static uint32_t pad_buttons(SDL_GameController *c) {
	uint32_t o = 0;
#define B(sdl, bit) if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_##sdl)) o |= bit
	B(A, BTN_A); B(B, BTN_B);
	B(DPAD_UP, BTN_UP); B(DPAD_DOWN, BTN_DOWN); B(DPAD_LEFT, BTN_LEFT); B(DPAD_RIGHT, BTN_RIGHT);
	B(START, BTN_PLUS); B(RIGHTSHOULDER, BTN_PLUS);
	B(BACK, BTN_MINUS); B(LEFTSHOULDER, BTN_MINUS);
	B(Y, BTN_1);    /* hint */
	B(X, BTN_2);    /* hide finished elements */
	B(GUIDE, BTN_HOME);
#undef B
	if (SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000) o |= BTN_MINUS;
	if (SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000) o |= BTN_PLUS;
	stick(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX) / 32767.f, SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY) / 32767.f);
	stick(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTX) / 32767.f, SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTY) / 32767.f);
	return o;
}

static uint32_t key_buttons(void) {
	uint32_t o = 0;
	const Uint8 *k = SDL_GetKeyboardState(NULL);
	if (k[SDL_SCANCODE_UP]) o |= BTN_UP;
	if (k[SDL_SCANCODE_DOWN]) o |= BTN_DOWN;
	if (k[SDL_SCANCODE_LEFT]) o |= BTN_LEFT;
	if (k[SDL_SCANCODE_RIGHT]) o |= BTN_RIGHT;
	if (k[SDL_SCANCODE_RETURN] || k[SDL_SCANCODE_KP_ENTER] || k[SDL_SCANCODE_SPACE]) o |= BTN_A;
	if (k[SDL_SCANCODE_BACKSPACE] || k[SDL_SCANCODE_DELETE]) o |= BTN_B;
	if (k[SDL_SCANCODE_X] || k[SDL_SCANCODE_H]) o |= BTN_1;
	if (k[SDL_SCANCODE_Y]) o |= BTN_2;
	if (k[SDL_SCANCODE_EQUALS] || k[SDL_SCANCODE_KP_PLUS] || k[SDL_SCANCODE_M]) o |= BTN_PLUS;
	if (k[SDL_SCANCODE_MINUS] || k[SDL_SCANCODE_KP_MINUS] || k[SDL_SCANCODE_C]) o |= BTN_MINUS;
	if (k[SDL_SCANCODE_ESCAPE]) o |= BTN_HOME;
	return o;
}

int main(int argc, char **argv) {
	init_paths(argc, argv);
#ifdef __WIIU__
	WHBMountSdCard();
#endif
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) < 0) {
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return 1;
	}
	SDL_Window *win = SDL_CreateWindow("Little Alchemy 2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	SDL_Renderer *ren = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC) : NULL;
	if (win && !ren) ren = SDL_CreateRenderer(win, -1, 0);
	if (!ren) { fprintf(stderr, "video: %s\n", SDL_GetError()); return 1; }
	SDL_RenderSetLogicalSize(ren, SW, SH);
	SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, SW, SH);
	if (!tex) { fprintf(stderr, "texture: %s\n", SDL_GetError()); return 1; }
	SDL_ShowCursor(SDL_DISABLE);

	for (int i = 0; i < SDL_NumJoysticks(); i++) if (SDL_IsGameController(i)) pad_added(i);
	sound_init();
	gfx_init(font_ttf);
	game_init();

	Input in;
	memset(&in, 0, sizeof in);
	float curX = SW / 2.f, curY = SH / 2.f, wheel = 0;
	int haveCursor = 0, mouseHeld = 0;
	uint32_t prevHeld = 0;
	Uint32 last = SDL_GetTicks();
	int running = 1, frames = 0;
	const char *shot = getenv("LA2_SHOT");   /* test hook: save frame 90 as a PPM and quit */
	while (running && !game_quit()) {
		SDL_Event e;
		float mdx = 0;
		while (SDL_PollEvent(&e)) {
			switch (e.type) {
			case SDL_QUIT: running = 0; break;
			case SDL_CONTROLLERDEVICEADDED: pad_added(e.cdevice.which); break;
			case SDL_CONTROLLERDEVICEREMOVED: pad_removed(e.cdevice.which); break;
			case SDL_MOUSEMOTION:
				curX = clampf((float)e.motion.x, 0, SW - 1);
				curY = clampf((float)e.motion.y, 0, SH - 1);
				haveCursor = 1;
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP: {
				int bit = e.button.button == SDL_BUTTON_LEFT ? BTN_A : e.button.button == SDL_BUTTON_RIGHT ? BTN_B : BTN_PLUS;
				if (e.type == SDL_MOUSEBUTTONDOWN) mouseHeld |= bit; else mouseHeld &= ~bit;
				curX = clampf((float)e.button.x, 0, SW - 1);
				curY = clampf((float)e.button.y, 0, SH - 1);
				haveCursor = 1;
				break;
			}
			case SDL_MOUSEWHEEL: wheel += e.wheel.y > 0 ? -1 : e.wheel.y < 0 ? 1 : 0; break;
			}
		}
		(void)mdx;
		Uint32 now = SDL_GetTicks();
		float dt = (now - last) / 1000.f;
		last = now;
		if (dt > .05f) dt = .05f;
		if (dt < .001f) dt = .001f;

		uint32_t held = key_buttons() | (uint32_t)mouseHeld;
		stickX = stickY = 0;
		for (int i = 0; i < 8; i++) if (pads[i]) held |= pad_buttons(pads[i]);
		if (stickX || stickY) {
			haveCursor = 1;
			curX = clampf(curX + stickX * fabsf(stickX) * 520 * dt, 0, SW - 1);
			curY = clampf(curY + stickY * fabsf(stickY) * 520 * dt, 0, SH - 1);
		}
		int anyPad = 0;
		for (int i = 0; i < 8; i++) anyPad |= pads[i] != NULL;
		in.held = held;
		in.down = held & ~prevHeld;
		in.up = prevHeld & ~held;
		in.wheel = wheel;
		wheel = 0;
		prevHeld = held;
		in.x = curX; in.y = curY;
		in.valid = haveCursor || anyPad;

		game_update(&in, dt);
		game_draw();
		if (shot && ++frames == 90) {
			FILE *f = fopen(shot, "wb");
			if (f) {
				fprintf(f, "P6\n%d %d\n255\n", SW, SH);
				for (int y = 0; y < SH; y++) for (int x = 0; x < SW; x++) { uint32_t c = fb[y][x]; fputc(CR(c), f); fputc(CG(c), f); fputc(CB(c), f); }
				fclose(f);
			}
			running = 0;
		}
		SDL_UpdateTexture(tex, NULL, fb, SW * 4);
		SDL_RenderClear(ren);
		SDL_RenderCopy(ren, tex, NULL, NULL);
		SDL_RenderPresent(ren);
	}
	audio_shutdown();
	if (audioDev) SDL_CloseAudioDevice(audioDev);
	for (int i = 0; i < 8; i++) if (pads[i]) SDL_GameControllerClose(pads[i]);
	SDL_DestroyTexture(tex);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();
#ifdef __WIIU__
	WHBUnmountSdCard();
#endif
	return 0;
}
