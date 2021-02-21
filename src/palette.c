#include "mathc/sca/int.h"
#include <stdbool.h>
#include <assert.h>
#include "e/input.h"
#include "r/r.h"
#include "u/pose.h"
#include "utilc/alloc.h"
#include "camera.h"
#include "brush.h"
#include "palette.h"

#define TILE_COLS 8
#define TILE_ROWS 8
#define TILE_SIZE 16.0f

_Static_assert(PALETTE_MAX == TILE_COLS * TILE_ROWS + 1, "wrong palette size");

static struct {
    Color_s palette[PALETTE_MAX];
    rRoBatch palette_ro;
    rRoSingle select_ro;
    rRoSingle background_ro;
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
    u_pose_set_size(&pose, TILE_SIZE, TILE_SIZE);
    if (camera_is_portrait_mode()) {
        u_pose_set_xy(&pose, camera_left() + TILE_SIZE / 2 + c * TILE_SIZE,
                      camera_bottom() + palette_get_hud_size() - TILE_SIZE / 2 - r * TILE_SIZE);
    } else {
        u_pose_set_xy(&pose, camera_right() - TILE_SIZE / 2 - r * TILE_SIZE,
                      camera_bottom() + TILE_SIZE / 2 + c * TILE_SIZE);
        u_pose_set_angle(&pose, M_PI_2);
    }
    return pose;
}

static bool load_tiles() {
	char file[128];
	sprintf(file, "tiles/tile_%02i.png", L.tile_id);
	SDL_Log("palette load tiles: %s", file);
	GLuint tex = r_texture_init_file(file, NULL);
	if(!tex)
	    return false;
	r_ro_batch_set_texture(&L.palette_ro, tex);
	for(int i=0; i<PALETTE_MAX;i++) {
		L.palette[i].b = L.tile_id;
	}
	return true;
}

void palette_init() {
    r_ro_batch_init(&L.palette_ro, PALETTE_MAX, camera.gl, 0);
    
    Color_s buf[4];
    buf[0] = buf[3] = color_from_hex("#99aa99");
    buf[1] = buf[2] = color_from_hex("#889988");

    r_ro_single_init(&L.background_ro, camera.gl, r_texture_init(2, 2, buf));
    
    r_ro_single_init(&L.select_ro, camera.gl, r_texture_init_file("res/palette_select.png", NULL));
    L.tile_id = 1;
    L.palette[PALETTE_MAX-2] = (Color_s) {0, 0, 0, 0};
    for(int i=0; i<PALETTE_MAX-1; i++) {
    	L.palette[i] = (Color_s) {0, 0, L.tile_id, i};
    }
    
    load_tiles();
    
    // setup uvs
    float w = 1.0/TILE_COLS;
    float h = 1.0/TILE_ROWS;
    int i=0;
    for(int r=0; r<TILE_ROWS; r++) {
    	for(int c=0; c<TILE_COLS; c++) {
    	    L.palette_ro.rects[i].uv = u_pose_new(c * w, r * h, w, h);
    	    i++;
        }
    }
    L.palette_ro.rects[PALETTE_MAX-2].color = (vec4) {{0}};
    r_ro_batch_update(&L.palette_ro);
}


void palette_update(float dtime) {
    int cols = TILE_COLS;
    int last_row = (PALETTE_MAX - 1) / cols;
    for (int i = 0; i < PALETTE_MAX; i++) {
        int r = i / cols;
        int c = i % cols;

        // pose
        mat4 pose = mat4_eye();
        if (r <= last_row)
            pose = setup_palette_color_pose(r, c);
        else
            u_pose_set(&pose, FLT_MAX, FLT_MAX, 0, 0, 0);
        L.palette_ro.rects[i].pose = pose;
    }

    float uw, uh;
    if(camera_is_portrait_mode()) {
        uw = camera_width() / 2;
        uh = (palette_get_hud_size()+1) / 2;
        L.background_ro.rect.pose = u_pose_new_aa(
            camera_left(), ceilf(camera_bottom() +  palette_get_hud_size()), 
            camera_width(), palette_get_hud_size()+1);
    } else {
         uw = (palette_get_hud_size()+1) / 2;
         uh = camera_height() / 2;
         L.background_ro.rect.pose = u_pose_new_aa(
             floorf(camera_right() - palette_get_hud_size()), camera_top(), 
             palette_get_hud_size()+1, camera_height());
    }
    u_pose_set_size(&L.background_ro.rect.uv, uw, uh);
    float ux = -(camera_right() + camera_left()) / 4;
    float uy = (camera_bottom() + camera_top()) / 4;
    
    u_pose_set_xy(&L.background_ro.rect.uv, ux, uy);

    L.select_ro.rect.pose = L.palette_ro.rects[L.last_selected].pose;

    r_ro_batch_update(&L.palette_ro);
}

void palette_render() {
    r_ro_single_render(&L.background_ro);
    r_ro_batch_render(&L.palette_ro);
    r_ro_single_render(&L.select_ro);
}


bool palette_pointer_event(ePointer_s pointer) {
    if (!pos_in_palette(pointer.pos.xy))
        return false;

    if (pointer.action != E_POINTER_DOWN)
        return true;

    for (int i = 0; i < PALETTE_MAX; i++) {
        if (u_pose_aa_contains(L.palette_ro.rects[i].pose, pointer.pos.xy)) {
            palette_set_color(i);
            return true;
        }
    }

    return true;
}

float palette_get_hud_size() {
	return TILE_ROWS * TILE_SIZE;
}

int palette_get_color() {
    return L.last_selected;
}

void palette_set_color(int index) {
    brush.current_color = L.palette[index];
    L.select_ro.rect.pose = L.palette_ro.rects[index].pose;
    L.last_selected = index;
}

void palette_change_tiles(bool next) {
    if(next)
        L.tile_id++;
    else
        L.tile_id = isca_min(L.tile_id-1, 1);
    
    if(!load_tiles()) {
    	L.tile_id = 1;
    	load_tiles();
    }
}
