/* play_feature_delivery.c on the host, against the Android stubs in
 * tools/platform_stubs/android.
 *
 * The stub Java answers a download inside the call, on the calling
 * thread; a watchdog turns a download that never returns into a
 * failure rather than a hang. Status updates for another core, or
 * for a transaction that has ended, are dropped; and the status a
 * poller reads is always one whole update, with Java's thread, the
 * downloads and the poller all running at once. */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../../../os/android/play_feature_delivery.c"

static unsigned failures;
#define CHECK(cond, ...) \
   do { if (!(cond)) { printf("FAIL line %d: ", __LINE__); \
        printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

/* ---- a Java that answers on the caller's thread ------------------ */

static JNIEnv  test_env;
static struct  JNINativeInterface test_fns;
static unsigned java_calls;

static jstring stub_new_string(JNIEnv *env, const char *s)
{ (void)env; return (jstring)strdup(s); }
static void stub_delete_ref(JNIEnv *env, jobject o)
{ (void)env; free(o); }
static const char *stub_get_chars(JNIEnv *env, jstring s, jboolean *c)
{ (void)env; (void)c; return (const char*)s; }
static void stub_release_chars(JNIEnv *env, jstring s, const char *c)
{ (void)env; (void)s; (void)c; }
static jboolean stub_exception_check(JNIEnv *env)
{ (void)env; return 0; }
static void stub_exception_clear(JNIEnv *env)
{ (void)env; }
static jthrowable stub_exception_occurred(JNIEnv *env)
{ (void)env; return NULL; }
static void stub_exception_describe(JNIEnv *env)
{ (void)env; }

static void stub_call_void(JNIEnv *env, jobject obj, jmethodID m, ...)
{
   va_list ap;
   jstring name;
   (void)obj; (void)m;
   va_start(ap, m);
   name = va_arg(ap, jstring);
   va_end(ap);
   java_calls++;
   /* Play Core accepts the request and reports it straight back. */
   Java_com_retroarch_browser_retroactivity_RetroActivityCommon_coreInstallInitiated(
         env, NULL, name, 1);
}

JNIEnv *jni_thread_getenv(void) { return &test_env; }

/* Each status update wakes the task watching the download. */
static retro_atomic_int_t wakes_a;
static retro_atomic_int_t woken_a;
#define wakes ((unsigned)retro_atomic_load_acquire_int(&wakes_a))
#define woken ((uint32_t)retro_atomic_load_acquire_int(&woken_a))
void task_queue_wake(uint32_t ident)
{
   retro_atomic_store_release_int(&woken_a, (int)ident);
   retro_atomic_fetch_add_int(&wakes_a, 1);
}

static ANativeActivity    test_activity;
static struct android_app test_app;
struct android_app       *g_android = &test_app;

/* ---- the test ------------------------------------------------------ */

static retro_atomic_int_t returned;
static bool               download_ok;

#define STRESS_ROUNDS 2000

static retro_atomic_int_t stop;
static retro_atomic_int_t torn;

static void status_changed(int status, long done, long total)
{
   Java_com_retroarch_browser_retroactivity_RetroActivityCommon_coreInstallStatusChanged(
         &test_env, NULL, NULL, status, done, total);
}

/* Java's thread: progress for whatever transaction is active. */
static void java_thread(void *unused)
{
   long n = 0;
   (void)unused;
   while (!retro_atomic_load_acquire_int(&stop))
      status_changed(0, n++ % 101, 100);
}

/* Every read is one whole update: a finished transaction is never
 * active, a running one always is, and the progress is in range. */
static void poll_thread(void *unused)
{
   (void)unused;
   while (!retro_atomic_load_acquire_int(&stop))
   {
      enum play_feature_delivery_install_status st;
      unsigned pr = 0;
      bool active = play_feature_delivery_download_status(&st, &pr);
      bool done   = st == PLAY_FEATURE_DELIVERY_INSTALLED
         || st == PLAY_FEATURE_DELIVERY_FAILED;
      if (pr > 100 || active == done)
         retro_atomic_store_release_int(&torn, 1);
   }
}

static void do_download(void *unused)
{
   (void)unused;
   download_ok = play_feature_delivery_download(
         "mgba_libretro_android.so", 77);
   retro_atomic_store_release_int(&returned, 1);
}

int main(void)
{
   sthread_t *t;
   enum play_feature_delivery_install_status status;
   unsigned progress = 0;
   int i;

   test_fns.NewStringUTF          = stub_new_string;
   test_fns.DeleteLocalRef        = stub_delete_ref;
   test_fns.GetStringUTFChars     = stub_get_chars;
   test_fns.ReleaseStringUTFChars = stub_release_chars;
   test_fns.CallVoidMethod        = stub_call_void;
   test_fns.ExceptionCheck        = stub_exception_check;
   test_fns.ExceptionClear        = stub_exception_clear;
   test_fns.ExceptionOccurred     = stub_exception_occurred;
   test_fns.ExceptionDescribe     = stub_exception_describe;
   test_env                       = &test_fns;
   test_activity.clazz            = (jobject)&test_activity;
   test_app.activity              = &test_activity;
   test_app.downloadCore          = (jmethodID)&test_app;

   t = sthread_create(do_download, NULL);
   for (i = 0; i < 2000 && !retro_atomic_load_acquire_int(&returned); i++)
   {
      struct timespec d = { 0, 1000000L };
      nanosleep(&d, NULL);
   }
   if (!retro_atomic_load_acquire_int(&returned))
   {
      printf("FAIL: a download whose status came back on the calling "
            "thread deadlocked on the status lock\n");
      return 1;   /* the thread is stuck; do not join it */
   }
   sthread_join(t);

   CHECK(download_ok, "the download was not started");
   CHECK(java_calls == 1, "Java was asked %u times, want 1", java_calls);
   CHECK(play_feature_delivery_download_status(&status, &progress),
         "the download is not active after it started");
   CHECK(status == PLAY_FEATURE_DELIVERY_STARTING,
         "the status Java reported was lost (status %d)", (int)status);
   CHECK(wakes == 1 && woken == 77,
         "the status update woke %u tasks, the last %u, want task 77",
         wakes, (unsigned)woken);

   /* A progress update wakes it too. */
   Java_com_retroarch_browser_retroactivity_RetroActivityCommon_coreInstallStatusChanged(
         &test_env, NULL, NULL, 0, 50, 100);
   CHECK(wakes == 2 && woken == 77, "a progress update did not wake the task");
   CHECK(play_feature_delivery_download_status(&status, &progress)
         && progress == 50, "the progress was %u, want 50", progress);

   /* One download at a time: a second is refused without asking Java. */
   CHECK(!play_feature_delivery_download("mgba_libretro_android.so", 78),
         "a second download started alongside the first");
   CHECK(java_calls == 1, "Java was asked again for a refused download");

   /* A status for another core is not this transaction's. */
   {
      jstring other = (jstring)"snes9x";
      unsigned w    = wakes;
      Java_com_retroarch_browser_retroactivity_RetroActivityCommon_coreInstallInitiated(
            &test_env, NULL, other, 0);
      CHECK(play_feature_delivery_download_status(&status, &progress)
            && status == PLAY_FEATURE_DELIVERY_DOWNLOADING,
            "another core's status ended the transaction (status %d)",
            (int)status);
      CHECK(wakes == w, "another core's status woke the task");
   }

   /* Installed: the transaction ends, and a late update does not
    * bring it back. */
   status_changed(2, 100, 100);
   CHECK(!play_feature_delivery_download_status(&status, &progress)
         && status == PLAY_FEATURE_DELIVERY_INSTALLED && progress == 100,
         "the install did not end the transaction (status %d, %u%%)",
         (int)status, progress);
   status_changed(0, 10, 100);
   CHECK(!play_feature_delivery_download_status(&status, &progress)
         && status == PLAY_FEATURE_DELIVERY_INSTALLED && progress == 100,
         "a late update revived a finished transaction (status %d, %u%%)",
         (int)status, progress);

   /* The next download starts fresh. */
   CHECK(play_feature_delivery_download("mgba_libretro_android.so", 79),
         "a download after the last one ended was refused");
   CHECK(play_feature_delivery_download_status(&status, &progress)
         && status == PLAY_FEATURE_DELIVERY_STARTING && progress == 0,
         "the new download did not start fresh (status %d, %u%%)",
         (int)status, progress);
   status_changed(3, 0, 0);

   /* Downloads, Java's thread and a poller at once. */
   {
      sthread_t *java = sthread_create(java_thread, NULL);
      sthread_t *poll = sthread_create(poll_thread, NULL);
      unsigned refused = 0;
      for (i = 0; i < STRESS_ROUNDS; i++)
      {
         if (!play_feature_delivery_download("mgba_libretro_android.so",
                  (uint32_t)i))
            refused++;
         status_changed(i & 1 ? 2 : 3, 100, 100);
         if (play_feature_delivery_download_status(&status, &progress))
            refused++;
      }
      retro_atomic_store_release_int(&stop, 1);
      sthread_join(java);
      sthread_join(poll);
      CHECK(!refused, "%u downloads were refused or did not end",
            refused);
      CHECK(!retro_atomic_load_acquire_int(&torn),
            "a poll read a status from two different updates");
   }

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] play_feature_delivery_test\n");
   return 0;
}
