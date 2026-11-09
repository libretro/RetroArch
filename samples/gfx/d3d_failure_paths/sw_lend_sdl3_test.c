/* The SDL3 software framebuffer lend, its shipping functions against a
 * stand-in for SDL's texture calls: a frame drawn into the loan is in
 * the texture with no upload, a loan stays open across a dupe, a window
 * of it comes out at its own size, a format SDL must widen is not lent,
 * and no texture is destroyed locked. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_inline.h>
#include <libretro.h>
#include "gfx/video_defines.h"

typedef enum
{
   SDL_PIXELFORMAT_UNKNOWN, SDL_PIXELFORMAT_ARGB8888,
   SDL_PIXELFORMAT_RGB565, SDL_PIXELFORMAT_RGBA4444
} SDL_PixelFormat;
enum { SDL_TEXTUREACCESS_STREAMING, SDL_SCALEMODE_NEAREST,
   SDL_BLENDMODE_BLEND, SDL_BLENDMODE_NONE };
enum { SDL3_FLAG_NATIVE_RGB565 = 1, SDL3_FLAG_NATIVE_RGBA4444 = 2 };
typedef struct { uint8_t *mem; int pitch; bool locked; int w, h; } SDL_Texture;
typedef struct { int unused; } SDL_Renderer;

static int      pitch_pad;
static unsigned destroyed_locked;
static SDL_Texture *SDL_CreateTexture(SDL_Renderer *r, SDL_PixelFormat f,
      int access, unsigned w, unsigned h)
{
   SDL_Texture *t = (SDL_Texture*)calloc(1, sizeof(*t));
   (void)r; (void)access;
   t->w     = (int)w;
   t->h     = (int)h;
   t->pitch = (int)w * (f == SDL_PIXELFORMAT_ARGB8888 ? 4 : 2) + pitch_pad;
   t->mem   = (uint8_t*)calloc((size_t)h, (size_t)t->pitch);
   return t;
}
static void SDL_DestroyTexture(SDL_Texture *t)
{
   destroyed_locked += t->locked;
   free(t->mem);
   free(t);
}
static bool SDL_LockTexture(SDL_Texture *t, void *rect, void **p, int *pitch)
{
   (void)rect;
   if (t->locked) return false;
   t->locked = true; *p = t->mem; *pitch = t->pitch;
   return true;
}
static void SDL_UnlockTexture(SDL_Texture *t) { t->locked = false; }
static void SDL_SetTextureScaleMode(SDL_Texture *t, int m) { (void)t; (void)m; }
static void SDL_SetTextureBlendMode(SDL_Texture *t, int m) { (void)t; (void)m; }
static const char *SDL_GetError(void) { return "stand-in"; }
#define RARCH_ERR(...) fprintf(stderr, __VA_ARGS__)
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %u: %s\n",(unsigned)__LINE__,#x); goto done; } } while (0)

typedef struct
{
   SDL_Texture *tex;
   unsigned w, h;
   SDL_PixelFormat widen;
   bool active, rgb32, failed;
} sdl3_tex_t;

typedef struct
{
   struct { bool rgb32; bool smooth; } video;
   unsigned flags;
#include "sw_lend_type.inc"
   sdl3_tex_t frame;
   sdl3_tex_t menu;
   SDL_Renderer *renderer;
} sdl3_video_t;

static void sdl3_set_frame_scale_mode(SDL_Texture *t, bool s) { (void)t; (void)s; }
static unsigned retired;
static void video_driver_cached_frame_retire(void) { retired++; }
#include "sw_lend.inc"

#define W 64
#define H 48

static void ask(struct retro_framebuffer *fb, unsigned w, unsigned h)
{
   memset(fb, 0, sizeof(*fb));
   fb->width = w; fb->height = h; fb->access_flags = RETRO_MEMORY_ACCESS_WRITE;
}

int main(void)
{
   static SDL_Renderer renderer;
   static uint32_t own[W * H];
   sdl3_video_t v;
   struct retro_framebuffer fb;
   const void *frame;
   unsigned pitch, frames;
   void *open_loan;
   int result = 1;

   memset(&v, 0, sizeof(v));
   v.video.rgb32 = true;
   v.renderer    = &renderer;

   for (frames = 0; frames < 8; frames++)
   {
      ask(&fb, W, H);
      CHECK(sdl3_get_current_software_framebuffer(&v, &fb));
      CHECK(fb.data == v.frame.tex->mem && fb.pitch == W * 4);
      CHECK(fb.format == RETRO_PIXEL_FORMAT_XRGB8888);
      CHECK(v.frame.tex->locked);
      frame = fb.data; pitch = (unsigned)fb.pitch;
      CHECK(sdl3_sw_lend_take(&v, &frame, VIDEO_SCALE_PACK(W, H), &pitch));
      CHECK(!v.frame.tex->locked && !v.sw_lend.pixels);
      CHECK(retired == frames + 1);
   }

   ask(&fb, W, H);
   fb.access_flags |= RETRO_MEMORY_ACCESS_READ;
   CHECK(!sdl3_get_current_software_framebuffer(&v, &fb));

   /* Left open across a dupe, the same loan is lent again */
   ask(&fb, W, H);
   CHECK(sdl3_get_current_software_framebuffer(&v, &fb));
   open_loan = fb.data;
   ask(&fb, W, H);
   CHECK(sdl3_get_current_software_framebuffer(&v, &fb));
   CHECK(fb.data == open_loan && v.frame.tex->locked);

   /* A window comes out into scratch, and the core is lent no more */
   ((uint32_t*)fb.data)[4 * W + 8] = 0x12345678u;
   frame = (const uint8_t*)fb.data + 4 * fb.pitch + 8 * 4;
   pitch = (unsigned)fb.pitch;
   frames = retired;
   CHECK(!sdl3_sw_lend_take(&v, &frame, VIDEO_SCALE_PACK(W / 2, H / 2), &pitch));
   CHECK(frame == v.sw_lend.scratch && pitch == (W / 2) * 4);
   CHECK(((const uint32_t*)frame)[0] == 0x12345678u);
   CHECK(!v.frame.tex->locked && retired == frames + 1);
   ask(&fb, W, H);
   CHECK(!sdl3_get_current_software_framebuffer(&v, &fb));
   v.sw_lend.declined_dims = 0;

   /* A window running past the loan is dropped, not read */
   ask(&fb, W, H);
   CHECK(sdl3_get_current_software_framebuffer(&v, &fb));
   frame = (const uint8_t*)fb.data + (H - 1) * fb.pitch;
   pitch = (unsigned)fb.pitch;
   CHECK(!sdl3_sw_lend_take(&v, &frame, VIDEO_SCALE_PACK(W, H), &pitch));
   CHECK(frame == NULL && !v.frame.tex->locked);
   v.sw_lend.declined_dims = 0;

   /* The core's own buffer: the loan closes, the cached frame stays */
   ask(&fb, W, H);
   CHECK(sdl3_get_current_software_framebuffer(&v, &fb));
   frames = retired;
   frame = own; pitch = W * 4;
   CHECK(!sdl3_sw_lend_take(&v, &frame, VIDEO_SCALE_PACK(W, H), &pitch));
   CHECK(frame == own && !v.frame.tex->locked && retired == frames);

   /* Another size closes the open loan before the texture goes */
   ask(&fb, W, H);
   CHECK(sdl3_get_current_software_framebuffer(&v, &fb));
   ask(&fb, W / 2, H / 2);
   CHECK(sdl3_get_current_software_framebuffer(&v, &fb));
   CHECK(!destroyed_locked && v.frame.w == W / 2);
   sdl3_sw_lend_close(&v, true);

   /* 16-bit frames SDL must widen are not lent */
   v.video.rgb32 = false;
   ask(&fb, W, H);
   CHECK(!sdl3_get_current_software_framebuffer(&v, &fb));
   CHECK(!v.frame.tex->locked);
   /* Rendered natively they are */
   v.flags = SDL3_FLAG_NATIVE_RGB565;
   sdl3_tex_zero(&v.frame);
   ask(&fb, W, H);
   CHECK(sdl3_get_current_software_framebuffer(&v, &fb));
   CHECK(fb.format == RETRO_PIXEL_FORMAT_RGB565 && fb.pitch == W * 2);
   sdl3_sw_lend_close(&v, true);

   /* A padded row is declined, as cores tend to ignore fb->pitch */
   v.video.rgb32 = true;
   v.flags       = 0;
   sdl3_tex_zero(&v.frame);
   pitch_pad     = 16;
   ask(&fb, W / 4, H / 4);
   CHECK(!sdl3_get_current_software_framebuffer(&v, &fb));
   CHECK(!v.frame.tex->locked);
   CHECK(!sdl3_get_current_software_framebuffer(&v, &fb));

   puts("PASS: SDL3 software framebuffer lend");
   result = 0;
done:
   if (v.frame.tex) { sdl3_sw_lend_close(&v, false); sdl3_tex_zero(&v.frame); }
   free(v.sw_lend.scratch);
   return result;
}
