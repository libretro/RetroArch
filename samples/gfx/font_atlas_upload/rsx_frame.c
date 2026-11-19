/* The RSX driver's flip wait and software framebuffer lend, extracted
 * from the shipping source: the wait parks on the flip handler's
 * eventcount until every queued flip is done, and the lend hands out the
 * frame texture the next frame goes into only once the RSX is past the
 * last draw that sampled it. */

#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200112L
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <boolean.h>
#include <libretro.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>
#include <retro_timers.h>

#define HAVE_THREADS 1

typedef uint32_t u32;
#define RSX_MAX_TEXTURES 4
#define RSX_FRAME_LABEL  198

typedef struct
{
   u32 *data;
   u32  size;
   u32  fence;
} rsx_texture_t;

typedef struct
{
   rsx_texture_t texture[RSX_MAX_TEXTURES];
   struct { bool is_threaded; } video_info;
   int  tex_index;
   bool hw_render;
   bool rgb32;
} rsx_t;

/* The GCM side, scripted */
static u32 label[256];
static u32 flip_status;
static u32 *gcmGetLabelAddress(const unsigned char index) { return &label[index]; }
static u32 gcmGetFlipStatus(void) { return flip_status; }

static retro_eventcount_t rsx_flip_ec;
static retro_atomic_int_t rsx_flips_done;
static int                rsx_flips_asked;
static bool               rsx_flip_ec_live;

#include "rsx_frame.inc"

/* The flip handler, called from another thread a little later */
static void flip_later(void *arg)
{
   (void)arg;
   retro_sleep_us(2000);
   flip_status = 0;
   rsx_flip_handler(0);
}

int main(void)
{
   rsx_t gcm;
   struct retro_framebuffer fb;
   u32 mem[4][64];
   unsigned i;

   /* ---- flip wait ---- */
   assert(retro_eventcount_init(&rsx_flip_ec));
   retro_atomic_int_init(&rsx_flips_done, 0);
   rsx_flip_ec_live = true;

   /* Nothing queued: no wait */
   rsx_wait_flip();

   /* A flip queued and done by the handler from another thread: the
    * wait returns once it has, and not before */
   for (i = 0; i < 50; i++)
   {
      sthread_t *t;
      rsx_flips_asked++;
      flip_status = 1;
      assert(t = sthread_create(flip_later, NULL));
      rsx_wait_flip();
      assert(retro_atomic_load_acquire_int(&rsx_flips_done) == rsx_flips_asked);
      sthread_join(t);
   }

   /* A handler that never comes: the watchdog finds the flip done */
   rsx_flips_asked++;
   flip_status = 0;
   rsx_wait_flip();
   retro_atomic_inc_int(&rsx_flips_done);
   retro_eventcount_free(&rsx_flip_ec);

   /* ---- lend ---- */
   memset(&gcm, 0, sizeof(gcm));
   memset(label, 0, sizeof(label));
   gcm.rgb32 = true;
   memset(&fb, 0, sizeof(fb));
   fb.width = 8; fb.height = 4;

   /* No texture yet */
   assert(!rsx_get_current_software_framebuffer(&gcm, &fb));
   for (i = 0; i < 4; i++)
   {
      gcm.texture[i].data = mem[i];
      gcm.texture[i].size = sizeof(mem[i]);
   }
   gcm.tex_index = 2;

   /* The next texture, once the RSX is past its last draw */
   gcm.texture[3].fence = 5;
   label[RSX_FRAME_LABEL] = 4;
   assert(!rsx_get_current_software_framebuffer(&gcm, &fb));
   label[RSX_FRAME_LABEL] = 5;
   assert(rsx_get_current_software_framebuffer(&gcm, &fb));
   assert(fb.data == mem[3] && fb.pitch == 8 * 4);
   assert(fb.format == RETRO_PIXEL_FORMAT_XRGB8888);
   assert(fb.memory_flags == RETRO_MEMORY_TYPE_CACHED);

   /* The serial wraps: still in order */
   gcm.texture[3].fence   = 2;
   label[RSX_FRAME_LABEL] = 0xFFFFFFF0u;
   assert(!rsx_get_current_software_framebuffer(&gcm, &fb));

   /* Wraps round the ring; refused when it does not hold the frame,
    * for a hardware core and under the threaded wrapper */
   label[RSX_FRAME_LABEL] = 100;
   gcm.tex_index = 3;
   assert(rsx_get_current_software_framebuffer(&gcm, &fb) && fb.data == mem[0]);
   fb.height = 9;
   assert(!rsx_get_current_software_framebuffer(&gcm, &fb));
   fb.height = 4;
   gcm.hw_render = true;
   assert(!rsx_get_current_software_framebuffer(&gcm, &fb));
   gcm.hw_render = false;
   gcm.video_info.is_threaded = true;
   assert(!rsx_get_current_software_framebuffer(&gcm, &fb));
   gcm.video_info.is_threaded = false;
   gcm.rgb32 = false;
   assert(rsx_get_current_software_framebuffer(&gcm, &fb));
   assert(fb.pitch == 8 * 2 && fb.format == RETRO_PIXEL_FORMAT_RGB565);

   puts("PASS RSX flip wait on the flip handler, lend fenced by the frame label");
   return 0;
}
