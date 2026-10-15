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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "openxr_binding_gl.h"

#ifdef HAVE_OPENXR_GL
#include "../video_driver.h"
#endif
#include "../../verbosity.h"

bool openxr_binding_gl_requirements(const openxr_runtime_t *rt,
      unsigned major, unsigned minor)
{
   XrResult res;
   XrVersion least;
   XrGraphicsRequirementsOpenGLKHR reqs;
   PFN_xrGetOpenGLGraphicsRequirementsKHR get_reqs =
      (PFN_xrGetOpenGLGraphicsRequirementsKHR)openxr_runtime_proc(rt,
            "xrGetOpenGLGraphicsRequirementsKHR");
   if (!get_reqs)
   {
      RARCH_ERR("[OpenXR] The runtime lacks functions headset output needs.\n");
      return false;
   }
   reqs.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR;
   reqs.next = NULL;
   if (XR_FAILED(res = get_reqs(rt->instance, rt->system, &reqs)))
   {
      RARCH_ERR("[OpenXR] xrGetOpenGLGraphicsRequirementsKHR failed (%d).\n",
            (int)res);
      return false;
   }
   RARCH_LOG("[OpenXR] OpenGL %u.%u to %u.%u.\n",
         (unsigned)XR_VERSION_MAJOR(reqs.minApiVersionSupported),
         (unsigned)XR_VERSION_MINOR(reqs.minApiVersionSupported),
         (unsigned)XR_VERSION_MAJOR(reqs.maxApiVersionSupported),
         (unsigned)XR_VERSION_MINOR(reqs.maxApiVersionSupported));
   /* The most is what the runtime was tested with; newer may work. */
   least = XR_MAKE_VERSION(XR_VERSION_MAJOR(reqs.minApiVersionSupported),
         XR_VERSION_MINOR(reqs.minApiVersionSupported), 0);
   if (XR_MAKE_VERSION(major, minor, 0) < least)
   {
      RARCH_ERR("[OpenXR] The runtime needs OpenGL %u.%u; the context is %u.%u.\n",
            (unsigned)XR_VERSION_MAJOR(least),
            (unsigned)XR_VERSION_MINOR(least), major, minor);
      return false;
   }
   return true;
}

int64_t openxr_binding_gl_format(const openxr_swapchains_t *sc)
{
   if (openxr_swapchains_supports(sc, OPENXR_GL_SRGB8_ALPHA8))
      return OPENXR_GL_SRGB8_ALPHA8;
   /* Taken as linear: the values are encoded twice. */
   if (openxr_swapchains_supports(sc, OPENXR_GL_RGBA8))
   {
      RARCH_WARN("[OpenXR] Only a linear swapchain format is offered; the output may look washed out.\n");
      return OPENXR_GL_RGBA8;
   }
   RARCH_ERR("[OpenXR] No swapchain format for OpenGL.\n");
   return 0;
}

XrResult openxr_binding_gl_images(void *user, XrSwapchain swapchain,
      unsigned layer, uint32_t *count)
{
   uint32_t i;
   XrResult res;
   XrSwapchainImageOpenGLKHR imgs[OPENXR_MAX_IMAGES];
   openxr_gl_images_t *li = (openxr_gl_images_t*)user;
   for (i = 0; i < OPENXR_MAX_IMAGES; i++)
   {
      imgs[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
      imgs[i].next = NULL;
   }
   res = li->sc->EnumerateSwapchainImages(swapchain, *count, count,
         (XrSwapchainImageBaseHeader*)imgs);
   for (i = 0; XR_SUCCEEDED(res) && i < *count; i++)
      li->tex[i][layer] = imgs[i].image;
   return res;
}

#ifdef HAVE_OPENXR_GL
const void *openxr_binding_gl_chain(openxr_gl_binding_t *binding,
      const struct gfx_ctx_shared_context *ctx)
{
   switch (ctx->kind)
   {
#ifdef HAVE_EGL
      case GFX_CTX_SHARED_EGL:
         binding->egl.type           = XR_TYPE_GRAPHICS_BINDING_EGL_MNDX;
         binding->egl.next           = NULL;
         binding->egl.getProcAddress =
            (PFN_xrEglGetProcAddressMNDX)ctx->get_proc_address;
         binding->egl.display        = (EGLDisplay)ctx->display;
         binding->egl.config         = (EGLConfig)ctx->config;
         binding->egl.context        = (EGLContext)ctx->context;
         return &binding->egl;
#endif
#ifdef HAVE_X11
      case GFX_CTX_SHARED_GLX:
         binding->glx.type        = XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR;
         binding->glx.next        = NULL;
         binding->glx.xDisplay    = (Display*)ctx->display;
         binding->glx.visualid    = (uint32_t)ctx->visual_id;
         binding->glx.glxFBConfig = (GLXFBConfig)ctx->config;
         binding->glx.glxDrawable = (GLXDrawable)ctx->drawable;
         binding->glx.glxContext  = (GLXContext)ctx->context;
         return &binding->glx;
#endif
      default:
         break;
   }
   return NULL;
}
#endif
