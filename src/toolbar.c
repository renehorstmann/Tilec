#include <assert.h>

#include "r/texture.h"
#include "r/ro_text.h"
#include "u/pose.h"
#include "mathc/float.h"

#include "button.h"
#include "tiles.h"
#include "camera.h"
#include "brush.h"
#include "brush_shape.h"
#include "canvas.h"
#include "canvas_camera_control.h"
#include "animation.h"
#include "selection.h"
#include "palette.h"
#include "savestate.h"
#include "io.h"
#include "toolbar.h"

#define MODES 7
#define LONG_PRESS_TIME 1.0

#define TOOL_MAX 32

struct ToolbarGlobals_s toolbar;

typedef struct {
    rRoSingle btn;
    float x, y;
} Tool;


static struct {
    Tool tools[TOOL_MAX];
    int tools_size;

    rRoSingle *undo;
    rRoSingle *clear;
    rRoSingle *import;
    rRoSingle *selection;
    rRoSingle *grid;
    bool grid_status;

    rRoSingle *camera;
    rRoSingle *animation;
    rRoSingle *shade;

    rRoSingle *modes[MODES];

    rRoSingle *shape_minus;
    rRoSingle *shape_plus;

    //non tools:
    rRoSingle shape;
    float shape_minus_time, shape_plus_time;

    rRoSingle color_drop;
    GLuint color_clear_tex;

    rRoSingle selection_copy;
    rRoSingle selection_cut;
    rRoSingle selection_rotate_left;
    rRoSingle selection_rotate_right;
    rRoSingle selection_mirror_horizontal;
    rRoSingle selection_mirror_vertical;
    rRoSingle selection_ok;

    rRoSingle layer_prev;
    rRoSingle layer_next;
    rRoText layer_num;
    rRoText layer_title;

    rRoSingle tiles_prev;
    rRoSingle tiles_next;
    rRoText tiles_num;
    rRoText tiles_title;

} L;

static rRoSingle *tool_append(float x, float y, const char *btn_file) {
    assert(L.tools_size < TOOL_MAX);
    Tool *self = &L.tools[L.tools_size++];

    button_init(&self->btn, r_texture_new_file(btn_file, NULL));
    self->x = x;
    self->y = y;
    return &self->btn;
}

static bool pos_in_toolbar(vec2 pos) {
    float size = toolbar.show_selection_copy_cut
                 || toolbar.show_selection_ok ?
                 53 : 34;
    if (camera_is_portrait_mode()) {
        if (pos.y >= camera_top() - size)
            return true;
        if (pos.y <= camera_bottom() + 50
            && pos.x >= 40)
            return true;
    } else {
        if (pos.x <= camera_left() + size)
            return true;
        if (pos.x >= camera_right() - 100
            && pos.y >= 60)
            return true;
    }
    return false;
}

static void unpress(rRoSingle **btns, int n, int ignore) {
    for (int i = 0; i < n; i++) {
        if (i == ignore)
            continue;
        button_set_pressed(btns[i], false);
    }
}


static mat4 pose_wh(float col, float row, float w, float h) {
    mat4 pose = mat4_eye();
    if (camera_is_portrait_mode()) {
        u_pose_set(&pose, col, camera_top() - row, w, h, 0);
    } else {
        u_pose_set(&pose, camera_left() + row, col, w, h, 0);
    }
    return pose;
}

static mat4 pose16(float col, float row) {
    return pose_wh(col, row, 16, 16);
}


void toolbar_init() {
    L.undo = tool_append(-80, 26, "res/button_undo.png");
    L.clear = tool_append(-80, 9, "res/button_clear.png");
    L.import = tool_append(-62, 9, "res/button_import.png");
    L.selection = tool_append(-44, 9, "res/button_selection.png");
    L.grid = tool_append(80, 26, "res/button_grid.png");
    L.camera = tool_append(80, 9, "res/button_camera.png");
    L.animation = tool_append(64, 26, "res/button_play.png");
    L.shade = tool_append(48, 9, "res/button_shade.png");

    for (int i = 0; i < MODES; i++) {
        L.modes[i] = tool_append(-55 + 16 * i, 26,
                                 (const char *[]) {
                                         "res/button_free.png",
                                         "res/button_dot.png",
                                         "res/button_dither.png",
                                         "res/button_dither2.png",
                                         "res/button_fill.png",
                                         "res/button_fill8.png",
                                         "res/button_replace.png"
                                 }[i]);
    }
    button_set_pressed(L.modes[0], true);

    L.shape_minus = tool_append(9, 9, "res/button_minus.png");
    L.shape_plus = tool_append(25, 9, "res/button_plus.png");


    // shape kernel:
    r_ro_single_init(&L.shape, camera.gl, brush_shape_create_kernel_texture(COLOR_TRANSPARENT, COLOR_WHITE));

    // secondar color:
    L.color_clear_tex = r_texture_new_file("res/toolbar_color_bg.png", NULL);

    r_ro_single_init(&L.color_drop, camera.gl, tiles.textures[0]);
    L.color_drop.owns_tex = false; // tiles.h owns



    // selection buttons:
    button_init(&L.selection_copy, r_texture_new_file("res/button_copy.png", NULL));

    button_init(&L.selection_cut, r_texture_new_file("res/button_cut.png", NULL));

    button_init(&L.selection_rotate_left, r_texture_new_file("res/button_rotate_left.png", NULL));

    button_init(&L.selection_rotate_right, r_texture_new_file("res/button_rotate_right.png", NULL));

    button_init(&L.selection_mirror_horizontal, r_texture_new_file("res/button_horizontal.png", NULL));

    button_init(&L.selection_mirror_vertical, r_texture_new_file("res/button_vertical.png", NULL));

    button_init(&L.selection_ok, r_texture_new_file("res/button_ok.png", NULL));

    // layer:
    button_init(&L.layer_prev, r_texture_new_file("res/button_prev.png", NULL));

    button_init(&L.layer_next, r_texture_new_file("res/button_next.png", NULL));

    r_ro_text_init_font55(&L.layer_num, 3, camera.gl);

    r_ro_text_init_font55(&L.layer_title, 5, camera.gl);
    r_ro_text_set_text(&L.layer_title, "layer");


    // tiles
    button_init(&L.tiles_prev, r_texture_new_file("res/button_prev.png", NULL));

    button_init(&L.tiles_next, r_texture_new_file("res/button_next.png", NULL));

    r_ro_text_init_font55(&L.tiles_num, 3, camera.gl);

    r_ro_text_init_font55(&L.tiles_title, 5, camera.gl);
    r_ro_text_set_text(&L.tiles_title, "tiles");
}

void toolbar_update(float dtime) {
    for (int i = 0; i < L.tools_size; i++) {
        Tool *t = &L.tools[i];
        t->btn.rect.pose = pose16(t->x, t->y);
    }

    // shape kernel:
    L.shape.rect.pose = pose16(-22, 10);  // should be 16x16
    L.shape.rect.uv = brush_shape_kernel_texture_uv(brush.shape);

    // secondary color:    
    L.color_drop.rect.pose = pose16(64, 9);
    int tile_id = brush.secondary_color.b;
    if (tile_id == 0) {
        r_ro_single_set_texture(&L.color_drop, L.color_clear_tex);
        L.color_drop.rect.uv = mat4_eye();
    } else {
        r_ro_single_set_texture(&L.color_drop, tiles.textures[tile_id - 1]);
        float w = 1.0 / TILES_COLS;
        float h = 1.0 / TILES_ROWS;
        int c = brush.secondary_color.a % TILES_COLS;
        int r = brush.secondary_color.a / TILES_COLS;
        L.color_drop.rect.uv = u_pose_new(c * w, r * h, w, h);
    }

    // selection buttons:
    L.selection_copy.rect.pose = pose16(-8, 43);
    L.selection_cut.rect.pose = pose16(8, 43);
    L.selection_rotate_left.rect.pose = pose16(-48, 43);
    L.selection_rotate_right.rect.pose = pose16(-32, 43);
    L.selection_mirror_horizontal.rect.pose = pose16(-16, 43);
    L.selection_mirror_vertical.rect.pose = pose16(0, 43);
    L.selection_ok.rect.pose = pose16(20, 43);

    // layer:
    char buf[8];
    sprintf(buf, "%d", canvas.current_layer);
    vec2 size = r_ro_text_set_text(&L.layer_num, buf);
    if (camera_is_portrait_mode()) {
        L.layer_prev.rect.pose = u_pose_new(50, floorf(camera_bottom() + 10), 16, 16);
        L.layer_next.rect.pose = u_pose_new(80, floorf(camera_bottom() + 10), 16, 16);
        u_pose_set_xy(&L.layer_num.pose, floorf(65 - size.x / 2), floorf(camera_bottom() + 10 + size.y / 2));
        u_pose_set_xy(&L.layer_title.pose, 50, floorf(camera_bottom() + 25));
    } else {
        L.layer_prev.rect.pose = u_pose_new(floorf(camera_right() - 40), 70, 16, 16);
        L.layer_next.rect.pose = u_pose_new(floorf(camera_right() - 10), 70, 16, 16);
        u_pose_set_xy(&L.layer_num.pose, floorf(camera_right() - 25 - size.x / 2), floorf(70 + size.y / 2));
        u_pose_set_xy(&L.layer_title.pose, floorf(camera_right() - 40), 85);
    }

    // tiles
    sprintf(buf, "%d", palette_get_tile_id());
    size = r_ro_text_set_text(&L.tiles_num, buf);
    if (camera_is_portrait_mode()) {
        L.tiles_prev.rect.pose = u_pose_new(50, floorf(camera_bottom() + 40), 16, 16);
        L.tiles_next.rect.pose = u_pose_new(80, floorf(camera_bottom() + 40), 16, 16);
        u_pose_set_xy(&L.tiles_num.pose, floorf(65 - size.x / 2), floorf(camera_bottom() + 40 + size.y / 2));
        u_pose_set_xy(&L.tiles_title.pose, 50, camera_bottom() + 55);
    } else {
        L.tiles_prev.rect.pose = u_pose_new(floorf(camera_right() - 90), 70, 16, 16);
        L.tiles_next.rect.pose = u_pose_new(floorf(camera_right() - 60), 70, 16, 16);
        u_pose_set_xy(&L.tiles_num.pose, floorf(camera_right() - 75 - size.x / 2), floorf(70 + size.y / 2));
        u_pose_set_xy(&L.tiles_title.pose, floorf(camera_right() - 90), 85);
    }


    // shape longpress:
    if (button_is_pressed(L.shape_minus)) {
        L.shape_minus_time += dtime;
        if (L.shape_minus_time > LONG_PRESS_TIME) {
            brush.shape = 0;
        }
    } else
        L.shape_minus_time = 0;

    if (button_is_pressed(L.shape_plus)) {
        L.shape_plus_time += dtime;
        if (L.shape_plus_time > LONG_PRESS_TIME) {
            brush.shape = BRUSH_NUM_SHAPES - 1;
        }
    } else
        L.shape_plus_time = 0;

}

void toolbar_render() {
    for (int i = 0; i < L.tools_size; i++) {
        r_ro_single_render(&L.tools[i].btn);
    }

    // shape kernel;
    r_ro_single_render(&L.shape);

    // secondary color:
    r_ro_single_render(&L.color_drop);

    // selection buttons:
    if (toolbar.show_selection_copy_cut) {
        r_ro_single_render(&L.selection_copy);
        r_ro_single_render(&L.selection_cut);
    }
    if (toolbar.show_selection_ok) {
        r_ro_single_render(&L.selection_rotate_left);
        r_ro_single_render(&L.selection_rotate_right);
        r_ro_single_render(&L.selection_mirror_horizontal);
        r_ro_single_render(&L.selection_mirror_vertical);
        r_ro_single_render(&L.selection_ok);
    }

    if (canvas_image()->layers > 1) {
        r_ro_single_render(&L.layer_prev);
        r_ro_single_render(&L.layer_next);
        r_ro_text_render(&L.layer_num);
        r_ro_text_render(&L.layer_title);
    }

    if (tiles.size > 1) {
        r_ro_single_render(&L.tiles_prev);
        r_ro_single_render(&L.tiles_next);
        r_ro_text_render(&L.tiles_num);
        r_ro_text_render(&L.tiles_title);
    }
}

// return true if the pointer was used (indicate event done)
bool toolbar_pointer_event(ePointer_s pointer) {
    if (!pos_in_toolbar(pointer.pos.xy))
        return false;


    if (button_clicked(L.undo, pointer)) {
        savestate_undo();
    }

    if (button_clicked(L.clear, pointer)) {
        canvas_clear();
    }

    if (button_clicked(L.import, pointer)) {
        brush_set_selection_active(false, true);
        if (toolbar.show_selection_ok) {
            canvas_redo_image();
        }
        toolbar.show_selection_copy_cut = false;

        Image *img = io_load_image(io.default_import_file, 1);
        button_set_pressed(L.selection, img != NULL);
        toolbar.show_selection_ok = img != NULL;


        if (img) {
            selection_init(0, 0, img->cols, img->rows);
            selection_copy(img, 0);
            selection_paste(canvas_image(), canvas.current_layer);
            image_delete(img);
            brush.selection_mode = BRUSH_SELECTION_PASTE;
            brush_set_selection_active(true, false);
        }
    }

    if (button_toggled(L.selection, pointer)) {
        bool pressed = button_is_pressed(L.selection);
        brush_set_selection_active(pressed, true);
        button_set_pressed(&L.selection_copy, false);
        button_set_pressed(&L.selection_cut, false);

        if (!pressed && toolbar.show_selection_ok) {
            canvas_redo_image();
        }
        toolbar.show_selection_copy_cut = false;
        toolbar.show_selection_ok = false;
    }

    if (button_toggled(L.grid, pointer)) {
        bool pressed = button_is_pressed(L.grid);
        canvas.show_grid = pressed;
        if (pressed) {
            canvas.alpha = L.grid_status ? 1 : 0.5;
            L.grid_status = !L.grid_status;
        }
    }

    if (button_clicked(L.camera, pointer)) {
        canvas_camera_control_set_home();
    }

    if (button_toggled(L.animation, pointer)) {
        animation.show = button_is_pressed(L.animation);
    }

    if (button_toggled(L.shade, pointer)) {
        brush.shading_active = button_is_pressed(L.shade);
    }


    for (int i = 0; i < MODES; i++) {
        if (button_pressed(L.modes[i], pointer)) {
            unpress(L.modes, MODES, i);

            if (i == 0)
                brush.mode = BRUSH_MODE_FREE;
            else if (i == 1)
                brush.mode = BRUSH_MODE_DOT;
            else if (i == 2)
                brush.mode = BRUSH_MODE_DITHER;
            else if (i == 3)
                brush.mode = BRUSH_MODE_DITHER2;
            else if (i == 4)
                brush.mode = BRUSH_MODE_FILL;
            else if (i == 5)
                brush.mode = BRUSH_MODE_FILL8;
            else if (i == 6)
                brush.mode = BRUSH_MODE_REPLACE;
        }
    }

    if (button_clicked(L.shape_minus, pointer)) {
        brush.shape--;
        if (brush.shape < 0)
            brush.shape = 0;
    }

    if (button_clicked(L.shape_plus, pointer)) {
        brush.shape++;
        if (brush.shape >= BRUSH_NUM_SHAPES)
            brush.shape = BRUSH_NUM_SHAPES - 1;
    }



    // secondary color:
    if (pointer.action == E_POINTER_DOWN && u_pose_aa_contains(L.color_drop.rect.pose, pointer.pos.xy)) {
        brush.secondary_color = brush.current_color;
    }


    // selection buttons:
    if (toolbar.show_selection_copy_cut) {
        if (button_toggled(&L.selection_copy, pointer)) {
            bool pressed = button_is_pressed(&L.selection_copy);

            if (pressed) {
                button_set_pressed(&L.selection_cut, false);
                brush.selection_mode = BRUSH_SELECTION_COPY;
            }
        }

        if (button_toggled(&L.selection_cut, pointer)) {
            bool pressed = button_is_pressed(&L.selection_cut);

            if (pressed) {
                button_set_pressed(&L.selection_copy, false);
                brush.selection_mode = BRUSH_SELECTION_CUT;
            }
        }
    }

    if (toolbar.show_selection_ok) {
        bool changed = false;
        if (button_clicked(&L.selection_rotate_left, pointer)) {
            selection_rotate(false);
            changed = true;
        }
        if (button_clicked(&L.selection_rotate_right, pointer)) {
            selection_rotate(true);
            changed = true;
        }

        if (button_clicked(&L.selection_mirror_horizontal, pointer)) {
            selection_mirror(false);
            changed = true;
        }
        if (button_clicked(&L.selection_mirror_vertical, pointer)) {
            selection_mirror(true);
            changed = true;
        }

        if (changed) {
            canvas_redo_image();
            selection_paste(canvas_image(), canvas.current_layer);
        }

        if (button_clicked(&L.selection_ok, pointer)) {
            canvas_save();
            brush_set_selection_active(false, true);
            toolbar.show_selection_ok = false;
            button_set_pressed(L.selection, false);
        }
    }


    if (canvas_image()->layers > 1) {
        if (button_clicked(&L.layer_prev, pointer)) {
            canvas.current_layer = sca_max(0, canvas.current_layer - 1);
        }
        if (button_clicked(&L.layer_next, pointer)) {
            canvas.current_layer = sca_min(canvas_image()->layers - 1, canvas.current_layer + 1);
        }
    }

    if (tiles.size > 1) {
        if (button_clicked(&L.tiles_prev, pointer)) {
            palette_change_tiles(false);
        }
        if (button_clicked(&L.tiles_next, pointer)) {
            palette_change_tiles(true);
        }
    }

    return true;
}
