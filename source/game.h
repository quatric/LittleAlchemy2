#ifndef GAME_H
#define GAME_H
#include <stdint.h>

enum {
	BTN_A = 1 << 0, BTN_B = 1 << 1, BTN_UP = 1 << 2, BTN_DOWN = 1 << 3, BTN_LEFT = 1 << 4, BTN_RIGHT = 1 << 5,
	BTN_PLUS = 1 << 6, BTN_MINUS = 1 << 7, BTN_1 = 1 << 8, BTN_2 = 1 << 9, BTN_HOME = 1 << 10
};
/* wheel: mouse wheel notches this frame, positive = towards the end of the list */
typedef struct { float x, y, wheel; int valid; uint32_t held, down, up; } Input;

void game_init(void);
void game_update(const Input *in, float dt);
void game_draw(void);
int game_quit(void);

/* provided by the platform layer */
int plat_load(char *buf, int max);              /* returns bytes read, or -1 */
void plat_save(const char *buf, int len);
enum { SFX_PICK, SFX_DROP, SFX_COMBINE, SFX_DISCOVER, SFX_FAIL, SFX_DELETE, SFX_CLICK };
void plat_sfx(int id);
void plat_rumble(int ms);
#endif
