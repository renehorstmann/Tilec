#ifndef TILEC_BACKGROUND_H
#define TILEC_BACKGROUND_H

#include "u/color.h"

void background_init(uColor_s a, uColor_s b);

void background_update(float dtime);

void background_render();

#endif //TILEC_BACKGROUND_H
