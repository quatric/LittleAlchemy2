/* Little Alchemy 2 music and sound effects for the 3DS, Wii U and Switch ports: the same Ogg Vorbis
 * pack as the Wii (tools/fetch_audio.py), decoded with stb_vorbis and mixed in software. Effects are
 * decoded once at start-up; music is streamed from the card and the track choice follows the
 * original, as in source/audio.c, but runs as a state machine inside audio_render(). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "audio_mixer.h"
#include "game.h"

#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

/* the original mixes effects at gain 0.6 and music at 0.08; effects get full scale here */
#define MUSIC_GAIN (.08f / .6f)
#define MAX_VOICES 8

static char dir[192];

/* ---------------- effects ---------------- */
typedef struct { int id; const char *name; int volume; int16_t *pcm; int frames; } Effect;
static Effect fx[] = {   /* volumes (percent) as in the original */
	{SFX_CLICK, "GenericClick", 100}, {SFX_PICK, "PickUpItem", 100}, {SFX_DROP, "DropItem", 100},
	{SFX_COMBINE, "AlreadyMixed", 400}, {SFX_CLONE, "CloneItem", 150}, {SFX_FAIL, "FailedToMix", 80},
	{SFX_DELETE, "RemoveItem", 80}, {SFX_DISCOVER, "ChargeUp", 10}, {SFX_POP, "PopUp", 30},
};
#define NFX (int)(sizeof fx / sizeof *fx)

/* decode a whole file to stereo, scaled by the effect's volume (clipped) */
static int load_effect(Effect *e) {
	char path[256];
	snprintf(path, sizeof path, "%s/sfx/%s.ogg", dir, e->name);
	int ch, rate;
	short *raw;
	int n = stb_vorbis_decode_filename(path, &ch, &rate, &raw);
	if (n <= 0 || !raw) return 0;
	if (rate != AUDIO_RATE || ch < 1 || ch > 2) { free(raw); return 0; }
	e->pcm = malloc((size_t)n * 4);
	if (!e->pcm) { free(raw); return 0; }
	float g = e->volume / 100.f;
	for (int i = 0; i < n; i++) {
		for (int c = 0; c < 2; c++) {
			int v = (int)(raw[i * ch + (ch == 2 ? c : 0)] * g);
			e->pcm[i * 2 + c] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
		}
	}
	free(raw);
	e->frames = n;
	return 1;
}

/* ---------------- commands from the game thread ---------------- */
/* single producer (the game), single consumer (audio_render): a ring of ids */
enum { CMD_DISCOVERY = -1 };
static volatile int cmdQ[64];
static volatile unsigned cmdHead, cmdTail;
static void push(int id) {
	unsigned h = cmdHead;
	if (h - cmdTail >= 64) return;
	cmdQ[h & 63] = id;
	__sync_synchronize();
	cmdHead = h + 1;
}
static volatile int musicOn = 1, running, haveMusic;

typedef struct { const Effect *e; int pos; } Voice;
static Voice voices[MAX_VOICES];

int audio_sfx(int id) {
	for (int i = 0; i < NFX; i++) {
		if (fx[i].id != id) continue;
		if (!fx[i].pcm) return 0;
		push(id);
		return 1;
	}
	return 0;
}
void audio_note_discovery(void) { push(CMD_DISCOVERY); }
void audio_music(int on) { musicOn = on; }

/* ---------------- music ---------------- */
enum { TRACK, CONNECTOR };
enum { CALM = 1, BUSY = 2 };
typedef struct {
	const char *name, *file;
	int type, loop, mood, duration, volume, silenceAfter, fadeIn, fadeOut;
	const char *prevents;
} Track;
/* the original's track table; looped tracks play for `duration` seconds */
static const Track TRACKS[] = {
	{"LA_001", "LA_001", TRACK, 0, BUSY, 0, 100, 10, 0, 0, "LA_001_loop"},
	{"LA_002", "LA_002", TRACK, 0, BUSY, 0, 100, 10, 0, 0, "LA_002_loop"},
	{"LA_003", "LA_003", TRACK, 0, BUSY, 0, 100, 15, 0, 0, "LA_003_loop"},
	{"LA_FS_3_4", "LA_FS_3_4", TRACK, 0, BUSY, 0, 100, 10, 0, 0, "LA_FS_3_4_loop"},
	{"LA_004", "LA_004_Loop", TRACK, 1, CALM, 320, 70, 6, 4, 4, ""},
	{"LA_005", "LA_005_Loop", TRACK, 1, CALM, 246, 70, 20, 4, 4, ""},
	{"LA_006", "LA_006_Loop", TRACK, 1, BUSY, 180, 100, 6, 2, 2, ""},
	{"LA_FS_001", "LA_FS_001_Loop", TRACK, 1, CALM, 200, 70, 10, 4, 4, ""},
	{"LA_FS_0032", "LA_FS_0032_Loop", TRACK, 1, CALM, 60, 100, 0, 0, 0, ""},
	{"LA_001_loop", "LA_001_Loop", CONNECTOR, 1, CALM, 50, 50, 6, 6, 6, "LA_001"},
	{"LA_002_loop", "LA_002_Loop", CONNECTOR, 1, CALM, 100, 50, 15, 3, 6, "LA_002"},
	{"LA_003_loop", "LA_003_Loop", CONNECTOR, 1, CALM, 90, 50, 10, 2, 5, "LA_003"},
	{"LA_FS_3_4_loop", "LA_FS_3_4_Loop", CONNECTOR, 1, CALM, 64, 50, 10, 3, 6, "LA_FS_3_4"},
	{"silence", NULL, CONNECTOR, 0, 0, 20, 0, 0, 0, 0, ""},
};
#define NTRACK (int)(sizeof TRACKS / sizeof *TRACKS)
#define FIRST 6   /* LA_006 opens every session */

static int history[5], nhistory;

/* discoveries in the last 1, 2.5 and 5 minutes decide whether the next track is busy or calm */
static double clockSec;         /* audio time: advances with the frames rendered */
static double found[32];
static int nfound;
static int mood(void) {
	int score = 0;
	for (int i = 0; i < 32 && i < nfound; i++) {
		double s = clockSec - found[i];
		score += s < 60 ? 3 : s < 150 ? 2 : s < 300 ? 1 : 0;
	}
	return score > 1 ? BUSY : CALM;
}

static int pick(int type, int md, int latest) {
	int c[NTRACK], n = 0;
	for (int i = 0; i < NTRACK; i++) {
		const Track *t = &TRACKS[i];
		if (t->type != type || i == latest || (md && t->mood != md)) continue;
		if (latest >= 0 && strstr(TRACKS[latest].prevents, t->name)) continue;
		c[n++] = i;
	}
	return n ? c[rand() % n] : FIRST;
}
static int next_track(void) {
	int latest = nhistory ? history[0] : -1;
	const Track *cur = latest >= 0 ? &TRACKS[latest] : NULL;
	int t;
	if (!cur) t = NTRACK - 1;
	else if (cur->type == TRACK && cur->mood == BUSY) t = pick(CONNECTOR, 0, latest);
	else if (cur->mood == CALM) t = rand() & 1 ? pick(CONNECTOR, 0, latest) : pick(TRACK, CALM, latest);
	else t = cur->type == CONNECTOR ? pick(TRACK, mood(), latest) : pick(CONNECTOR, 0, latest);
	return t;
}
static void remember(int t) {
	memmove(history + 1, history, sizeof history - sizeof *history);
	history[0] = t;
	if (nhistory < 5) nhistory++;
}

static stb_vorbis *mv;            /* the track being played */
static const Track *mt;
static int mch, first = 1;
static double mSecs;              /* how far into the track */
static double gap;                /* silence still to wait, seconds */

static void music_close(void) {
	if (mv) stb_vorbis_close(mv);
	mv = NULL;
}

static void music_start(void) {
	int t = first ? FIRST : next_track();
	if (nhistory && t != FIRST) {
		/* tracks heard recently get one re-roll, like the original */
		for (int i = 0; i < nhistory; i++) if (history[i] == t) { t = next_track(); break; }
	}
	first = 0;
	remember(t);
	mt = &TRACKS[t];
	mSecs = 0;
	if (!mt->file) { gap = mt->duration; mt = NULL; return; }
	char path[256];
	snprintf(path, sizeof path, "%s/music/%s.ogg", dir, mt->file);
	mv = stb_vorbis_open_filename(path, NULL, NULL);
	if (!mv) { gap = 2; mt = NULL; return; }
	stb_vorbis_info vi = stb_vorbis_get_info(mv);
	mch = vi.channels;
	if (vi.sample_rate != AUDIO_RATE || mch < 1 || mch > 2) { music_close(); gap = 2; mt = NULL; }
}

/* mix up to n frames of music into acc (stereo, 32-bit); returns how many were mixed, which is
 * fewer than n when the track ended (mt is then NULL and gap holds the silence that follows) */
static int music_mix(int32_t *acc, int n) {
	static short tmp[2048 * 2];
	float end = mt->loop ? mt->duration : 1e9f;
	float full = 256 * MUSIC_GAIN * mt->volume / 100;   /* gain in 1/256 */
	int done = 0;
	while (done < n) {
		int want = n - done > 2048 ? 2048 : n - done;
		int got = stb_vorbis_get_samples_short_interleaved(mv, mch, tmp, want * mch);
		if (got <= 0) {
			if (mt->loop && stb_vorbis_seek_start(mv)) continue;
			break;
		}
		for (int i = 0; i < got; i++) {
			double secs = mSecs + (double)i / AUDIO_RATE;
			float v = full;
			if (mt->fadeIn && secs < mt->fadeIn) v *= secs / mt->fadeIn;
			if (mt->loop && mt->fadeOut && secs > end - mt->fadeOut) v *= (end - secs) / mt->fadeOut;
			int g = v < 0 ? 0 : (int)v;
			int l = tmp[i * mch], r = tmp[i * mch + (mch == 2)];
			acc[(done + i) * 2] += l * g >> 8;
			acc[(done + i) * 2 + 1] += r * g >> 8;
		}
		done += got;
		mSecs += (double)got / AUDIO_RATE;
		if (mSecs >= end) break;   /* looped tracks play for their duration only */
	}
	if (done < n) {
		music_close();
		gap = mt->silenceAfter;
		mt = NULL;
	}
	return done;
}

static int32_t acc[1024 * 2];

void audio_render(int16_t *out, int frames) {
	while (frames > 0) {
		int n = frames > 1024 ? 1024 : frames;
		memset(acc, 0, n * 8);
		if (running) {
			while (cmdTail != cmdHead) {
				int id = cmdQ[cmdTail & 63];
				__sync_synchronize();
				cmdTail++;
				if (id == CMD_DISCOVERY) { found[nfound++ % 32] = clockSec; continue; }
				for (int i = 0; i < NFX; i++) {
					if (fx[i].id != id || !fx[i].pcm) continue;
					for (int v = 0; v < MAX_VOICES; v++)
						if (!voices[v].e) { voices[v].e = &fx[i]; voices[v].pos = 0; break; }
					break;
				}
			}
			for (int v = 0; v < MAX_VOICES; v++) {
				if (!voices[v].e) continue;
				const Effect *e = voices[v].e;
				int k = e->frames - voices[v].pos;
				if (k > n) k = n;
				const int16_t *p = e->pcm + voices[v].pos * 2;
				for (int i = 0; i < k * 2; i++) acc[i] += p[i];
				voices[v].pos += k;
				if (voices[v].pos >= e->frames) voices[v].e = NULL;
			}
			if (!musicOn || !haveMusic) { music_close(); mt = NULL; gap = 0; }
			else {
				int off = 0;
				for (int guard = 0; off < n && guard < 8; guard++) {
					if (!mt && gap <= 0) music_start();
					if (!mt) {   /* waiting out a gap */
						int g = (int)(gap * AUDIO_RATE) + 1;
						if (g >= n - off) { gap -= (double)(n - off) / AUDIO_RATE; break; }
						off += g;
						gap = 0;
						continue;
					}
					off += music_mix(acc + off * 2, n - off);
				}
			}
		}
		for (int i = 0; i < n * 2; i++) out[i] = (int16_t)(acc[i] > 32767 ? 32767 : acc[i] < -32768 ? -32768 : acc[i]);
		clockSec += (double)n / AUDIO_RATE;
		out += n * 2;
		frames -= n;
	}
}

int audio_init(const char *base) {
	snprintf(dir, sizeof dir, "%s", base);
	int n = 0;
	for (int i = 0; i < NFX; i++) n += load_effect(&fx[i]);
	char path[256];
	snprintf(path, sizeof path, "%s/music/%s.ogg", dir, TRACKS[FIRST].file);
	FILE *f = fopen(path, "rb");
	if (f) { fclose(f); srand((unsigned)time(NULL)); haveMusic = 1; }
	running = n > 0 || haveMusic;
	return n;
}

void audio_shutdown(void) {
	running = 0;
}
