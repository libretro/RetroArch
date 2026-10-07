/* The graphics processor.
 *
 * Commands go through the write-gather pipe into a ring the command
 * processor reads.  The functions here write whole register states;
 * the few registers that pack several settings (generation mode,
 * blending) are kept as shadows so each setter writes one register. */

#ifndef GEKKO_GX_H
#define GEKKO_GX_H

#include <gekko/gekko.h>

/* ---- the pipe ---- */

#define GK_GX_PIPE 0xcc008000u

typedef union gk_gx_pipe
{
   volatile uint8_t  u8;
   volatile uint16_t u16;
   volatile uint32_t u32;
   volatile float    f32;
} gk_gx_pipe_t;

#define GK_GX_W ((gk_gx_pipe_t*)GK_GX_PIPE)

static INLINE void gk_gx_u8(uint8_t v)   { GK_GX_W->u8  = v; }
static INLINE void gk_gx_u16(uint16_t v) { GK_GX_W->u16 = v; }
static INLINE void gk_gx_u32(uint32_t v) { GK_GX_W->u32 = v; }
static INLINE void gk_gx_f32(float v)    { GK_GX_W->f32 = v; }

/* Raw register loads. */
static INLINE void gk_gx_bp(uint32_t reg_value)
{
   gk_gx_u8(0x61);
   gk_gx_u32(reg_value);
}

static INLINE void gk_gx_cp(uint8_t reg, uint32_t v)
{
   gk_gx_u8(0x08);
   gk_gx_u8(reg);
   gk_gx_u32(v);
}

void gk_gx_xf(uint16_t addr, unsigned n, const uint32_t *v);
void gk_gx_xf1(uint16_t addr, uint32_t v);

/* ---- setup and synchronisation ---- */

/* A ring of fifo_bytes (a multiple of 32, at least 64 KiB) in MEM1,
 * and the default state: one colour channel from the vertex, one
 * texture coordinate, one modulating TEV stage, no culling, no
 * clipping, no blending, no depth test. */
int  gk_gx_init(size_t fifo_bytes);
/* Push out what the gather pipe holds. */
void gk_gx_flush(void);
/* Wait until everything sent so far has been drawn. */
void gk_gx_draw_done(void);
/* Drop everything queued (and reset the ring). */
void gk_gx_abort(void);

/* ---- vertices ---- */

enum gk_gx_attr
{
   GK_GX_POS = 0,
   GK_GX_NRM,
   GK_GX_CLR0,
   GK_GX_CLR1,
   GK_GX_TEX0,
   GK_GX_TEX1,
   GK_GX_TEX2,
   GK_GX_TEX3,
   GK_GX_TEX4,
   GK_GX_TEX5,
   GK_GX_TEX6,
   GK_GX_TEX7
};

enum gk_gx_input
{
   GK_GX_NONE = 0,
   GK_GX_DIRECT,
   GK_GX_INDEX8,
   GK_GX_INDEX16
};

/* Component types: numbers for positions and coordinates, colours. */
enum gk_gx_comp
{
   GK_GX_U8 = 0, GK_GX_S8, GK_GX_U16, GK_GX_S16, GK_GX_F32,
   GK_GX_RGB565 = 0, GK_GX_RGB8, GK_GX_RGBX8, GK_GX_RGBA4,
   GK_GX_RGBA6, GK_GX_RGBA8
};

enum gk_gx_prim
{
   GK_GX_QUADS     = 0x80,
   GK_GX_TRIANGLES = 0x90,
   GK_GX_TRISTRIP  = 0x98,
   GK_GX_TRIFAN    = 0xa0,
   GK_GX_LINES     = 0xa8,
   GK_GX_LINESTRIP = 0xb0,
   GK_GX_POINTS    = 0xb8
};

void gk_gx_vtx_clear(void);
void gk_gx_vtx_desc(enum gk_gx_attr attr, enum gk_gx_input in);
/* count: 2 or 3 position components, 1 or 2 coordinates, 3 or 4
 * colour components. */
void gk_gx_vtx_fmt(unsigned vat, enum gk_gx_attr attr, unsigned count,
      enum gk_gx_comp type, unsigned frac);
void gk_gx_array(enum gk_gx_attr attr, const void *base, unsigned stride);
void gk_gx_invalidate_vtx_cache(void);

static INLINE void gk_gx_begin(enum gk_gx_prim prim, unsigned vat,
      uint16_t n)
{
   gk_gx_u8((uint8_t)(prim | (vat & 7)));
   gk_gx_u16(n);
}

/* ---- transform ---- */

typedef float gk_mtx34[3][4];
typedef float gk_mtx44[4][4];

void gk_mtx_identity(gk_mtx34 m);
void gk_mtx_concat(gk_mtx34 a, gk_mtx34 b, gk_mtx34 out);
void gk_mtx_rotate_z(gk_mtx34 m, float degrees);
void gk_mtx_ortho(gk_mtx44 m, float top, float bottom, float left,
      float right, float near, float far);

/* Matrix slots: position/normal matrices are 0..9. */
void gk_gx_load_pos_mtx(gk_mtx34 m, unsigned slot);
void gk_gx_current_mtx(unsigned slot);
void gk_gx_projection(gk_mtx44 m, int ortho);
/* jitter: half-line offset for field rendering (0 none, 1 or 2). */
void gk_gx_viewport(float x, float y, float w, float h, float near,
      float far, unsigned jitter);
void gk_gx_scissor(unsigned x, unsigned y, unsigned w, unsigned h);

/* ---- textures ---- */

enum gk_gx_texfmt
{
   GK_GX_TF_I4 = 0, GK_GX_TF_I8, GK_GX_TF_IA4, GK_GX_TF_IA8,
   GK_GX_TF_RGB565, GK_GX_TF_RGB5A3, GK_GX_TF_RGBA8
};

enum gk_gx_wrap   { GK_GX_CLAMP = 0, GK_GX_REPEAT, GK_GX_MIRROR };
enum gk_gx_filter { GK_GX_NEAR = 0, GK_GX_LINEAR };

typedef struct gk_gx_tex
{
   uint32_t mode0;
   uint32_t image0;
   uint32_t image3;
   uint16_t width;
   uint16_t height;
} gk_gx_tex_t;

/* data: tiled texels, 32-byte aligned, flushed by the caller. */
void gk_gx_tex_init(gk_gx_tex_t *t, const void *data, unsigned width,
      unsigned height, enum gk_gx_texfmt fmt, enum gk_gx_wrap s,
      enum gk_gx_wrap t_wrap);
void gk_gx_tex_filter(gk_gx_tex_t *t, enum gk_gx_filter min,
      enum gk_gx_filter mag);
/* Bind to a texture map; texture coordinate `coord` is scaled to it. */
void gk_gx_tex_load(const gk_gx_tex_t *t, unsigned map, unsigned coord);
void gk_gx_invalidate_tex(void);

/* ---- shading ---- */

enum gk_gx_tev
{
   GK_GX_MODULATE = 0,   /* texture x vertex colour */
   GK_GX_REPLACE,        /* texture */
   GK_GX_PASSCLR         /* vertex colour */
};

void gk_gx_tev(unsigned stage, enum gk_gx_tev op);
/* map < 0: no texture for the stage. */
void gk_gx_tev_order(unsigned stage, unsigned coord, int map);
void gk_gx_num_stages(unsigned n);
void gk_gx_num_texgens(unsigned n);

/* ---- pixels ---- */

enum gk_gx_cull { GK_GX_CULL_NONE = 0, GK_GX_CULL_BACK, GK_GX_CULL_FRONT,
                  GK_GX_CULL_ALL };
enum gk_gx_cmp  { GK_GX_NEVER = 0, GK_GX_LESS, GK_GX_EQUAL, GK_GX_LEQUAL,
                  GK_GX_GREATER, GK_GX_NEQUAL, GK_GX_GEQUAL, GK_GX_ALWAYS };
enum gk_gx_blendf { GK_GX_BL_ZERO = 0, GK_GX_BL_ONE, GK_GX_BL_DSTCLR,
                    GK_GX_BL_INVDSTCLR, GK_GX_BL_SRCALPHA,
                    GK_GX_BL_INVSRCALPHA, GK_GX_BL_DSTALPHA,
                    GK_GX_BL_INVDSTALPHA };

void gk_gx_cull(enum gk_gx_cull mode);
void gk_gx_clip(int enable);
void gk_gx_z_mode(int test, enum gk_gx_cmp func, int update);
/* enable 0 writes the source as it is. */
void gk_gx_blend(int enable, enum gk_gx_blendf src, enum gk_gx_blendf dst);
void gk_gx_color_update(int on);
void gk_gx_alpha_update(int on);

/* ---- the embedded framebuffer and copies out of it ---- */

#define GK_GX_EFB_WIDTH  640
#define GK_GX_EFB_HEIGHT 528

void gk_gx_copy_clear(uint32_t rgba, uint32_t z);
/* vfilter: seven vertical taps summing to 64, or NULL for none. */
void gk_gx_copy_filter(const uint8_t *vfilter);
void gk_gx_copy_gamma(unsigned gamma);   /* 0: 1.0, 1: 1.7, 2: 2.2 */
/* Copy efb_w x efb_h from (x, y) to an XFB fb_width pixels wide and
 * xfb_lines tall (vertically scaled when that differs), optionally
 * clearing the EFB behind it. */
void gk_gx_copy_xfb(void *xfb, unsigned fb_width, unsigned x, unsigned y,
      unsigned efb_w, unsigned efb_h, unsigned xfb_lines, int clear);

/* Direct EFB pixel write, for the few places that poke. */
void gk_gx_poke(unsigned x, unsigned y, uint32_t rgba);

/* ---- display lists ---- */

/* Commands written between these go into buf (32-byte aligned)
 * instead of to the GPU; end returns the size, 0 if it overflowed. */
void   gk_gx_list_begin(void *buf, size_t size);
size_t gk_gx_list_end(void);
void   gk_gx_list_call(const void *buf, size_t size);

#endif
