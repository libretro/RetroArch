/*
 * (C) Gražvydas "notaz" Ignotas, 2009-2010
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/fb.h>
#include <linux/matroxfb.h>

#include "fbdev.h"

#define PFX "fbdev: "

struct vout_fbdev {
	int	fd;
	void	*mem;
	size_t	mem_size;
	struct	fb_var_screeninfo fbvar_old;
	struct	fb_var_screeninfo fbvar_new;
	int	buffer_write;
	int	fb_size;
	int	buffer_count;
	int	top_border, bottom_border;
	void	*mem_saved;
	size_t	mem_saved_size;
};

void *vout_fbdev_flip(struct vout_fbdev *fbdev)
{
	int draw_buf;

	if (fbdev->buffer_count < 2)
		return fbdev->mem;

	draw_buf = fbdev->buffer_write;
	fbdev->buffer_write++;
	if (fbdev->buffer_write >= fbdev->buffer_count)
		fbdev->buffer_write = 0;

	fbdev->fbvar_new.yoffset = 
		(fbdev->top_border + fbdev->fbvar_new.yres + fbdev->bottom_border) * draw_buf +
		fbdev->top_border;

	ioctl(fbdev->fd, FBIOPAN_DISPLAY, &fbdev->fbvar_new);

	return (char *)fbdev->mem + fbdev->fb_size * fbdev->buffer_write;
}

void vout_fbdev_wait_vsync(struct vout_fbdev *fbdev)
{
	int arg = 0;
	ioctl(fbdev->fd, FBIO_WAITFORVSYNC, &arg);
}

/* it is recommended to call vout_fbdev_clear() before this */
void *vout_fbdev_resize(struct vout_fbdev *fbdev, int w, int h, int bpp,
		      int left_border, int right_border, int top_border, int bottom_border, int buffer_cnt)
{
	int w_total = left_border + w + right_border;
	int h_total = top_border + h + bottom_border;
	size_t mem_size;
	int ret;

	// unblank to be sure the mode is really accepted
	ioctl(fbdev->fd, FBIOBLANK, FB_BLANK_UNBLANK);

	if (fbdev->fbvar_new.bits_per_pixel != bpp ||
			fbdev->fbvar_new.xres != w ||
			fbdev->fbvar_new.yres != h ||
			fbdev->fbvar_new.xres_virtual != w_total||
			fbdev->fbvar_new.yres_virtual < h_total ||
			fbdev->fbvar_new.xoffset != left_border ||
			fbdev->buffer_count != buffer_cnt)
	{
		if (fbdev->fbvar_new.bits_per_pixel != bpp ||
				w != fbdev->fbvar_new.xres || h != fbdev->fbvar_new.yres)
			printf(PFX "switching to %dx%d@%d\n", w, h, bpp);

		fbdev->fbvar_new.xres = w;
		fbdev->fbvar_new.yres = h;
		fbdev->fbvar_new.xres_virtual = w_total;
		fbdev->fbvar_new.yres_virtual = h_total * buffer_cnt;
		fbdev->fbvar_new.xoffset = left_border;
		fbdev->fbvar_new.yoffset = top_border;
		fbdev->fbvar_new.bits_per_pixel = bpp;
		fbdev->fbvar_new.nonstd = 0; // can set YUV here on omapfb
		fbdev->buffer_count = buffer_cnt;
		fbdev->buffer_write = buffer_cnt > 1 ? 1 : 0;

		// seems to help a bit to avoid glitches
		vout_fbdev_wait_vsync(fbdev);

		ret = ioctl(fbdev->fd, FBIOPUT_VSCREENINFO, &fbdev->fbvar_new);
		if (ret == -1) {
			// retry with no multibuffering
			fbdev->fbvar_new.yres_virtual = h_total;
			ret = ioctl(fbdev->fd, FBIOPUT_VSCREENINFO, &fbdev->fbvar_new);
			if (ret == -1) {
				perror(PFX "FBIOPUT_VSCREENINFO ioctl");
				return NULL;
			}
			fbdev->buffer_count = 1;
			fbdev->buffer_write = 0;
			fprintf(stderr, PFX "Warning: failed to increase virtual resolution, "
					"multibuffering disabled\n");
		}

	}

	fbdev->fb_size = w_total * h_total * bpp / 8;
	fbdev->top_border = top_border;
	fbdev->bottom_border = bottom_border;

	mem_size = fbdev->fb_size * fbdev->buffer_count;
	if (fbdev->mem_size >= mem_size)
		goto out;

	if (fbdev->mem != NULL)
		munmap(fbdev->mem, fbdev->mem_size);

	fbdev->mem = mmap(0, mem_size, PROT_WRITE|PROT_READ, MAP_SHARED, fbdev->fd, 0);
	if (fbdev->mem == MAP_FAILED && fbdev->buffer_count > 1) {
		fprintf(stderr, PFX "Warning: can't map %zd bytes, doublebuffering disabled\n", mem_size);
		fbdev->buffer_count = 1;
		fbdev->buffer_write = 0;
		mem_size = fbdev->fb_size;
		fbdev->mem = mmap(0, mem_size, PROT_WRITE|PROT_READ, MAP_SHARED, fbdev->fd, 0);
	}
	if (fbdev->mem == MAP_FAILED) {
		fbdev->mem = NULL;
		fbdev->mem_size = 0;
		perror(PFX "mmap framebuffer");
		return NULL;
	}

	fbdev->mem_size = mem_size;

out:
	return (char *)fbdev->mem + fbdev->fb_size * fbdev->buffer_write;
}

void vout_fbdev_clear(struct vout_fbdev *fbdev)
{
	memset(fbdev->mem, 0, fbdev->mem_size);
}

void vout_fbdev_clear_lines(struct vout_fbdev *fbdev, int y, int count)
{
	int stride = fbdev->fbvar_new.xres_virtual * fbdev->fbvar_new.bits_per_pixel / 8;
	int i;

	if (y + count > fbdev->top_border + fbdev->fbvar_new.yres)
		count = fbdev->top_border + fbdev->fbvar_new.yres - y;

	if (y >= 0 && count > 0)
		for (i = 0; i < fbdev->buffer_count; i++)
			memset((char *)fbdev->mem + fbdev->fb_size * i + y * stride, 0, stride * count);
}

void *vout_fbdev_get_active_mem(struct vout_fbdev *fbdev)
{
	int i;

	i = fbdev->buffer_write - 1;
	if (i < 0)
		i = fbdev->buffer_count - 1;

	return (char *)fbdev->mem + fbdev->fb_size * i;
}

int vout_fbdev_get_fd(struct vout_fbdev *fbdev)
{
  if (fbdev == NULL) return -1;

  return fbdev->fd;
}

struct vout_fbdev *vout_fbdev_preinit(int fbdev_fd)
{
  struct vout_fbdev *fbdev;

  fbdev = calloc(1, sizeof(*fbdev));
  if (fbdev == NULL) return NULL;

  fbdev->fd = fbdev_fd;

  return fbdev;
}

int vout_fbdev_init(struct vout_fbdev *fbdev, int *w, int *h, int bpp, int buffer_cnt)
{
  int req_w, req_h;
  void *pret;
  int ret;

  ret = ioctl(fbdev->fd, FBIOGET_VSCREENINFO, &fbdev->fbvar_old);
  if (ret == -1) {
    perror(PFX "FBIOGET_VSCREENINFO ioctl");
    return -1;
  }

  fbdev->fbvar_new = fbdev->fbvar_old;

  req_w = fbdev->fbvar_new.xres;
  if (*w != 0)
    req_w = *w;
  req_h = fbdev->fbvar_new.yres;
  if (*h != 0)
    req_h = *h;

  pret = vout_fbdev_resize(fbdev, req_w, req_h, bpp, 0, 0, 0, 0, buffer_cnt);
  if (pret == NULL)
    return -1;

  printf(PFX "%ix%i@%d\n", fbdev->fbvar_new.xres,
         fbdev->fbvar_new.yres, fbdev->fbvar_new.bits_per_pixel);
  *w = fbdev->fbvar_new.xres;
  *h = fbdev->fbvar_new.yres;

  memset(fbdev->mem, 0, fbdev->mem_size);

  // some checks
  ret = 0;
  ret = ioctl(fbdev->fd, FBIO_WAITFORVSYNC, &ret);
  if (ret != 0)
    fprintf(stderr, PFX "Warning: vsync doesn't seem to be supported\n");

  if (fbdev->buffer_count > 1) {
    fbdev->buffer_write = 0;
    fbdev->fbvar_new.yoffset = fbdev->fbvar_new.yres * (fbdev->buffer_count - 1);
    ret = ioctl(fbdev->fd, FBIOPAN_DISPLAY, &fbdev->fbvar_new);
    if (ret != 0) {
      fbdev->buffer_count = 1;
      fprintf(stderr, PFX "Warning: can't pan display, doublebuffering disabled\n");
    }
  }

  printf("fbdev initialized.\n");
  return 0;
}

void vout_fbdev_release(struct vout_fbdev *fbdev)
{
  if (fbdev->mem == NULL) return;

	ioctl(fbdev->fd, FBIOPUT_VSCREENINFO, &fbdev->fbvar_old);
	if (fbdev->mem != MAP_FAILED)
		munmap(fbdev->mem, fbdev->mem_size);
	fbdev->mem = NULL;
}

int vout_fbdev_save(struct vout_fbdev *fbdev)
{
	void *tmp;

	if (fbdev == NULL || fbdev->mem == NULL || fbdev->mem == MAP_FAILED) {
		fprintf(stderr, PFX "bad args for save\n");
		return -1;
	}

	if (fbdev->mem_saved_size < fbdev->mem_size) {
		tmp = realloc(fbdev->mem_saved, fbdev->mem_size);
		if (tmp == NULL)
			return -1;
		fbdev->mem_saved = tmp;
	}
	memcpy(fbdev->mem_saved, fbdev->mem, fbdev->mem_size);
	fbdev->mem_saved_size = fbdev->mem_size;

	vout_fbdev_release(fbdev);
	return 0;
}

int vout_fbdev_restore(struct vout_fbdev *fbdev)
{
	int ret;

	if (fbdev == NULL || fbdev->mem != NULL) {
		fprintf(stderr, PFX "bad args/state for restore\n");
		return -1;
	}

	fbdev->mem = mmap(0, fbdev->mem_size, PROT_WRITE|PROT_READ, MAP_SHARED, fbdev->fd, 0);
	if (fbdev->mem == MAP_FAILED) {
		perror(PFX "restore: memory restore failed");
		return -1;
	}
	memcpy(fbdev->mem, fbdev->mem_saved, fbdev->mem_size);

	ret = ioctl(fbdev->fd, FBIOPUT_VSCREENINFO, &fbdev->fbvar_new);
	if (ret == -1) {
		perror(PFX "restore: FBIOPUT_VSCREENINFO");
		return -1;
	}

	return 0;
}

void vout_fbdev_teardown(struct vout_fbdev* fbdev)
{
  if (fbdev == NULL) return;

  if (fbdev->fd >= 0) close(fbdev->fd);
  fbdev->fd = -1;
  free(fbdev);
}
