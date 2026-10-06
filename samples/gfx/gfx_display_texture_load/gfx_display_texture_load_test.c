/* gfx_display texture loads: a load into a slot that holds a texture
 * keeps that texture until the new one has landed, then unloads it,
 * under the threaded wrapper and without; the white texture loads
 * once. Stub driver, simulated wrapper. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

   /* 4. the white texture: loaded once, however often it is asked for */
   st_async = 1;
   gfx_display_deinit_white_texture();
   {
      int loads = st_loads, live = st_live;
      gfx_display_init_white_texture();
      gfx_display_init_white_texture();
      st_flush();
      CHECK(st_loads == loads + 1 && st_live == live + 1,
            "the white texture loaded %d times", st_loads - loads);
      gfx_display_init_white_texture();
      st_flush();
      CHECK(st_loads == loads + 1, "a loaded white texture loaded again");
      gfx_display_deinit_white_texture();
      CHECK(st_live == live, "deinit left the white texture");
   }

   /* 5. a cancelled load lands nowhere and leaks nothing */
   CHECK(load(&slot), "load before cancel refused");
   gfx_display_texture_loads_cancel(&slot, sizeof(slot));
   a = slot;
   st_flush();
   CHECK(slot == a && st_live == 1, "a cancelled load wrote the slot");

   video_driver_texture_unload(&slot);
   CHECK(st_live == 0, "%d textures live at the end", st_live);

   printf("%s\n", failures ? "FAILED" : "PASS");
   return failures ? 1 : 0;
}
