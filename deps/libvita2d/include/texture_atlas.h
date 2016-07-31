#ifndef TEXTURE_ATLAS_H
#define TEXTURE_ATLAS_H

#include "vita2d.h"
#include "bin_packing_2d.h"
#include "int_htab.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct atlas_htab_entry {
	bp2d_rectangle rect;
	int bitmap_left;
	int bitmap_top;
	int advance_x;
	int advance_y;
	int glyph_size;
} atlas_htab_entry;

typedef struct texture_atlas {
	vita2d_texture *tex;
	bp2d_node *bp_root;
	int_htab *htab;
} texture_atlas;

texture_atlas *texture_atlas_create(int width, int height, SceGxmTextureFormat format);
void texture_atlas_free(texture_atlas *atlas);
int texture_atlas_insert(texture_atlas *atlas, unsigned int character, int width, int height, int bitmap_left, int bitmap_top, int advance_x, int advance_y, int glyph_size, int *inserted_x, int *inserted_y);
int texture_atlas_insert_draw(texture_atlas *atlas, unsigned int character, const void *image, int width, int height, int bitmap_left, int bitmap_top, int advance_x, int advance_y, int glyph_size);
int texture_atlas_exists(texture_atlas *atlas, unsigned int character);
int texture_atlas_get(texture_atlas *atlas, unsigned int character, bp2d_rectangle *rect, int *bitmap_left, int *bitmap_top, int *advance_x, int *advance_y, int *glyph_size);

#ifdef __cplusplus
}
#endif

#endif
