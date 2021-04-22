#include "r/texture.h"
#include "rhc/error.h"
#include "tiles.h"


struct TilesGlobals_s tiles;

void tiles_init() {
    int tile_id = 1;
    tiles.size = 0;
    for (;;) {
        char file[128];
        sprintf(file, "tiles/tile_%02i.png", tile_id);

        uImage img = u_image_new_file(2, file);
        if (!u_image_valid(img))
            break;

        assume(img.cols == TILES_COLS * TILES_SIZE
               && img.rows == TILES_ROWS * TILES_SIZE,
               "wrong tiles size");

        rTexture tex = r_texture_new(img.cols, img.rows, TILES_COLS, TILES_ROWS, u_image_layer(img, 0));


        tiles.imgs[tiles.size] = img;
        tiles.textures[tiles.size] = tex;
        tiles.ids[tiles.size] = tile_id;

        tile_id++;
        tiles.size++;
    }
    log_info("tiles: loaded %i", tiles.size);
    if (tiles.size == 0)
        log_error("tiles: WARNING: 0 tiles loaded! Put some into tiles/tile_xx.png, starting with xx=01");
}
