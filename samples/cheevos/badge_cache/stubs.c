/* Stubs for badge_cache_test: the frontend surface cheevos_badge.c
 * uses, made steppable. Loads are parked; the test completes them. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <boolean.h>
#include <compat/strl.h>
#include <formats/image.h>
#include <queues/task_queue.h>

#include "../../../gfx/gfx_display.h"
#include "../../../gfx/video_driver.h"
#include "../../../file_path_special.h"
#include "../../../tasks/tasks_internal.h"
#include "../../../cheevos/cheevos.h"
#include "../../../gfx/gfx_surface.h"

/* ---- test-visible state ---- */
int      st_on_main_thread = 1;
char     st_badge_dir[256] = "/tmp";
char     st_existing[32][64];          /* badge files that "exist" */
unsigned st_existing_count;
unsigned st_downloads;                /* rcheevos_badge_request_download calls */
char     st_last_download[64];
unsigned st_uploads;                  /* texture handles minted */
unsigned st_unloads;                  /* surfaces freed with a texture */
uintptr_t st_last_unloaded;
unsigned st_surfaces_live;            /* stills made and not yet freed */

/* parked image-load tasks */
typedef struct
{
   char path[256];
   retro_task_callback_t cb;
   void *user;
} parked_t;
parked_t st_parked[32];
unsigned st_parked_count;

/* parked uploads: a still given an image while the wrapper is "up" */
typedef struct
{
   gfx_surface_t *s;
   struct texture_image *img;
} upload_t;
upload_t st_uploads_pending[16];
unsigned st_uploads_pending_count;
int      st_async_available = 1;      /* 1: uploads park; 0: they land at once */
int      st_upload_refused;           /* 1: the still refuses the image */

/* ---- stubs ---- */
bool task_is_on_main_thread(void) { return st_on_main_thread != 0; }

/* image_texture_free from formats/image_texture.c pulls in every
 * decoder; the test's images are plain calloc pixels. */
void image_texture_free(struct texture_image *img)
{
   if (img && img->pixels)
      free(img->pixels);
   if (img)
      img->pixels = NULL;
}

size_t fill_pathname_application_special(char *s, size_t len, enum application_special_type type)
{
   (void)type;
   return strlcpy(s, st_badge_dir, len);
}

/* file_path.c's path_is_valid stats the file; override with the table. */
bool path_is_valid(const char *path)
{
   unsigned i;
   const char *base = strrchr(path, '/');
   base = base ? base + 1 : path;
   for (i = 0; i < st_existing_count; i++)
      if (!strcmp(st_existing[i], base))
         return true;
   return false;
}

void rcheevos_badge_request_download(const char* badge, bool locked)
{
   (void)locked;
   st_downloads++;
   strlcpy(st_last_download, badge, sizeof(st_last_download));
}

enum texture_filter_type gfx_display_texture_filter(void) { return TEXTURE_FILTER_LINEAR; }
enum texture_filter_type gfx_display_texture_filter_latched(void) { return TEXTURE_FILTER_LINEAR; }
uint32_t video_driver_get_disp_flags(void) { return 0; }

/* The badge loader asks the surface layer what the driver wants
 * before it queues a decode; there is no driver here, so the answer
 * is what a software path takes: ARGB words, 8 bits a channel, no
 * in-place texture update. */
bool gfx_surface_query_requirements(unsigned width,
      gfx_surface_requirements_t *req)
{
   if (!req)
      return false;
   req->rgba       = false;
   req->formats    = GFX_SURFACE_PIXFMT_8888;
   req->preferred  = GFX_SURFACE_PIXFMT_8888;
   req->can_update = false;
   req->pitch      = (size_t)width * sizeof(uint32_t);
   req->align      = 4;
   return true;
}

bool task_push_image_load(const char *fullpath, bool supports_rgba,
      unsigned upscale_threshold, unsigned downscale_cap,
      retro_task_callback_t cb, void *userdata)
{
   (void)supports_rgba; (void)upscale_threshold; (void)downscale_cap;
   if (st_parked_count >= 32)
      return false;
   strlcpy(st_parked[st_parked_count].path, fullpath, sizeof(st_parked[0].path));
   st_parked[st_parked_count].cb   = cb;
   st_parked[st_parked_count].user = userdata;
   st_parked_count++;
   return true;
}

/* The surface layer, as the badge cache sees it: a still is made
 * empty, takes one image, and either has its texture at once (direct
 * video) or after a completion the test drives (threaded video). */
gfx_surface_t *gfx_surface_new_still(enum texture_filter_type filter)
{
   gfx_surface_t *s = (gfx_surface_t*)calloc(1, sizeof(*s));
   if (s)
   {
      s->filter = filter;
      st_surfaces_live++;
   }
   return s;
}

static void st_image_free(struct texture_image *img)
{
   image_texture_free(img);
   free(img);
}

bool gfx_surface_submit_image(gfx_surface_t *s, struct texture_image *img)
{
   if (!img)
      return false;
   if (!s || !img->width || !img->height || !img->pixels || st_upload_refused)
   {
      st_image_free(img);
      return false;
   }
   if (st_async_available && st_uploads_pending_count < 16)
   {
      st_uploads_pending[st_uploads_pending_count].s   = s;
      st_uploads_pending[st_uploads_pending_count].img = img;
      st_uploads_pending_count++;
      s->inflight = 1;
      return true;
   }
   st_image_free(img);
   s->handle = 0x1000 + ++st_uploads;
   return true;
}

void gfx_surface_free(gfx_surface_t *s)
{
   if (!s)
      return;
   if (s->handle)
   {
      st_unloads++;
      st_last_unloaded = s->handle;
   }
   st_surfaces_live--;
   free(s);
}

/* ---- steps the test drives ---- */

/* Complete parked decode #i with a fake image (ok) or NULL (failed). */
void st_finish_decode(unsigned i, bool ok)
{
   parked_t p = st_parked[i];
   struct texture_image *img = NULL;
   memmove(&st_parked[i], &st_parked[i + 1], (st_parked_count - i - 1) * sizeof(parked_t));
   st_parked_count--;
   if (ok)
   {
      img = (struct texture_image*)calloc(1, sizeof(*img));
      img->width = img->height = 8;
      img->pixels = (uint32_t*)calloc(64, 4);
   }
   p.cb(NULL, img, p.user, ok ? NULL : "decode failed");
}

/* Complete parked upload #i: the image is consumed, the still has its
 * texture (or none, failed), and its release callback runs. */
void st_finish_upload(unsigned i, bool ok)
{
   upload_t u = st_uploads_pending[i];
   memmove(&st_uploads_pending[i], &st_uploads_pending[i + 1],
         (st_uploads_pending_count - i - 1) * sizeof(upload_t));
   st_uploads_pending_count--;
   st_image_free(u.img);
   u.s->inflight = 0;
   if (ok)
      u.s->handle = 0x1000 + ++st_uploads;
   if (u.s->release)
      u.s->release(u.s->user, u.s, 0);
}
