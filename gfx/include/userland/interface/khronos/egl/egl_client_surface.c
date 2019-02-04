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

#define VCOS_LOG_CATEGORY (&egl_client_log_cat)

#include "interface/khronos/common/khrn_int_common.h"

#include "interface/khronos/common/khrn_client_rpc.h"
#include "interface/khronos/common/khrn_client_platform.h"

#include "interface/khronos/egl/egl_client_surface.h"
#include "interface/khronos/egl/egl_client_config.h"

#include "interface/khronos/common/khrn_client.h"

#ifdef KHRONOS_EGL_PLATFORM_OPENWFC
#include "interface/khronos/wf/wfc_int.h"
#include "interface/khronos/wf/wfc_client_stream.h"
#endif

#ifdef RPC_DIRECT
#include "interface/khronos/egl/egl_int_impl.h"
#endif

#include <stdlib.h>

/*
   surface_pool

   cache for a small number of pre-allocated surface objects

   Validity:
   surfaces[i] is valid if allocated & (1<<i)
*/

#define EGL_SURFACE_POOL_SIZE 2
static struct
{
   EGL_SURFACE_T surfaces[EGL_SURFACE_POOL_SIZE];
   uint32_t allocated;
} surface_pool;

extern VCOS_LOG_CAT_T egl_client_log_cat;

/*
   EGL_SURFACE_T* egl_surface_pool_alloc(void)

   Implementation notes:

   We have a small static pool of structures (surface_pool) which we try and allocate out of
   in order to reduce memory fragmentation. When we have run out of space in the pool we
   resort to khrn_platform_malloc.

   Preconditions:

   Whoever calls this must initialise (or free) the returned structure in order to satisfy the invariant
   on surface_pool.

   Postconditions:

   Return value is NULL or an uninitialised EGL_SURFACE_T structure, valid until egl_surface_pool_free
   is called.
*/

static EGL_SURFACE_T* egl_surface_pool_alloc(void)
{
   int i = 0;

   while(surface_pool.allocated & (1 << i))
      i++;

   if (i < EGL_SURFACE_POOL_SIZE)
   {
      surface_pool.allocated |= 1 << i;
      return &surface_pool.surfaces[i];
   }
   else
   {
      return (EGL_SURFACE_T*)khrn_platform_malloc(sizeof(EGL_SURFACE_T), "EGL_SURFACE_T");
   }
}

static void egl_surface_pool_free(EGL_SURFACE_T* surface)
{
   unsigned int i = 0;

   /* todo: this doesn't belong here */
   //semaphore now gets destroyed on async callback from VC
   if (surface->avail_buffers_valid)
      khronos_platform_semaphore_destroy(&surface->avail_buffers);
   surface->avail_buffers_valid = false;

   i = (unsigned int) (surface - surface_pool.surfaces);

   if (i < EGL_SURFACE_POOL_SIZE)
   {
      surface_pool.allocated &= ~(1 << i);
   }
   else
   {
      khrn_platform_free((void*)surface);
   }
}

/*
   EGLBoolean egl_surface_check_attribs_window(const EGLint *attrib_list, EGLBoolean *linear, EGLBoolean *premult, EGLBoolean *single)

   TODO: are we actually supposed to validate our parameters and generate an
   error if they're wrong? I can't find an explicit mention in the spec about it.
   (except for EGL_WIDTH and EGL_HEIGHT in pbuffer)

   Preconditions:

   type in {WINDOW, PBUFFER, PIXMAP}
   attrib_list is NULL or a pointer to an EGL_NONE-terminated list of attribute/value pairs
   linear, premult are NULL or valid pointers
   If type == WINDOW then single is NULL or a valid pointer
   If type == PBUFFER then width, height, largest_pbuffer, texture_format, texture_target, mipmap_texture are NULL or valid pointers

   Postconditions:

   -
*/

bool egl_surface_check_attribs(
   EGL_SURFACE_TYPE_T type,
   const EGLint *attrib_list,
   bool *linear,
   bool *premult,
   bool *single,
   int *width,
   int *height,
   bool *largest_pbuffer,
   EGLenum *texture_format,
   EGLenum *texture_target,
   bool *mipmap_texture
   )
{
   if (!attrib_list)
      return true;

   while (*attrib_list != EGL_NONE) {
      int name = *attrib_list++;
      int value = *attrib_list++;

      switch (name) {
      case EGL_VG_COLORSPACE:
         if (value != EGL_VG_COLORSPACE_sRGB && value != EGL_VG_COLORSPACE_LINEAR)
            return false;
         if (value == EGL_VG_COLORSPACE_LINEAR && linear != NULL)
            *linear = true;
         break;
      case EGL_VG_ALPHA_FORMAT:
         if (value != EGL_VG_ALPHA_FORMAT_NONPRE && value != EGL_VG_ALPHA_FORMAT_PRE)
            return false;
         if (value == EGL_VG_ALPHA_FORMAT_PRE && premult != NULL)
            *premult = true;
         break;

      /* For WINDOW types only */
      case EGL_RENDER_BUFFER:
         if (type != WINDOW || (value != EGL_SINGLE_BUFFER && value != EGL_BACK_BUFFER))
            return false;
         if (value == EGL_SINGLE_BUFFER && single != NULL)
            *single = true;
         break;

      /* For PBUFFER types only */
      case EGL_WIDTH:
         if (type != PBUFFER || value < 0)
            return false;
         if (width != NULL)
            *width = value;
         break;
      case EGL_HEIGHT:
         if (type != PBUFFER || value < 0)
            return false;
         if (height != NULL)
            *height = value;
         break;
      case EGL_LARGEST_PBUFFER:
         if (type != PBUFFER || (value != EGL_FALSE && value != EGL_TRUE))
            return false;
         if (largest_pbuffer != NULL)
            *largest_pbuffer = value;
         break;
      case EGL_TEXTURE_FORMAT:
         if (type != PBUFFER || (value != EGL_NO_TEXTURE && value != EGL_TEXTURE_RGB && value != EGL_TEXTURE_RGBA))
            return false;
         if (texture_format != NULL)
            *texture_format = value;
         break;
      case EGL_TEXTURE_TARGET:
         if (type != PBUFFER || (value != EGL_NO_TEXTURE && value != EGL_TEXTURE_2D))
            return false;
         if (texture_target != NULL)
            *texture_target = value;
         break;
      case EGL_MIPMAP_TEXTURE:
         if (type != PBUFFER || (value != EGL_FALSE && value != EGL_TRUE))
            return false;
         if (mipmap_texture != NULL)
            *mipmap_texture = value;
         break;
      default:
         return false;
      }
   }

   return true;
}

/*
   EGL_SURFACE_T *egl_surface_create(
      EGLSurface name,
      EGL_SURFACE_TYPE_T type,
      EGL_SURFACE_COLORSPACE_T colorspace,
      EGL_SURFACE_ALPHAFORMAT_T alphaformat,
      uint32_t buffers,
      uint32_t width,
      uint32_t height,
      EGLConfig config,
      EGLNativeWindowType win,
      uint32_t serverwin,
      bool largest_pbuffer,
      bool mipmap_texture,
      EGLenum texture_format,
      EGLenum texture_target,
      EGLNativePixmapType pixmap,
      const uint32_t *pixmap_server_handle)

   Implementation notes:

   TODO: respect largest_pbuffer?

   Preconditions:
      type in {WINDOW,PBUFFER,PIXMAP}
      colorspace in {SRGB,LINEAR}
      alphaformat in {NONPRE,PRE}
      1 <= buffers <= EGL_MAX_BUFFERS
      0 < width, 0 < height
      If !largest_pbuffer then width <= EGL_CONFIG_MAX_WIDTH, height <= EGL_CONFIG_MAX_HEIGHT
      config is a valid EGL config

      If type == WINDOW then
         win is a valid client-side handle to window W
         serverwin is a valid server-side handle to window W
      else
         win == 0
         serverwin == PLATFORM_WIN_NONE

      If type == PBBUFFER then
         texture_format in {EGL_NO_TEXTURE, EGL_TEXTURE_RGB, EGL_TEXTURE_RGBA}
         texture_target in {EGL_NO_TEXTURE, EGL_TEXTURE_2D}
      else
         largest_pbuffer == mipmap_texture == false
         texture_format == texture_target == EGL_NO_TEXTURE

      If type == PIXMAP then
         pixmap is a valid client-side handle to pixmap P
         If pixmap is a server-side pixmap then
            pixmap_server_handle is a pointer to two elements, representing a valid server-side handle to pixmap P
         else
            pixmap_server_handle == 0
      else
         pixmap == pixmap_server_handle == 0

   Postconditions:
      Return value is NULL or an EGL_SURFACE_T structure, valid until egl_surface_free is called

   Invariants preserved:
      All invaraints on EGL_SURFACE_T
*/

EGL_SURFACE_T *egl_surface_create(
   EGLSurface name,
   EGL_SURFACE_TYPE_T type,
   EGL_SURFACE_COLORSPACE_T colorspace,
   EGL_SURFACE_ALPHAFORMAT_T alphaformat,
   uint32_t buffers,
   uint32_t width,
   uint32_t height,
   EGLConfig config,
   EGLNativeWindowType win,
   uint32_t serverwin,
   bool largest_pbuffer,
   bool texture_compatibility,
   bool mipmap_texture,
   EGLenum texture_format,
   EGLenum texture_target,
   EGLNativePixmapType pixmap,
   const uint32_t *pixmap_server_handle)
{
   KHRN_IMAGE_FORMAT_T color;
   KHRN_IMAGE_FORMAT_T depth;
   KHRN_IMAGE_FORMAT_T mask;
   KHRN_IMAGE_FORMAT_T multi;
   uint32_t configid;
   uint32_t sem_name;
   EGLint   config_depth_bits;
   EGLint   config_stencil_bits;
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   EGL_SURFACE_T *surface = egl_surface_pool_alloc();

   //vcos_assert(width > 0 && height > 0);
   vcos_assert((width <= EGL_CONFIG_MAX_WIDTH && height <= EGL_CONFIG_MAX_HEIGHT) || largest_pbuffer);

   if (!surface) {
      return 0;
   }

   /* TODO: respect largest_pbuffer? */

   surface->name = name;
   surface->type = type;

   surface->colorspace = colorspace;
   surface->alphaformat = alphaformat;

   surface->config = config;
   surface->win = win;
   surface->width = width;
   surface->height = height;
   surface->swap_interval = 1;

   surface->base_width = width;
   surface->base_height = height;

   surface->internal_handle = serverwin;

   surface->largest_pbuffer = largest_pbuffer;
   surface->mipmap_texture = mipmap_texture;
   surface->mipmap_level = 0;
   surface->texture_format = texture_format;
   surface->texture_target = texture_target;
   surface->pixmap = pixmap;
   surface->pixmap_server_handle[0] = 0;
   surface->pixmap_server_handle[1] = (uint32_t)-1;
   surface->server_owned = false;
   surface->swap_behavior = buffers > 1 ? EGL_BUFFER_DESTROYED : EGL_BUFFER_PRESERVED;
   surface->multisample_resolve = EGL_MULTISAMPLE_RESOLVE_DEFAULT;

   surface->context_binding_count = 0;
   surface->is_destroyed = false;

#if EGL_KHR_lock_surface
   surface->is_locked = false;
   surface->mapped_buffer = 0;
#endif

   configid = egl_config_to_id(config);
   color = egl_config_get_color_format(configid);
   if (texture_compatibility) { color = khrn_image_to_tf_format(color); }
   if (alphaformat == PRE) { color = (KHRN_IMAGE_FORMAT_T)(color | IMAGE_FORMAT_PRE); }
   if (colorspace == LINEAR) { color = (KHRN_IMAGE_FORMAT_T)(color | IMAGE_FORMAT_LIN); }
   depth = egl_config_get_depth_format(configid);
   mask = egl_config_get_mask_format(configid);
   multi = egl_config_get_multisample_format(configid);

   /* Find depth and stencil bits from chosen config (these may NOT be the same as the underlying format!) */
   egl_config_get_attrib(configid, EGL_DEPTH_SIZE, &config_depth_bits);
   egl_config_get_attrib(configid, EGL_STENCIL_SIZE, &config_stencil_bits);

   vcos_assert(color != IMAGE_FORMAT_INVALID);

#ifdef KHRONOS_EGL_PLATFORM_OPENWFC
   // Create stream for this window
   if(type != PBUFFER)
   {
      WFCNativeStreamType stream = (WFCNativeStreamType) surface->internal_handle;
      vcos_assert(stream != WFC_INVALID_HANDLE);
      uint32_t failure = wfc_stream_create(stream, WFC_STREAM_FLAGS_EGL);
      vcos_log_trace("Creating stream with handle %X", stream);
      vcos_assert(failure == 0);
   } // if
#endif

   surface->buffers = buffers;

   if (pixmap_server_handle) {
      vcos_assert(type == PIXMAP);
      surface->pixmap_server_handle[0] = pixmap_server_handle[0];
      surface->pixmap_server_handle[1] = pixmap_server_handle[1];
      surface->serverbuffer = RPC_UINT_RES(RPC_CALL7_RES(eglIntCreateWrappedSurface_impl,
                                                         thread,
                                                         EGLINTCREATEWRAPPEDSURFACE_ID,
                                                         RPC_UINT(pixmap_server_handle[0]),
                                                         RPC_UINT(pixmap_server_handle[1]),
                                                         RPC_UINT(depth),
                                                         RPC_UINT(mask),
                                                         RPC_UINT(multi),
                                                         RPC_UINT(config_depth_bits),
                                                         RPC_UINT(config_stencil_bits)));
      surface->avail_buffers_valid = false;
   } else {
#ifdef __SYMBIAN32__
      uint32_t nbuff = 0;
      surface->avail_buffers_valid = 0;

      if (surface->buffers > 1) {
         uint64_t pid = rpc_get_client_id(thread);
         int sem[3] = { (int)pid, (int)(pid >> 32), (int)name };
         if (khronos_platform_semaphore_create(&surface->avail_buffers, sem, surface->buffers) == KHR_SUCCESS)
            surface->avail_buffers_valid = 1;
            nbuff = (int)surface->avail_buffers;
      }

      if (surface->buffers == 1 || surface->avail_buffers_valid) {
         uint32_t results[3];

         RPC_CALL15_OUT_CTRL(eglIntCreateSurface_impl,
                             thread,
                             EGLINTCREATESURFACE_ID,
                             RPC_UINT(serverwin),
                             RPC_UINT(buffers),
                             RPC_UINT(width),
                             RPC_UINT(height),
                             RPC_UINT(color),
                             RPC_UINT(depth),
                             RPC_UINT(mask),
                             RPC_UINT(multi),
                             RPC_UINT(largest_pbuffer),
                             RPC_UINT(mipmap_texture),
                             RPC_UINT(config_depth_bits),
                             RPC_UINT(config_stencil_bits),
                             RPC_UINT((int)nbuff),
                             RPC_UINT(type),
                             results);
#else
      surface->avail_buffers_valid = false;

      sem_name = KHRN_NO_SEMAPHORE;
#ifndef KHRONOS_EGL_PLATFORM_OPENWFC
      if (surface->buffers > 1) {
         uint64_t pid = rpc_get_client_id(thread);
         int sem[3];
         sem[0] = (int)pid; sem[1] = (int)(pid >> 32); sem[2] = (int)name;

         sem_name = (int)name;
         if (khronos_platform_semaphore_create(&surface->avail_buffers, sem, surface->buffers) == KHR_SUCCESS)
            surface->avail_buffers_valid = true;
      }
      if (sem_name == KHRN_NO_SEMAPHORE || surface->avail_buffers_valid) {
#else
      sem_name = (uint32_t)surface->internal_handle;
      vcos_log_trace("Surface create has semaphore %X", sem_name);
#endif
         uint32_t results[3];

         RPC_CALL15_OUT_CTRL(eglIntCreateSurface_impl,
                             thread,
                             EGLINTCREATESURFACE_ID,
                             RPC_UINT(serverwin),
                             RPC_UINT(buffers),
                             RPC_UINT(width),
                             RPC_UINT(height),
                             RPC_UINT(color),
                             RPC_UINT(depth),
                             RPC_UINT(mask),
                             RPC_UINT(multi),
                             RPC_UINT(largest_pbuffer),
                             RPC_UINT(mipmap_texture),
                             RPC_UINT(config_depth_bits),
                             RPC_UINT(config_stencil_bits),
                             RPC_UINT(sem_name),
                             RPC_UINT(type),
                             results);
#endif
         surface->width = results[0];
         surface->height = results[1];
         surface->serverbuffer = results[2];
#ifndef KHRONOS_EGL_PLATFORM_OPENWFC
      } else {
         surface->serverbuffer = 0;
      }
#endif
   }

   if (surface->serverbuffer)
      return surface;
   else {
      /* Server failed to create a surface due to out-of-memory or
         we failed to create the named semaphore object. */
      egl_surface_pool_free(surface);
      return 0;
   }
}

#ifndef NO_OPENVG

/* Either returns a valid EGL_SURFACE_T, or returns null and sets error appropriately */

EGL_SURFACE_T *egl_surface_from_vg_image(
   VGImage vg_handle,
   EGLSurface name,
   EGLConfig config,
   EGLBoolean largest_pbuffer,
   EGLBoolean mipmap_texture,
   EGLenum texture_format,
   EGLenum texture_target,
   EGLint *error)
{
   KHRN_IMAGE_FORMAT_T color;
   KHRN_IMAGE_FORMAT_T depth;
   KHRN_IMAGE_FORMAT_T mask;
   KHRN_IMAGE_FORMAT_T multi;
   EGLint   config_depth_bits;
   EGLint   config_stencil_bits;
   uint32_t results[5];
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   EGL_SURFACE_T *surface = egl_surface_pool_alloc();

   if (!surface) {
      *error = EGL_BAD_ALLOC;
      return 0;
   }

   /* TODO: respect largest_pbuffer? */

   surface->name = name;
   surface->type = PBUFFER;

   surface->config = config;
   surface->win = 0;
   surface->swap_interval = 1;

   surface->largest_pbuffer = largest_pbuffer;
   surface->mipmap_texture = mipmap_texture;
   surface->mipmap_level = 0;
   surface->texture_format = texture_format;
   surface->texture_target = texture_target;
   surface->pixmap = 0;
   surface->pixmap_server_handle[0] = 0;
   surface->pixmap_server_handle[1] = (uint32_t)-1;
   surface->server_owned = false;

   surface->context_binding_count = 0;
   surface->is_destroyed = false;

#if EGL_KHR_lock_surface
   surface->is_locked = false;
   surface->mapped_buffer = 0;
#endif

   color = egl_config_get_color_format((int)(size_t)config - 1);
   depth = egl_config_get_depth_format((int)(size_t)config - 1);
   mask = egl_config_get_mask_format((int)(size_t)config - 1);
   multi = egl_config_get_multisample_format((int)(size_t)config - 1);

   /* Find depth and stencil bits from chosen config (these may NOT be the same as the underlying format!) */
   egl_config_get_attrib((int)(size_t)config - 1, EGL_DEPTH_SIZE, &config_depth_bits);
   egl_config_get_attrib((int)(size_t)config - 1, EGL_STENCIL_SIZE, &config_stencil_bits);

   vcos_assert(color != IMAGE_FORMAT_INVALID);

   surface->buffers = 1;

   RPC_CALL9_OUT_CTRL(eglIntCreatePbufferFromVGImage_impl,
                     thread,
                     EGLINTCREATEPBUFFERFROMVGIMAGE_ID,
                     RPC_UINT(vg_handle),
                     RPC_UINT(color),
                     RPC_UINT(depth),
                     RPC_UINT(mask),
                     RPC_UINT(multi),
                     RPC_UINT(mipmap_texture),
                     RPC_UINT(config_depth_bits),
                     RPC_UINT(config_stencil_bits),
                     results);

   surface->avail_buffers_valid = false;

   if (results[0]) {
      KHRN_IMAGE_FORMAT_T format = (KHRN_IMAGE_FORMAT_T)results[4];

      surface->serverbuffer = results[0];
      surface->width = results[2];
      surface->height = results[3];

      /* TODO: picking apart image formats like this seems messy */
      surface->colorspace = (format & IMAGE_FORMAT_LIN) ? LINEAR : SRGB;
      surface->alphaformat = (format & IMAGE_FORMAT_PRE) ? PRE : NONPRE;
      *error = EGL_SUCCESS;
      return surface;
   } else {
      *error = results[1];
      egl_surface_pool_free(surface);
      return 0;
   }
}

#endif /* NO_OPENVG */

/*
   void egl_surface_free(EGL_SURFACE_T *surface)

   Preconditions:

   surface is a valid EGL_SURFACE_T returned from egl_surface_create or egl_surface_from_vg_image

   Postconditions:

   surface is freed and any associated server-side resources are dereferenced.
*/

void egl_surface_free(EGL_SURFACE_T *surface)
{
   CLIENT_THREAD_STATE_T *thread;

   vcos_log_trace("egl_surface_free");

   thread = CLIENT_GET_THREAD_STATE();

   if( surface->type == WINDOW ) {
      vcos_log_trace("egl_surface_free: calling platform_destroy_winhandle...");
      platform_destroy_winhandle( surface->win, surface->internal_handle );
   }
   /* return value ignored -- read performed to ensure blocking. we want this to
    * block so clients can safely destroy the surface's window as soon as the
    * egl call that destroys the surface returns (usually eglDestroySurface, but
    * could be eg eglMakeCurrent) */
   vcos_log_trace("egl_surface_free: calling eglIntDestroySurface_impl via RPC...; "
      "thread = 0x%X; serverbuffer = 0x%X",
      (uint32_t) thread, (uint32_t) surface->serverbuffer);
   (void)RPC_INT_RES(RPC_CALL1_RES(eglIntDestroySurface_impl,
                                   thread,
                                   EGLINTDESTROYSURFACE_ID,
                                   RPC_UINT(surface->serverbuffer)));

#ifdef KHRONOS_EGL_PLATFORM_OPENWFC
   if(surface->internal_handle != PLATFORM_WIN_NONE) // TODO what about pbuffers?
   {
      vcos_log_trace("egl_surface_free: calling wfc_stream_destroy...");
      wfc_stream_destroy((WFCNativeStreamType) surface->internal_handle);
   }
#endif

   vcos_log_trace("egl_surface_free: calling egl_surface_pool_free...");
   egl_surface_pool_free(surface);

   vcos_log_trace("egl_surface_free: end");
}

EGLint egl_surface_get_render_buffer(EGL_SURFACE_T *surface)
{
   switch (surface->type) {
   case WINDOW:
      if (surface->buffers == 1)
         return EGL_SINGLE_BUFFER;
      else
         return EGL_BACK_BUFFER;
   case PBUFFER:
      return EGL_BACK_BUFFER;
   case PIXMAP:
      return EGL_SINGLE_BUFFER;
   default:
      UNREACHABLE();
      return EGL_NONE;
   }
}

EGLBoolean egl_surface_get_attrib(EGL_SURFACE_T *surface, EGLint attrib, EGLint *value)
{
   switch (attrib) {
   case EGL_VG_ALPHA_FORMAT:
      if (surface->alphaformat == NONPRE)
         *value = EGL_VG_ALPHA_FORMAT_NONPRE;
      else
         *value = EGL_VG_ALPHA_FORMAT_PRE;
      return EGL_TRUE;
   case EGL_VG_COLORSPACE:
      if (surface->colorspace == SRGB)
         *value = EGL_VG_COLORSPACE_sRGB;
      else
         *value = EGL_VG_COLORSPACE_LINEAR;
      return EGL_TRUE;
   case EGL_CONFIG_ID:
      *value = (EGLint)(size_t)surface->config;
      return EGL_TRUE;
   case EGL_HEIGHT:
      *value = surface->height;
      return EGL_TRUE;
   case EGL_HORIZONTAL_RESOLUTION:
   case EGL_VERTICAL_RESOLUTION:
      *value = EGL_UNKNOWN;
      return EGL_TRUE;
   case EGL_LARGEST_PBUFFER:
      // For a window or pixmap surface, the contents of value are not modified.
      if (surface->type == PBUFFER)
         *value = surface->largest_pbuffer;
      return EGL_TRUE;
   case EGL_MIPMAP_TEXTURE:
      // Querying EGL_MIPMAP_TEXTURE for a non-pbuffer surface is not
      // an error, but value is not modified.
      if (surface->type == PBUFFER)
         *value = surface->mipmap_texture;
      return EGL_TRUE;
   case EGL_MIPMAP_LEVEL:
      // Querying EGL_MIPMAP_LEVEL for a non-pbuffer surface is not
      // an error, but value is not modified.
      if (surface->type == PBUFFER)
         *value = surface->mipmap_level;
      return EGL_TRUE;
   case EGL_PIXEL_ASPECT_RATIO:
      *value = EGL_DISPLAY_SCALING;
      return EGL_TRUE;
   case EGL_RENDER_BUFFER:
      *value = egl_surface_get_render_buffer(surface);
      return EGL_TRUE;
   case EGL_SWAP_BEHAVIOR:
      *value = surface->swap_behavior;
      return EGL_TRUE;
   case EGL_MULTISAMPLE_RESOLVE:
      *value = surface->multisample_resolve;
      return EGL_TRUE;
   case EGL_TEXTURE_FORMAT:
      // Querying EGL_TEXTURE_FORMAT for a non-pbuffer surface is not
      // an error, but value is not modified.
      if (surface->type == PBUFFER)
         *value = surface->texture_format;
      return EGL_TRUE;
   case EGL_TEXTURE_TARGET:
      // Querying EGL_TEXTURE_TARGET for a non-pbuffer surface is not
      // an error, but value is not modified.
      if (surface->type == PBUFFER)
         *value = surface->texture_target;
      return EGL_TRUE;
   case EGL_WIDTH:
      *value = surface->width;
      return EGL_TRUE;
   default:
      return EGL_FALSE;
   }
}

EGLint egl_surface_set_attrib(EGL_SURFACE_T *surface, EGLint attrib, EGLint value)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   switch (attrib) {
   case EGL_MIPMAP_LEVEL:
      if (surface->type == PBUFFER) {
         RPC_CALL2(eglIntSelectMipmap_impl,
                   thread,
                   EGLINTSELECTMIPMAP_ID,
                   RPC_UINT(surface->serverbuffer),
                   RPC_INT(value));

         surface->mipmap_level = value;
      }
      return EGL_SUCCESS;
   case EGL_SWAP_BEHAVIOR:
      switch (value) {
      case EGL_BUFFER_PRESERVED:
      case EGL_BUFFER_DESTROYED:
         surface->swap_behavior = value;
         return EGL_SUCCESS;
      default:
         return EGL_BAD_PARAMETER;
      }
   case EGL_MULTISAMPLE_RESOLVE:
      switch (value) {
      case EGL_MULTISAMPLE_RESOLVE_DEFAULT:
      case EGL_MULTISAMPLE_RESOLVE_BOX:
         surface->multisample_resolve = value;
         return EGL_SUCCESS;
      default:
         return EGL_BAD_PARAMETER;
      }
   default:
      return EGL_BAD_ATTRIBUTE;
   }
}

#if EGL_KHR_lock_surface

EGLint egl_surface_get_mapped_buffer_attrib(EGL_SURFACE_T *surface, EGLint attrib, EGLint *value)
{
   KHRN_IMAGE_FORMAT_T format;
   bool is565;

   vcos_assert(surface);

   if (attrib == EGL_BITMAP_POINTER_KHR || attrib == EGL_BITMAP_PITCH_KHR) {
      // Querying either of these causes the buffer to be mapped (if it isn't already)
      // They also require that the surface is locked

      if (!surface->is_locked) {
         return EGL_BAD_ACCESS;   // TODO is this the right error?
      }

      if (!surface->mapped_buffer) {
         uint32_t size;
         void *buffer;
         format = egl_config_get_mapped_format((int)((intptr_t)surface->config - 1)); // type juggling to avoid pointer truncation warnings
         size = khrn_image_get_size(format, surface->width, surface->height);
         buffer = khrn_platform_malloc(size, "EGL_SURFACE_T.mapped_buffer");

         if (!buffer) {
            return EGL_BAD_ALLOC;
         }

         surface->mapped_buffer = buffer;
      }
   }

   if (!egl_config_is_lockable((int)((intptr_t)surface->config-1))) {    // type juggling to avoid pointer truncation warnings
      // Calling any of these on unlockable surfaces is allowed but returns undefined results
      *value = 0;
      return EGL_SUCCESS;
   }

   format = egl_config_get_mapped_format((int)((intptr_t)surface->config-1));  // type juggling to avoid pointer truncation warnings
   vcos_assert(format == RGB_565_RSO || format == ARGB_8888_RSO);
   is565 = (format == RGB_565_RSO);       // else 888

   switch (attrib) {
   case EGL_BITMAP_POINTER_KHR:
      *value = (EGLint)(intptr_t)surface->mapped_buffer; // type juggling to avoid pointer truncation warnings
      return EGL_SUCCESS;
   case EGL_BITMAP_PITCH_KHR:
      *value = khrn_image_get_stride(format, surface->width);
      return EGL_SUCCESS;
   case EGL_BITMAP_ORIGIN_KHR:
      *value = EGL_LOWER_LEFT_KHR;     // TODO: is this correct?
      return EGL_SUCCESS;
   case EGL_BITMAP_PIXEL_RED_OFFSET_KHR:
      *value = is565 ? 11 : 0;         // TODO: I've probably got these wrong too
      return EGL_SUCCESS;
   case EGL_BITMAP_PIXEL_GREEN_OFFSET_KHR:
      *value = is565 ? 5 : 8;
      return EGL_SUCCESS;
   case EGL_BITMAP_PIXEL_BLUE_OFFSET_KHR:
      *value = is565 ? 0 : 16;
      return EGL_SUCCESS;
   case EGL_BITMAP_PIXEL_ALPHA_OFFSET_KHR:
      *value = is565 ? 0 : 24;
      return EGL_SUCCESS;
   case EGL_BITMAP_PIXEL_LUMINANCE_OFFSET_KHR:
      *value = 0;
      return EGL_SUCCESS;
   default:
      UNREACHABLE();
      return EGL_BAD_PARAMETER;
   }
}
#endif

/*
   void egl_surface_maybe_free(EGL_SURFACE_T *surface)

   Frees a surface together with its server-side resources if:
   - it has been destroyed
   - it is no longer current

   Implementation notes:

   -

   Preconditions:

   surface is a valid pointer

   Postconditions:

   Either:
   - surface->is_destroyed is false (we don't change this), or
   - surface->context_binding_count > 0, or
   - surface has been deleted.

   Invariants preserved:

   -

   Invariants used:

   -
 */

void egl_surface_maybe_free(EGL_SURFACE_T *surface)
{
   vcos_assert(surface);

   if (!surface->is_destroyed)
      return;

   if (surface->context_binding_count)
      return;

   egl_surface_free(surface);
}
