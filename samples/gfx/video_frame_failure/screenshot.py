#!/usr/bin/env python3
"""Exercise production screenshot snapshot, rotation and ownership code."""
from pathlib import Path
import os
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
source = Path(os.environ.get('SCREENSHOT_SOURCE', root / 'tasks/task_screenshot.c')).read_text()
start = source.find('static INLINE void screenshot_copy_pixel(')
if start < 0:
    start = source.index('static void screenshot_rotate(')
rotate = source[start:source.index('\n/* Take frame bottom-up. */', start)]
start = source.index('struct ss_raw_copy\n')
snapshot = source[start:source.index('\nstatic bool take_screenshot_choice(', start)]
program = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <boolean.h>
#include <retro_inline.h>
#include <libretro.h>
#include "gfx/video_defines.h"
typedef struct { int unused; } video_driver_state_t;
typedef struct { unsigned rotation; } rarch_system_info_t;
typedef struct { rarch_system_info_t system; } runloop_state_t;
typedef void (*retro_task_callback_t)(void*, void*, void*, const char*);
struct rpng_hdr_metadata;
static runloop_state_t runloop;
static runloop_state_t *runloop_state_get_ptr(void) { return &runloop; }
static void video_driver_cached_frame_read(void*, void (*)(void*, const void*, unsigned, size_t));
static bool screenshot_dump(const char*, const char*, const void*, unsigned, int,
      bool, void*, bool, uint32_t, bool, bool, unsigned,
      const struct rpng_hdr_metadata*, unsigned, retro_task_callback_t, void*);
static void *snapshot_malloc(size_t);
static void *snapshot_calloc(size_t, size_t);
static void snapshot_free(void*);
#define malloc snapshot_malloc
#define calloc snapshot_calloc
#define free snapshot_free
''' + rotate + snapshot + r'''
#undef malloc
#undef calloc
#undef free
static unsigned failures, calls, dump_calls;
static int budget = -1, dump_ok = 1, poison = 1;
static size_t live_bytes, peak_bytes;
static struct { void *ptr; size_t size; } allocations[4];
static uint8_t *input;
static size_t input_size, input_pitch;
static unsigned input_dims, output_dims;
static unsigned format_override = RETRO_PIXEL_FORMAT_UNKNOWN;
static int output_pitch;
static void *owned;
static volatile uint32_t checksum;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); failures++; } } while (0)
static void *snapshot_malloc(size_t size)
{
   unsigned i;
   void *p;
   calls++;
   if (budget == 0) return NULL;
   if (budget > 0) budget--;
   p = malloc(size);
   assert(p);
   for (i = 0; i < 4; i++)
      if (!allocations[i].ptr)
      {
         allocations[i].ptr = p;
         allocations[i].size = size;
         live_bytes += size;
         if (live_bytes > peak_bytes) peak_bytes = live_bytes;
         return p;
      }
   abort();
   return NULL;
}
static void *snapshot_calloc(size_t count, size_t size)
{
   void *p = snapshot_malloc(count * size);
   if (p) memset(p, 0, count * size);
   return p;
}
static void snapshot_free(void *p)
{
   unsigned i;
   if (!p) return;
   for (i = 0; i < 4; i++)
      if (allocations[i].ptr == p)
      {
         live_bytes -= allocations[i].size;
         allocations[i].ptr = NULL;
         free(p);
         return;
      }
   abort();
}
static void video_driver_cached_frame_read(void *ud,
      void (*cb)(void*, const void*, unsigned, size_t))
{
   cb(ud, input, input_dims, input_pitch);
   if (poison && input) memset(input, 0xee, input_size);
}
static bool screenshot_dump(const char *dir, const char *name, const void *frame,
      unsigned dims, int pitch, bool bgr, void *userbuf, bool savestate,
      uint32_t flags, bool fullpath, bool threaded, unsigned fmt,
      const struct rpng_hdr_metadata *hdr, unsigned frame_max,
      retro_task_callback_t cb, void *ud)
{
   dump_calls++;
   CHECK(threaded, "snapshot lost deferred-task flag");
   CHECK(frame == (uint8_t*)userbuf + (VIDEO_SCALE_H(dims) - 1) * (size_t)-pitch,
         "snapshot does not point at its last row");
   if (!dump_ok) return false;
   owned = userbuf;
   output_dims = dims;
   output_pitch = pitch;
   return true;
}
static uint32_t pixel(unsigned x, unsigned y) { return 0xa5a50000u ^ (y * 257 + x); }
static void reset(void)
{
   CHECK(!live_bytes && !owned, "previous snapshot leaked");
   calls = dump_calls = 0;
   peak_bytes = 0;
}
static bool capture(unsigned bpp)
{
   return take_screenshot_raw(NULL, "", "", false, 0, false, true,
         format_override != RETRO_PIXEL_FORMAT_UNKNOWN ? format_override
            : (bpp == 4 ? RETRO_PIXEL_FORMAT_XRGB8888 : RETRO_PIXEL_FORMAT_RGB565),
         0, NULL, NULL);
}
static void lane(unsigned w, unsigned h, unsigned bpp, unsigned padding,
      unsigned rotation, int fail_alloc, int fail_dump, int full_last_row)
{
   unsigned x, y, ow = w, oh = h;
   bool ret;
   reset();
   input_pitch = (size_t)w * bpp + padding;
   input_size = input_pitch * (h - 1) + w * bpp;
   if (full_last_row) input_size = input_pitch * h;
   input = (uint8_t*)malloc(input_size);
   assert(input);
   memset(input, 0xa5, input_size);
   for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
      {
         uint32_t p = pixel(x, y);
         uint16_t p16 = (uint16_t)p;
         memcpy(input + y * input_pitch + x * bpp, bpp == 4 ? (void*)&p : (void*)&p16, bpp);
      }
   input_dims = VIDEO_SCALE_PACK(w, h);
   runloop.system.rotation = rotation;
   budget = fail_alloc ? 0 : (full_last_row ? -1 : 1);
   dump_ok = !fail_dump;
   ret = capture(bpp);
   budget = -1;
   CHECK(ret == (!fail_alloc && !fail_dump), "snapshot return/ownership mismatch");
   if (fail_alloc || fail_dump)
      CHECK(!live_bytes && !owned, "failed snapshot leaked");
   else if (ret)
   {
      if (rotation == VIDEO_ROTATION_90_DEG || rotation == VIDEO_ROTATION_270_DEG)
      { ow = h; oh = w; }
      CHECK(calls == 1, "snapshot allocated a second frame");
      CHECK(live_bytes == (size_t)w * h * bpp, "snapshot retained row padding");
      CHECK(output_dims == VIDEO_SCALE_PACK(ow, oh), "snapshot rotation dimensions");
      CHECK(output_pitch == -(int)(ow * bpp), "snapshot rotation stride");
      if (output_dims == VIDEO_SCALE_PACK(ow, oh) && output_pitch == -(int)(ow * bpp))
         for (y = 0; y < oh; y++)
            for (x = 0; x < ow; x++)
            {
               unsigned sx = x, sy = y;
               uint32_t got = 0, want;
               if (rotation == VIDEO_ROTATION_90_DEG) { sx = w - 1 - y; sy = x; }
               if (rotation == VIDEO_ROTATION_180_DEG) { sx = w - 1 - x; sy = h - 1 - y; }
               if (rotation == VIDEO_ROTATION_270_DEG) { sx = y; sy = h - 1 - x; }
               want = pixel(sx, sy);
               if (bpp == 4) memcpy(&got, (uint8_t*)owned + ((size_t)y * ow + x) * 4, 4);
               else { uint16_t p; memcpy(&p, (uint8_t*)owned + ((size_t)y * ow + x) * 2, 2); got = p; want &= 0xffff; }
               CHECK(got == want, "snapshot pixels/borrow lifetime");
            }
      snapshot_free(owned);
      owned = NULL;
   }
   free(input);
   input = NULL;
}
static void invalid(void)
{
   uint8_t data[8];
   unsigned i;
   reset();
   input = data;
   input_size = sizeof(data);
   poison = 0;
   for (i = 0; i < 4; i++)
   {
      input_dims = VIDEO_SCALE_PACK(i == 0 ? 0 : 2, 2);
      input_pitch = i == 1 ? 0 : (i == 2 ? 7 : (size_t)-1);
      CHECK(!capture(4) && !calls && !dump_calls, "invalid snapshot layout accepted");
   }
   if (sizeof(size_t) == 4)
   {
      input_dims = VIDEO_SCALE_PACK(65535, 65535);
      input_pitch = 65535 * 4;
      CHECK(!capture(4) && !calls && !dump_calls, "snapshot allocation size overflow");
   }
   input_dims = VIDEO_SCALE_PACK(2, 2);
   input = NULL;
   input_pitch = 8;
   CHECK(!capture(4) && !calls && !dump_calls, "missing snapshot accepted");
   input = data;
   format_override = RETRO_PIXEL_FORMAT_SCRGB_RGBA16F;
   CHECK(!capture(4) && !calls && !dump_calls, "unsupported raw format accepted");
   format_override = RETRO_PIXEL_FORMAT_UNKNOWN;
   input = NULL;
   poison = 1;
}
static void bench(void)
{
   unsigned bpp, r, i;
   size_t j;
   poison = 0;
   for (bpp = 2; bpp <= 4; bpp += 2)
   {
      input_dims = VIDEO_SCALE_PACK(3840, 2160);
      input_pitch = 3840 * bpp;
      input_size = input_pitch * 2160;
      input = (uint8_t*)calloc(1, input_size);
      assert(input);
      for (j = 0; j < input_size; j++)
         input[j] = (uint8_t)(j * 37 + j / 257);
      for (r = 0; r < 4; r++)
      {
         clock_t begin = clock();
         size_t peak = 0;
         unsigned count = 0;
         runloop.system.rotation = r;
         for (i = 0; i < 5; i++)
         {
            reset();
            assert(capture(bpp));
            checksum += ((uint8_t*)owned)[0] + ((uint8_t*)owned)[input_size - 1];
            peak = peak_bytes; count = calls;
            snapshot_free(owned); owned = NULL;
         }
         printf("4K bpp=%u rotation=%u: %.3f ms, peak=%lu bytes, allocations=%u\n", bpp, r,
               1000.0 * (clock() - begin) / CLOCKS_PER_SEC / 5, (unsigned long)peak, count);
      }
      free(input); input = NULL;
   }
}
int main(int argc, char **argv)
{
   static const unsigned sizes[][2] = {
      {1,1},{1,5},{5,1},{3,2},{17,9},{33,19},{64,48},
      {31,32},{32,33},{63,65},{129,67}
   };
   unsigned s, bpp, r, p;
   if (argc > 1 && !strcmp(argv[1], "--bench")) { bench(); return 0; }
   if (argc > 1) { lane(3, 2, 2, 16, 1, 0, 0, 1); return failures ? 1 : 0; }
   for (s = 0; s < sizeof(sizes)/sizeof(sizes[0]); s++)
      for (bpp = 2; bpp <= 4; bpp += 2)
         for (r = 0; r < 4; r++)
            for (p = 0; p < 4; p++)
               lane(sizes[s][0], sizes[s][1], bpp, p == 3 ? sizes[s][0]*bpp : p*4, r, 0, 0, 0);
   lane(3, 5, 4, 8, 2, 1, 0, 0);
   lane(3, 5, 2, 8, 3, 0, 1, 0);
   lane(3, 5, 4, 8, 99, 0, 0, 0);
   format_override = RETRO_PIXEL_FORMAT_0RGB1555;
   lane(3, 5, 2, 16, 1, 0, 0, 0);
   format_override = RETRO_PIXEL_FORMAT_XRGB2101010;
   lane(3, 5, 4, 16, 2, 0, 0, 0);
   format_override = RETRO_PIXEL_FORMAT_HDR10_2101010;
   lane(3, 5, 4, 16, 3, 0, 0, 0);
   format_override = RETRO_PIXEL_FORMAT_UNKNOWN;
   invalid();
   if (failures) return 1;
   puts("PASS: screenshot pixels, padded strides, single allocation, lifetime and failure ownership");
   return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='screenshot-snapshot-') as tmp:
    tmp = Path(tmp)
    src = tmp / 'test.c'
    src.write_text(program)
    exe = tmp / 'test.exe'
    flags = ['-O2', '-std=gnu89', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
             '-Wno-unused-function', '-Wdeclaration-after-statement',
             '-I'+str(root), '-I'+str(root/'libretro-common/include')]
    sanitizer = os.environ.get('SANITIZER')
    if sanitizer:
        flags += ['-g', '-fsanitize='+sanitizer, '-fno-omit-frame-pointer']
    subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + flags + [str(src), '-o', str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)] + sys.argv[1:], check=True)
