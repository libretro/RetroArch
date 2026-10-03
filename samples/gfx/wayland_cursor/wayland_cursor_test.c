/* input/common/wayland_cursor.h, the size and scale the Wayland context
 * loads its own cursor theme at when the compositor has no
 * cursor-shape-v1 to draw one: XCURSOR_SIZE honoured, and on a scaled
 * output - fractional ones included - a buffer at the next whole scale
 * up, shrunk to size by the compositor rather than drawn tiny. */

#include <stdio.h>

#include "input/common/wayland_cursor.h"

static unsigned failures;

#define CHECK_EQ(got, want, what) \
   do { \
      unsigned _g = (got), _w = (want); \
      if (_g != _w) \
      { \
         fprintf(stderr, "FAIL %s:%d: %s: got %u, want %u\n", \
               __FILE__, __LINE__, (what), _g, _w); \
         failures++; \
      } \
   } while (0)

int main(void)
{
   /* The size: XCURSOR_SIZE as the toolkits read it. */
   CHECK_EQ(wl_cursor_size(NULL), 24, "unset size");
   CHECK_EQ(wl_cursor_size(""),   24, "empty size");
   CHECK_EQ(wl_cursor_size("32"), 32, "XCURSOR_SIZE=32");
   CHECK_EQ(wl_cursor_size("48"), 48, "XCURSOR_SIZE=48");
   CHECK_EQ(wl_cursor_size("0"),  24, "zero size");
   CHECK_EQ(wl_cursor_size("-8"), 24, "negative size");
   CHECK_EQ(wl_cursor_size("32px"), 24, "trailing garbage");
   CHECK_EQ(wl_cursor_size("99999"), 24, "oversized");

   /* The scale: whole buffer scales as they are. */
   CHECK_EQ(wl_cursor_scale(false, 1, 120, true), 1, "scale 1");
   CHECK_EQ(wl_cursor_scale(false, 2, 120, true), 2, "buffer scale 2");
   CHECK_EQ(wl_cursor_scale(false, 3, 120, true), 3, "buffer scale 3");
   CHECK_EQ(wl_cursor_scale(false, 0, 120, true), 1, "buffer scale unset");

   /* Fractional: the next whole scale up, never down. */
   CHECK_EQ(wl_cursor_scale(true, 1, 120, true), 1, "fractional 100%");
   CHECK_EQ(wl_cursor_scale(true, 1, 150, true), 2, "fractional 125%");
   CHECK_EQ(wl_cursor_scale(true, 1, 180, true), 2, "fractional 150%");
   CHECK_EQ(wl_cursor_scale(true, 1, 240, true), 2, "fractional 200%");
   CHECK_EQ(wl_cursor_scale(true, 1, 270, true), 3, "fractional 225%");
   CHECK_EQ(wl_cursor_scale(true, 2, 150, true), 2,
         "fractional wins over the buffer scale");
   CHECK_EQ(wl_cursor_scale(true, 1, 0, true), 1, "fractional unset");

   /* A cursor surface that cannot take a buffer scale draws at 1. */
   CHECK_EQ(wl_cursor_scale(false, 2, 120, false), 1, "no buffer scale");
   CHECK_EQ(wl_cursor_scale(true, 1, 150, false), 1,
         "no buffer scale, fractional");

   /* The theme loads at size x scale: 24 at 125% is a 48 px buffer. */
   CHECK_EQ(wl_cursor_size(NULL) * wl_cursor_scale(true, 1, 150, true), 48,
         "load size at 125%");

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] wayland_cursor_test\n");
   return 0;
}
