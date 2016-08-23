#include <psp2/pgf.h>
#include <psp2/kernel/sysmem.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>
#include <math.h>
#include "vita2d.h"
#include "texture_atlas.h"
#include "bin_packing_2d.h"
#include "utils.h"
#include "shared.h"

#define ATLAS_DEFAULT_W 512
#define ATLAS_DEFAULT_H 512

typedef struct vita2d_pgf {
	SceFontLibHandle lib_handle;
	SceFontHandle font_handle;
	texture_atlas *atlas;
	float vsize;
} vita2d_pgf;

static void *pgf_alloc_func(void *userdata, unsigned int size)
{
	return memalign(sizeof(int), size + sizeof(int));
}

static void pgf_free_func(void *userdata, void *p)
{
	free(p);
}

vita2d_pgf *vita2d_load_default_pgf()
{
	unsigned int error;
	SceFontInfo fontinfo;

	vita2d_pgf *font = malloc(sizeof(*font));
	if (!font)
		return NULL;

	SceFontNewLibParams params = {
		font,
		1,
		NULL,
		pgf_alloc_func,
		pgf_free_func,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL
	};

	font->lib_handle = sceFontNewLib(&params, &error);
	if (error != 0) {
		free(font);
		return NULL;
	}

	font->font_handle = sceFontOpen(font->lib_handle, 0, 0, &error);
	if (error != 0) {
		sceFontDoneLib(font->lib_handle);
		free(font);
		return NULL;
	}

	sceFontGetFontInfo(font->font_handle, &fontinfo);
	font->vsize = (fontinfo.fontStyle.fontV / fontinfo.fontStyle.fontVRes)
		* SCREEN_DPI;

	font->atlas = texture_atlas_create(ATLAS_DEFAULT_W, ATLAS_DEFAULT_H,
		SCE_GXM_TEXTURE_FORMAT_U8_R111);

	return font;
}

void vita2d_free_pgf(vita2d_pgf *font)
{
	if (font) {
		sceFontClose(font->font_handle);
		sceFontDoneLib(font->lib_handle);
		texture_atlas_free(font->atlas);
		free(font);
	}
}

static int atlas_add_glyph_pgf(vita2d_pgf *font, unsigned int character)
{
	SceFontCharInfo char_info;
	bp2d_position position;
	void *texture_data;
	vita2d_texture *tex = font->atlas->texture;

	if (sceFontGetCharInfo(font->font_handle, character, &char_info) < 0)
		return 0;

	bp2d_size size = {
		char_info.bitmapWidth,
		char_info.bitmapHeight
	};

	texture_atlas_entry_data data = {
		char_info.bitmapLeft,
		char_info.bitmapTop,
		char_info.sfp26AdvanceH,
		char_info.sfp26AdvanceV,
		0
	};

	if (!texture_atlas_insert(font->atlas, character, &size, &data,
				  &position))
			return 0;

	texture_data = vita2d_texture_get_datap(tex);

	SceFontGlyphImage glyph_image;
	glyph_image.pixelFormat = SCE_FONT_PIXELFORMAT_8;
	glyph_image.xPos64 = position.x << 6;
	glyph_image.yPos64 = position.y << 6;
	glyph_image.bufWidth = vita2d_texture_get_width(tex);
	glyph_image.bufHeight = vita2d_texture_get_height(tex);
	glyph_image.bytesPerLine = vita2d_texture_get_stride(tex);
	glyph_image.pad = 0;
	glyph_image.bufferPtr = (unsigned int)texture_data;

	return sceFontGetCharGlyphImage(font->font_handle, character, &glyph_image) == 0;
}

int generic_pgf_draw_text(vita2d_pgf *font, int draw, int *height,
			  int x, int y, unsigned int color, float scale,
			  const char *text)
{
	unsigned int character;
	bp2d_rectangle rect;
	texture_atlas_entry_data data;
	vita2d_texture *tex = font->atlas->texture;
	int start_x = x;
	int max_x = 0;
	int pen_x = x;
	int pen_y = y;

	while (*text) {
		character = utf8_character(&text);

		if (character == '\n') {
			if (pen_x > max_x)
				max_x = pen_x;
			pen_x = start_x;
			pen_y += font->vsize * scale;
			continue;
		}

		if (!texture_atlas_get(font->atlas, character, &rect, &data)) {
			if (!atlas_add_glyph_pgf(font, character)) {
				continue;
			}

			if (!texture_atlas_get(font->atlas, character,
					       &rect, &data))
					continue;
		}

		if (draw) {
			vita2d_draw_texture_tint_part_scale(tex,
				pen_x + data.bitmap_left * scale,
				pen_y - data.bitmap_top * scale,
				rect.x, rect.y, rect.w, rect.h,
				scale,
				scale,
				color);
		}

		pen_x += (data.advance_x >> 6) * scale;
	}

	if (pen_x > max_x)
		max_x = pen_x;

	if (height)
		*height = pen_y + font->vsize * scale - y;

	return max_x - x;
}

int vita2d_pgf_draw_text(vita2d_pgf *font, int x, int y,
			 unsigned int color, float scale,
			 const char *text)
{
	return generic_pgf_draw_text(font, 1, NULL, x, y, color, scale, text);
}

int vita2d_pgf_draw_textf(vita2d_pgf *font, int x, int y,
			  unsigned int color, float scale,
			  const char *text, ...)
{
	char buf[1024];
	va_list argptr;
	va_start(argptr, text);
	vsnprintf(buf, sizeof(buf), text, argptr);
	va_end(argptr);
	return vita2d_pgf_draw_text(font, x, y, color, scale, buf);
}

void vita2d_pgf_text_dimensions(vita2d_pgf *font, float scale,
				const char *text, int *width, int *height)
{
	int w;
	w = generic_pgf_draw_text(font, 0, height, 0, 0, 0, scale, text);

	if (width)
		*width = w;
}

int vita2d_pgf_text_width(vita2d_pgf *font, float scale, const char *text)
{
	int width;
	vita2d_pgf_text_dimensions(font, scale, text, &width, NULL);
	return width;
}

int vita2d_pgf_text_height(vita2d_pgf *font, float scale, const char *text)
{
	int height;
	vita2d_pgf_text_dimensions(font, scale, text, NULL, &height);
	return height;
}
