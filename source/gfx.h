#ifndef GFX_H
#define GFX_H
#include <stdint.h>

#define SW 640
#define SH 480

#define RGB(r,g,b) ((uint32_t)(((r)<<16)|((g)<<8)|(b)))
#define CR(c) (((c)>>16)&255)
#define CG(c) (((c)>>8)&255)
#define CB(c) ((c)&255)

/* The screen; drawing goes to the current target, which is the screen unless redirected. */
extern uint32_t fb[SH][SW];
void gfx_target(uint32_t *buf, int w, int h);
void gfx_screen(void);
void gfx_clip(int x0, int y0, int x1, int y1);
void gfx_noclip(void);

typedef struct Font Font;
extern Font *fHuge, *fBig, *fUI, *fName, *fSmall, *fTiny;
enum { AL_LEFT = 0, AL_CENTER = 1, AL_RIGHT = 2 };
void gfx_init(const unsigned char *ttf);

void blend(int x, int y, uint32_t c, int a);
void fill_rect(int x, int y, int w, int h, uint32_t c, int a);
void rrect(float x, float y, float w, float h, float r, uint32_t c, int a);
void disc(float cx, float cy, float rad, uint32_t c, int a);
void ellipse(float cx, float cy, float rx, float ry, uint32_t c, int a);
void ring(float cx, float cy, float rad, float thick, uint32_t c, int a);
void line_aa(float x0, float y0, float x1, float y1, float w, uint32_t c, int a);
void poly_fill(const float *pts, int n, uint32_t c, int a);

/* ARGB sprite with straight alpha */
typedef struct { int w, h; uint32_t *px; } Sprite;
void blit(const Sprite *s, int x, int y, int alpha);
void blit_scaled(const Sprite *s, float cx, float cy, float size, int alpha);

float text_w(Font *f, const char *s);
float text_h(Font *f);
void text(Font *f, float x, float y, const char *s, uint32_t c, int a, int align);

uint32_t shade(uint32_t c, float f);   /* f<1 darker, f>1 lighter */
uint32_t mix(uint32_t a, uint32_t b, float t);

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
#endif
