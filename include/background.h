#ifndef TILEC_BACKGROUND_H
#define TILEC_BACKGROUND_H

#include "color.h"

void background_init(Color_s a, Color_s b);

void background_update(float dtime);

void background_render();

#endif //TILEC_BACKGROUND_H
