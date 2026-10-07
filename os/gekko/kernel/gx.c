/* Graphics processor: the command ring, register state and copies.
 * Register layouts follow YAGCD and Dolphin's VideoCommon headers. */

#include <math.h>
#include <string.h>

#include <gekko/gx.h>
#include <gekko/irq.h>
#include <gekko/thread.h>

#include "kernel.h"

/* Command processor, pixel engine, processor-interface FIFO. */
#define CP_SR        0xcc000000u
#define CP_CR        0xcc000002u
#define CP_CLEAR     0xcc000004u
#define CP_FIFO      0xcc000020u   /* base, end, hi, lo, dist, wp, rp, bp */
#define PE_CTRL      0xcc00100au
#define PI_FIFO_BASE 0xcc00300cu
#define PI_FIFO_END  0xcc003010u
#define PI_FIFO_WP   0xcc003014u
#define PI_FIFO_RST  0xcc003018u
#define EFB_BASE     0xc8000000u

#define CR_READ      0x01u
#define CR_OVF_INT   0x04u
#define CR_UNF_INT   0x08u
#define CR_LINK      0x10u
#define SR_OVF       0x01u
#define SR_UNF       0x02u

#define PE_FINISH_EN  0x02u
#define PE_TOKEN_ACK  0x04u
#define PE_FINISH_ACK 0x08u

/* XF addresses. */
#define XF_CLIP      0x1005
#define XF_VTXSPEC   0x1008
#define XF_NUMCHAN   0x1009
#define XF_MATCOLOR0 0x100c
#define XF_CHAN0     0x100e
#define XF_ALPHA0    0x1010
#define XF_DUALTEX   0x1012
#define XF_MTXIDX_A  0x1018
#define XF_MTXIDX_B  0x1019
#define XF_VIEWPORT  0x101a
#define XF_PROJ      0x1020
#define XF_NUMTEXGEN 0x103f
#define XF_TEXGEN    0x1040
#define XF_POSTMTX   0x1050

#define MTX_TEX_IDENTITY 60   /* texture matrix slot kept at identity */
#define MTX_POST_IDENTITY 61

/* BP registers. */
#define BP(reg, v)   (((uint32_t)(reg) << 24) | ((v) & 0xffffffu))
#define BP_GENMODE   0x00
#define BP_COPYPAT   0x01   /* 0x01-0x04 */
#define BP_IND_IMASK 0x0f
#define BP_IND_CMD   0x10
#define BP_SCISSOR_TL 0x20
#define BP_SCISSOR_BR 0x21
#define BP_IREF      0x27
#define BP_TREF      0x28
#define BP_SU_SSIZE  0x30
#define BP_SU_TSIZE  0x31
#define BP_ZMODE     0x40
#define BP_BLEND     0x41
#define BP_CONSTALPHA 0x42
#define BP_PECONTROL 0x43
#define BP_FIELDMASK 0x44
#define BP_DRAWDONE  0x45
#define BP_EFB_TL    0x49
#define BP_EFB_WH    0x4a
#define BP_EFB_ADDR  0x4b
#define BP_EFB_STRIDE 0x4d
#define BP_YSCALE    0x4e
#define BP_CLEAR_AR  0x4f
#define BP_CLEAR_GB  0x50
#define BP_CLEAR_Z   0x51
#define BP_COPY      0x52
#define BP_COPYFILT0 0x53
#define BP_COPYFILT1 0x54
#define BP_SCISSOR_OFF 0x59
#define BP_TEXINVAL  0x66
#define BP_FIELDMODE 0x68
#define BP_TEV_COLOR 0xc0
#define BP_TEV_ALPHA 0xc1
#define BP_FOG3      0xf1
#define BP_ALPHATEST 0xf3
#define BP_KSEL      0xf6

#define COPY_CLAMP   0x0003u
#define COPY_SCALE   0x0400u
#define COPY_CLEAR   0x0800u
#define COPY_TO_XFB  0x4000u

static uint8_t          *fifo;
static size_t            fifo_size;
static uint32_t          genmode = (1u << 4);   /* one colour channel */
static uint32_t          blendmode = 0x18;      /* colour and alpha update */
static uint32_t          vcd_lo, vcd_hi;
static uint32_t          vat_a[8], vat_b[8], vat_c[8];
static uint32_t          tref[8];
static uint32_t          copy_gamma;
static volatile uint32_t draw_done_count;
static volatile uint32_t fifo_full;
static uint32_t          list_saved_wp;
static uint32_t          list_start;
static size_t            list_size;

/* ---- ring ---- */

static void cp_write32(uint32_t reg, uint32_t v)
{
   GK_REG16(reg)     = (uint16_t)(v & 0xffff);
   GK_REG16(reg + 2) = (uint16_t)(v >> 16);
}

void gk_gx_xf(uint16_t addr, unsigned n, const uint32_t *v)
{
   unsigned i;
   gk_gx_u8(0x10);
   gk_gx_u32(((uint32_t)(n - 1) << 16) | addr);
   for (i = 0; i < n; i++)
      gk_gx_u32(v[i]);
}

void gk_gx_xf1(uint16_t addr, uint32_t v)
{
   gk_gx_xf(addr, 1, &v);
}

static void xf_f(uint16_t addr, unsigned n, const float *v)
{
   unsigned i;
   gk_gx_u8(0x10);
   gk_gx_u32(((uint32_t)(n - 1) << 16) | addr);
   for (i = 0; i < n; i++)
      gk_gx_f32(v[i]);
}

void gk_gx_flush(void)
{
   unsigned i;
   for (i = 0; i < 8; i++)
      gk_gx_u32(0);
}

/* The GPU caught up after an overflow, or finished a draw-done. */
static void cp_irq(enum gk_irq irq, void *data)
{
   uint16_t sr = GK_REG16(CP_SR);
   uint16_t cr = GK_REG16(CP_CR);
   (void)irq;
   (void)data;
   if ((sr & SR_OVF) && (cr & CR_OVF_INT))
   {
      /* Stop the writer until the ring drains to the low mark. */
      GK_REG16(CP_CR)    = (uint16_t)((cr & ~CR_OVF_INT) | CR_UNF_INT);
      GK_REG16(CP_CLEAR) = 1;
      fifo_full = 1;
      gk_irq_block_current(&fifo_full);
   }
   else if ((sr & SR_UNF) && (cr & CR_UNF_INT))
   {
      GK_REG16(CP_CR)    = (uint16_t)((cr & ~CR_UNF_INT) | CR_OVF_INT);
      GK_REG16(CP_CLEAR) = 2;
      fifo_full = 0;
      gk_futex_wake(&fifo_full, 0x7fffffff);
   }
   else
      GK_REG16(CP_CLEAR) = 3;
}

static void pe_finish_irq(enum gk_irq irq, void *data)
{
   (void)irq;
   (void)data;
   GK_REG16(PE_CTRL) = PE_FINISH_EN | PE_FINISH_ACK;
   draw_done_count++;
   gk_futex_wake(&draw_done_count, 0x7fffffff);
}

static void ring_reset(void)
{
   uint32_t base = GK_PHYS(fifo);
   uint32_t end  = base + (uint32_t)fifo_size - 32;
   GK_REG16(CP_CR)    = 0;
   GK_REG16(CP_CLEAR) = 3;
   cp_write32(CP_FIFO + 0x00, base);
   cp_write32(CP_FIFO + 0x04, end);
   cp_write32(CP_FIFO + 0x08, (uint32_t)fifo_size - 16384);   /* high */
   cp_write32(CP_FIFO + 0x0c, (uint32_t)fifo_size / 2);       /* low */
   cp_write32(CP_FIFO + 0x10, 0);                             /* distance */
   cp_write32(CP_FIFO + 0x14, base);                          /* write */
   cp_write32(CP_FIFO + 0x18, base);                          /* read */
   GK_REG32(PI_FIFO_BASE) = base;
   GK_REG32(PI_FIFO_END)  = end;
   GK_REG32(PI_FIFO_WP)   = base;
   fifo_full = 0;
   GK_REG16(CP_CR) = CR_READ | CR_LINK | CR_OVF_INT;
}

/* ---- default state ---- */

static void load_identity(uint16_t addr)
{
   static const float id[12] = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0 };
   xf_f(addr, 12, id);
}

int gk_gx_init(size_t bytes)
{
   unsigned i;
   gk_mtx34 m;

   if (bytes < 65536)
      bytes = 65536;
   bytes &= ~31u;
   if (!fifo)
   {
      if (!(fifo = (uint8_t*)gk_arena_take_top(&gk_mem1, bytes, 32)))
         return -1;
      fifo_size = bytes;
   }
   gk_dcache_invalidate(fifo, fifo_size);

   /* The write-gather pipe targets the GPU port. */
   __asm__ __volatile__("mtspr 921,%0" : : "r"(GK_PHYS(GK_GX_PIPE)));
   ring_reset();
   gk_irq_set(GK_IRQ_CP_FIFO, cp_irq, NULL);
   gk_irq_set(GK_IRQ_PE_DONE, pe_finish_irq, NULL);
   GK_REG16(PE_CTRL) = PE_FINISH_EN | PE_FINISH_ACK | PE_TOKEN_ACK;

   /* Vertex input. */
   for (i = 0; i < 8; i++)
   {
      vat_a[i] = 1u << 30;   /* byte dequantisation */
      vat_b[i] = 1u << 31;   /* vertex cache enhancement */
      vat_c[i] = 0;
      gk_gx_cp((uint8_t)(0x70 + i), vat_a[i]);
      gk_gx_cp((uint8_t)(0x80 + i), vat_b[i]);
      gk_gx_cp((uint8_t)(0x90 + i), vat_c[i]);
   }
   gk_gx_vtx_clear();

   /* Transform: matrix 0 and the texture identity, no clipping. */
   gk_mtx_identity(m);
   gk_gx_load_pos_mtx(m, 0);
   load_identity(MTX_TEX_IDENTITY * 4);
   load_identity(0x500 + MTX_POST_IDENTITY * 4);
   gk_gx_current_mtx(0);
   gk_gx_clip(0);
   gk_gx_xf1(XF_NUMCHAN, 1);
   gk_gx_xf1(XF_MATCOLOR0, 0xffffffffu);
   gk_gx_xf1(XF_CHAN0, 1);    /* material from the vertex, no lighting */
   gk_gx_xf1(XF_ALPHA0, 1);
   gk_gx_xf1(XF_DUALTEX, 0);
   for (i = 0; i < 8; i++)
   {
      gk_gx_xf1((uint16_t)(XF_TEXGEN + i), (5u + i) << 7);
      gk_gx_xf1((uint16_t)(XF_POSTMTX + i), MTX_POST_IDENTITY);
   }

   /* Shading: four identity swap tables, no indirect texturing. */
   for (i = 0; i < 4; i++)
   {
      gk_gx_bp(BP(BP_KSEL + i * 2,     0x04));
      gk_gx_bp(BP(BP_KSEL + i * 2 + 1, 0x0e));
   }
   gk_gx_bp(BP(BP_IND_IMASK, 0));
   gk_gx_bp(BP(BP_IREF, 0));
   for (i = 0; i < 16; i++)
      gk_gx_bp(BP(BP_IND_CMD + i, 0));
   for (i = 0; i < 8; i++)
      tref[i] = 0;
   gk_gx_num_texgens(1);
   gk_gx_num_stages(1);
   gk_gx_tev_order(0, 0, 0);
   gk_gx_tev(0, GK_GX_MODULATE);
   gk_gx_bp(BP(BP_ALPHATEST, (7u << 16) | (7u << 19)));
   gk_gx_bp(BP(BP_CONSTALPHA, 0));
   gk_gx_bp(BP(BP_FOG3, 0));

   /* Pixels. */
   gk_gx_bp(BP(BP_PECONTROL, 0));   /* RGB8, Z24, linear depth */
   gk_gx_cull(GK_GX_CULL_NONE);
   gk_gx_z_mode(0, GK_GX_ALWAYS, 0);
   gk_gx_blend(0, GK_GX_BL_ONE, GK_GX_BL_ZERO);
   gk_gx_bp(BP(BP_FIELDMASK, 3));
   gk_gx_bp(BP(BP_FIELDMODE, 0));
   gk_gx_bp(BP(BP_SCISSOR_OFF, (342 >> 1) | ((342 >> 1) << 10)));
   gk_gx_scissor(0, 0, GK_GX_EFB_WIDTH, GK_GX_EFB_HEIGHT);
   gk_gx_viewport(0, 0, GK_GX_EFB_WIDTH, 480, 0.0f, 1.0f, 0);
   for (i = 0; i < 4; i++)
      gk_gx_bp(BP(BP_COPYPAT + i, 0x666666));
   gk_gx_copy_filter(NULL);
   gk_gx_copy_gamma(0);
   gk_gx_copy_clear(0x000000ffu, 0x00ffffffu);
   gk_gx_invalidate_tex();
   gk_gx_invalidate_vtx_cache();
   gk_gx_flush();
   return 0;
}

void gk_gx_draw_done(void)
{
   uint32_t n;
   uint32_t level = gk_irq_disable();
   n = draw_done_count;
   gk_gx_bp(BP(BP_DRAWDONE, 0x02));
   gk_gx_flush();
   gk_irq_restore(level);
   while (draw_done_count == n)
      gk_futex_wait(&draw_done_count, n, GK_US_TO_TICKS(500000));
}

void gk_gx_abort(void)
{
   GK_REG32(PI_FIFO_RST) = 1;
   gk_sleep_us(50);
   GK_REG32(PI_FIFO_RST) = 0;
   gk_sleep_us(5);
   ring_reset();
}

/* ---- vertices ---- */

static void write_vtxspec(void)
{
   unsigned colors = 0, texs = 0, i;
   if ((vcd_lo >> 13) & 3)
      colors++;
   if ((vcd_lo >> 15) & 3)
      colors++;
   for (i = 0; i < 8; i++)
      if ((vcd_hi >> (i * 2)) & 3)
         texs++;
   gk_gx_cp(0x50, vcd_lo);
   gk_gx_cp(0x60, vcd_hi);
   gk_gx_xf1(XF_VTXSPEC, colors | ((((vcd_lo >> 11) & 3) ? 1u : 0u) << 2)
         | (texs << 4));
}

void gk_gx_vtx_clear(void)
{
   vcd_lo = vcd_hi = 0;
   write_vtxspec();
}

void gk_gx_vtx_desc(enum gk_gx_attr attr, enum gk_gx_input in)
{
   if (attr >= GK_GX_TEX0)
   {
      unsigned sh = (attr - GK_GX_TEX0) * 2;
      vcd_hi = (vcd_hi & ~(3u << sh)) | ((uint32_t)in << sh);
   }
   else
   {
      unsigned sh = 9 + attr * 2;
      vcd_lo = (vcd_lo & ~(3u << sh)) | ((uint32_t)in << sh);
   }
   write_vtxspec();
}

void gk_gx_vtx_fmt(unsigned vat, enum gk_gx_attr attr, unsigned count,
      enum gk_gx_comp type, unsigned frac)
{
   uint32_t *a = &vat_a[vat & 7], *b = &vat_b[vat & 7], *c = &vat_c[vat & 7];
   uint32_t t = (uint32_t)type, f = frac & 31;
   switch (attr)
   {
      case GK_GX_POS:
         *a = (*a & ~0x1ffu) | (count == 3 ? 1u : 0u) | (t << 1) | (f << 4);
         break;
      case GK_GX_NRM:
         *a = (*a & ~(0xfu << 9)) | (t << 10);
         break;
      case GK_GX_CLR0:
         *a = (*a & ~(0xfu << 13)) | ((count == 4 ? 1u : 0u) << 13)
            | (t << 14);
         break;
      case GK_GX_CLR1:
         *a = (*a & ~(0xfu << 17)) | ((count == 4 ? 1u : 0u) << 17)
            | (t << 18);
         break;
      case GK_GX_TEX0:
         *a = (*a & ~(0x1ffu << 21)) | ((count == 2 ? 1u : 0u) << 21)
            | (t << 22) | (f << 25);
         break;
      case GK_GX_TEX1:
      case GK_GX_TEX2:
      case GK_GX_TEX3:
         {
            unsigned sh = (attr - GK_GX_TEX1) * 9;
            *b = (*b & ~(0x1ffu << sh)) | (((count == 2 ? 1u : 0u)
                     | (t << 1) | (f << 4)) << sh);
         }
         break;
      case GK_GX_TEX4:
         *b = (*b & ~(0xfu << 27)) | ((count == 2 ? 1u : 0u) << 27)
            | (t << 28);
         *c = (*c & ~0x1fu) | f;
         break;
      default:
         {
            unsigned sh = 5 + (attr - GK_GX_TEX5) * 9;
            *c = (*c & ~(0x1ffu << sh)) | (((count == 2 ? 1u : 0u)
                     | (t << 1) | (f << 4)) << sh);
         }
         break;
   }
   gk_gx_cp((uint8_t)(0x70 + (vat & 7)), *a);
   gk_gx_cp((uint8_t)(0x80 + (vat & 7)), *b);
   gk_gx_cp((uint8_t)(0x90 + (vat & 7)), *c);
}

void gk_gx_array(enum gk_gx_attr attr, const void *base, unsigned stride)
{
   gk_gx_cp((uint8_t)(0xa0 + attr), GK_PHYS(base));
   gk_gx_cp((uint8_t)(0xb0 + attr), stride & 0xff);
}

void gk_gx_invalidate_vtx_cache(void)
{
   gk_gx_u8(0x48);
}

/* ---- transform ---- */

void gk_mtx_identity(gk_mtx34 m)
{
   memset(m, 0, sizeof(gk_mtx34));
   m[0][0] = m[1][1] = m[2][2] = 1.0f;
}

void gk_mtx_concat(gk_mtx34 a, gk_mtx34 b, gk_mtx34 out)
{
   gk_mtx34 t;
   unsigned r, c;
   for (r = 0; r < 3; r++)
   {
      for (c = 0; c < 4; c++)
         t[r][c] = a[r][0] * b[0][c] + a[r][1] * b[1][c] + a[r][2] * b[2][c];
      t[r][3] += a[r][3];
   }
   memcpy(out, t, sizeof(t));
}

void gk_mtx_rotate_z(gk_mtx34 m, float degrees)
{
   float r = degrees * 3.14159265358979f / 180.0f;
   float s = (float)sin(r), c = (float)cos(r);
   gk_mtx_identity(m);
   m[0][0] = c;
   m[0][1] = -s;
   m[1][0] = s;
   m[1][1] = c;
}

void gk_mtx_ortho(gk_mtx44 m, float t, float b, float l, float r,
      float n, float f)
{
   memset(m, 0, sizeof(gk_mtx44));
   m[0][0] = 2.0f / (r - l);
   m[0][3] = -(r + l) / (r - l);
   m[1][1] = 2.0f / (t - b);
   m[1][3] = -(t + b) / (t - b);
   /* Clip space z runs from -w (near) to 0 (far). */
   m[2][2] = -1.0f / (f - n);
   m[2][3] = -f / (f - n);
   m[3][3] = 1.0f;
}

void gk_gx_load_pos_mtx(gk_mtx34 m, unsigned slot)
{
   xf_f((uint16_t)(slot * 12), 12, &m[0][0]);
}

void gk_gx_current_mtx(unsigned slot)
{
   uint32_t a = (slot * 3) | (MTX_TEX_IDENTITY << 6)
      | (MTX_TEX_IDENTITY << 12) | (MTX_TEX_IDENTITY << 18)
      | (MTX_TEX_IDENTITY << 24);
   uint32_t b = MTX_TEX_IDENTITY | (MTX_TEX_IDENTITY << 6)
      | (MTX_TEX_IDENTITY << 12) | (MTX_TEX_IDENTITY << 18);
   gk_gx_cp(0x30, a);
   gk_gx_cp(0x40, b);
   gk_gx_xf1(XF_MTXIDX_A, a);
   gk_gx_xf1(XF_MTXIDX_B, b);
}

void gk_gx_projection(gk_mtx44 m, int ortho)
{
   float p[6];
   uint32_t w[7];
   p[0] = m[0][0];
   p[1] = ortho ? m[0][3] : m[0][2];
   p[2] = m[1][1];
   p[3] = ortho ? m[1][3] : m[1][2];
   p[4] = m[2][2];
   p[5] = m[2][3];
   memcpy(w, p, sizeof(p));
   w[6] = ortho ? 1u : 0u;
   gk_gx_xf(XF_PROJ, 7, w);
}

void gk_gx_viewport(float x, float y, float w, float h, float n, float f,
      unsigned jitter)
{
   float v[6];
   if (jitter == 1)
      y -= 0.5f;
   v[0] = w * 0.5f;
   v[1] = -h * 0.5f;
   v[2] = (f - n) * 16777215.0f;
   v[3] = x + w * 0.5f + 342.0f;
   v[4] = y + h * 0.5f + 342.0f;
   v[5] = f * 16777215.0f;
   xf_f(XF_VIEWPORT, 6, v);
}

void gk_gx_scissor(unsigned x, unsigned y, unsigned w, unsigned h)
{
   gk_gx_bp(BP(BP_SCISSOR_TL, (y + 342) | ((x + 342) << 12)));
   gk_gx_bp(BP(BP_SCISSOR_BR, (y + h - 1 + 342) | ((x + w - 1 + 342) << 12)));
}

void gk_gx_clip(int enable)
{
   gk_gx_xf1(XF_CLIP, enable ? 0 : 1);
}

/* ---- textures ---- */

void gk_gx_tex_init(gk_gx_tex_t *t, const void *data, unsigned w,
      unsigned h, enum gk_gx_texfmt fmt, enum gk_gx_wrap s,
      enum gk_gx_wrap tw)
{
   t->width  = (uint16_t)w;
   t->height = (uint16_t)h;
   t->image0 = (w - 1) | ((h - 1) << 10) | ((uint32_t)fmt << 20);
   t->image3 = GK_PHYS(data) >> 5;
   t->mode0  = (uint32_t)s | ((uint32_t)tw << 2)
      | ((uint32_t)GK_GX_LINEAR << 4) | ((uint32_t)GK_GX_LINEAR << 7);
}

void gk_gx_tex_filter(gk_gx_tex_t *t, enum gk_gx_filter min,
      enum gk_gx_filter mag)
{
   t->mode0 = (t->mode0 & ~((1u << 4) | (1u << 7)))
      | ((uint32_t)mag << 4) | ((uint32_t)min << 7);
}

void gk_gx_tex_load(const gk_gx_tex_t *t, unsigned map, unsigned coord)
{
   unsigned off = map < 4 ? map : 0x20 + (map - 4);
   uint32_t even = (map * 0x8000u) >> 5;
   uint32_t odd  = (0x80000u + map * 0x8000u) >> 5;
   gk_gx_bp(BP(0x80 + off, t->mode0));
   gk_gx_bp(BP(0x84 + off, 0));
   gk_gx_bp(BP(0x88 + off, t->image0));
   gk_gx_bp(BP(0x8c + off, even | (3u << 15) | (3u << 18)));
   gk_gx_bp(BP(0x90 + off, odd  | (3u << 15) | (3u << 18)));
   gk_gx_bp(BP(0x94 + off, t->image3));
   gk_gx_bp(BP(BP_SU_SSIZE + coord * 2, t->width - 1u));
   gk_gx_bp(BP(BP_SU_TSIZE + coord * 2, t->height - 1u));
}

void gk_gx_invalidate_tex(void)
{
   /* Both 512 KiB halves of texture memory. */
   gk_gx_bp(BP(BP_TEXINVAL, 0x1000));
   gk_gx_bp(BP(BP_TEXINVAL, 0x1100));
}

/* ---- shading ---- */

void gk_gx_tev(unsigned stage, enum gk_gx_tev op)
{
   /* Colour inputs: d + lerp(a, b, c); clamped, to PREV. */
   static const uint32_t color[3] = {
      0xf8afu,   /* a 0, b tex, c ras, d 0 */
      0xfff8u,   /* d tex */
      0xfffau    /* d ras */
   };
   static const uint32_t alpha[3] = {
      0xf2f0u,   /* a 0, b tex, c ras, d 0 */
      0xffc0u,   /* d tex */
      0xffd0u    /* d ras */
   };
   gk_gx_bp(BP(BP_TEV_COLOR + stage * 2, color[op] | (1u << 19)));
   gk_gx_bp(BP(BP_TEV_ALPHA + stage * 2, alpha[op] | (1u << 19)));
}

void gk_gx_tev_order(unsigned stage, unsigned coord, int map)
{
   unsigned pair = stage / 2, sh = (stage & 1) ? 12 : 0;
   uint32_t v = (map < 0 ? 0 : ((uint32_t)map | (1u << 6)))
      | ((coord & 7) << 3);   /* colour channel 0 */
   tref[pair] = (tref[pair] & ~(0xfffu << sh)) | (v << sh);
   gk_gx_bp(BP(BP_TREF + pair, tref[pair]));
}

void gk_gx_num_stages(unsigned n)
{
   genmode = (genmode & ~(0xfu << 10)) | (((n ? n : 1) - 1) << 10);
   gk_gx_bp(BP(BP_GENMODE, genmode));
}

void gk_gx_num_texgens(unsigned n)
{
   genmode = (genmode & ~0xfu) | (n & 0xf);
   gk_gx_bp(BP(BP_GENMODE, genmode));
   gk_gx_xf1(XF_NUMTEXGEN, n);
}

/* ---- pixels ---- */

void gk_gx_cull(enum gk_gx_cull mode)
{
   genmode = (genmode & ~(3u << 14)) | ((uint32_t)mode << 14);
   gk_gx_bp(BP(BP_GENMODE, genmode));
}

void gk_gx_z_mode(int test, enum gk_gx_cmp func, int update)
{
   gk_gx_bp(BP(BP_ZMODE, (test ? 1u : 0u) | ((uint32_t)func << 1)
            | (update ? 0x10u : 0u)));
}

void gk_gx_blend(int enable, enum gk_gx_blendf src, enum gk_gx_blendf dst)
{
   blendmode = (blendmode & 0x18) | (enable ? 1u : 0u)
      | ((uint32_t)dst << 5) | ((uint32_t)src << 8);
   gk_gx_bp(BP(BP_BLEND, blendmode));
}

void gk_gx_color_update(int on)
{
   blendmode = (blendmode & ~0x08u) | (on ? 0x08u : 0u);
   gk_gx_bp(BP(BP_BLEND, blendmode));
}

void gk_gx_alpha_update(int on)
{
   blendmode = (blendmode & ~0x10u) | (on ? 0x10u : 0u);
   gk_gx_bp(BP(BP_BLEND, blendmode));
}

/* ---- copies ---- */

void gk_gx_copy_clear(uint32_t rgba, uint32_t z)
{
   uint32_t r = rgba >> 24, g = (rgba >> 16) & 0xff;
   uint32_t b = (rgba >> 8) & 0xff, a = rgba & 0xff;
   gk_gx_bp(BP(BP_CLEAR_AR, (a << 8) | r));
   gk_gx_bp(BP(BP_CLEAR_GB, (g << 8) | b));
   gk_gx_bp(BP(BP_CLEAR_Z, z & 0xffffff));
}

void gk_gx_copy_filter(const uint8_t *vf)
{
   static const uint8_t none[7] = { 0, 0, 21, 22, 21, 0, 0 };
   if (!vf)
      vf = none;
   gk_gx_bp(BP(BP_COPYFILT0, (vf[0] & 63) | ((vf[1] & 63) << 6)
            | ((vf[2] & 63) << 12) | ((uint32_t)(vf[3] & 63) << 18)));
   gk_gx_bp(BP(BP_COPYFILT1, (vf[4] & 63) | ((vf[5] & 63) << 6)
            | ((vf[6] & 63) << 12)));
}

void gk_gx_copy_gamma(unsigned gamma)
{
   copy_gamma = gamma & 3;
}

void gk_gx_copy_xfb(void *xfb, unsigned fb_width, unsigned x, unsigned y,
      unsigned w, unsigned h, unsigned xfb_lines, int clear)
{
   uint32_t copy = COPY_CLAMP | COPY_TO_XFB | (copy_gamma << 7)
      | (clear ? COPY_CLEAR : 0);
   gk_gx_bp(BP(BP_EFB_TL, x | (y << 10)));
   gk_gx_bp(BP(BP_EFB_WH, (w - 1) | ((h - 1) << 10)));
   gk_gx_bp(BP(BP_EFB_ADDR, GK_PHYS(xfb) >> 5));
   gk_gx_bp(BP(BP_EFB_STRIDE, fb_width * 2 / 32));
   if (xfb_lines && xfb_lines != h)
   {
      /* Lines out = 1 + (h - 1) * 256 / scale. */
      uint32_t scale = (256u * (h - 1) + (xfb_lines - 1) / 2)
         / (xfb_lines - 1);
      gk_gx_bp(BP(BP_YSCALE, scale));
      copy |= COPY_SCALE;
   }
   else
      gk_gx_bp(BP(BP_YSCALE, 256));
   gk_gx_bp(BP(BP_COPY, copy));
}

void gk_gx_poke(unsigned x, unsigned y, uint32_t rgba)
{
   uint32_t argb = (rgba >> 8) | (rgba << 24);
   *(volatile uint32_t*)(EFB_BASE | (y << 12) | (x << 2)) = argb;
}

/* ---- display lists: the processor-interface FIFO points at the list
 * buffer with the GPU unlinked, so nothing reaches it meanwhile ---- */

void gk_gx_list_begin(void *buf, size_t size)
{
   uint32_t base = GK_PHYS(buf);
   gk_gx_flush();
   gk_dcache_invalidate(buf, size);
   list_saved_wp = GK_REG32(PI_FIFO_WP);
   list_start    = base;
   list_size     = size;
   GK_REG16(CP_CR) = GK_REG16(CP_CR) & ~CR_LINK;
   GK_REG32(PI_FIFO_BASE) = base;
   GK_REG32(PI_FIFO_END)  = base + (uint32_t)size - 32;
   GK_REG32(PI_FIFO_WP)   = base;
}

size_t gk_gx_list_end(void)
{
   uint32_t wp, base = GK_PHYS(fifo);
   size_t used;
   gk_gx_flush();
   wp   = GK_REG32(PI_FIFO_WP) & 0x3fffffe0u;
   used = wp - list_start;
   /* Back into the ring where the GPU's write pointer is. */
   GK_REG32(PI_FIFO_BASE) = base;
   GK_REG32(PI_FIFO_END)  = base + (uint32_t)fifo_size - 32;
   GK_REG32(PI_FIFO_WP)   = list_saved_wp;
   GK_REG16(CP_CR) = GK_REG16(CP_CR) | CR_LINK;
   /* The pipe padded with zero words (NOPs); a wrap means it overran. */
   return (wp < list_start || used >= list_size) ? 0 : used;
}

void gk_gx_list_call(const void *buf, size_t size)
{
   gk_gx_u8(0x40);
   gk_gx_u32(GK_PHYS(buf));
   gk_gx_u32((uint32_t)size);
}
