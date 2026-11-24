#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <wingdi.h>
#include <boolean.h>
#include <memalign.h>
#include "../../../gfx/video_defines.h"

typedef struct
{
#include "storage.inc"
   unsigned menu_dims, menu_pitch, menu_bits;
   HDC memDC, texDC, winDC;
   HBITMAP scratch_rgui_bmp;
   uint32_t *scratch_rgui_pixels;
   unsigned scratch_rgui_dims;
} gdi_t;
#include "callbacks.inc"
#include "pixels.inc"

static void check(gdi_t *gdi, uint32_t *dest, unsigned width)
{
   const uint16_t *src = (const uint16_t*)gdi->menu_source;
   unsigned x, y, c;
   for (y = 0; y < 5; y++)
      for (x = 0; x < 8; x++)
         dest[y * 8 + x] = 0x00102030u;
   gdi_blit_rgui_alpha(gdi, src, width, 3, 1, 1, width, 3);
   GdiFlush();
   for (y = 0; y < 5; y++)
      for (x = 0; x < 8; x++)
         for (c = 0; c < 3; c++)
         {
            unsigned background = (0x00102030u >> (c * 8)) & 255;
            unsigned expected = background;
            unsigned actual = (dest[y * 8 + x] >> (c * 8)) & 255;
            if (x >= 1 && x < width + 1 && y >= 1 && y < 4)
            {
               uint16_t value = src[(y - 1) * width + x - 1];
               unsigned alpha = (value & 15) * 17;
               unsigned channel = ((value >> (c * 4 + 4)) & 15) * 17;
               unsigned premul = (channel * alpha + 127) / 255;
               unsigned scratch = gdi->scratch_rgui_pixels[
                  (y - 1) * VIDEO_SCALE_W(gdi->scratch_rgui_dims) + x - 1];
               assert(((scratch >> (c * 8)) & 255) == premul);
               expected = premul + (background * (255 - alpha) + 127) / 255;
            }
            assert(actual + 1 >= expected && actual <= expected + 1);
         }
}

int main(void)
{
   gdi_t gdi;
   uintptr_t ids[2];
   uint16_t *pixels[2], raw[15];
   BITMAPINFO info;
   HBITMAP bitmap, old;
   uint32_t *dest;
   unsigned frame, i;
   memset(&gdi, 0, sizeof(gdi));
   memset(&info, 0, sizeof(info));
   gdi.winDC = GetDC(NULL);
   gdi.memDC = CreateCompatibleDC(gdi.winDC);
   assert(gdi.memDC);
   info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
   info.bmiHeader.biWidth = 8;
   info.bmiHeader.biHeight = -5;
   info.bmiHeader.biPlanes = 1;
   info.bmiHeader.biBitCount = 32;
   bitmap = CreateDIBSection(gdi.memDC, &info, DIB_RGB_COLORS,
         (void**)&dest, NULL, 0);
   assert(bitmap && dest);
   old = (HBITMAP)SelectObject(gdi.memDC, bitmap);
   for (i = 0; i < 2; i++)
   {
      pixels[i] = (uint16_t*)gdi_menu_mem_alloc(&gdi, sizeof(raw), &ids[i]);
      assert(pixels[i]);
   }
   for (frame = 0; frame < 32; frame++)
   {
      unsigned slot = frame & 1, width = frame & 2 ? 5 : 3;
      assert(gdi_menu_mem_ready(&gdi, ids[slot]));
      for (i = 0; i < width * 3; i++)
         pixels[slot][i] = (uint16_t)(frame * 1741 + i * 123);
      gdi_set_texture_frame(&gdi, pixels[slot], false, VIDEO_SCALE_PACK(width, 3), 1);
      assert(gdi.menu_source == (const uint8_t*)pixels[slot] && !gdi.menu_frame);
      check(&gdi, dest, width);
      check(&gdi, dest, width);
      assert(!gdi_menu_mem_ready(&gdi, ids[slot]));
   }
   for (i = 0; i < 15; i++)
      raw[i] = (uint16_t)(i * 547);
   gdi_set_texture_frame(&gdi, raw, false, VIDEO_SCALE_PACK(5, 3), 1);
   assert(gdi.menu_source == gdi.menu_frame);
   check(&gdi, dest, 5);
   gdi_set_texture_frame(&gdi, pixels[0], false, VIDEO_SCALE_PACK(5, 3), 1);
   gdi_menu_mem_free(&gdi, ids[0]);
   assert(!gdi.menu_source);
   gdi_blit_rgui_alpha(&gdi, gdi.menu_source, 5, 3, 1, 1, 5, 3);
   gdi_menu_mem_free(&gdi, ids[1]);
   SelectObject(gdi.memDC, old);
   DeleteObject(bitmap);
   DeleteObject(gdi.scratch_rgui_bmp);
   DeleteDC(gdi.texDC);
   DeleteDC(gdi.memDC);
   ReleaseDC(NULL, gdi.winDC);
   free(gdi.menu_frame);
   puts("PASS GDI DIB pixels: borrowed 4444, alpha, resized scratch, retained redraws and free");
   return 0;
}
