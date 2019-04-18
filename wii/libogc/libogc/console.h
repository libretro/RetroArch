#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#define FONT_XSIZE		8
#define FONT_YSIZE		16
#define FONT_XFACTOR	1
#define FONT_YFACTOR	1
#define FONT_XGAP			0
#define FONT_YGAP			0
#define TAB_SIZE			4

typedef struct _console_data_s {
	void *destbuffer;
	unsigned char *font;
	int con_xres,con_yres,con_stride;
	int target_x,target_y, tgt_stride;
	int cursor_row,cursor_col;
	int saved_row,saved_col;
	int con_rows, con_cols;

	unsigned int foreground,background;
} console_data_s;

extern int __console_write(struct _reent *r,void *fd,const char *ptr,size_t len);
extern void __console_init(void *framebuffer,int xstart,int ystart,int xres,int yres,int stride);

//extern const devoptab_t dotab_stdout;

#endif
