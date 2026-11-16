#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfx/video_driver.h"

typedef struct repack_texture
{
   uint8_t *bits;
   unsigned pitch;
   bool locked;
} *LPDIRECT3DTEXTURE9;
typedef struct { void *pBits; int Pitch; } D3DLOCKED_RECT;
typedef struct
{
   LPDIRECT3DTEXTURE9 tex, frame_tex;
   const uint8_t *frame;
   uint8_t *bits;
   unsigned pitch, dims, tex_dims;
} d3d9_sw_fb_t;
#define D3DLOCK_NOSYSLOCK 0
#ifndef FAILED
#define FAILED(result) ((result) < 0)
#endif
static unsigned allocations;
static void *repack_alloc(size_t size)
{
   (void)size;
   allocations++;
   return NULL;
}
static void d3d9_tex_unlock(LPDIRECT3DTEXTURE9 tex, unsigned level, bool wrote)
{
   (void)level; (void)wrote;
   assert(tex->locked);
   tex->locked = false;
}
static int d3d9_tex_lock(LPDIRECT3DTEXTURE9 tex, unsigned level,
      D3DLOCKED_RECT *rect, const void *region, unsigned flags)
{
   (void)level; (void)region; (void)flags;
   assert(!tex->locked);
   tex->locked = true;
   rect->pBits = tex->bits;
   rect->Pitch = (int)tex->pitch;
   return 0;
}
static void d3d9_sw_fb_drop(d3d9_sw_fb_t *fb)
{
   d3d9_tex_unlock(fb->tex, 0, false);
   fb->tex = NULL;
}
#define malloc repack_alloc
#include "settle.inc"
#undef malloc

static uint8_t pixels[8192], snapshot[8192], expected[8192];
static uint32_t seed = 1;
static unsigned next_value(void)
{
   seed = seed * 1664525u + 1013904223u;
   return seed;
}
static void check(unsigned bpp, unsigned width, unsigned height,
      unsigned pitch, unsigned dst_pitch, unsigned off)
{
   struct repack_texture texture;
   d3d9_sw_fb_t fb;
   unsigned i, y, row = width * bpp, tex_row = 32 * bpp;
   for (i = 0; i < sizeof(pixels); i++)
      pixels[i] = (uint8_t)(i * 37 + (i >> 8) * 19);
   memcpy(snapshot, pixels, sizeof(pixels));
   memcpy(expected, pixels, sizeof(pixels));
   for (y = 0; y < height; y++)
   {
      memcpy(expected + (size_t)y * dst_pitch,
            snapshot + off + (size_t)y * pitch, row);
      memset(expected + (size_t)y * dst_pitch + row, 0, tex_row - row);
   }
   for (y = height; y < 32; y++)
      memset(expected + (size_t)y * dst_pitch, 0, tex_row);
   memset(&fb, 0, sizeof(fb));
   texture.bits = pixels; texture.pitch = dst_pitch; texture.locked = true;
   fb.tex = &texture; fb.bits = pixels; fb.pitch = dst_pitch;
   fb.tex_dims = fb.dims = VIDEO_SCALE_PACK(32, 32);
   d3d9_sw_fb_settle(&fb, pixels + off, width, height, pitch, bpp, 0);
   assert(!allocations);
   assert(!texture.locked && !fb.tex && fb.frame_tex == &texture);
   assert(!memcmp(pixels, expected, sizeof(pixels)));
}
static void steady(unsigned bpp)
{
   struct repack_texture texture;
   d3d9_sw_fb_t fb;
   unsigned invalid;
   memset(pixels, 0x5a, sizeof(pixels));
   memcpy(snapshot, pixels, sizeof(pixels));
   memset(&fb, 0, sizeof(fb));
   texture.bits = pixels; texture.pitch = 32 * bpp + 16;
   fb.bits = pixels; fb.pitch = texture.pitch;
   fb.tex_dims = VIDEO_SCALE_PACK(32, 32);
   for (invalid = 0; invalid < 4; invalid++)
   {
      unsigned width = invalid == 1 ? 33 : 17;
      unsigned height = invalid == 2 ? 0 : 11;
      unsigned pitch = invalid == 3 ? 1 : texture.pitch;
      texture.locked = true; fb.tex = &texture; fb.frame_tex = NULL;
      fb.dims = VIDEO_SCALE_PACK(width, height);
      d3d9_sw_fb_settle(&fb, pixels, width, height, pitch, bpp, fb.dims);
      assert(!texture.locked && !fb.tex);
      assert(fb.frame_tex == (invalid ? NULL : &texture));
      assert(!memcmp(pixels, snapshot, sizeof(pixels)));
   }
}
#ifdef TEST_REPACK_ROWS
static void large_rows(void)
{
   static uint8_t data[32768], saved[32768], want[32768];
   static const unsigned widths[] = {2047, 2048, 2049, 4080};
   static const unsigned offsets[] = {0, 1, 15, 31};
   unsigned i, w, o, y;
   for (w = 0; w < sizeof(widths) / sizeof(widths[0]); w++)
      for (o = 0; o < sizeof(offsets) / sizeof(offsets[0]); o++)
      {
         for (i = 0; i < sizeof(data); i++)
            data[i] = (uint8_t)(i * 37 + (i >> 8) * 19);
         memcpy(saved, data, sizeof(data));
         memcpy(want, data, sizeof(data));
         for (y = 0; y < 8; y++)
            memcpy(want + y * 4096, saved + offsets[o] + y * 4092, widths[w]);
         d3d9_sw_fb_repack_rows(data, 4096, data + offsets[o], 4092,
               widths[w], 8, offsets[o]);
         assert(!allocations && !memcmp(data, want, sizeof(data)));
      }
}
#endif
int main(void)
{
   unsigned i, bpp;
   (void)repack_alloc;
#ifdef TEST_REPACK_ROWS
   large_rows();
#endif
   /* All-backward, mixed-direction, all-forward, and exact crossings. */
   for (bpp = 2; bpp <= 4; bpp += 2)
   {
      unsigned off;
      steady(bpp);
      for (off = 0; off < 1024; off++)
         check(bpp, 13, 16, 16 * bpp, 32 * bpp, off);
      for (i = 0; i < 20000; i++)
      {
         unsigned width = 1 + next_value() % 32;
         unsigned height = 1 + next_value() % 32;
         unsigned dst_pitch = 32 * bpp + next_value() % 65;
         unsigned pitch = width * bpp + next_value() % (dst_pitch + 1);
         size_t used = (size_t)(height - 1) * pitch + width * bpp;
         unsigned off;
         if (used > 32 * dst_pitch)
            continue;
         off = next_value() % (32 * dst_pitch - (unsigned)used + 1);
         check(bpp, width, height, pitch, dst_pitch, off);
      }
   }
   puts("PASS: 16/32-bit overlapping loans, crossover directions, padding and zero allocations");
   return 0;
}
