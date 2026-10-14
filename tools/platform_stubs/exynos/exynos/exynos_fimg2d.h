#ifndef EXYNOS_FIMG2D_STUB_H
#define EXYNOS_FIMG2D_STUB_H
/* Stand-in for libdrm's exynos_fimg2d.h: the types, enumerants and
 * prototypes gfx/drivers/exynos_gfx.c uses, with the values and
 * signatures libdrm gives them, so the driver is compiled - and its
 * calls are checked - where libdrm is built without its exynos
 * module. */
#define G2D_PLANE_MAX_NR 2

enum e_g2d_color_mode
{
   G2D_COLOR_FMT_XRGB8888 = 0,
   G2D_COLOR_FMT_ARGB8888,
   G2D_COLOR_FMT_RGB565,
   G2D_COLOR_FMT_XRGB1555,
   G2D_COLOR_FMT_ARGB1555,
   G2D_COLOR_FMT_XRGB4444,
   G2D_COLOR_FMT_ARGB4444,
   G2D_COLOR_FMT_PRGB888,
   G2D_COLOR_FMT_YCbCr444,
   G2D_COLOR_FMT_YCbCr422,
   G2D_COLOR_FMT_YCbCr420 = 10,
   G2D_COLOR_FMT_A8,
   G2D_COLOR_FMT_L8,
   G2D_COLOR_FMT_MASK = 15,

   G2D_ORDER_AXRGB = (0 << 4),
   G2D_ORDER_RGBAX = (1 << 4),
   G2D_ORDER_AXBGR = (3 << 4),
   G2D_ORDER_BGRAX = (4 << 4),
   G2D_ORDER_MASK  = (7 << 4)
};

enum e_g2d_select_mode
{
   G2D_SELECT_MODE_NORMAL = (0 << 0),
   G2D_SELECT_MODE_FGCOLOR = (1 << 0),
   G2D_SELECT_MODE_BGCOLOR = (2 << 0)
};

enum e_g2d_repeat_mode
{
   G2D_REPEAT_MODE_REPEAT,
   G2D_REPEAT_MODE_PAD,
   G2D_REPEAT_MODE_REFLECT,
   G2D_REPEAT_MODE_CLAMP,
   G2D_REPEAT_MODE_NONE
};

enum e_g2d_scale_mode
{
   G2D_SCALE_MODE_NONE = 0,
   G2D_SCALE_MODE_NEAREST,
   G2D_SCALE_MODE_BILINEAR,
   G2D_SCALE_MODE_MAX
};

enum e_g2d_buf_type
{
   G2D_IMGBUF_COLOR,
   G2D_IMGBUF_GEM,
   G2D_IMGBUF_USERPTR
};

enum e_g2d_op
{
   G2D_OP_CLEAR       = 0x00,
   G2D_OP_SRC         = 0x01,
   G2D_OP_DST         = 0x02,
   G2D_OP_OVER        = 0x03,
   G2D_OP_INTERPOLATE = 0x04,
   G2D_OP_DISJOINT_CLEAR = 0x10,
   G2D_OP_DISJOINT_SRC   = 0x11,
   G2D_OP_DISJOINT_DST   = 0x12,
   G2D_OP_CONJOINT_CLEAR = 0x20,
   G2D_OP_CONJOINT_SRC   = 0x21,
   G2D_OP_CONJOINT_DST   = 0x22
};

struct drm_exynos_g2d_userptr
{
   unsigned long userptr;
   unsigned long size;
};

struct g2d_context;

struct g2d_image
{
   enum e_g2d_select_mode select_mode;
   enum e_g2d_color_mode color_mode;
   enum e_g2d_repeat_mode repeat_mode;
   enum e_g2d_scale_mode scale_mode;
   unsigned int xscale;
   unsigned int yscale;
   unsigned char rotate_90;
   unsigned char x_dir;
   unsigned char y_dir;
   unsigned char component_alpha;
   unsigned int width;
   unsigned int height;
   unsigned int stride;
   unsigned int need_free;
   unsigned int color;
   enum e_g2d_buf_type buf_type;
   unsigned int bo[G2D_PLANE_MAX_NR];
   struct drm_exynos_g2d_userptr user_ptr[G2D_PLANE_MAX_NR];
   void *mapped_ptr[G2D_PLANE_MAX_NR];
};

struct g2d_context *g2d_init(int fd);
void g2d_fini(struct g2d_context *ctx);
int g2d_exec(struct g2d_context *ctx);
int g2d_solid_fill(struct g2d_context *ctx, struct g2d_image *img,
      unsigned int x, unsigned int y, unsigned int w, unsigned int h);
int g2d_copy_with_scale(struct g2d_context *ctx, struct g2d_image *src,
      struct g2d_image *dst, unsigned int src_x, unsigned int src_y,
      unsigned int src_w, unsigned int src_h, unsigned int dst_x,
      unsigned int dst_y, unsigned int dst_w, unsigned int dst_h,
      unsigned int negative);
int g2d_scale_and_blend(struct g2d_context *ctx, struct g2d_image *src,
      struct g2d_image *dst, unsigned int src_x, unsigned int src_y,
      unsigned int src_w, unsigned int src_h, unsigned int dst_x,
      unsigned int dst_y, unsigned int dst_w, unsigned int dst_h,
      enum e_g2d_op op);

#endif
