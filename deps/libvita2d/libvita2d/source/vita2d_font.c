#include <psp2/kernel/sysmem.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <freetype2/ft2build.h>
#include FT_CACHE_H
#include FT_FREETYPE_H
#include "vita2d.h"
#include "texture_atlas.h"
#include "bin_packing_2d.h"
#include "utils.h"
#include "shared.h"

#define ATLAS_DEFAULT_W 256
#define ATLAS_DEFAULT_H 256

typedef enum {
	VITA2D_LOAD_FROM_FILE,
	VITA2D_LOAD_FROM_MEM
} vita2d_font_load_from;

typedef struct vita2d_font {
	vita2d_font_load_from load_from;
	union {
		char *filename;
		struct {
			const void *font_buffer;
			unsigned int buffer_size;
		};
	};
	FT_Library ftlibrary;
	FTC_Manager ftcmanager;
	FTC_CMapCache cmapcache;
	FTC_ImageCache imagecache;
	texture_atlas *tex_atlas;
} vita2d_font;

static FT_Error ftc_face_requester(FTC_FaceID face_id, FT_Library library, FT_Pointer request_data, FT_Face *face)
{
	vita2d_font *font = (vita2d_font *)face_id;

	FT_Error error = FT_Err_Cannot_Open_Resource;

	if (font->load_from == VITA2D_LOAD_FROM_FILE) {
		error = FT_New_Face(
				font->ftlibrary,
				font->filename,
				0,
				face);
	} else if (font->load_from == VITA2D_LOAD_FROM_MEM) {
		error = FT_New_Memory_Face(
				font->ftlibrary,
				font->font_buffer,
				font->buffer_size,
				0,
				face);
	}
	return error;
}

vita2d_font *vita2d_load_font_file(const char *filename)
{
	FT_Error error;

	vita2d_font *font = malloc(sizeof(*font));
	if (!font)
		return NULL;

	error = FT_Init_FreeType(&font->ftlibrary);
	if (error != FT_Err_Ok) {
		free(font);
		return NULL;
	}

	error = FTC_Manager_New(
		font->ftlibrary,
		0,  /* use default */
		0,  /* use default */
		0,  /* use default */
		&ftc_face_requester,  /* use our requester */
		NULL,                 /* user data  */
		&font->ftcmanager);

	if (error != FT_Err_Ok) {
		free(font);
		FT_Done_FreeType(font->ftlibrary);
		return NULL;
	}

	size_t len = strlen(filename);
	font->filename = malloc(len + 1);
	strcpy(font->filename, filename);

	FTC_CMapCache_New(font->ftcmanager, &font->cmapcache);
	FTC_ImageCache_New(font->ftcmanager, &font->imagecache);

	font->load_from = VITA2D_LOAD_FROM_FILE;

	font->tex_atlas = texture_atlas_create(ATLAS_DEFAULT_W, ATLAS_DEFAULT_H,
		SCE_GXM_TEXTURE_FORMAT_U8_R111);

	return font;
}

vita2d_font *vita2d_load_font_mem(const void *buffer, unsigned int size)
{
	FT_Error error;

	vita2d_font *font = malloc(sizeof(*font));
	if (!font)
		return NULL;

	error = FT_Init_FreeType(&font->ftlibrary);
	if (error != FT_Err_Ok) {
		free(font);
		return NULL;
	}

	error = FTC_Manager_New(
		font->ftlibrary,
		0,  /* use default */
		0,  /* use default */
		0,  /* use default */
		&ftc_face_requester,  /* use our requester */
		NULL,                 /* user data  */
		&font->ftcmanager);

	if (error != FT_Err_Ok) {
		free(font);
		FT_Done_FreeType(font->ftlibrary);
		return NULL;
	}

	font->font_buffer = buffer;
	font->buffer_size = size;

	FTC_CMapCache_New(font->ftcmanager, &font->cmapcache);
	FTC_ImageCache_New(font->ftcmanager, &font->imagecache);

	font->load_from = VITA2D_LOAD_FROM_MEM;

	font->tex_atlas = texture_atlas_create(ATLAS_DEFAULT_W, ATLAS_DEFAULT_H,
		SCE_GXM_TEXTURE_FORMAT_U8_R111);

	return font;
}

void vita2d_free_font(vita2d_font *font)
{
	if (font) {
		FTC_FaceID face_id = (FTC_FaceID)font;
		FTC_Manager_RemoveFaceID(font->ftcmanager, face_id);
		FTC_Manager_Done(font->ftcmanager);
		if (font->load_from == VITA2D_LOAD_FROM_FILE) {
			free(font->filename);
		}
		texture_atlas_free(font->tex_atlas);
		free(font);
	}
}

static int atlas_add_glyph(texture_atlas *atlas, unsigned int glyph_index, const FT_BitmapGlyph bitmap_glyph, int glyph_size)
{
	const FT_Bitmap *bitmap = &bitmap_glyph->bitmap;

	unsigned char *buffer = malloc(bitmap->width * bitmap->rows);
	unsigned int w = bitmap->width;
	unsigned int h = bitmap->rows;

	int j, k;
	for (j = 0; j < h; j++) {
		for (k = 0; k < w; k++) {
			if (bitmap->pixel_mode == FT_PIXEL_MODE_MONO) {
				buffer[j*w + k] =
					(bitmap->buffer[j*bitmap->pitch + k/8] & (1 << (7 - k%8)))
					? 0xFF : 0;
			} else {
				buffer[j*w + k] = bitmap->buffer[j*bitmap->pitch + k];
			}
		}
	}

	int ret = texture_atlas_insert_draw(atlas, glyph_index, buffer,
		bitmap->width, bitmap->rows,
		bitmap_glyph->left, bitmap_glyph->top,
		bitmap_glyph->root.advance.x, bitmap_glyph->root.advance.y,
		glyph_size);

	free(buffer);

	return ret;
}

int vita2d_font_draw_text(vita2d_font *font, int x, int y, unsigned int color, unsigned int size, const char *text)
{
	FTC_FaceID face_id = (FTC_FaceID)font;
	FT_Face face;
	FTC_Manager_LookupFace(font->ftcmanager, face_id, &face);

	FT_Int charmap_index;
	charmap_index = FT_Get_Charmap_Index(face->charmap);

	FT_Glyph glyph;
	FT_Bool use_kerning = FT_HAS_KERNING(face);
	FT_UInt glyph_index, previous = 0;
	int pen_x = x;
	int pen_y = y + size;

	FTC_ScalerRec scaler;
	scaler.face_id = face_id;
	scaler.width = size;
	scaler.height = size;
	scaler.pixel = 1;

	FT_ULong flags = FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL;

	int max = 0;

	while (*text) {

		if (*text == '\n') {
			if ((pen_x - x) > max)
				max = pen_x-x;
			pen_x = x;
			pen_y += size;
			text++;
			continue;
		}

		glyph_index = FTC_CMapCache_Lookup(font->cmapcache, (FTC_FaceID)font, charmap_index, *text);

		if (use_kerning && previous && glyph_index) {
			FT_Vector delta;
			FT_Get_Kerning(face, previous, glyph_index, FT_KERNING_DEFAULT, &delta);
			pen_x += delta.x >> 6;
		}

		if (!texture_atlas_exists(font->tex_atlas, glyph_index)) {
			FTC_ImageCache_LookupScaler(font->imagecache, &scaler, flags, glyph_index, &glyph, NULL);

			if (!atlas_add_glyph(font->tex_atlas, glyph_index, (FT_BitmapGlyph)glyph, size)) {
				continue;
			}
		}

		bp2d_rectangle rect;
		int bitmap_left, bitmap_top;
		int advance_x, advance_y;
		int glyph_size;

		if (!texture_atlas_get(font->tex_atlas, glyph_index,
			&rect, &bitmap_left, &bitmap_top,
			&advance_x, &advance_y, &glyph_size))
				continue;

		const float draw_scale = size/(float)glyph_size;

		vita2d_draw_texture_tint_part_scale(font->tex_atlas->tex,
			pen_x + bitmap_left * draw_scale,
			pen_y - bitmap_top * draw_scale,
			rect.x, rect.y, rect.w, rect.h,
			draw_scale,
			draw_scale,
			color);

		pen_x += (advance_x >> 16) * draw_scale;
		pen_y += (advance_y >> 16) * draw_scale;

		previous = glyph_index;
		text++;
	}
	if ((pen_x - x) > max)
		max = pen_x-x;

	return max;
}

void vita2d_font_draw_textf(vita2d_font *font, int x, int y, unsigned int color, unsigned int size, const char *text, ...)
{
	char buf[1024];
	va_list argptr;
	va_start(argptr, text);
	vsnprintf(buf, sizeof(buf), text, argptr);
	va_end(argptr);
	vita2d_font_draw_text(font, x, y, color, size, buf);
}

void vita2d_font_text_dimensions(vita2d_font *font, unsigned int size, const char *text, int *width, int *height)
{
	FTC_FaceID face_id = (FTC_FaceID)font;
	FT_Face face;
	FTC_Manager_LookupFace(font->ftcmanager, face_id, &face);

	FT_Int charmap_index;
	charmap_index = FT_Get_Charmap_Index(face->charmap);

	FT_Glyph glyph;
	FT_Bool use_kerning = FT_HAS_KERNING(face);
	FT_UInt glyph_index, previous = 0;

	int pen_x = 0;
	int pen_y = size >> 1;

	FTC_ScalerRec scaler;
	scaler.face_id = face_id;
	scaler.width = size;
	scaler.height = size;
	scaler.pixel = 1;

	FT_ULong flags = FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL;

	int max = 0;

	while (*text) {
		if (*text == '\n') {
			if (pen_x > max)
				max = pen_x;
			pen_x = 0;
			pen_y += size;
			text++;
			continue;
		}
		glyph_index = FTC_CMapCache_Lookup(font->cmapcache, (FTC_FaceID)font, charmap_index, *text);

		if (use_kerning && previous && glyph_index) {
			FT_Vector delta;
			FT_Get_Kerning(face, previous, glyph_index, FT_KERNING_DEFAULT, &delta);
			pen_x += delta.x >> 6;
		}

		FTC_ImageCache_LookupScaler(font->imagecache, &scaler, flags, glyph_index, &glyph, NULL);

		const FT_BitmapGlyph bitmap_glyph = (FT_BitmapGlyph)glyph;

		pen_x += bitmap_glyph->root.advance.x >> 16;
		//pen_y += bitmap_glyph->root.advance.y >> 16;

		previous = glyph_index;
		text++;
	}
	if(pen_x > max)
		max = pen_x;
	if (width)
		*width = max;
	if (height)
		*height = size + pen_y;
}

int vita2d_font_text_width(vita2d_font *font, unsigned int size, const char *text)
{
	int width;
	vita2d_font_text_dimensions(font, size, text, &width, NULL);
	return width;
}

int vita2d_font_text_height(vita2d_font *font, unsigned int size, const char *text)
{
	int height;
	vita2d_font_text_dimensions(font, size, text, NULL, &height);
	return height;
}
