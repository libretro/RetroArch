/* The frontend the pack's upload calls into, reduced to a driver that
 * keeps what it is given. A texture is a checksum of the pixels it
 * was made from, read in full at the upload - so a texture made from
 * freed memory is an ASan report, not a wrong picture nobody sees. */

#include <stdint.h>
#include <stdlib.h>
#include <boolean.h>

#include "../../../gfx/video_driver.h"
#include "../../../gfx/gfx_surface.h"

#ifdef HAVE_THREADS
#include "../../../gfx/video_thread_wrapper.h"
#endif

#include "stubs_retroarch.h"

struct stub_texture stub_tex[STUB_MAX_TEXTURES];
unsigned stub_tex_loads;
unsigned stub_tex_live;
bool     stub_tex_load_fails;

uint32_t stub_checksum(const struct texture_image *img)
{
   size_t i, n  = (size_t)img->width * img->height;
   uint32_t sum = 2166136261u;
   for (i = 0; i < n; i++)
      sum = (sum ^ img->pixels[i]) * 16777619u;
   return sum;
}

void stub_reset(void)
{
   unsigned i;
   for (i = 0; i < STUB_MAX_TEXTURES; i++)
      stub_tex[i].live = false;
   stub_tex_loads      = 0;
   stub_tex_live       = 0;
   stub_tex_load_fails = false;
}

const struct stub_texture *stub_texture_get(uintptr_t id)
{
   if (!id || id > STUB_MAX_TEXTURES || !stub_tex[id - 1].live)
      return NULL;
   return &stub_tex[id - 1];
}

bool video_driver_texture_load(void *data,
      enum texture_filter_type filter, uintptr_t *id)
{
   unsigned i;
   const struct texture_image *img = (const struct texture_image*)data;
   (void)filter;
   if (stub_tex_load_fails || !img || !img->pixels)
      return false;
   for (i = 0; i < STUB_MAX_TEXTURES; i++)
   {
      if (stub_tex[i].live)
         continue;
      stub_tex[i].live     = true;
      stub_tex[i].checksum = stub_checksum(img);
      stub_tex[i].width    = img->width;
      stub_tex[i].height   = img->height;
      stub_tex_loads++;
      stub_tex_live++;
      *id = (uintptr_t)(i + 1);
      return true;
   }
   return false;
}

bool video_driver_texture_unload(uintptr_t *id)
{
   if (*id && *id <= STUB_MAX_TEXTURES && stub_tex[*id - 1].live)
   {
      stub_tex[*id - 1].live = false;
      stub_tex_live--;
   }
   *id = 0;
   return true;
}

bool video_driver_texture_can_update(void) { return true; }

bool video_driver_texture_update(uintptr_t id, void *data)
{
   const struct texture_image *img = (const struct texture_image*)data;
   if (!id || id > STUB_MAX_TEXTURES || !stub_tex[id - 1].live)
      return false;
   stub_tex[id - 1].checksum = stub_checksum(img);
   return true;
}

uint32_t video_driver_get_disp_flags(void) { return 0; }

bool video_driver_test_all_flags(enum display_flags testflag)
{
   (void)testflag;
   return false;
}

bool video_driver_supports_texture_format(enum texture_gpu_format fmt)
{
   (void)fmt;
   return false;
}

enum texture_filter_type gfx_display_texture_filter(void)
{
   return TEXTURE_FILTER_LINEAR;
}

#ifdef HAVE_THREADS
/* The threaded wrapper, without the thread. A post queues the node
 * exactly as video_thread_async_post() does; the "video thread" is
 * stub_video_thread_run(), which the test calls at the point in the
 * sequence where it wants that thread to have got round to its queue;
 * and video_thread_async_poll() delivers what has completed, on the
 * caller, as the real one does. The race between the post and the
 * poll is therefore the test's to decide rather than the scheduler's,
 * and both outcomes are lanes. */
bool stub_thread_active;
bool stub_thread_wins_race;

static video_thread_async_load_t *stub_in_head,  *stub_in_tail;
static video_thread_async_load_t *stub_out_head, *stub_out_tail;

bool video_driver_thread_wrapper_active(void) { return stub_thread_active; }

unsigned stub_video_thread_run(void)
{
   unsigned ran                 = 0;
   video_thread_async_load_t *n = stub_in_head;
   stub_in_head = stub_in_tail  = NULL;
   while (n)
   {
      video_thread_async_load_t *next = n->next;
      /* This is where the video thread reads the poster's pixels. */
      if (n->kind == VIDEO_THREAD_ASYNC_LOAD)
      {
         uintptr_t id = 0;
         video_driver_texture_load(n->img, n->filter, &id);
         n->handle    = id;
      }
      else if (!video_driver_texture_update(n->handle, n->img))
         n->handle    = 0;
      n->next = NULL;
      if (stub_out_tail)
         stub_out_tail->next = n;
      else
         stub_out_head       = n;
      stub_out_tail          = n;
      n = next;
      ran++;
   }
   return ran;
}

bool video_thread_async_post(video_thread_async_load_t *n)
{
   if (!stub_thread_active || !n)
      return false;
   n->next         = NULL;
   n->caller_owned = 1;
   if (stub_in_tail)
      stub_in_tail->next = n;
   else
      stub_in_head       = n;
   stub_in_tail          = n;
   if (stub_thread_wins_race)
      stub_video_thread_run();
   return true;
}

void video_thread_async_poll(void)
{
   video_thread_async_load_t *n = stub_out_head;
   stub_out_head = stub_out_tail = NULL;
   while (n)
   {
      video_thread_async_load_t *next = n->next;
      if (n->done)
         n->done(n->user, n->handle);
      n = next;
   }
}
#endif
