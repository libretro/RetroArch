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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <retro_timers.h>
#include <string/stdstring.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <glsym/glsym.h>

#include <jni.h>
#include <android/native_activity.h>
#include <android/keycodes.h>
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
#include "../../input/drivers/openxr_input.h"
#include "../video_driver.h"

#include "gl_android_openxr.h"
#include "openxr_tracking.h"

#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x00000040
#endif

#define XR_GL_RGBA8        0x8058
#define XR_GL_SRGB8_ALPHA8 0x8C43

#define XR_FLAT_QUAD_DISTANCE 1.5f
#define XR_FLAT_QUAD_WIDTH    1.5f

typedef struct gl_android_openxr
{
   XrInstance instance;
   XrSystemId system;
   XrSession session;
   XrSessionState session_state;
   bool running;
   bool focused;

   XrSpace stage_space;
   XrSpace view_space;

   openxr_tracking_t tracking;
   XrDuration predicted_period;
   uint32_t frame_flags;
   enum retro_vr_reference_space ref_space;

   int32_t eye_width, eye_height;
   int32_t swap_width;
   XrSwapchain swapchain;
   int64_t swapchain_format;
   uint32_t image_count;
   XrSwapchainImageOpenGLESKHR *images;

   XrView views[2];
   bool views_valid;

   XrTime predicted_display_time;
   bool frame_began;
   bool should_render;
   bool image_acquired;
   uint32_t image_index;

   GLuint fbo;

   EGLDisplay egl_display;
   EGLConfig egl_config;
   EGLSurface egl_surface;

   EGLContext egl_context;       /* OpenXR / video-thread context */
   EGLContext egl_hw_context;    /* hardware-core / main-thread context */

   struct android_app *android_app;
} gl_android_openxr_t;

static gl_android_openxr_t gl_xr;
static bool gl_xr_stereo;

/* ===================== EGL ===================== */

static bool gl_xr_egl_init(gl_android_openxr_t *xr)
{
   EGLint num_configs = 0;
   EGLint major = 0, minor = 0;
   EGLint selected_version = 0;
   int pass;

   static const EGLint pbuffer_attribs[] = {
      EGL_WIDTH, 16,
      EGL_HEIGHT, 16,
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

   /* Prefer an ES3 context, fall back to ES2. */
   for (pass = 0; pass < 2; pass++)
   {
      EGLint version       = pass == 0 ? 3 : 2;
      EGLint renderable    = pass == 0
         ? EGL_OPENGL_ES3_BIT_KHR : EGL_OPENGL_ES2_BIT;
      EGLint cfg_attribs[] = {
         EGL_RED_SIZE,        8,
         EGL_GREEN_SIZE,      8,
         EGL_BLUE_SIZE,       8,
         EGL_ALPHA_SIZE,     8,
         EGL_DEPTH_SIZE,      0,
         EGL_STENCIL_SIZE,    0,
         EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT,
         EGL_RENDERABLE_TYPE, renderable,
         EGL_NONE
      };
      EGLint ctx_attribs[] = {
         EGL_CONTEXT_CLIENT_VERSION, version,
         EGL_NONE
      };

      if (!eglChooseConfig(xr->egl_display, cfg_attribs,
               &xr->egl_config, 1, &num_configs)
            || num_configs < 1)
         continue;

      xr->egl_context = eglCreateContext(
            xr->egl_display,
            xr->egl_config,
            EGL_NO_CONTEXT,
            ctx_attribs);

      if (xr->egl_context != EGL_NO_CONTEXT)
      {
         selected_version = version;

         RARCH_DBG("[XR] Created EGL %d.%d, OpenGL ES %d context.\n",
               major, minor, version);
         break;
      }
   }

   if (xr->egl_context == EGL_NO_CONTEXT)
   {
      RARCH_ERR("[XR] Failed to create an OpenGL ES context.\n");
      return false;
   }

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
   {
      EGLint hw_ctx_attribs[] = {
         EGL_CONTEXT_CLIENT_VERSION, selected_version,
         EGL_NONE
      };

      xr->egl_hw_context = eglCreateContext(
            xr->egl_display,
            xr->egl_config,
            xr->egl_context,
            hw_ctx_attribs);

      if (xr->egl_hw_context == EGL_NO_CONTEXT)
      {
         RARCH_ERR("[XR] eglCreateContext(hw) failed: EGL error 0x%x\n",
               eglGetError());
         return false;
      }
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

/* ===================== OpenXR frame loop ===================== */

bool gl_android_openxr_active(void)
{
   return gl_xr.session != XR_NULL_HANDLE;
}

bool gl_android_openxr_is_session_ready(void)
{
   return gl_xr.session != XR_NULL_HANDLE && gl_xr.images != NULL;
}

uint32_t gl_android_openxr_get_framebuffer(void)
{
   if (!gl_xr.image_acquired)
      return 0;
   return (uint32_t)gl_xr.fbo;
}

bool gl_android_openxr_begin_frame(void)
{
   gl_android_openxr_t *xr      = &gl_xr;
   XrFrameWaitInfo wait_info    = { XR_TYPE_FRAME_WAIT_INFO };
   XrFrameBeginInfo begin_info  = { XR_TYPE_FRAME_BEGIN_INFO };
   XrFrameState frame_state     = { XR_TYPE_FRAME_STATE };
   XrViewLocateInfo locate_info = { XR_TYPE_VIEW_LOCATE_INFO };
   XrViewState view_state       = { XR_TYPE_VIEW_STATE };
   XrSwapchainImageAcquireInfo acquire = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
   XrSwapchainImageWaitInfo wait_img   = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
   uint32_t view_count          = 0;
   XrResult result;

   if (xr->session == XR_NULL_HANDLE)
      return false;

   /* Already bracketed this frame (swap_buffers has not run yet). */
   if (xr->frame_began)
      return xr->image_acquired;

   xr->views_valid    = false;
   xr->image_acquired = false;

   if (!xr->running)
   {
      /* no sync on this frame: what the last one read is not held */
      openxr_input_idle();
      return false;
   }

   result = xrWaitFrame(xr->session, &wait_info, &frame_state);

   /* The controllers are read once the wait for the frame is over, not
    * before it. The wait takes up what is left of the frame's period,
    * and the frontend polls straight after this frame is handed in: read
    * before the wait, what that poll gets is most of a period old; read
    * after it, it is as old as the frame took to draw. Read whether or
    * not the wait went well, as it was before. */
   openxr_input_sync(xr->session);

   if (result != XR_SUCCESS)
   {
      RARCH_ERR("[XR] xrWaitFrame failed (%d).\n", result);
      return false;
   }

   xr->predicted_display_time = frame_state.predictedDisplayTime;
   xr->predicted_period = frame_state.predictedDisplayPeriod;
   xr->should_render          = frame_state.shouldRender != 0;

   if ((result = xrBeginFrame(xr->session, &begin_info)) != XR_SUCCESS)
   {
      RARCH_ERR("[XR] xrBeginFrame failed (%d).\n", result);
      return false;
   }

   xr->frame_began = true;

   /* Poses are only informational in flat mode (the quad is
    * head-locked); a failed locate just leaves views_valid false. */
   locate_info.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
   locate_info.displayTime           = xr->predicted_display_time;
   locate_info.space                 = xr->stage_space;
   xr->views[0].type = XR_TYPE_VIEW;
   xr->views[1].type = XR_TYPE_VIEW;
   if (    xr->should_render
        && xrLocateViews(xr->session, &locate_info, &view_state,
              2, &view_count, xr->views) == XR_SUCCESS
        && view_count == 2
        && (view_state.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT)
        && (view_state.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT))
      xr->views_valid = true;

   /* Acquire even when shouldRender is false so the FBO always has a
    * valid attachment; the layer is simply not submitted. */
   if (xrAcquireSwapchainImage(xr->swapchain, &acquire, &xr->image_index)
         != XR_SUCCESS)
   {
      RARCH_ERR("[XR] xrAcquireSwapchainImage failed.\n");
      return false;
   }

   wait_img.timeout = XR_INFINITE_DURATION;
   if (xrWaitSwapchainImage(xr->swapchain, &wait_img) != XR_SUCCESS)
   {
      XrSwapchainImageReleaseInfo release = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
      RARCH_ERR("[XR] xrWaitSwapchainImage failed.\n");
      xrReleaseSwapchainImage(xr->swapchain, &release);
      return false;
   }

   if (!xr->fbo)
      glGenFramebuffers(1, &xr->fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, xr->fbo);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
         GL_TEXTURE_2D, xr->images[xr->image_index].image, 0);
   if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
   {
      XrSwapchainImageReleaseInfo release = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
      RARCH_ERR("[XR] Swapchain FBO is incomplete.\n");
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      xrReleaseSwapchainImage(xr->swapchain, &release);
      return false;
   }

   glViewport(0, 0, xr->swap_width, xr->eye_height);
   glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
   glClear(GL_COLOR_BUFFER_BIT);

   xr->image_acquired = true;
   return true;
}

static void gl_xr_end_frame(void)
{
   gl_android_openxr_t *xr   = &gl_xr;
   XrFrameEndInfo end_info   = { XR_TYPE_FRAME_END_INFO };
   XrCompositionLayerQuad quad = { XR_TYPE_COMPOSITION_LAYER_QUAD };
   XrCompositionLayerProjection proj = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
   XrCompositionLayerProjectionView pv[2];
   const XrCompositionLayerBaseHeader *layers[1];
   const XrView *vsrc = NULL;
   XrResult result;
   bool submit;
   bool menu_is_alive = false;
   bool stereo;
   int eye;

   if (!xr->frame_began)
      return;
   xr->frame_began = false;

   submit = xr->image_acquired && xr->should_render;

#ifdef HAVE_MENU
   menu_is_alive = (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif

   stereo = gl_xr_stereo &&
              xr->swap_width >= xr->eye_width * 2 &&
              !menu_is_alive;

   if (xr->image_acquired)
   {
      XrSwapchainImageReleaseInfo release = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };

      /* The runtime consumes the image on the compositor's context;
       * make sure our commands are in flight first. */
      glFlush();
      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      if ((result = xrReleaseSwapchainImage(xr->swapchain, &release))
            != XR_SUCCESS)
         RARCH_ERR("[XR] xrReleaseSwapchainImage failed (%d).\n", result);
      xr->image_acquired = false;
   }

   end_info.displayTime          = xr->predicted_display_time;
   end_info.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
   end_info.layerCount           = 0;
   end_info.layers               = NULL;

   if (stereo)
   {
      /* The exact poses/FOV the core rendered with (sampled before retro_run) */
      if (xr->tracking.valid)
         vsrc = xr->tracking.views;
      else if (xr->views_valid)
         vsrc = xr->views;
      submit = submit && vsrc;
   }

   if (submit && stereo)
   {
      memset(pv, 0, sizeof(pv));
      for (eye = 0; eye < 2; eye++)
      {
         pv[eye].type                          = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
         pv[eye].pose                          = vsrc[eye].pose;
         pv[eye].fov                           = vsrc[eye].fov;
         pv[eye].subImage.swapchain            = xr->swapchain;
         pv[eye].subImage.imageRect.offset.x   = eye * xr->eye_width;
         pv[eye].subImage.imageRect.offset.y   = 0;
         pv[eye].subImage.imageRect.extent.width  = xr->eye_width;
         pv[eye].subImage.imageRect.extent.height = xr->eye_height;
         pv[eye].subImage.imageArrayIndex      = 0;
      }
      proj.layerFlags = 0;
      proj.space      = xr->stage_space;  /* same space tracking was located in */
      proj.viewCount  = 2;
      proj.views      = pv;
      layers[0]           = (const XrCompositionLayerBaseHeader*)&proj;
      end_info.layerCount = 1;
      end_info.layers     = layers;
   }
   else if (submit)
   {
      quad.layerFlags                       = 0;
      quad.space                            = xr->view_space;
      quad.eyeVisibility                    = XR_EYE_VISIBILITY_BOTH;
      quad.subImage.swapchain               = xr->swapchain;
      quad.subImage.imageRect.offset.x      = 0;
      quad.subImage.imageRect.offset.y      = 0;
      quad.subImage.imageArrayIndex         = 0;
      quad.pose.orientation.w               = 1.0f;
      quad.pose.position.z                  = -XR_FLAT_QUAD_DISTANCE;

      if (menu_is_alive)
      {
         /* Render the menu in a square aspect ratio so it is not stretched. */
         quad.subImage.imageRect.extent.width  = xr->eye_width;
         quad.subImage.imageRect.extent.height = xr->eye_height;

         quad.size.width  = XR_FLAT_QUAD_WIDTH;
         quad.size.height = XR_FLAT_QUAD_WIDTH
            * (float)xr->eye_height / (float)xr->eye_width;
      }
      else
      {
         quad.subImage.imageRect.extent.width  = xr->swap_width;
         quad.subImage.imageRect.extent.height = xr->eye_height;

         quad.size.width  = XR_FLAT_QUAD_WIDTH;
         quad.size.height = XR_FLAT_QUAD_WIDTH
            * (float)xr->eye_height / (float)xr->swap_width;
      }

      layers[0]           = (const XrCompositionLayerBaseHeader*)&quad;
      end_info.layerCount = 1;
      end_info.layers     = layers;
   }

   if ((result = xrEndFrame(xr->session, &end_info)) != XR_SUCCESS)
      RARCH_ERR("[XR] xrEndFrame failed (%d).\n", result);
}

bool gl_android_openxr_sample_tracking(void)
{
   gl_android_openxr_t *xr = &gl_xr;
   xr->tracking.valid      = false;
   if (!xr->running || xr->predicted_display_time <= 0)
      return false;
   return openxr_tracking_sample(&xr->tracking, xr->session,
         xr->stage_space, xr->view_space,
         xr->predicted_display_time + xr->predicted_period);
}

bool gl_android_openxr_get_head_pose(struct retro_vr_head_pose *out)
{
   if (!gl_xr.tracking.valid)
      return false;
   *out = gl_xr.tracking.head;
   return true;
}

bool gl_android_openxr_get_eye_state(struct retro_vr_eye_state out[2])
{
   gl_android_openxr_t *xr = &gl_xr;
   const XrView *src;
   int eye;
   if (xr->tracking.valid)
      src = xr->tracking.views;
   else if (xr->views_valid)
      src = xr->views;
   else
      return false;
   for (eye = 0; eye < 2; eye++)
      openxr_view_to_eye(&src[eye], &out[eye]);
   return true;
}

bool gl_android_openxr_get_eye_size(unsigned *w, unsigned *h)
{
   if (gl_xr.eye_width <= 0 || gl_xr.eye_height <= 0)
      return false;
   *w = (unsigned)gl_xr.eye_width;
   *h = (unsigned)gl_xr.eye_height;
   return true;
}

bool gl_android_openxr_set_reference_space(enum retro_vr_reference_space *space)
{
   if (!openxr_reference_space_set(gl_xr.session, &gl_xr.stage_space, space))
      return false;
   gl_xr.ref_space = *space;
   return true;
}

uint32_t gl_android_openxr_take_frame_flags(void)
{
   uint32_t f         = gl_xr.frame_flags;
   gl_xr.frame_flags  = 0;
   return f;
}

/* ===================== setup / teardown ===================== */

static void gl_xr_destroy_swapchain(gl_android_openxr_t *xr)
{
   if (xr->fbo)
   {
      glDeleteFramebuffers(1, &xr->fbo);
      xr->fbo = 0;
   }
   if (xr->swapchain != XR_NULL_HANDLE)
      xrDestroySwapchain(xr->swapchain);
   xr->swapchain = XR_NULL_HANDLE;
   free(xr->images);
   xr->images      = NULL;
   xr->image_count = 0;
}

static bool gl_xr_init_swapchain(gl_android_openxr_t *xr)
{
   uint32_t i, format_count = 0, view_count = 0;
   int64_t *formats         = NULL;
   XrSwapchainCreateInfo info = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
   XrViewConfigurationView views[2] =
   {
      { XR_TYPE_VIEW_CONFIGURATION_VIEW },
      { XR_TYPE_VIEW_CONFIGURATION_VIEW }
   };

   if (xrEnumerateViewConfigurationViews(xr->instance, xr->system,
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2,
            &view_count, views) != XR_SUCCESS || view_count != 2)
   {
      RARCH_ERR("[XR] Expected 2 views in primary stereo config.\n");
      return false;
   }

   xr->eye_width  = (int32_t)views[0].recommendedImageRectWidth;
   xr->eye_height = (int32_t)views[0].recommendedImageRectHeight;
   xr->swap_width = gl_xr_stereo ? xr->eye_width * 2 : xr->eye_width;

   xrEnumerateSwapchainFormats(xr->session, 0, &format_count, NULL);
   if (!format_count)
      return false;
   if (!(formats = (int64_t*)malloc(sizeof(*formats) * format_count)))
      return false;
   xrEnumerateSwapchainFormats(xr->session, format_count,
         &format_count, formats);

   /* gl2 writes display-referred values, so a plain UNORM target is
    * the correct one; an sRGB target would double-encode unless
    * GL_FRAMEBUFFER_SRGB were enabled, which GLES does not offer. */
   xr->swapchain_format = 0;
   for (i = 0; i < format_count; i++)
      if (formats[i] == XR_GL_RGBA8)
      {
         xr->swapchain_format = formats[i];
         break;
      }
   if (!xr->swapchain_format)
      for (i = 0; i < format_count; i++)
         if (formats[i] == XR_GL_SRGB8_ALPHA8)
         {
            xr->swapchain_format = formats[i];
            RARCH_WARN("[XR] Only an sRGB swapchain format offered; "
                  "output may look washed out.\n");
            break;
         }
   free(formats);

   if (!xr->swapchain_format)
   {
      RARCH_ERR("[XR] No suitable OpenXR swapchain format found.\n");
      return false;
   }

   info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT
                   | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
   info.format      = xr->swapchain_format;
   info.sampleCount = 1;
   info.width       = (uint32_t)xr->eye_width;
   info.height      = (uint32_t)xr->eye_height;
   info.faceCount   = 1;
   info.arraySize   = 1;
   info.mipCount    = 1;
   info.width     = (uint32_t)xr->swap_width;

   if (xrCreateSwapchain(xr->session, &info, &xr->swapchain) != XR_SUCCESS)
   {
      RARCH_ERR("[XR] xrCreateSwapchain failed.\n");
      return false;
   }

   xrEnumerateSwapchainImages(xr->swapchain, 0, &xr->image_count, NULL);
   if (!xr->image_count)
      return false;
   if (!(xr->images = (XrSwapchainImageOpenGLESKHR*)
            calloc(xr->image_count, sizeof(*xr->images))))
      return false;
   for (i = 0; i < xr->image_count; i++)
      xr->images[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
   xrEnumerateSwapchainImages(xr->swapchain, xr->image_count,
         &xr->image_count, (XrSwapchainImageBaseHeader*)xr->images);

   return true;
}

static bool gl_xr_create_instance(gl_android_openxr_t *xr,
      struct android_app *android_app)
{
   PFN_xrInitializeLoaderKHR init_loader = NULL;
   XrLoaderInitInfoAndroidKHR loader_info =
      { XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR };
   XrInstanceCreateInfoAndroidKHR android_info =
      { XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR };
   XrInstanceCreateInfo create_info = { XR_TYPE_INSTANCE_CREATE_INFO };
   const char *extensions[] = { XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME };
   XrSystemGetInfo system_info = { XR_TYPE_SYSTEM_GET_INFO };

   if (xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
            (PFN_xrVoidFunction*)&init_loader) != XR_SUCCESS || !init_loader)
   {
      RARCH_ERR("[XR] xrInitializeLoaderKHR unavailable.\n");
      return false;
   }

   loader_info.applicationVM      = android_app->activity->vm;
   loader_info.applicationContext = android_app->activity->clazz;
   if (init_loader((XrLoaderInitInfoBaseHeaderKHR*)&loader_info) != XR_SUCCESS)
      return false;

   android_info.applicationVM       = android_app->activity->vm;
   android_info.applicationActivity = android_app->activity->clazz;

   create_info.next = &android_info;
   strlcpy(create_info.applicationInfo.applicationName, "RetroArch",
         sizeof(create_info.applicationInfo.applicationName));
   create_info.applicationInfo.apiVersion = XR_API_VERSION_1_0;
   create_info.enabledExtensionCount      = 1;
   create_info.enabledExtensionNames      = extensions;

   if (xrCreateInstance(&create_info, &xr->instance) != XR_SUCCESS)
   {
      RARCH_ERR("[XR] xrCreateInstance failed.\n");
      return false;
   }

   system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
   if (xrGetSystem(xr->instance, &system_info, &xr->system) != XR_SUCCESS)
   {
      RARCH_ERR("[XR] xrGetSystem failed (no HMD?).\n");
      return false;
   }
   return true;
}

/* Binds an XR session to the already-current EGL context via
 * XR_KHR_opengl_es_enable. */
static bool gl_xr_create_session(gl_android_openxr_t *xr)
{
   PFN_xrGetOpenGLESGraphicsRequirementsKHR get_reqs = NULL;
   XrGraphicsRequirementsOpenGLESKHR reqs =
      { XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR };
   XrGraphicsBindingOpenGLESAndroidKHR binding =
      { XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR };
   XrSessionCreateInfo session_info      = { XR_TYPE_SESSION_CREATE_INFO };
   XrReferenceSpaceCreateInfo space_info = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
   XrResult result;

   if (xrGetInstanceProcAddr(xr->instance,
            "xrGetOpenGLESGraphicsRequirementsKHR",
            (PFN_xrVoidFunction*)&get_reqs) != XR_SUCCESS || !get_reqs)
   {
      RARCH_ERR("[XR] xrGetOpenGLESGraphicsRequirementsKHR unavailable.\n");
      return false;
   }

   /* Mandatory before xrCreateSession, even though the answer is only
    * logged here. */
   if ((result = get_reqs(xr->instance, xr->system, &reqs)) != XR_SUCCESS)
   {
      RARCH_ERR("[XR] xrGetOpenGLESGraphicsRequirementsKHR failed (%d).\n",
            result);
      return false;
   }

   binding.display = xr->egl_display;
   binding.config  = xr->egl_config;
   binding.context = xr->egl_context;

   session_info.next     = &binding;
   session_info.systemId = xr->system;

   if ((result = xrCreateSession(xr->instance, &session_info, &xr->session))
         != XR_SUCCESS)
   {
      RARCH_ERR("[XR] xrCreateSession failed (%d).\n", result);
      return false;
   }

   space_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
   space_info.poseInReferenceSpace.orientation.w = 1.0f;
   if (xrCreateReferenceSpace(xr->session, &space_info, &xr->stage_space)
         != XR_SUCCESS)
   {
      space_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
      if (xrCreateReferenceSpace(xr->session, &space_info, &xr->stage_space)
            != XR_SUCCESS)
      {
         RARCH_ERR("[XR] xrCreateReferenceSpace failed.\n");
         return false;
      }
   }

   space_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
   if (xrCreateReferenceSpace(xr->session, &space_info, &xr->view_space)
         != XR_SUCCESS)
   {
      RARCH_ERR("[XR] Failed to create view space.\n");
      return false;
   }

   xr->ref_space = (space_info.referenceSpaceType == XR_REFERENCE_SPACE_TYPE_STAGE)
      ? RETRO_VR_REFERENCE_SPACE_STAGE : RETRO_VR_REFERENCE_SPACE_LOCAL;

   if (!gl_xr_init_swapchain(xr))
      return false;

   return openxr_input_attach(xr->session);
}

static void gl_xr_poll_events(gl_android_openxr_t *xr, bool *quit)
{
   XrEventDataBuffer event = { XR_TYPE_EVENT_DATA_BUFFER };

   while (xrPollEvent(xr->instance, &event) == XR_SUCCESS)
   {
      switch (event.type)
      {
         case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
            *quit = true;
            break;
         case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
            xr->frame_flags |= RETRO_VR_FRAME_RECENTERED;
            break;
         case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
            {
               XrEventDataSessionStateChanged *e =
                  (XrEventDataSessionStateChanged*)&event;
               xr->session_state = e->state;
               switch (e->state)
               {
                  case XR_SESSION_STATE_READY:
                     {
                        XrSessionBeginInfo begin = { XR_TYPE_SESSION_BEGIN_INFO };
                        begin.primaryViewConfigurationType =
                           XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                        if (xrBeginSession(xr->session, &begin) == XR_SUCCESS)
                           xr->running = true;
                        else
                           RARCH_ERR("[XR] xrBeginSession failed.\n");
                     }
                     break;
                  case XR_SESSION_STATE_STOPPING:
                     xrEndSession(xr->session);
                     xr->running = false;
                     break;
                  case XR_SESSION_STATE_FOCUSED:
                     xr->focused = true;
                     break;
                  case XR_SESSION_STATE_VISIBLE:
                  case XR_SESSION_STATE_SYNCHRONIZED:
                     xr->focused = false;
                     break;
                  case XR_SESSION_STATE_EXITING:
                  case XR_SESSION_STATE_LOSS_PENDING:
                     *quit = true;
                     break;
                  default:
                     break;
               }
            }
            break;
         default:
            break;
      }
      event.type = XR_TYPE_EVENT_DATA_BUFFER;
   }
}

void gl_android_openxr_set_stereo(bool on)
{
   gl_xr_stereo = on;
}

bool gl_android_openxr_stereo_active(void)
{
   return gl_xr_stereo;
}

/* ===================== gfx_ctx_driver_t entry points ===================== */

static void gl_android_openxr_ctx_destroy(void *data);

static void *gl_android_openxr_ctx_init(void *video_driver)
{
   gl_android_openxr_t *xr = &gl_xr;
   uint32_t count = 0, i;
   XrViewConfigurationType types[8];
   bool stereo_supported = false;

   memset(xr, 0, sizeof(*xr));
   xr->egl_display    = EGL_NO_DISPLAY;
   xr->egl_context    = EGL_NO_CONTEXT;
   xr->egl_hw_context = EGL_NO_CONTEXT;
   xr->egl_surface    = EGL_NO_SURFACE;

   if (!(xr->android_app = (struct android_app*)g_android))
      return NULL;

   if (!gl_xr_create_instance(xr, xr->android_app))
      goto error;

   if (!openxr_input_init(xr->instance))
   {
      RARCH_ERR("[XR] Touch controller input setup failed.\n");
      goto error;
   }

   xrEnumerateViewConfigurations(xr->instance, xr->system, 0, &count, NULL);
   if (count == 0 || count > 8)
      goto error;
   xrEnumerateViewConfigurations(xr->instance, xr->system, count, &count, types);
   for (i = 0; i < count; i++)
      if (types[i] == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO)
         stereo_supported = true;
   if (!stereo_supported)
   {
      RARCH_WARN("[XR] No PRIMARY_STEREO view configuration.\n");
      goto error;
   }

   if (!gl_xr_egl_init(xr))
      goto error;

   if (!gl_xr_create_session(xr))
      goto error;

   return xr;

error:
   gl_android_openxr_ctx_destroy(xr);
   return NULL;
}

static void gl_android_openxr_ctx_destroy(void *data)
{
   gl_android_openxr_t *xr = (gl_android_openxr_t*)data;
   if (!xr)
      return;

   /* GL objects first, while the context is still current. */
   if (xr->egl_display != EGL_NO_DISPLAY && xr->egl_context != EGL_NO_CONTEXT)
      eglMakeCurrent(xr->egl_display, xr->egl_surface, xr->egl_surface,
            xr->egl_context);
   gl_xr_destroy_swapchain(xr);

   if (xr->stage_space != XR_NULL_HANDLE)
      xrDestroySpace(xr->stage_space);
   if (xr->view_space != XR_NULL_HANDLE)
      xrDestroySpace(xr->view_space);
   if (xr->session != XR_NULL_HANDLE)
      xrDestroySession(xr->session);
   openxr_input_deinit();
   if (xr->instance != XR_NULL_HANDLE)
      xrDestroyInstance(xr->instance);

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
   *dims = (xr && xr->swap_width > 0)
      ? VIDEO_SCALE_PACK((unsigned)xr->swap_width, (unsigned)xr->eye_height)
      : 0;
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
   bool local_quit         = false;
   unsigned eye_dims       = 0;

   if (!xr)
      return;

   gl_xr_poll_events(xr, &local_quit);

   if (xr->swap_width > 0)
      eye_dims = VIDEO_SCALE_PACK((unsigned)xr->swap_width,
            (unsigned)xr->eye_height);

   *quit   = local_quit;
   *resize = (eye_dims && *dims != eye_dims);
   if (eye_dims)
      *dims = eye_dims;
}

/* The frame's xrEndFrame lives here: gl2 calls swap_buffers exactly
 * once per presented frame, which is the bracket's natural end. */
static void gl_android_openxr_ctx_swap_buffers(void *data)
{
   (void)data;
   gl_xr_end_frame();
}

static bool gl_android_openxr_ctx_has_focus(void *data)
{
   gl_android_openxr_t *xr = (gl_android_openxr_t*)data;
   return xr && xr->focused;
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
   uint32_t flags = 0;
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);
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
