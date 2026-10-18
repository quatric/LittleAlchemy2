#ifndef ICONS_H
#define ICONS_H
#include "gfx.h"
/* big = 0: 52px tile icon, big = 1: 104px for the discovery card. Baked on first use. */
const Sprite *icon_get(int el, int big);
/* provided by the platform layer: where the optional icon pack lives, or NULL */
const char *plat_icon_path(void);
int icon_size(int big);
#endif
