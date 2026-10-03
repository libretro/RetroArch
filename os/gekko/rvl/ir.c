/* Wii: the remote's pointer, after WiiBrew's description of the IR
 * camera: 1024x768 pixels, the sensor bar's two LED clusters seen as
 * dots.
 *
 * The pointer is the dots' midpoint turned back by the roll the pair
 * shows, mirrored (the image moves against the remote) and scaled so
 * the screen's edges are where the remote turns about 12 degrees
 * sideways or 10 up or down from the centre.  With one dot seen, the
 * other is placed where it was last seen to be. */

#include <math.h>

#include "ir.h"

/* Camera pixels per screen half: sideways and up/down. */
#define IR_HALF_X       388.0f
#define IR_HALF_Y       334.0f
/* Camera pixels the bar sits from the screen's centre, seen from
 * about 2 m. */
#define IR_BAR_Y        68.0f

/* The basic format: two pairs of dots in 5 bytes each, 10-bit x and
 * y, all ones where there is no dot. */
void ir_pointer(ir_track *r, const uint8_t *p, int bar_on_top,
      int16_t *out_x, int16_t *out_y, uint8_t *dots, uint8_t *valid)
{
   float dot[4][2], mx, my, hx, hy, len, x, y;
   unsigned i, n = 0;
   for (i = 0; i < 2; i++)
   {
      const uint8_t *q = p + i * 5;
      unsigned x0 = q[0] | ((q[2] & 0x30) << 4);
      unsigned y0 = q[1] | ((q[2] & 0xc0) << 2);
      unsigned x1 = q[3] | ((q[2] & 0x03) << 8);
      unsigned y1 = q[4] | ((q[2] & 0x0c) << 6);
      if (y0 != 0x3ff)
      {
         dot[n][0]   = (float)x0;
         dot[n++][1] = (float)y0;
      }
      if (y1 != 0x3ff)
      {
         dot[n][0]   = (float)x1;
         dot[n++][1] = (float)y1;
      }
   }
   *dots = (uint8_t)n;
   if (n >= 2)
   {
      /* The right-hand dot first. */
      int k = dot[0][0] > dot[1][0] ? 0 : 1;
      r->half[0]  = (dot[k][0] - dot[!k][0]) * 0.5f;
      r->half[1]  = (dot[k][1] - dot[!k][1]) * 0.5f;
      r->mid[0]   = (dot[0][0] + dot[1][0]) * 0.5f;
      r->mid[1]   = (dot[0][1] + dot[1][1]) * 0.5f;
      r->have_bar = 1;
   }
   else if (n == 1 && r->have_bar)
   {
      /* Whichever end of the bar the dot is nearer to where it was. */
      float dr = (dot[0][0] - r->mid[0] - r->half[0])
               * (dot[0][0] - r->mid[0] - r->half[0])
               + (dot[0][1] - r->mid[1] - r->half[1])
               * (dot[0][1] - r->mid[1] - r->half[1]);
      float dl = (dot[0][0] - r->mid[0] + r->half[0])
               * (dot[0][0] - r->mid[0] + r->half[0])
               + (dot[0][1] - r->mid[1] + r->half[1])
               * (dot[0][1] - r->mid[1] + r->half[1]);
      float sgn = dr < dl ? -1.0f : 1.0f;
      r->mid[0] = dot[0][0] + sgn * r->half[0];
      r->mid[1] = dot[0][1] + sgn * r->half[1];
   }
   else
   {
      if (!n)
         r->have_bar = 0;
      *valid = 0;
      return;
   }

   /* Undo the roll: turn the midpoint about the image's centre by
    * the bar's angle. */
   hx  = r->half[0];
   hy  = r->half[1];
   len = (float)sqrt(hx * hx + hy * hy);
   mx  = r->mid[0] - 512.0f;
   my  = r->mid[1] - 384.0f;
   if (len > 1.0f)
   {
      float c = hx / len, s = hy / len;
      float rx = c * mx + s * my;
      float ry = c * my - s * mx;
      mx = rx;
      my = ry;
   }
   /* The bar is off the screen's centre: on top of it, it shows below
    * the image's centre when the remote points at the screen's. */
   my -= bar_on_top ? IR_BAR_Y : -IR_BAR_Y;
   x = -mx / IR_HALF_X;
   y = my / IR_HALF_Y;
   *valid = x >= -1.0f && x <= 1.0f && y >= -1.0f && y <= 1.0f;
   if (x < -1.0f) x = -1.0f; else if (x > 1.0f) x = 1.0f;
   if (y < -1.0f) y = -1.0f; else if (y > 1.0f) y = 1.0f;
   *out_x = (int16_t)(x * 32767.0f);
   *out_y = (int16_t)(y * 32767.0f);
}
