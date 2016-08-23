#ifndef TEXTURE_ATLAS_H
#define TEXTURE_ATLAS_H

#include "vita2d.h"
#include "bin_packing_2d.h"
#include "int_htab.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct texture_atlas_entry_data {
	int bitmap_left;
	int bitmap_top;
	int advance_x;
	int advance_y;
	int glyph_size;
} texture_atlas_entry_data;

typedef struct texture_atlas_htab_entry {
	bp2d_rectangle rect;
	texture_atlas_entry_data data;
} atlas_htab_entry;

typedef struct texture_atlas {
	vita2d_texture *texture;
	bp2d_node *bp_root;
	int_htab *htab;
} texture_atlas;

texture_atlas *texture_atlas_create(int width, int height, SceGxmTextureFormat format);
void texture_atlas_free(texture_atlas *atlas);
int texture_atlas_insert(texture_atlas *atlas, unsigned int character,
			 const bp2d_size *size,
			 const texture_atlas_entry_data *data,
			 bp2d_position *inserted_pos);

int texture_atlas_exists(texture_atlas *atlas, unsigned int character);
int texture_atlas_get(texture_atlas *atlas, unsigned int character,
		      bp2d_rectangle *rect, texture_atlas_entry_data *data);


#ifdef __cplusplus
}
#endif

#endif
