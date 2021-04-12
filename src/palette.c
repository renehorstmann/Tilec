#include <stdbool.h>
#include <assert.h>
#include "e/input.h"
#include "r/r.h"
#include "u/pose.h"
#include "mathc/sca/int.h"
#include "utilc/alloc.h"

#include "tiles.h"
#include "camera.h"
#include "brush.h"
#include "palette.h"

#define PALETTE_SIZE (TILES_COLS * TILES_ROWS)


static struct {
    Color_s palette[PALETTE_SIZE];
    RoBatch palette_ro;
    RoSingle palette_clear_ro;
    RoSingle select_ro;
    RoSingle background_ro;
    int last_selected;
    float last_camera_width, last_camera_height;
    int tile_id;
} L;


static bool pos_in_palette(vec2 pos) {
    if (camera_is_portrait_mode()) {
        return pos.y <= camera_bottom() + palette_get_hud_size();
    } else {
        return pos.x >= camera_right() - palette_get_hud_size();
    }
}


static mat4 setup_palette_color_pose(int r, int c) {
    mat4 pose = mat4_eye();
    u_pose_set_size(&pose, TILES_SIZE, TILES_SIZE);
    if (camera_is_portrait_mode()) {
        u_pose_set_xy(&pose, camera_left() + TILES_SIZE / 2 + c * TILES_SIZE,
                      floorf(camera_bottom() + palette_get_hud_size() - TILES_SIZE / 2 - r * TILES_SIZE));
    } else {
        u_pose_set_xy(&pose, floorf(camera_right() - palette_get_hud_size() + TILES_SIZE / 2 + c * TILES_SIZE),
                      floorf(camera_bottom() + TILES_SIZE * TILES_ROWS - TILES_SIZE / 2 - r * TILES_SIZE));
    }
    return pose;
}


void palette_init() {
    L.tile_id = 1;
    ro_batch_init(&L.palette_ro, PALETTE_SIZE, camera.gl, tiles.textures[L.tile_id - 1]);
    L.palette_ro.owns_tex = false; // tiles.h owns

    ro_single_init(&L.palette_clear_ro, camera.gl, r_texture_new_file("res/toolbar_color_bg.png", NULL));

    Color_s buf[4];
    buf[0] = buf[3] = color_from_hex("#99aa99");
    buf[1] = buf[2] = color_from_hex("#889988");

    ro_single_init(&L.background_ro, camera.gl, r_texture_new(2, 2, buf));

    ro_single_init(&L.select_ro, camera.gl, r_texture_new_file("res/palette_select.png", NULL));
    for (int i = 0; i < PALETTE_SIZE; i++) {
        L.palette[i] = (Color_s) {0, 0, L.tile_id, i};
    }


    // setup uvs
    float w = 1.0 / TILES_COLS;
    float h = 1.0 / TILES_ROWS;
    int i = 0;
    for (int r = 0; r < TILES_ROWS; r++) {
        for (int c = 0; c < TILES_COLS; c++) {
            L.palette_ro.rects[i].uv = u_pose_new(c * w, r * h, w, h);
            i++;
        }
    }
    ro_batch_update(&L.palette_ro);

    palette_set_color(-1);
    brush.secondary_color = brush.current_color;
}


void palette_update(float dtime) {
    for (int i = 0; i < PALETTE_SIZE; i++) {
        int r = i / TILES_COLS;
        int c = i % TILES_COLS;

        L.palette_ro.rects[i].pose = setup_palette_color_pose(r, c);;
    }

    if (camera_is_portrait_mode())
        L.palette_clear_ro.rect.pose = setup_palette_color_pose(0, TILES_COLS + 1);
    else
        L.palette_clear_ro.rect.pose = setup_palette_color_pose(-2, 0);

    float uw, uh;
    if (camera_is_portrait_mode()) {
        uw = camera_width() / 2;
        uh = (palette_get_hud_size() + 1) / 2;
        L.background_ro.rect.pose = u_pose_new_aa(
                camera_left(), ceilf(camera_bottom() + palette_get_hud_size()),
                camera_width(), ceilf(palette_get_hud_size() + 1));
    } else {
        uw = (palette_get_hud_size() + 1) / 2;
        uh = camera_height() / 2;
        L.background_ro.rect.pose = u_pose_new_aa(
                floorf(camera_right() - palette_get_hud_size()), camera_top(),
                ceilf(palette_get_hud_size() + 1), camera_height());
    }
    u_pose_set_size(&L.background_ro.rect.uv, uw, uh);

    if (L.last_selected == -1)
        L.select_ro.rect.pose = L.palette_clear_ro.rect.pose;
    else
        L.select_ro.rect.pose = L.palette_ro.rects[L.last_selected].pose;

    ro_batch_update(&L.palette_ro);
}

void palette_render() {
    ro_single_render(&L.background_ro);
    ro_batch_render(&L.palette_ro);
    ro_single_render(&L.palette_clear_ro);
    ro_single_render(&L.select_ro);
}


bool palette_pointer_event(ePointer_s pointer) {
    if (!pos_in_palette(pointer.pos.xy))
        return false;

    if (pointer.action != E_POINTER_DOWN)
        return true;

    for (int i = 0; i < PALETTE_SIZE; i++) {
        if (u_pose_aa_contains(L.palette_ro.rects[i].pose, pointer.pos.xy)) {
            palette_set_color(i);
            return true;
        }
    }

    if (u_pose_aa_contains(L.palette_clear_ro.rect.pose, pointer.pos.xy)) {
        palette_set_color(-1);
        return true;
    }

    return true;
}

float palette_get_hud_size() {
    return TILES_ROWS * TILES_SIZE;
}

int palette_get_color() {
    return L.last_selected;
}

int palette_get_tile_id() {
    return L.tile_id;
}

void palette_set_color(int index) {
    if (index == -1)
        brush.current_color = (Color_s) {0};
    else
        brush.current_color = L.palette[index];
    L.last_selected = index;
}

void palette_change_tiles(bool next) {
    L.tile_id += next ? 1 : -1;
    if (L.tile_id <= 0)
        L.tile_id = tiles.size;
    if (L.tile_id > tiles.size)
        L.tile_id = 1;

    ro_batch_set_texture(&L.palette_ro, tiles.textures[L.tile_id - 1]);

    for (int i = 0; i < PALETTE_SIZE; i++) {
        L.palette[i] = (Color_s) {0, 0, L.tile_id, i};
    }

    palette_set_color(-1);
}
