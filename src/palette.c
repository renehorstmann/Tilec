#include <stdbool.h>
#include <assert.h>
#include "mathc/sca/int.h"
#include "utilc/alloc.h"
#include "e/input.h"
#include "r/r.h"
#include "u/pose.h"

#include "tiles.h"
#include "camera.h"
#include "brush.h"
#include "palette.h"

_Static_assert(PALETTE_MAX == TILES_COLS * TILES_ROWS + 1, "wrong palette size");

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
    u_pose_set_size(&pose, TILES_SIZE, TILES_SIZE);
    if (camera_is_portrait_mode()) {
        u_pose_set_xy(&pose, camera_left() + TILES_SIZE / 2 + c * TILES_SIZE,
                      camera_bottom() + palette_get_hud_size() - TILES_SIZE / 2 - r * TILES_SIZE);
    } else {
        u_pose_set_xy(&pose, camera_right() - TILES_SIZE / 2 - r * TILES_SIZE,
                      camera_bottom() + TILES_SIZE / 2 + c * TILES_SIZE);
        u_pose_set_angle(&pose, M_PI_2);
    }
    return pose;
}


void palette_init() {
    L.tile_id = 1;
    r_ro_batch_init(&L.palette_ro, PALETTE_MAX, camera.gl, tiles.textures[L.tile_id-1]);
    L.palette_ro.owns_tex = false; // tiles.h owns
    
    Color_s buf[4];
    buf[0] = buf[3] = color_from_hex("#99aa99");
    buf[1] = buf[2] = color_from_hex("#889988");

    r_ro_single_init(&L.background_ro, camera.gl, r_texture_init(2, 2, buf));
    
    r_ro_single_init(&L.select_ro, camera.gl, r_texture_init_file("res/palette_select.png", NULL));
    L.palette[PALETTE_MAX-2] = (Color_s) {0, 0, 0, 0};
    for(int i=0; i<PALETTE_MAX-1; i++) {
    	L.palette[i] = (Color_s) {0, 0, L.tile_id, i};
    }
    
    
    // setup uvs
    float w = 1.0/TILES_COLS;
    float h = 1.0/TILES_ROWS;
    int i=0;
    for(int r=0; r<TILES_ROWS; r++) {
    	for(int c=0; c<TILES_COLS; c++) {
    	    L.palette_ro.rects[i].uv = u_pose_new(c * w, r * h, w, h);
    	    i++;
        }
    }
    L.palette_ro.rects[PALETTE_MAX-2].color = (vec4) {{0}};
    r_ro_batch_update(&L.palette_ro);
}


void palette_update(float dtime) {
    int cols = TILES_COLS;
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
	return TILES_ROWS * TILES_SIZE;
}

int palette_get_color() {
    return L.last_selected;
}

int palette_get_tile_id() {
    return L.tile_id;	
}

void palette_set_color(int index) {
    brush.current_color = L.palette[index];
    L.select_ro.rect.pose = L.palette_ro.rects[index].pose;
    L.last_selected = index;
}

void palette_change_tiles(bool next) {
    L.tile_id += next? 1 : -1;
    if(L.tile_id<=0)
        L.tile_id = tiles.size;
    if(L.tile_id>tiles.size)
        L.tile_id = 1;
    
    r_ro_batch_set_texture(&L.palette_ro, tiles.textures[L.tile_id-1]);
    
    for(int i=0; i<PALETTE_MAX-1; i++) {
    	L.palette[i] = (Color_s) {0, 0, L.tile_id, i};
    }
    
    // todo: set selection and color to 0
}
