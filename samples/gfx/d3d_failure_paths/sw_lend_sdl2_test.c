/* The SDL2 software framebuffer lend, its shipping functions on a real
 * SDL renderer: a frame drawn into the loan reaches the texture with no
 * upload, a loan stays open across a dupe, a window of it comes out at
 * its own size, a core's own buffer leaves the cached frame alone. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <SDL.h>
#include <boolean.h>
#include <retro_inline.h>
#include <libretro.h>
#include "gfx/video_defines.h"

#define RARCH_ERR(...) fprintf(stderr, __VA_ARGS__)
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %u: %s\n",(unsigned)__LINE__,#x); goto done; } } while (0)

typedef struct
{
   SDL_Texture *tex;
   unsigned dims;
   unsigned pitch;
   bool rgb32;
   bool active;
} sdl2_tex_t;

typedef struct
{
   struct { bool rgb32; bool smooth; } video;
   sdl2_tex_t frame;
   sdl2_tex_t menu;
#include "sw_lend_type.inc"
   SDL_Renderer *renderer;
} sdl2_video_t;

static unsigned retired;
static void video_driver_cached_frame_retire(void) { retired++; }
#include "sw_lend.inc"

#define W 64
#define H 48

static uint32_t cell(unsigned seed, unsigned x, unsigned y)
{
   return 0xff000000u | ((seed * 2654435761u + y * W + x) & 0x00ffffffu);
}

static void paint(void *p, size_t pitch, unsigned w, unsigned h, unsigned seed)
{
   unsigned x, y;
   for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
         ((uint32_t*)((uint8_t*)p + y * pitch))[x] = cell(seed, x, y);
}

static bool on_screen(sdl2_video_t *v, unsigned w, unsigned h, unsigned seed)
{
   static uint32_t out[W * H];
   SDL_Rect r;
   unsigned x, y;
   r.x = 0; r.y = 0; r.w = (int)w; r.h = (int)h;
   SDL_RenderClear(v->renderer);
   SDL_RenderCopy(v->renderer, v->frame.tex, NULL, &r);
   if (SDL_RenderReadPixels(v->renderer, &r, SDL_PIXELFORMAT_ARGB8888, out,
            (int)(W * 4)))
      return false;
   for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
         if ((out[y * W + x] & 0x00ffffffu) != (cell(seed, x, y) & 0x00ffffffu))
            return false;
   return true;
}

static void ask(struct retro_framebuffer *fb, unsigned w, unsigned h)
{
   memset(fb, 0, sizeof(*fb));
   fb->width = w; fb->height = h; fb->access_flags = RETRO_MEMORY_ACCESS_WRITE;
}

int main(void)
{
   static uint32_t own[W * H];
   sdl2_video_t v;
   struct retro_framebuffer fb;
   SDL_Window *window = NULL;
   const void *frame;
   unsigned pitch, frames;
   void *open_loan;
   int result = 1;

   memset(&v, 0, sizeof(v));
   v.video.rgb32 = true;
   SDL_setenv("SDL_RENDER_DRIVER", "software", 1);
   if (SDL_Init(SDL_INIT_VIDEO))
   {
      printf("SKIP: no SDL video: %s\n", SDL_GetError());
      return 0;
   }
   CHECK((window = SDL_CreateWindow("lend", 0, 0, W, H, SDL_WINDOW_HIDDEN)));
   CHECK((v.renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE)));

   /* Each frame drawn into the loan is in the texture, no upload */
   for (frames = 0; frames < 8; frames++)
   {
      ask(&fb, W, H);
      CHECK(sdl2_get_current_software_framebuffer(&v, &fb));
      CHECK(fb.data && fb.pitch == W * 4);
      CHECK(fb.format == RETRO_PIXEL_FORMAT_XRGB8888);
      paint(fb.data, fb.pitch, W, H, frames);
      frame = fb.data; pitch = (unsigned)fb.pitch;
      CHECK(sdl2_sw_lend_take(&v, &frame, VIDEO_SCALE_PACK(W, H), &pitch));
      CHECK(!v.sw_lend.pixels && retired == frames + 1);
      CHECK(on_screen(&v, W, H, frames));
   }

   /* Asked for reading it is not lent: it may not read back */
   ask(&fb, W, H);
   fb.access_flags |= RETRO_MEMORY_ACCESS_READ;
   CHECK(!sdl2_get_current_software_framebuffer(&v, &fb));
   CHECK(!v.sw_lend.pixels);

   /* Left open across a dupe, the same loan is lent again */
   ask(&fb, W, H);
   CHECK(sdl2_get_current_software_framebuffer(&v, &fb));
   open_loan = fb.data;
   ask(&fb, W, H);
   CHECK(sdl2_get_current_software_framebuffer(&v, &fb));
   CHECK(fb.data == open_loan && v.sw_lend.pixels);

   /* A window of the loan comes out into scratch at its own size */
   paint(fb.data, fb.pitch, W, H, 77);
   frame = (const uint8_t*)fb.data + 4 * fb.pitch + 8 * 4;
   pitch = (unsigned)fb.pitch;
   frames = retired;
   CHECK(!sdl2_sw_lend_take(&v, &frame, VIDEO_SCALE_PACK(W / 2, H / 2), &pitch));
   CHECK(frame == v.sw_lend.scratch && pitch == (W / 2) * 4);
   CHECK(!v.sw_lend.pixels && retired == frames + 1);
   CHECK(((const uint32_t*)frame)[0] == cell(77, 8, 4));
   CHECK(((const uint32_t*)((const uint8_t*)frame + pitch))[W / 2 - 1]
         == cell(77, 8 + W / 2 - 1, 5));
   /* Not lent at that size again: each loan would remake the texture */
   ask(&fb, W, H);
   CHECK(!sdl2_get_current_software_framebuffer(&v, &fb));
   v.sw_lend.declined_dims = 0;

   /* A window running past the loan is dropped, not read */
   ask(&fb, W, H);
   CHECK(sdl2_get_current_software_framebuffer(&v, &fb));
   frame = (const uint8_t*)fb.data + (H - 1) * fb.pitch;
   pitch = (unsigned)fb.pitch;
   CHECK(!sdl2_sw_lend_take(&v, &frame, VIDEO_SCALE_PACK(W, H), &pitch));
   CHECK(frame == NULL && !v.sw_lend.pixels);
   v.sw_lend.declined_dims = 0;

   /* The core's own buffer: the loan closes, the cached frame stays */
   ask(&fb, W, H);
   CHECK(sdl2_get_current_software_framebuffer(&v, &fb));
   frames = retired;
   frame = own; pitch = W * 4;
   CHECK(!sdl2_sw_lend_take(&v, &frame, VIDEO_SCALE_PACK(W, H), &pitch));
   CHECK(frame == own && !v.sw_lend.pixels && retired == frames);

   /* Another size closes the open loan and remakes the texture */
   ask(&fb, W, H);
   CHECK(sdl2_get_current_software_framebuffer(&v, &fb));
   ask(&fb, W / 2, H / 2);
   CHECK(sdl2_get_current_software_framebuffer(&v, &fb));
   CHECK(v.sw_lend.dims == VIDEO_SCALE_PACK(W / 2, H / 2));
   CHECK(v.frame.dims == VIDEO_SCALE_PACK(W / 2, H / 2));
   sdl2_sw_lend_close(&v, true);

   puts("PASS: SDL2 software framebuffer lend");
   result = 0;
done:
   if (v.frame.tex) { sdl2_sw_lend_close(&v, false); SDL_DestroyTexture(v.frame.tex); }
   free(v.sw_lend.scratch);
   if (v.renderer) SDL_DestroyRenderer(v.renderer);
   if (window) SDL_DestroyWindow(window);
   SDL_Quit();
   return result;
}
