#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include <boolean.h>
#include <retro_inline.h>
#include <memalign.h>
#include <gfx/scaler/pixconv.h>
#include "gfx/video_defines.h"
#include "texture.inc"
#include "flags.inc"

typedef struct
{
   struct { bool smooth; } video;
   sdl3_tex_t menu, frame;
   unsigned flags;
   SDL_Renderer *renderer;
#include "memory.inc"
#include "menu_lend.inc"
} sdl3_video_t;

#define RARCH_ERR(...) fprintf(stderr, __VA_ARGS__)
static bool fail_lock, bad_pitch, fail_alloc;
static unsigned locks, unlocks, copies, allocations, releases;
static bool lock_texture(SDL_Texture *texture, const SDL_Rect *rect,
      void **pixels, int *pitch)
{
   bool result;
   if (fail_lock)
      return false;
   result = SDL_LockTexture(texture, rect, pixels, pitch);
   if (result)
   {
      locks++;
      if (bad_pitch)
         *pitch = 1;
   }
   return result;
}
static void unlock_texture(SDL_Texture *texture)
{ unlocks++; SDL_UnlockTexture(texture); }
static void *copy(void *dst, const void *src, size_t size)
{ copies++; return memcpy(dst, src, size); }
static void *allocate(size_t alignment, size_t size)
{
   void *p = fail_alloc ? NULL : memalign_alloc(alignment, size);
   if (p) allocations++;
   return p;
}
static void release(void *p)
{ if (p) releases++; memalign_free(p); }
#define SDL_LockTexture lock_texture
#define SDL_UnlockTexture unlock_texture
#define memcpy copy
#define memalign_alloc allocate
#define memalign_free release
#include "callbacks.inc"
#undef SDL_LockTexture
#undef SDL_UnlockTexture
#undef memcpy
#undef memalign_alloc
#undef memalign_free

static void paint(void *pixels, unsigned width, unsigned seed, bool rgb32)
{
   unsigned i;
   for (i = 0; i < width * 3; i++)
      if (rgb32)
         ((uint32_t*)pixels)[i] = 0xff000000u | ((seed * 7841 + i * 547) & 0xffffffu);
      else
         ((uint16_t*)pixels)[i] = (uint16_t)((seed * 67 + i * 113) | 15);
}

static void check_pixels(sdl3_video_t *vid, const void *pixels,
      unsigned width, bool rgb32)
{
   SDL_Rect rect = {0, 0, 0, 3};
   SDL_FRect dst = {0, 0, 0, 3};
   SDL_Surface *readback, *converted;
   unsigned x, y;
   rect.w = (int)width;
   dst.w = (float)width;
   assert(SDL_SetTextureBlendMode(vid->menu.tex, SDL_BLENDMODE_NONE));
   assert(SDL_RenderClear(vid->renderer));
   assert(SDL_RenderTexture(vid->renderer, vid->menu.tex, NULL, &dst));
   readback = SDL_RenderReadPixels(vid->renderer, &rect);
   assert(readback);
   converted = SDL_ConvertSurface(readback, SDL_PIXELFORMAT_ARGB8888);
   assert(converted);
   for (y = 0; y < 3; y++)
      for (x = 0; x < width; x++)
      {
         uint32_t expected;
         const uint32_t *row = (const uint32_t*)((const uint8_t*)converted->pixels
               + (size_t)y * converted->pitch);
         if (rgb32)
            expected = ((const uint32_t*)pixels)[y * width + x];
         else
         {
            uint16_t value = ((const uint16_t*)pixels)[y * width + x];
            expected = 0xff000000u | (((value >> 12) & 15) * 17u << 16)
               | (((value >> 8) & 15) * 17u << 8) | (((value >> 4) & 15) * 17u);
         }
         assert((row[x] & 0xffffffu) == (expected & 0xffffffu));
      }
   SDL_DestroySurface(converted);
   SDL_DestroySurface(readback);
}

int main(void)
{
   SDL_Window *window;
   int driver;
   unsigned tested = 0, direct = 0, widened = 0;
   assert(SDL_Init(SDL_INIT_VIDEO));
   window = SDL_CreateWindow("SDL3 menu loans", 8, 8, SDL_WINDOW_HIDDEN);
   assert(window);
   for (driver = 0; driver < SDL_GetNumRenderDrivers(); driver++)
   {
      sdl3_video_t vid;
      uintptr_t ids[SDL3_MENU_MEM_MAX], extra;
      void *pixels[SDL3_MENU_MEM_MAX];
      uint32_t raw[15];
      unsigned i, frame;
      size_t pitch;
      memset(&vid, 0, sizeof(vid));
      vid.renderer = SDL_CreateRenderer(window, SDL_GetRenderDriver(driver));
      if (!vid.renderer) continue;
      printf("SDL3 menu renderer: %s\n", SDL_GetRendererName(vid.renderer));
      if (sdl3_renderer_has_format(vid.renderer, SDL_PIXELFORMAT_RGBA4444))
         vid.flags |= SDL3_FLAG_NATIVE_RGBA4444;
      assert(!sdl3_get_menu_framebuffer(&vid, false, VIDEO_SCALE_PACK(5, 3), &pitch));
      fail_alloc = true;
      assert(!sdl3_menu_mem_alloc(&vid, sizeof(raw), &extra) && !extra);
      fail_alloc = false;
      assert(!sdl3_menu_mem_alloc(&vid, 0, &extra) && !extra);
      for (i = 0; i < SDL3_MENU_MEM_MAX; i++)
      {
         pixels[i] = sdl3_menu_mem_alloc(&vid, sizeof(raw), &ids[i]);
         assert(pixels[i] && !((uintptr_t)pixels[i] & 63));
      }
      assert(!sdl3_menu_mem_alloc(&vid, 1, &extra) && !extra);
      for (frame = 0; frame < 32; frame++)
      {
         unsigned slot = frame % SDL3_MENU_MEM_MAX, width = frame & 2 ? 5 : 3;
         bool rgb32 = (frame & 1) != 0;
         unsigned dims = VIDEO_SCALE_PACK(width, 3), before;
         size_t row = width * (rgb32 ? 4 : 2);
         void *loan;
         assert(sdl3_menu_mem_ready(&vid, ids[slot]));
         paint(pixels[slot], width, frame, rgb32);
         memcpy(raw, pixels[slot], row * 3);
         sdl3_poke_set_texture_frame(&vid, pixels[slot], rgb32, dims, 1);
         memset(pixels[slot], 0, sizeof(raw));
         check_pixels(&vid, raw, width, rgb32);
         loan = sdl3_get_menu_framebuffer(&vid, rgb32, dims, &pitch);
         if (vid.menu.widen != SDL_PIXELFORMAT_UNKNOWN)
         {
            assert(!loan && !rgb32);
            widened++;
         }
         else
         {
            unsigned y;
            assert(loan && pitch >= row);
            assert(sdl3_get_menu_framebuffer(&vid, rgb32, dims, &pitch) == loan);
            paint(raw, width, frame + 57, rgb32);
            for (y = 0; y < 3; y++)
               memcpy((uint8_t*)loan + y * pitch, (uint8_t*)raw + y * row, row);
            before = copies;
            sdl3_poke_set_texture_frame(&vid, loan, rgb32, dims, 1);
            assert(copies == before && !vid.menu_lend.pixels && locks == unlocks);
            check_pixels(&vid, raw, width, rgb32);
            fail_lock = true;
            assert(!sdl3_get_menu_framebuffer(&vid, rgb32, dims, &pitch));
            fail_lock = false;
            bad_pitch = true;
            assert(!sdl3_get_menu_framebuffer(&vid, rgb32, dims, &pitch));
            bad_pitch = false;
            assert(!vid.menu_lend.pixels && locks == unlocks);
            direct++;
         }
         /* Raw CPU sources retain their upload/conversion path. */
         sdl3_poke_set_texture_frame(&vid, raw, rgb32, dims, 1);
         check_pixels(&vid, raw, width, rgb32);
      }
      assert(sdl3_get_menu_framebuffer(&vid, true, VIDEO_SCALE_PACK(5, 3), &pitch));
      sdl3_poke_set_texture_frame(&vid, raw, true, VIDEO_SCALE_PACK(5, 3), 1);
      check_pixels(&vid, raw, 5, true);
      assert(sdl3_get_menu_framebuffer(&vid, true, VIDEO_SCALE_PACK(5, 3), &pitch));
      assert(!sdl3_get_menu_framebuffer(&vid, false, VIDEO_SCALE_PACK(5, 3), &pitch));
      assert(!vid.menu_lend.pixels);
      assert(!sdl3_get_menu_framebuffer(&vid, true, VIDEO_SCALE_PACK(7, 3), &pitch));
      {
         void *loan = sdl3_get_menu_framebuffer(&vid, true, VIDEO_SCALE_PACK(5, 3), &pitch);
         unsigned before = copies;
         assert(loan);
         sdl3_poke_set_texture_frame(&vid, loan, false, VIDEO_SCALE_PACK(3, 3), 1);
         assert(!vid.menu_lend.pixels && copies == before && vid.menu.rgb32);
         assert(vid.menu.w == 5 && vid.menu.h == 3);
      }
      /* Exercise the native-4444 branch even on renderers whose format
       * list omits it: SDL still supplies a compatible streaming texture.
       * The ordinary capability path above must decline these loans. */
      vid.flags |= SDL3_FLAG_NATIVE_RGBA4444;
      paint(raw, 5, 73, false);
      sdl3_poke_set_texture_frame(&vid, raw, false, VIDEO_SCALE_PACK(5, 3), 1);
      {
         unsigned y, before = copies;
         void *loan = sdl3_get_menu_framebuffer(&vid, false, VIDEO_SCALE_PACK(5, 3), &pitch);
         assert(loan && vid.menu.widen == SDL_PIXELFORMAT_UNKNOWN);
         paint(raw, 5, 93, false);
         for (y = 0; y < 3; y++)
            memcpy((uint8_t*)loan + y * pitch, (uint8_t*)raw + y * 10, 10);
         sdl3_poke_set_texture_frame(&vid, loan, false, VIDEO_SCALE_PACK(5, 3), 1);
         assert(copies == before && locks == unlocks);
         check_pixels(&vid, raw, 5, false);
      }
      assert(sdl3_get_menu_framebuffer(&vid, false, VIDEO_SCALE_PACK(5, 3), &pitch));
      sdl3_menu_lend_close(&vid);
      for (i = 0; i < SDL3_MENU_MEM_MAX; i++)
      {
         sdl3_menu_mem_free(&vid, ids[i]);
         assert(!vid.menu_mem[i]);
      }
      assert(allocations == releases && locks == unlocks);
      sdl3_tex_zero(&vid.menu);
      SDL_DestroyRenderer(vid.renderer);
      tested++;
   }
   SDL_DestroyWindow(window);
   SDL_Quit();
   assert(tested && direct);
   printf("PASS SDL3 menu loans: %u renderers, %u direct writes, %u widening fallbacks\n",
         tested, direct, widened);
   return 0;
}
