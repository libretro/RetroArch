/* gfx_display texture loads: a load into a slot that holds a texture
 * keeps that texture until the new one has landed, then unloads it,
 * under the threaded wrapper and without; a gfx_surface still keeps
 * its texture across uploads, decodes and a free in flight. Stub
 * driver, simulated wrapper and task queue. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <queues/task_queue.h>

#include "gfx/gfx_display.h"
#include "gfx/video_driver.h"
#include "gfx/video_thread_wrapper.h"
#include "gfx/gfx_surface.h"

/* --- stub driver ------------------------------------------------- */
static uintptr_t st_next = 1;
static int       st_live, st_loads, st_unloads, st_async;
static uintptr_t st_last_unloaded;
static video_thread_async_load_t *st_head, *st_tail;

bool video_driver_texture_load(void *data,
      enum texture_filter_type filter, uintptr_t *id)
{
   (void)data;
   (void)filter;
   *id = st_next++;
   st_loads++;
   st_live++;
   return true;
}

bool video_driver_texture_unload(uintptr_t *id)
{
   if (*id)
   {
      st_last_unloaded = *id;
      st_unloads++;
      st_live--;
   }
   *id = 0;
   return true;
}

enum video_texture_update video_driver_texture_update(uintptr_t id,
      void *data)
{
   (void)id;
   (void)data;
   return VIDEO_TEXTURE_UPDATE_DONE;
}

bool video_driver_texture_can_update(void) { return true; }
bool video_driver_thread_wrapper_active(void) { return st_async != 0; }
bool task_is_on_main_thread(void) { return true; }
unsigned video_driver_get_disp_flags(void) { return 0; }
bool video_driver_supports_texture_format(enum texture_gpu_format fmt)
{
   (void)fmt;
   return false;
}
void *video_driver_texture_lend(uintptr_t id, unsigned slot, size_t pitch)
{
   (void)id; (void)slot; (void)pitch;
   return NULL;
}
bool video_driver_texture_lend_ready(uintptr_t id, unsigned slot)
{
   (void)id; (void)slot;
   return true;
}

bool video_thread_async_post(video_thread_async_load_t *n)
{
   if (!st_async)
      return false;
   n->link.next = NULL;
   if (st_tail)
      st_tail->link.next = &n->link;
   else
      st_head = n;
   st_tail = n;
   return true;
}

/* The video thread's frame, then the main thread's completions */
static void st_flush(void)
{
   video_thread_async_load_t *n = st_head;
   st_head = st_tail = NULL;
   while (n)
   {
      video_thread_async_load_t *next = n->link.next
         ? (video_thread_async_load_t*)n->link.next : NULL;
      uintptr_t id = 0;
      if (n->kind == VIDEO_THREAD_ASYNC_UPDATE)
         id = n->handle;
      else
         video_driver_texture_load(n->img, n->filter, &id);
      if (n->done)
         n->done(n->user, id);
      n = next;
   }
}

/* The task queue: one decode held until the test answers it */
static retro_task_callback_t st_decode_cb;
static void                 *st_decode_user;
static unsigned              st_decodes;

bool task_push_image_load(const char *fullpath, bool supports_rgba,
      unsigned upscale_threshold, unsigned downscale_cap,
      retro_task_callback_t cb, void *user_data)
{
   (void)fullpath; (void)supports_rgba; (void)upscale_threshold;
   (void)downscale_cap;
   st_decode_cb   = cb;
   st_decode_user = user_data;
   st_decodes++;
   return true;
}

/* Answers the held decode with an image of @w x @w, or nothing */
static void st_decode_done(unsigned w)
{
   retro_task_callback_t cb = st_decode_cb;
   void *user               = st_decode_user;
   struct texture_image *img = NULL;
   st_decode_cb = NULL;
   if (w)
   {
      img = (struct texture_image*)calloc(1, sizeof(*img));
      img->width  = img->height = w;
      img->pixels = (uint32_t*)calloc(w * w, 4);
   }
   if (cb)
      cb(NULL, img, user, NULL);
}

static struct texture_image *st_image(unsigned w)
{
   struct texture_image *img = (struct texture_image*)calloc(1, sizeof(*img));
   img->width  = img->height = w;
   img->pixels = (uint32_t*)calloc(w * w, 4);
   return img;
}

/* --- the test ---------------------------------------------------- */
static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; \
   printf("[FAIL] " __VA_ARGS__); printf("\n"); } } while (0)

static bool load(uintptr_t *slot)
{
   struct texture_image ti;
   memset(&ti, 0, sizeof(ti));
   ti.width  = 8;
   ti.height = 8;
   ti.pixels = (uint32_t*)calloc(64, 4);
   if (!gfx_display_texture_load(&ti, TEXTURE_FILTER_NEAREST, slot))
   {
      free(ti.pixels);
      return false;
   }
   free(ti.pixels); /* a load that took the pixels left NULL here */
   return true;
}

int main(void)
{
   uintptr_t slot = 0, a, b;

   /* 1. threaded: the texture up stays until the new one lands */
   st_async = 1;
   CHECK(load(&slot), "threaded first load refused");
   CHECK(slot == 0, "a queued load wrote the slot early");
   st_flush();
   a = slot;
   CHECK(a != 0, "threaded first load never landed");
   CHECK(load(&slot), "threaded second load refused");
   CHECK(slot == a && st_unloads == 0,
         "the texture up went before the new one landed");
   st_flush();
   b = slot;
   CHECK(b != 0 && b != a, "threaded second load never landed");
   CHECK(st_unloads == 1 && st_last_unloaded == a && st_live == 1,
         "the replaced texture was not unloaded once (%d unloads, %d live)",
         st_unloads, st_live);

   /* 2. threaded: two loads queued at once - the later wins, nothing
    *    leaks */
   CHECK(load(&slot) && load(&slot), "queued pair refused");
   CHECK(slot == b, "a queued pair wrote the slot early");
   st_flush();
   CHECK(slot != 0 && slot != b && st_live == 1,
         "a queued pair left %d textures live", st_live);

   /* 3. direct: the old one goes at once, after the new one loaded */
   st_async = 0;
   a = slot;
   CHECK(load(&slot), "direct load refused");
   CHECK(slot != a && st_last_unloaded == a && st_live == 1,
         "direct replacement left %d live", st_live);

   /* 4. a cancelled load lands nowhere and leaks nothing */
   st_async = 1;
   CHECK(load(&slot), "load before cancel refused");
   gfx_display_texture_loads_cancel(&slot, sizeof(slot));
   a = slot;
   st_flush();
   CHECK(slot == a && st_live == 1, "a cancelled load wrote the slot");

   video_driver_texture_unload(&slot);
   CHECK(st_live == 0, "%d textures live at the end", st_live);

   /* --- a still that owns its texture ------------------------------ */

   /* 5. threaded: a same-size image updates the texture up in place
    *    once landed, another size replaces it, and one given while
    *    the first is in flight goes up after it */
   {
      gfx_surface_t *s = gfx_surface_new_still(TEXTURE_FILTER_NEAREST);
      int loads;
      CHECK(s != NULL, "no still");
      CHECK(gfx_surface_submit_image(s, st_image(8)), "first image refused");
      CHECK(s->handle == 0, "a queued image landed early");
      st_flush();
      a = s->handle;
      CHECK(a != 0, "first image never landed");
      loads = st_loads;
      CHECK(gfx_surface_submit_image(s, st_image(8)), "second image refused");
      CHECK(gfx_surface_submit_image(s, st_image(16)),
            "an image during the flight refused");
      CHECK(s->handle == a && st_live == 1, "the texture up went early");
      st_flush();
      CHECK(s->handle == a && st_loads == loads,
            "a same-size image did not update in place");
      st_flush();
      b = s->handle;
      CHECK(b != 0 && b != a && VIDEO_SCALE_W(s->dims) == 16 && st_live == 1,
            "the image given in flight did not go up after (%d live)",
            st_live);
      gfx_surface_free(s);
      CHECK(st_live == 0, "%d textures live after the still went", st_live);
   }

   /* 6. a still freed with an upload in flight: the completion frees
    *    it, texture and all */
   {
      gfx_surface_t *s = gfx_surface_new_still(TEXTURE_FILTER_NEAREST);
      CHECK(gfx_surface_submit_image(s, st_image(8)), "image refused");
      gfx_surface_submit_image(s, st_image(8)); /* queued behind */
      gfx_surface_free(s);
      st_flush();
      CHECK(st_live == 0, "a still freed in flight left %d live", st_live);
   }

   /* 7. a decode: the file's image goes up when the decode answers;
    *    a newer path makes the older decode land nowhere; a free
    *    while decoding is honoured at the answer */
   {
      gfx_surface_t *s = gfx_surface_new_still(TEXTURE_FILTER_NEAREST);
      retro_task_callback_t first_cb; void *first_user;
      CHECK(gfx_surface_submit_path(s, "a.png", true), "decode refused");
      first_cb   = st_decode_cb;
      first_user = st_decode_user;
      CHECK(gfx_surface_submit_path(s, "b.png", true), "second decode refused");
      /* the first answers: stale, lands nowhere */
      {
         struct texture_image *img = st_image(4);
         first_cb(NULL, img, first_user, NULL);
      }
      st_flush();
      CHECK(s->handle == 0 && st_live == 0, "a stale decode landed");
      st_decode_done(8);
      st_flush();
      CHECK(s->handle != 0 && VIDEO_SCALE_W(s->dims) == 8 && st_live == 1,
            "the decode did not land");
      CHECK(gfx_surface_submit_path(s, "c.png", true), "third decode refused");
      gfx_surface_free(s);
      CHECK(st_live == 1, "a still freed while decoding went at once");
      st_decode_done(8);
      st_flush();
      CHECK(st_live == 0, "a still freed while decoding left %d live",
            st_live);
   }

   /* 8. direct: the image goes up at once */
   st_async = 0;
   {
      gfx_surface_t *s = gfx_surface_new_still(TEXTURE_FILTER_NEAREST);
      CHECK(gfx_surface_submit_image(s, st_image(8)) && s->handle != 0,
            "direct image did not land");
      a = s->handle;
      CHECK(gfx_surface_submit_image(s, st_image(8)) && s->handle != a
            && st_last_unloaded == a && st_live == 1,
            "direct replacement left %d live", st_live);
      gfx_surface_free(s);
      CHECK(st_live == 0, "%d textures live at the end", st_live);
   }

   printf("%s\n", failures ? "FAILED" : "PASS");
   return failures ? 1 : 0;
}
