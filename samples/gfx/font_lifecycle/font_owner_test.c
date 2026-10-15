/* The font driver's state shared between threads, and the state each
 * thread keeps to itself.
 *
 * The bytes of a font file are shared by every font built from it, on
 * any thread - the main thread's menu fonts, the video thread's, the
 * fallback loader's - and are taken and let go without a lock. Several
 * threads here take and let go of the same file at once; under
 * ThreadSanitizer anything they share unordered is a reported race,
 * under AddressSanitizer bytes freed while held are a use after free,
 * and at the end the file's entry holds nothing.
 *
 * A font belongs to the thread that made it: only that thread finds
 * it, rebuilds it for a new language and frees it once retired. A font
 * made on the video thread is checked to stay out of the main thread's
 * reach, and to follow a language change and come due on its own
 * thread. A font retired on another thread is handed back to its
 * owner, from several threads at once, and freed there exactly once.
 *
 * font_driver.c is included rather than linked, for the file cache. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>

#include "../../../gfx/font_driver.c"

#define READERS 4
#define ROUNDS  2000
#define BYTES   4096

#define TEST_LANG_KOREAN 10   /* RETRO_LANGUAGE_KOREAN */

extern pthread_t test_video_thread;
extern int       test_video_thread_set;
extern unsigned  test_language;
extern int       read_real_files;

/* The stubs' junk reads, which these tests do not make */
int read_should_fail = 0;

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

/* --- a backend that only counts --- */
static int live_states = 0;
static pthread_mutex_t live_lock = PTHREAD_MUTEX_INITIALIZER;

static void *owner_init(void *data, const char *font_path,
      float font_size, bool is_threaded)
{
   float *st = (float*)malloc(sizeof(*st));
   (void)data; (void)font_path; (void)is_threaded;
   if (!st)
      return NULL;
   *st = font_size;
   pthread_mutex_lock(&live_lock);
   live_states++;
   pthread_mutex_unlock(&live_lock);
   return st;
}

static void owner_free(void *data, bool is_threaded)
{
   (void)is_threaded;
   free(data);
   pthread_mutex_lock(&live_lock);
   live_states--;
   pthread_mutex_unlock(&live_lock);
}

static int owner_width(void *data, const char *msg, size_t len,
      float scale)
{ (void)data; (void)msg; (void)scale; return (int)len * 8; }

static font_renderer_t owner_font = {
   owner_init, owner_free, NULL, "owner",
   NULL, NULL, NULL, owner_width, NULL
};

static int live(void)
{
   int n;
   pthread_mutex_lock(&live_lock);
   n = live_states;
   pthread_mutex_unlock(&live_lock);
   return n;
}

static font_data_t *mk(const char *path)
{
   font_data_t *font = font_driver_init_first(NULL, path, 16.0f, false,
         false, &owner_font);
   if (font)
      font_driver_set_language_font(font, "/assets/pkg", path);
   return font;
}

/* --- the file cache --- */
static const char *file_path = "font_owner_test.bin";
static retro_atomic_int_t readers_bad;

static void *reader(void *unused)
{
   int i;
   (void)unused;
   for (i = 0; i < ROUNDS; i++)
   {
      font_file_ref_t *ref = font_file_ref_acquire(file_path);
      if (     !ref || ref->len != BYTES
            || ref->data[0] != 0x5A || ref->data[BYTES - 1] != 0xA5)
         retro_atomic_fetch_add_int(&readers_bad, 1);
      font_file_ref_release(ref);
   }
   return NULL;
}

/* --- the video thread's phases --- */
static font_data_t *video_font;
static int          video_rebuilt;

static void video_thread_begin(void)
{
   test_video_thread     = pthread_self();
   test_video_thread_set = 1;
}

static void *video_make(void *unused)
{
   (void)unused;
   video_thread_begin();
   video_font = mk("/tmp/san/video.ttf");
   return NULL;
}

static void *video_follow(void *unused)
{
   (void)unused;
   video_thread_begin();
   video_rebuilt = (int)font_driver_follow_language();
   font_driver_free_deferred(video_font);
   return NULL;
}

static void *video_frames(void *unused)
{
   (void)unused;
   video_thread_begin();
   font_driver_free_pending(false);
   font_driver_free_pending(false);
   return NULL;
}

/* --- handing back from other threads --- */
#define RETIRES (READERS * 4)
static font_data_t       *retiring[RETIRES];
static retro_atomic_int_t retire_done;
static retro_atomic_int_t retire_ready;

static void *retirer(void *arg)
{
   int r;
   for (r = (int)(intptr_t)arg; r < RETIRES; r += READERS)
      font_driver_free_deferred(retiring[r]);
   return NULL;
}

/* Frames until the retirers are done, then the teardown flush */
static void *video_drain(void *unused)
{
   (void)unused;
   video_thread_begin();
   retro_atomic_store_release_int(&retire_ready, 1);
   while (!retro_atomic_load_acquire_int(&retire_done))
      font_driver_free_pending(false);
   font_driver_free_pending(true);
   return NULL;
}

static void run(void *(*fn)(void*))
{
   pthread_t t;
   pthread_create(&t, NULL, fn, NULL);
   pthread_join(t, NULL);
   test_video_thread_set = 0;
}

int main(void)
{
   int i;
   pthread_t readers[READERS];
   font_data_t *main_font;
   font_file_ref_t *entry;

   font_driver_bind_video_state(video_state_get_ptr());

   /* 1. One file, taken and let go by several threads at once */
   {
      unsigned char *bytes = (unsigned char*)calloc(1, BYTES);
      FILE *f              = fopen(file_path, "wb");
      if (!bytes || !f)
         return 1;
      bytes[0]         = 0x5A;
      bytes[BYTES - 1] = 0xA5;
      fwrite(bytes, 1, BYTES, f);
      fclose(f);
      free(bytes);
   }
   read_real_files = 1;
   for (i = 0; i < READERS; i++)
      pthread_create(&readers[i], NULL, reader, NULL);
   for (i = 0; i < READERS; i++)
      pthread_join(readers[i], NULL);
   read_real_files = 0;
   remove(file_path);

   CHECK(!retro_atomic_load_acquire_int(&readers_bad), "a reader saw bytes that were not the file's");
   for (entry = FONT_REFS_HEAD(); entry; entry = entry->next)
      if (string_is_equal(entry->path, file_path))
         break;
   CHECK(entry, "the file has an entry");
   CHECK(entry && retro_atomic_load_acquire_int(&entry->refs) == 0
         && !entry->data,
         "once every reference is let go the entry holds no bytes");

   /* 2. A font made on the video thread is that thread's */
   main_font = mk("/tmp/san/main.ttf");
   run(video_make);
   CHECK(main_font && main_font->owner == FONT_OWNER_MAIN,
         "a font made on the main thread is the main thread's");
   CHECK(video_font && video_font->owner == FONT_OWNER_VIDEO,
         "a font made on the video thread is the video thread's");
   CHECK(font_driver_matches(main_font, "/tmp/san/main.ttf", 16.0f),
         "the main thread finds its own font");
   CHECK(!font_driver_matches(video_font, "/tmp/san/video.ttf", 16.0f),
         "the main thread does not reach into the video thread's fonts");

   /* 3. A language change: each thread rebuilds its own */
   test_language = TEST_LANG_KOREAN;
   CHECK(font_driver_reload_fonts() == 1,
         "the main thread rebuilds its own font, and only that");
   CHECK(strstr(main_font->path, "korean") != NULL,
         "the main thread's font follows the language");
   CHECK(!strcmp(video_font->path, "/tmp/san/video.ttf"),
         "the video thread's font is left to the video thread");
   run(video_follow);
   CHECK(video_rebuilt == 1,
         "the video thread rebuilds its font when it sees the change");

   /* 4. A retired font comes due on its own thread's frames */
   i = live();
   font_driver_free_pending(true);
   CHECK(live() == i,
         "the main thread's frames do not free the video thread's font");
   run(video_frames);
   CHECK(live() == i - 1,
         "the video thread's frames free its retired font");

   /* 5. A font retired off its thread goes back to that thread */
   run(video_make);
   i = live();
   font_driver_free_deferred(video_font);
   font_driver_free_pending(true);
   CHECK(live() == i,
         "the main thread freed a font the video thread made");
   run(video_frames);
   CHECK(live() == i - 1,
         "the video thread's frames free what was handed back to it");

   /* 6. Handed back from several threads while the video thread frees */
   {
      pthread_t retirers[READERS];
      pthread_t drainer;
      int       r;
      for (r = 0; r < RETIRES; r++)
      {
         run(video_make);
         retiring[r] = video_font;
      }
      i = live();
      retro_atomic_store_release_int(&retire_done, 0);
      pthread_create(&drainer, NULL, video_drain, NULL);
      /* Which thread is the video thread is known before anyone asks */
      while (!retro_atomic_load_acquire_int(&retire_ready))
         sched_yield();
      for (r = 0; r < READERS; r++)
         pthread_create(&retirers[r], NULL, retirer, (void*)(intptr_t)r);
      for (r = 0; r < READERS; r++)
         pthread_join(retirers[r], NULL);
      retro_atomic_store_release_int(&retire_done, 1);
      pthread_join(drainer, NULL);
      test_video_thread_set = 0;
      CHECK(live() == i - RETIRES,
            "fonts handed back from several threads are all freed once");
   }

   font_driver_free(main_font);
   CHECK(live() == 0, "every renderer state is released");

   if (fails)
   {
      printf("%d failure(s)\n", fails);
      return 1;
   }
   printf("font_owner_test: all checks passed\n");
   return 0;
}
