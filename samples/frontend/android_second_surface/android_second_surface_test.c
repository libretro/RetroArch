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
 *    another try;
 *  - the same surface at a new size, as a turn gives it, is one
 *    GEOMETRY at that size: the same generation, no CLOSE, still
 *    shown; a window kept across a driver reinit meanwhile is taken
 *    at the size last drawn, and the next driver's first poll brings
 *    the new size;
 *  - the display's peak luminance goes with its surface to the window
 *    that takes it;
 *  - a kept surface the next renderer would draw another kind of
 *    buffer into is not drawn: Java is asked once for a new surface,
 *    which is, across a driver reinit too; the same kind, or a kind
 *    unknown on either side, is drawn as before;
 *  - GL's context says a kind for the windows it polls, never one of
 *    Vulkan's, so a kept surface is not drawn by the other renderer.
 *
 * The functions are taken out of platform_unix.c, android_windows.c,
 * android_ctx.c and vulkan.c at build time by extract.awk; the test
 * supplies the types they touch, so it builds and runs on the host. */
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
typedef float jfloat;
#define JNIEXPORT
#define JNICALL
#define RARCH_ERR(...) ((void)0)

typedef struct ANativeWindow { int refs; } ANativeWindow;

struct android_second_surface
{
   ANativeWindow *window;
   unsigned dims;
   float peak_nits;
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

/* What the windows asked Java to replace */
static unsigned renews;
static int last_renew;
static bool renew_asked = true;

static bool android_second_surface_renew(int generation)
{
   renews++;
   last_renew = generation;
   return renew_asked;
}

/* What android_ctx.c's poll touches */
typedef int EGLint;
typedef void *EGLSurface;
typedef struct { unsigned freed; } egl_ctx_data_t;

typedef struct
{
   egl_ctx_data_t egl;
   EGLint format;
} android_ctx_data_t;

static void egl_window_surface_free(egl_ctx_data_t *egl, EGLSurface surf)
{
   (void)surf;
   egl->freed++;
}

/* What vulkan.c's vulkan_window_buffers() touches */
#define VULKAN_HDR_SWAPCHAIN
#define VK_CTX_FLAG_HDR_ENABLE (1 << 1)
#define VK_COLOR_SPACE_SRGB_NONLINEAR_KHR       0
#define VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT 1000104002
#define VK_COLOR_SPACE_HDR10_ST2084_EXT         1000104008
#define VK_FORMAT_R8G8B8A8_UNORM                37
#define VK_FORMAT_A2B10G10R10_UNORM_PACK32      64
#define VK_FORMAT_R16G16B16A16_SFLOAT           97

typedef struct
{
   unsigned flags;
   int swapchain_format;
   int swapchain_colour_space;
   struct { unsigned bit_depth; } swapchain_settings;
} vulkan_context_t;

typedef struct { vulkan_context_t *context; } vk_t;

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

#define SURFACE_CHANGED_PEAK(win, w, h, peak) \
   Java_com_retroarch_browser_retroactivity_SecondDisplayController_nativeSurfaceChanged( \
         NULL, NULL, (jobject)(win), (w), (h), 1, (peak))
#define SURFACE_CHANGED(win, w, h) SURFACE_CHANGED_PEAK(win, w, h, 0.0f)
#define SURFACE_DESTROYED() \
   Java_com_retroarch_browser_retroactivity_SecondDisplayController_nativeSurfaceDestroyed( \
         NULL, NULL)

/* Window 1 on display 2, made by @who, with @a posted for it */
static android_window_t *window_made(ANativeWindow *a, void *who)
{
   android_window_t *made = (android_window_t*)calloc(1, sizeof(*made));
   memset(&app, 0, sizeof(app));
   memset(a, 0, sizeof(*a));
   memset(&native, 0, sizeof(native));
   made->common.owner              = who;
   made->common.native             = &native;
   made->common.request.screen     = 1;
   made->common.request.generation = 7;
   made->common.request.display    = 2;
   native.state                    = made;
   native.generation               = 7;
   android_windows[1]              = made;
   SURFACE_CHANGED(a, 1240, 1080);
   requests = 0;
   renews   = 0;
   return made;
}

/* Made and shown on @a by 'owner', which draws @buffers there, then
 * kept as a driver teardown keeps it */
static android_window_t *window_kept_as(ANativeWindow *a, unsigned buffers)
{
   android_window_t *kept = window_made(a, &owner);
   native.buffers         = buffers;
   poll_once();
   android_windows_keep(&owner, &native);
   return kept;
}

static android_window_t *window_kept(ANativeWindow *a)
{
   return window_kept_as(a, 0);
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

/* @who's poll: how many GEOMETRY and CLOSE events, and the size the
 * last GEOMETRY gave */
static unsigned poll_events(void *who, unsigned *closes, unsigned *dims)
{
   video_window_event_t events[4];
   unsigned n = android_windows_poll(who, events, 4);
   unsigned geometry = 0;
   unsigned i;
   *closes = 0;
   for (i = 0; i < n; i++)
   {
      if (events[i].kind == VIDEO_WINDOW_EVENT_GEOMETRY)
      {
         geometry++;
         *dims = events[i].dims;
      }
      else if (events[i].kind == VIDEO_WINDOW_EVENT_CLOSE)
         (*closes)++;
   }
   return geometry;
}

/* A turn: Java's surfaceChanged with the same surface at the swapped
 * size */
static void test_resize(void)
{
   ANativeWindow a;
   unsigned closes, dims = 0;
   jint gen;
   window_reset();
   memset(&a, 0, sizeof(a));
   gen = SURFACE_CHANGED(&a, 1240, 1080);
   poll_once();
   changed_cmds = 0;
   CHECK(SURFACE_CHANGED(&a, 1080, 1240) == gen,
         "a new size keeps the generation");
   CHECK(!waited && changed_cmds == 1,
         "the renderer is told once, not waited for");
   CHECK(poll_events(&owner, &closes, &dims) == 1 && !closes
         && dims == VIDEO_SCALE_PACK(1080, 1240),
         "one GEOMETRY at the new size, no CLOSE");
   CHECK(native.surface == &a && native.presentable && native.configured
         && native.dims == VIDEO_SCALE_PACK(1080, 1240),
         "still shown, at the new size");
   CHECK(retro_atomic_load_acquire_int(&app.second_display.bound_surface)
         == gen, "input maps into it as before");
   CHECK(a.refs == 1, "the old size's reference released");
   CHECK(!poll_events(&owner, &closes, &dims) && !closes,
         "the next poll has nothing more");
   SURFACE_DESTROYED();
   poll_once();
   CHECK(a.refs == 0, "and every reference goes with it");
}

/* A turn landing between one driver keeping the window and the next
 * taking it */
static void test_resize_kept(void)
{
   static int next_owner;
   ANativeWindow a;
   video_window_native_t taken;
   unsigned closes, dims = 0;
   unsigned display = 0;
   jint gen;
   window_kept(&a);
   gen = retro_atomic_load_acquire_int(
         &app.second_display.surface_generation);
   CHECK(SURFACE_CHANGED(&a, 1080, 1240) == gen,
         "a turn while kept keeps the generation");
   poll_once();
   CHECK(retro_atomic_load_acquire_ptr(&app.second_display.surface) != NULL,
         "the new size waits for the next owner");
   memset(&taken, 0, sizeof(taken));
   CHECK(android_windows_take(&next_owner, 1, &taken, &display)
         && display == 2, "the next owner takes it");
   CHECK(taken.surface == &a && taken.presentable
         && taken.dims == VIDEO_SCALE_PACK(1240, 1080),
         "as it was last drawn");
   CHECK(poll_events(&next_owner, &closes, &dims) == 1 && !closes
         && dims == VIDEO_SCALE_PACK(1080, 1240),
         "its first poll brings the new size");
   CHECK(taken.surface == &a && taken.presentable && taken.configured
         && taken.dims == VIDEO_SCALE_PACK(1080, 1240),
         "shown at the new size");
   CHECK(retro_atomic_load_acquire_int(&app.second_display.bound_surface)
         == gen, "input maps into it as before");
   CHECK(a.refs == 1, "one reference, the new size's");
   android_windows_destroy(&next_owner, &taken);
   CHECK(a.refs == 0 && !android_windows[1], "then it goes");
}

/* The display's peak luminance, with each surface Java hands over */
static void test_peak_reaches_window(void)
{
   static int next_owner;
   ANativeWindow a;
   video_window_native_t taken;
   unsigned display = 0;
   window_kept(&a);
   SURFACE_CHANGED_PEAK(&a, 1080, 1240, 600.0f);
   memset(&taken, 0, sizeof(taken));
   android_windows_take(&next_owner, 1, &taken, &display);
   poll_for(&next_owner);
   CHECK(taken.surface == &a && taken.peak_nits == 600.0f,
         "the poll that takes a surface takes its display's peak");
   android_windows_keep(&next_owner, &taken);
   memset(&native, 0, sizeof(native));
   CHECK(android_windows_take(&owner, 1, &native, &display)
         && native.peak_nits == 600.0f,
         "and a window taken across a reinit keeps it");
   SURFACE_CHANGED_PEAK(&a, 1240, 1080, 0.0f);
   poll_once();
   CHECK(native.dims == VIDEO_SCALE_PACK(1240, 1080)
         && native.peak_nits == 0.0f,
         "a later surface with none leaves none");
   SURFACE_CHANGED_PEAK(&a, 1080, 1240, 600.0f);
   poll_once();
   SURFACE_DESTROYED();
   poll_once();
   CHECK(!native.surface && native.peak_nits == 0.0f,
         "nor does a surface that went");
   android_windows_destroy(&owner, &native);
   CHECK(a.refs == 0 && !android_windows[1], "then it goes");
}

/* The next owner takes the window kept on @a, where 'owner' drew
 * @was, and polls once to draw @now */
static void take_kept(ANativeWindow *a, unsigned was, unsigned now,
      video_window_native_t *taken, void *next_owner)
{
   unsigned display = 0;
   window_kept_as(a, was);
   memset(taken, 0, sizeof(*taken));
   android_windows_take(next_owner, 1, taken, &display);
   taken->buffers = now;
   poll_for(next_owner);
}

/* A surface kept from a renderer that drew another kind of buffer */
static void test_renew(void)
{
   static int next_owner;
   ANativeWindow a, b;
   video_window_native_t taken;
   unsigned display = 0;
   int gen_a, gen_b;

   /* Another kind: a new surface asked for, once; the old one not
    * drawn, at any size */
   take_kept(&a, 1, 2, &taken, &next_owner);
   gen_a = retro_atomic_load_acquire_int(
         &app.second_display.surface_generation);
   CHECK(renews == 1 && last_renew == gen_a && requests == 0,
         "another kind asks Java for a new surface");
   CHECK(!taken.surface && !taken.presentable && !taken.configured
         && a.refs == 0, "and the kept one is let go of, not drawn");
   CHECK(!retro_atomic_load_acquire_int(&app.second_display.bound_surface),
         "input stops mapping into it");
   poll_for(&next_owner);
   CHECK(renews == 1, "asked once");
   SURFACE_CHANGED(&a, 1080, 1240);
   poll_for(&next_owner);
   CHECK(!taken.surface && a.refs == 0 && renews == 1,
         "nor is it drawn at another size");
   SURFACE_DESTROYED();
   memset(&b, 0, sizeof(b));
   gen_b = SURFACE_CHANGED(&b, 1240, 1080);
   poll_for(&next_owner);
   CHECK(gen_b != gen_a && taken.surface == &b && taken.presentable
         && taken.configured
         && retro_atomic_load_acquire_int(
            &app.second_display.bound_surface) == gen_b,
         "the new surface is drawn");
   poll_for(&next_owner);
   CHECK(renews == 1 && taken.surface == &b,
         "and kept: it carried nothing before");
   /* The renderer's kind changes while it draws */
   taken.buffers = 1;
   poll_for(&next_owner);
   CHECK(renews == 2 && last_renew == gen_b && !taken.surface
         && b.refs == 0, "a kind changed on the way is a new surface too");
   android_windows_destroy(&next_owner, &taken);
   CHECK(!android_windows[1], "then it goes");

   /* A driver reinit before the new surface comes */
   take_kept(&a, 1, 2, &taken, &next_owner);
   gen_a = retro_atomic_load_acquire_int(
         &app.second_display.surface_generation);
   android_windows_keep(&next_owner, &taken);
   memset(&native, 0, sizeof(native));
   CHECK(android_windows_take(&owner, 1, &native, &display)
         && !native.surface && !native.presentable,
         "a reinit meanwhile takes the window with no surface");
   native.buffers = 2;
   poll_once();
   SURFACE_CHANGED(&a, 1080, 1240);
   poll_once();
   CHECK(!native.surface && a.refs == 0 && renews == 1,
         "the old one is still not drawn, nor asked for again");
   SURFACE_DESTROYED();
   memset(&b, 0, sizeof(b));
   gen_b = SURFACE_CHANGED(&b, 1240, 1080);
   poll_once();
   CHECK(gen_b != gen_a && native.surface == &b && native.presentable
         && retro_atomic_load_acquire_int(
            &app.second_display.bound_surface) == gen_b,
         "the new surface is drawn");
   poll_once();
   CHECK(renews == 1 && native.surface == &b, "and kept");
   android_windows_destroy(&owner, &native);
   CHECK(b.refs == 0 && !android_windows[1], "then it goes");

   /* The same kind: drawn as before */
   take_kept(&a, 1, 1, &taken, &next_owner);
   CHECK(renews == 0 && taken.surface == &a && taken.presentable,
         "the same kind draws the kept surface, asking nothing");
   android_windows_destroy(&next_owner, &taken);
   CHECK(a.refs == 0 && !android_windows[1], "then it goes");

   /* Unknown on either side, or Java not there to ask: as before */
   take_kept(&a, 0, 2, &taken, &next_owner);
   CHECK(renews == 0 && taken.surface == &a && taken.presentable,
         "after a renderer that did not say, the kept surface is drawn");
   android_windows_destroy(&next_owner, &taken);
   take_kept(&a, 1, 0, &taken, &next_owner);
   CHECK(renews == 0 && taken.surface == &a && taken.presentable,
         "a renderer that does not say draws the kept surface");
   android_windows_destroy(&next_owner, &taken);
   renew_asked = false;
   take_kept(&a, 1, 2, &taken, &next_owner);
   renew_asked = true;
   CHECK(renews == 1 && taken.surface == &a && taken.presentable,
         "with Java not asked, the kept surface is drawn");
   android_windows_destroy(&next_owner, &taken);
   CHECK(a.refs == 0 && !android_windows[1], "then it goes");
}

static void gl_poll(android_ctx_data_t *gl)
{
   video_window_event_t events[4];
   android_gfx_ctx_window_poll(gl, events, 4);
}

/* What Vulkan says of a swapchain in @format and @space */
static unsigned vk_buffers(int format, int space, unsigned bit_depth)
{
   vk_t vk;
   vulkan_context_t ctx;
   memset(&ctx, 0, sizeof(ctx));
   if (space != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
      ctx.flags                  = VK_CTX_FLAG_HDR_ENABLE;
   ctx.swapchain_format          = format;
   ctx.swapchain_colour_space    = space;
   ctx.swapchain_settings.bit_depth = bit_depth;
   vk.context                    = &ctx;
   return vulkan_window_buffers(&vk);
}

/* Shown by GL context @gl, kept, then taken by a Vulkan renderer
 * that draws @buffers */
static void gl_to_vulkan(ANativeWindow *a, android_ctx_data_t *gl,
      unsigned buffers, video_window_native_t *taken, void *next_owner)
{
   unsigned display = 0;
   window_made(a, gl);
   gl_poll(gl);
   android_windows_keep(gl, &native);
   memset(taken, 0, sizeof(*taken));
   android_windows_take(next_owner, 1, taken, &display);
   taken->buffers = buffers;
   poll_for(next_owner);
}

/* A kept surface across a switch of renderer */
static void test_renderer_switch(void)
{
   static const int gl_formats[] = { 0, 1, 2, 3, 4, 22, 43, -1, 0x7fffffff };
   static const int spaces[]     = {
      VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
      VK_COLOR_SPACE_HDR10_ST2084_EXT,
      VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT
   };
   static int next_owner;
   static android_ctx_data_t gl, next_gl;
   ANativeWindow a, b;
   video_window_native_t taken;
   unsigned i, s, depth;
   unsigned display = 0;
   unsigned vulkan  = 0;
   unsigned said    = 0;
   unsigned scrgb   = vk_buffers(VK_FORMAT_R16G16B16A16_SFLOAT,
         VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT, 0);
   unsigned hdr10   = vk_buffers(VK_FORMAT_A2B10G10R10_UNORM_PACK32,
         VK_COLOR_SPACE_HDR10_ST2084_EXT, 2);
   unsigned sdr     = vk_buffers(VK_FORMAT_R8G8B8A8_UNORM,
         VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, 0);
   int gen_b;

   /* GL's kinds are multiples of 8 and Vulkan's never, in any format:
    * neither is unknown, and none of one is one of the other */
   for (i = 0; i < 256; i++)
      for (s = 0; s < 3; s++)
         for (depth = 0; depth < 3; depth++)
            if (vk_buffers((int)i, spaces[s], depth) % 8)
               vulkan++;
   CHECK(vulkan == 256 * 3 * 3, "no kind of Vulkan's is a multiple of 8");
   for (i = 0; i < sizeof(gl_formats) / sizeof(gl_formats[0]); i++)
   {
      gl.format = gl_formats[i];
      window_made(&a, &gl);
      gl_poll(&gl);
      if (native.buffers && !(native.buffers % 8))
         said++;
      android_windows_destroy(&gl, &native);
   }
   CHECK(said == i, "GL's context says a kind, a multiple of 8");

   /* GL, then Vulkan */
   gl.format = 1;
   gl_to_vulkan(&a, &gl, scrgb, &taken, &next_owner);
   CHECK(renews == 1 && !taken.surface && !taken.presentable
         && a.refs == 0, "Vulkan scRGB does not draw the surface GL drew");
   android_windows_destroy(&next_owner, &taken);
   gl_to_vulkan(&a, &gl, hdr10, &taken, &next_owner);
   CHECK(renews == 1 && !taken.surface && a.refs == 0,
         "nor Vulkan HDR10");
   android_windows_destroy(&next_owner, &taken);
   gl_to_vulkan(&a, &gl, sdr, &taken, &next_owner);
   CHECK(renews == 1 && !taken.surface && a.refs == 0,
         "nor Vulkan SDR: a renderer's kinds are its own");
   android_windows_destroy(&next_owner, &taken);
   CHECK(!android_windows[1], "then it goes");

   /* Vulkan, then GL */
   window_kept_as(&a, scrgb);
   memset(&taken, 0, sizeof(taken));
   android_windows_take(&gl, 1, &taken, &display);
   gl_poll(&gl);
   CHECK(renews == 1 && !taken.surface && !taken.presentable
         && a.refs == 0, "GL does not draw the surface Vulkan scRGB drew");
   gl_poll(&gl);
   CHECK(renews == 1, "asked once");
   SURFACE_DESTROYED();
   memset(&b, 0, sizeof(b));
   gen_b = SURFACE_CHANGED(&b, 1240, 1080);
   gl_poll(&gl);
   CHECK(taken.surface == &b && taken.presentable
         && retro_atomic_load_acquire_int(
            &app.second_display.bound_surface) == gen_b,
         "the new surface is drawn");
   gl_poll(&gl);
   CHECK(renews == 1 && taken.surface == &b, "and kept");

   /* GL, then GL */
   android_windows_keep(&gl, &taken);
   next_gl.format = gl.format;
   memset(&taken, 0, sizeof(taken));
   android_windows_take(&next_gl, 1, &taken, &display);
   gl_poll(&next_gl);
   CHECK(renews == 1 && taken.surface == &b && taken.presentable,
         "GL in the same format draws the surface GL drew");
   android_windows_keep(&next_gl, &taken);
   gl.format = 4;
   memset(&taken, 0, sizeof(taken));
   android_windows_take(&gl, 1, &taken, &display);
   gl_poll(&gl);
   CHECK(renews == 2 && !taken.surface && b.refs == 0,
         "GL in another format does not");
   android_windows_destroy(&gl, &taken);
   CHECK(!android_windows[1], "then it goes");
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
   test_resize();
   test_resize_kept();
   test_peak_reaches_window();
   test_renew();
   test_renderer_switch();

   if (failures)
   {
      printf("FAILED: %u\n", failures);
      return 1;
   }
   printf("   all passed\n");
   return 0;
}
