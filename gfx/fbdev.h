#ifndef _FBDEV_H
#define _FBDEV_H

struct vout_fbdev;

struct vout_fbdev *vout_fbdev_preinit(int fbdev_fd);
int vout_fbdev_init(struct vout_fbdev *fbdev, int *w, int *h, int bpp, int buffer_cnt);

void *vout_fbdev_flip(struct vout_fbdev *fbdev);
void  vout_fbdev_wait_vsync(struct vout_fbdev *fbdev);
void *vout_fbdev_resize(struct vout_fbdev *fbdev, int w, int h, int bpp,
			int left_border, int right_border, int top_border, int bottom_border,
			int buffer_count);
void  vout_fbdev_clear(struct vout_fbdev *fbdev);
void  vout_fbdev_clear_lines(struct vout_fbdev *fbdev, int y, int count);
int   vout_fbdev_get_fd(struct vout_fbdev *fbdev);
void *vout_fbdev_get_active_mem(struct vout_fbdev *fbdev);
int   vout_fbdev_save(struct vout_fbdev *fbdev);
int   vout_fbdev_restore(struct vout_fbdev *fbdev);

void vout_fbdev_release(struct vout_fbdev *fbdev);
void vout_fbdev_teardown(struct vout_fbdev* fbdev);

#endif
