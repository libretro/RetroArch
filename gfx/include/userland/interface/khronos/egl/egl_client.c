/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
   Global preconditions?

   Server is up (needed by RPC_CALL[n]_RES)

   Spec ambiguity:

   What should we do if eglGetError is called twice? Currently we reset the error to EGL_SUCCESS.

   eglGetConfigs:
   "The number of configurations is returned in num config"
   We assume this refers to the number of configurations returned, rather than the total number of
   configurations available. (These are different values if config_size is too small).

   eglChooseConfig:
   Fail if the same attribute is specified more than once?
   (I can't find anything in the spec saying what to do in this case)

   In general, which attribute values are accepted and which raise
   EGL_BAD_ATTRIBUTE is vague.

   In particular, what do we do about the "ignored" and conditionally
   ignored ones?

   Currently ignoring ("ignored"):
   case EGL_MAX_PBUFFER_HEIGHT:
   case EGL_MAX_PBUFFER_PIXELS:
   case EGL_MAX_PBUFFER_WIDTH:
   case EGL_NATIVE_VISUAL_ID:

   Currently not ignoring ("conditionally ignored")
   case EGL_TRANSPARENT_BLUE_VALUE:
   case EGL_TRANSPARENT_GREEN_VALUE:
   case EGL_TRANSPARENT_RED_VALUE:

   Currently ignoring ("conditionally ignored")
   case EGL_NATIVE_VISUAL_TYPE:

   The following sentences in the spec seem to contradict each other:
   "If EGL_MATCH_NATIVE_PIXMAP is specified in attrib list, it must be followed
by an attribute value"

   "If EGL_DONT_CARE is specified as an attribute value, then the
   attribute will not be checked. EGL_DONT_CARE may be specified for all attributes
   except EGL_LEVEL."

   In addition, EGL_NONE is listed as the default match value for EGL_MATCH_NATIVE_PIXMAP.
   What happens if EGL_DONT_CARE or EGL_NONE is a valid native pixmap value?

   What we actually do is we always treat the value supplied with EGL_MATCH_NATIVE_PIXMAP
   as a valid handle (and fail if it's invalid), and ignore it only if it's not in the list
   at all.

   EGL_MATCH_NATIVE_PIXMAP: todo: we'll set thread->error to something like EGL_BAD_ATTRIBUTE; should we be setting it to EGL_BAD_NATIVE_PIXMAP?
   What is EGL_PRESERVED_RESOURCES?

   Do we need to do anything for EGL_LEVEL?

   What is EGL_PRESERVED_RESOURCES?
   What exactly are EGL_NATIVE_VISUAL_ID, EGL_NATIVE_VISUAL_TYPE?

   Implementation notes:

   We only support one display. This is assumed to have a native display_id
   of 0 (==EGL_DEFAULT_DISPLAY) and an EGLDisplay id of 1

   All EGL client functions preserve the invariant (CLIENT_THREAD_STATE_ERROR)

   It would be nice for the EGL version to only be defined in one place (rather than both eglInitialize and eglQueryString).

   We allow implicit casts from bool to EGLint

   We make the following assumptions:

      EGL_CONFIG_CAVEAT (all EGL_NONE)
      EGL_COLOR_BUFFER_TYPE (all EGL_RGB_BUFFER)
      EGL_SAMPLES (if EGL_SAMPLES is 1 then all 4 else all 0)
      EGL_NATIVE_RENDERABLE is true
      EGL_MAX_PBUFFER_WIDTH, EGL_MAX_PBUFFER_HEIGHT, EGL_MIN_SWAP_INTERVAL, EGL_MAX_SWAP_INTERVAL are the same for all configs

      All configs support all of:
         (EGL_PBUFFER_BIT | EGL_PIXMAP_BIT | EGL_WINDOW_BIT | EGL_VG_COLORSPACE_LINEAR_BIT | EGL_VG_ALPHA_FORMAT_PRE_BIT | EGL_MULTISAMPLE_RESOLVE_BOX_BIT | EGL_SWAP_BEHAVIOR_PRESERVED_BIT);

   EGL_OPTIMAL_FORMAT_BIT_KHR: Considered optimal if no format conversion needs doing

   EGL_TRANSPARENT_TYPE is always EGL_NONE because we don't support EGL_TRANSPARENT_RGB. Should there be an EGL_TRANSPARENT_ALPHA?
*/

#define VCOS_LOG_CATEGORY (&egl_client_log_cat)

#include "interface/khronos/common/khrn_client_mangle.h"

#include "interface/khronos/common/khrn_int_common.h"
#include "interface/khronos/common/khrn_options.h"

#include "interface/khronos/egl/egl_client_surface.h"
#include "interface/khronos/egl/egl_client_context.h"
#include "interface/khronos/egl/egl_client_config.h"

#include "interface/khronos/common/khrn_client.h"
#include "interface/khronos/common/khrn_client_rpc.h"

#ifdef RPC_DIRECT
#include "interface/khronos/egl/egl_int_impl.h"
#endif

#if defined(WIN32) || defined(__mips__)
#include "interface/khronos/common/khrn_int_misc_impl.h"
#endif

#ifdef KHRONOS_EGL_PLATFORM_OPENWFC
#include "interface/khronos/wf/wfc_client_stream.h"
#endif

#if defined(RPC_DIRECT_MULTI)
#include "middleware/khronos/egl/egl_server.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "interface/khronos/egl/egl_client_cr.c"

VCOS_LOG_CAT_T egl_client_log_cat;

static void egl_current_release(CLIENT_PROCESS_STATE_T *process, EGL_CURRENT_T *current);
void egl_gl_flush_callback(bool wait);
void egl_vg_flush_callback(bool wait);

/*
TODO: do an RPC call to make sure the Khronos vll is loaded (and that it stays loaded until eglTerminate)
Also affects global image (and possibly others?)
   EGLAPI EGLBoolean EGLAPIENTRY eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor)

   Khronos documentation:

   EGL may be initialized on a display by calling
   EGLBoolean eglInitialize(EGLDisplay dpy, EGLint
   *major, EGLint *minor);
   EGL TRUE is returned on success, and major and minor are updated with the major
   and minor version numbers of the EGL implementation (for example, in an EGL
   1.2 implementation, the values of *major and *minor would be 1 and 2, respectively).
   major and minor are not updated if they are specified as NULL.
   EGL FALSE is returned on failure and major and minor are not updated. An
   EGL BAD DISPLAY error is generated if the dpy argument does not refer to a valid
   EGLDisplay. An EGL NOT INITIALIZED error is generated if EGL cannot be
   initialized for an otherwise valid dpy.
   Initializing an already-initialized display is allowed, but the only effect of such
   a call is to return EGL TRUE and update the EGL version numbers. An initialized
   display may be used from other threads in the same address space without being
   initalized again in those threads.

   Implementation notes:

   client_egl_get_process_state sets some errors for us.

   Preconditions:

   major is NULL or a valid pointer
   minor is NULL or a valid pointer
   eglTerminate(dpy) must be called at some point after calling this function if we return EGL_TRUE.

   Postconditions:

   The following conditions cause error to assume the specified value

      EGL_BAD_DISPLAY               An EGLDisplay argument does not name a valid EGLDisplay
      EGL_NOT_INITIALIZED           EGL is not initialized, or could not be initialized, for the specified display.
      EGL_SUCCESS                   Function succeeded.

   if more than one condition holds, the first error is generated.

   Invariants preserved:

   -

   Invariants used:

   -
*/

EGLAPI EGLBoolean EGLAPIENTRY eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   EGLBoolean result;

   CLIENT_LOCK();

   {
      CLIENT_PROCESS_STATE_T *process = client_egl_get_process_state(thread, dpy, EGL_FALSE);

      if (process) {
         if (!client_process_state_init(process))
         {
            thread->error = EGL_NOT_INITIALIZED;
            result = EGL_FALSE;
         }
         else
         {
            thread->error = EGL_SUCCESS;
            result = EGL_TRUE;
         }
      } else
         result = EGL_FALSE;

      if (result) {
         if (major)
            *major = 1;
         if (minor)
            *minor = 4;
      }
   }

   CLIENT_UNLOCK();

   vcos_log_set_level(&egl_client_log_cat, VCOS_LOG_WARN);
   vcos_log_register("egl_client", &egl_client_log_cat);
   vcos_log_info("eglInitialize end. dpy=%d.", (int)dpy);
   khrn_init_options();

   return result;
}

/*
   EGLAPI EGLBoolean EGLAPIENTRY eglTerminate(EGLDisplay dpy)

   Khronos documentation:

   To release resources associated with use of EGL and client APIs on a display,
   call
   EGLBoolean eglTerminate(EGLDisplay dpy);
   Termination marks all EGL-specific resources associated with the specified display
   for deletion. If contexts or surfaces created with respect to dpy are current (see
   section 3.7.3) to any thread, then they are not actually released while they remain
   current. Such contexts and surfaces will be destroyed, and all future references to
   them will become invalid, as soon as any otherwise valid eglMakeCurrent call is
   made from the thread they are bound to.
   eglTerminate returns EGL TRUE on success.
   If the dpy argument does not refer to a valid EGLDisplay, EGL FALSE is
   returned, and an EGL BAD DISPLAY error is generated.
   Termination of a display that has already been terminated, or has not yet been
   initialized, is allowed, but the only effect of such a call is to return EGL TRUE, since
   there are no EGL resources associated with the display to release. A terminated
   display may be re-initialized by calling eglInitialize again. When re-initializing
   a terminated display, resources which were marked for deletion as a result of the
   earlier termination remain so marked, and references to them are not valid.

   Implementation notes:

   -

   Preconditions:

   -

   Postconditions:

   The following conditions cause error to assume the specified value

      EGL_BAD_DISPLAY               An EGLDisplay argument does not name a valid EGLDisplay
      EGL_SUCCESS                   Function succeeded.

   if more than one condition holds, the first error is generated.

   Invariants preserved:

   -

   Invariants used:

   -
*/

EGLAPI EGLBoolean EGLAPIENTRY eglTerminate(EGLDisplay dpy)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   vcos_log_trace("eglTerminate start. dpy=%d", (int)dpy);

#ifdef RPC_DIRECT_MULTI
	return true;  //it is moved to khronos_exit
#else
   {
      EGLBoolean result;
      CLIENT_LOCK();

      {
         CLIENT_PROCESS_STATE_T *process = client_egl_get_process_state(thread, dpy, EGL_FALSE);

         if (process) {
            client_process_state_term(process);

            thread->error = EGL_SUCCESS;
            result = EGL_TRUE;

            client_try_unload_server(process);
         } else
            result = EGL_FALSE;
      }

      CLIENT_UNLOCK();

      vcos_log_trace("eglTerminate end. dpy=%d", (int)dpy);
      vcos_log_unregister(&egl_client_log_cat);
      return result;
   }
#endif
}

/*
   EGLAPI const char EGLAPIENTRY * eglQueryString(EGLDisplay dpy, EGLint name)

   Khronos documentation:

   3.3 EGL Versioning
const char *eglQueryString(EGLDisplay dpy, EGLint
name);
eglQueryString returns a pointer to a static, zero-terminated string describing
some aspect of the EGL implementation running on the specified display.
name may be one of EGL CLIENT APIS, EGL EXTENSIONS, EGL VENDOR, or
EGL VERSION.
The EGL CLIENT APIS string describes which client rendering APIs are supported.
It is zero-terminated and contains a space-separated list of API names,
which must include at least one of ‘‘OpenGL ES’’ or ‘‘OpenVG’’.
Version 1.3 - December 4, 2006
3.4. CONFIGURATION MANAGEMENT 13
The EGL EXTENSIONS string describes which EGL extensions are supported
by the EGL implementation running on the specified display. The string is zeroterminated
and contains a space-separated list of extension names; extension names
themselves do not contain spaces. If there are no extensions to EGL, then the empty
string is returned.
The format and contents of the EGL VENDOR string is implementation dependent.
The format of the EGL VERSION string is:
<major version.minor version><space><vendor specific info>
Both the major and minor portions of the version number are numeric. Their values
must match the major and minor values returned by eglInitialize (see section 3.2).
The vendor-specific information is optional; if present, its format and contents are
implementation specific.
On failure, NULL is returned. An EGL NOT INITIALIZED error is generated if
EGL is not initialized for dpy. An EGL BAD PARAMETER error is generated if name
is not one of the values described above.

   Implementation notes:

   We support the following extensions but they can be removed from the driver if defined to zero.
      EGL_KHR_image extensions
      EGL_KHR_lock_surface

   Preconditions:

   -

   Postconditions:

   The following conditions cause error to assume the specified value

      EGL_BAD_DISPLAY               An EGLDisplay argument does not name a valid EGLDisplay
      EGL_NOT_INITIALIZED           EGL is not initialized for the specified display.
      EGL_BAD_PARAMETER             name is not one of {EGL_CLIENT_APIS, EGL_EXTENSIONS, EGL_VENDOR, EGL_VERSION}
      EGL_SUCCESS                   Function succeeded.

   if more than one condition holds, the first error is generated.

   Return value is NULL or a pointer to a null-terminated string which is valid forever.

   Invariants preserved:

   -

   Invariants used:

   -
*/

EGLAPI const char EGLAPIENTRY * eglQueryString(EGLDisplay dpy, EGLint name)
{
   CLIENT_THREAD_STATE_T *thread;
   CLIENT_PROCESS_STATE_T *process;
   const char *result = NULL;

   if (CLIENT_LOCK_AND_GET_STATES(dpy, &thread, &process))
   {
      thread->error = EGL_SUCCESS;
      switch (name) {
      case EGL_CLIENT_APIS:
         result = "OpenGL_ES OpenVG";
         break;
      case EGL_EXTENSIONS:
         //TODO: this list isn't quite correct
         result = ""
#if EGL_KHR_image
            "EGL_KHR_image EGL_KHR_image_base EGL_KHR_image_pixmap EGL_KHR_vg_parent_image EGL_KHR_gl_texture_2D_image EGL_KHR_gl_texture_cubemap_image "
#endif
#if EGL_KHR_lock_surface
            "EGL_KHR_lock_surface "
#endif
#if EGL_ANDROID_swap_rectangle
            "EGL_ANDROID_swap_rectangle "
#endif
#ifdef ANDROID
            "EGL_ANDROID_image_native_buffer "
#endif
#ifdef ANDROID
#ifdef EGL_KHR_fence_sync
            "EGL_KHR_fence_sync "
#endif
#endif
            ;
         break;
      case EGL_VENDOR:
         result = "Broadcom";
         break;
      case EGL_VERSION:
         result = "1.4";
         break;
      default:
         thread->error = EGL_BAD_PARAMETER;
         result = NULL;
      }
      CLIENT_UNLOCK();
   } else
      result = NULL;

   return result;
}

/*
   EGLAPI EGLSurface EGLAPIENTRY eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list)

   Khronos documentation:

   3.5.1 Creating On-Screen Rendering Surfaces
   To create an on-screen rendering surface, first create a native platform window
   with attributes corresponding to the desired EGLConfig (e.g. with the same color
   depth, with other constraints specific to the platform). Using the platform-specific
   type EGLNativeWindowType, which is the type of a handle to that native window,
   then call:

   EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win,
   const EGLint *attrib_list);

   eglCreateWindowSurface creates an onscreen EGLSurface and returns a handle
   to it. Any EGL context created with a compatible EGLConfig can be used to
   render into this surface.

   attrib list specifies a list of attributes for the window. The list has the same
   structure as described for eglChooseConfig. Attributes that can be specified in
   attrib list include EGL_RENDER_BUFFER, EGL_VG_COLORSPACE, and EGL_VG_ALPHA_FORMAT.
   It is possible that some platforms will define additional attributes specific to
   those environments, as an EGL extension.

   attrib list may be NULL or empty (first attribute is EGL_NONE), in which case
   all attributes assumes their default value as described below.
   EGL_RENDER_BUFFER specifies which buffer should be used for client API
   rendering to the window, as described in section 2.2.2. If its value is EGL_SINGLE_BUFFER,
   then client APIs should render directly into the visible window.

   If its value is EGL_BACK_BUFFER, then all client APIs should render into the back
   buffer. The default value of EGL_RENDER_BUFFER is EGL_BACK_BUFFER.

   Client APIs may not be able to respect the requested rendering buffer. To
   determine the actual buffer being rendered to by a context, call eglQueryContext
   (see section 3.7.4).

   EGL_VG_COLORSPACE specifies the color space used by OpenVG when
   rendering to the surface. If its value is EGL_VG_COLORSPACE_sRGB, then
   a non-linear, perceptually uniform color space is assumed, with a corresponding
   VGImageFormat of form VG_s*. If its value is EGL_VG_-
   COLORSPACE_LINEAR, then a linear color space is assumed, with a corresponding
   VGImageFormat of form VG_l*. The default value of EGL_VG_COLORSPACE
   is EGL_VG_COLORSPACE_sRGB.

   EGL_VG_ALPHA_FORMAT specifies how alpha values are interpreted by
   OpenVG when rendering to the surface. If its value is EGL_VG_ALPHA_FORMAT_-
   NONPRE, then alpha values are not premultipled. If its value is EGL_VG_ALPHA_-
   FORMAT_PRE, then alpha values are premultiplied. The default value of EGL_VG_-
   ALPHA_FORMAT is EGL_VG_ALPHA_FORMAT_NONPRE.

   Note that the EGL_VG_COLORSPACE and EGL_VG_ALPHA_FORMAT attributes
   are used only by OpenVG . EGL itself, and other client APIs such as OpenGL and
   OpenGL ES , do not distinguish multiple colorspace models. Refer to section 11.2
   of the OpenVG 1.0 specification for more information.

   Similarly, the EGL_VG_ALPHA_FORMAT attribute does not necessarily control
   or affect the window system’s interpretation of alpha values, even when the window
   system makes use of alpha to composite surfaces at display time. The window system's
   use and interpretation of alpha values is outside the scope of EGL. However,
   the preferred behavior is for window systems to ignore the value of EGL_VG_-
   ALPHA_FORMAT when compositing window surfaces.

   On failure eglCreateWindowSurface returns EGL_NO_SURFACE. If the attributes
   of win do not correspond to config, then an EGL_BAD_MATCH error is generated.
   If config does not support rendering to windows (the EGL_SURFACE_TYPE
   attribute does not contain EGL_WINDOW_BIT), an EGL_BAD_MATCH error is generated.
   If config does not support the colorspace or alpha format attributes specified
   in attrib list (as defined for eglCreateWindowSurface), an EGL_BAD_MATCH error
   is generated. If config is not a valid EGLConfig, an EGL_BAD_CONFIG error
   is generated. If win is not a valid native window handle, then an EGL_BAD_NATIVE_WINDOW
   error should be generated. If there is already an EGLConfig
   associated with win (as a result of a previous eglCreateWindowSurface call), then
   an EGL_BAD_ALLOC error is generated. Finally, if the implementation cannot allocate
   resources for the new EGL window, an EGL_BAD_ALLOC error is generated.

   Implementation notes:

   -

   Preconditions:

   attrib_list is NULL or a pointer to an EGL_NONE-terminated list of attribute/value pairs

   Postconditions:

   The following conditions cause error to assume the specified value

      EGL_BAD_DISPLAY               An EGLDisplay argument does not name a valid EGLDisplay
      EGL_NOT_INITIALIZED           EGL is not initialized for the specified display.
      EGL_BAD_CONFIG                config is not a valid EGLConfig
      EGL_BAD_NATIVE_WINDOW         win is not a valid native window handle
      EGL_BAD_ATTRIBUTE             attrib_list contains an undefined EGL attribute or an attribute value that is unrecognized or out of range.
           (TODO: EGL_BAD_ATTRIBUTE not mentioned in spec)
      EGL_BAD_NATIVE_WINDOW         window is larger than EGL_CONFIG_MAX_WIDTH x EGL_CONFIG_MAX_HEIGHT
           (TODO: Maybe EGL_BAD_ALLOC might be more appropriate?)
      EGL_BAD_ALLOC                 implementation cannot allocate resources for the new EGL window
           (TODO: If there is already an EGLConfig associated with win)
      EGL_SUCCESS                   Function succeeded.

   if more than one condition holds, the first error is generated.

   Return value is EGL_NO_SURFACE or an EGLSurface handle which is valid until the EGL session ends or
   eglDestroySurface is called.

   Invariants preserved:

   (CLIENT_PROCESS_STATE_SURFACES)
   (CLIENT_PROCESS_STATE_NEXT_SURFACE)

   Invariants used:

   -
*/

EGLAPI EGLSurface EGLAPIENTRY eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list)
{
   CLIENT_THREAD_STATE_T *thread;
   CLIENT_PROCESS_STATE_T *process;
   EGLSurface result;

   vcos_log_trace("eglCreateWindowSurface for window %p", win);

   if (CLIENT_LOCK_AND_GET_STATES(dpy, &thread, &process))
   {
      uint32_t handle = platform_get_handle(dpy, win);

      if ((int)(size_t)config < 1 || (int)(size_t)config > EGL_MAX_CONFIGS) {
         thread->error = EGL_BAD_CONFIG;
         result = EGL_NO_SURFACE;
      } else if (handle == PLATFORM_WIN_NONE) {
         // The platform reports that this is an invalid window handle
         thread->error = EGL_BAD_NATIVE_WINDOW;
         result = EGL_NO_SURFACE;
      } else {
         bool linear = false;
         bool premult = false;
         bool single = false;

         if (!egl_surface_check_attribs(WINDOW, attrib_list, &linear, &premult, &single, 0, 0, 0, 0, 0, 0)) {
            thread->error = EGL_BAD_ATTRIBUTE;
            result = EGL_NO_SURFACE;
         } else {
            EGL_SURFACE_T *surface;

            uint32_t width, height;
            uint32_t num_buffers = 3;
            uint32_t swapchain_count;

            platform_get_dimensions(dpy,
                  win, &width, &height, &swapchain_count);

            if (swapchain_count > 0)
               num_buffers = swapchain_count;
            else
            {
               if (khrn_options.double_buffer)
                  num_buffers = 2;
            }

            if (width <= 0 || width > EGL_CONFIG_MAX_WIDTH || height <= 0 || height > EGL_CONFIG_MAX_HEIGHT) {
               /* TODO: Maybe EGL_BAD_ALLOC might be more appropriate? */
               thread->error = EGL_BAD_NATIVE_WINDOW;
               result = EGL_NO_SURFACE;
            } else {
               surface = egl_surface_create(
                                (EGLSurface)(size_t)process->next_surface,
                                WINDOW,
                                linear ? LINEAR : SRGB,
                                premult ? PRE : NONPRE,
#ifdef DIRECT_RENDERING
                                1,
#else
                                (uint32_t)(single ? 1 : num_buffers),
#endif
                                width, height,
                                config,
                                win,
                                handle,
                                false,
                                false,
                                false,
                                EGL_NO_TEXTURE,
                                EGL_NO_TEXTURE,
                                0, 0);

               if (surface) {
                  if (khrn_pointer_map_insert(&process->surfaces, process->next_surface, surface)) {
                     thread->error = EGL_SUCCESS;
                     result = (EGLSurface)(size_t)process->next_surface++;
                  } else {
                     thread->error = EGL_BAD_ALLOC;
                     result = EGL_NO_SURFACE;
                     egl_surface_free(surface);
                  }
               } else {
                  thread->error = EGL_BAD_ALLOC;
                  result = EGL_NO_SURFACE;
               }
            }
         }
      }
      CLIENT_UNLOCK();
   }
   else
      result = EGL_NO_SURFACE;

   vcos_log_trace("eglCreateWindowSurface end %i", (int) result);

   return result;
}

/*
   EGLAPI EGLSurface EGLAPIENTRY eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list)

   Khronos documentation:

   3.5.2 Creating Off-Screen Rendering Surfaces
   EGL supports off-screen rendering surfaces in pbuffers. Pbuffers differ from windows
   in the following ways:

   1. Pbuffers are typically allocated in offscreen (non-visible) graphics memory
   and are intended only for accelerated offscreen rendering. Allocation can fail
   if there are insufficient graphics resources (implementations are not required
   to virtualize framebuffer memory). Clients should deallocate pbuffers when
   they are no longer in use, since graphics memory is often a scarce resource.

   2. Pbuffers are EGL resources and have no associated native window or native
   window type. It may not be possible to render to pbuffers using native
   rendering APIs.

   To create a pbuffer, call

      EGLSurface eglCreatePbufferSurface(EGLDisplay dpy,
         EGLConfig config, const EGLint
         *attrib_list);

   This creates a single pbuffer surface and returns a handle to it.
   attrib list specifies a list of attributes for the pbuffer. The list has the same
   structure as described for eglChooseConfig. Attributes that can be specified in
   attrib list include EGL_WIDTH, EGL_HEIGHT, EGL_LARGEST_PBUFFER, EGL_TEXTURE_FORMAT,
   EGL_TEXTURE_TARGET, EGL_MIPMAP_TEXTURE, EGL_VG_COLORSPACE, and EGL_VG_ALPHA_FORMAT.

   It is possible that some platforms will define additional attributes specific to
   those environments, as an EGL extension.

   attrib list may be NULL or empty (first attribute is EGL_NONE), in which case
   all the attributes assume their default values as described below.

   EGL_WIDTH and EGL_HEIGHT specify the pixel width and height of the rectangular
   pbuffer. If the value of EGLConfig attribute EGL_TEXTURE_FORMAT is
   not EGL_NO_TEXTURE, then the pbuffer width and height specify the size of the
   level zero texture image. The default values for EGL_WIDTH and EGL_HEIGHT are
   zero.

   EGL_TEXTURE_FORMAT specifies the format of the OpenGL ES texture that
   will be created when a pbuffer is bound to a texture map. It can be set to EGL_-
   TEXTURE_RGB, EGL_TEXTURE_RGBA, or EGL_NO_TEXTURE. The default value of
   EGL_TEXTURE_FORMAT is EGL_NO_TEXTURE.

   EGL_TEXTURE_TARGET specifies the target for the OpenGL ES texture that
   will be created when the pbuffer is created with a texture format of EGL_-
   TEXTURE_RGB or EGL_TEXTURE_RGBA. The target can be set to EGL_NO_-
   TEXTURE or EGL_TEXTURE_2D. The default value of EGL_TEXTURE_TARGET is
   EGL_NO_TEXTURE.

   EGL_MIPMAP_TEXTURE indicates whether storage for OpenGL ES mipmaps
   should be allocated. Space for mipmaps will be set aside if the attribute value
   is EGL_TRUE and EGL_TEXTURE_FORMAT is not EGL_NO_TEXTURE. The default
   value for EGL_MIPMAP_TEXTURE is EGL_FALSE.

   Use EGL_LARGEST_PBUFFER to get the largest available pbuffer when the allocation
   of the pbuffer would otherwise fail. The width and height of the allocated
   pbuffer will never exceed the values of EGL_WIDTH and EGL_HEIGHT, respectively.
   If the pbuffer will be used as a OpenGL ES texture (i.e., the value of
   EGL_TEXTURE_TARGET is EGL_TEXTURE_2D, and the value of EGL_TEXTURE_-
   FORMAT is EGL_TEXTURE_RGB or EGL_TEXTURE_RGBA), then the aspect ratio
   will be preserved and the new width and height will be valid sizes for the texture
   target (e.g. if the underlying OpenGL ES implementation does not support
   non-power-of-two textures, both the width and height will be a power of 2). Use
   eglQuerySurface to retrieve the dimensions of the allocated pbuffer. The default
   value of EGL_LARGEST_PBUFFER is EGL_FALSE.

   EGL_VG_COLORSPACE and EGL_VG_ALPHA_FORMAT have the same meaning
   and default values as when used with eglCreateWindowSurface.
   The resulting pbuffer will contain color buffers and ancillary buffers as specified
   by config.

   The contents of the depth and stencil buffers may not be preserved when rendering
   an OpenGL ES texture to the pbuffer and switching which image of the
   texture is rendered to (e.g., switching from rendering one mipmap level to rendering
   another).

   On failure eglCreatePbufferSurface returns EGL_NO_SURFACE. If the pbuffer
   could not be created due to insufficient resources, then an EGL_BAD_ALLOC error
   is generated. If config is not a valid EGLConfig, an EGL_BAD_CONFIG error is
   generated. If the value specified for either EGL_WIDTH or EGL_HEIGHT is less
   than zero, an EGL_BAD_PARAMETER error is generated. If config does not support
   pbuffers, an EGL_BAD_MATCH error is generated. In addition, an EGL_BAD_MATCH
   error is generated if any of the following conditions are true:

   The EGL_TEXTURE_FORMAT attribute is not EGL_NO_TEXTURE, and EGL_WIDTH
   and/or EGL_HEIGHT specify an invalid size (e.g., the texture size is
   not a power of two, and the underlying OpenGL ES implementation does not
   support non-power-of-two textures).

   The EGL_TEXTURE_FORMAT attribute is EGL_NO_TEXTURE, and EGL_TEXTURE_TARGET
   is something other than EGL_NO_TEXTURE; or, EGL_TEXTURE_FORMAT is something
   other than EGL_NO_TEXTURE, and EGL_TEXTURE_TARGET is EGL_NO_TEXTURE.

   Finally, an EGL_BAD_ATTRIBUTE error is generated if any of the EGL_TEXTURE_FORMAT,
   EGL_TEXTURE_TARGET, or EGL_MIPMAP_TEXTURE attributes
   are specified, but config does not support OpenGL ES rendering (e.g.
   the EGL_RENDERABLE_TYPE attribute does not include at least one of EGL_OPENGL_ES_BIT
   or EGL_OPENGL_ES2_BIT.

   Implementation notes:

   -

   attrib_list is NULL or a pointer to an EGL_NONE-terminated list of attribute/value pairs

   Postconditions:

   The following conditions cause error to assume the specified value

      EGL_BAD_DISPLAY               An EGLDisplay argument does not name a valid EGLDisplay
      EGL_NOT_INITIALIZED           EGL is not initialized for the specified display.
      EGL_BAD_CONFIG                config is not a valid EGLConfig
      EGL_BAD_ATTRIBUTE             attrib_list contains an undefined EGL attribute or an attribute value that is unrecognized or out of range.
           (TODO: EGL_BAD_ATTRIBUTE not mentioned in spec)
      EGL_BAD_MATCH                 config doesn't support EGL_BIND_TO_TEXTURE_RGB(A) and you specify EGL_TEXTURE_FORMAT=EGL_TEXTURE_RGB(A)
           (TODO: no mention of this in the spec)
      EGL_BAD_ALLOC                 requested dimensions are larger than EGL_CONFIG_MAX_WIDTH x EGL_CONFIG_MAX_HEIGHT
           (TODO: no mention of this in the spec)
      EGL_BAD_ALLOC                 implementation cannot allocate resources for the new EGL window
           (TODO: If there is already an EGLConfig associated with win)
      EGL_SUCCESS                   Function succeeded.

   if more than one condition holds, the first error is generated.

   Return value is EGL_NO_SURFACE or an EGLSurface handle which is valid until the EGL session ends or
   eglDestroySurface is called.

   Invariants preserved:

   (CLIENT_PROCESS_STATE_SURFACES)
   (CLIENT_PROCESS_STATE_NEXT_SURFACE)

   Invariants used:

   -
*/

EGLAPI EGLSurface EGLAPIENTRY eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config,
               const EGLint *attrib_list)
{
   CLIENT_THREAD_STATE_T *thread;
   CLIENT_PROCESS_STATE_T *process;
   EGLSurface result;

   if (CLIENT_LOCK_AND_GET_STATES(dpy, &thread, &process))
   {
      if ((int)(size_t)config < 1 || (int)(size_t)config > EGL_MAX_CONFIGS) {
         thread->error = EGL_BAD_CONFIG;
         result = EGL_NO_SURFACE;
      } else {
         int width = 0;
         int height = 0;
         bool largest_pbuffer = 0;
         EGLenum texture_format = EGL_NO_TEXTURE;
         EGLenum texture_target = EGL_NO_TEXTURE;
         bool mipmap_texture = EGL_FALSE;
         bool linear = EGL_FALSE;
         bool premult = EGL_FALSE;

         if (!egl_surface_check_attribs(PBUFFER, attrib_list, &linear, &premult, 0, &width, &height, &largest_pbuffer, &texture_format, &texture_target, &mipmap_texture)) {
            thread->error = EGL_BAD_ATTRIBUTE;
            result = EGL_NO_SURFACE;
         } else if (
            (texture_format != EGL_NO_TEXTURE && (width == 0 || height == 0)) ||
            ((texture_format == EGL_NO_TEXTURE) != (texture_target == EGL_NO_TEXTURE)) ||
            !egl_config_bindable((int)(size_t)config - 1, texture_format)
         ) {

         /*
         "In addition, an EGL_BAD_MATCH
         error is generated if any of the following conditions are true:
         - The EGL_TEXTURE_FORMAT attribute is not EGL_NO_TEXTURE, and
         EGL_WIDTH and/or EGL_HEIGHT specify an invalid size (e.g., the texture size
         is not a power of two, and the underlying OpenGL ES implementation does
         not support non-power-of-two textures).
         - The EGL_TEXTURE_FORMAT attribute is EGL_NO_TEXTURE, and
         EGL_TEXTURE_TARGET is something other than EGL_NO_TEXTURE; or,
         EGL_TEXTURE_FORMAT is something other than EGL_NO_TEXTURE, and
         EGL_TEXTURE_TARGET is EGL_NO_TEXTURE."
          */

         /*
         TODO It doesn't seem to explicitly say it in the spec, but I'm also
         generating EGL_BAD_MATCH if the config doesn't support EGL_BIND_TO_TEXTURE_RGB(A)
         and you specify EGL_TEXTURE_FORMAT=EGL_TEXTURE_RGB(A)
         */
            thread->error = EGL_BAD_MATCH;
            result = EGL_NO_SURFACE;
         } else if ((width > EGL_CONFIG_MAX_WIDTH || height > EGL_CONFIG_MAX_HEIGHT) && !largest_pbuffer) {
            /*
               TODO no mention of this in the spec, but clearly we fail if we try to allocate
               an oversize pbuffer without the largest_pbuffer stuff enabled
            */

            thread->error = EGL_BAD_ALLOC;
            result = EGL_NO_SURFACE;
         } else {
            EGL_SURFACE_T *surface = egl_surface_create(
                             (EGLSurface)(size_t)process->next_surface,
                             PBUFFER,
                             linear ? LINEAR : SRGB,
                             premult ? PRE : NONPRE,
                             1,
                             width, height,
                             config,
                             0,
                             PLATFORM_WIN_NONE,
                             largest_pbuffer,
                             true,
                             mipmap_texture,
                             texture_format,
                             texture_target,
                             0, 0);

            if (surface) {
               if (khrn_pointer_map_insert(&process->surfaces, process->next_surface, surface)) {
                  thread->error = EGL_SUCCESS;
                  result = (EGLSurface)(size_t)process->next_surface++;
               } else {
                  thread->error = EGL_BAD_ALLOC;
                  result = EGL_NO_SURFACE;
                  egl_surface_free(surface);
               }
            } else {
               thread->error = EGL_BAD_ALLOC;
               result = EGL_NO_SURFACE;
            }
         }
      }
      CLIENT_UNLOCK();
   }
   else
      result = EGL_NO_SURFACE;

   return result;
}

typedef struct {
   CLIENT_PROCESS_STATE_T *process;
   EGLNativePixmapType pixmap;
   uint32_t pixmap_server_handle[2];
   int is_dup;
} PIXMAP_CHECK_DATA_T;

static void callback_check_duplicate_pixmap(KHRN_POINTER_MAP_T *map, uint32_t key, void *value, void *data)
{
   PIXMAP_CHECK_DATA_T *pixmap_check_data = (PIXMAP_CHECK_DATA_T *)data;
   EGL_SURFACE_T *surface = (EGL_SURFACE_T *)value;

   UNUSED_NDEBUG(map);
   UNUSED_NDEBUG(key);

   vcos_assert(map == &pixmap_check_data->process->surfaces);
   vcos_assert(surface != NULL);
   vcos_assert((uintptr_t)key == (uintptr_t)surface->name);

   if ((surface->type == PIXMAP) && ((pixmap_check_data->pixmap_server_handle[0] || (pixmap_check_data->pixmap_server_handle[1] != (uint32_t)-1)) ?
      /* compare server handles for server-side pixmaps */
      ((surface->pixmap_server_handle[0] == pixmap_check_data->pixmap_server_handle[0]) &&
      (surface->pixmap_server_handle[1] == pixmap_check_data->pixmap_server_handle[1])) :
      /* compare EGLNativePixmapType for client-side pixmaps */
      (!surface->pixmap_server_handle[0] && (surface->pixmap_server_handle[1] == (uint32_t)-1) &&
      (surface->pixmap == pixmap_check_data->pixmap)))) {
      pixmap_check_data->is_dup = 1;
   }
}

EGLAPI EGLSurface EGLAPIENTRY eglCreatePixmapSurface(EGLDisplay dpy, EGLConfig config,
              EGLNativePixmapType pixmap,
              const EGLint *attrib_list)
{
   CLIENT_THREAD_STATE_T *thread;
   CLIENT_PROCESS_STATE_T *process;
   EGLSurface result;

   vcos_log_trace("eglCreatePixmapSurface");

   if (CLIENT_LOCK_AND_GET_STATES(dpy, &thread, &process))
   {
      if ((int)(size_t)config < 1 || (int)(size_t)config > EGL_MAX_CONFIGS) {
         thread->error = EGL_BAD_CONFIG;
         result = EGL_NO_SURFACE;
      } else {
         bool linear = false;
         bool premult = false;

         if (!egl_surface_check_attribs(PIXMAP, attrib_list, &linear, &premult, 0, 0, 0, 0, 0, 0, 0)) {
            thread->error = EGL_BAD_ATTRIBUTE;
            result = EGL_NO_SURFACE;
         } else {
            EGL_SURFACE_T *surface;

            KHRN_IMAGE_WRAP_T image;
            if (!platform_get_pixmap_info(pixmap, &image)) {
               thread->error = EGL_BAD_NATIVE_PIXMAP;
               result = EGL_NO_SURFACE;
            } else {
               uint32_t server_handle[2] = {0, (uint32_t)-1};
               platform_get_pixmap_server_handle(pixmap, server_handle);

#if !EGL_BRCM_global_image
               if (server_handle[1] != -1) {
                  thread->error = EGL_BAD_PARAMETER;
                  result = EGL_NO_SURFACE;
               } else
#endif
               if (image.width > EGL_CONFIG_MAX_WIDTH || image.height > EGL_CONFIG_MAX_HEIGHT) {
                  /* Maybe EGL_BAD_ALLOC might be more appropriate? */
                  thread->error = EGL_BAD_NATIVE_WINDOW;
                  result = EGL_NO_SURFACE;
               } else if (!egl_config_match_pixmap_info((int)(size_t)config - 1, &image) ||
                  !platform_match_pixmap_api_support(pixmap, egl_config_get_api_support((int)(size_t)config - 1))
#if EGL_BRCM_global_image
                  || ((server_handle[1] != (uint32_t)(-1)) && (
                  (!(image.format & IMAGE_FORMAT_LIN) != !linear) ||
                  (!(image.format & IMAGE_FORMAT_PRE) != !premult)))
#endif
                  ) {
                  thread->error = EGL_BAD_MATCH;
                  result = EGL_NO_SURFACE;
               } else {
                  /*
                   * Check that we didn't already use this pixmap in an
                   * earlier call to eglCreatePixmapSurface()
                   */
                  PIXMAP_CHECK_DATA_T pixmap_check_data;
                  pixmap_check_data.process = process;
                  pixmap_check_data.pixmap = pixmap;
                  pixmap_check_data.pixmap_server_handle[0] = 0;
                  pixmap_check_data.pixmap_server_handle[1] = (uint32_t)-1;
                  platform_get_pixmap_server_handle(pixmap, pixmap_check_data.pixmap_server_handle);
                  pixmap_check_data.is_dup = 0;

                  khrn_pointer_map_iterate(&process->surfaces, callback_check_duplicate_pixmap, &pixmap_check_data);

                  if (pixmap_check_data.is_dup) {
                     thread->error = EGL_BAD_ALLOC;
                     result = EGL_NO_SURFACE;
                  } else {
                     surface = egl_surface_create(
                                   (EGLSurface)(size_t)process->next_surface,
                                   PIXMAP,
                                   linear ? LINEAR : SRGB,
                                   premult ? PRE : NONPRE,
                                   1,
                                   image.width, image.height,
                                   config,
                                   0,
                                   PLATFORM_WIN_NONE,
                                   false,
                                   false,
                                   false,
                                   EGL_NO_TEXTURE,
                                   EGL_NO_TEXTURE,
                                   pixmap, ((server_handle[0] == 0) && (server_handle[1] == (uint32_t)(-1))) ? NULL : server_handle);

                     if (surface) {
                        if (khrn_pointer_map_insert(&process->surfaces, process->next_surface, surface)) {
                           thread->error = EGL_SUCCESS;
                           result = (EGLSurface)(size_t)process->next_surface++;
                        } else {
                           thread->error = EGL_BAD_ALLOC;
                           result = EGL_NO_SURFACE;
                           egl_surface_free(surface);
                        }
                     } else {
                        thread->error = EGL_BAD_ALLOC;
                        result = EGL_NO_SURFACE;
                     }
                  }
               }
            }
         }
      }
      CLIENT_UNLOCK();
   }
   else
      result = EGL_NO_SURFACE;

   return result;
}

//TODO: if this is a pixmap surface, should we make sure the pixmap gets
//updated?
//TODO: should this be a blocking call to the server, so that we know the EGL surface is really
//destroyed before subsequently destroying the associated window?
//TODO: is it safe for asynchronous swap notifications to come back after the surface has been
//destroyed, or do we need to wait for them? (and how?)
EGLAPI EGLBoolean EGLAPIENTRY eglDestroySurface(EGLDisplay dpy, EGLSurface surf)
{
   CLIENT_THREAD_STATE_T *thread;
   CLIENT_PROCESS_STATE_T *process;
   EGLBoolean result;

   vcos_log_trace("eglDestroySurface: surf=%d.\n calling CLIENT_LOCK_AND_GET_STATES...", (int)surf);

   if (CLIENT_LOCK_AND_GET_STATES(dpy, &thread, &process))
   {
      EGL_SURFACE_T *surface;

      thread->error = EGL_SUCCESS;

      vcos_log_trace("eglDestroySurface: calling client_egl_get_surface...");
      surface = client_egl_get_surface(thread, process, surf);

      if (surface) {
         surface->is_destroyed = true;
         khrn_pointer_map_delete(&process->surfaces, (uint32_t)(uintptr_t)surf);
         vcos_log_trace("eglDestroySurface: calling egl_surface_maybe_free...");
         egl_surface_maybe_free(surface);
      }

      result = (thread->error == EGL_SUCCESS ? EGL_TRUE : EGL_FALSE );

      CLIENT_UNLOCK();
   } else
      result = EGL_FALSE;

   vcos_log_trace("eglDestroySurface: end");
   return result;
}

EGLAPI EGLBoolean EGLAPIENTRY eglQuerySurface(EGLDisplay dpy, EGLSurface surf,
            EGLint attribute, EGLint *value)
{
   CLIENT_THREAD_STATE_T *thread;
   CLIENT_PROCESS_STATE_T *process;
   EGLBoolean result;

   if (CLIENT_LOCK_AND_GET_STATES(dpy, &thread, &process))
   {
      EGL_SURFACE_T *surface;

      thread->error = EGL_SUCCESS;

      surface = client_egl_get_locked_surface(thread, process, surf);

      if (surface) {
#if EGL_KHR_lock_surface
         switch (attribute)
         {
         case EGL_BITMAP_POINTER_KHR:
         case EGL_BITMAP_PITCH_KHR:
         case EGL_BITMAP_ORIGIN_KHR:
         case EGL_BITMAP_PIXEL_RED_OFFSET_KHR:
         case EGL_BITMAP_PIXEL_GREEN_OFFSET_KHR:
         case EGL_BITMAP_PIXEL_BLUE_OFFSET_KHR:
         case EGL_BITMAP_PIXEL_ALPHA_OFFSET_KHR:
         case EGL_BITMAP_PIXEL_LUMINANCE_OFFSET_KHR:
            thread->error = egl_surface_get_mapped_buffer_attrib(surface, attribute, value);

            CLIENT_UNLOCK();
            return (thread->error == EGL_SUCCESS ? EGL_TRUE : EGL_FALSE );
         default:
            /* Other attributes can only be queried if the surface is unlocked */
            if (surface->is_locked) {
               thread->error = EGL_BAD_ACCESS;
               CLIENT_UNLOCK();
               return EGL_FALSE;
            }
         }
#endif
         if (!egl_surface_get_attrib(surface, attribute, value))
            thread->error = EGL_BAD_ATTRIBUTE;
      }

      result = (thread->error == EGL_SUCCESS ? EGL_TRUE : EGL_FALSE );

      CLIENT_UNLOCK();
   } else
      result = EGL_FALSE;

   return result;
}

EGLAPI EGLBoolean EGLAPIENTRY eglBindAPI(EGLenum api)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   switch (api) {
   case EGL_OPENVG_API:
   case EGL_OPENGL_ES_API:
      thread->bound_api = api;

      thread->error = EGL_SUCCESS;
      return EGL_TRUE;
   default:
      thread->error = EGL_BAD_PARAMETER;
      return EGL_FALSE;
   }
}

EGLAPI EGLenum EGLAPIENTRY eglQueryAPI(void)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   return thread->bound_api;
}

EGLAPI EGLBoolean EGLAPIENTRY eglWaitClient(void)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   //TODO: "If the surface associated with the calling thread's current context is no
   //longer valid, EGL_FALSE is returned and an EGL_BAD_CURRENT_SURFACE error is
   //generated".

   (void) RPC_INT_RES(RPC_CALL2_RES(eglIntFlushAndWait_impl,
                 thread,
                 EGLINTFLUSHANDWAIT_ID,
                 RPC_UINT(thread->bound_api == EGL_OPENGL_ES_API),
                 RPC_UINT(thread->bound_api == EGL_OPENVG_API)));   // return unimportant - read is just to cause blocking

   if (thread->bound_api == EGL_OPENGL_ES_API)
      egl_gl_flush_callback(true);
   else
      egl_vg_flush_callback(true);

   thread->error = EGL_SUCCESS;
   return EGL_TRUE;
}

//TODO: update pixmap surfaces?
EGLAPI EGLBoolean EGLAPIENTRY eglReleaseThread(void)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   bool destroy = false;

   vcos_log_trace("eglReleaseThread start.");

   CLIENT_LOCK();

   {
      CLIENT_PROCESS_STATE_T *process = CLIENT_GET_PROCESS_STATE();

      if (process) {
         egl_current_release(process, &thread->opengl);
         egl_current_release(process, &thread->openvg);

#ifdef RPC_LIBRARY
         /* TODO: not thread safe */
         const KHRONOS_FUNC_TABLE_T *func_table = khronos_server_lock_func_table(client_library_get_connection());
         if (func_table) {
            func_table->khrn_misc_rpc_flush_impl();
         }
         khronos_server_unlock_func_table();
#else
         RPC_FLUSH(thread);
#endif

#ifndef RPC_DIRECT_MULTI
			//move it to khronos_exit()
         client_try_unload_server(process);
#endif

         thread->error = EGL_SUCCESS;
         destroy = true;
      }
   }

   CLIENT_UNLOCK();

   if (destroy)
      platform_hint_thread_finished();

   vcos_log_trace("eglReleaseThread end.");

   return EGL_TRUE;

   //TODO free thread state?
}

EGLAPI EGLSurface EGLAPIENTRY eglCreatePbufferFromClientBuffer(
         EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer,
         EGLConfig config, const EGLint *attrib_list)
{
   CLIENT_THREAD_STATE_T *thread;
   CLIENT_PROCESS_STATE_T *process;
   EGLSurface result;

   if (CLIENT_LOCK_AND_GET_STATES(dpy, &thread, &process))
   {
#ifndef NO_OPENVG
      if (buftype != EGL_OPENVG_IMAGE) {
         thread->error = EGL_BAD_PARAMETER;
         result = EGL_NO_SURFACE;
      } else if ((int)(size_t)config < 1 || (int)(size_t)config > EGL_MAX_CONFIGS) {
         thread->error = EGL_BAD_CONFIG;
         result = EGL_NO_SURFACE;
      } else {
         bool largest_pbuffer = 0;
         EGLenum texture_format = EGL_NO_TEXTURE;
         EGLenum texture_target = EGL_NO_TEXTURE;
         bool mipmap_texture = EGL_FALSE;

         /* Ignore the width and height as specified by attrib_list */
         /* Also ignore linear and premult */
         if (!egl_surface_check_attribs(PBUFFER, attrib_list, 0, 0, 0, 0, 0, &largest_pbuffer, &texture_format, &texture_target, &mipmap_texture)) {
            thread->error = EGL_BAD_ATTRIBUTE;
            result = EGL_NO_SURFACE;
         } else if (
            (texture_format == EGL_NO_TEXTURE) != (texture_target == EGL_NO_TEXTURE) ||
            !egl_config_bindable((int)(size_t)config - 1, texture_format)
         ) {

         /*
         "In addition, an EGL_BAD_MATCH
         error is generated if any of the following conditions are true:
         - The EGL_TEXTURE_FORMAT attribute is not EGL_NO_TEXTURE, and
         EGL_WIDTH and/or EGL_HEIGHT specify an invalid size (e.g., the texture size
         is not a power of two, and the underlying OpenGL ES implementation does
         not support non-power-of-two textures).
         - The EGL_TEXTURE_FORMAT attribute is EGL_NO_TEXTURE, and
         EGL_TEXTURE_TARGET is something other than EGL_NO_TEXTURE; or,
         EGL_TEXTURE_FORMAT is something other than EGL_NO_TEXTURE, and
         EGL_TEXTURE_TARGET is EGL_NO_TEXTURE."
          */

         /*
         It doesn't seem to explicitly say it in the spec, but I'm also
         generating EGL_BAD_MATCH if the config doesn't support EGL_BIND_TO_TEXTURE_RGB(A)
         and you specify EGL_TEXTURE_FORMAT=EGL_TEXTURE_RGB(A)
         */
            thread->error = EGL_BAD_MATCH;
            result = EGL_NO_SURFACE;
         } else {
            EGLint error;
            EGL_SURFACE_T *surface = egl_surface_from_vg_image(
                       (VGImage)(size_t)buffer,
                       (EGLSurface)(size_t)process->next_surface,
                       config,
                       largest_pbuffer,
                       mipmap_texture,
                       texture_format,
                       texture_target,
                       &error);

            if (surface) {
               if (khrn_pointer_map_insert(&process->surfaces, process->next_surface, surface)) {
                  thread->error = EGL_SUCCESS;
                  result = (EGLSurface)(size_t)process->next_surface++;
               } else {
                  thread->error = EGL_BAD_ALLOC;
                  result = EGL_NO_SURFACE;
                  egl_surface_free(surface);
               }
            } else {
               thread->error = error;
               result = EGL_NO_SURFACE;
            }
         }
      }
#else
      UNUSED(buftype);
      UNUSED(buffer);
      UNUSED(config);
      UNUSED(attrib_list);

      thread->error = EGL_BAD_PARAMETER;
      result = EGL_NO_SURFACE;
#endif /* NO_OPENVG */
      CLIENT_UNLOCK();
   }
   else
      result = EGL_NO_SURFACE;

   return result;
}

EGLAPI EGLBoolean EGLAPIENTRY eglSurfaceAttrib(EGLDisplay dpy, EGLSurface surf,
             EGLint attribute, EGLint value)
{
   CLIENT_THREAD_STATE_T *thread;
   CLIENT_PROCESS_STATE_T *process;
   EGLBoolean result;

   if (CLIENT_LOCK_AND_GET_STATES(dpy, &thread, &process))
   {
      EGL_SURFACE_T *surface;

      thread->error = EGL_SUCCESS;

      surface = client_egl_get_surface(thread, process, surf);

      if (surface)
         thread->error = egl_surface_set_attrib(surface, attribute, value);

      result = (thread->error == EGL_SUCCESS ? EGL_TRUE : EGL_FALSE );
      CLIENT_UNLOCK();
   } else
      result = EGL_FALSE;

   return result;
}

EGLAPI EGLBoolean EGLAPIENTRY eglBindTexImage(EGLDisplay dpy, EGLSurface surf, EGLint buffer)
{
   CLIENT_THREAD_STATE_T *thread;
   CLIENT_PROCESS_STATE_T *process;
   EGLBoolean result;

   if (CLIENT_LOCK_AND_GET_STATES(dpy, &thread, &process))
   {
//TODO: is behaviour correct if there is no current rendering context?
      EGL_SURFACE_T *surface;

      thread->error = EGL_SUCCESS;

      surface = client_egl_get_surface(thread, process, surf);

      if (surface) {
         if (surface->texture_format != EGL_NO_TEXTURE) {
            if (buffer == EGL_BACK_BUFFER) {
               if (surface->type == PBUFFER && surface->texture_target == EGL_TEXTURE_2D) {
                  result = (EGLBoolean) RPC_BOOLEAN_RES(RPC_CALL1_RES(eglIntBindTexImage_impl,
                     thread,
                     EGLINTBINDTEXIMAGE_ID,
                     surface->serverbuffer));
                  if (result != EGL_TRUE) {
                     // If buffer is already bound to a texture then an
                     // EGL_BAD_ACCESS error is returned.
                     // But we don't know whether it is or not until we call
                     // the server.
                     thread->error = EGL_BAD_ACCESS;
                  }
               } else {
                  thread->error = EGL_BAD_SURFACE;
                  result = EGL_FALSE;
               }
            } else {
               thread->error = EGL_BAD_PARAMETER;
               result = EGL_FALSE;
            }
         } else {
            thread->error = EGL_BAD_MATCH;
            result = EGL_FALSE;
         }
      }

      result = (thread->error == EGL_SUCCESS ? EGL_TRUE : EGL_FALSE );
      CLIENT_UNLOCK();
   } else
      result = EGL_FALSE;

   return result;
}

EGLAPI EGLBoolean EGLAPIENTRY eglReleaseTexImage(EGLDisplay dpy, EGLSurface surf, EGLint buffer)
{
   CLIENT_THREAD_STATE_T *thread;
   CLIENT_PROCESS_STATE_T *process;
   EGLBoolean result;

   if (CLIENT_LOCK_AND_GET_STATES(dpy, &thread, &process))
   {
      EGL_SURFACE_T *surface;

      thread->error = EGL_SUCCESS;

      surface = client_egl_get_surface(thread, process, surf);

      if (surface) {
         if (surface->texture_format != EGL_NO_TEXTURE) {
            if (buffer == EGL_BACK_BUFFER) {
               if (surface->type == PBUFFER) {
                  //TODO: not a "bound" pbuffer?
                  RPC_CALL1(eglIntReleaseTexImage_impl,
                     thread,
                     EGLINTRELEASETEXIMAGE_ID,
                     surface->serverbuffer);
               } else {
                  thread->error = EGL_BAD_SURFACE;
                  result = EGL_FALSE;
               }
            } else {
               thread->error = EGL_BAD_PARAMETER;
               result = EGL_FALSE;
            }
         } else {
            thread->error = EGL_BAD_MATCH;
            result = EGL_FALSE;
         }
      }

      result = (thread->error == EGL_SUCCESS ? EGL_TRUE : EGL_FALSE );
      CLIENT_UNLOCK();
   } else
      result = EGL_FALSE;

   return result;
}

EGLAPI EGLBoolean EGLAPIENTRY eglSwapInterval(EGLDisplay dpy, EGLint interval)
{
   CLIENT_THREAD_STATE_T *thread;
   CLIENT_PROCESS_STATE_T *process;
   EGLBoolean result;

   if (CLIENT_LOCK_AND_GET_STATES(dpy, &thread, &process))
   {
      EGL_CURRENT_T *current;
      EGL_SURFACE_T *surface;

      /* the spec says "the window associated with the current context". it
       * doesn't explicitly say "the current context for the current rendering
       * api" (which it does in most other places), but i'm assuming that's what
       * it means */

      if (thread->bound_api == EGL_OPENVG_API)
         current = &thread->openvg;
      else
         current = &thread->opengl;

      surface = current->draw;

      if (surface) {
         if (surface->type == WINDOW) {
            if (interval < EGL_CONFIG_MIN_SWAP_INTERVAL)
               interval = EGL_CONFIG_MIN_SWAP_INTERVAL;
            if (interval > EGL_CONFIG_MAX_SWAP_INTERVAL)
               interval = EGL_CONFIG_MAX_SWAP_INTERVAL;

            surface->swap_interval = (uint32_t) interval;
         }

         RPC_CALL2(eglIntSwapInterval_impl,
            thread,
            EGLINTSWAPINTERVAL_ID,
            surface->serverbuffer,
            surface->swap_interval);

         /* TODO: should we raise an error if it's not a window
          * surface, or silently ignore it?
          */
         thread->error = EGL_SUCCESS;
         result = EGL_TRUE;
      } else {
         /*
         "If there is no current context
         on the calling thread, a EGL BAD CONTEXT error is generated. If there is no surface
         bound to the current context, a EGL BAD SURFACE error is generated."

         TODO
         This doesn't make sense to me - the current context always has surfaces
         bound to it, so which error do we raise?
         */
         thread->error = EGL_BAD_SURFACE;
         result = EGL_FALSE;
      }
      CLIENT_UNLOCK();
   }
   else
      result = EGL_FALSE;

   return result;
}

EGLAPI EGLContext EGLAPIENTRY eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_ctx, const EGLint *attrib_list)
{
   CLIENT_THREAD_STATE_T *thread;
   CLIENT_PROCESS_STATE_T *process;
   EGLContext result;

vcos_log_trace("eglCreateContext start");

   if (CLIENT_LOCK_AND_GET_STATES(dpy, &thread, &process))
   {
      if ((int)(size_t)config < 1 || (int)(size_t)config > EGL_MAX_CONFIGS) {
         thread->error = EGL_BAD_CONFIG;
         result = EGL_NO_CONTEXT;
      } else {
         EGLint max_version = (EGLint) (thread->bound_api == EGL_OPENGL_ES_API ? 2 : 1);
         EGLint version = 1;

         if (!egl_context_check_attribs(attrib_list, max_version, &version)) {
            thread->error = EGL_BAD_ATTRIBUTE;
            result = EGL_NO_CONTEXT;
         } else if (!(egl_config_get_api_support((int)(intptr_t)config - 1) &
            ((thread->bound_api == EGL_OPENVG_API) ? EGL_OPENVG_BIT :
            ((version == 1) ? EGL_OPENGL_ES_BIT : EGL_OPENGL_ES2_BIT)))) {
            thread->error = EGL_BAD_CONFIG;
            result = EGL_NO_CONTEXT;
         } else {
            EGL_CONTEXT_T *share_context;

            if (share_ctx != EGL_NO_CONTEXT) {
               share_context = client_egl_get_context(thread, process, share_ctx);

               if (share_context) {
                  if ((share_context->type == OPENVG && thread->bound_api != EGL_OPENVG_API) ||
                     (share_context->type != OPENVG && thread->bound_api == EGL_OPENVG_API)) {
                     thread->error = EGL_BAD_MATCH;
                     share_context = NULL;
                  }
               } else {
                  thread->error = EGL_BAD_CONTEXT;
               }
            } else {
               share_context = NULL;
            }

            if (share_ctx == EGL_NO_CONTEXT || share_context) {
               EGL_CONTEXT_T *context;
               EGL_CONTEXT_TYPE_T type;

#ifndef NO_OPENVG
               if (thread->bound_api == EGL_OPENVG_API)
                  type = OPENVG;
               else
#endif
                  if (version == 1)
                     type = OPENGL_ES_11;
                  else
                     type = OPENGL_ES_20;

               context = egl_context_create(
                                share_context,
                                (EGLContext)(size_t)process->next_context,
                                dpy, config, type);

               if (context) {
                  if (khrn_pointer_map_insert(&process->contexts, process->next_context, context)) {
                     thread->error = EGL_SUCCESS;
                     result = (EGLContext)(size_t)process->next_context++;
                  } else {
                     thread->error = EGL_BAD_ALLOC;
                     result = EGL_NO_CONTEXT;
                     egl_context_term(context);
                     khrn_platform_free(context);
                  }
               } else {
                  thread->error = EGL_BAD_ALLOC;
                  result = EGL_NO_CONTEXT;
               }
            } else {
               /* thread->error set above */
               result = EGL_NO_CONTEXT;
            }
         }
      }
      CLIENT_UNLOCK();
   }
   else
      result = EGL_NO_CONTEXT;

   vcos_log_trace("eglCreateContext end");

   return result;
}

EGLAPI EGLBoolean EGLAPIENTRY eglDestroyContext(EGLDisplay dpy, EGLContext ctx)
{
   CLIENT_THREAD_STATE_T *thread;
   CLIENT_PROCESS_STATE_T *process;
   EGLBoolean result;

   vcos_log_trace("eglDestroyContext ctx=%d.", (int)ctx);

   if (CLIENT_LOCK_AND_GET_STATES(dpy, &thread, &process))
   {
      EGL_CONTEXT_T *context;

      thread->error = EGL_SUCCESS;

      context = client_egl_get_context(thread, process, ctx);

      if (context) {
         context->is_destroyed = true;
         khrn_pointer_map_delete(&process->contexts, (uint32_t)(uintptr_t)ctx);
         egl_context_maybe_free(context);
      }
      result = thread->error == EGL_SUCCESS;
      CLIENT_UNLOCK();
   }
   else
      result = EGL_FALSE;

   return result;
}

static void egl_current_release(CLIENT_PROCESS_STATE_T *process, EGL_CURRENT_T *current)
{
   if (current->context) {
      EGL_CONTEXT_T *context = current->context;
      vcos_assert(context->is_current);
      context->is_current = false;
      context->renderbuffer = EGL_NONE;
      egl_context_set_callbacks(context, NULL, NULL, NULL, NULL);

      current->context = 0;

      egl_context_maybe_free(context);

      vcos_assert(process->context_current_count > 0);
      process->context_current_count--;
   }
   if (current->draw) {
      EGL_SURFACE_T *draw = current->draw;

      vcos_assert(draw->context_binding_count > 0);
      draw->context_binding_count--;

      current->draw = 0;

      egl_surface_maybe_free(draw);
   }
   if (current->read) {
      EGL_SURFACE_T *read = current->read;

      vcos_assert(read->context_binding_count > 0);
      read->context_binding_count--;

      current->read = 0;

      egl_surface_maybe_free(read);
   }
}

static void set_color_data(EGL_SURFACE_ID_T surface_id, KHRN_IMAGE_WRAP_T *image)
{
   int line_size = (image->stride < 0) ? -image->stride : image->stride;
   int lines = KHDISPATCH_WORKSPACE_SIZE / line_size;
   int offset = 0;
   int height = image->height;

   if (khrn_image_is_brcm1(image->format))
      lines &= ~63;

   vcos_assert(lines > 0);

   while (height > 0) {
      int batch = _min(lines, height);
#ifndef RPC_DIRECT
      uint32_t len = batch * line_size;
#endif

      CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
      int adjusted_offset = (image->stride < 0) ? (offset + (batch - 1)) : offset;

      RPC_CALL7_IN_BULK(eglIntSetColorData_impl,
         thread,
         EGLINTSETCOLORDATA_ID,
         surface_id,
         image->format,
         image->width,
         batch,
         image->stride,
         offset,
         (const char *)image->storage + adjusted_offset * image->stride,
         len);

      offset += batch;
      height -= batch;
   }
}

static void send_pixmap(EGL_SURFACE_T *surface)
{
   if (surface && surface->type == PIXMAP && !surface->pixmap_server_handle[0] && (surface->pixmap_server_handle[1] == (uint32_t)-1) && !surface->server_owned) {
      KHRN_IMAGE_WRAP_T image;

      if (!platform_get_pixmap_info(surface->pixmap, &image)) {
         (void)vcos_verify(0); /* the pixmap has become invalid... */
         return;
      }

      set_color_data(surface->serverbuffer, &image);

      platform_send_pixmap_completed(surface->pixmap);

      surface->server_owned = true;

      khrn_platform_release_pixmap_info(surface->pixmap, &image);
   }
}

void egl_gl_render_callback(void)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   CLIENT_LOCK();

   send_pixmap(thread->opengl.draw);

   CLIENT_UNLOCK();
}

void egl_vg_render_callback(void)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   CLIENT_LOCK();

   send_pixmap(thread->openvg.draw);

   CLIENT_UNLOCK();
}

static void get_color_data(EGL_SURFACE_ID_T surface_id, KHRN_IMAGE_WRAP_T *image)
{
   int line_size = (image->stride < 0) ? -image->stride : image->stride;
   int lines = KHDISPATCH_WORKSPACE_SIZE / line_size;
   int offset = 0;
   int height = image->height;

   if (khrn_image_is_brcm1(image->format))
      lines &= ~63;

   vcos_assert(lines > 0);

   while (height > 0) {
      int batch = _min(lines, height);

      CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
      int adjusted_offset = (image->stride < 0) ? (offset + (batch - 1)) : offset;

      RPC_CALL7_OUT_BULK(eglIntGetColorData_impl,
         thread,
         EGLINTGETCOLORDATA_ID,
         surface_id,
         image->format,
         image->width,
         batch,
         image->stride,
         offset,
         (char *)image->storage + adjusted_offset * image->stride);

      offset += batch;
      height -= batch;
   }
}

static void retrieve_pixmap(EGL_SURFACE_T *surface, bool wait)
{
   UNUSED(wait);

   /*TODO: currently we always wait */
   if (surface && surface->type == PIXMAP && !surface->pixmap_server_handle[0] && (surface->pixmap_server_handle[1] == (uint32_t)-1) && surface->server_owned) {
      KHRN_IMAGE_WRAP_T image;

      if (!platform_get_pixmap_info(surface->pixmap, &image)) {
         (void)vcos_verify(0); /* the pixmap has become invalid... */
         return;
      }

      get_color_data(surface->serverbuffer, &image);

//Do any platform specific syncronisation or notification of modification
      platform_retrieve_pixmap_completed(surface->pixmap);

      surface->server_owned = false;
      khrn_platform_release_pixmap_info(surface->pixmap, &image);
   }
}

void egl_gl_flush_callback(bool wait)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   CLIENT_LOCK();

   retrieve_pixmap(thread->opengl.draw, wait);

   CLIENT_UNLOCK();
}

void egl_vg_flush_callback(bool wait)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   CLIENT_LOCK();

   retrieve_pixmap(thread->openvg.draw, wait);

   CLIENT_UNLOCK();
}

static bool context_and_surface_are_compatible(EGL_CONTEXT_T *context, EGL_SURFACE_T *surface)
{
   /*
      from section 2.2 of the (1.3) spec, a context and surface are compatible
      if:
      1) they support the same type of color buffer (rgb or luminance). this is
         trivially true for us as we only support rgb color buffers
      2) they have color buffers and ancillary buffers of the same depth
      3) the surface was created with respect to an EGLConfig supporting client
         api rendering of the same type as the api type of the context
      4) they were created with respect to the same EGLDisplay. this is
         trivially true for us as we only have one EGLDisplay
   */

   uint32_t api_type = 0;
   switch (context->type) {
   case OPENGL_ES_11: api_type = EGL_OPENGL_ES_BIT; break;
   case OPENGL_ES_20: api_type = EGL_OPENGL_ES2_BIT; break;
   case OPENVG:       api_type = EGL_OPENVG_BIT; break;
   default:           UNREACHABLE();
   }

   return
      egl_config_bpps_match((int)(intptr_t)context->configname - 1, (int)(intptr_t)surface->config - 1) && /* (2) */
      (egl_config_get_api_support((int)(intptr_t)surface->config - 1) & api_type); /* (3) */
}

static bool egl_current_set(CLIENT_PROCESS_STATE_T *process, CLIENT_THREAD_STATE_T *thread, EGL_CURRENT_T *current, EGL_CONTEXT_T *context, EGL_SURFACE_T *draw, EGL_SURFACE_T *read)
{
   bool result = false;

   UNUSED(process);

   if (context->is_current && context->thread != thread) {
      // Fail - context is current to some other thread
      thread->error = EGL_BAD_ACCESS;
   } else if (draw->context_binding_count && draw->thread != thread) {
      // Fail - draw surface is bound to context which is current to another thread
      thread->error = EGL_BAD_ACCESS;
   } else if (read->context_binding_count && read->thread != thread) {
      // Fail - read surface is bound to context which is current to another thread
      thread->error = EGL_BAD_ACCESS;
   } else if (!context_and_surface_are_compatible(context, draw)) {
      // Fail - draw surface is not compatible with context
      thread->error = EGL_BAD_MATCH;
   } else if (!context_and_surface_are_compatible(context, read)) {
      // Fail - read surface is not compatible with context
      thread->error = EGL_BAD_MATCH;
   } else {
      egl_current_release(process, current);

      context->is_current = true;
      context->thread = thread;

      /* TODO: GLES supposedly doesn't support single-buffered rendering. Should we take this into account? */
      context->renderbuffer = egl_surface_get_render_buffer(draw);

      // Check surfaces are not bound to a different thread, and increase their reference count

      draw->thread = thread;
      draw->context_binding_count++;

      read->thread = thread;
      read->context_binding_count++;

      current->context = context;
      current->draw = draw;
      current->read = read;

      process->context_current_count++;

      result = true;
   }
   if (draw->type == PIXMAP) {
      egl_context_set_callbacks(context, egl_gl_render_callback, egl_gl_flush_callback, egl_vg_render_callback, egl_vg_flush_callback);
   } else {
      egl_context_set_callbacks(context, NULL,NULL, NULL, NULL);
   }

   return result;
}

static void flush_current_api(CLIENT_THREAD_STATE_T *thread)
{
   RPC_CALL2(eglIntFlush_impl,
                 thread,
                 EGLINTFLUSH_ID,
                 RPC_UINT(thread->bound_api == EGL_OPENGL_ES_API),
                 RPC_UINT(thread->bound_api == EGL_OPENVG_API));
   RPC_FLUSH(thread);

   if (thread->bound_api == EGL_OPENGL_ES_API)
      egl_gl_flush_callback(false);
   else
      egl_vg_flush_callback(false);
}

EGLAPI EGLBoolean EGLAPIENTRY eglMakeCurrent(EGLDisplay dpy, EGLSurface dr, EGLSurface rd, EGLContext ctx)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   EGLBoolean result;
   CLIENT_PROCESS_STATE_T *process = NULL; /* init to avoid warnings */

   CLIENT_LOCK();

   vcos_log_trace("Actual eglMakeCurrent %d %d %x", (int)ctx, (int)dr, (unsigned int) thread->error);

   /*
      check whether we are trying to release the current context
      Note that we can do this even if the display isn't initted.
   */

   if (dr == EGL_NO_SURFACE && rd == EGL_NO_SURFACE && ctx == EGL_NO_CONTEXT) {
      process = client_egl_get_process_state(thread, dpy, EGL_FALSE);

      if (process) {
         /* spec says we should flush in this case */
         flush_current_api(thread);

         egl_current_release(process,
            (thread->bound_api == EGL_OPENVG_API) ? &thread->openvg : &thread->opengl);

         client_send_make_current(thread);

         client_try_unload_server(process);

         thread->error = EGL_SUCCESS;
         result = EGL_TRUE;
      } else {
         result = EGL_FALSE;
      }
   } else  if (dr == EGL_NO_SURFACE || rd == EGL_NO_SURFACE || ctx == EGL_NO_CONTEXT) {
      thread->error = EGL_BAD_MATCH;
      result = EGL_FALSE;
   } else {
      /*
         get display
      */

      process = client_egl_get_process_state(thread, dpy, EGL_TRUE);

      if (!process)
         result = EGL_FALSE;
      else {
         /*
            get context
         */

         EGL_CONTEXT_T *context = client_egl_get_context(thread, process, ctx);

         if (!context) {
            result = EGL_FALSE;
         }  else {

            /*
               get surfaces
            */

            EGL_SURFACE_T *draw = client_egl_get_surface(thread, process, dr);
            EGL_SURFACE_T *read = client_egl_get_surface(thread, process, rd);

            if (!draw || !read) {
               result = EGL_FALSE;
            } else if (context->type == OPENVG && dr != rd) {
               thread->error = EGL_BAD_MATCH;   //TODO: what error are we supposed to return here?
               result = EGL_FALSE;
            } else {
               EGL_CURRENT_T *current;

               if (context->type == OPENVG)
                  current = &thread->openvg;
               else
                  current = &thread->opengl;

               if (!egl_current_set(process, thread, current, context, draw, read))
                  result = EGL_FALSE;
               else {
                  client_send_make_current(thread);

                  thread->error = EGL_SUCCESS;
                  result = EGL_TRUE;
               }
            }
         }
      }
   }

   CLIENT_UNLOCK();

   vcos_log_trace("Actual eglMakeCurrent end %d %d %d %x", (int)ctx, (int)dr, result, (unsigned int)thread->error);

   return result;
}

EGLAPI EGLContext EGLAPIENTRY eglGetCurrentContext(void)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   EGLContext result;

   CLIENT_LOCK();

   {
      EGL_CURRENT_T *current;

      if (thread->bound_api == EGL_OPENVG_API)
         current = &thread->openvg;
      else
         current = &thread->opengl;

      if (!current->context)
         result = EGL_NO_CONTEXT;
      else
         result = current->context->name;

      thread->error = EGL_SUCCESS;
   }

   CLIENT_UNLOCK();

   return result;
}

EGLAPI EGLSurface EGLAPIENTRY eglGetCurrentSurface(EGLint readdraw)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   EGLSurface result;

   CLIENT_LOCK();

   {
      EGL_CURRENT_T *current;
      EGL_SURFACE_T *surface;

      if (thread->bound_api == EGL_OPENVG_API)
         current = &thread->openvg;
      else
         current = &thread->opengl;

      switch (readdraw) {
      case EGL_READ:
         surface = current->read;
         thread->error = EGL_SUCCESS;
         break;
      case EGL_DRAW:
         surface = current->draw;
         thread->error = EGL_SUCCESS;
         break;
      default:
         surface = 0;
         thread->error = EGL_BAD_PARAMETER;
         break;
      }

      if (!surface)
         result = EGL_NO_SURFACE;
      else
         result = surface->name;
   }

   CLIENT_UNLOCK();

   return result;
}

EGLAPI EGLDisplay EGLAPIENTRY eglGetCurrentDisplay(void)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   EGLDisplay result;

   CLIENT_LOCK();

   {
      EGL_CURRENT_T *current;

      if (thread->bound_api == EGL_OPENVG_API)
         current = &thread->openvg;
      else
         current = &thread->opengl;

      if (!current->context)
         result = EGL_NO_DISPLAY;
      else
         result = current->context->display;

      thread->error = EGL_SUCCESS;
   }

   CLIENT_UNLOCK();

   return result;
}

EGLAPI EGLBoolean EGLAPIENTRY eglQueryContext(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value)
{
   CLIENT_THREAD_STATE_T *thread;
   CLIENT_PROCESS_STATE_T *process;
   EGLBoolean result;

   if (CLIENT_LOCK_AND_GET_STATES(dpy, &thread, &process))
   {
      if (!value) {
         thread->error = EGL_BAD_PARAMETER;
         result = EGL_FALSE;
      } else {
         EGL_CONTEXT_T *context;

         thread->error = EGL_SUCCESS;

         context = client_egl_get_context(thread, process, ctx);

         if (context) {
            if (!egl_context_get_attrib(context, attribute, value))
               thread->error = EGL_BAD_ATTRIBUTE;

         }
         result = thread->error == EGL_SUCCESS;
      }
      CLIENT_UNLOCK();
   }
   else
      result = EGL_FALSE;

   return result;
}

EGLAPI EGLBoolean EGLAPIENTRY eglWaitGL(void)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   EGLBoolean result;

   //TODO: "If the surface associated with the calling thread's current context is no
   //longer valid, EGL_FALSE is returned and an EGL_BAD_CURRENT_SURFACE error is
   //generated".
   (void) RPC_INT_RES(RPC_CALL2_RES(eglIntFlushAndWait_impl,
                 thread,
                 EGLINTFLUSHANDWAIT_ID,
                 RPC_UINT(true),
                 RPC_UINT(false)));   // return unimportant - read is just to cause blocking

   egl_gl_flush_callback(true);

   thread->error = EGL_SUCCESS;
   result = EGL_TRUE;

   return result;
}

EGLAPI EGLBoolean EGLAPIENTRY eglWaitNative(EGLint engine)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   EGLBoolean result;

   //TODO: "If the surface associated with the calling thread's current context is no
   //longer valid, EGL_FALSE is returned and an EGL_BAD_CURRENT_SURFACE error is
   //generated".

   if (engine == EGL_CORE_NATIVE_ENGINE) {
   //TODO: currently nothing we can do here
      thread->error = EGL_SUCCESS;
      result = EGL_TRUE;
   } else {
      thread->error = EGL_BAD_PARAMETER;
      result = EGL_FALSE;
   }

   return result;
}

EGLAPI EGLBoolean EGLAPIENTRY eglSwapBuffers(EGLDisplay dpy, EGLSurface surf)
{
#ifdef DIRECT_RENDERING
   /* Wrapper layer shouldn't call eglSwapBuffers */
   UNREACHABLE();
   return EGL_FALSE;
#else
   CLIENT_THREAD_STATE_T *thread;
   CLIENT_PROCESS_STATE_T *process;
   EGLBoolean result;

   vcos_log_trace("eglSwapBuffers start. dpy=%d. surf=%d.", (int)dpy, (int)surf);

   if (CLIENT_LOCK_AND_GET_STATES(dpy, &thread, &process))
   {
      EGL_SURFACE_T *surface;

      thread->error = EGL_SUCCESS;

      surface = client_egl_get_surface(thread, process, surf);

      vcos_log_trace("eglSwapBuffers get surface %x",(int)surface);

      if (surface) {

#if !(EGL_KHR_lock_surface)
         /* Surface to be displayed must be bound to current context and API */
         /* This check is disabled if we have the EGL_KHR_lock_surface extension */
         if (thread->bound_api == EGL_OPENGL_ES_API && surface != thread->opengl.draw && surface != thread->opengl.read
          || thread->bound_api == EGL_OPENVG_API    && surface != thread->openvg.draw) {
            thread->error = EGL_BAD_SURFACE;
         } else
#endif
         {

            if (surface->type == WINDOW) {
               uint32_t width, height, swapchain_count;

               /* the egl spec says eglSwapBuffers is supposed to be a no-op for
                * single-buffered surfaces, but we pass it through as the
                * semantics are potentially useful:
                * - any ops outstanding on the surface are flushed
                * - the surface is resubmitted to the display once the
                *   outstanding ops complete (for displays which have their own
                *   memory, this is useful)
                * - the surface is resized to fit the backing window */

               // We need to check at this point if the surface has resized, and pass
               // size data down to the server.

               width = surface->width;
               height = surface->height;

               platform_get_dimensions(dpy, surface->win,
                     &width, &height, &swapchain_count);

               if((width!=surface->width)||(height!=surface->height)) {
                  uint32_t handle = platform_get_handle(dpy, surface->win);
                  surface->internal_handle = handle;
                  surface->width = width;
                  surface->height = height;
               }

               vcos_log_trace("eglSwapBuffers comparison: %d %d, %d %d",
                        surface->width, surface->base_width, surface->height,
                        surface->base_height);

               /* TODO: raise EGL_BAD_ALLOC if we try to enlarge window and then run out of memory

                  if (surface->width <= surface->base_width && surface->height <= surface->base_height ||
                  surface->width <= surface->base_height && surface->height <= surface->base_width)
                  */
               // We don't call flush_current_api() here because it's only relevant
               // for pixmap surfaces (eglIntSwapBuffers takes care of flushing on
               // the server side).

               platform_surface_update(surface->internal_handle);

               vcos_log_trace("eglSwapBuffers server call");

               RPC_CALL6(eglIntSwapBuffers_impl,
                     thread,
                     EGLINTSWAPBUFFERS_ID,
                     RPC_UINT(surface->serverbuffer),
                     RPC_UINT(surface->width),
                     RPC_UINT(surface->height),
                     RPC_UINT(surface->internal_handle),
                     RPC_UINT(surface->swap_behavior == EGL_BUFFER_PRESERVED ? 1 : 0),
                     RPC_UINT(khrn_platform_get_window_position(surface->win)));

               RPC_FLUSH(thread);

#ifdef ANDROID
               CLIENT_UNLOCK();
               platform_dequeue(dpy, surface->win);
               CLIENT_LOCK();
#else

#  ifdef KHRONOS_EGL_PLATFORM_OPENWFC
               wfc_stream_await_buffer((WFCNativeStreamType) surface->internal_handle);
#  else
#     ifndef RPC_LIBRARY
               if (surface->buffers > 1) {
                  //TODO implement khan (khronos async notification) receiver for linux
#        ifndef RPC_DIRECT_MULTI
                  vcos_log_trace("eglSwapBuffers waiting for semaphore");
                  khronos_platform_semaphore_acquire(&surface->avail_buffers);
#        endif
               }
#     endif // RPC_LIBRARY
#  endif // KHRONOS_EGL_PLATFORM_OPENWFC

#endif   /* ANDROID */

            } else {
#ifdef KHRN_COMMAND_MODE_DISPLAY
//Check for single buffered windows surface (and VG) in which case call vgFlush to allow screen update for command mode screens
               EGL_SURFACE_T *surface = CLIENT_GET_THREAD_STATE()->openvg.draw;
               if (surface->type == WINDOW && surface->buffers==1 && thread->bound_api == EGL_OPENVG_API) {
                  vgFlush();
               }
#endif
            }
            // else do nothing. eglSwapBuffers has no effect on pixmap or pbuffer surfaces
         }
      }

      result = (thread->error == EGL_SUCCESS);
      CLIENT_UNLOCK();
   }
   else
      result = EGL_FALSE;

   vcos_log_trace("eglSwapBuffers end");

   return result;
#endif // DIRECT_RENDERING
}

EGLAPI EGLBoolean EGLAPIENTRY eglCopyBuffers(EGLDisplay dpy, EGLSurface surf, EGLNativePixmapType target)
{
   CLIENT_THREAD_STATE_T *thread;
   CLIENT_PROCESS_STATE_T *process;
   EGLBoolean result;

   if (CLIENT_LOCK_AND_GET_STATES(dpy, &thread, &process))
   {
      EGL_SURFACE_T *surface;

      thread->error = EGL_SUCCESS;

      surface = client_egl_get_surface(thread, process, surf);

      if ((thread->bound_api == EGL_OPENGL_ES_API && surface != thread->opengl.draw && surface != thread->opengl.read)
       || (thread->bound_api == EGL_OPENVG_API    && surface != thread->openvg.draw)) {
         /* Surface to be displayed must be bound to current context and API */
         /* TODO remove this restriction, as we'll need to for eglSwapBuffers? */
         thread->error = EGL_BAD_SURFACE;
      } else if (surface) {
         KHRN_IMAGE_WRAP_T image;

         if (!platform_get_pixmap_info(target, &image)) {
            thread->error = EGL_BAD_NATIVE_PIXMAP;
         } else {
            if (image.width != surface->width || image.height != surface->height) {
               thread->error = EGL_BAD_MATCH;
            } else {
               //Bear in mind it's possible to call eglCopyBuffers on a pixmap
               //surface which will result in the data being transferred twice, onto
               //two different native pixmaps.

               //TODO: flush the other API too?
                  //TODO: is this necessary?
               flush_current_api(thread);

                  get_color_data(surface->serverbuffer, &image);
            }
            khrn_platform_release_pixmap_info(target, &image);
         }
      }

      result = (thread->error == EGL_SUCCESS);
      CLIENT_UNLOCK();
   }
   else
      result = EGL_FALSE;

   return result;
}

//#define EXPORT_DESTROY_BY_PID          // define me to export a utility function which will destroy the resources associated with a given process
#ifdef EXPORT_DESTROY_BY_PID
EGLAPI void EGLAPIENTRY eglDestroyByPidBRCM(EGLDisplay dpy, uint32_t pid_0, uint32_t pid_1)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   EGLBoolean result;

   CLIENT_LOCK();

   {
      CLIENT_PROCESS_STATE_T *process = client_egl_get_process_state(thread, dpy, EGL_TRUE);

      if (!process)
         result = 0;
      else {
         RPC_CALL2(eglIntDestroyByPid_impl,
                   thread,
                   EGLINTDESTROYBYPID_ID,
                   RPC_UINT(pid_0),
                   RPC_UINT(pid_1));

         result = 1;
      }
   }

   CLIENT_UNLOCK();
}
#endif

#ifdef DIRECT_RENDERING
EGLAPI EGLBoolean EGLAPIENTRY eglDirectRenderingPointer(EGLDisplay dpy, EGLSurface surf, void *image)
{
   CLIENT_THREAD_STATE_T *thread;
   CLIENT_PROCESS_STATE_T *process;
   EGLBoolean result = EGL_FALSE;

   if (CLIENT_LOCK_AND_GET_STATES(dpy, &thread, &process))
   {
      EGL_SURFACE_T *surface;

      thread->error = EGL_SUCCESS;

      surface = client_egl_get_surface(thread, process, surf);

      if (surface)
      {
         KHRN_IMAGE_WRAP_T *image_wrap = (KHRN_IMAGE_WRAP_T *)image;
         surface->width = image_wrap->width;
         surface->height = image_wrap->height;
         RPC_CALL6(eglDirectRenderingPointer_impl,
                 thread,
                 EGLDIRECTRENDERINGPOINTER_ID,
                 surface->serverbuffer,
                 (uint32_t)image_wrap->storage,
                 image_wrap->format,
                 image_wrap->width,
                 image_wrap->height,
                 image_wrap->stride);
      }

      result = (thread->error == EGL_SUCCESS ? EGL_TRUE : EGL_FALSE );

      CLIENT_UNLOCK();
   }

   return result;
}
#endif

#if EGL_proc_state_valid
EGLAPI void EGLAPIENTRY eglProcStateValid( EGLDisplay dpy, EGLBoolean *result )
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   CLIENT_LOCK();

   vcos_log_trace("eglProcStateValid dpy=%d", (int)dpy );

   {
      CLIENT_PROCESS_STATE_T *process = client_egl_get_process_state(thread, dpy, EGL_TRUE);

      if (!process) {
         *result = EGL_FALSE;
      }
      else {
         *result = EGL_TRUE;
      }
   }

   CLIENT_UNLOCK();

   vcos_log_trace("eglProcStateValid result=%d", *result );
   return;
}
#endif
