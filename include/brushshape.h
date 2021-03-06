#ifndef TILEC_BRUSHSHAPE_H
#define TILEC_BRUSHSHAPE_H

#include "r/core.h"
#include "r/texture.h"
#include "u/color.h"

#define BRUSH_KERNEL_SIZE 7
#define BRUSH_NUM_SHAPES 16

#define BRUSH_KERNEL_TEXTURE_SIZE 8

struct BrushShapeGlobals_s {
    const char kernels
    [BRUSH_NUM_SHAPES]
    [BRUSH_KERNEL_SIZE][BRUSH_KERNEL_SIZE];
};
extern struct BrushShapeGlobals_s brushshape;

rTexture brushshape_create_kernel_texture(uColor_s bg, uColor_s fg);

bool brushshape_draw(int c, int r);

bool brushshape_draw_dither(int c, int r, bool a);

#endif //TILEC_BRUSHSHAPE_H
