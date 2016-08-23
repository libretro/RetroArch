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

#define ATLAS_DEFAULT_W 512
#define ATLAS_DEFAULT_H 512

typedef enum {
	VITA2D_LOAD_FONT_FROM_FILE,
	VITA2D_LOAD_FONT_FROM_MEM
} vita2d_load_font_from;

typedef struct vita2d_font {
	vita2d_load_font_from load_from;
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
	texture_atlas *atlas;
} vita2d_font;

static FT_Error ftc_face_requester(FTC_FaceID face_id, FT_Library library,
				   FT_Pointer request_data, FT_Face *face)
{
	vita2d_font *font = (vita2d_font *)face_id;

	FT_Error error = FT_Err_Cannot_Open_Resource;

	if (font->load_from == VITA2D_LOAD_FONT_FROM_FILE) {
		error = FT_New_Face(
				font->ftlibrary,
				font->filename,
				0,
				face);
	} else if (font->load_from == VITA2D_LOAD_FONT_FROM_MEM) {
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
		FT_Done_FreeType(font->ftlibrary);
		free(font);
		return NULL;
	}

	font->filename = strdup(filename);

	FTC_CMapCache_New(font->ftcmanager, &font->cmapcache);
	FTC_ImageCache_New(font->ftcmanager, &font->imagecache);

	font->load_from = VITA2D_LOAD_FONT_FROM_FILE;

	font->atlas = texture_atlas_create(ATLAS_DEFAULT_W, ATLAS_DEFAULT_H,
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
		FT_Done_FreeType(font->ftlibrary);
		free(font);
		return NULL;
	}

	font->font_buffer = buffer;
	font->buffer_size = size;

	FTC_CMapCache_New(font->ftcmanager, &font->cmapcache);
	FTC_ImageCache_New(font->ftcmanager, &font->imagecache);

	font->load_from = VITA2D_LOAD_FONT_FROM_MEM;

	font->atlas = texture_atlas_create(ATLAS_DEFAULT_W, ATLAS_DEFAULT_H,
		SCE_GXM_TEXTURE_FORMAT_U8_R111);

	return font;
}

void vita2d_free_font(vita2d_font *font)
{
	if (font) {
		FTC_FaceID face_id = (FTC_FaceID)font;
		FTC_Manager_RemoveFaceID(font->ftcmanager, face_id);
		FTC_Manager_Done(font->ftcmanager);
		if (font->load_from == VITA2D_LOAD_FONT_FROM_FILE) {
			free(font->filename);
		}
		texture_atlas_free(font->atlas);
		free(font);
	}
}

static int atlas_add_glyph(texture_atlas *atlas, unsigned int glyph_index,
			   const FT_BitmapGlyph bitmap_glyph, int glyph_size)
{
	int ret;
	int i, j;
	bp2d_position position;
	void *texture_data;
	unsigned int tex_width;
	const FT_Bitmap *bitmap = &bitmap_glyph->bitmap;
	unsigned int w = bitmap->width;
	unsigned int h = bitmap->rows;
	unsigned char buffer[w * h];

	bp2d_size size = {
		bitmap->width,
		bitmap->rows
	};

	texture_atlas_entry_data data = {
		bitmap_glyph->left,
		bitmap_glyph->top,
		bitmap_glyph->root.advance.x,
		bitmap_glyph->root.advance.y,
		glyph_size
	};

	ret = texture_atlas_insert(atlas, glyph_index, &size, &data,
				  &position);
	if (!ret)
		return 0;

	for (i = 0; i < h; i++) {
		for (j = 0; j < w; j++) {
			if (bitmap->pixel_mode == FT_PIXEL_MODE_MONO) {
				buffer[i*w + j] =
					(bitmap->buffer[i*bitmap->pitch + j/8] & (1 << (7 - j%8)))
					? 0xFF : 0;
			} else if (bitmap->pixel_mode == FT_PIXEL_MODE_GRAY) {
				buffer[i*w + j] = bitmap->buffer[i*bitmap->pitch + j];
			}
		}
	}

	texture_data = vita2d_texture_get_datap(atlas->texture);
	tex_width = vita2d_texture_get_width(atlas->texture);

	for (i = 0; i < size.h; i++) {
		memcpy(texture_data + (position.x + (position.y + i) * tex_width),
		       buffer + i * size.w, size.w);
	}

	return 1;
}

static int generic_font_draw_text(vita2d_font *font, int draw,
				   int *height, int x, int y,
				   unsigned int color,
				   unsigned int size,
				   const char *text)
{
	const FT_ULong flags = FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL;
	FT_Face face;
	FT_Int charmap_index;
	FT_Glyph glyph;
	FT_UInt glyph_index;
	FT_Bool use_kerning;
	FTC_FaceID face_id = (FTC_FaceID)font;
	FT_UInt previous = 0;
	vita2d_texture *tex = font->atlas->texture;

	unsigned int character;
	int start_x = x;
	int max_x = 0;
	int pen_x = x;
	int pen_y = y;
	bp2d_rectangle rect;
	texture_atlas_entry_data data;

	FTC_ScalerRec scaler;
	scaler.face_id = face_id;
	scaler.width = size;
	scaler.height = size;
	scaler.pixel = 1;

	FTC_Manager_LookupFace(font->ftcmanager, face_id, &face);
	use_kerning = FT_HAS_KERNING(face);
	charmap_index = FT_Get_Charmap_Index(face->charmap);

	while (text[0]) {
		character = utf8_character(&text);

		if (character == '\n') {
			if (pen_x > max_x)
				max_x = pen_x;
			pen_x = start_x;
			pen_y += size;
			continue;
		}

		glyph_index = FTC_CMapCache_Lookup(font->cmapcache,
						   (FTC_FaceID)font,
						   charmap_index,
						   character);

		if (use_kerning && previous && glyph_index) {
			FT_Vector delta;
			FT_Get_Kerning(face, previous, glyph_index,
				       FT_KERNING_DEFAULT, &delta);
			pen_x += delta.x >> 6;
		}

		if (!texture_atlas_get(font->atlas, glyph_index, &rect, &data)) {
			FTC_ImageCache_LookupScaler(font->imagecache,
						    &scaler,
						    flags,
						    glyph_index,
						    &glyph,
						    NULL);

			if (!atlas_add_glyph(font->atlas, glyph_index,
					     (FT_BitmapGlyph)glyph, size)) {
				continue;
			}

			if (!texture_atlas_get(font->atlas, glyph_index, &rect, &data))
				continue;
		}

		const float draw_scale = size / (float)data.glyph_size;

		if (draw) {
			vita2d_draw_texture_tint_part_scale(tex,
				pen_x + data.bitmap_left * draw_scale,
				pen_y - data.bitmap_top * draw_scale,
				rect.x, rect.y, rect.w, rect.h,
				draw_scale,
				draw_scale,
				color);
		}

		pen_x += (data.advance_x >> 16) * draw_scale;
	}

	if (pen_x > max_x)
		max_x = pen_x;

	if (height)
		*height = pen_y + size - y;

	return max_x - x;
}

int vita2d_font_draw_text(vita2d_font *font, int x, int y, unsigned int color,
			   unsigned int size, const char *text)
{
	return generic_font_draw_text(font, 1, NULL, x, y, color, size, text);
}

int vita2d_font_draw_textf(vita2d_font *font, int x, int y, unsigned int color,
			   unsigned int size, const char *text, ...)
{
	char buf[1024];
	va_list argptr;

	va_start(argptr, text);
	vsnprintf(buf, sizeof(buf), text, argptr);
	va_end(argptr);

	return vita2d_font_draw_text(font, x, y, color, size, buf);
}

void vita2d_font_text_dimensions(vita2d_font *font, unsigned int size,
				 const char *text, int *width, int *height)
{
	int w;
	w = generic_font_draw_text(font, 0, height, 0, 0, 0, size, text);

	if (width)
		*width = w;
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
