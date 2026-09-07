/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <android/native_window.h>

#include "../../verbosity.h"
#include "../video_display_server.h"
#include "../../frontend/drivers/platform_unix.h"

/* FORWARD DECLARATIONS */
int system_property_get(const char *cmd, const char *args,
      char *value, size_t value_size);

static void* android_display_server_init(void) { return NULL; }
static void android_display_server_destroy(void *data) { }
static bool android_display_server_set_window_opacity(void *data, unsigned opacity) { return true; }
static bool android_display_server_set_window_progress(void *data, int progress, bool finished) { return true; }
static uint32_t android_display_server_get_flags(void *data) { return 0; }

static void android_display_server_set_screen_orientation(void *data,
      enum rotation rotation)
{
   JNIEnv *env = jni_thread_getenv();

   if (!env || !g_android)
      return;

   if (g_android->setScreenOrientation)
      CALL_VOID_METHOD_PARAM(env, g_android->activity->clazz,
            g_android->setScreenOrientation, rotation);
}

/* The rate the screen is actually running at.
 *
 * Reported by the Activity rather than derived from the video driver:
 * Android switches display mode underneath the app - for refresh rate
 * switching, battery saver, or an app-requested preferredRefreshRate
 * such as the one set in RetroActivityFuture - and only the platform
 * knows the mode currently in effect.  Read live for the same reason:
 * a cached value would go stale the moment the system switched.
 *
 * Returns 0 when it cannot be determined, which is what the caller
 * already treats as "unknown". */
static float android_display_server_get_refresh_rate(void *data)
{
   jfloat refresh_rate = 0.0f;
   JNIEnv *env         = jni_thread_getenv();

   if (!env || !g_android)
      return 0.0f;

   if (g_android->getRefreshRate)
      CALL_FLOAT_METHOD(env, refresh_rate,
            g_android->activity->clazz, g_android->getRefreshRate);

   return (float)refresh_rate;
}

/* The modes the display supports.
 *
 * Mode enumeration is API 23 on the Java side; older devices report a
 * single mode describing their current state, so this always yields
 * at least one entry with exactly one marked current, whatever the
 * device.  The Java packs {id, width, height, millihertz} per mode -
 * millihertz because the config carries an integer rate alongside the
 * float one, and 59.94 must survive the trip as more than 59.
 *
 * The caller owns the returned array and frees it. */
/* What was last asked for, so it can be asserted again on a new
 * window.
 *
 * Both halves of a mode change are window state and neither survives
 * the app going to the background: the window is torn down and a new
 * ANativeWindow arrives on resume, taking the frame rate with it,
 * and the framework re-evaluates the mode for the new window. */
static int   android_last_mode_id  = 0;
static float android_last_mode_hz  = 0.0f;

static void *android_display_server_get_resolution_list(
      void *data, unsigned *len)
{
   struct video_display_config *conf = NULL;
   jintArray packed                  = NULL;
   jint *modes                       = NULL;
   jint current_id                   = 0;
   jsize packed_len                  = 0;
   unsigned count                    = 0;
   unsigned i;
   JNIEnv *env                       = jni_thread_getenv();

   if (len)
      *len = 0;

   if (!env || !g_android || !g_android->getDisplayModes)
      return NULL;

   CALL_OBJ_METHOD(env, packed, g_android->activity->clazz,
         g_android->getDisplayModes);

   if (!packed)
      return NULL;

   packed_len = (*env)->GetArrayLength(env, packed);
   count      = (unsigned)(packed_len / 4);

   if (count < 1)
   {
      (*env)->DeleteLocalRef(env, packed);
      return NULL;
   }

   if (g_android->getCurrentDisplayModeId)
      CALL_INT_METHOD(env, current_id, g_android->activity->clazz,
            g_android->getCurrentDisplayModeId);

   if (!(modes = (*env)->GetIntArrayElements(env, packed, NULL)))
   {
      (*env)->DeleteLocalRef(env, packed);
      return NULL;
   }

   if (!(conf = (struct video_display_config*)
         calloc(count, sizeof(struct video_display_config))))
   {
      (*env)->ReleaseIntArrayElements(env, packed, modes, JNI_ABORT);
      (*env)->DeleteLocalRef(env, packed);
      return NULL;
   }

   for (i = 0; i < count; i++)
   {
      jint id       = modes[i * 4];
      jint width    = modes[i * 4 + 1];
      jint height   = modes[i * 4 + 2];
      jint millihz  = modes[i * 4 + 3];

      conf[i].width             = (unsigned)width;
      conf[i].height            = (unsigned)height;
      /* Android composites 32-bit regardless of the mode. */
      conf[i].bpp               = 32;
      conf[i].refreshrate       = (unsigned)((millihz + 500) / 1000);
      conf[i].refreshrate_float = (float)millihz / 1000.0f;
      /* The mode id, not the array position: set_resolution has to
       * hand this exact value back to the platform. */
      conf[i].idx               = (unsigned)id;
      conf[i].current           = (id == current_id);
      conf[i].interlaced        = false;
      conf[i].dblscan           = false;
   }

   /* A display that reported no current mode - pre-Marshmallow, or a
    * query that failed - would otherwise leave the list with nothing
    * marked, and the menu with no selection. */
   if (count == 1)
      conf[0].current = true;

   (*env)->ReleaseIntArrayElements(env, packed, modes, JNI_ABORT);
   (*env)->DeleteLocalRef(env, packed);

   if (len)
      *len = count;

   {
      static bool logged_once = false;
      if (!logged_once)
      {
         logged_once = true;
         RARCH_LOG("[Android] Display reports %u mode(s); current is"
               " %ux%u @ %.2f Hz.\n", count,
               conf[0].width, conf[0].height, conf[0].refreshrate_float);
      }
   }

   return conf;
}

/* Asks the system for a mode.
 *
 * Android does not let an app set an arbitrary resolution: it picks
 * from the modes the display declares, by id.  The width and height
 * are matched against the list to find that id, and the refresh rate
 * decides between modes that share a size - which is how a 60Hz and a
 * 120Hz entry of the same resolution stay distinguishable.
 *
 * A request, not a guarantee: the system may stay where it is. */
static bool android_display_server_set_resolution(void *data,
      unsigned width, unsigned height, int int_hz, float hz,
      int center, int monitor_index, int xoffset, int padjust)
{
   struct video_display_config *conf = NULL;
   unsigned count                    = 0;
   unsigned i;
   int best_id                       = -1;
   float best_delta                  = 0.0f;
   jboolean ok                       = JNI_FALSE;
   JNIEnv *env                       = jni_thread_getenv();

   if (!env || !g_android || !g_android->setDisplayModeId)
      return false;

   if (!(conf = (struct video_display_config*)
         android_display_server_get_resolution_list(data, &count)))
      return false;

   /* A rate-only change asks for size 0x0.
    *
    * video_display_server_set_refresh_rate() - which is how the
    * frontend changes refresh rate, as opposed to picking an entry
    * from the resolution list - calls in with width and height both
    * zero and only the rate filled in.  Matching those literally
    * matched nothing, so every rate-only switch failed silently.
    * Zero means "whatever size is current". */
   if (width == 0 || height == 0)
   {
      for (i = 0; i < count; i++)
      {
         if (!conf[i].current)
            continue;
         width  = conf[i].width;
         height = conf[i].height;
         break;
      }
   }

   for (i = 0; i < count; i++)
   {
      float delta;

      if (conf[i].width != width || conf[i].height != height)
         continue;

      delta = conf[i].refreshrate_float - hz;
      if (delta < 0.0f)
         delta = -delta;

      if (best_id < 0 || delta < best_delta)
      {
         best_id    = (int)conf[i].idx;
         best_delta = delta;
      }
   }

   free(conf);

   if (best_id < 0)
      return false;

   CALL_BOOLEAN_METHOD_PARAM(env, ok, g_android->activity->clazz,
         g_android->setDisplayModeId, (jint)best_id);

   if (ok == JNI_TRUE)
   {
      android_last_mode_id = best_id;
      android_last_mode_hz = hz;
   }

   /* Android treats a mode change as a REQUEST.  It can decline
    * silently - most often for a mode whose resolution differs from
    * the current one, which many devices will not switch for an
    * ordinary app even though they list it.  Logging what was asked
    * for, and what the display reports afterwards, is the difference
    * between "it did not work" and knowing why. */
   RARCH_LOG("[Android] Display mode %d requested for %ux%u @ %.2f Hz"
         " (accepted: %s).\n",
         best_id, width, height, hz, (ok == JNI_TRUE) ? "yes" : "no");

   /* Tell SurfaceFlinger what this window wants, as well as asking
    * the framework for the mode.
    *
    * They are separate mechanisms and both matter.  The mode request
    * moves the DISPLAY; the frame rate on the window moves the vote
    * SurfaceFlinger casts for OUR LAYER.  With no frame rate set the
    * layer reports "Max (can\'t resolve refresh rate)" and the
    * compositor picks for us - on this hardware, via Samsung\'s touch
    * control, that pick is 60 Hz, which then holds the panel there
    * whatever mode was requested.
    *
    * FIXED_SOURCE says the content really is produced at this rate,
    * which is what a fixed-rate emulated system is, and is the
    * compatibility value that permits a mode switch rather than
    * asking the compositor to fit the content to whatever it is
    * already doing.
    *
    * API 30.  Older devices simply do not have the call; the mode
    * request alone is all there is there. */
#if __ANDROID_API__ >= 30
   if (ok == JNI_TRUE && g_android->window)
   {
      int fr = ANativeWindow_setFrameRate(g_android->window, hz,
            ANATIVEWINDOW_FRAME_RATE_COMPATIBILITY_FIXED_SOURCE);
      RARCH_LOG("[Android] Window frame rate set to %.2f Hz (result:"
            " %d).\n", hz, fr);
   }
#endif

   /* Whether the panel actually moved is a separate question from
    * whether the request was accepted: a device policy - Samsung's
    * Game Optimizing Service pins apps it classifies as games to
    * 60 fps, for instance - can hold the display where it is
    * regardless.  Report what the display says afterwards so the two
    * are distinguishable. */
   if (ok == JNI_TRUE && g_android->getRefreshRate)
   {
      jfloat now = 0.0f;
      CALL_FLOAT_METHOD(env, now, g_android->activity->clazz,
            g_android->getRefreshRate);
      RARCH_LOG("[Android] Display now reports %.2f Hz.\n", (float)now);
   }

   return (ok == JNI_TRUE);
}

/* The mode currently in effect, for the "Screen Resolution" label.
 *
 * This is a separate callback from get_resolution_list, and the label
 * reads only this one: with it unimplemented the menu shows N/A no
 * matter how complete the list is, and never changes after a
 * selection.
 *
 * The description carries the refresh rate, because a phone panel
 * offers the same resolution at several rates and the resolution
 * alone would not say which one is running. */
static void android_display_server_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *s, size_t len)
{
   struct video_display_config *conf = NULL;
   unsigned count                    = 0;
   unsigned i;

   if (!(conf = (struct video_display_config*)
         android_display_server_get_resolution_list(data, &count)))
      return;

   for (i = 0; i < count; i++)
   {
      if (!conf[i].current)
         continue;

      if (width)
         *width  = conf[i].width;
      if (height)
         *height = conf[i].height;
      if (s && len)
         snprintf(s, len, "%.2f Hz", conf[i].refreshrate_float);
      break;
   }

   free(conf);
}

/* Re-assert the chosen mode and frame rate on a window that has just
 * been created.  Called from the INIT_WINDOW path, which is where a
 * resume produces a fresh surface. */
void android_display_server_reapply_mode(void)
{
   JNIEnv *env = jni_thread_getenv();

   if (!android_last_mode_id || !env || !g_android)
      return;

   if (g_android->setDisplayModeId)
   {
      jboolean ok = JNI_FALSE;
      CALL_BOOLEAN_METHOD_PARAM(env, ok, g_android->activity->clazz,
            g_android->setDisplayModeId, (jint)android_last_mode_id);
      RARCH_LOG("[Android] Re-applied display mode %d on new window"
            " (accepted: %s).\n", android_last_mode_id,
            (ok == JNI_TRUE) ? "yes" : "no");
   }

#if __ANDROID_API__ >= 30
   if (g_android->window && android_last_mode_hz > 0.0f)
      ANativeWindow_setFrameRate(g_android->window,
            android_last_mode_hz,
            ANATIVEWINDOW_FRAME_RATE_COMPATIBILITY_FIXED_SOURCE);
#endif
}

/* Step to the adjacent video mode.
 *
 * "Adjacent" means the next entry with a DIFFERENT resolution, the
 * way the win32 server reads it: a phone lists the same size at
 * several refresh rates, and stepping through those one at a time
 * would make the control feel broken - several presses that change
 * nothing visible.  Rates are chosen from the resolution list
 * instead; this control moves between sizes.
 *
 * Wraps at both ends, so holding either direction cycles rather than
 * sticking.  When only one distinct size exists there is nowhere to
 * go and this reports failure, which is what lets the caller fall
 * through to the video driver's own handler.
 *
 * @dir is +1 for next, -1 for previous. */
static bool android_display_server_step_video_output(void *data, int dir)
{
   struct video_display_config *conf = NULL;
   unsigned count                    = 0;
   unsigned current                  = 0;
   unsigned curr_width               = 0;
   unsigned curr_height              = 0;
   unsigned i;
   bool found                        = false;

   if (!(conf = (struct video_display_config*)
         android_display_server_get_resolution_list(data, &count)))
      return false;

   for (i = 0; i < count; i++)
   {
      if (conf[i].current)
      {
         current     = i;
         curr_width  = conf[i].width;
         curr_height = conf[i].height;
         break;
      }
   }

   /* Walk the whole list once from the current position, so the
    * search terminates whether or not a different size exists. */
   for (i = 1; i <= count; i++)
   {
      unsigned idx = (dir > 0)
         ? (current + i) % count
         : (current + count - (i % count)) % count;

      if (     conf[idx].width  == curr_width
            && conf[idx].height == curr_height)
         continue;

      found = android_display_server_set_resolution(data,
            conf[idx].width, conf[idx].height,
            (int)conf[idx].refreshrate, conf[idx].refreshrate_float,
            0, 0, 0, 0);
      break;
   }

   free(conf);

   return found;
}

static void android_display_server_get_video_output_next(void *data)
{
   android_display_server_step_video_output(data, 1);
}

static void android_display_server_get_video_output_prev(void *data)
{
   android_display_server_step_video_output(data, -1);
}

static void android_display_dpi_get_density(char *s, size_t len)
{
   static bool inited_once             = false;
   static bool inited2_once            = false;
   static char string[PROP_VALUE_MAX]  = {0};
   static char string2[PROP_VALUE_MAX] = {0};
   if (!inited_once)
   {
      system_property_get("getprop", "ro.sf.lcd_density", string, sizeof(string));
      inited_once = true;
   }

   if (*string)
   {
      strlcpy(s, string, len);
      return;
   }

   if (!inited2_once)
   {
      system_property_get("wm", "density", string2, sizeof(string2));
      inited2_once = true;
   }

   strlcpy(s, string2, len);
}

bool android_display_get_metrics(void *data,
	enum display_metric_types type, float *value)
{
   static int dpi = -1;

   switch (type)
   {
      case DISPLAY_METRIC_MM_WIDTH:
      case DISPLAY_METRIC_MM_HEIGHT:
         return false;
      case DISPLAY_METRIC_DPI:
         if (dpi == -1)
         {
            char density[PROP_VALUE_MAX];
            android_display_dpi_get_density(density, sizeof(density));
            if (!*density)
               goto dpi_fallback;
            if ((dpi = atoi(density)) <= 0)
               goto dpi_fallback;
         }
         *value = (float)dpi;
         break;
      case DISPLAY_METRIC_NONE:
      default:
         *value = 0;
         return false;
   }

   return true;

dpi_fallback:
   /* add a fallback in case the device doesn't report DPI.
    * Hopefully fixes issues with the moto G2. */
   dpi    = 90;
   *value = (float)dpi;
   return true;
}

bool android_display_has_focus(void *data)
{
   bool                    focused = false;
   struct android_app *android_app = (struct android_app*)g_android;
   if (!android_app)
      return true;

   slock_lock(android_app->mutex);
   focused = !retro_atomic_load_acquire_int(&android_app->unfocused);
   slock_unlock(android_app->mutex);

   return focused;
}

const video_display_server_t dispserv_android = {
   android_display_server_init,
   android_display_server_destroy,
   android_display_server_set_window_opacity,
   android_display_server_set_window_progress,
   NULL, /* set_window_decorations */
   android_display_server_set_resolution,
   android_display_server_get_resolution_list,
   NULL, /* get_output_options */
   android_display_server_set_screen_orientation,
   NULL, /* get_screen_orientation */
   android_display_server_get_refresh_rate,
   android_display_server_get_video_output_size,
   android_display_server_get_video_output_prev,
   android_display_server_get_video_output_next,
   android_display_get_metrics,
   android_display_server_get_flags,
   NULL, /* get_scanline */
   NULL, /* wait_vblank */
   NULL, /* modeline_list_outputs */
   NULL, /* modeline_open */
   NULL, /* modeline_close */
   NULL, /* modeline_caps */
   NULL, /* modeline_enum */
   NULL, /* modeline_add */
   NULL, /* modeline_update */
   NULL, /* modeline_delete */
   NULL, /* modeline_set */
   NULL, /* modeline_flush */
   "android"
};
