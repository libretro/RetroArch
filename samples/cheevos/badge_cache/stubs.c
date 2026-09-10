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

/* ---- test-visible state ---- */
int      st_on_main_thread = 1;
char     st_badge_dir[256] = "/tmp";
char     st_existing[8][64];          /* badge files that "exist" */
unsigned st_existing_count;
unsigned st_downloads;                /* rcheevos_badge_request_download calls */
char     st_last_download[64];
unsigned st_uploads;                  /* texture handles minted */
unsigned st_unloads;
uintptr_t st_last_unloaded;

/* parked image-load tasks */
typedef struct
{
   char path[256];
   retro_task_callback_t cb;
   void *user;
} parked_t;
parked_t st_parked[16];
unsigned st_parked_count;

/* parked async uploads */
typedef struct
{
   void *img;
   void (*done)(void*, uintptr_t);
   void *user;
   void (*release)(void*);
} upload_t;
upload_t st_uploads_pending[16];
unsigned st_uploads_pending_count;
int      st_async_available = 1;

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
uint32_t video_driver_get_disp_flags(void) { return 0; }

bool task_push_image_load(const char *fullpath, bool supports_rgba,
      unsigned upscale_threshold, unsigned downscale_cap,
      retro_task_callback_t cb, void *userdata)
{
   (void)supports_rgba; (void)upscale_threshold; (void)downscale_cap;
   if (st_parked_count >= 16)
      return false;
   strlcpy(st_parked[st_parked_count].path, fullpath, sizeof(st_parked[0].path));
   st_parked[st_parked_count].cb   = cb;
   st_parked[st_parked_count].user = userdata;
   st_parked_count++;
   return true;
}

bool video_driver_texture_load_async(void *data,
      enum texture_filter_type filter,
      void (*done)(void *user, uintptr_t handle), void *user,
      void (*release)(void *img))
{
   (void)filter;
   if (!st_async_available)
      return false;
   if (st_uploads_pending_count >= 16)
      return false;
   st_uploads_pending[st_uploads_pending_count].img     = data;
   st_uploads_pending[st_uploads_pending_count].done    = done;
   st_uploads_pending[st_uploads_pending_count].user    = user;
   st_uploads_pending[st_uploads_pending_count].release = release;
   st_uploads_pending_count++;
   return true;
}

bool video_driver_texture_unload(uintptr_t *id)
{
   st_unloads++;
   st_last_unloaded = *id;
   *id = 0;
   return true;
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

/* Complete parked upload #i: release the image, hand a handle (or 0). */
void st_finish_upload(unsigned i, bool ok)
{
   upload_t u = st_uploads_pending[i];
   uintptr_t h = 0;
   memmove(&st_uploads_pending[i], &st_uploads_pending[i + 1],
         (st_uploads_pending_count - i - 1) * sizeof(upload_t));
   st_uploads_pending_count--;
   if (u.release)
      u.release(u.img);
   if (ok)
      h = 0x1000 + ++st_uploads;
   u.done(u.user, h);
}
