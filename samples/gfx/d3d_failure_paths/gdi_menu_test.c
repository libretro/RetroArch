#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <boolean.h>
#include <memalign.h>
#include "../../../gfx/video_defines.h"

typedef struct
{
#include "storage.inc"
   unsigned menu_dims, menu_pitch, menu_bits;
} gdi_t;
static unsigned copies;
static bool fail_alloc;
static void *allocate(size_t alignment, size_t size)
{ return fail_alloc ? NULL : memalign_alloc(alignment, size); }
static void *copy(void *dst, const void *src, size_t size)
{ copies++; return memcpy(dst, src, size); }
#define memalign_alloc allocate
#define memcpy copy
#include "callbacks.inc"
#undef memalign_alloc
#undef memcpy

int main(void)
{
   gdi_t gdi;
   uintptr_t ids[GDI_MENU_MEM_MAX], extra;
   uint8_t *pixels[GDI_MENU_MEM_MAX], raw[60];
   unsigned i, frame;
   memset(&gdi, 0, sizeof(gdi));
   memset(raw, 57, sizeof(raw));
   fail_alloc = true;
   assert(!gdi_menu_mem_alloc(&gdi, sizeof(raw), &extra) && !extra);
   fail_alloc = false;
   assert(!gdi_menu_mem_alloc(&gdi, 0, &extra) && !extra);
   for (i = 0; i < GDI_MENU_MEM_MAX; i++)
   {
      pixels[i] = (uint8_t*)gdi_menu_mem_alloc(&gdi, sizeof(raw), &ids[i]);
      assert(pixels[i] && !((uintptr_t)pixels[i] & 63));
   }
   assert(!gdi_menu_mem_alloc(&gdi, 1, &extra) && !extra);
   gdi_set_texture_frame(&gdi, pixels[0], true, VIDEO_SCALE_PACK(10, 6), 1);
   assert(!gdi.menu_source && !gdi.menu_frame && !copies);
   for (frame = 0; frame < 48; frame++)
   {
      unsigned slot = frame % GDI_MENU_MEM_MAX;
      bool rgb32 = (frame & 1) != 0;
      assert(gdi_menu_mem_ready(&gdi, ids[slot]));
      memset(pixels[slot], (int)frame, sizeof(raw));
      gdi_set_texture_frame(&gdi, pixels[slot], rgb32, VIDEO_SCALE_PACK(5, 3), 1);
      assert(gdi.menu_source == pixels[slot] && !gdi.menu_frame && !copies);
      assert(gdi.menu_pitch == 5 * (rgb32 ? 4u : 2u));
      assert(gdi.menu_bits == (rgb32 ? 32u : 16u));
      for (i = 0; i < GDI_MENU_MEM_MAX; i++)
         assert(gdi_menu_mem_ready(&gdi, ids[i]) == (i != slot));
      /* Cached redraws retain this same immutable source. */
      assert(gdi.menu_source[0] == frame);
      assert(!gdi_menu_mem_ready(&gdi, ids[slot]));
   }
   gdi_set_texture_frame(&gdi, raw, true, VIDEO_SCALE_PACK(5, 3), 1);
   assert(copies == 1 && gdi.menu_source == gdi.menu_frame);
   assert(!memcmp(raw, gdi.menu_source, sizeof(raw)));
   for (i = 0; i < GDI_MENU_MEM_MAX; i++)
      assert(gdi_menu_mem_ready(&gdi, ids[i]));
   gdi_set_texture_frame(&gdi, pixels[0], true, VIDEO_SCALE_PACK(5, 3), 1);
   gdi_menu_mem_free(&gdi, ids[0]);
   assert(!gdi.menu_source && gdi_menu_mem_ready(&gdi, ids[0]));
   pixels[0] = (uint8_t*)gdi_menu_mem_alloc(&gdi, 240, &ids[0]);
   assert(pixels[0]);
   memset(pixels[0], 93, 240);
   gdi_set_texture_frame(&gdi, pixels[0], true, VIDEO_SCALE_PACK(10, 6), 1);
   assert(gdi.menu_source == pixels[0] && copies == 1);
   gdi_set_texture_frame(&gdi, NULL, false, 0, 1);
   assert(gdi.menu_source == pixels[0]);
   for (i = 0; i < GDI_MENU_MEM_MAX; i++)
   {
      gdi_menu_mem_free(&gdi, ids[i]);
      assert(!gdi.menu_mem[i] && !gdi.menu_mem_size[i]);
      gdi_menu_mem_free(&gdi, ids[i]);
   }
   assert(!gdi.menu_source);
   free(gdi.menu_frame);
   puts("PASS GDI menu loans: zero copies, bounded storage, retained redraws, fallback and retirement");
   return 0;
}
