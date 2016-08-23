#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <psp2/io/fcntl.h>
#include <psp2/gxm.h>
#include "vita2d.h"

#define BMP_SIGNATURE (0x4D42)

typedef struct {
	unsigned short	bfType;
	unsigned int	bfSize;
	unsigned short	bfReserved1;
	unsigned short	bfReserved2;
	unsigned int	bfOffBits;
} __attribute__((packed)) BITMAPFILEHEADER;

typedef struct {
	unsigned int	biSize;
	int		biWidth;
	int		biHeight;
	unsigned short	biPlanes;
	unsigned short	biBitCount;
	unsigned int	biCompression;
	unsigned int	biSizeImage;
	int		biXPelsPerMeter;
	int		biYPelsPerMeter;
	unsigned int	biClrUsed;
	unsigned int	biClrImportant;
} __attribute__((packed)) BITMAPINFOHEADER;


static vita2d_texture *_vita2d_load_BMP_generic(
	BITMAPFILEHEADER *bmp_fh,
	BITMAPINFOHEADER *bmp_ih,
	void *user_data,
	void (*seek_fn)(void *user_data, unsigned int offset),
	void (*read_fn)(void *user_data, void *buffer, unsigned int length))
{
	unsigned int row_stride = bmp_ih->biWidth * (bmp_ih->biBitCount/8);
	if (row_stride%4 != 0) {
		row_stride += 4-(row_stride%4);
	}

	void *buffer = malloc(row_stride);
	if (!buffer)
		return NULL;

	vita2d_texture *texture = vita2d_create_empty_texture(
		bmp_ih->biWidth,
		bmp_ih->biHeight);

	if (!texture) {
		free(buffer);
		return NULL;
	}

	void *texture_data = vita2d_texture_get_datap(texture);
	unsigned int tex_stride = vita2d_texture_get_stride(texture);

	int i, x, y;

	seek_fn(user_data, bmp_fh->bfOffBits);

	for (i = 0; i < bmp_ih->biHeight; i++) {

		read_fn(user_data, buffer, row_stride);

		y = bmp_ih->biHeight - 1 - i;
		unsigned int *tex_ptr = (unsigned int *)(texture_data + y*tex_stride);

		for (x = 0; x < bmp_ih->biWidth; x++) {

			if (bmp_ih->biBitCount == 32) {		//ABGR8888
				unsigned int color = *(unsigned int *)(buffer + x*4);
				*tex_ptr = (color&0xFF)<<24 | ((color>>8)&0xFF)<<16 |
					((color>>16)&0xFF)<<8 | (color>>24);

			} else if (bmp_ih->biBitCount == 24) {	//BGR888
				unsigned char *address = buffer + x*3;
				*tex_ptr = (*address)<<16 | (*(address+1))<<8 |
					(*(address+2)) | (0xFF<<24);

			} else if (bmp_ih->biBitCount == 16) {	//BGR565
				unsigned int color = *(unsigned short *)(buffer + x*2);
				unsigned char r = (color       & 0x1F)  *((float)255/31);
				unsigned char g = ((color>>5)  & 0x3F)  *((float)255/63);
				unsigned char b = ((color>>11) & 0x1F)  *((float)255/31);
				*tex_ptr = ((r<<16) | (g<<8) | b | (0xFF<<24));
			}

			tex_ptr++;
		}
	}

	free(buffer);

	return texture;
}

static void _vita2d_read_bmp_file_seek_fn(void *user_data, unsigned int offset)
{
	sceIoLseek(*(SceUID*)user_data, offset, SEEK_SET);
}

static void _vita2d_read_bmp_file_read_fn(void *user_data, void *buffer, unsigned int length)
{
	sceIoRead(*(SceUID*)user_data, buffer, length);
}

static void _vita2d_read_bmp_buffer_seek_fn(void *user_data, unsigned int offset)
{
	*(unsigned int *)user_data += offset;
}

static void _vita2d_read_bmp_buffer_read_fn(void *user_data, void *buffer, unsigned int length)
{
	memcpy(buffer, (void *)*(unsigned int *)user_data, length);
	*(unsigned int *)user_data += length;
}

vita2d_texture *vita2d_load_BMP_file(const char *filename)
{
	SceUID fd;
	if ((fd = sceIoOpen(filename, SCE_O_RDONLY, 0777)) < 0) {
		goto exit_error;
	}

	BITMAPFILEHEADER bmp_fh;
	sceIoRead(fd, (void *)&bmp_fh, sizeof(BITMAPFILEHEADER));
	if (bmp_fh.bfType != BMP_SIGNATURE) {
		goto exit_close;
	}

	BITMAPINFOHEADER bmp_ih;
	sceIoRead(fd, (void *)&bmp_ih, sizeof(BITMAPINFOHEADER));

	vita2d_texture *texture = _vita2d_load_BMP_generic(&bmp_fh,
		&bmp_ih,
		(void *)&fd,
		_vita2d_read_bmp_file_seek_fn,
		_vita2d_read_bmp_file_read_fn);

	sceIoClose(fd);
	return texture;

exit_close:
	sceIoClose(fd);
exit_error:
	return NULL;
}

vita2d_texture *vita2d_load_BMP_buffer(const void *buffer)
{
	BITMAPFILEHEADER bmp_fh;
	memcpy(&bmp_fh, buffer, sizeof(BITMAPFILEHEADER));
	if (bmp_fh.bfType != BMP_SIGNATURE) {
		goto exit_error;
	}

	BITMAPINFOHEADER bmp_ih;
	memcpy(&bmp_ih, buffer + sizeof(BITMAPFILEHEADER), sizeof(BITMAPINFOHEADER));

	unsigned int buffer_address = (unsigned int)buffer;

	vita2d_texture *texture = _vita2d_load_BMP_generic(&bmp_fh,
		&bmp_ih,
		(void *)&buffer_address,
		_vita2d_read_bmp_buffer_seek_fn,
		_vita2d_read_bmp_buffer_read_fn);

	return texture;
exit_error:
	return NULL;
}
