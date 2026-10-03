/* rvl/ir.c: the pointer from camera dots. */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../rvl/ir.h"

static unsigned failures;

#define CHECK(cond, what) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s (x %d y %d dots %u valid %u)\n", what, x, y, dots, \
               valid); \
         failures++; \
      } \
   } while (0)

static int16_t x, y;
static uint8_t dots, valid;

/* Up to four dots, (x, y) pairs; 1023 for none. */
static void pack(uint8_t *b, const int *d)
{
   unsigned i;
   for (i = 0; i < 2; i++)
   {
      int x0 = d[i * 4], y0 = d[i * 4 + 1], x1 = d[i * 4 + 2],
          y1 = d[i * 4 + 3];
      uint8_t *q = b + i * 5;
      q[0] = (uint8_t)x0;
      q[1] = (uint8_t)y0;
      q[2] = (uint8_t)(((y0 >> 8) << 6) | ((x0 >> 8) << 4)
            | ((y1 >> 8) << 2) | (x1 >> 8));
      q[3] = (uint8_t)x1;
      q[4] = (uint8_t)y1;
   }
}

static void run(ir_track *t, int x0, int y0, int x1, int y1)
{
   int d[8];
   uint8_t b[10];
   d[0] = x0; d[1] = y0; d[2] = x1; d[3] = y1;
   d[4] = d[5] = d[6] = d[7] = 1023;
   pack(b, d);
   ir_pointer(t, b, 1, &x, &y, &dots, &valid);
}

/* The two dots of a bar 100 pixels wide centred on (cx, cy), rolled by
 * deg about the image's centre. */
static void bar(ir_track *t, double cx, double cy, double deg, int drop)
{
   double r = deg * 3.14159265358979 / 180.0, c = cos(r), s = sin(r);
   double p[2][2];
   int i;
   p[0][0] = cx - 50; p[0][1] = cy;
   p[1][0] = cx + 50; p[1][1] = cy;
   for (i = 0; i < 2; i++)
   {
      double dx = p[i][0] - 512, dy = p[i][1] - 384;
      p[i][0] = 512 + c * dx - s * dy;
      p[i][1] = 384 + s * dx + c * dy;
   }
   run(t,
         drop == 0 ? 1023 : (int)floor(p[0][0] + 0.5),
         drop == 0 ? 1023 : (int)floor(p[0][1] + 0.5),
         drop == 1 ? 1023 : (int)floor(p[1][0] + 0.5),
         drop == 1 ? 1023 : (int)floor(p[1][1] + 0.5));
}

int main(void)
{
   ir_track t;
   int16_t x0, y0;
   memset(&t, 0, sizeof(t));

   /* With the bar on top, aimed at the screen's centre it shows 68
    * pixels below the image's. */
   bar(&t, 512, 384 + 68, 0, -1);
   CHECK(dots == 2 && valid && abs(x) < 100 && abs(y) < 100, "centre");
   bar(&t, 512 - 194, 384 + 68, 0, -1);
   CHECK(valid && x > 16000 && x < 17000 && abs(y) < 100,
         "bar left in the image: pointing right, half way");
   bar(&t, 512, 384 + 68 - 167, 0, -1);
   CHECK(valid && abs(x) < 100 && y < -16000 && y > -17000,
         "bar high in the image: pointing up, half way");
   bar(&t, 512 + 400, 384, 0, -1);
   CHECK(!valid && x == -32767, "past the left edge: held there, off");

   bar(&t, 512 - 194, 384 + 68, 0, -1);
   x0 = x;
   y0 = y;
   bar(&t, 512 - 194, 384 + 68, 30, -1);
   CHECK(abs(x - x0) < 200 && abs(y - y0) < 200, "rolled 30 degrees: same");
   bar(&t, 512 - 194, 384 + 68, -40, -1);
   CHECK(abs(x - x0) < 200 && abs(y - y0) < 200, "rolled -40 degrees: same");

   bar(&t, 512 - 194, 384 + 68, 0, -1);
   bar(&t, 512 - 194, 384 + 68, 0, 1);
   CHECK(dots == 1 && valid && abs(x - x0) < 100 && abs(y - y0) < 100,
         "right dot gone: from the left one");
   bar(&t, 512 - 184, 384 + 68, 0, 0);
   CHECK(dots == 1 && valid && x < x0 && x > x0 - 1000,
         "then the left one gone and the bar moved: from the right one");

   run(&t, 1023, 1023, 1023, 1023);
   CHECK(!dots && !valid, "no dots");
   bar(&t, 512, 384 + 68, 0, 1);
   CHECK(dots == 1 && !valid, "one dot with nothing before: no pointer");

   printf("ir: %s\n", failures ? "FAILED" : "ok");
   return failures ? 1 : 0;
}
