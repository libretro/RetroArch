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

#ifndef __OPENXR_BINDING_GL_H
#define __OPENXR_BINDING_GL_H

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

/* The native bindings' headers, before OpenXR's platform header. */
#ifdef HAVE_OPENXR_GL
#ifdef HAVE_EGL
#include <EGL/egl.h>
#ifndef XR_USE_PLATFORM_EGL
#define XR_USE_PLATFORM_EGL
#endif
#endif
#ifdef HAVE_X11
#include <X11/Xlib.h>
#include <GL/glx.h>
#ifndef XR_USE_PLATFORM_XLIB
#define XR_USE_PLATFORM_XLIB
#endif
#endif
#endif
#ifndef XR_USE_GRAPHICS_API_OPENGL
#define XR_USE_GRAPHICS_API_OPENGL
#endif
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "openxr_runtime.h"
#include "openxr_swapchain.h"

RETRO_BEGIN_DECLS

#define OPENXR_GL_RGBA8        0x8058  /* GL_RGBA8 */
#define OPENXR_GL_SRGB8_ALPHA8 0x8C43  /* GL_SRGB8_ALPHA8 */

/* Where a slot's images go as textures: [image][layer]. */
typedef struct openxr_gl_images
{
   openxr_swapchains_t *sc;
   uint32_t (*tex)[2];
} openxr_gl_images_t;

/* Asked of the runtime before a GL session, as it must be, and held
 * against the context's version: older than the runtime's least, no
 * session. */
bool openxr_binding_gl_requirements(const openxr_runtime_t *rt,
      unsigned major, unsigned minor);

/* The frames are display-referred: sRGB8_ALPHA8, which the compositor
 * decodes once; else RGBA8, with a warning; else 0. */
int64_t openxr_binding_gl_format(const openxr_swapchains_t *sc);

/* An openxr_images_cb; user is an openxr_gl_images_t. */
XrResult openxr_binding_gl_images(void *user, XrSwapchain swapchain,
      unsigned layer, uint32_t *count);

#ifdef HAVE_OPENXR_GL
struct gfx_ctx_shared_context;

/* A session's graphics binding; the caller keeps it until the session
 * is made. */
typedef union openxr_gl_binding
{
#ifdef HAVE_EGL
   XrGraphicsBindingEGLMNDX egl;
#endif
#ifdef HAVE_X11
   XrGraphicsBindingOpenGLXlibKHR glx;
#endif
   int none;
} openxr_gl_binding_t;

/* XrSessionCreateInfo::next for a shared context, written to binding;
 * NULL for a context this build has no binding for. */
const void *openxr_binding_gl_chain(openxr_gl_binding_t *binding,
      const struct gfx_ctx_shared_context *ctx);
#endif

RETRO_END_DECLS

#endif
