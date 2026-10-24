/* The second display's surface, from the Android UI thread to the
 * renderer.
 *
 * nativeSurfaceChanged() and nativeSurfaceDestroyed() run on the UI
 * thread; android_windows_poll() runs on the thread that renders. Held
 * here, on the shipping code of both ends:
 *
 *  - surfaceDestroyed never waits for the renderer: Java's callback
 *    returns without the renderer having done anything;
 *  - the renderer's next poll lets the surface go: no surface, not
 *    presentable, its window reference released, input unmapped;
 *  - a surface Java took back before the renderer took it is freed on
 *    the spot, and every reference balances;
 *  - a new surface after one went is taken and shown, and the next
 *    poll does not drop it for the old one's sake;
 *  - each going surface asks the main thread to give the windows
 *    another try.
 *
 * The functions are taken out of platform_unix.c and android_windows.c
 * at build time by extract.awk; the test supplies the types they touch,
 * so it builds and runs on the host. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include "gfx/video_defines.h"
#include "gfx/common/android_windows.h"

/* What the functions touch, and nothing more */
typedef void *JNIEnv;
typedef void *jclass;
typedef void *jobject;
typedef int jint;
#define JNIEXPORT
#define JNICALL
#define RARCH_ERR(...) ((void)0)

typedef struct ANativeWindow { int refs; } ANativeWindow;

struct android_second_surface
{
   ANativeWindow *window;
   unsigned dims;
   int generation;
   int request;
};

struct android_second_display
{
   retro_atomic_ptr_t surface;
   retro_atomic_int_t surface_generation;
   retro_atomic_int_t surface_gone;
   retro_atomic_int_t bound_surface;
   retro_atomic_int_t bound_window;
   retro_atomic_int_t bound_generation;
   retro_atomic_int_t released;
};

#define ANDROID_SECOND_BEFORE(a, b) \
   ((int)((unsigned)(a) - (unsigned)(b)) < 0)

enum { APP_CMD_SECOND_WINDOW_CHANGED = 1 };

struct android_app
{
   struct android_second_display second_display;
   ANativeWindow *posted_second_window;
   unsigned posted_second_dims;
};

static struct android_app app;
static struct android_app *g_android_early = &app;
static struct android_app *g_android       = &app;
static android_window_t *android_windows[RETRO_VIDEO_VIEWS_MAX];

/* What the UI thread did besides */
static unsigned changed_cmds;
static bool app_thread_gone;

/* Fails the test if the UI thread ever waits for the renderer */
static bool waited;
#define android_lifecycle_wait(...) (waited = true, true)

static bool android_app_thread_gone(struct android_app *android_app)
{
   (void)android_app;
   return app_thread_gone;
}

static bool android_app_write_cmd(struct android_app *android_app, int cmd)
{
   (void)android_app;
   if (cmd == APP_CMD_SECOND_WINDOW_CHANGED)
      changed_cmds++;
   return true;
}

static ANativeWindow *ANativeWindow_fromSurface(JNIEnv *env, jobject surface)
{
   ANativeWindow *window = (ANativeWindow*)surface;
   (void)env;
   window->refs++;
   return window;
}

static void ANativeWindow_release(ANativeWindow *window)
{
   window->refs--;
}

#include "second_surface_driver.h"

static unsigned failures;

#define CHECK(cond, what) do { if (!(cond)) \
   { printf("   FAIL %s\n", what); failures++; } } while (0)

static video_window_native_t native;
static android_window_t state;
static int owner;

static void window_reset(void)
{
   memset(&native, 0, sizeof(native));
   memset(&state, 0, sizeof(state));
   memset(&app, 0, sizeof(app));
   state.common.owner    = &owner;
   state.common.native   = &native;
   state.common.request.screen     = 1;
   state.common.request.generation = 7;
   native.state          = &state;
   android_windows[1]    = &state;
   changed_cmds          = 0;
   waited                = false;
   app_thread_gone       = false;
}

static void poll_once(void)
{
   video_window_event_t events[4];
   android_windows_poll(&owner, events, 4);
}

#define SURFACE_CHANGED(win, w, h) \
   Java_com_retroarch_browser_retroactivity_SecondDisplayController_nativeSurfaceChanged( \
         NULL, NULL, (jobject)(win), (w), (h), 1)
#define SURFACE_DESTROYED() \
   Java_com_retroarch_browser_retroactivity_SecondDisplayController_nativeSurfaceDestroyed( \
         NULL, NULL)

int main(void)
{
   ANativeWindow a, b;
   jint gen_a, gen_b;
   printf("android second surface:\n");

   /* Shown, then taken back: no wait, and the renderer lets it go */
   window_reset();
   memset(&a, 0, sizeof(a));
   gen_a = SURFACE_CHANGED(&a, 1920, 1080);
   CHECK(gen_a > 0, "a surface gets a generation");
   poll_once();
   CHECK(native.surface == &a && native.presentable,
         "the renderer takes the surface");
   CHECK(retro_atomic_load_acquire_int(&app.second_display.bound_surface)
         == gen_a, "input maps into it");
   changed_cmds = 0;
   SURFACE_DESTROYED();
   CHECK(!waited, "surfaceDestroyed does not wait for the renderer");
   CHECK(changed_cmds == 1, "the windows get another try");
   CHECK(a.refs == 1, "the renderer still holds its reference");
   poll_once();
   CHECK(!native.surface && !native.presentable && !native.configured,
         "the next poll lets the surface go");
   CHECK(a.refs == 0, "every reference released");
   CHECK(!retro_atomic_load_acquire_int(&app.second_display.bound_surface),
         "input stops mapping into it");
   poll_once();
   CHECK(a.refs == 0, "a second poll releases nothing more");

   /* Taken back before the renderer took it */
   window_reset();
   memset(&a, 0, sizeof(a));
   SURFACE_CHANGED(&a, 640, 480);
   CHECK(a.refs == 1, "the posted surface holds one reference");
   SURFACE_DESTROYED();
   CHECK(!waited, "an untaken surface is not waited for either");
   CHECK(a.refs == 0, "an untaken surface is freed on the spot");
   poll_once();
   CHECK(!native.surface && !native.presentable,
         "the renderer never sees it");

   /* Replaced: the old one goes, the new one stays */
   window_reset();
   memset(&a, 0, sizeof(a));
   memset(&b, 0, sizeof(b));
   gen_a = SURFACE_CHANGED(&a, 1920, 1080);
   poll_once();
   gen_b = SURFACE_CHANGED(&b, 1280, 720);
   CHECK(!waited, "a window change does not wait");
   CHECK(gen_b != gen_a, "a new window, a new generation");
   poll_once();
   CHECK(native.surface == &b && native.presentable,
         "the new surface is shown");
   CHECK(a.refs == 0 && b.refs == 1, "the old one released");
   poll_once();
   CHECK(native.surface == &b && native.presentable,
         "the old one going does not drop the new");

   /* Gone, then back */
   SURFACE_DESTROYED();
   poll_once();
   CHECK(!native.surface && b.refs == 0, "gone again");
   gen_a = SURFACE_CHANGED(&a, 800, 600);
   poll_once();
   CHECK(native.surface == &a && native.presentable
         && retro_atomic_load_acquire_int(
            &app.second_display.bound_surface) == gen_a,
         "a surface after one went is shown");
   poll_once();
   CHECK(native.surface == &a, "and kept");

   /* With no app thread left, nothing is asked of it */
   app_thread_gone = true;
   changed_cmds    = 0;
   SURFACE_DESTROYED();
   CHECK(!waited && changed_cmds == 0, "nothing posted to a gone thread");
   poll_once();
   CHECK(a.refs == 0, "released all the same");

   if (failures)
   {
      printf("FAILED: %u\n", failures);
      return 1;
   }
   printf("   all passed\n");
   return 0;
}
