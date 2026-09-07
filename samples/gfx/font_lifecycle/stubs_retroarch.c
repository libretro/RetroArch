/* Minimal stand-ins for the layers below font_driver.c. */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <boolean.h>
#include "gfx/font_driver.h"
#include "gfx/video_driver.h"

extern int read_should_fail;

/* font_driver.c logs when it discards a stale OSD font or fails to
 * rebuild one. Silent here: the tests assert on state, not output. */
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }

/* Never threaded and never hw-render in these tests, so the rebuild
 * takes the direct route rather than marshalling to a video thread
 * that does not exist. */
bool video_driver_is_hw_context(void) { return false; }

bool path_is_valid(const char *path) { (void)path; return true; }

const char *last_read_path = NULL;

bool filestream_read_file(const char *path, void **buf, int64_t *len)
{
   last_read_path = path;
   if (read_should_fail)
   {
      *buf = NULL;
      if (len) *len = 0;
      return false;
   }
   /* Not a real TTF: the renderer must reject it, and whoever read it
    * must release it. */
   *buf = calloc(1, 64);
   if (len) *len = 64;
   return *buf != NULL;
}

/* font_driver_language_font_file() asks which language is selected.
 * English here: the tests are about the lifecycle, not the mapping. */
unsigned test_language = 0;   /* RETRO_LANGUAGE_ENGLISH */

unsigned *msg_hash_get_uint(enum msg_hash_action type)
{

   (void)type;
   return &test_language;
}

static video_driver_state_t vst;
video_driver_state_t *video_state_get_ptr(void) { return &vst; }

/* The lifecycle test drives a stub backend and never links the real
 * stb renderer, so font_driver.c's reference to it needs satisfying.
 * The create test links stb.c itself and defines it for real. */
#ifndef FONT_TEST_REAL_STB
font_renderer_driver_t stb_font_renderer;
#endif

#ifdef HAVE_THREADS
/* Built a second time with HAVE_THREADS so the marshalling branch is
 * exercised rather than compiled away.
 *
 * The stub runs the init on a real thread and joins it, as the video
 * thread does, so a sanitizer sees the same cross-thread handoff the
 * frontend performs. */
#include <pthread.h>
#include "gfx/video_thread_wrapper.h"

int video_thread_font_init_calls = 0;

bool video_driver_is_threaded(void) { return true; }

uintptr_t video_thread_texture_handle(void *data,
      uintptr_t (*handle_get)(void *data))
{ return handle_get ? handle_get(data) : 0; }

typedef struct
{
   const void                  **font_driver;
   void                        **font_handle;
   void                         *data;
   const char                   *font_path;
   const font_renderer_t        *backend;
   custom_font_command_method_t  func;
   float                         font_size;
   bool                          is_threaded;
   bool                          ret;
} font_init_job_t;

static void *font_init_worker(void *p)
{
   font_init_job_t *j = (font_init_job_t*)p;
   j->ret = j->func(j->font_driver, j->font_handle, j->data,
         j->font_path, j->font_size, j->backend, j->is_threaded);
   return NULL;
}

bool video_thread_font_init(const void **font_driver, void **font_handle,
      void *data, const char *font_path, float video_font_size,
      const font_renderer_t *backend, custom_font_command_method_t func,
      bool is_threaded)
{
   font_init_job_t job;
   pthread_t       tid;

   video_thread_font_init_calls++;

   job.font_driver = font_driver;
   job.font_handle = font_handle;
   job.data        = data;
   job.font_path   = font_path;
   job.backend     = backend;
   job.func        = func;
   job.font_size   = video_font_size;
   job.is_threaded = is_threaded;
   job.ret         = false;

   if (pthread_create(&tid, NULL, font_init_worker, &job))
      return func(font_driver, font_handle, data, font_path,
            video_font_size, backend, is_threaded);
   pthread_join(tid, NULL);
   return job.ret;
}

/* font_driver.c's shared-bytes bookkeeping takes a lock under
 * HAVE_THREADS. A real mutex, so a sanitizer can see the ordering it
 * establishes. */
slock_t *slock_new(void)
{
   pthread_mutex_t *m = (pthread_mutex_t*)calloc(1, sizeof(*m));
   if (m && pthread_mutex_init(m, NULL) != 0)
   {
      free(m);
      return NULL;
   }
   return (slock_t*)m;
}
void slock_free(slock_t *l)
{
   if (l)
   {
      pthread_mutex_destroy((pthread_mutex_t*)l);
      free(l);
   }
}
void slock_lock(slock_t *l)   { pthread_mutex_lock((pthread_mutex_t*)l); }
void slock_unlock(slock_t *l) { pthread_mutex_unlock((pthread_mutex_t*)l); }
#endif
