#include <stdio.h>
/* Minimal stand-ins for the layers below font_driver.c. */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <pthread.h>
#include <sched.h>
#include <boolean.h>
#include <compat/strl.h>
#include "gfx/font_driver.h"
#include "gfx/video_driver.h"
#include "configuration.h"

extern int read_should_fail;

/* font_driver.c logs when it discards a stale OSD font or fails to
 * rebuild one. Silent here: the tests assert on state, not output. */
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }

/* Never threaded and never hw-render in these tests, so the rebuild
 * takes the direct route rather than marshalling to a video thread
 * that does not exist. */
bool video_driver_is_hw_context(void) { return false; }

bool path_is_valid(const char *path) { (void)path; return true; }

/* font_driver_resolve_params() reads the on-screen message settings */
static settings_t test_settings;
settings_t *config_get_ptr(void) { return &test_settings; }

const char *last_read_path = NULL;

/* Set by the fallback tests, which read real fonts from disk, and the
 * files they read, in order. Fallback files are read on threads of
 * their own, so the record is kept under a lock. */
int  read_real_files = 0;
static char real_reads[16][512];
static int  real_reads_n = 0;
static pthread_mutex_t real_reads_lock = PTHREAD_MUTEX_INITIALIZER;

/* Whether a file whose path contains @name has been read */
int stub_real_read(const char *name)
{
   int i, found = 0;
   pthread_mutex_lock(&real_reads_lock);
   for (i = 0; i < real_reads_n && !found; i++)
      found = strstr(real_reads[i], name) != NULL;
   pthread_mutex_unlock(&real_reads_lock);
   return found;
}

bool filestream_read_file(const char *path, void **buf, int64_t *len)
{
   /* The real reads come from more than one thread; only the junk
    * reads the lifecycle tests make are recorded */
   if (read_real_files)
   {
      long  n;
      FILE *f = fopen(path, "rb");
      pthread_mutex_lock(&real_reads_lock);
      if (real_reads_n < (int)(sizeof(real_reads) / sizeof(real_reads[0])))
         strlcpy(real_reads[real_reads_n++], path, sizeof(real_reads[0]));
      pthread_mutex_unlock(&real_reads_lock);
      *buf    = NULL;
      if (!f)
         return false;
      fseek(f, 0, SEEK_END);
      n = ftell(f);
      fseek(f, 0, SEEK_SET);
      if (n <= 0 || !(*buf = malloc((size_t)n))
            || fread(*buf, 1, (size_t)n, f) != (size_t)n)
      {
         free(*buf);
         *buf = NULL;
         fclose(f);
         return false;
      }
      fclose(f);
      if (len)
         *len = n;
      return true;
   }
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
const font_rasterizer_t stb_font_rasterizer;
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

/* font_driver.c asks this before routing a font call through the
 * threaded wrapper. This sample drives that path deliberately - its
 * whole subject is what the wrapper does to a font's lifetime - so it
 * answers the same as video_driver_is_threaded() above. */
bool video_driver_thread_wrapper_active(void) { return true; }

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

#ifndef FONT_TEST_REAL_THREADS
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
/* No fallback font is read here: a thread that cannot be made leaves
 * font_driver.c drawing the missing-glyph mark, as without fallbacks. */
sthread_t *sthread_create(void (*fn)(void*), void *userdata)
{ (void)fn; (void)userdata; return NULL; }
int sthread_detach(sthread_t *thread) { (void)thread; return 0; }
/* rtime_localtime()'s guard yields while another caller holds it. */
void sthread_yield(void) { sched_yield(); }
#endif
#endif

/* font_driver.c sends gfx_display's batch out before it draws text,
 * so that quads asked for first land under it. There is no batch
 * here; the pointer is a blob for the symbol to return. */
void *disp_get_ptr(void) { static char b[8192]; return b; }
void gfx_display_flush_batch(void *p_disp) { (void)p_disp; }
