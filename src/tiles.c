#include "utilc/assume.h"
#include "r/texture.h"
#include "io.h"
#include "tiles.h"


struct TilesGlobals_s tiles;

void tiles_init() {
	int tile_id = 1;
	tiles.size = 0;
    for(;;) {
    	char file[128];
    	sprintf(file, "tiles/tile_%02i.png", tile_id);
    	
	    Image *img = io_load_image(file, 2);
	    if(!img)
	        break;
	        
	    assume(img->cols == TILES_COLS * TILES_SIZE 
	    && img->rows == TILES_ROWS * TILES_SIZE,
	    "wrong tiles size");
	    
	    GLuint tex = r_texture_init(img->cols, img->rows, image_layer(img, 0));
	    
	    
	    tiles.imgs[tiles.size] = img;
	    tiles.textures[tiles.size] = tex;
	    tiles.ids[tiles.size] = tile_id;
	    
	    tile_id++;
	    tiles.size++;
    }
    SDL_Log("tiles loaded: %i", tiles.size);
    if(tiles.size == 0)
        SDL_Log("WARNING: 0 tiles loaded! Put some into tiles/tile_xx.png, starting with xx=01");
}
