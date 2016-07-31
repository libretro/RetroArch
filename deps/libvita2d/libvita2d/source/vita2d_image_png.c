#include <string.h>
#include <stdlib.h>
#include <psp2/io/fcntl.h>
#include <psp2/gxm.h>
#include <png.h>
#include "vita2d.h"

#define PNG_SIGSIZE (8)

static void _vita2d_read_png_file_fn(png_structp png_ptr, png_bytep data, png_size_t length)
{
	SceUID fd = *(SceUID*) png_get_io_ptr(png_ptr);
	sceIoRead(fd, data, length);
}

static void _vita2d_read_png_buffer_fn(png_structp png_ptr, png_bytep data, png_size_t length)
{
	unsigned int *address = png_get_io_ptr(png_ptr);
	memcpy(data, (void *)*address, length);
	*address += length;
}

static vita2d_texture *_vita2d_load_PNG_generic(const void *io_ptr, png_rw_ptr read_data_fn)
{
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		goto error_create_read;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		goto error_create_info;
	}

	png_bytep *row_ptrs = NULL;

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
		if (row_ptrs != NULL)
			free(row_ptrs);
		return NULL;
	}

	png_set_read_fn(png_ptr, (png_voidp)io_ptr, read_data_fn);
	png_set_sig_bytes(png_ptr, PNG_SIGSIZE);
	png_read_info(png_ptr, info_ptr);

	unsigned int width, height;
	int bit_depth, color_type;

	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
		&color_type, NULL, NULL, NULL);

	if ((color_type == PNG_COLOR_TYPE_PALETTE && bit_depth <= 8)
		|| (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		|| png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)
		|| (bit_depth == 16)) {
			png_set_expand(png_ptr);
	}

	if (bit_depth == 16)
		png_set_scale_16(png_ptr);

	if (bit_depth == 8 && color_type == PNG_COLOR_TYPE_RGB)
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);

	if (color_type == PNG_COLOR_TYPE_GRAY ||
	    color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);

	if (color_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_palette_to_rgb(png_ptr);
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
	}

	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png_ptr);

	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png_ptr);

	if (bit_depth < 8)
		png_set_packing(png_ptr);

	png_read_update_info(png_ptr, info_ptr);

	row_ptrs = (png_bytep *)malloc(sizeof(png_bytep) * height);
	if (!row_ptrs)
		goto error_alloc_rows;

	vita2d_texture *texture = vita2d_create_empty_texture(width, height);
	if (!texture)
		goto error_create_tex;

	void *texture_data = vita2d_texture_get_datap(texture);
	unsigned int stride = vita2d_texture_get_stride(texture);

	int i;
	for (i = 0; i < height; i++) {
		row_ptrs[i] = (png_bytep)(texture_data + i*stride);
	}

	png_read_image(png_ptr, row_ptrs);

	png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
	free(row_ptrs);

	return texture;

error_create_tex:
	free(row_ptrs);
error_alloc_rows:
	png_destroy_info_struct(png_ptr, &info_ptr);
error_create_info:
	png_destroy_read_struct(&png_ptr, (png_infopp)0, (png_infopp)0);
error_create_read:
	return NULL;
}


vita2d_texture *vita2d_load_PNG_file(const char *filename)
{
	png_byte pngsig[PNG_SIGSIZE];
	SceUID fd;

	if ((fd = sceIoOpen(filename, SCE_O_RDONLY, 0777)) < 0) {
		goto exit_error;
	}

	if (sceIoRead(fd, pngsig, PNG_SIGSIZE) != PNG_SIGSIZE) {
		goto exit_close;
	}

	if (png_sig_cmp(pngsig, 0, PNG_SIGSIZE) != 0) {
		goto exit_close;
	}

	vita2d_texture *texture = _vita2d_load_PNG_generic((void *)&fd, _vita2d_read_png_file_fn);
	sceIoClose(fd);
	return texture;

exit_close:
	sceIoClose(fd);
exit_error:
	return NULL;
}

vita2d_texture *vita2d_load_PNG_buffer(const void *buffer)
{
	if (png_sig_cmp((png_byte *) buffer, 0, PNG_SIGSIZE) != 0) {
		return NULL;
	}

	unsigned int buffer_address = (unsigned int)buffer + PNG_SIGSIZE;

	return _vita2d_load_PNG_generic((void *)&buffer_address, _vita2d_read_png_buffer_fn);
}
