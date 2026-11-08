/* Android's content rectangle, from the UI thread to the video thread.
 *
 * onContentRectChanged() runs on the Android UI thread and publishes
 * the new size; the Vulkan context's check_window() reads it on the
 * video thread. Held here, on the shipping code:
 *
 *  - a size read back is one the UI thread published: a change landing
 *    between the reader's atomic operations never pairs one change's
 *    width with another's height;
 *  - a change raised while check_window() is reading is not cleared
 *    unseen: the next check still asks for a new swapchain, even when
 *    the size did not move;
 *  - a size round-trips, and an unchanged one asks for no resize;
 *  - a quarter turn asks for one resize, at the swapped size, on the
 *    Vulkan context and on the gl one, which reads its size from the
 *    EGL surface instead.
 *
 * Deterministic, not a race: the reader's atomics go through a hook,
 * which delivers the UI thread's change after the reader's first,
 * second or third atomic operation - every gap the reader has. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include "gfx/video_defines.h"

static void (*op_hook)(void);
static int    op_skip;   /* operations to let past before the hook */

static void run_hook(void)
{
   if (op_hook && op_skip-- == 0)
   {
      void (*hook)(void) = op_hook;
      op_hook = NULL;
      hook();
   }
}

static int hooked_load_acquire_int(retro_atomic_int_t *p)
{
   int v = retro_atomic_load_acquire_int(p);
   run_hook();
   return v;
}

static int hooked_exchange_int(retro_atomic_int_t *p, int nv)
{
   int v = retro_atomic_exchange_int(p, nv);
   run_hook();
   return v;
}
#undef  retro_atomic_load_acquire_int
#define retro_atomic_load_acquire_int(p) hooked_load_acquire_int(p)
#undef  retro_atomic_exchange_int
#define retro_atomic_exchange_int(p, v)  hooked_exchange_int(p, v)

/* What the functions touch, and nothing more. */
typedef struct { void *instance; } ANativeActivity;
typedef struct { int left, top, right, bottom; } ARect;
struct android_app
{
   struct
   {
      retro_atomic_int_t dims;
      retro_atomic_int_t changed;
   } content_rect;
};
enum { VK_DATA_FLAG_NEED_NEW_SWAPCHAIN = (1 << 1) };
typedef struct
{
   struct { unsigned flags; } vk;
   unsigned dims;
} android_ctx_data_vk_t;
static struct android_app *g_android;
#define RARCH_LOG(...) ((void)0)

/* The gl context's surface, as eglQuerySurface() reports it */
#define HAVE_EGL
typedef struct { unsigned dims; } egl_ctx_data_t;
typedef struct { egl_ctx_data_t egl; } android_ctx_data_t;

static void egl_get_video_size(egl_ctx_data_t *egl, unsigned *dims)
{
   *dims = egl->dims;
}

#include "content_rect_driver.h"

static unsigned failures;
static struct android_app app;
static ANativeActivity activity;
static android_ctx_data_vk_t ctx;
static android_ctx_data_t gl_ctx;
static unsigned dims;

static void ui_change(int w, int h)
{
   ARect r;
   r.left   = 10;
   r.top    = 20;
   r.right  = 10 + w;
   r.bottom = 20 + h;
   onContentRectChanged(&activity, &r);
}

static bool check(void)
{
   bool quit = false, resize = false;
   ctx.vk.flags &= ~VK_DATA_FLAG_NEED_NEW_SWAPCHAIN;
   android_gfx_ctx_vk_check_window(&ctx, &quit, &resize, &dims);
   return resize;
}

static bool gl_check(void)
{
   bool quit = false, resize = false;
   android_gfx_ctx_check_window(&gl_ctx, &quit, &resize, &dims);
   return resize;
}

static void fail(const char *what)
{
   printf("   FAIL %s (read %ux%u)\n", what,
         VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims));
   failures++;
}

static void change_2400_1080(void) { ui_change(2400, 1080); }
static void change_same(void)      { ui_change(1280, 720); }

int main(void)
{
   printf("android content rect:\n");
   g_android         = &app;
   activity.instance = &app;
   memset(&app, 0, sizeof(app));

   ui_change(1280, 720);
   if (!check() || VIDEO_SCALE_W(dims) != 1280 || VIDEO_SCALE_H(dims) != 720)
      fail("a published size reaches check_window");
   else
      printf("   ok   1280x720 published, read, resize asked\n");

   if (check())
      fail("no change, yet a resize was asked");
   else
      printf("   ok   no change: no resize\n");

   /* A change landing in each gap between the reader's operations. */
   {
      int k;
      for (k = 0; k < 3; k++)
      {
         ui_change(1280, 720);
         check();
         op_hook = change_2400_1080;
         op_skip = k;
         check();
         /* A reader with fewer operations than this gap: the change
          * lands after it instead. */
         if (op_hook)
         {
            op_hook = NULL;
            change_2400_1080();
         }
         if (   !(VIDEO_SCALE_W(dims) == 1280 && VIDEO_SCALE_H(dims) == 720)
             && !(VIDEO_SCALE_W(dims) == 2400 && VIDEO_SCALE_H(dims) == 1080))
            fail("a change during check_window gave a size no one published");
         else
            printf("   ok   a change after operation %d: %ux%u, as published\n",
                  k + 1, VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims));
         check();
         if (VIDEO_SCALE_W(dims) != 2400 || VIDEO_SCALE_H(dims) != 1080)
            fail("the next check did not take the change");
      }
   }

   /* A same-size change (the rectangle moved, the size did not) raised
    * again while the reader is taking the first one's flag: the second
    * must survive to the next check. */
   ui_change(1280, 720);
   check();
   ui_change(1280, 720);
   op_hook = change_same;
   op_skip = 0;
   check();
   op_hook = NULL;
   if (!check())
      fail("a change raised during the read was cleared unseen");
   else
      printf("   ok   a change raised during the read is seen next check\n");

   /* A quarter turn: the width and height swap */
   ui_change(1920, 1080);
   if (!check() || VIDEO_SCALE_W(dims) != 1920 || VIDEO_SCALE_H(dims) != 1080)
      fail("vulkan: 1920x1080 published, read");
   ui_change(1080, 1920);
   if (!check() || VIDEO_SCALE_W(dims) != 1080 || VIDEO_SCALE_H(dims) != 1920)
      fail("vulkan: a turn asks a resize at the swapped size");
   else if (check())
      fail("vulkan: the turn asked a second resize");
   else
      printf("   ok   vulkan: 1920x1080 turned to 1080x1920, one resize\n");

   /* gl asks a resize only when the size moves: start from another */
   dims            = VIDEO_SCALE_PACK(1280, 720);
   gl_ctx.egl.dims = VIDEO_SCALE_PACK(1920, 1080);
   if (     !gl_check()
         || VIDEO_SCALE_W(dims) != 1920 || VIDEO_SCALE_H(dims) != 1080)
      fail("gl: a 1920x1080 surface, read");
   gl_ctx.egl.dims = VIDEO_SCALE_PACK(1080, 1920);
   if (     !gl_check()
         || VIDEO_SCALE_W(dims) != 1080 || VIDEO_SCALE_H(dims) != 1920)
      fail("gl: a turn asks a resize at the swapped size");
   else if (gl_check())
      fail("gl: the turn asked a second resize");
   else
      printf("   ok   gl: 1920x1080 turned to 1080x1920, one resize\n");

   if (failures)
   {
      printf("android content rect: %u failure(s)\n", failures);
      return 1;
   }
   printf("android content rect: every size read was published, no change lost\n");
   return 0;
}
