#include <assert.h>
#include <float.h>
#include "r/ro_single.h"
#include "r/ro_batch.h"
#include "r/texture.h"
#include "u/pose.h"
#include "utilc/alloc.h"
#include "mathc/mat/float.h"

#include "image.h"
#include "canvas_camera.h"
#include "io.h"
#include "palette.h"
#include "toolbar.h"
#include "selection.h"
#include "savestate.h"
#include "canvas.h"

#define MAX_LAYERS 16

#define TILE_COLS 8
#define TILE_ROWS 8
#define MAX_TILE_SETS 256

struct CanvasGlobals_s canvas;

static struct {
    mat4 pose;
    mat4 mvp;

    Image *image;
    Image *last_image;
    
    rRoSingle bg;
    rRoSingle grid;
    
    rRoBatch selection_border;
    
    rRoBatch tiles[MAX_LAYERS][MAX_TILE_SETS];
    GLuint tiles_textures[MAX_TILE_SETS];
    int tiles_size;
    
    int save_id;
} L;

static void init_tile_ro(rRoBatch *ro, GLuint tex) {
	float w = 1.0 / L.image->cols;
	float h = 1.0 / L.image->rows;
	r_ro_batch_init(ro, L.image->cols * L.image->rows, &L.mvp.m00, tex);
	for(int r=0; r<L.image->rows; r++) {
		for(int c=0; c<L.image->cols; c++) {
			u_pose_set_size(&ro->rects[r*L.image->cols + c].uv, 1.0/TILE_COLS, 1.0/TILE_ROWS);
			ro->rects[r*L.image->cols + c].pose = u_pose_new_aa(-0.5+c*w, 0.5-r*h, w, h);
		}
	}
}

static void init_render_objects() {
    for(int layer=0; layer<L.image->layers; layer++) {
        GLuint tex = r_texture_init(L.image->cols, L.image->rows, image_layer(L.image, layer));
        
        for(int i=0; i<L.tiles_size; i++) {
            init_tile_ro(&L.tiles[layer][i], L.tiles_textures[i]);
        }
    }
}

static mat4 pixel_pose(int x, int y) {
	float w = u_pose_get_w(L.pose);
	float size = w/L.image->cols;
	
	
	float pos_x = u_pose_aa_get_left(L.pose) + (x + 0.5) * size;    
	float pos_y = u_pose_aa_get_top(L.pose) - (y + 0.5) * size;
	
	mat4 pose = mat4_eye();
	u_pose_set(&pose, pos_x, pos_y, size, size, 0);
	return pose;
}

static void setup_selection() {
	int x = selection_pos().x;
	int y = selection_pos().y;
	int w = selection_size().x;
	int h = selection_size().y;
	
	int idx = 0;
	for(int i=0;i<h;i++) {
		L.selection_border.rects[idx].pose = pixel_pose(x-1, y+i);
		u_pose_set(&L.selection_border.rects[idx].uv, 0, 0, 0.5, 0.5, 0);
		idx++;
		
		L.selection_border.rects[idx].pose = pixel_pose(x+w, y+i);
		u_pose_set(&L.selection_border.rects[idx].uv, 0, 0.5, 0.5, 0.5, 0);
		idx++;
	}
	for(int i=0;i<w;i++) {
		L.selection_border.rects[idx].pose = pixel_pose(x+i, y-1);
		u_pose_set(&L.selection_border.rects[idx].uv, 0.5, 0, 0.5, 0.5, 0);
		idx++;
		
		L.selection_border.rects[idx].pose = pixel_pose(x+i, y+h);
		u_pose_set(&L.selection_border.rects[idx].uv, 0.5, 0.5, 0.5, 0.5, 0);
		idx++;
	}
	
	for(;idx < L.selection_border.num; idx++) {
		u_pose_set(&L.selection_border.rects[idx].pose, FLT_MAX, FLT_MAX, 0, 0, 0);
	}
	
	r_ro_batch_update(&L.selection_border);
}


static void set_pixel_tile(int layer, int c, int r) {
	
	for(int i=0; i<L.tiles_size; i++) {
    	int idx = r * L.image->cols + c;
        L.tiles[layer][i].rects[idx].color.a = 0;
    }
    
	Color_s code = *image_pixel(L.image, layer, c, r);
	
    int tile_id = code.b;
    
    if(layer <= canvas.current_layer && tile_id>0 && tile_id<=L.tiles_size) {
    	tile_id--;
    	
    	vec4 color = (vec4) {{1, 1, 1, (float) layer/canvas.current_layer}};
    	
    	float tile_x = (float) (code.a%TILE_COLS) / TILE_COLS;
        float tile_y = (float) (code.a/TILE_COLS) / TILE_ROWS;
         
        int idx = r * L.image->cols + c;
        u_pose_set_xy(&L.tiles[layer][tile_id].rects[idx].uv, tile_x, tile_y);
        
        float alpha = (layer+1.0) / (canvas.current_layer + 1.0);
        L.tiles[layer][tile_id].rects[idx].color.a = alpha;
    }
    
    
}


static void save_state(void **data, size_t *size);
static void load_state(const void *data, size_t size);


void canvas_init(int cols, int rows, int layers, int grid_cols, int grid_rows) {
    
    L.save_id = savestate_register(save_state, load_state);
     
    L.pose = mat4_eye();
    L.mvp = mat4_eye();
    
    // load tiles:
    int tile_id = 0;
    do {
    	char file[128];
	    sprintf(file, "tiles/tile_%02i.png", tile_id+1);
	    L.tiles_textures[tile_id] = r_texture_init_file(file, NULL);
	    tile_id++;
    } while(L.tiles_textures[tile_id-1]);
    L.tiles_size = tile_id - 1;
    SDL_Log("Canvas loaded %i tiles", L.tiles_size);
    

    L.image = image_new_zeros(layers, cols, rows);
    canvas.current_layer = 0;

    init_render_objects();
    
    r_ro_single_init(&L.grid, canvas_camera.gl, 
            r_texture_init_file("res/canvas_grid.png", NULL));
    u_pose_set_size(&L.grid.rect.uv, cols, rows);


    r_ro_batch_init(&L.selection_border, 2*(rows+cols), canvas_camera.gl,
            r_texture_init_file("res/selection_border.png", NULL));
    for(int i=0; i<L.selection_border.num; i++) {
        L.selection_border.rects[i].color = color_to_vec4(color_from_hex("#357985"));       
    }
            

    Color_s buf[4];
    buf[0] = buf[3] = color_from_hex("#999999");
    buf[1] = buf[2] = color_from_hex("#777777");
    GLuint bg_tex = r_texture_init(2, 2, buf);
    r_texture_filter_nearest(bg_tex);
    r_ro_single_init(&L.bg, canvas_camera.gl, bg_tex);
    {    
        float w = (float) cols/(2*grid_cols);
        float h = (float) rows/(2*grid_rows);
        u_pose_set_size(&L.bg.rect.uv, w, h);
    }

    Image *img = io_load_image(io.default_image_file, layers);
    if(img) {
        image_copy(L.image, img);
        image_delete(img);
    }
    
    L.last_image = image_new_clone(L.image);
}

void canvas_update(float dtime) {
    float w, h;
    if(L.image->rows < L.image->cols) {
        w = 160;
        h = 160.0f * L.image->rows / L.image->cols;      } else {
        h = 160;
        w = 160.0f * L.image->cols / L.image->rows;
    }
    
    float x = 0, y = 0;
    if(canvas_camera_is_portrait_mode()) {
        y = 30;
    }
    
    u_pose_set(&L.pose, x, y, w, h, 0);
    
    L.mvp = mat4_mul_mat(Mat4(canvas_camera.gl), L.pose);

    for(int layer=0; layer<=canvas.current_layer; layer++) {
        for(int r=0; r<L.image->rows; r++) {
        	for(int c=0; c<L.image->cols; c++) {
        		set_pixel_tile(layer, c, r);
        	}
        }
        
        for(int i=0; i<L.tiles_size; i++) {
        	r_ro_batch_update(&L.tiles[layer][i]);
        }
    }

    L.grid.rect.pose = L.pose;
    L.bg.rect.pose = L.pose;
    
    setup_selection();
}

void canvas_render() {
    r_ro_single_render(&L.bg);

    for(int layer=0; layer<=canvas.current_layer; layer++) {
        for(int i=0; i<L.tiles_size; i++) {
        	r_ro_batch_render(&L.tiles[layer][i]);
        }
    }

    if(canvas.show_grid)
        r_ro_single_render(&L.grid);
        
    if(selection_active())
        r_ro_batch_render(&L.selection_border);
}


mat4 canvas_pose() {
    return L.pose;
}

Image *canvas_image() {
    return L.image;
}


ivec2 canvas_get_cr(vec4 pointer_pos) {
    mat4 pose_inv = mat4_inv(L.pose);
    vec4 pose_pos = mat4_mul_vec(pose_inv, pointer_pos);

    ivec2 cr;
    cr.x = (pose_pos.x + 0.5) * canvas_image()->cols;    
    cr.y = (0.5 - pose_pos.y) * canvas_image()->rows;
    return cr;
}

void canvas_clear() {
    for(int r=0; r<L.image->rows; r++) {
        for(int c=0; c<L.image->cols; c++) {
        	if(!selection_contains(c, r))
        	    continue;
        	
            *image_pixel(L.image, canvas.current_layer, c, r) = COLOR_TRANSPARENT;
        }
    }
    canvas_save();
}

void canvas_save() {
	if(!image_equals(L.image, L.last_image)) {
	    image_copy(L.last_image, L.image);
	    savestate_save();
        io_save_image(io.default_image_file, canvas_image());
	}
}

void canvas_redo_image() {
	image_copy(L.image, L.last_image);
}


static void save_state(void **data, size_t *size) {
	*data = L.image;
	*size = image_full_size(L.image);
}
static void load_state(const void *data, size_t size) {
	// todo: check new layers, rows, cols
	image_delete(L.image);
	L.image = image_new_clone(data);
	image_copy(L.last_image, L.image);
	assert(image_full_size(L.image) == size);
	io_save_image(io.default_image_file, canvas_image());
}

