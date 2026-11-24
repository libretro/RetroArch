/* Cached menu loans consumed by the shipping SDL2 texture upload. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <boolean.h>
#include <retro_inline.h>
#include <memalign.h>
#include "gfx/video_defines.h"
#include "texture.inc"

typedef struct
{
   struct { bool smooth; } video;
   sdl2_tex_t menu, frame;
   SDL_Renderer *renderer;
#include "memory.inc"
#include "menu_lend.inc"
} sdl2_video_t;

#define RARCH_ERR(...) fprintf(stderr, __VA_ARGS__)
static const void *source;
static bool fail_alloc;
static unsigned allocations, releases, uploads;
static unsigned locks, unlocks;
static bool fail_lock;
static bool bad_pitch;
static int lock_texture(SDL_Texture *texture, const SDL_Rect *rect,
      void **pixels, int *pitch)
{
   int result;
   if (fail_lock)
      return -1;
   result = SDL_LockTexture(texture, rect, pixels, pitch);
   if (!result)
   {
      locks++;
      if (bad_pitch)
         *pitch = 1;
   }
   return result;
}
static void unlock_texture(SDL_Texture *texture)
{
   unlocks++;
   SDL_UnlockTexture(texture);
}
static void *allocate(size_t alignment, size_t size)
{
   void *p = fail_alloc ? NULL : memalign_alloc(alignment, size);
   if (p)
      allocations++;
   return p;
}
static void release(void *p)
{
   if (p)
      releases++;
   memalign_free(p);
}
static int upload(SDL_Texture *texture, const SDL_Rect *rect,
      const void *pixels, int pitch)
{
   assert(pixels == source);
   uploads++;
   return SDL_UpdateTexture(texture, rect, pixels, pitch);
}
#define memalign_alloc allocate
#define memalign_free release
#define SDL_UpdateTexture upload
#define SDL_LockTexture lock_texture
#define SDL_UnlockTexture unlock_texture
#include "callbacks.inc"
#undef memalign_alloc
#undef memalign_free
#undef SDL_UpdateTexture
#undef SDL_LockTexture
#undef SDL_UnlockTexture

static void check_pixels(sdl2_video_t *vid, const void *pixels,
      unsigned width, bool rgb32)
{
   uint32_t result[5 * 3];
   SDL_Rect rect;
   unsigned i;
   rect.x = rect.y = 0;
   rect.w = (int)width;
   rect.h = 3;
   assert(!SDL_SetTextureBlendMode(vid->menu.tex, SDL_BLENDMODE_NONE));
   assert(!SDL_RenderClear(vid->renderer));
   assert(!SDL_RenderCopy(vid->renderer, vid->menu.tex, NULL, &rect));
   assert(!SDL_RenderReadPixels(vid->renderer, &rect,
            SDL_PIXELFORMAT_ARGB8888, result, (int)width * 4));
   for (i = 0; i < width * 3; i++)
   {
      uint32_t expected;
      if (rgb32)
         expected = ((const uint32_t*)pixels)[i];
      else
      {
         uint16_t value = ((const uint16_t*)pixels)[i];
         expected = 0xff000000u | (((value >> 12) & 15) * 17u << 16)
            | (((value >> 8) & 15) * 17u << 8) | (((value >> 4) & 15) * 17u);
      }
      assert(result[i] == expected);
   }
}

int main(int argc, char **argv)
{
   SDL_Window *window;
   unsigned tested = 0;
   int driver;
   (void)argc;
   (void)argv;
   SDL_SetMainReady();
   assert(!SDL_Init(SDL_INIT_VIDEO));
   window = SDL_CreateWindow("SDL2 menu loan test", 0, 0, 8, 8, SDL_WINDOW_HIDDEN);
   assert(window);
   for (driver = 0; driver < SDL_GetNumRenderDrivers(); driver++)
   {
      sdl2_video_t vid;
      SDL_RendererInfo info;
      uintptr_t ids[SDL2_MENU_MEM_MAX], extra;
      void *pixels[SDL2_MENU_MEM_MAX];
      uint32_t raw[5 * 3];
      unsigned i, frame;
      size_t cold_pitch;
      memset(&vid, 0, sizeof(vid));
      vid.renderer = SDL_CreateRenderer(window, driver, 0);
      if (!vid.renderer)
         continue;
      assert(!SDL_GetRendererInfo(vid.renderer, &info));
      printf("SDL2 menu renderer: %s\n", info.name);
      assert(!sdl2_get_menu_framebuffer(&vid, false,
               VIDEO_SCALE_PACK(5, 3), &cold_pitch) && !vid.menu.tex);
      fail_alloc = true;
      assert(!sdl2_menu_mem_alloc(&vid, sizeof(raw), &extra) && !extra);
      fail_alloc = false;
      assert(!sdl2_menu_mem_alloc(&vid, 0, &extra) && !extra);
      for (i = 0; i < SDL2_MENU_MEM_MAX; i++)
      {
         pixels[i] = sdl2_menu_mem_alloc(&vid, sizeof(raw), &ids[i]);
         assert(pixels[i] && !((uintptr_t)pixels[i] & 63));
      }
      assert(!sdl2_menu_mem_alloc(&vid, 1, &extra) && !extra);
      for (frame = 0; frame < 48; frame++)
      {
         unsigned slot = frame % SDL2_MENU_MEM_MAX;
         unsigned width = frame & 2 ? 5 : 3;
         bool rgb32 = (frame & 1) != 0;
         assert(sdl2_menu_mem_ready(&vid, ids[slot]));
         for (i = 0; i < width * 3; i++)
            if (rgb32)
               ((uint32_t*)pixels[slot])[i] = 0xff000000u
                  | ((frame * 7841 + i * 547) & 0x00ffffffu);
            else
               ((uint16_t*)pixels[slot])[i] = (uint16_t)((frame * 67 + i * 113) | 15);
         source = pixels[slot];
         memcpy(raw, source, width * 3 * (rgb32 ? 4 : 2));
         sdl2_poke_set_texture_frame(&vid, source, rgb32,
               VIDEO_SCALE_PACK(width, 3), 1);
         /* SDL owns the uploaded pixels after the call returns. Reusing
          * or freeing the producer's source cannot change the texture. */
         memset(pixels[slot], 0, sizeof(raw));
         check_pixels(&vid, raw, width, rgb32);
         source = raw;
         sdl2_poke_set_texture_frame(&vid, source, rgb32,
               VIDEO_SCALE_PACK(width, 3), .5f);
         check_pixels(&vid, source, width, rgb32);
      }
      memcpy(pixels[0], raw, sizeof(raw));
      source = pixels[0];
      sdl2_poke_set_texture_frame(&vid, source, true, VIDEO_SCALE_PACK(5, 3), 1);
      sdl2_menu_mem_free(&vid, ids[0]);
      check_pixels(&vid, raw, 5, true);
      for (i = 0; i < SDL2_MENU_MEM_MAX; i++)
      {
         sdl2_menu_mem_free(&vid, ids[i]);
         assert(!vid.menu_mem[i]);
         sdl2_menu_mem_free(&vid, ids[i]);
      }
      assert(allocations == releases);
      /* Direct loans use SDL's actual row pitch. Publishing unlocks the
       * texture without passing its pixels through SDL_UpdateTexture. */
      for (frame = 0; frame < 16; frame++)
      {
         bool rgb32 = (frame & 1) != 0;
         unsigned width = frame & 2 ? 5 : 3;
         unsigned dims = VIDEO_SCALE_PACK(width, 3);
         unsigned before, y;
         size_t pitch = 0, row = width * (rgb32 ? 4 : 2);
         void *loan;
         source = raw;
         sdl2_poke_set_texture_frame(&vid, source, rgb32, dims, 1);
         before = uploads;
         fail_lock = true;
         assert(!sdl2_get_menu_framebuffer(&vid, rgb32, dims, &pitch));
         fail_lock = false;
         bad_pitch = true;
         assert(!sdl2_get_menu_framebuffer(&vid, rgb32, dims, &pitch));
         assert(!vid.menu_lend.pixels && locks == unlocks);
         bad_pitch = false;
         loan = sdl2_get_menu_framebuffer(&vid, rgb32, dims, &pitch);
         assert(loan && pitch >= row);
         assert(sdl2_get_menu_framebuffer(&vid, rgb32, dims, &pitch) == loan);
         for (i = 0; i < width * 3; i++)
            if (rgb32)
               raw[i] = 0xff000000u | ((frame * 541 + i * 349) & 0xffffffu);
            else
               ((uint16_t*)raw)[i] = (uint16_t)((frame * 71 + i * 19) | 15);
         for (y = 0; y < 3; y++)
            memcpy((uint8_t*)loan + y * pitch, (uint8_t*)raw + y * row, row);
         source = loan;
         sdl2_poke_set_texture_frame(&vid, source, rgb32, dims, 1);
         assert(uploads == before && !vid.menu_lend.pixels);
         assert(locks == unlocks);
         check_pixels(&vid, raw, width, rgb32);
      }
      {
         size_t pitch;
         unsigned dims = VIDEO_SCALE_PACK(5, 3);
         unsigned before = uploads;
         void *loan = sdl2_get_menu_framebuffer(&vid, true, dims, &pitch);
         assert(loan);
         /* A foreign source closes the outstanding loan before uploading. */
         source = raw;
         sdl2_poke_set_texture_frame(&vid, source, true, dims, 1);
         assert(uploads == before + 1 && !vid.menu_lend.pixels);
         check_pixels(&vid, raw, 5, true);
         assert(sdl2_get_menu_framebuffer(&vid, true, dims, &pitch));
         assert(!sdl2_get_menu_framebuffer(&vid, false, dims, &pitch));
         assert(!vid.menu_lend.pixels);
         assert(!sdl2_get_menu_framebuffer(&vid, true, VIDEO_SCALE_PACK(7, 3), &pitch));
         loan = sdl2_get_menu_framebuffer(&vid, true, dims, &pitch);
         assert(loan);
         source = loan;
         sdl2_poke_set_texture_frame(&vid, source, false, VIDEO_SCALE_PACK(3, 3), 1);
         assert(uploads == before + 1 && vid.menu.dims == dims && vid.menu.rgb32);
         assert(sdl2_get_menu_framebuffer(&vid, true, dims, &pitch));
         sdl2_menu_lend_close(&vid);
         assert(!vid.menu_lend.pixels && locks == unlocks);
      }
      sdl2_menu_mem_free(&vid, 0);
      sdl2_menu_mem_free(&vid, SDL2_MENU_MEM_MAX + 1);
      sdl_tex_zero(&vid.menu);
      SDL_DestroyRenderer(vid.renderer);
      tested++;
   }
   SDL_DestroyWindow(window);
   SDL_Quit();
   assert(tested && uploads == tested * (97 + 16 + 1));
   puts("PASS SDL2 menu loans: cached storage, direct locked-texture writes, fallback and retirement");
   return 0;
}
