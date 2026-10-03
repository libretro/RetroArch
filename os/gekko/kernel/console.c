/* Text console: the 5x10 bitmap font drawn at twice its size into a
 * YUYV framebuffer, scrolling when full. */

#include <string.h>

#include <gekko/console.h>

#include "../../../gfx/bitmapfont.h"

#define SCALE   2
#define CELL_W  (FONT_WIDTH_STRIDE * SCALE)
#define CELL_H  (FONT_HEIGHT_STRIDE * SCALE)
#define MARGIN  16

#define FG GK_YUYV_WHITE
#define BG 0x10801080u

static uint32_t *fb;
static unsigned  fb_words;     /* 32-bit words per line */
static unsigned  fb_lines;
static unsigned  cols, rows, cx, cy;

void gk_console_attach(void *xfb, const gk_vi_mode_t *m)
{
   fb       = (uint32_t*)xfb;
   fb_words = m->fb_width / 2;
   fb_lines = m->fb_lines;
   cols     = (m->fb_width - 2 * MARGIN) / CELL_W;
   rows     = (m->fb_lines - 2 * MARGIN) / CELL_H;
   cx = cy  = 0;
   gk_vi_clear_fb(xfb, m, BG);
}

void gk_console_detach(void)
{
   fb = NULL;
}

static uint32_t *cell(unsigned col, unsigned row)
{
   return fb + (MARGIN + row * CELL_H) * fb_words
      + (MARGIN + col * CELL_W) / 2;
}

static void glyph(unsigned col, unsigned row, unsigned char c)
{
   uint32_t *dst = cell(col, row);
   unsigned x, y;
   for (y = 0; y < CELL_H; y++)
   {
      unsigned fy = y / SCALE;
      for (x = 0; x < CELL_W / 2; x++)
      {
         unsigned bit = x + fy * FONT_WIDTH;
         int on = fy < FONT_HEIGHT && x < FONT_WIDTH
            && (bitmap_bin[FONT_OFFSET(c) + (bit >> 3)] & (1 << (bit & 7)));
         dst[x] = on ? FG : BG;
      }
      dst += fb_words;
   }
}

static void scroll(void)
{
   uint32_t *top  = cell(0, 0);
   size_t    line = (size_t)CELL_H * fb_words;
   unsigned  i;
   memmove(top, top + line, (rows - 1) * line * 4);
   for (i = 0; i < line; i++)
      top[(rows - 1) * line + i] = BG;
}

void gk_console_write(const char *s, size_t len)
{
   size_t i;
   if (!fb || !cols || !rows)
      return;
   for (i = 0; i < len; i++)
   {
      unsigned char c = (unsigned char)s[i];
      if (c == '\n' || cx >= cols)
      {
         cx = 0;
         if (++cy >= rows)
         {
            scroll();
            cy = rows - 1;
         }
         if (c == '\n')
            continue;
      }
      if (c == '\r')
      {
         cx = 0;
         continue;
      }
      glyph(cx++, cy, c);
   }
   gk_dcache_flush(fb, (size_t)fb_words * 4 * fb_lines);
}

void gk_vi_show_now(void *xfb);

void gk_console_show_now(void)
{
   if (fb)
      gk_vi_show_now(fb);
}
