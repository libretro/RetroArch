#ifndef GFX_MENU_BLOOM_H
#define GFX_MENU_BLOOM_H

#include <stdint.h>
#include <retro_inline.h>
#include <boolean.h>
#include "../video_defines.h"

static INLINE unsigned menu_bloom_dims(unsigned dims)
{
   unsigned w = VIDEO_SCALE_W(dims);
   unsigned h = VIDEO_SCALE_H(dims);
   if (!w || !h)
      return 0;
   w = w > 1920 ? 960 : (w + 1) / 2;
   h = (unsigned)(((uint64_t)VIDEO_SCALE_H(dims) * w
            + VIDEO_SCALE_W(dims) - 1) / VIDEO_SCALE_W(dims));
   if (h > 540)
   {
      w = w * 540 / h;
      if (!w)
         w = 1;
      h = 540;
   }
   return VIDEO_SCALE_PACK(w, h);
}

#define MENU_BLOOM_PASSES 6
/* A pass's dst when it adds onto the output instead of an image */
#define MENU_BLOOM_OUTPUT 2

typedef struct menu_bloom_pass
{
   float params[4]; /* step x, step y, threshold, gain */
   uint8_t src;     /* the small image read: 0 or 1; the blur's copy, 2 */
   uint8_t dst;     /* the small image written, or MENU_BLOOM_OUTPUT */
   uint8_t add;     /* blended onto what dst holds, else replacing it */
} menu_bloom_pass_t;

/* The passes after the ribbon is drawn into image 0, for images of
 * dims; returns how many. Float images keep values above 1, so the
 * wide blur is added onto the narrow one at the small size and the
 * output takes one pass: 0.45 (b1 + 2 b2) is 0.45 b1 + 0.9 b2. */
static INLINE unsigned menu_bloom_plan(menu_bloom_pass_t *pass,
      unsigned dims, bool floating)
{
   static const struct
   {
      uint8_t src, dst, add, axis;
      float radius, threshold, gain;
   } two[MENU_BLOOM_PASSES] = {
      { 0, 1,                 0, 0, 1.2f, 0.12f, 1.00f },
      { 1, 0,                 0, 1, 1.2f, 0.00f, 1.00f },
      { 0, MENU_BLOOM_OUTPUT, 1, 2, 0.0f, 0.00f, 0.45f },
      { 0, 1,                 0, 0, 5.0f, 0.00f, 1.00f },
      { 1, 0,                 0, 1, 5.0f, 0.00f, 1.00f },
      { 0, MENU_BLOOM_OUTPUT, 1, 2, 0.0f, 0.00f, 0.90f },
   }, one[MENU_BLOOM_PASSES - 1] = {
      { 0, 1,                 0, 0, 1.2f, 0.12f, 1.00f },
      { 1, 0,                 0, 1, 1.2f, 0.00f, 1.00f },
      { 0, 1,                 0, 0, 5.0f, 0.00f, 1.00f },
      { 1, 0,                 1, 1, 5.0f, 0.00f, 2.00f },
      { 0, MENU_BLOOM_OUTPUT, 1, 2, 0.0f, 0.00f, 0.45f },
   };
   unsigned i, count = floating ? MENU_BLOOM_PASSES - 1 : MENU_BLOOM_PASSES;
   for (i = 0; i < count; i++)
   {
      unsigned axis   = floating ? one[i].axis : two[i].axis;
      float radius    = floating ? one[i].radius : two[i].radius;
      pass[i].src       = floating ? one[i].src : two[i].src;
      pass[i].dst       = floating ? one[i].dst : two[i].dst;
      pass[i].add       = floating ? one[i].add : two[i].add;
      pass[i].params[0] = axis == 0 ? radius / VIDEO_SCALE_W(dims) : 0.0f;
      pass[i].params[1] = axis == 1 ? radius / VIDEO_SCALE_H(dims) : 0.0f;
      pass[i].params[2] = floating ? one[i].threshold : two[i].threshold;
      pass[i].params[3] = floating ? one[i].gain : two[i].gain;
   }
   return count;
}

/* The menu's backdrop blur. The rect is copied at twice the small
 * size (MENU_BLUR_COPY_DIMS), which makes the copy a 2x2 average up to
 * 1920 x 1080 rather than a sample of a quarter of the pixels, losing
 * one-pixel lines; the passes blur it down into two images of the
 * small size, a quarter of the rect and at most 480 x 270, so they
 * cost the same whatever the output. */
#define MENU_BLUR_PASSES 6
/* A blur pass's src when it reads the copy */
#define MENU_BLUR_COPY   2
#define MENU_BLUR_COPY_DIMS(dims) VIDEO_SCALE_PACK( \
      VIDEO_SCALE_W(dims) * 2, VIDEO_SCALE_H(dims) * 2)

static INLINE unsigned menu_blur_dims(unsigned dims)
{
   unsigned w = (VIDEO_SCALE_W(dims) + 3) / 4;
   unsigned h = (VIDEO_SCALE_H(dims) + 3) / 4;
   if (w > 480)
   {
      h = (unsigned)(((uint64_t)h * 480 + w - 1) / w);
      w = 480;
   }
   if (h > 270)
   {
      w = (unsigned)(((uint64_t)w * 270 + h - 1) / h);
      h = 270;
   }
   return VIDEO_SCALE_PACK(w ? w : 1, h ? h : 1);
}

/* The blur's passes for small images of dims, ending in image 0:
 * three rounds of horizontal then vertical, each twice as wide as the
 * last so its taps land on what the one before smoothed; the first
 * reads the copy and halves it again. Steps are in texture coordinates,
 * threshold 0 and gain 1, nothing blended. */
static INLINE void menu_blur_plan(menu_bloom_pass_t *pass, unsigned dims)
{
   static const struct { uint8_t src, dst; float x, y; }
   round[MENU_BLUR_PASSES] = {
      { MENU_BLUR_COPY, 1, 0.5f, 0.0f },
      { 1,              0, 0.0f, 1.0f },
      { 0,              1, 2.0f, 0.0f },
      { 1,              0, 0.0f, 2.0f },
      { 0,              1, 4.0f, 0.0f },
      { 1,              0, 0.0f, 4.0f },
   };
   unsigned i;
   for (i = 0; i < MENU_BLUR_PASSES; i++)
   {
      pass[i].src       = round[i].src;
      pass[i].dst       = round[i].dst;
      pass[i].add       = 0;
      pass[i].params[0] = round[i].x / VIDEO_SCALE_W(dims);
      pass[i].params[1] = round[i].y / VIDEO_SCALE_H(dims);
      pass[i].params[2] = 0.0f;
      pass[i].params[3] = 1.0f;
   }
}

#endif
