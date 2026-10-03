/* viewport_pack_test.c -- the size-pair word video_viewport_t travels in.
 *
 * video_viewport_t carries the drawn area and the window that holds it
 * as one word each, width in the high half and height in the low. Every
 * driver reads those two halves back, so the claims below are what keeps
 * a viewport the shape its producer gave it:
 *
 *   1. Round trip: an asymmetric pair comes back the way it went in, on
 *      both words, so a transposed axis fails here rather than showing
 *      up as a viewport rotated 90 degrees on someone's screen.
 *   2. Independence: setting one axis on its own leaves the other
 *      standing. VIDEO_SCALE_PUT_W/H are the only way a caller writes a
 *      single axis, and an axis that overwrote its neighbour would blank
 *      a viewport whenever a driver set width and height apart.
 *   3. Clamping: neither axis can carry into the other. A size past
 *      65535 saturates rather than wrapping, which is what makes the
 *      two halves safe to sit in one word at all.
 *   4. Separation: the origin, the drawn area and the full window are
 *      three different words, and writing one does not move another.
 *   5. Sign: the origin's halves are signed, since integer scaling
 *      overscans and pushes an axis negative, and a sign bit must not
 *      reach the other axis.
 *
 * Header-only: the pack layout is all macros, so the harness needs
 * nothing from the tree but the header that declares them. */

#include <stdio.h>
#include <stdlib.h>
#include <boolean.h>
#include <retro_common_api.h>
#include "../../../gfx/video_defines.h"

static unsigned failures;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         printf("[fail] "); \
         printf(__VA_ARGS__); \
         printf("\n"); \
         failures++; \
      } \
   } while (0)

/* 1. an asymmetric pair survives the trip in both directions */
static void lane_round_trip(void)
{
   static const unsigned pairs[][2] = {
      {  640,  480 }, {  480,  640 }, { 1920, 1080 }, { 1080, 1920 },
      { 2560, 1792 }, {    1,    1 }, {    1,  720 }, {  720,    1 },
      {    0,    0 }, {    0,  480 }, {  640,    0 }, { 65535, 65535 }
   };
   size_t i;

   for (i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++)
   {
      unsigned w = pairs[i][0];
      unsigned h = pairs[i][1];
      unsigned d = VIDEO_SCALE_PACK(w, h);

      CHECK(VIDEO_SCALE_W(d) == w,
            "round trip: %ux%u read back a width of %u", w, h,
            VIDEO_SCALE_W(d));
      CHECK(VIDEO_SCALE_H(d) == h,
            "round trip: %ux%u read back a height of %u", w, h,
            VIDEO_SCALE_H(d));
   }

   /* the two axes are not interchangeable: a transposed pack differs */
   CHECK(VIDEO_SCALE_PACK(640, 480) != VIDEO_SCALE_PACK(480, 640),
         "round trip: 640x480 and 480x640 pack to the same word");
}

/* 2. one axis at a time, the other left standing */
static void lane_independence(void)
{
   unsigned d = VIDEO_SCALE_PACK(640, 480);

   VIDEO_SCALE_PUT_W(d, 800);
   CHECK(VIDEO_SCALE_W(d) == 800,
         "independence: a width put as 800 read back %u",
         VIDEO_SCALE_W(d));
   CHECK(VIDEO_SCALE_H(d) == 480,
         "independence: putting a width moved the height to %u",
         VIDEO_SCALE_H(d));

   VIDEO_SCALE_PUT_H(d, 600);
   CHECK(VIDEO_SCALE_H(d) == 600,
         "independence: a height put as 600 read back %u",
         VIDEO_SCALE_H(d));
   CHECK(VIDEO_SCALE_W(d) == 800,
         "independence: putting a height moved the width to %u",
         VIDEO_SCALE_W(d));

   /* a driver that sets the axes apart, starting from nothing */
   d = 0;
   VIDEO_SCALE_PUT_H(d, 272);
   VIDEO_SCALE_PUT_W(d, 480);
   CHECK(VIDEO_SCALE_W(d) == 480 && VIDEO_SCALE_H(d) == 272,
         "independence: axes set apart came out %ux%u, not 480x272",
         VIDEO_SCALE_W(d), VIDEO_SCALE_H(d));

   /* putting zero is a size, not a no-op */
   VIDEO_SCALE_PUT_W(d, 0);
   CHECK(VIDEO_SCALE_W(d) == 0 && VIDEO_SCALE_H(d) == 272,
         "independence: a width put as 0 came out %ux%u, not 0x272",
         VIDEO_SCALE_W(d), VIDEO_SCALE_H(d));
}

/* 3. neither axis carries into the other */
static void lane_clamp(void)
{
   unsigned d = VIDEO_SCALE_PACK(70000, 80000);

   CHECK(VIDEO_SCALE_W(d) == 65535,
         "clamp: a width of 70000 read back %u", VIDEO_SCALE_W(d));
   CHECK(VIDEO_SCALE_H(d) == 65535,
         "clamp: a height of 80000 read back %u", VIDEO_SCALE_H(d));

   /* an oversized height must not reach the width's half */
   d = VIDEO_SCALE_PACK(640, 100000);
   CHECK(VIDEO_SCALE_W(d) == 640,
         "clamp: an oversized height moved the width to %u",
         VIDEO_SCALE_W(d));

   /* the same through the single-axis put */
   d = VIDEO_SCALE_PACK(640, 480);
   VIDEO_SCALE_PUT_H(d, 100000);
   CHECK(VIDEO_SCALE_W(d) == 640 && VIDEO_SCALE_H(d) == 65535,
         "clamp: an oversized height put came out %ux%u, not 640x65535",
         VIDEO_SCALE_W(d), VIDEO_SCALE_H(d));
}

/* 3b. the origin, which is signed: integer scaling overscans and pushes
 *     x or y negative, so both halves sign-extend on the way out and a
 *     sign bit never reaches the other axis */
static void lane_origin(void)
{
   static const int pairs[][2] = {
      {     0,     0 }, {     3,     5 }, {    -1,     0 }, {  0,  -1 },
      {   -64,   -48 }, {   -64,    48 }, {    64,   -48 }, { 19, 108 },
      {  -320,   200 }, { 32767, 32767 }, {-32768,-32768 }
   };
   size_t i;
   unsigned p;

   for (i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++)
   {
      int x = pairs[i][0];
      int y = pairs[i][1];
      p     = VIDEO_POS_PACK(x, y);

      CHECK(VIDEO_POS_X(p) == x,
            "origin: %d,%d read back an x of %d", x, y, VIDEO_POS_X(p));
      CHECK(VIDEO_POS_Y(p) == y,
            "origin: %d,%d read back a y of %d", x, y, VIDEO_POS_Y(p));
   }

   /* a negative y must not borrow into x */
   p = VIDEO_POS_PACK(5, -1);
   CHECK(VIDEO_POS_X(p) == 5,
         "origin: a y of -1 moved the x to %d", VIDEO_POS_X(p));

   /* the zero word is the origin, so a cleared viewport sits at 0,0 */
   p = 0;
   CHECK(VIDEO_POS_X(p) == 0 && VIDEO_POS_Y(p) == 0,
         "origin: a zero word read %d,%d, not 0,0",
         VIDEO_POS_X(p), VIDEO_POS_Y(p));

   /* one axis at a time */
   p = VIDEO_POS_PACK(10, 20);
   VIDEO_POS_PUT_X(p, -30);
   CHECK(VIDEO_POS_X(p) == -30 && VIDEO_POS_Y(p) == 20,
         "origin: an x put as -30 came out %d,%d, not -30,20",
         VIDEO_POS_X(p), VIDEO_POS_Y(p));
   VIDEO_POS_PUT_Y(p, -40);
   CHECK(VIDEO_POS_X(p) == -30 && VIDEO_POS_Y(p) == -40,
         "origin: a y put as -40 came out %d,%d, not -30,-40",
         VIDEO_POS_X(p), VIDEO_POS_Y(p));

   /* an offset past a half saturates rather than wrapping, which would
    * put the image on the opposite side of the display */
   p = VIDEO_POS_PACK(40000, -40000);
   CHECK(VIDEO_POS_X(p) == VIDEO_POS_MAX,
         "origin: an x of 40000 read back %d", VIDEO_POS_X(p));
   CHECK(VIDEO_POS_Y(p) == VIDEO_POS_MIN,
         "origin: a y of -40000 read back %d", VIDEO_POS_Y(p));
}

/* 4. the drawn area and the window that holds it are separate words */
static void lane_viewport(void)
{
   video_viewport_t vp;

   vp.pos       = VIDEO_POS_PACK(3, 5);
   vp.dims      = VIDEO_SCALE_PACK(640, 480);
   vp.full_dims = VIDEO_SCALE_PACK(1920, 1080);

   CHECK(VIDEO_SCALE_W(vp.dims) == 640 && VIDEO_SCALE_H(vp.dims) == 480,
         "viewport: the drawn area read %ux%u, not 640x480",
         VIDEO_SCALE_W(vp.dims), VIDEO_SCALE_H(vp.dims));
   CHECK(VIDEO_SCALE_W(vp.full_dims) == 1920
         && VIDEO_SCALE_H(vp.full_dims) == 1080,
         "viewport: the full size read %ux%u, not 1920x1080",
         VIDEO_SCALE_W(vp.full_dims), VIDEO_SCALE_H(vp.full_dims));

   VIDEO_SCALE_PUT_W(vp.dims, 800);
   CHECK(VIDEO_SCALE_W(vp.full_dims) == 1920
         && VIDEO_SCALE_H(vp.full_dims) == 1080,
         "viewport: writing the drawn area moved the full size to %ux%u",
         VIDEO_SCALE_W(vp.full_dims), VIDEO_SCALE_H(vp.full_dims));
   CHECK(VIDEO_POS_X(vp.pos) == 3 && VIDEO_POS_Y(vp.pos) == 5,
         "viewport: writing a size moved the origin to %d,%d", VIDEO_POS_X(vp.pos), VIDEO_POS_Y(vp.pos));
}

int main(void)
{
   lane_round_trip();
   lane_independence();
   lane_clamp();
   lane_origin();
   lane_viewport();

   if (failures)
   {
      printf("viewport_pack_test: %u failure(s)\n", failures);
      return 1;
   }

   printf("viewport_pack_test: all lanes pass\n");
   return 0;
}
