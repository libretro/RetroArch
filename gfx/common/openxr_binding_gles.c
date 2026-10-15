/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

#include <string.h>

#ifdef ANDROID
#include <EGL/egl.h>
#endif

#ifndef XR_USE_GRAPHICS_API_OPENGL_ES
#define XR_USE_GRAPHICS_API_OPENGL_ES
#endif
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "openxr_binding_gles.h"

#include "../../verbosity.h"

bool openxr_binding_gles_requirements(const openxr_runtime_t *rt)
{
   XrResult res;
   XrGraphicsRequirementsOpenGLESKHR reqs;
   PFN_xrGetOpenGLESGraphicsRequirementsKHR get_reqs =
      (PFN_xrGetOpenGLESGraphicsRequirementsKHR)openxr_runtime_proc(rt,
            "xrGetOpenGLESGraphicsRequirementsKHR");
   if (!get_reqs)
   {
      RARCH_ERR("[OpenXR] The runtime lacks functions headset output needs.\n");
      return false;
   }
   reqs.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR;
   reqs.next = NULL;
   if (XR_FAILED(res = get_reqs(rt->instance, rt->system, &reqs)))
   {
      RARCH_ERR("[OpenXR] xrGetOpenGLESGraphicsRequirementsKHR failed (%d).\n",
            (int)res);
      return false;
   }
   RARCH_LOG("[OpenXR] OpenGL ES %u.%u to %u.%u.\n",
         (unsigned)XR_VERSION_MAJOR(reqs.minApiVersionSupported),
         (unsigned)XR_VERSION_MINOR(reqs.minApiVersionSupported),
         (unsigned)XR_VERSION_MAJOR(reqs.maxApiVersionSupported),
         (unsigned)XR_VERSION_MINOR(reqs.maxApiVersionSupported));
   return true;
}

int64_t openxr_binding_gles_format(const openxr_swapchains_t *sc,
      bool srgb_write_control)
{
   /* gl2's values are already sRGB-encoded. */
   if (     srgb_write_control
         && openxr_swapchains_supports(sc, OPENXR_GLES_SRGB8_ALPHA8))
      return OPENXR_GLES_SRGB8_ALPHA8;
   if (openxr_swapchains_supports(sc, OPENXR_GLES_RGBA8))
      return OPENXR_GLES_RGBA8;
   /* Without the extension the copy encodes the values a second time. */
   if (openxr_swapchains_supports(sc, OPENXR_GLES_SRGB8_ALPHA8))
   {
      RARCH_WARN("[OpenXR] Only an sRGB swapchain format is offered; the output may look washed out.\n");
      return OPENXR_GLES_SRGB8_ALPHA8;
   }
   RARCH_ERR("[OpenXR] No swapchain format for OpenGL ES.\n");
   return 0;
}

XrResult openxr_binding_gles_images(void *user, XrSwapchain swapchain,
      unsigned layer, uint32_t *count)
{
   uint32_t i;
   XrResult res;
   XrSwapchainImageOpenGLESKHR imgs[OPENXR_MAX_IMAGES];
   openxr_gles_images_t *li = (openxr_gles_images_t*)user;
   for (i = 0; i < OPENXR_MAX_IMAGES; i++)
   {
      imgs[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
      imgs[i].next = NULL;
   }
   res = li->sc->EnumerateSwapchainImages(swapchain, *count, count,
         (XrSwapchainImageBaseHeader*)imgs);
   for (i = 0; XR_SUCCEEDED(res) && i < *count; i++)
      li->tex[i][layer] = imgs[i].image;
   return res;
}
