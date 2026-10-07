/* play_feature_delivery.c on the host, against the Android stubs in
 * tools/platform_stubs/android: a core download asks Java with the
 * status lock released.
 *
 * The status callbacks Java delivers (coreInstallInitiated,
 * coreInstallStatusChanged) take the same lock as the download that
 * set them off. Here the stub Java answers inside the call, on the
 * calling thread - the case where a lock still held across the call
 * deadlocks the thread against itself. A watchdog turns that into a
 * failure rather than a hang. */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../../../play_feature_delivery/play_feature_delivery.c"

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

static ANativeActivity    test_activity;
static struct android_app test_app;
struct android_app       *g_android = &test_app;

/* ---- the test ------------------------------------------------------ */

static retro_atomic_int_t returned;
static bool               download_ok;

static void do_download(void *unused)
{
   (void)unused;
   download_ok = play_feature_delivery_download(
         "mgba_libretro_android.so");
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

   play_feature_delivery_init();

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

   /* One download at a time: a second is refused without asking Java. */
   CHECK(!play_feature_delivery_download("mgba_libretro_android.so"),
         "a second download started alongside the first");
   CHECK(java_calls == 1, "Java was asked again for a refused download");

   play_feature_delivery_deinit();

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] play_feature_delivery_test\n");
   return 0;
}
