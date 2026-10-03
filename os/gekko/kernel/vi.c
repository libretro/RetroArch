/* Video interface.
 *
 * Each standard has one base timing for its full picture; a mode
 * narrower or shorter than that moves the blanking edges, keeping the
 * line and field lengths, so the refresh rate never depends on the
 * picture size.  Double strike is the first field's timing repeated,
 * rounded to whole lines, which is what makes it non-interlaced.
 * Base timings: YAGCD (interlaced) and NetBSD's wiifb (480p). */

#include <string.h>

#include <gekko/irq.h>
#include <gekko/thread.h>
#include <gekko/video.h>
#if GK_RVL
#include <gekko/conf.h>
#endif

#define VI_VTR     0xcc002000u
#define VI_DCR     0xcc002002u
#define VI_HTR0    0xcc002004u
#define VI_HTR1    0xcc002008u
#define VI_VTO     0xcc00200cu
#define VI_VTE     0xcc002010u
#define VI_BBOI    0xcc002014u
#define VI_BBEI    0xcc002018u
#define VI_TFBL    0xcc00201cu
#define VI_TFBR    0xcc002020u
#define VI_BFBL    0xcc002024u
#define VI_BFBR    0xcc002028u
#define VI_DI0     0xcc002030u
#define VI_DI1     0xcc002034u
#define VI_DI2     0xcc002038u
#define VI_DI3     0xcc00203cu
#define VI_PICCONF 0xcc002048u
#define VI_HSR     0xcc00204au
#define VI_FCT0    0xcc00204cu
#define VI_UNK68   0xcc002068u
#define VI_VICLK   0xcc00206cu
#define VI_VISEL   0xcc00206eu
#define VI_FBWIDTH 0xcc002070u
#define VI_HBE656  0xcc002072u
#define VI_HBS656  0xcc002074u

#define DCR_ENB    0x0001u
#define DCR_RST    0x0002u
#define DCR_NIN    0x0004u
#define DI_INT     0x80000000u
#define DI_ENB     0x10000000u
#define FB_POFF    0x10000000u

struct vi_base
{
   uint32_t htr0, htr1;
   uint32_t bboi, bbei;
   uint16_t acv;         /* active lines per field */
   uint16_t prb_o, psb_o, prb_e, psb_e;
   uint16_t max_lines;   /* per frame */
   uint8_t  equ;
   int8_t   ds_psb;      /* half-line making a field whole lines */
};

static const struct vi_base base_ntsc = {
   0x476901adu, 0x02ea5140u, 0x410c410cu, 0x40ed40edu,
   240, 24, 3, 25, 2, 480, 6, 1
};
static const struct vi_base base_pal = {
   0x4b6a01b0u, 0x02f85640u, 0x4d2b4d6du, 0x4d8a4d4cu,
   287, 35, 1, 36, 0, 574, 5, -1
};
static const struct vi_base base_480p = {
   0x476901adu, 0x030a4940u, 0x81d881d8u, 0x81d881d8u,
   480, 48, 6, 48, 6, 480, 12, 0
};

/* Anti-flicker filter coefficients (YAGCD defaults). */
static const uint32_t filter[7] = {
   0x1ae771f0u, 0x0db4a574u, 0x00c1188eu, 0xc4c0cbe2u,
   0xfcecdecfu, 0x13130f08u, 0x00080c0fu
};

struct vi_regs
{
   uint32_t htr0, htr1, vto, vte, bboi, bbei, di0, di1;
   uint32_t vto_black, vte_black;
   uint16_t vtr, vtr_black, dcr, picconf, hsr, fbwidth, viclk;
   uint16_t field_bytes;   /* XFB offset of the second field */
};

static gk_vi_mode_t      cur_mode;
static struct vi_regs    cur;
static volatile uint32_t retraces;
static volatile uint32_t field;
static uint32_t          configured;
static uint32_t          black = 1;
static uint32_t          pending_black = 1;
static uint32_t          pending_fb;   /* physical, 0 = none */
static gk_vi_fn          retrace_fn;
static void             *retrace_data;

#if GK_RVL
void gk_ave_set(unsigned std, int component);
#endif

unsigned gk_vi_max_lines(unsigned std)
{
   return std == GK_VI_PAL ? base_pal.max_lines : base_ntsc.max_lines;
}

int gk_vi_component_cable(void)
{
   return GK_REG16(VI_VISEL) & 1;
}

static unsigned boot_std(void)
{
#if GK_RVL
   switch (gk_conf_video())
   {
      case GK_CONF_VIDEO_PAL:
         return gk_conf_eurgb60() > 0 ? GK_VI_EURGB60 : GK_VI_PAL;
      case GK_CONF_VIDEO_MPAL:
         return GK_VI_MPAL;
      case GK_CONF_VIDEO_NTSC:
         return GK_VI_NTSC;
   }
#endif
   switch ((GK_REG16(VI_DCR) >> 8) & 3)
   {
      case 1:  return GK_VI_PAL;
      case 2:  return GK_VI_MPAL;
   }
   return GK_VI_NTSC;
}

void gk_vi_preferred(gk_vi_mode_t *m)
{
   int progressive = gk_vi_component_cable();
   memset(m, 0, sizeof(*m));
   m->std = (uint8_t)boot_std();
#if GK_RVL
   progressive = progressive && gk_conf_progressive() > 0;
#endif
   if (progressive && m->std == GK_VI_PAL)
      m->std = GK_VI_EURGB60;
   m->scan     = progressive ? GK_VI_PROGRESSIVE : GK_VI_INTERLACED;
   m->fb_width = 640;
   m->width    = 640;
   m->lines    = (uint16_t)gk_vi_max_lines(m->std);
   m->fb_lines = m->lines;
   m->x        = -1;
   m->y        = -1;
}

static int compute(const gk_vi_mode_t *m, struct vi_regs *r)
{
   const struct vi_base *b;
   unsigned hlw, hbe_b, hbs_b, wb, w, lines, max, y, acv;
   unsigned prb_o, psb_o, prb_e, psb_e, odd_hl, fmt;
   int left;

   if (m->scan == GK_VI_PROGRESSIVE)
   {
      if (m->std == GK_VI_PAL)
         return -1;
      b = &base_480p;
   }
   else
      b = (m->std == GK_VI_PAL) ? &base_pal : &base_ntsc;

   if (!m->fb_width || (m->fb_width & 15) || m->fb_width > 720
         || !m->lines || m->fb_lines < m->lines)
      return -1;

   /* Horizontal: the base window is centred in a 720-pixel line. */
   hlw   = b->htr0 & 0x1ff;
   hbe_b = (b->htr1 >> 7) & 0x3ff;
   hbs_b = (b->htr1 >> 17) & 0x3ff;
   wb    = hbs_b + hlw - hbe_b;
   w     = m->width < m->fb_width ? m->fb_width : m->width;
   if (w > 720)
      w = 720;
   w    &= ~1u;
   left  = (int)hbe_b - (int)(720 - wb) / 2
         + (m->x < 0 ? (int)(720 - w) / 2 : m->x);
   if (left < 0 || left + (int)w - (int)hlw <= 0)
      return -1;
   r->htr0 = b->htr0;
   r->htr1 = (b->htr1 & 0x7f) | ((uint32_t)left << 7)
         | ((uint32_t)(left + w - hlw) << 17);
   r->hsr  = w == m->fb_width ? 256
         : (uint16_t)(((256u * m->fb_width + w / 2) / w) | 0x1000);
   r->fbwidth = m->fb_width;

   /* Vertical, in half-lines. */
   max   = b->max_lines;
   lines = m->lines;
   prb_o = b->prb_o;
   psb_o = b->psb_o;
   prb_e = b->prb_e;
   psb_e = b->psb_e;
   switch (m->scan)
   {
      case GK_VI_INTERLACED:
         lines &= ~1u;
         if (lines > max)
            lines = max;
         y   = m->y < 0 ? ((max - lines) / 2) & ~1u : (unsigned)m->y & ~1u;
         if (y + lines > max)
            return -1;
         acv    = lines / 2;
         prb_o += y;
         prb_e += y;
         psb_o += max - lines - y;
         psb_e += max - lines - y;
         break;
      case GK_VI_DOUBLE_STRIKE:
         max /= 2;
         /* fall through */
      default:
         if (lines > max)
            lines = max;
         y = m->y < 0 ? (max - lines) / 2 : (unsigned)m->y;
         if (y + lines > max)
            return -1;
         acv   = lines;
         prb_o = prb_o + 2 * y;
         psb_o = psb_o + 2 * (max - lines - y);
         if (m->scan == GK_VI_DOUBLE_STRIKE)
            psb_o = (unsigned)((int)psb_o + b->ds_psb);
         prb_e = prb_o;
         psb_e = psb_o;
         break;
   }

   r->vtr       = (uint16_t)(b->equ | (acv << 4));
   r->vtr_black = b->equ;
   r->vto       = (psb_o << 16) | prb_o;
   r->vte       = (psb_e << 16) | prb_e;
   /* Black keeps every field the same length. */
   r->vto_black = (psb_o << 16) | (prb_o + 2 * acv);
   r->vte_black = (psb_e << 16) | (prb_e + 2 * acv);
   r->bboi      = b->bboi;
   r->bbei      = b->bbei;

   /* One interrupt at the start of each field; the second field starts
    * where the first ends, mid-line when interlaced. */
   odd_hl = 3u * b->equ + prb_o + 2 * acv + psb_o;
   r->di1 = DI_ENB | (1u << 16) | 1u;
   r->di0 = DI_ENB | ((odd_hl / 2 + 1) << 16)
         | ((odd_hl & 1) ? hlw + 1 : 1u);

   fmt = m->std == GK_VI_PAL ? 1 : m->std == GK_VI_MPAL ? 2 : 0;
   r->dcr     = (uint16_t)((fmt << 8)
         | (m->scan != GK_VI_INTERLACED ? DCR_NIN : 0));
   r->viclk   = m->scan == GK_VI_PROGRESSIVE ? 1 : 0;
   r->picconf = (uint16_t)(((m->fb_width / 16) << 8)
         | (m->scan == GK_VI_INTERLACED ? m->fb_width / 8 : m->fb_width / 16));
   r->field_bytes = m->scan == GK_VI_INTERLACED ? m->fb_width * 2 : 0;
   return 0;
}

static void write_fb(uint32_t phys)
{
   uint32_t bot = phys + cur.field_bytes;
   GK_REG32(VI_TFBL) = FB_POFF | (phys >> 5) | (((phys / 2) & 15) << 24);
   GK_REG32(VI_BFBL) = FB_POFF | (bot >> 5)  | (((bot / 2) & 15) << 24);
   GK_REG32(VI_TFBR) = 0;
   GK_REG32(VI_BFBR) = 0;
}

static void write_black(uint32_t on)
{
   GK_REG16(VI_VTR) = on ? cur.vtr_black : cur.vtr;
   GK_REG32(VI_VTO) = on ? cur.vto_black : cur.vto;
   GK_REG32(VI_VTE) = on ? cur.vte_black : cur.vte;
   black = on;
}

static void vi_irq(enum gk_irq irq, void *data)
{
   uint32_t di0 = GK_REG32(VI_DI0);
   uint32_t di1 = GK_REG32(VI_DI1);
   (void)irq;
   (void)data;
   if (di0 & DI_INT)
      GK_REG32(VI_DI0) = di0 & ~DI_INT;
   if (di1 & DI_INT)
      GK_REG32(VI_DI1) = di1 & ~DI_INT;
   /* Interrupts nobody here set up. */
   GK_REG32(VI_DI2) &= ~DI_INT;
   GK_REG32(VI_DI3) &= ~DI_INT;
   if (!((di0 | di1) & DI_INT) || !configured)
      return;

   field = (di0 & DI_INT) ? 1 : 0;
   if (pending_fb)
   {
      write_fb(pending_fb);
      pending_fb = 0;
   }
   if (pending_black != black)
      write_black(pending_black);
   retraces++;
   gk_futex_wake(&retraces, 0x7fffffff);
   if (retrace_fn)
      retrace_fn(retraces, retrace_data);
}

int gk_vi_configure(const gk_vi_mode_t *m)
{
   struct vi_regs r;
   uint32_t level;
   unsigned i;
   int restart;

   if (compute(m, &r))
      return -1;

   if (configured)
      gk_vi_wait_retrace();
   level = gk_irq_disable();
   restart = !configured || r.dcr != cur.dcr || r.viclk != cur.viclk;
   configured = 0;
   cur        = r;
   cur_mode   = *m;
   if (restart)
   {
      GK_REG16(VI_DCR) = DCR_RST;
      GK_REG16(VI_DCR) = 0;
   }
   GK_REG32(VI_HTR0) = r.htr0;
   GK_REG32(VI_HTR1) = r.htr1;
   GK_REG32(VI_BBOI) = r.bboi;
   GK_REG32(VI_BBEI) = r.bbei;
   for (i = 0; i < 7; i++)
      GK_REG32(VI_FCT0 + i * 4) = filter[i];
   GK_REG32(VI_UNK68)    = 0x00ff0000u;
   GK_REG16(VI_PICCONF)  = r.picconf;
   GK_REG16(VI_HSR)      = r.hsr;
   GK_REG16(VI_FBWIDTH)  = r.fbwidth;
   GK_REG16(VI_HBE656)   = 0;
   GK_REG16(VI_HBS656)   = 0;
   GK_REG16(VI_VICLK)    = r.viclk;
   write_black(1);
   pending_black = 1;
   GK_REG32(VI_DI0) = r.di0;
   GK_REG32(VI_DI1) = r.di1;
   GK_REG32(VI_DI2) = 0;
   GK_REG32(VI_DI3) = 0;
   GK_REG16(VI_DCR) = r.dcr | DCR_ENB;
   retraces   = 0;
   configured = 1;
   gk_irq_set(GK_IRQ_VI, vi_irq, NULL);
   gk_irq_restore(level);

#if GK_RVL
   gk_ave_set(m->std, gk_vi_component_cable());
#endif
   gk_vi_wait_retrace();
   return 0;
}

const gk_vi_mode_t *gk_vi_current(void)
{
   return configured ? &cur_mode : NULL;
}

double gk_vi_refresh_hz(const gk_vi_mode_t *m)
{
   struct vi_regs r;
   const struct vi_base *b;
   unsigned hl;
   double line_hz;
   if (compute(m, &r))
      return 0.0;
   b  = m->scan == GK_VI_PROGRESSIVE ? &base_480p
      : m->std == GK_VI_PAL ? &base_pal : &base_ntsc;
   /* Half-lines per field from the registers; a 27 MHz pixel pair
    * clock, doubled for progressive. */
   hl = 3u * (r.vtr & 15) + (r.vto & 0x3ff) + 2 * (r.vtr >> 4)
      + (r.vto >> 16);
   line_hz = (m->scan == GK_VI_PROGRESSIVE ? 27000000.0 : 13500000.0)
      / (2.0 * (b->htr0 & 0x1ff));
   return line_hz * 2.0 / hl;
}

void gk_vi_set_fb(void *xfb)
{
   uint32_t level = gk_irq_disable();
   pending_fb = GK_PHYS(xfb);
   gk_irq_restore(level);
}

/* Interrupts off, for the crash screen: show xfb right away. */
void gk_vi_show_now(void *xfb)
{
   if (!configured)
      return;
   write_fb(GK_PHYS(xfb));
   write_black(0);
   pending_black = 0;
   pending_fb    = 0;
}

void gk_vi_set_black(int on)
{
   uint32_t level = gk_irq_disable();
   pending_black = on ? 1 : 0;
   gk_irq_restore(level);
}

void *gk_vi_alloc_fb(const gk_vi_mode_t *m)
{
   return gk_arena_take_top(&gk_mem1,
         (size_t)m->fb_width * m->fb_lines * 2, 32);
}

void gk_vi_clear_fb(void *xfb, const gk_vi_mode_t *m, uint32_t yuyv)
{
   uint32_t *p = (uint32_t*)xfb;
   size_t n    = (size_t)m->fb_width * m->fb_lines / 2;
   size_t i;
   for (i = 0; i < n; i++)
      p[i] = yuyv;
   gk_dcache_flush(xfb, n * 4);
}

uint32_t gk_vi_retraces(void)
{
   return retraces;
}

void gk_vi_wait_retrace(void)
{
   uint32_t now = retraces;
   while (retraces == now)
      gk_futex_wait(&retraces, now, GK_US_TO_TICKS(100000));
}

int gk_vi_field(void)
{
   return (int)field;
}

void gk_vi_set_retrace_cb(gk_vi_fn fn, void *data)
{
   uint32_t level = gk_irq_disable();
   retrace_fn   = fn;
   retrace_data = data;
   gk_irq_restore(level);
}
