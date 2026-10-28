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

/* What the windows asked Java to show */
static unsigned requests;
static unsigned last_ordinal;
static bool last_given_up;

static void android_second_display_request(unsigned ordinal, bool given_up)
{
   requests++;
   last_ordinal  = ordinal;
   last_given_up = given_up;
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

static void poll_for(void *who)
{
   video_window_event_t events[4];
   android_windows_poll(who, events, 4);
}

static void poll_once(void)
{
   poll_for(&owner);
}

#define SURFACE_CHANGED(win, w, h) \
   Java_com_retroarch_browser_retroactivity_SecondDisplayController_nativeSurfaceChanged( \
         NULL, NULL, (jobject)(win), (w), (h), 1)
#define SURFACE_DESTROYED() \
   Java_com_retroarch_browser_retroactivity_SecondDisplayController_nativeSurfaceDestroyed( \
         NULL, NULL)

/* Window 1 on display 2, made and shown on @a by 'owner', then kept
 * as a driver teardown keeps it */
static android_window_t *window_kept(ANativeWindow *a)
{
   android_window_t *kept = (android_window_t*)calloc(1, sizeof(*kept));
   memset(&app, 0, sizeof(app));
   memset(a, 0, sizeof(*a));
   memset(&native, 0, sizeof(native));
   kept->common.owner              = &owner;
   kept->common.native             = &native;
   kept->common.request.screen     = 1;
   kept->common.request.generation = 7;
   kept->common.request.display    = 2;
   native.state                    = kept;
   native.generation               = 7;
   android_windows[1]              = kept;
   SURFACE_CHANGED(a, 1240, 1080);
   poll_once();
   requests = 0;
   android_windows_keep(&owner, &native);
   return kept;
}

/* A driver reinit: the old owner keeps the window up, the next one
 * takes it, or it goes once nobody did */
static void test_handover(void)
{
   static int next_owner;
   ANativeWindow a;
   android_window_t *kept;
   video_window_native_t taken;
   unsigned display;
   int bound_surface;

   /* Kept, then taken and let go of by the next owner */
   kept          = window_kept(&a);
   bound_surface = retro_atomic_load_acquire_int(
         &app.second_display.bound_surface);
   CHECK(bound_surface > 0, "the first owner showed the surface");
   CHECK(requests == 0, "keep asks Java nothing");
   CHECK(a.refs == 1, "keep holds the surface");
   CHECK(!native.state && !native.surface,
         "the old owner's record is cleared");
   CHECK(android_windows[1] == kept && !kept->common.owner,
         "the window stays, owned by none");
   android_windows_free(&owner, android_windows_destroy);
   CHECK(requests == 0 && android_windows[1] == kept,
         "the old context's sweep passes it by");
   memset(&taken, 0, sizeof(taken));
   display = 0;
   CHECK(android_windows_take(&next_owner, 1, &taken, &display)
         && display == 2, "the next owner takes it, on its display");
   CHECK(     android_windows[1] == kept && taken.state == kept
         && taken.surface == &a && kept->node
         && taken.dims == kept->node->dims && taken.dims
         && taken.configured && taken.presentable
         && taken.generation == 7
         && taken.kind == VIDEO_WINDOW_NATIVE_ANDROID,
         "with its surface, size and generation");
   CHECK(     retro_atomic_load_acquire_int(
               &app.second_display.bound_surface) == bound_surface
         && retro_atomic_load_acquire_int(
               &app.second_display.bound_window) == 1
         && retro_atomic_load_acquire_int(
               &app.second_display.bound_generation) == 7,
         "input maps into it as before");
   CHECK(!android_windows_take(&owner, 1, &native, &display),
         "a window with an owner is not taken");
   android_windows_release_kept();
   CHECK(requests == 0 && android_windows[1] == kept && a.refs == 1,
         "a taken window is not let go of");
   android_windows_destroy(&next_owner, &taken);
   CHECK(requests == 1 && last_ordinal == 0 && last_given_up,
         "the next owner's destroy hides it, once");
   CHECK(a.refs == 0 && !android_windows[1] && !taken.state,
         "and releases it");

   /* Taken, then Java takes the surface back */
   kept = window_kept(&a);
   memset(&taken, 0, sizeof(taken));
   CHECK(android_windows_take(&next_owner, 1, &taken, &display),
         "taken again");
   SURFACE_DESTROYED();
   poll_for(&next_owner);
   CHECK(!taken.surface && !taken.presentable && !taken.configured
         && a.refs == 0, "the next owner's poll lets the surface go");
   CHECK(!retro_atomic_load_acquire_int(&app.second_display.bound_surface),
         "input stops mapping into it");
   CHECK(android_windows[1] == kept && requests == 0,
         "the window stays until it is let go of");
   android_windows_destroy(&next_owner, &taken);
   CHECK(requests == 1 && !android_windows[1], "then it goes");

   /* Nobody took it */
   kept = window_kept(&a);
   android_windows_release_kept();
   CHECK(requests == 1 && last_ordinal == 0 && last_given_up,
         "one nobody took is hidden, once");
   CHECK(a.refs == 0 && !android_windows[1], "and released");
   CHECK(!retro_atomic_load_acquire_int(&app.second_display.bound_surface)
         && !retro_atomic_load_acquire_int(
            &app.second_display.bound_window),
         "input stops mapping into it");
   android_windows_release_kept();
   CHECK(requests == 1, "a second release asks nothing");
}

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

   android_windows[1] = NULL;
   test_handover();

   if (failures)
   {
      printf("FAILED: %u\n", failures);
      return 1;
   }
   printf("   all passed\n");
   return 0;
}
