/* The 3DS software framebuffer lend, its shipping functions against a
 * stand-in linear heap: a frame is drawn in linear memory at a pitch
 * the GPU's copy takes, the buffers are lent in turn, a size they do
 * not fit remakes them after retiring the cached frame. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <boolean.h>
#include <libretro.h>
#include "gfx/video_defines.h"

static unsigned live;
static void *linearMemAlign(size_t size, size_t align)
{
   void *p = NULL;
   if (posix_memalign(&p, align, size)) return NULL;
   live++;
   return p;
}
static void linearFree(void *p) { if (p) { live--; free(p); } }

#define CTR_SW_LEND_SLOTS 2
typedef struct
{
   struct
   {
      void    *buffer[CTR_SW_LEND_SLOTS];
      unsigned dims;
      unsigned next;
   } sw_lend;
   bool rgb32;
   unsigned texture_width, texture_height;
} ctr_video_t;

static unsigned retired;
static void video_driver_cached_frame_retire(void) { retired++; }
#include "sw_lend.inc"

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %u: %s\n",(unsigned)__LINE__,#x); return 1; } } while (0)

static void ask(struct retro_framebuffer *fb, unsigned w, unsigned h)
{
   memset(fb, 0, sizeof(*fb));
   fb->width = w; fb->height = h; fb->access_flags = RETRO_MEMORY_ACCESS_WRITE;
}

int main(void)
{
   ctr_video_t v;
   struct retro_framebuffer fb;
   void *a, *b;

   memset(&v, 0, sizeof(v));
   v.texture_width = v.texture_height = 512;

   ask(&fb, 320, 240);
   CHECK(ctr_get_current_software_framebuffer(&v, &fb));
   CHECK(fb.pitch == 640 && fb.format == RETRO_PIXEL_FORMAT_RGB565);
   CHECK(fb.memory_flags == RETRO_MEMORY_TYPE_CACHED);
   CHECK(!((uintptr_t)fb.data & 0x7F) && !(fb.pitch & 0xF) && fb.pitch > 0x40);
   a = fb.data;
   CHECK(ctr_get_current_software_framebuffer(&v, &fb));
   b = fb.data;
   CHECK(a != b);
   CHECK(ctr_get_current_software_framebuffer(&v, &fb));
   CHECK(fb.data == a && live == 2 && retired == 0);

   /* A pitch the GPU's copy does not take */
   ask(&fb, 301, 240);
   CHECK(!ctr_get_current_software_framebuffer(&v, &fb));
   ask(&fb, 16, 16);
   CHECK(!ctr_get_current_software_framebuffer(&v, &fb));
   ask(&fb, 1024, 240);
   CHECK(!ctr_get_current_software_framebuffer(&v, &fb));
   CHECK(live == 2);

   /* Another size: the old buffers go, the cached frame first */
   ask(&fb, 400, 240);
   CHECK(ctr_get_current_software_framebuffer(&v, &fb));
   CHECK(fb.pitch == 800 && live == 2 && retired == 1);

   v.rgb32 = true;
   ask(&fb, 256, 224);
   CHECK(ctr_get_current_software_framebuffer(&v, &fb));
   CHECK(fb.pitch == 1024 && fb.format == RETRO_PIXEL_FORMAT_XRGB8888);

   ctr_sw_lend_release(&v);
   CHECK(!live && !v.sw_lend.dims);
   puts("PASS: 3DS software framebuffer lend");
   return 0;
}
