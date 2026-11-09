/* The Dingux and RS90 software framebuffer lends, their shipping
 * functions against a stand-in surface: a frame at the surface's size
 * and tight pitch is written where it is shown, anywhere else or in
 * the menu it is declined, and the surface's lock is held once and
 * always released. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <boolean.h>
#include <libretro.h>

typedef struct { void *pixels; int pitch; int locks; } SDL_Surface;
#define SDL_MUSTLOCK(s) 1
static int SDL_LockSurface(SDL_Surface *s) { s->locks++; return 0; }
static void SDL_UnlockSurface(SDL_Surface *s) { s->locks--; }

#define SDL_RS90_WIDTH  240
#define SDL_RS90_HEIGHT 160
typedef struct
{
   SDL_Surface *screen;
   bool rgb32, menu_active, was_in_menu, mode_valid;
   unsigned frame_width, frame_height, frame_padding_y;
   unsigned content_width, content_height;
   void *sw_lend;
   bool sw_lend_locked;
} VIDEO_T;

static unsigned retired;
static void video_driver_cached_frame_retire(void) { retired++; }
#include "sw_lend.inc"

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %u: %s\n",(unsigned)__LINE__,#x); return 1; } } while (0)

#define W SDL_RS90_WIDTH
#define H SDL_RS90_HEIGHT

static uint32_t pixels[W * (H + 20)];

static void ask(struct retro_framebuffer *fb, unsigned w, unsigned h)
{
   memset(fb, 0, sizeof(*fb));
   fb->width = w; fb->height = h; fb->access_flags = RETRO_MEMORY_ACCESS_WRITE;
}

int main(void)
{
   static uint32_t own[W * H];
   static SDL_Surface screen;
   VIDEO_T v;
   struct retro_framebuffer fb;

   memset(&v, 0, sizeof(v));
   screen.pixels    = pixels;
   screen.pitch     = W * 4;
   v.screen         = &screen;
   v.rgb32          = true;
   v.mode_valid     = true;
   v.frame_width    = W;
   v.frame_height   = H;
   v.content_width  = W;
   v.content_height = H;

   ask(&fb, W, H);
   CHECK(GET_FB(&v, &fb));
   CHECK(fb.data == pixels && fb.pitch == W * 4 && screen.locks == 1);
   CHECK(fb.format == RETRO_PIXEL_FORMAT_XRGB8888);
   CHECK(TAKE(&v, fb.data, W * 4));
   CHECK(screen.locks == 0 && !v.sw_lend && retired == 1);

   /* A skipped frame leaves the loan open: locked once, lent again */
   ask(&fb, W, H);
   CHECK(GET_FB(&v, &fb));
   ask(&fb, W, H);
   CHECK(GET_FB(&v, &fb));
   CHECK(screen.locks == 1);

   /* The core's own buffer goes through the blit; the lock is let go
    * and the cached frame kept */
   CHECK(!TAKE(&v, own, W * 4));
   CHECK(screen.locks == 0 && !v.sw_lend && retired == 1);

   /* A loan pushed at another pitch is not the frame */
   ask(&fb, W, H);
   CHECK(GET_FB(&v, &fb));
   CHECK(!TAKE(&v, fb.data, W * 2));
   CHECK(screen.locks == 0);

   ask(&fb, W, H);
   fb.access_flags |= RETRO_MEMORY_ACCESS_READ;
   CHECK(!GET_FB(&v, &fb));
   ask(&fb, W - 8, H);
   CHECK(!GET_FB(&v, &fb));
   v.menu_active = true;
   ask(&fb, W, H);
   CHECK(!GET_FB(&v, &fb));
   v.menu_active = false;
   v.was_in_menu = true;
   CHECK(!GET_FB(&v, &fb));
   v.was_in_menu = false;
   v.mode_valid  = false;
   CHECK(!GET_FB(&v, &fb));
   v.mode_valid  = true;
   screen.pitch  = W * 4 + 32;
   CHECK(!GET_FB(&v, &fb));
   screen.pitch  = W * 2;
   v.rgb32       = false;
   CHECK(GET_FB(&v, &fb));
   CHECK(fb.format == RETRO_PIXEL_FORMAT_RGB565 && fb.pitch == W * 2);
   TAKE(&v, NULL, 0);

#ifdef DINGUX_LANE
   /* Padding above the picture: lent from where the picture starts */
   v.rgb32           = true;
   screen.pitch      = W * 4;
   v.frame_padding_y = 8;
   ask(&fb, W, H);
   CHECK(GET_FB(&v, &fb));
   CHECK(fb.data == (uint8_t*)pixels + 8 * W * 4);
   TAKE(&v, NULL, 0);
#endif
   CHECK(screen.locks == 0);

   puts("PASS: " LANE " software framebuffer lend");
   return 0;
}
