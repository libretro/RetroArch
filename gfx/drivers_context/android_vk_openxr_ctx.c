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

/* Vulkan context driver that binds to an OpenXR session instead of an
 * Android ANativeWindow-backed VkSwapchainKHR.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <retro_timers.h>
#include <string/stdstring.h>

#include <android/native_activity.h>
#include <android/keycodes.h>
#define XR_USE_PLATFORM_ANDROID
#define XR_USE_GRAPHICS_API_VULKAN
#ifdef HAVE_OPENGLES
#define XR_USE_GRAPHICS_API_OPENGL_ES
#endif

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "../../frontend/drivers/platform_unix.h"
#include "../../verbosity.h"
#include "../common/vulkan_common.h"
#include "../video_driver.h"

#include "android_vk_openxr.h"
#include "openxr_tracking.h"
#include "../../input/drivers/openxr_input.h"

#define XR_FLAT_QUAD_DISTANCE 1.5f
#define XR_FLAT_QUAD_WIDTH    1.5f

typedef struct android_vk_openxr
{
   XrInstance instance;
   XrSystemId system;
   XrSession session;
   XrSessionState session_state;
   bool running;                 /* xrBeginSession succeeded, not yet ended */
   bool session_ready_to_exit;
   bool focused;

   XrSpace stage_space;
   XrActionSet input_action_set;
   XrAction input_actions[12];
   bool input_attached;
   bool input_buttons[8];
   int16_t input_axes[6];

   /* Per-eye view configuration (both eyes share resolution). */
   int32_t eye_width, eye_height;
   XrSwapchain eye_swapchain[2];
   uint32_t eye_image_count;      /* same for both eyes */
   XrSwapchainImageVulkanKHR *eye_images[2]; /* [eye_image_count] each */

   VkImage *combined_images;
   unsigned combined_count;

   VkFormat swapchain_format;
   unsigned swapchain_dims;       /* VIDEO_SCALE_PACK(eye_width, eye_height) */

   XrView views[2];
   bool views_valid;

   XrTime predicted_display_time;
   bool frame_began;
   bool should_render;

   gfx_ctx_vulkan_data_t vk;

   struct android_app *android_app;
   unsigned current_eye_image_index[2];

   XrSpace view_space;

   openxr_tracking_t tracking;
   bool tracking_consumed;       /* core fetched eye state this frame */
   XrDuration predicted_period;
   uint32_t frame_flags;         /* RETRO_VR_FRAME_* */
   enum retro_vr_reference_space ref_space;
} android_vk_openxr_t;

static android_vk_openxr_t android_vk_openxr_ctx;

static XrFrameState android_vk_openxr_frame_state;

bool android_vk_openxr_owns_vk_context(const void *vk_data)
{
   return vk_data == (const void*)&android_vk_openxr_ctx.vk;
}

XrInstance android_vk_openxr_xr_instance(void)
{
   return android_vk_openxr_ctx.instance;
}

XrSystemId android_vk_openxr_xr_system_id(void)
{
   return android_vk_openxr_ctx.system;
}

bool android_vk_openxr_begin_frame(void)
{
   android_vk_openxr_t *xr = &android_vk_openxr_ctx;
   XrFrameWaitInfo wait_info   = { XR_TYPE_FRAME_WAIT_INFO };
   XrFrameBeginInfo begin_info = { XR_TYPE_FRAME_BEGIN_INFO };
   XrViewLocateInfo locate_info = { XR_TYPE_VIEW_LOCATE_INFO };
   XrViewState view_state = { XR_TYPE_VIEW_STATE };
   uint32_t view_count = 0;
   int eye;

   /* Already bracketed this frame (end_frame has not run yet). */
   if (xr->frame_began)
      return xr->should_render;

   xr->frame_began = false;
   xr->views_valid = false;

   if (!xr->running)
   {
      RARCH_DBG("[XR] begin_frame: session is not running.\n");
      return false;
   }

   openxr_input_sync(xr->session);

   android_vk_openxr_frame_state.type = XR_TYPE_FRAME_STATE;

   {
      XrResult result = xrWaitFrame(
            xr->session,
            &wait_info,
            &android_vk_openxr_frame_state);

      if (result != XR_SUCCESS)
      {
         char result_str[XR_MAX_RESULT_STRING_SIZE];
         xrResultToString(xr->instance, result, result_str);

         RARCH_ERR("[XR] xrWaitFrame failed: %s (%d).\n",
               result_str, result);
         return false;
      }
   }

   xr->predicted_display_time =
      android_vk_openxr_frame_state.predictedDisplayTime;
   xr->predicted_period = android_vk_openxr_frame_state.predictedDisplayPeriod;
   xr->should_render =
      android_vk_openxr_frame_state.shouldRender;

   {
      XrResult result = xrBeginFrame(xr->session, &begin_info);

      if (result != XR_SUCCESS)
      {
         char result_str[XR_MAX_RESULT_STRING_SIZE];
         xrResultToString(xr->instance, result, result_str);

         RARCH_ERR("[XR] xrBeginFrame failed: %s (%d).\n",
            result_str, result);
         return false;
      }
   }

   xr->frame_began = true;

   if (!xr->should_render)
   {
      RARCH_DBG("[XR] shouldRender=false, sessionState=%d\n",
         xr->session_state);

      /* Still a valid frame - just nothing to submit. */
      xr->views_valid = false;
      return false;
   }

   locate_info.viewConfigurationType =
      XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
   locate_info.displayTime = xr->predicted_display_time;
   locate_info.space = xr->stage_space;

   xr->views[0].type = XR_TYPE_VIEW;
   xr->views[1].type = XR_TYPE_VIEW;

   {
      XrResult result = xrLocateViews(
            xr->session,
            &locate_info,
            &view_state,
            2,
            &view_count,
            xr->views);

      if (result != XR_SUCCESS)
      {
         char result_str[XR_MAX_RESULT_STRING_SIZE];
         xrResultToString(xr->instance, result, result_str);

         RARCH_ERR("[XR] xrLocateViews failed: %s (%d).\n",
               result_str, result);

         xr->views_valid = false;
         return false;
      }
   }

   if (view_count != 2)
   {
      RARCH_ERR("[XR] xrLocateViews returned %u views, expected 2.\n",
            view_count);

      xr->views_valid = false;
      return false;
   }

   struct menu_state *menu_st = menu_state_get_ptr();
   bool menu_active = menu_st->flags & MENU_ST_FLAG_ALIVE;
   bool position_valid = view_state.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT;
   bool orientation_valid = view_state.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT;

   if (!position_valid && !menu_active)
   {
      RARCH_ERR("[XR] XR_VIEW_STATE_POSITION_VALID_BIT is not set.\n");

      xr->views_valid = false;
      return false;
   }

   if (!orientation_valid && !menu_active)
   {
      RARCH_ERR("[XR] XR_VIEW_STATE_ORIENTATION_VALID_BIT is not set.\n");

      xr->views_valid = false;
      return false;
   }

   xr->views_valid = true;

   for (eye = 0; eye < 2; eye++)
   {
      XrSwapchainImageAcquireInfo acquire =
         { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
      XrSwapchainImageWaitInfo wait =
         { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
      uint32_t image_index = 0;

      {
         XrResult result = xrAcquireSwapchainImage(
               xr->eye_swapchain[eye],
               &acquire,
               &image_index);

         if (result != XR_SUCCESS)
         {
            char result_str[XR_MAX_RESULT_STRING_SIZE];
            xrResultToString(xr->instance, result, result_str);

            RARCH_ERR(
                  "[XR] xrAcquireSwapchainImage eye %d failed: %s (%d).\n",
                  eye, result_str, result);

            xr->views_valid = false;
            return false;
         }
      }

      wait.timeout = XR_INFINITE_DURATION;

      {
         XrResult result = xrWaitSwapchainImage(
               xr->eye_swapchain[eye],
               &wait);

         if (result != XR_SUCCESS)
         {
            char result_str[XR_MAX_RESULT_STRING_SIZE];
            xrResultToString(xr->instance, result, result_str);

            RARCH_ERR(
                  "[XR] xrWaitSwapchainImage eye %d failed: %s (%d).\n",
                  eye, result_str, result);

            xr->views_valid = false;
            return false;
         }
      }

      xr->current_eye_image_index[eye] = image_index;
   }

   return true;
}

void android_vk_openxr_end_frame(bool stereo_layer)
{
   android_vk_openxr_t *xr = &android_vk_openxr_ctx;
   XrFrameEndInfo end_info = { XR_TYPE_FRAME_END_INFO };
   XrCompositionLayerProjection projection =
      { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
   XrCompositionLayerQuad quad = { XR_TYPE_COMPOSITION_LAYER_QUAD };
   XrCompositionLayerProjectionView proj_views[2] =
   {
      { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW },
      { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW }
   };
   const XrCompositionLayerBaseHeader *layers[1];
   int eye;

   if (!xr->frame_began)
      return;

   xr->frame_began = false;
   bool menu_is_alive = false;

#ifdef HAVE_MENU
   menu_is_alive = (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE) != 0;
#endif

   end_info.displayTime          = xr->predicted_display_time;
   end_info.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
   end_info.layerCount           = 0;
   end_info.layers               = NULL;

   const XrView *src_views = (xr->tracking_consumed && xr->tracking.valid)
      ? xr->tracking.views : xr->views;

   if (xr->views_valid && xr->should_render)
   {
      for (eye = 0; eye < 2; eye++)
      {
         XrSwapchainImageReleaseInfo release =
            { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
         XrResult result;

         result = xrReleaseSwapchainImage(
               xr->eye_swapchain[eye], &release);

         if (result != XR_SUCCESS)
         {
            char result_str[XR_MAX_RESULT_STRING_SIZE];

            xrResultToString(xr->instance, result, result_str);

            RARCH_ERR(
                  "[XR] xrReleaseSwapchainImage eye %d failed: %s (%d).\n",
                  eye, result_str, result);
         }

         proj_views[eye].pose = src_views[eye].pose;
         proj_views[eye].fov  = src_views[eye].fov;

         proj_views[eye].subImage.swapchain =
            xr->eye_swapchain[eye];

         proj_views[eye].subImage.imageRect.offset.x = 0;
         proj_views[eye].subImage.imageRect.offset.y = 0;
         proj_views[eye].subImage.imageRect.extent.width =
            (uint32_t)xr->eye_width;
         proj_views[eye].subImage.imageRect.extent.height =
            (uint32_t)xr->eye_height;
         proj_views[eye].subImage.imageArrayIndex = 0;
      }

      projection.space     = xr->stage_space;
      projection.viewCount = 2;
      projection.views     = proj_views;

      if (stereo_layer && !menu_is_alive)
      {
         /* existing projection setup */
         layers[0] = (const XrCompositionLayerBaseHeader*)&projection;
      }
      else
      {
         quad.layerFlags                          = 0; /* ignore alpha */
         quad.space                               = xr->view_space;
         quad.eyeVisibility                       = XR_EYE_VISIBILITY_BOTH;
         quad.subImage.swapchain                  = xr->eye_swapchain[0];
         quad.subImage.imageRect.offset.x         = 0;
         quad.subImage.imageRect.offset.y         = 0;
         quad.subImage.imageRect.extent.width     = (uint32_t)xr->eye_width;
         quad.subImage.imageRect.extent.height    = (uint32_t)xr->eye_height;
         quad.subImage.imageArrayIndex            = 0;
         quad.pose.orientation.w                  = 1.0f;
         quad.pose.position.z                     = -XR_FLAT_QUAD_DISTANCE;
         quad.size.width                          = XR_FLAT_QUAD_WIDTH;
         quad.size.height                         = XR_FLAT_QUAD_WIDTH
               * (float)xr->eye_height / (float)xr->eye_width;
         layers[0] = (const XrCompositionLayerBaseHeader*)&quad;
      }
      end_info.layerCount = 1;
      end_info.layers     = layers;
   }

   {
      XrResult result = xrEndFrame(xr->session, &end_info);

      if (result != XR_SUCCESS)
      {
         char result_str[XR_MAX_RESULT_STRING_SIZE];

         xrResultToString(xr->instance, result, result_str);

         RARCH_ERR("[XR] xrEndFrame failed: %s (%d).\n",
               result_str, result);
      }
   }

   xr->tracking_consumed = false;
}

bool android_vk_openxr_sample_tracking(void)
{
   android_vk_openxr_t *xr = &android_vk_openxr_ctx;
   xr->tracking.valid      = false;
   xr->tracking_consumed   = false;
   if (!xr->running || xr->predicted_display_time <= 0)
      return false;
   /* When the frame about to be built will be displayed */
   return openxr_tracking_sample(&xr->tracking, xr->session,
         xr->stage_space, xr->view_space,
         xr->predicted_display_time + xr->predicted_period);
}

bool android_vk_openxr_get_head_pose(struct retro_vr_head_pose *out)
{
   android_vk_openxr_t *xr = &android_vk_openxr_ctx;
   if (!xr->tracking.valid)
      return false;
   *out = xr->tracking.head;
   return true;
}

bool android_vk_openxr_get_eye_state(struct retro_vr_eye_state out[2])
{
   android_vk_openxr_t *xr = &android_vk_openxr_ctx;
   const XrView *src;
   int eye;

   if (xr->tracking.valid)
   {
      src                   = xr->tracking.views;
      xr->tracking_consumed = true;
   }
   else if (xr->views_valid)
      src = xr->views;
   else
      return false;

   for (eye = 0; eye < 2; eye++)
      openxr_view_to_eye(&src[eye], &out[eye]);
   return true;
}

bool android_vk_openxr_get_eye_size(unsigned *w, unsigned *h)
{
   android_vk_openxr_t *xr = &android_vk_openxr_ctx;
   if (xr->eye_width <= 0 || xr->eye_height <= 0)
      return false;
   *w = (unsigned)xr->eye_width;
   *h = (unsigned)xr->eye_height;
   return true;
}

bool android_vk_openxr_set_reference_space(enum retro_vr_reference_space *space)
{
   android_vk_openxr_t *xr = &android_vk_openxr_ctx;
   if (!openxr_reference_space_set(xr->session, &xr->stage_space, space))
      return false;
   xr->ref_space = *space;
   return true;
}

uint32_t android_vk_openxr_take_frame_flags(void)
{
   android_vk_openxr_t *xr = &android_vk_openxr_ctx;
   uint32_t f              = xr->frame_flags;
   xr->frame_flags         = 0;
   return f;
}

unsigned android_vk_openxr_get_backbuffer_index(int eye)
{
   android_vk_openxr_t *xr = &android_vk_openxr_ctx;

   return xr->current_eye_image_index[eye] * 2 + (unsigned)eye;
}

static void android_vk_openxr_destroy_swapchains(android_vk_openxr_t *xr)
{
   int eye;
   for (eye = 0; eye < 2; eye++)
   {
      if (xr->eye_swapchain[eye] != XR_NULL_HANDLE)
         xrDestroySwapchain(xr->eye_swapchain[eye]);
      xr->eye_swapchain[eye] = XR_NULL_HANDLE;
      if (xr->eye_images[eye])
         free(xr->eye_images[eye]);
      xr->eye_images[eye] = NULL;
   }
   if (xr->combined_images)
      free(xr->combined_images);
   xr->combined_images = NULL;
   xr->combined_count  = 0;
}

static bool android_vk_openxr_init_swapchains(android_vk_openxr_t *xr,
      VkFormat preferred_format)
{
   uint32_t format_count = 0;
   int64_t *formats      = NULL;
   int64_t chosen_format = 0;
   uint32_t view_count   = 0;
   XrViewConfigurationView views[2] =
   {
      { XR_TYPE_VIEW_CONFIGURATION_VIEW },
      { XR_TYPE_VIEW_CONFIGURATION_VIEW }
   };
   int eye;

   if (xrEnumerateViewConfigurationViews(xr->instance, xr->system,
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0,
            &view_count, NULL) != XR_SUCCESS || view_count != 2)
   {
      RARCH_ERR("[XR] Expected 2 views in primary stereo config, got %u.\n",
            view_count);
      return false;
   }
   xrEnumerateViewConfigurationViews(xr->instance, xr->system,
         XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &view_count, views);

   xr->eye_width  = (int32_t)views[0].recommendedImageRectWidth;
   xr->eye_height = (int32_t)views[0].recommendedImageRectHeight;
   xr->swapchain_dims = VIDEO_SCALE_PACK((unsigned)xr->eye_width,
         (unsigned)xr->eye_height);

   xrEnumerateSwapchainFormats(xr->session, 0, &format_count, NULL);
   if (!format_count)
      return false;
   formats = (int64_t*)malloc(sizeof(*formats) * format_count);
   if (!formats)
      return false;
   xrEnumerateSwapchainFormats(xr->session, format_count, &format_count, formats);

   /* Prefer the format vulkan.c's swapchain init already picked for the
    * flat path (VK_FORMAT_B8G8R8A8_UNORM in the common case); fall back
    * to whatever the runtime offers first. */
   {
      uint32_t i;

      chosen_format = VK_FORMAT_UNDEFINED;

      for (i = 0; i < format_count; i++)
      {
         if ((VkFormat)formats[i] == preferred_format)
         {
            chosen_format = formats[i];
            break;
         }
      }

      /*
      * Quest/OpenXR does not necessarily expose the format used by
      * RetroArch's normal Vulkan surface. Prefer a 4-channel UNORM
      * format rather than blindly taking formats[0].
      */
      if (chosen_format == VK_FORMAT_UNDEFINED)
      {
         for (i = 0; i < format_count; i++)
         {
            if ((VkFormat)formats[i] == VK_FORMAT_R8G8B8A8_UNORM)
            {
               chosen_format = formats[i];
               break;
            }
         }
      }

      /*
      * If RGBA8 UNORM isn't available, try RGBA8 SRGB.
      */
      if (chosen_format == VK_FORMAT_UNDEFINED)
      {
         for (i = 0; i < format_count; i++)
         {
            if ((VkFormat)formats[i] == VK_FORMAT_R8G8B8A8_SRGB)
            {
               chosen_format = formats[i];

               RARCH_DBG("[XR] Swapchain format chosen from RGBA8 SRGB fallback: %d\n",
                     (VkFormat)chosen_format);
               break;
            }
         }
      }
   }

   free(formats);

   if (chosen_format == VK_FORMAT_UNDEFINED)
   {
      RARCH_ERR("[XR] No suitable OpenXR swapchain format found!\n");
      return false;
   }

   xr->swapchain_format = (VkFormat)chosen_format;

   for (eye = 0; eye < 2; eye++)
   {
      XrSwapchainCreateInfo info = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
      uint32_t image_count       = 0;

      info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT
                       | XR_SWAPCHAIN_USAGE_SAMPLED_BIT
                       | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
      info.format      = chosen_format;
      info.sampleCount  = 1;
      info.width        = (uint32_t)xr->eye_width;
      info.height       = (uint32_t)xr->eye_height;
      info.faceCount    = 1;
      info.arraySize    = 1;
      info.mipCount     = 1;

      if (xrCreateSwapchain(xr->session, &info, &xr->eye_swapchain[eye])
            != XR_SUCCESS)
      {
         RARCH_ERR("[XR] xrCreateSwapchain failed for eye %d.\n", eye);
         android_vk_openxr_destroy_swapchains(xr);
         return false;
      }

      xrEnumerateSwapchainImages(xr->eye_swapchain[eye], 0, &image_count, NULL);
      if (eye == 0)
         xr->eye_image_count = image_count;
      else if (image_count != xr->eye_image_count)
      {
         /* Runtimes are expected to hand back matching counts for
          * identically-configured swapchains; if not, clamp to the
          * smaller so the interleaved array stays rectangular. */
         if (image_count < xr->eye_image_count)
            xr->eye_image_count = image_count;
      }

      xr->eye_images[eye] = (XrSwapchainImageVulkanKHR*)
         calloc(image_count, sizeof(XrSwapchainImageVulkanKHR));
      if (!xr->eye_images[eye])
      {
         android_vk_openxr_destroy_swapchains(xr);
         return false;
      }
      {
         uint32_t i;
         for (i = 0; i < image_count; i++)
            xr->eye_images[eye][i].type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
      }
      xrEnumerateSwapchainImages(xr->eye_swapchain[eye], image_count,
            &image_count,
            (XrSwapchainImageBaseHeader*)xr->eye_images[eye]);
   }

   /* Interleave: slot i (frame index i) = [eye0 image i, eye1 image i].
    * vulkan.c's vk->num_swapchain_images becomes xr->eye_image_count,
    * and every place it indexes backbuffers[swapchain_index] instead
    * indexes combined_images[frame_index * 2 + eye] via the driver
    * hook added in vulkan_frame(). */
   xr->combined_count  = xr->eye_image_count * 2;
   xr->combined_images = (VkImage*)malloc(sizeof(VkImage) * xr->combined_count);
   if (!xr->combined_images)
   {
      android_vk_openxr_destroy_swapchains(xr);
      return false;
   }
   {
      uint32_t i;
      for (i = 0; i < xr->eye_image_count; i++)
      {
         xr->combined_images[i * 2 + 0] = xr->eye_images[0][i].image;
         xr->combined_images[i * 2 + 1] = xr->eye_images[1][i].image;
      }
   }

   return true;
}

static bool android_vk_openxr_create_instance(android_vk_openxr_t *xr,
      struct android_app *android_app)
{
   PFN_xrInitializeLoaderKHR init_loader = NULL;
   XrLoaderInitInfoAndroidKHR loader_info = { XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR };
   XrInstanceCreateInfoAndroidKHR android_info = { XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR };
   XrInstanceCreateInfo create_info = { XR_TYPE_INSTANCE_CREATE_INFO };
   const char *extensions[] = { XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME };
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
   create_info.applicationInfo.apiVersion            = XR_API_VERSION_1_0;
   create_info.enabledExtensionCount                 = 1;
   create_info.enabledExtensionNames                 = extensions;

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

/* this function binds an XR session to that already-live device via
 * XR_KHR_vulkan_enable2's graphics requirements + binding struct. */
static bool android_vk_openxr_create_session(android_vk_openxr_t *xr,
      vulkan_context_t *vkctx)
{
   PFN_xrGetVulkanGraphicsRequirements2KHR get_reqs = NULL;
   XrGraphicsRequirementsVulkan2KHR reqs =
      { XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR };
   XrGraphicsBindingVulkan2KHR binding   = { XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR };
   XrSessionCreateInfo session_info      = { XR_TYPE_SESSION_CREATE_INFO };
   XrReferenceSpaceCreateInfo space_info = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
   XrResult result = xrGetInstanceProcAddr(xr->instance,
      "xrGetVulkanGraphicsRequirements2KHR",
      (PFN_xrVoidFunction*)&get_reqs);

   if (result != XR_SUCCESS || !get_reqs)
   {
      RARCH_ERR("[XR] xrGetVulkanGraphicsRequirements2KHR failed: %d\n", result);
      return false;
   }

   result = get_reqs(xr->instance, xr->system, &reqs);

   if (result != XR_SUCCESS)
   {
      RARCH_ERR("[XR] xrGetVulkanGraphicsRequirements2KHR failed: %d\n", result);
      return false;
   }

   binding.instance             = vkctx->instance;
   binding.physicalDevice       = vkctx->gpu;
   binding.device               = vkctx->device;
   binding.queueFamilyIndex     = vkctx->graphics_queue_index;
   binding.queueIndex           = 0;

   session_info.next     = &binding;
   session_info.systemId = xr->system;

   {
      XrResult result = xrCreateSession(
            xr->instance, &session_info, &xr->session);

      if (result != XR_SUCCESS)
      {
         char result_str[XR_MAX_RESULT_STRING_SIZE];
         xrResultToString(xr->instance, result, result_str);
         RARCH_ERR("[XR] xrCreateSession failed: %s (%d)\n",
            result_str, result);
         return false;
      }
   }

   space_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
   space_info.poseInReferenceSpace.orientation.w = 1.0f;
   if (xrCreateReferenceSpace(xr->session, &space_info, &xr->stage_space)
         != XR_SUCCESS)
   {
      /* STAGE may be unavailable on some Quest configs (no guardian set
       * up yet); LOCAL is guaranteed by the spec. */
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

   if (!android_vk_openxr_init_swapchains(xr, VK_FORMAT_B8G8R8A8_UNORM))
   {
      xrDestroySpace(xr->stage_space);
      xr->stage_space = XR_NULL_HANDLE;
      xrDestroySession(xr->session);
      xr->session = XR_NULL_HANDLE;
      return false;
   }

   if (!openxr_input_attach(xr->session))
      return false;

   {
      unsigned i, n = xr->combined_count;
      if (n > VULKAN_MAX_SWAPCHAIN_IMAGES)
         n = VULKAN_MAX_SWAPCHAIN_IMAGES;
      for (i = 0; i < n; i++)
         vkctx->swapchain_images[i] = xr->combined_images[i];
      vkctx->num_swapchain_images = n;
      vkctx->swapchain_format     = xr->swapchain_format;
      vkctx->swapchain_dims       = xr->swapchain_dims;
      vkctx->flags               |= VK_CTX_FLAG_INVALID_SWAPCHAIN;
   }

   return true;
}

static const char *android_vk_openxr_session_state_name(
      XrSessionState state)
{
   switch (state)
   {
      case XR_SESSION_STATE_UNKNOWN:       return "UNKNOWN";
      case XR_SESSION_STATE_IDLE:          return "IDLE";
      case XR_SESSION_STATE_READY:         return "READY";
      case XR_SESSION_STATE_SYNCHRONIZED:  return "SYNCHRONIZED";
      case XR_SESSION_STATE_VISIBLE:       return "VISIBLE";
      case XR_SESSION_STATE_FOCUSED:       return "FOCUSED";
      case XR_SESSION_STATE_STOPPING:      return "STOPPING";
      case XR_SESSION_STATE_LOSS_PENDING:  return "LOSS_PENDING";
      case XR_SESSION_STATE_EXITING:       return "EXITING";
      default:                             return "UNKNOWN";
   }
}

static void android_vk_openxr_poll_events(android_vk_openxr_t *xr, bool *quit)
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
                        {
                           xr->running = true;
                        }
                        else
                           RARCH_ERR("[XR] xrBeginSession failed: session=%p\n",
                              (void *)xr->session);
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

/* ===================== gfx_ctx_driver_t entry points ===================== */

/* forward declaration */
static void android_vk_openxr_gfx_ctx_destroy(void *data);

static void *android_vk_openxr_gfx_ctx_init(void *video_driver)
{
   android_vk_openxr_t *xr = &android_vk_openxr_ctx;

   if (xr->instance != XR_NULL_HANDLE
         && xr->session != XR_NULL_HANDLE
         && xr->vk.context.instance != VK_NULL_HANDLE
         && xr->vk.context.device != VK_NULL_HANDLE
         && xr->combined_images)
      return xr;

   memset(xr, 0, sizeof(*xr));

   xr->android_app = (struct android_app*)g_android;
   if (!xr->android_app)
      return NULL;

   if (!android_vk_openxr_create_instance(xr, xr->android_app))
   {
      android_vk_openxr_gfx_ctx_destroy(xr);
      return NULL;
   }

   if (!openxr_input_init(xr->instance))
   {
      RARCH_ERR("[XR] Touch controller input setup failed.\n");
      android_vk_openxr_gfx_ctx_destroy(xr);
      return NULL;
   }

   /* Capability probe. */
   {
      uint32_t count = 0;
      XrViewConfigurationType types[8];
      uint32_t i;
      bool stereo_supported = false;

      xrEnumerateViewConfigurations(xr->instance, xr->system, 0, &count, NULL);
      if (count == 0 || count > 8)
      {
         android_vk_openxr_gfx_ctx_destroy(xr);
         return NULL;
      }
      xrEnumerateViewConfigurations(xr->instance, xr->system, count, &count, types);
      for (i = 0; i < count; i++)
      {
         if (types[i] == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO)
         {
            stereo_supported = true;
            break;
         }
      }
      if (!stereo_supported)
      {
         RARCH_WARN("[XR] No PRIMARY_STEREO view configuration; no HMD paired?\n");
         android_vk_openxr_gfx_ctx_destroy(xr);
         return NULL;
      }
   }

   /* reports real dimensions instead of 0x0. */
   {
      uint32_t view_count = 0;
      XrViewConfigurationView views[2] =
      {
         { XR_TYPE_VIEW_CONFIGURATION_VIEW },
         { XR_TYPE_VIEW_CONFIGURATION_VIEW }
      };

      if (xrEnumerateViewConfigurationViews(xr->instance, xr->system,
               XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &view_count, views)
            == XR_SUCCESS && view_count == 2)
      {
         xr->eye_width  = (int32_t)views[0].recommendedImageRectWidth;
         xr->eye_height = (int32_t)views[0].recommendedImageRectHeight;
         xr->swapchain_dims = VIDEO_SCALE_PACK((unsigned)xr->eye_width,
               (unsigned)xr->eye_height);
      }
   }

   if (!vulkan_context_init(&xr->vk, VULKAN_WSI_ANDROID)
         || !vulkan_context_create_android_surface(&xr->vk,
            android_app_window(xr->android_app))
       || !vulkan_context_init_headless_device(&xr->vk))
   {
      RARCH_ERR("[XR] Vulkan context/device init failed.\n");
      android_vk_openxr_gfx_ctx_destroy(xr);
      return NULL;
   }

   return xr;
}

static enum gfx_ctx_api android_vk_openxr_gfx_ctx_get_api(void *data)
{
   return GFX_CTX_VULKAN_API;
}

static bool android_vk_openxr_gfx_ctx_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   return api == GFX_CTX_VULKAN_API;
}

static void android_vk_openxr_gfx_ctx_get_video_size(void *data, unsigned *dims)
{
   android_vk_openxr_t *xr = (android_vk_openxr_t*)data;
   *dims = (xr && xr->eye_width > 0) ? xr->swapchain_dims : 0;
}

static bool android_vk_openxr_gfx_ctx_set_video_mode(void *data,
      unsigned dims, bool fullscreen)
{
   return true;
}

static bool android_vk_openxr_gfx_ctx_set_resize(void *data,
      unsigned dims)
{
   return true;
}

static void android_vk_openxr_gfx_ctx_set_flags(void *data, uint32_t flags)
{
}

static void android_vk_openxr_gfx_ctx_check_window(void *data,
      bool *quit, bool *resize, unsigned *dims)
{
   android_vk_openxr_t *xr = (android_vk_openxr_t*)data;
   bool local_quit = false;

   if (!xr)
      return;

   android_vk_openxr_poll_events(xr, &local_quit);

   if (xr->running)
      vulkan_context_advance_frame(&xr->vk);

   *quit   = local_quit;
   *resize = false;
}

static void android_vk_openxr_gfx_ctx_swap_buffers(void *data)
{
   /* Deliberately empty: vulkan_frame() calls xrEndFrame() directly
    * once both eyes are recorded. */
   (void)data;
}

static void android_vk_openxr_gfx_ctx_destroy(void *data)
{
   android_vk_openxr_t *xr = (android_vk_openxr_t*)data;
   if (!xr)
      return;

   video_driver_state_t *video_st = video_state_get_ptr();

   if ((uint32_t)retro_atomic_load_relaxed_int(&video_st->flags)
         & VIDEO_FLAG_CACHE_CONTEXT)
   {
      RARCH_DBG("[XR] CACHE_CONTEXT set - preserving OpenXR/Vulkan context\n");
      return;
   }

   android_vk_openxr_destroy_swapchains(xr);

   if (xr->stage_space != XR_NULL_HANDLE)
      xrDestroySpace(xr->stage_space);

   xr->stage_space = XR_NULL_HANDLE;

   if (xr->view_space != XR_NULL_HANDLE)
      xrDestroySpace(xr->view_space);

   xr->view_space = XR_NULL_HANDLE;

   if (xr->session != XR_NULL_HANDLE)
      xrDestroySession(xr->session);

   xr->session = XR_NULL_HANDLE;
   xr->running = false;

   openxr_input_deinit();

   if (xr->instance != XR_NULL_HANDLE)
      xrDestroyInstance(xr->instance);

   xr->instance = XR_NULL_HANDLE;

   /* OpenXR is now completely detached from the Vulkan device. */
   vulkan_context_destroy(&xr->vk, true);
}

static bool android_vk_openxr_gfx_ctx_has_focus(void *data)
{
   android_vk_openxr_t *xr = (android_vk_openxr_t*)data;
   return xr && xr->focused;
}

static bool android_vk_openxr_gfx_ctx_suppress_screensaver(void *data, bool enable)
{
   return false; /* not meaningful under a VR compositor */
}

static void android_vk_openxr_gfx_ctx_input_driver(void *data,
      const char *joypad_name)
{
   input_driver_video_window(INPUT_WINDOW_PLATFORM, NULL);
}

static gfx_ctx_proc_t android_vk_openxr_gfx_ctx_get_proc_address(const char *symbol)
{
   return NULL; /* Vulkan uses vkGetInstanceProcAddr/vkGetDeviceProcAddr, not this */
}

static void android_vk_openxr_gfx_ctx_show_mouse(void *data, bool state) { }

static uint32_t android_vk_openxr_gfx_ctx_get_flags(void *data)
{
   return 0;
}

static void *android_vk_openxr_gfx_ctx_get_context_data(void *data)
{
   android_vk_openxr_t *xr = (android_vk_openxr_t*)data;
   if (!xr)
      return NULL;
   if (xr->session == XR_NULL_HANDLE
         && !android_vk_openxr_create_session(xr, &xr->vk.context))
      return NULL;
   return &xr->vk.context;
}

bool android_vk_openxr_is_session_ready(void)
{
   android_vk_openxr_t *xr = &android_vk_openxr_ctx;

   return xr->session != XR_NULL_HANDLE
       && xr->combined_images != NULL;
}

static void android_vk_openxr_gfx_ctx_release_current(void *data)
{
}

const gfx_ctx_driver_t gfx_ctx_android_vk_openxr = {
   android_vk_openxr_gfx_ctx_init,
   android_vk_openxr_gfx_ctx_destroy,
   android_vk_openxr_gfx_ctx_get_api,
   android_vk_openxr_gfx_ctx_bind_api,
   NULL, /* swap_interval: no vsync under the XR compositor */
   android_vk_openxr_gfx_ctx_set_video_mode,
   android_vk_openxr_gfx_ctx_get_video_size,
   NULL, /* get_refresh_rate: query via XR_FB_display_refresh_rate if needed later */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL, /* translate_aspect */
   NULL, /* update_window_title */
   android_vk_openxr_gfx_ctx_check_window,
   android_vk_openxr_gfx_ctx_set_resize,
   android_vk_openxr_gfx_ctx_has_focus,
   android_vk_openxr_gfx_ctx_suppress_screensaver,
   true, /* has_windowed: XR presentation, no Android window */
   android_vk_openxr_gfx_ctx_swap_buffers,
   android_vk_openxr_gfx_ctx_input_driver,
   android_vk_openxr_gfx_ctx_get_proc_address,
   NULL, /* image_buffer_init */
   NULL, /* image_buffer_write */
   android_vk_openxr_gfx_ctx_show_mouse,
   "android_vk_openxr",
   android_vk_openxr_gfx_ctx_get_flags,
   android_vk_openxr_gfx_ctx_set_flags,
   NULL, /* bind_hw_render */
   android_vk_openxr_gfx_ctx_get_context_data,
   NULL, /* make_current */
   NULL, /* create_surface */
   NULL, /* destroy_surface */
   NULL, /* presentable */
   NULL, /* last_present_time */
   android_vk_openxr_gfx_ctx_release_current
};
