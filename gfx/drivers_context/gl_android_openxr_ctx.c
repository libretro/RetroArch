/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Foundation,
 *  either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* OpenGL ES context driver that binds to an OpenXR session instead of
 * an Android ANativeWindow EGL surface.
 * Ffor use with gfx/drivers/gl2.c. */

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <string/stdstring.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <glsym/glsym.h>
#include <gfx/gl_capabilities.h>

#include <jni.h>
#include <android/native_activity.h>
#ifndef XR_USE_PLATFORM_ANDROID
#define XR_USE_PLATFORM_ANDROID
#endif
#ifndef XR_USE_GRAPHICS_API_OPENGL_ES
#define XR_USE_GRAPHICS_API_OPENGL_ES
#endif
#ifdef HAVE_VULKAN
/* openxr_platform.h is include-guarded, so in the single griffin TU this
 * first inclusion decides which API sections exist; enable the Vulkan one
 * for the Vulkan OpenXR context that follows, with the types it needs. */
#ifndef XR_USE_GRAPHICS_API_VULKAN
#define XR_USE_GRAPHICS_API_VULKAN
#endif
#include "../common/vulkan_common.h"
#endif

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "../../frontend/drivers/platform_unix.h"
#include "../../verbosity.h"
#include "../../input/input_driver.h"
#include "../video_driver.h"

#include "gl_android_openxr.h"
#include "../common/openxr_runtime.h"
#include "../common/openxr_session.h"
#include "../common/openxr_swapchain.h"
#include "../common/openxr_frame.h"
#include "../common/openxr_tracking.h"
#include "../common/openxr_host.h"
#include "../common/openxr_binding_gles.h"
#include "../video_xr.h"
#include "../../configuration.h"
#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x00000040
#endif

typedef struct gl_android_openxr
{
   openxr_runtime_t rt;
   openxr_session_t session;
   openxr_swapchains_t sc;
   openxr_frame_t frame;
   openxr_frame_begun_t begun;
   openxr_host_t host;           /* the pose answers, as every host's */
   XrLoaderInitInfoAndroidKHR loader;
   XrInstanceCreateInfoAndroidKHR instance_info;
   video_xr_quad_set_t set;
   /* The slots' images, [slot][image][layer]. */
   uint32_t images[OPENXR_MAX_SLOTS][OPENXR_MAX_IMAGES][2];
   unsigned num_images[OPENXR_MAX_SLOTS];
   unsigned slot_dims[OPENXR_MAX_SLOTS];
   unsigned slot_failed[OPENXR_MAX_SLOTS];
   int64_t format;
   /* Set and read on the video thread. */
   retro_atomic_int_t stereo;
   bool in_frame;
   bool ended;
   /* The Recenter hotkey's count gl2 last passed. */
   unsigned recenter;
   bool recenter_seen;
   unsigned interval;
   /* The headset was given a projection to show. */
   bool proj_shown;
   /* The window: gl2 draws its frame here, the headset shows it. */
   GLuint fbo;
   GLuint tex;
   GLuint copy_fbo;
   unsigned dims;
   /* The screens' distance and width gl2 last passed. */
   float distance;
   float width;
   /* The frame's views as gl2 last passed them, with where the window
    * has the frame: gl2 sets them in its frame, before the swap that
    * draws with them, both on the video thread. */
   video_views_map_t views;
   video_viewport_t views_vp;
   unsigned views_dims;
   unsigned rotation;
   unsigned screen_layout;
   bool swap_eyes;
   /* An sRGB swapchain filled with GL_FRAMEBUFFER_SRGB_EXT off around
    * the copy (EXT_sRGB_write_control). */
   bool srgb_copy;

   EGLDisplay egl_display;
   EGLConfig egl_config;
   EGLSurface egl_surface;
   EGLContext egl_context;       /* OpenXR / video-thread context */
   EGLContext egl_hw_context;    /* hardware-core / main-thread context */

   struct android_app *android_app;
} gl_android_openxr_t;

static gl_android_openxr_t gl_xr;

/* ===================== EGL ===================== */

static bool gl_xr_egl_init(gl_android_openxr_t *xr)
{
   EGLint num_configs = 0;
   EGLint major = 0, minor = 0;

   static const EGLint pbuffer_attribs[] = {
      EGL_WIDTH, 16,
      EGL_HEIGHT, 16,
      EGL_NONE
   };
   /* ES3: the copies into the headset's images blit. */
   static const EGLint cfg_attribs[] = {
      EGL_RED_SIZE,        8,
      EGL_GREEN_SIZE,      8,
      EGL_BLUE_SIZE,       8,
      EGL_ALPHA_SIZE,      8,
      EGL_DEPTH_SIZE,      0,
      EGL_STENCIL_SIZE,    0,
      EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
      EGL_NONE
   };
   static const EGLint ctx_attribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 3,
      EGL_NONE
   };

   xr->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

   if (xr->egl_display == EGL_NO_DISPLAY
         || !eglInitialize(xr->egl_display, &major, &minor))
   {
      RARCH_ERR("[XR] eglInitialize failed.\n");
      return false;
   }

   const char *ext = eglQueryString(xr->egl_display, EGL_EXTENSIONS);

   if (ext && strstr(ext, "EGL_KHR_surfaceless_context"))
      RARCH_DBG("[XR] EGL_KHR_surfaceless_context supported\n");
   else
      RARCH_WARN("[XR] EGL_KHR_surfaceless_context NOT supported\n");

   eglBindAPI(EGL_OPENGL_ES_API);

   if (     eglChooseConfig(xr->egl_display, cfg_attribs,
               &xr->egl_config, 1, &num_configs)
         && num_configs >= 1)
      xr->egl_context = eglCreateContext(
            xr->egl_display,
            xr->egl_config,
            EGL_NO_CONTEXT,
            ctx_attribs);

   if (xr->egl_context == EGL_NO_CONTEXT)
   {
      RARCH_ERR("[XR] Failed to create an OpenGL ES context.\n");
      return false;
   }

   RARCH_DBG("[XR] Created EGL %d.%d, OpenGL ES 3 context.\n",
         major, minor);

   xr->egl_surface = eglCreatePbufferSurface(
         xr->egl_display,
         xr->egl_config,
         pbuffer_attribs);

   if (xr->egl_surface == EGL_NO_SURFACE)
   {
      RARCH_ERR("[XR] Failed to create pbuffer surface: EGL error 0x%x\n",
            eglGetError());
      return false;
   }

   if (!eglMakeCurrent(
         xr->egl_display,
         xr->egl_surface,
         xr->egl_surface,
         xr->egl_context))
   {
      RARCH_ERR("[XR] eglMakeCurrent failed: EGL error 0x%x\n",
            eglGetError());
      return false;
   }

   /*
    * The hardware core runs on a different thread from the threaded
    * video/OpenXR context. EGL does not permit the same EGLContext
    * to be current on two threads, so create a second context that
    * shares objects with the OpenXR context.
    */
   xr->egl_hw_context = eglCreateContext(
         xr->egl_display,
         xr->egl_config,
         xr->egl_context,
         ctx_attribs);

   if (xr->egl_hw_context == EGL_NO_CONTEXT)
   {
      RARCH_ERR("[XR] eglCreateContext(hw) failed: EGL error 0x%x\n",
            eglGetError());
      return false;
   }

   return true;
}

static void gl_xr_egl_deinit(gl_android_openxr_t *xr)
{
   if (xr->egl_display == EGL_NO_DISPLAY || !xr->egl_display)
      return;

   eglMakeCurrent(xr->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
         EGL_NO_CONTEXT);
   if (xr->egl_surface && xr->egl_surface != EGL_NO_SURFACE)
      eglDestroySurface(xr->egl_display, xr->egl_surface);
   if (xr->egl_hw_context != EGL_NO_CONTEXT)
   {
      eglDestroyContext(xr->egl_display, xr->egl_hw_context);
      xr->egl_hw_context = EGL_NO_CONTEXT;
   }
   if (xr->egl_context && xr->egl_context != EGL_NO_CONTEXT)
      eglDestroyContext(xr->egl_display, xr->egl_context);
   eglTerminate(xr->egl_display);
   xr->egl_surface = EGL_NO_SURFACE;
   xr->egl_context = EGL_NO_CONTEXT;
   xr->egl_display = EGL_NO_DISPLAY;
}

/* ===================== the host ===================== */

static void gl_xr_local_changed(void *user)
{
   openxr_frame_local_changed(&((gl_android_openxr_t*)user)->frame);
}

static void gl_xr_stage_changed(void *user)
{
   openxr_host_stage_changed(&((gl_android_openxr_t*)user)->host);
}

/* Exit or loss: the headset was all there was. */
static void gl_xr_ended(void *user)
{
   gl_android_openxr_t *xr = (gl_android_openxr_t*)user;
   xr->ended               = true;
   retro_atomic_store_release_int(&xr->stereo, 0);
}

static unsigned gl_android_openxr_host_state(void *user)
{
   gl_android_openxr_t *xr = (gl_android_openxr_t*)user;
   unsigned bits           = 0;
   if (openxr_session_alive(&xr->session))
      bits |= VIDEO_XR_HEADSET_PRESENTING;
   if (openxr_session_running(&xr->session))
      bits |= VIDEO_XR_HEADSET_TRACKING;
   if (openxr_session_focused(&xr->session))
      bits |= VIDEO_XR_HEADSET_FOCUSED;
   return bits;
}

static bool gl_android_openxr_host_sample(void *user, unsigned ahead,
      uint32_t *frame_flags)
{
   return openxr_host_sample(&((gl_android_openxr_t*)user)->host, ahead, frame_flags);
}

static bool gl_android_openxr_host_eye_state(void *user,
      struct retro_vr_eye_state out[2], unsigned *token)
{
   return openxr_host_eye_state(&((gl_android_openxr_t*)user)->host, out, token);
}

static void gl_android_openxr_host_give(void *user, unsigned token)
{
   openxr_host_give(&((gl_android_openxr_t*)user)->host, token);
}

static bool gl_android_openxr_host_set_space(void *user,
      enum retro_vr_reference_space *space, unsigned *w, unsigned *h)
{
   return openxr_host_set_space(&((gl_android_openxr_t*)user)->host, space,
         w, h);
}

static bool gl_android_openxr_host_head_pose(void *user,
      struct retro_vr_head_pose *out)
{
   return openxr_host_head_pose(&((gl_android_openxr_t*)user)->host, out);
}

static bool gl_android_openxr_host_locate_hands(void *user)
{
   return openxr_host_locate_hands(&((gl_android_openxr_t*)user)->host);
}

static bool gl_android_openxr_host_controller_poses(void *user,
      struct retro_vr_controller_poses *out)
{
   return openxr_host_controller_poses(&((gl_android_openxr_t*)user)->host, out);
}

static bool gl_android_openxr_host_eye_size(void *user, unsigned *w,
      unsigned *h)
{
   return openxr_host_eye_size(&((gl_android_openxr_t*)user)->host, w, h);
}

static void gl_android_openxr_host_resume(void *user)
{
   openxr_host_resume(&((gl_android_openxr_t*)user)->host);
}

static void gl_android_openxr_host_register(gl_android_openxr_t *xr)
{
   video_xr_host_t host;
   memset(&host, 0, sizeof(host));
   host.state     = gl_android_openxr_host_state;
   host.sample    = gl_android_openxr_host_sample;
   host.eye_state = gl_android_openxr_host_eye_state;
   host.give      = gl_android_openxr_host_give;
   host.set_space = gl_android_openxr_host_set_space;
   host.head_pose = gl_android_openxr_host_head_pose;
   host.locate_hands = gl_android_openxr_host_locate_hands;
   host.controller_poses = gl_android_openxr_host_controller_poses;
   host.eye_size  = gl_android_openxr_host_eye_size;
   host.resume    = gl_android_openxr_host_resume;
   host.user      = xr;
   video_driver_set_xr_host(&host);
}

/* ===================== the session ===================== */

/* The dot's image, with the video context current. */
static bool gl_xr_cursor_fill(void *user, uint32_t index,
      const char **step, int *code)
{
   static uint8_t px[VIDEO_XR_CURSOR_DIM * VIDEO_XR_CURSOR_DIM * 4];
   uint32_t (*tex)[2] = (uint32_t (*)[2])user;
   video_xr_cursor_pixels(px);
   glBindTexture(GL_TEXTURE_2D, (GLuint)tex[index][0]);
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, VIDEO_XR_CURSOR_DIM,
         VIDEO_XR_CURSOR_DIM, GL_RGBA, GL_UNSIGNED_BYTE, px);
   glBindTexture(GL_TEXTURE_2D, 0);
   /* The compositor reads the image on its own context. */
   glFlush();
   return true;
}

/* The laser's dot, as Vulkan's; none if the runtime will not make it. */
static void gl_xr_cursor_create(gl_android_openxr_t *xr)
{
   static const int64_t formats[2] = {
      OPENXR_GLES_SRGB8_ALPHA8, OPENXR_GLES_RGBA8 };
   uint32_t tex[OPENXR_MAX_IMAGES][2];
   openxr_gles_images_t li;
   const char *step = NULL;
   int code         = 0;

   li.sc            = &xr->sc;
   li.tex           = tex;
   xr->frame.cursor = openxr_swapchains_create_filled(&xr->sc, formats, 2,
         VIDEO_SCALE_PACK(VIDEO_XR_CURSOR_DIM, VIDEO_XR_CURSOR_DIM),
         openxr_binding_gles_images, &li, gl_xr_cursor_fill, tex,
         &step, &code);
   if (xr->frame.cursor)
      RARCH_LOG("[XR] Laser cursor ready.\n");
   else
      RARCH_WARN("[XR] No laser cursor (%s, %d).\n", step, code);
}

static bool gl_xr_start(gl_android_openxr_t *xr)
{
   XrGraphicsBindingOpenGLESAndroidKHR binding;

   xr->loader.type                       = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
   xr->loader.applicationVM              = xr->android_app->activity->vm;
   xr->loader.applicationContext         = xr->android_app->activity->clazz;
   xr->instance_info.type                = XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR;
   xr->instance_info.applicationVM       = xr->android_app->activity->vm;
   xr->instance_info.applicationActivity = xr->android_app->activity->clazz;
   xr->rt.platform.loader        =
      (const XrLoaderInitInfoBaseHeaderKHR*)&xr->loader;
   xr->rt.platform.instance_next = &xr->instance_info;
   xr->rt.platform.instance_ext  =
      XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME;

   if (     openxr_runtime_create_instance(&xr->rt, xrGetInstanceProcAddr,
               XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME) != OPENXR_RUNTIME_OK
         || openxr_runtime_find_system(&xr->rt) != OPENXR_RUNTIME_OK
         || !openxr_binding_gles_requirements(&xr->rt))
      return false;

   /* One thread holds the session's EGL context: no lock to take. */
   xr->session.hooks.ended         = gl_xr_ended;
   xr->session.hooks.local_changed = gl_xr_local_changed;
   xr->session.hooks.stage_changed = gl_xr_stage_changed;
   xr->session.hooks.user          = xr;
   if (     !openxr_session_load(&xr->session, &xr->rt)
         || !openxr_swapchains_load(&xr->sc, &xr->rt, &xr->session)
         || !openxr_frame_load(&xr->frame, &xr->rt, &xr->session, &xr->sc))
   {
      RARCH_ERR("[OpenXR] The runtime lacks session functions.\n");
      return false;
   }
   openxr_host_init(&xr->host, &xr->rt, &xr->session, &xr->frame);

   binding.type    = XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR;
   binding.next    = NULL;
   binding.display = xr->egl_display;
   binding.config  = xr->egl_config;
   binding.context = xr->egl_context;
   if (!openxr_session_create(&xr->session, &binding))
      return false;
   openxr_swapchains_list_formats(&xr->sc);
   {
      bool write_control = gl_query_extension("GL_EXT_sRGB_write_control");
      if (!(xr->format = openxr_binding_gles_format(&xr->sc, write_control)))
         return false;
      xr->srgb_copy = write_control
         && xr->format == OPENXR_GLES_SRGB8_ALPHA8;
      if (xr->srgb_copy)
         RARCH_LOG("[OpenXR] sRGB swapchain.\n");
   }
   openxr_frame_session_created(&xr->frame);
   openxr_host_session_created(&xr->host);

   openxr_frame_input_attach(&xr->frame);
   gl_xr_cursor_create(xr);
   return openxr_frame_start_inline(&xr->frame);
}

static void gl_xr_stop(gl_android_openxr_t *xr)
{
   unsigned s;
   if (xr->frame.session)
      openxr_frame_stop_inline(&xr->frame);
   for (s = 0; s < OPENXR_MAX_SLOTS; s++)
      openxr_slot_destroy(&xr->sc, s);
   if (xr->frame.cursor)
   {
      xr->sc.DestroySwapchain(xr->frame.cursor);
      xr->frame.cursor = XR_NULL_HANDLE;
   }
   if (xr->session.session)
   {
      openxr_frame_input_detach(&xr->frame);
      openxr_session_destroy(&xr->session);
   }
   openxr_frame_deinit(&xr->frame);
   openxr_runtime_deinit(&xr->rt);
}

/* ===================== the window ===================== */

/* 16:9 at an eye's width: the frame and the menu, as a screen. With the
 * video context current, at init. */
static bool gl_xr_window_create(gl_android_openxr_t *xr)
{
   unsigned w = xr->rt.rec_width ? xr->rt.rec_width : 1280;
   unsigned h = w * 9 / 16;
   glGenTextures(1, &xr->tex);
   glBindTexture(GL_TEXTURE_2D, xr->tex);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA,
         GL_UNSIGNED_BYTE, NULL);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glBindTexture(GL_TEXTURE_2D, 0);
   glGenFramebuffers(1, &xr->fbo);
   glGenFramebuffers(1, &xr->copy_fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, xr->fbo);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
         GL_TEXTURE_2D, xr->tex, 0);
   if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
   {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      RARCH_ERR("[XR] The window's framebuffer is incomplete.\n");
      return false;
   }
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   xr->dims = VIDEO_SCALE_PACK(w, h);
   RARCH_LOG("[XR] The window is %ux%u, shown in the headset.\n", w, h);
   return true;
}

static void gl_xr_window_free(gl_android_openxr_t *xr)
{
   if (xr->fbo)
      glDeleteFramebuffers(1, &xr->fbo);
   if (xr->copy_fbo)
      glDeleteFramebuffers(1, &xr->copy_fbo);
   if (xr->tex)
      glDeleteTextures(1, &xr->tex);
   xr->fbo      = 0;
   xr->copy_fbo = 0;
   xr->tex      = 0;
}

/* ===================== the headset's frame ===================== */

/* A slot's swapchains at dims, made again when not. */
static bool gl_xr_slot_ensure(gl_android_openxr_t *xr, unsigned s,
      unsigned dims, unsigned layers)
{
   openxr_gles_images_t li;
   if (     xr->num_images[s] && xr->slot_dims[s] == dims
         && xr->sc.slots[s].layers == layers)
      return true;
   if (xr->slot_failed[s] == dims)
      return false;
   li.sc  = &xr->sc;
   li.tex = xr->images[s];
   if (     !openxr_slot_create(&xr->sc, s, xr->format, false, dims, layers,
               openxr_binding_gles_images, &li, &xr->num_images[s])
         || !xr->num_images[s])
   {
      xr->num_images[s]  = 0;
      xr->slot_failed[s] = dims;
      return false;
   }
   xr->slot_dims[s]   = dims;
   xr->slot_failed[s] = 0;
   return true;
}

/* One eye's image, as Vulkan's: the recommended size, within max_dim. */
static unsigned gl_xr_eye_dims(const gl_android_openxr_t *xr)
{
   unsigned w = xr->rt.rec_width;
   unsigned h = xr->rt.rec_height;
   if (!w || !h)
      return 0;
   return VIDEO_SCALE_PACK(MIN(w, xr->rt.max_dim), MIN(h, xr->rt.max_dim));
}

/* The window as a screen in the room, or an untracked map's screens
 * cut from it, unless a tracked map's eyes fill the headset and the
 * menu is closed. An image the compositor still holds keeps the last
 * one shown. */
static void gl_xr_draw(gl_android_openxr_t *xr)
{
   video_xr_params_t p;
   video_views_rect_t rects[VIDEO_XR_MAX_SLOTS][2];
   unsigned s, l;
   unsigned index[2];
   bool menu   = false;
   bool stereo = retro_atomic_load_acquire_int(&xr->stereo) != 0;

#ifdef HAVE_MENU
   menu = (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE) != 0;
#endif
   if (!stereo)
   {
      openxr_frame_clear_projection(&xr->frame);
      xr->proj_shown = false;
   }

   memset(&p, 0, sizeof(p));
   if (!stereo || menu || !xr->proj_shown)
   {
      p.map           = &xr->views;
      p.frame_dims    = xr->dims;
      video_xr_pose_identity(&p.anchor);
      p.distance      = xr->distance;
      p.width         = xr->width;
      p.px_per_rad    = openxr_frame_pixels_per_radian(&xr->frame);
      p.max_dim       = xr->rt.max_dim;
      p.screen_layout = xr->screen_layout;
      p.rotation      = xr->rotation;
      p.swap_eyes     = xr->swap_eyes;
      p.window        = true;
   }
   /* With no width, an empty set: the eyes are all there is. */
   video_xr_place_window(&p, menu, stereo, &xr->views_vp, xr->views_dims,
         &xr->set, rects);
   /* The blits are scissored: whatever gl2 drew last must not clip them. */
   glDisable(GL_SCISSOR_TEST);
   /* The copy keeps the stored values; the compositor decodes them.
    * Back on after it: gl2's sRGB passes share this context. */
   if (xr->srgb_copy)
      glDisable(OPENXR_GLES_FRAMEBUFFER_SRGB);
   glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
   for (s = 0; s < VIDEO_XR_MAX_SLOTS; s++)
   {
      unsigned dims   = xr->set.slots[s].dims;
      unsigned layers = xr->set.slots[s].layers;
      if (     !dims
            || !gl_xr_slot_ensure(xr, s, dims, layers)
            || !openxr_slot_acquire(&xr->sc, s, index))
         continue;
      glBindFramebuffer(GL_READ_FRAMEBUFFER, xr->fbo);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, xr->copy_fbo);
      for (l = 0; l < layers; l++)
      {
         /* The window is bottom up. What lies past it is not copied:
          * the clear leaves it black. */
         const video_views_rect_t *r = &rects[s][l];
         GLint x = VIDEO_POS_X(r->pos);
         GLint y = (GLint)VIDEO_SCALE_H(xr->views_vp.full_dims)
            - VIDEO_POS_Y(r->pos) - (GLint)VIDEO_SCALE_H(r->dims);
         glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
               GL_TEXTURE_2D, (GLuint)xr->images[s][index[l]][l], 0);
         glClear(GL_COLOR_BUFFER_BIT);
         glBlitFramebuffer(x, y, x + (GLint)VIDEO_SCALE_W(r->dims),
               y + (GLint)VIDEO_SCALE_H(r->dims), 0, 0,
               (GLint)VIDEO_SCALE_W(dims), (GLint)VIDEO_SCALE_H(dims),
               GL_COLOR_BUFFER_BIT, GL_LINEAR);
      }
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      /* The compositor reads the images on its own context. */
      glFlush();
      openxr_slot_release(&xr->sc, s);
   }
   if (xr->srgb_copy)
      glEnable(OPENXR_GLES_FRAMEBUFFER_SRGB);
   openxr_frame_publish(&xr->frame, &xr->set);
}

bool gl_android_openxr_active(void)
{
   return gl_xr.session.session != XR_NULL_HANDLE;
}

uint32_t gl_android_openxr_get_framebuffer(void)
{
   return (uint32_t)gl_xr.fbo;
}

void gl_android_openxr_set_stereo(bool on)
{
   retro_atomic_store_release_int(&gl_xr.stereo, on ? 1 : 0);
}

void gl_android_openxr_recenter(unsigned count)
{
   gl_android_openxr_t *xr = &gl_xr;
   if (     xr->session.session && xr->recenter_seen
         && xr->recenter != count)
      openxr_frame_request_recenter(&xr->frame);
   xr->recenter      = count;
   xr->recenter_seen = true;
}

void gl_android_openxr_set_placement(unsigned placement, float distance,
      float width)
{
   gl_xr.distance = distance;
   gl_xr.width    = width;
   openxr_frame_set_placement(&gl_xr.frame, placement);
}

void gl_android_openxr_set_views(const video_views_map_t *map,
      unsigned frame_dims, const video_viewport_t *vp, unsigned rotation,
      unsigned screen_layout, bool swap_eyes)
{
   gl_android_openxr_t *xr = &gl_xr;
   xr->views               = *map;
   xr->views_dims          = frame_dims;
   xr->views_vp            = *vp;
   xr->rotation            = rotation;
   xr->screen_layout       = screen_layout;
   xr->swap_eyes           = swap_eyes;
}

void gl_android_openxr_set_pacing(unsigned interval, float hz)
{
   gl_android_openxr_t *xr = &gl_xr;
   xr->interval = interval;
   if (xr->session.session)
      openxr_frame_request_rate(&xr->frame, hz);
}

float gl_android_openxr_refresh(float *rates, unsigned cap,
      unsigned *count)
{
   gl_android_openxr_t *xr = &gl_xr;
   *count = 0;
   if (!openxr_session_alive(&xr->session))
      return 0.0f;
   *count = openxr_frame_refresh_rates(&xr->frame, rates, cap);
   return openxr_frame_refresh_rate(&xr->frame);
}

bool gl_android_openxr_eye_targets(uint32_t tex[2], unsigned *dims)
{
   openxr_projection_t given;
   unsigned index[2];
   gl_android_openxr_t *xr = &gl_xr;
   unsigned d              = gl_xr_eye_dims(xr);
   if (     !d || !xr->in_frame || !xr->begun.render
         || !retro_atomic_load_acquire_int(&xr->stereo))
      return false;
   /* Only eyes the core was given poses for. */
   openxr_host_projection(&xr->host, 1, 1, &given);
   if (     !given.active
         || !gl_xr_slot_ensure(xr, OPENXR_PROJECTION_SLOT, d, 2)
         || !openxr_slot_acquire(&xr->sc, OPENXR_PROJECTION_SLOT, index))
      return false;
   tex[0] = xr->images[OPENXR_PROJECTION_SLOT][index[0]][0];
   tex[1] = xr->images[OPENXR_PROJECTION_SLOT][index[1]][1];
   *dims  = d;
   /* otherwise washed out picture */
   if (xr->srgb_copy)
      glDisable(OPENXR_GLES_FRAMEBUFFER_SRGB);
   return true;
}

/* With the poses the core was given, which it drew them with. */
void gl_android_openxr_eyes_drawn(unsigned width, unsigned height)
{
   openxr_projection_t proj;
   gl_android_openxr_t *xr = &gl_xr;
   if (xr->srgb_copy)
      glEnable(OPENXR_GLES_FRAMEBUFFER_SRGB);
   /* The compositor reads the images on its own context. */
   glFlush();
   openxr_slot_release(&xr->sc, OPENXR_PROJECTION_SLOT);
   openxr_host_projection(&xr->host, width, height, &proj);
   if (proj.active)
   {
      openxr_frame_publish_projection(&xr->frame, &proj);
      xr->proj_shown = true;
   }
}

void gl_android_openxr_new_frame(void)
{
   openxr_host_new_frame(&gl_xr.host);
}

void gl_android_openxr_place_kept(void)
{
   openxr_host_place_kept(&gl_xr.host);
}

bool gl_android_openxr_repeat_eyes(void)
{
   return !gl_xr.proj_shown && openxr_host_has_given(&gl_xr.host);
}

/* gl2's frame, with the video context current, once a frame. */
bool gl_android_openxr_begin_frame(void)
{
   gl_android_openxr_t *xr = &gl_xr;
   if (!xr->session.session || xr->in_frame)
      return xr->begun.begun;
   /* The frame keeps what openxr_host samples from. */
   openxr_frame_begin(&xr->frame, &xr->begun);
   xr->in_frame = true;
   return xr->begun.begun;
}

/* ===================== gfx_ctx_driver_t entry points ===================== */

static void gl_android_openxr_ctx_destroy(void *data);

static void *gl_android_openxr_ctx_init(void *video_driver)
{
   gl_android_openxr_t *xr = &gl_xr;

   memset(xr, 0, sizeof(*xr));
   xr->egl_display    = EGL_NO_DISPLAY;
   xr->egl_context    = EGL_NO_CONTEXT;
   xr->egl_hw_context = EGL_NO_CONTEXT;
   xr->egl_surface    = EGL_NO_SURFACE;

   if (!(xr->android_app = (struct android_app*)g_android))
      return NULL;
   /* glcore draws nothing this context shows yet: the flat context. */
   if (string_is_equal(config_get_ptr()->arrays.video_driver, "glcore"))
   {
      openxr_host_forget();
      return NULL;
   }
   if (     !gl_xr_egl_init(xr)
         || !gl_xr_start(xr)
         || !gl_xr_window_create(xr))
   {
      gl_android_openxr_ctx_destroy(xr);
      openxr_host_forget();
      return NULL;
   }
   gl_android_openxr_host_register(xr);
   return xr;
}

static void gl_android_openxr_ctx_destroy(void *data)
{
   gl_android_openxr_t *xr = (gl_android_openxr_t*)data;
   if (!xr)
      return;
   /* While the session still shows what the core drew. */
   if (xr->session.session)
      openxr_host_keep(&xr->host);
   retro_atomic_store_release_int(&xr->stereo, 0);
   video_driver_set_xr_host(NULL);
   /* GL objects first, while the context is still current. */
   if (xr->egl_display != EGL_NO_DISPLAY && xr->egl_context != EGL_NO_CONTEXT)
      eglMakeCurrent(xr->egl_display, xr->egl_surface, xr->egl_surface,
            xr->egl_context);
   gl_xr_window_free(xr);
   gl_xr_stop(xr);
   gl_xr_egl_deinit(xr);
   memset(xr, 0, sizeof(*xr));
}

static enum gfx_ctx_api gl_android_openxr_ctx_get_api(void *data)
{
   return GFX_CTX_OPENGL_ES_API;
}

static bool gl_android_openxr_ctx_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   return api == GFX_CTX_OPENGL_ES_API;
}

static void gl_android_openxr_ctx_get_video_size(void *data, unsigned *dims)
{
   gl_android_openxr_t *xr = (gl_android_openxr_t*)data;
   *dims = xr ? xr->dims : 0;
}

static bool gl_android_openxr_ctx_set_video_mode(void *data,
      unsigned dims, bool fullscreen)
{
   return true;
}

static bool gl_android_openxr_ctx_set_resize(void *data, unsigned dims)
{
   return true;
}

static void gl_android_openxr_ctx_set_flags(void *data, uint32_t flags)
{
}

static void gl_android_openxr_ctx_check_window(void *data,
      bool *quit, bool *resize, unsigned *dims)
{
   gl_android_openxr_t *xr = (gl_android_openxr_t*)data;
   if (!xr)
      return;
   *quit   = xr->ended;
   *resize = (*dims != xr->dims);
   *dims   = xr->dims;
}

/* gl2 swaps once a frame, then again for BFI and replays: only the
 * first ends the headset's frame. */
static void gl_android_openxr_ctx_swap_buffers(void *data)
{
   gl_android_openxr_t *xr = (gl_android_openxr_t*)data;
   if (!xr || !xr->in_frame)
      return;
   xr->in_frame = false;
   if (xr->begun.begun && xr->begun.render)
      gl_xr_draw(xr);
   openxr_frame_end(&xr->frame, &xr->begun);
   if (xr->interval > 1)
      openxr_frame_repeat(&xr->frame, xr->interval - 1);
}

static bool gl_android_openxr_ctx_has_focus(void *data)
{
   gl_android_openxr_t *xr = (gl_android_openxr_t*)data;
   return xr && openxr_session_focused(&xr->session);
}

static bool gl_android_openxr_ctx_suppress_screensaver(void *data, bool enable)
{
   return false;
}

static void gl_android_openxr_ctx_input_driver(void *data,
      const char *joypad_name)
{
   input_driver_video_window(INPUT_WINDOW_PLATFORM, NULL);
}

static gfx_ctx_proc_t gl_android_openxr_ctx_get_proc_address(const char *symbol)
{
   return (gfx_ctx_proc_t)eglGetProcAddress(symbol);
}

static void gl_android_openxr_ctx_show_mouse(void *data, bool state) { }

/* gl2 picks its shader backend from these; without GLSL it would
 * report "no supported shader backend". */
static uint32_t gl_android_openxr_ctx_get_flags(void *data)
{
   uint32_t flags          = 0;
   gl_android_openxr_t *xr = (gl_android_openxr_t*)data;
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);
   BIT32_SET(flags, GFX_CTX_FLAGS_HEADSET_INLINE);
   /* gl2 draws the frame whole in the window, so touch, overlays and
    * the UI follow it; the headset shows a tracked map's eyes from the
    * frame's views, and an untracked map's screens from the window. */
   if (xr && openxr_session_running(&xr->session))
   {
      BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS);
      BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS_FALLBACK);
   }
   return flags;
}

/* The private context is the only one */
static void gl_android_openxr_ctx_bind_hw_render(void *data, bool enable)
{
   gl_android_openxr_t *xr = (gl_android_openxr_t *)data;
   EGLBoolean ret;

   if (!xr || xr->egl_display == EGL_NO_DISPLAY)
      return;

   if (enable)
      ret = eglMakeCurrent(
            xr->egl_display,
            EGL_NO_SURFACE,
            EGL_NO_SURFACE,
            xr->egl_hw_context);
   else
      ret = eglMakeCurrent(
            xr->egl_display,
            xr->egl_surface,
            xr->egl_surface,
            xr->egl_context);

   if (!ret)
      RARCH_ERR("[XR] eglMakeCurrent(%s) failed: EGL error 0x%x\n",
            enable ? "HW offscreen" : "OpenXR video",
            eglGetError());
}

/* Called by gl2 (threaded video) to take the context on this thread. */
static void gl_android_openxr_ctx_make_current(bool release)
{
   gl_android_openxr_t *xr = &gl_xr;
   EGLBoolean ret;

   if (xr->egl_display == EGL_NO_DISPLAY)
      return;

   if (release)
      ret = eglMakeCurrent(xr->egl_display,
            EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   else
      ret = eglMakeCurrent(xr->egl_display,
            xr->egl_surface, xr->egl_surface,
            xr->egl_context);

   if (!ret)
      RARCH_ERR("[XR] eglMakeCurrent(%s) failed: EGL error 0x%x\n",
            release ? "release" : "driver",
            eglGetError());
}

static void gl_android_openxr_ctx_release_current(void *data)
{
   gl_android_openxr_t *xr = (gl_android_openxr_t *)data;

   if (!xr || xr->egl_display == EGL_NO_DISPLAY)
      return;

   eglMakeCurrent(
         xr->egl_display,
         EGL_NO_SURFACE,
         EGL_NO_SURFACE,
         EGL_NO_CONTEXT);
}

const gfx_ctx_driver_t gfx_ctx_gl_android_openxr = {
   gl_android_openxr_ctx_init,
   gl_android_openxr_ctx_destroy,
   gl_android_openxr_ctx_get_api,
   gl_android_openxr_ctx_bind_api,
   NULL, /* swap_interval: no vsync under the XR compositor */
   gl_android_openxr_ctx_set_video_mode,
   gl_android_openxr_ctx_get_video_size,
   NULL, /* get_refresh_rate */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL, /* translate_aspect */
   NULL, /* update_window_title */
   gl_android_openxr_ctx_check_window,
   gl_android_openxr_ctx_set_resize,
   gl_android_openxr_ctx_has_focus,
   gl_android_openxr_ctx_suppress_screensaver,
   true, /* has_windowed: XR presentation, no Android window */
   gl_android_openxr_ctx_swap_buffers,
   gl_android_openxr_ctx_input_driver,
   gl_android_openxr_ctx_get_proc_address,
   NULL, /* image_buffer_init */
   NULL, /* image_buffer_write */
   gl_android_openxr_ctx_show_mouse,
   "gl_android_openxr",
   gl_android_openxr_ctx_get_flags,
   gl_android_openxr_ctx_set_flags,
   gl_android_openxr_ctx_bind_hw_render,
   NULL, /* get_context_data */
   gl_android_openxr_ctx_make_current,
   NULL, /* create_surface */
   NULL, /* destroy_surface */
   NULL,  /* presentable */
   NULL, /* last_present_time */
   gl_android_openxr_ctx_release_current
};
