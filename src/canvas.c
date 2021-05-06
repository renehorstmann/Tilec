#include <assert.h>
#include <float.h>
#include "r/ro_single.h"
#include "r/ro_batch.h"
#include "r/texture.h"
#include "u/pose.h"
#include "mathc/mat/float.h"

#include "tiles.h"
#include "canvascam.h"
#include "palette.h"
#include "toolbar.h"
#include "selection.h"
#include "savestate.h"
#include "canvas.h"

#define MAX_LAYERS 16
#define SELECTION_BORDER_FACTOR 4


//
// private
//

typedef struct {
    int cols, rows, layers;
    uColor_s data[];
} SaveImage;

struct CanvasGlobals_s canvas = {
        .default_image_file = "tilemap.png",
        .default_import_file = "import.png"
};

static struct {
    mat4 pose;
    mat4 mvp;

    uImage image;
    uImage prev_image;

    RoSingle bg;
    RoSingle grid;

    RoBatch selection_border;

    RoBatch tiles[MAX_LAYERS][MAX_TILES];

    int save_id;
} L;

static void init_tile_ro(RoBatch *ro, rTexture tex) {
    float w = 1.0 / L.image.cols;
    float h = 1.0 / L.image.rows;
    *ro = ro_batch_new(L.image.cols * L.image.rows, &L.mvp.m00, tex);
    ro->owns_tex = false; // tiles.h owns it

    for (int r = 0; r < L.image.rows; r++) {
        for (int c = 0; c < L.image.cols; c++) {
            ro->rects[r * L.image.cols + c].pose = u_pose_new_aa(-0.5 + c * w, 0.5 - r * h, w, h);
        }
    }
}

static void init_render_objects() {
    for (int layer = 0; layer < L.image.layers; layer++) {
        for (int i = 0; i < tiles.size; i++) {
            init_tile_ro(&L.tiles[layer][i], tiles.textures[i]);
        }
    }
}

static mat4 pixel_pose(int x, int y) {
    float w = u_pose_get_w(L.pose);
    float size = w / L.image.cols;


    float pos_x = u_pose_aa_get_left(L.pose) + (x + 0.5f) * size;
    float pos_y = u_pose_aa_get_top(L.pose) - (y + 0.5f) * size;

    mat4 pose = mat4_eye();
    u_pose_set(&pose, pos_x, pos_y, size, size, 0);
    return pose;
}

static void setup_selection() {
    int x = selection_pos().x;
    int y = selection_pos().y;
    int w = selection_size().x;
    int h = selection_size().y;

    int max = L.selection_border.num;

    int idx = 0;
    for (int i = 0; i < h; i++) {
        L.selection_border.rects[idx].pose = pixel_pose(x - 1, y + i);
        L.selection_border.rects[idx].sprite = (vec2) {0, 0};
        idx++;
        if (idx >= max) goto UPDATE;

        L.selection_border.rects[idx].pose = pixel_pose(x + w, y + i);
        L.selection_border.rects[idx].sprite = (vec2) {0, 1};
        idx++;
        if (idx >= max) goto UPDATE;
    }
    for (int i = 0; i < w; i++) {
        L.selection_border.rects[idx].pose = pixel_pose(x + i, y - 1);
        L.selection_border.rects[idx].sprite = (vec2) {1, 0};
        idx++;
        if (idx >= max) goto UPDATE;

        L.selection_border.rects[idx].pose = pixel_pose(x + i, y + h);
        L.selection_border.rects[idx].sprite = (vec2) {1, 1};
        idx++;
        if (idx >= max) goto UPDATE;
    }

    for (; idx < L.selection_border.num; idx++) {
        u_pose_set(&L.selection_border.rects[idx].pose, FLT_MAX, FLT_MAX, 0, 0, 0);
    }

    UPDATE:
    ro_batch_update(&L.selection_border);
}


static void set_pixel_tile(int layer, int c, int r) {

    for (int i = 0; i < tiles.size; i++) {
        int idx = r * L.image.cols + c;
        L.tiles[layer][i].rects[idx].color.a = 0;
    }

    uColor_s code = *u_image_pixel(L.image, c, r, layer);

    int tile_id = code.b;

    if (layer <= canvas.current_layer && tile_id > 0 && tile_id <= tiles.size) {
        tile_id--;

        vec4 color = (vec4) {{1, 1, 1, (float) layer / canvas.current_layer}};

        int tile_x = code.a % TILES_COLS;
        int tile_y = code.a / TILES_COLS;

        int idx = r * L.image.cols + c;
        L.tiles[layer][tile_id].rects[idx].sprite = (vec2) {{tile_x, tile_y}};

        float alpha = (layer + 1.0) / (canvas.current_layer + 1.0);
        L.tiles[layer][tile_id].rects[idx].color.a = alpha * canvas.alpha;
    }


}


static void save_state() {
    log_info("canvas: save_state");
    size_t img_size = sizeof(SaveImage) + u_image_data_size(L.image);
    SaveImage *img = rhc_malloc_raising(img_size);
    img->cols = L.image.cols;
    img->rows = L.image.rows;
    img->layers = L.image.layers;
    memcpy(img->data, L.image.data, u_image_data_size(L.image));
    savestate_save_data(img, img_size);
    rhc_free(img);
}

static void load_state(const void *data, size_t size) {
    log_info("canvas: load_state");
    // todo: check new layers, rows, cols
    u_image_kill(&L.image);
    const SaveImage *img = data;
    L.image = u_image_new_empty(img->cols, img->rows, img->layers);
    assume(sizeof(SaveImage) + u_image_data_size(L.image) == size, "invalid data + size pair");
    memcpy(L.image.data, img->data, u_image_data_size(L.image));
    u_image_copy(L.prev_image, L.image);
    u_image_save_file(canvas_image(), canvas.default_image_file);
}



//
// public
//

void canvas_init(int cols, int rows, int layers, int grid_cols, int grid_rows) {
    assume(layers <= MAX_LAYERS, "too many layers");
    canvas.alpha = 1.0;

    L.save_id = savestate_register(save_state, load_state);

    L.pose = mat4_eye();
    L.mvp = mat4_eye();


    L.image = u_image_new_zeros(cols, rows, layers);
    canvas.current_layer = layers>=2? 1 : 0;

    init_render_objects();

    L.grid = ro_single_new(canvascam.gl,
                     r_texture_new_file(1, 1, "res/canvas_grid.png"));
    u_pose_set_size(&L.grid.rect.uv, cols, rows);


    L.selection_border = ro_batch_new(2 * (rows + cols) * SELECTION_BORDER_FACTOR, canvascam.gl,
                    r_texture_new_file(2, 2, "res/selection_border.png"));
    for (int i = 0; i < L.selection_border.num; i++) {
        L.selection_border.rects[i].color = u_color_to_vec4(u_color_from_hex("#357985"));
    }


    uColor_s buf[4];
    buf[0] = buf[3] = u_color_from_hex("#999999");
    buf[1] = buf[2] = u_color_from_hex("#777777");
    rTexture bg_tex = r_texture_new(2, 2, 1, 1, buf);
    L.bg = ro_single_new(canvascam.gl, bg_tex);
    {
        float w = (float) cols / (2 * grid_cols);
        float h = (float) rows / (2 * grid_rows);
        u_pose_set_size(&L.bg.rect.uv, w, h);
    }

    uImage img = u_image_new_file(layers, canvas.default_image_file);
    if (u_image_valid(img)) {
        u_image_copy(L.image, img);
        u_image_kill(&img);
    }

    L.prev_image = u_image_new_clone(L.image);
}

void canvas_update(float dtime) {
    float w, h;
    if (L.image.rows < L.image.cols) {
        w = 160;
        h = 160.0f * L.image.rows / L.image.cols;
    } else {
        h = 160;
        w = 160.0f * L.image.cols / L.image.rows;
    }

    float x = 0, y = 0;
    if (canvascam_is_portrait_mode()) {
        y = 45;
    } else
        x = -45;

    u_pose_set(&L.pose, x, y, w, h, 0);

    L.mvp = mat4_mul_mat(Mat4(canvascam.gl), L.pose);

    for (int layer = 0; layer <= canvas.current_layer; layer++) {
        for (int r = 0; r < L.image.rows; r++) {
            for (int c = 0; c < L.image.cols; c++) {
                set_pixel_tile(layer, c, r);
            }
        }

        for (int i = 0; i < tiles.size; i++) {
            ro_batch_update(&L.tiles[layer][i]);
        }
    }

    L.grid.rect.pose = L.pose;
    L.bg.rect.pose = L.pose;

    setup_selection();
}

void canvas_render() {
    ro_single_render(&L.bg);

    for (int layer = 0; layer <= canvas.current_layer; layer++) {
        for (int i = 0; i < tiles.size; i++) {
            ro_batch_render(&L.tiles[layer][i]);
        }
    }

    if (canvas.show_grid)
        ro_single_render(&L.grid);

    if (selection_active())
        ro_batch_render(&L.selection_border);
}


mat4 canvas_pose() {
    return L.pose;
}

uImage canvas_image() {
    return L.image;
}


ivec2 canvas_get_cr(vec4 pointer_pos) {
    mat4 pose_inv = mat4_inv(L.pose);
    vec4 pose_pos = mat4_mul_vec(pose_inv, pointer_pos);

    ivec2 cr;
    cr.x = (pose_pos.x + 0.5) * L.image.cols;
    cr.y = (0.5 - pose_pos.y) * L.image.rows;
    return cr;
}

void canvas_clear() {
    for (int r = 0; r < L.image.rows; r++) {
        for (int c = 0; c < L.image.cols; c++) {
            if (!selection_contains(c, r))
                continue;

            *u_image_pixel(L.image, c, r, canvas.current_layer) = U_COLOR_TRANSPARENT;
        }
    }
    canvas_save();
}

void canvas_save() {
    if (!u_image_equals(L.image, L.prev_image)) {
        u_image_copy(L.prev_image, L.image);
        savestate_save();
        u_image_save_file(canvas_image(), canvas.default_image_file);
    }
}

void canvas_redo_image() {
    u_image_copy(L.image, L.prev_image);
}

