/* Video interface: scan-out timing, the external framebuffer and the
 * retrace interrupt; on the Wii also the A/V encoder.
 *
 * External framebuffers (XFBs) are YUYV, two pixels per 32-bit word,
 * in MEM1. */

#ifndef GEKKO_VIDEO_H
#define GEKKO_VIDEO_H

#include <gekko/gekko.h>

enum gk_vi_std
{
   GK_VI_NTSC = 0,
   GK_VI_PAL,
   GK_VI_MPAL,
   GK_VI_EURGB60   /* 60 Hz timing, PAL colour */
};

enum gk_vi_scan
{
   GK_VI_INTERLACED = 0,
   GK_VI_PROGRESSIVE,
   GK_VI_DOUBLE_STRIKE   /* one field repeated: 240 or 287 lines */
};

typedef struct gk_vi_mode
{
   uint16_t fb_width;   /* XFB pixels per line, a multiple of 16 */
   uint16_t fb_lines;   /* XFB lines */
   uint16_t width;      /* shown width in pixels, up to 720 */
   uint16_t lines;      /* shown lines per frame */
   int16_t  x;          /* left edge in the 720-pixel line; -1 centres */
   int16_t  y;          /* top line; -1 centres */
   uint8_t  std;        /* enum gk_vi_std */
   uint8_t  scan;       /* enum gk_vi_scan */
} gk_vi_mode_t;

#define GK_YUYV_BLACK 0x10801080u
#define GK_YUYV_WHITE 0xeb80eb80u

/* Lines a standard shows at most per frame (per field when double
 * striking: half of it). */
unsigned gk_vi_max_lines(unsigned std);

/* The mode the console is set up for: its standard, 480p on a
 * component cable when the settings allow it, 640 wide. */
void gk_vi_preferred(gk_vi_mode_t *mode);
int  gk_vi_component_cable(void);

/* Program a mode, black until gk_vi_set_black(0).  Takes effect at
 * the next retrace; returns once it has.  0 on success. */
int  gk_vi_configure(const gk_vi_mode_t *mode);
const gk_vi_mode_t *gk_vi_current(void);
double gk_vi_refresh_hz(const gk_vi_mode_t *mode);

/* Shown from the next field on. */
void gk_vi_set_fb(void *xfb);
void gk_vi_set_black(int black);

/* An XFB for a mode, in MEM1, and filling one with a colour. */
void *gk_vi_alloc_fb(const gk_vi_mode_t *mode);
void  gk_vi_clear_fb(void *xfb, const gk_vi_mode_t *mode, uint32_t yuyv);

/* Retraces (fields) since gk_vi_configure, and waiting for the next. */
uint32_t gk_vi_retraces(void);
void     gk_vi_wait_retrace(void);
/* The field being shown: 0 for the first (odd), 1 for the second. */
int      gk_vi_field(void);

/* Called at each retrace in interrupt context, after the VI has taken
 * any pending framebuffer. */
typedef void (*gk_vi_fn)(uint32_t retrace, void *data);
void gk_vi_set_retrace_cb(gk_vi_fn fn, void *data);

#if GK_RVL
/* The A/V encoder: the composite video trap filter (better luma and
 * chroma separation, a softer picture), and the output gamma in
 * tenths, 10 being 1.0, from 1 to 30.  Both stay across modes. */
void gk_vi_set_trap_filter(int on);
void gk_vi_set_gamma(unsigned tenths);
#endif

#endif
