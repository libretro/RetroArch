#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psp2/io/fcntl.h>
#include <psp2/gxm.h>
#include <jpeglib.h>
#include "vita2d.h"

static vita2d_texture *_vita2d_load_JPEG_generic(struct jpeg_decompress_struct *jinfo, struct jpeg_error_mgr *jerr)
{
	float downScaleWidth = (float)jinfo->image_width / 4096;
	float downScaleHeight = (float)jinfo->image_height / 4096;
	float downScale = (downScaleWidth >= downScaleHeight) ? downScaleWidth : downScaleHeight;

	if (downScale <= 1.f) {
		jinfo->scale_denom = 1;
	} else if (downScale <= 2.f) {
		jinfo->scale_denom = 2;
	} else if (downScale <= 4.f) {
		jinfo->scale_denom = 4;
	} else if (downScale <= 8.f) {
		jinfo->scale_denom = 8;
	} else {
		return NULL;
	}

	jpeg_start_decompress(jinfo);

	vita2d_texture *texture = vita2d_create_empty_texture_format(
		jinfo->output_width,
		jinfo->output_height,
		SCE_GXM_TEXTURE_FORMAT_U8U8U8_BGR);

	if (!texture) {
		jpeg_abort_decompress(jinfo);
		return NULL;
	}

	void *texture_data = vita2d_texture_get_datap(texture);
	unsigned int row_stride = vita2d_texture_get_stride(texture);
	unsigned char *row_pointer = texture_data;

	while (jinfo->output_scanline < jinfo->output_height) {
		jpeg_read_scanlines(jinfo, (JSAMPARRAY)&row_pointer, 1);
		row_pointer += row_stride;
	}

	jpeg_finish_decompress(jinfo);

	return texture;
}


vita2d_texture *vita2d_load_JPEG_file(const char *filename)
{
	FILE *fp;
	if ((fp = fopen(filename, "rb")) < 0) {
		return NULL;
	}

	unsigned int magic = 0;
	fread(&magic, 1, sizeof(unsigned int), fp);
	fseek(fp, 0, SEEK_SET);

	if (magic != 0xE0FFD8FF && magic != 0xE1FFD8FF) {
		fclose(fp);
		return NULL;
	}

	struct jpeg_decompress_struct jinfo;
	struct jpeg_error_mgr jerr;

	jinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&jinfo);
	jpeg_stdio_src(&jinfo, fp);
	jpeg_read_header(&jinfo, 1);

	vita2d_texture *texture = _vita2d_load_JPEG_generic(&jinfo, &jerr);

	jpeg_destroy_decompress(&jinfo);

	fclose(fp);
	return texture;
}


vita2d_texture *vita2d_load_JPEG_buffer(const void *buffer, unsigned long buffer_size)
{
	unsigned int magic = *(unsigned int *)buffer;
	if (magic != 0xE0FFD8FF && magic != 0xE1FFD8FF) {
		return NULL;
	}

	struct jpeg_decompress_struct jinfo;
	struct jpeg_error_mgr jerr;

	jinfo.err = jpeg_std_error(&jerr);

	jpeg_create_decompress(&jinfo);
	jpeg_mem_src(&jinfo, (void *)buffer, buffer_size);
	jpeg_read_header(&jinfo, 1);

	vita2d_texture *texture = _vita2d_load_JPEG_generic(&jinfo, &jerr);

	jpeg_destroy_decompress(&jinfo);

	return texture;
}
