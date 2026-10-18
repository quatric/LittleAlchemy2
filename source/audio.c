/* Little Alchemy 2 music and sound effects from the audio pack (tools/fetch_audio.py):
 * Ogg Vorbis files on the SD card, decoded with Tremor. Effects are decoded once at start-up;
 * music is streamed from the card by a thread that picks tracks the way the original does. */
#include <gccore.h>
#include <asndlib.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ogc/lwp_watchdog.h>
#include <tremor/ivorbisfile.h>
#include "audio.h"
#include "game.h"

#define MUSIC_VOICE 15   /* effects use the voices below it */
/* the original mixes effects at gain 0.6 and music at 0.08; effects get full scale here */
#define MUSIC_GAIN (.08f / .6f)

static char dir[128];

/* ---------------- effects ---------------- */
typedef struct { int id; const char *name; int volume; s16 *pcm; int bytes, rate, stereo; } Effect;
static Effect fx[] = {   /* volumes (percent) as in the original */
	{SFX_CLICK, "GenericClick", 100}, {SFX_PICK, "PickUpItem", 100}, {SFX_DROP, "DropItem", 100},
	{SFX_COMBINE, "AlreadyMixed", 400}, {SFX_CLONE, "CloneItem", 150}, {SFX_FAIL, "FailedToMix", 80},
	{SFX_DELETE, "RemoveItem", 80}, {SFX_DISCOVER, "ChargeUp", 10}, {SFX_POP, "PopUp", 30},
};
#define NFX (int)(sizeof fx / sizeof *fx)

/* decode a whole file, scaling it by gain (clipped); returns the number of effects loaded */
static int load_effect(Effect *e) {
	char path[192];
	snprintf(path, sizeof path, "%s/sfx/%s.ogg", dir, e->name);
	FILE *f = fopen(path, "rb");
	if (!f) return 0;
	OggVorbis_File vf;
	if (ov_open(f, &vf, NULL, 0) < 0) { fclose(f); return 0; }
	vorbis_info *vi = ov_info(&vf, -1);
	e->rate = vi->rate;
	e->stereo = vi->channels == 2;
	int cap = 1 << 16, n = 0, bs;
	char *buf = malloc(cap);
	for (long r = 1; buf && r != 0;) {
		if (cap - n < 4096) { char *nb = realloc(buf, cap * 2); if (!nb) { free(buf); buf = NULL; break; } buf = nb; cap *= 2; }
		r = ov_read(&vf, buf + n, cap - n, &bs);
		if (r > 0) n += r;
	}
	ov_clear(&vf);
	if (!buf || !n) { free(buf); return 0; }
	int bytes = (n + 31) & ~31;
	e->pcm = memalign(32, bytes);
	if (!e->pcm) { free(buf); return 0; }
	memset(e->pcm, 0, bytes);
	float g = e->volume / 100.f;
	const s16 *src = (const s16 *)buf;
	for (int i = 0; i < n / 2; i++) {
		int v = (int)(src[i] * g);
		e->pcm[i] = (s16)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
	}
	free(buf);
	e->bytes = bytes;
	DCFlushRange(e->pcm, bytes);
	return 1;
}

int audio_sfx(int id) {
	for (int i = 0; i < NFX; i++) {
		if (fx[i].id != id) continue;
		if (!fx[i].pcm) return 0;
		for (int v = 0; v < MUSIC_VOICE; v++)
			if (ASND_StatusVoice(v) == SND_UNUSED) {
				ASND_SetVoice(v, fx[i].stereo ? VOICE_STEREO_16BIT : VOICE_MONO_16BIT, fx[i].rate, 0, fx[i].pcm, fx[i].bytes, 255, 255, NULL);
				break;
			}
		return 1;
	}
	return 0;
}

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

static lwp_t musicThread = LWP_THREAD_NULL;
static volatile int running, musicOn = 1;
static int history[5], nhistory;

/* discoveries in the last 1, 2.5 and 5 minutes decide whether the next track is busy or calm */
static u64 found[32];
static int nfound;
void audio_note_discovery(void) { found[nfound++ % 32] = gettime(); }
static int mood(void) {
	u64 now = gettime();
	int score = 0;
	for (int i = 0; i < 32 && i < nfound; i++) {
		u32 ms = ticks_to_millisecs(now - found[i]);
		score += ms < 60000 ? 3 : ms < 150000 ? 2 : ms < 300000 ? 1 : 0;
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

/* sleep in short steps, giving up early when the music is stopped */
static int wait_ms(int ms) {
	for (; ms > 0 && running && musicOn; ms -= 20) usleep(20000);
	return running && musicOn;
}

#define MBYTES (4096 * 4)
static s16 mbuf[2][MBYTES / 2] ATTRIBUTE_ALIGN(32);

static void play(const Track *t) {
	if (!t->file) { wait_ms(t->duration * 1000); return; }
	char path[192];
	snprintf(path, sizeof path, "%s/music/%s.ogg", dir, t->file);
	FILE *f = fopen(path, "rb");
	if (!f) { wait_ms(2000); return; }
	OggVorbis_File vf;
	if (ov_open(f, &vf, NULL, 0) < 0) { fclose(f); wait_ms(2000); return; }
	vorbis_info *vi = ov_info(&vf, -1);
	int rate = vi->rate, stereo = vi->channels == 2, frame = stereo ? 4 : 2, started = 0, b = 0, bs, errs = 0;
	float full = 255 * MUSIC_GAIN * t->volume / 100, secs = 0, end = t->loop ? t->duration : 1e9f;
	while (running && musicOn && secs < end) {
		if (started) while (running && musicOn && !ASND_TestVoiceBufferReady(MUSIC_VOICE)) usleep(2000);
		if (!running || !musicOn) break;
		int n = 0;
		while (n < MBYTES) {
			long r = ov_read(&vf, (char *)mbuf[b] + n, MBYTES - n, &bs);
			if (r > 0) n += r;
			else if (r == 0) { if (!t->loop || ov_pcm_seek(&vf, 0)) break; }
			else if (++errs > 16) break;   /* a damaged file: give up on it */
		}
		if (!n) break;
		memset((char *)mbuf[b] + n, 0, MBYTES - n);
		DCFlushRange(mbuf[b], MBYTES);
		float v = full;
		if (t->fadeIn && secs < t->fadeIn) v *= secs / t->fadeIn;
		if (t->loop && t->fadeOut && secs > end - t->fadeOut) v *= (end - secs) / t->fadeOut;
		int vol = v < 0 ? 0 : (int)v;
		if (!started) {
			ASND_SetVoice(MUSIC_VOICE, stereo ? VOICE_STEREO_16BIT : VOICE_MONO_16BIT, rate, 0, mbuf[b], MBYTES, vol, vol, NULL);
			started = 1;
		} else {
			ASND_AddVoice(MUSIC_VOICE, mbuf[b], MBYTES);
			ASND_ChangeVolumeVoice(MUSIC_VOICE, vol, vol);
		}
		secs += (float)n / frame / rate;
		b ^= 1;
	}
	/* let the queued audio finish, unless the music was switched off */
	while (running && musicOn && ASND_StatusVoice(MUSIC_VOICE) != SND_UNUSED) usleep(5000);
	ASND_StopVoice(MUSIC_VOICE);
	ov_clear(&vf);
	wait_ms(t->silenceAfter * 1000);
}

static void *music_main(void *arg) {
	(void)arg;
	int first = 1;
	while (running) {
		if (!musicOn) { usleep(50000); continue; }
		int t = first ? FIRST : next_track();
		if (nhistory && t != FIRST) {
			/* tracks heard recently get one re-roll, like the original */
			for (int i = 0; i < nhistory; i++) if (history[i] == t) { t = next_track(); break; }
		}
		first = 0;
		remember(t);
		play(&TRACKS[t]);
	}
	return NULL;
}

void audio_music(int on) {
	musicOn = on;
	if (!on) ASND_StopVoice(MUSIC_VOICE);
}

int audio_init(const char *base) {
	snprintf(dir, sizeof dir, "%s", base);
	int n = 0;
	for (int i = 0; i < NFX; i++) n += load_effect(&fx[i]);
	char path[192];
	snprintf(path, sizeof path, "%s/music/%s.ogg", dir, TRACKS[FIRST].file);
	FILE *f = fopen(path, "rb");
	if (f) {
		fclose(f);
		srand(gettime());
		running = 1;
		LWP_CreateThread(&musicThread, music_main, NULL, NULL, 0x8000, 70);
	}
	return n;
}

void audio_shutdown(void) {
	running = 0;
	if (musicThread != LWP_THREAD_NULL) LWP_JoinThread(musicThread, NULL);
	musicThread = LWP_THREAD_NULL;
	ASND_StopVoice(MUSIC_VOICE);
}
