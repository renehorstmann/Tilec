#ifndef TILEC_IO_H
#define TILEC_IO_H

#include <stdbool.h>
#include "image.h"

struct IoGlobals_s {
	const char *default_image_file;
	const char *default_import_file;
};
extern struct IoGlobals_s io;

Image *io_load_image(const char *file, int layers);

bool io_save_image(const char *file, const Image *image);

#endif //TILEC_IO_H
