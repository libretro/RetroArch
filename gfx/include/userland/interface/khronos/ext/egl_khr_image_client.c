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

#define EGL_EGLEXT_PROTOTYPES /* we want the prototypes so the compiler will check that the signatures match */

#define VCOS_LOG_CATEGORY (&egl_khr_image_client_log)
#include "interface/khronos/common/khrn_client_mangle.h"

#include "interface/khronos/common/khrn_int_common.h"

#include "interface/khronos/common/khrn_client.h"
#include "interface/khronos/common/khrn_client_rpc.h"

#include "interface/khronos/include/EGL/egl.h"
#include "interface/khronos/include/EGL/eglext.h"
#include "interface/khronos/include/EGL/eglext_brcm.h"
#include "interface/khronos/include/GLES/gl.h"

#include "interface/vcos/vcos.h"

#if EGL_ANDROID_image_native_buffer
#include <gralloc_brcm.h>
#endif

#if defined(ANDROID) && defined(KHRN_BCG_ANDROID)
#include "gralloc_priv.h"
#include "middleware/khronos/common/2708/khrn_prod_4.h"
#endif

#ifdef KHRONOS_HAVE_VCSM
#include "user-vcsm.h"
#else
#include <dlfcn.h>
#include <dlfcn.h>
typedef enum
{
   VCSM_CACHE_TYPE_NONE = 0,        // No caching applies.
   VCSM_CACHE_TYPE_HOST,            // Allocation is cached on host (user space).
   VCSM_CACHE_TYPE_VC,              // Allocation is cached on videocore.
   VCSM_CACHE_TYPE_HOST_AND_VC,     // Allocation is cached on both host and videocore.

} VCSM_CACHE_TYPE_T;
static unsigned int (*vcsm_malloc_cache)(unsigned int size, VCSM_CACHE_TYPE_T cache, char *name);
static unsigned int (*vcsm_vc_hdl_from_hdl)(unsigned int handle);
static void (*vcsm_free) (unsigned int handle);
#endif /* KHRONOS_HAVE_VCSM */

static VCOS_LOG_CAT_T egl_khr_image_client_log = VCOS_LOG_INIT("egl_khr_image_client", VCOS_LOG_WARN);

static bool egl_init_vcsm()
{
#ifdef KHRONOS_HAVE_VCSM
    return true;
#else
    static bool warn_once;
    bool success = false;

    if (vcsm_malloc_cache)
       return true;

    if (! vcsm_malloc_cache) {
        /* Try LD_LIBRARY_PATH first */
        void *dl = dlopen("libvcsm.so", RTLD_LAZY);

        if (!dl)
           dl = dlopen("/opt/vc/lib/libvcsm.so", RTLD_LAZY);

        if (dl)
        {
           vcsm_malloc_cache = dlsym(dl, "vcsm_malloc_cache");
           vcsm_vc_hdl_from_hdl = dlsym(dl, "vcsm_vc_hdl_from_hdl");
           vcsm_free = dlsym(dl, "vcsm_free");

           if (vcsm_malloc_cache && vcsm_vc_hdl_from_hdl && vcsm_free) 
           {
              success = true;
           }
           else
           {
              vcsm_malloc_cache = NULL;
              vcsm_vc_hdl_from_hdl = NULL;
              vcsm_free = NULL;
           }
        }
    }
    if (! success && ! warn_once)
    {
        vcos_log_error("Unable to load libvcsm.so for target EGL_IMAGE_BRCM_VCSM");
        warn_once = false;
    }
    return success;
#endif /* KHRONOS_HAVE_VCSM */
}

EGLAPI EGLImageKHR EGLAPIENTRY eglCreateImageKHR (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attr_list)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   EGLImageKHR result = EGL_NO_IMAGE_KHR;

   CLIENT_LOCK();

   vcos_log_info("eglCreateImageKHR: dpy %p ctx %p target %x buf %p\n",
                                      dpy, ctx, target, buffer);

   {
      CLIENT_PROCESS_STATE_T *process = client_egl_get_process_state(thread, dpy, EGL_TRUE);

      if (process) {
         EGL_CONTEXT_T *context;
         bool ctx_error;
         if (target == EGL_NATIVE_PIXMAP_KHR
#ifdef EGL_BRCM_image_wrap
            || target == EGL_IMAGE_WRAP_BRCM
#endif
#ifdef EGL_BRCM_image_wrap_bcg
            || target == EGL_IMAGE_WRAP_BRCM_BCG
#endif
#if EGL_ANDROID_image_native_buffer
            || target == EGL_NATIVE_BUFFER_ANDROID
            || target == EGL_IMAGE_BRCM_RAW_PIXELS
#endif
            || target == EGL_IMAGE_BRCM_MULTIMEDIA
            || target == EGL_IMAGE_BRCM_MULTIMEDIA_Y
            || target == EGL_IMAGE_BRCM_MULTIMEDIA_U
            || target == EGL_IMAGE_BRCM_MULTIMEDIA_V
            || target == EGL_IMAGE_BRCM_DUPLICATE
            || target == EGL_IMAGE_BRCM_VCSM
            ) {
            context = NULL;
            ctx_error = ctx != EGL_NO_CONTEXT;
         } else {
            context = client_egl_get_context(thread, process, ctx);
            ctx_error = !context;
         }
         if (ctx_error) {
            thread->error = EGL_BAD_PARAMETER;
         }
         else {
            uint32_t buf[2];
            KHRN_IMAGE_FORMAT_T buffer_format = IMAGE_FORMAT_INVALID;
            uint32_t buffer_width = 0;
            uint32_t buffer_height = 0;
            uint32_t buffer_stride = 0;
            bool buf_error = false;
            if (target == EGL_NATIVE_PIXMAP_KHR) {
               buf[0] = 0; buf[1] = (uint32_t)(-1);
               platform_get_pixmap_server_handle((EGLNativePixmapType)buffer, buf);
#if EGL_BRCM_global_image
               if ((buf[0] == 0) && (buf[1] == (uint32_t)(-1))) { /* allow either regular or global image server-side pixmaps */
#else
               if ((buf[0] == 0) || (buf[1] != (uint32_t)(-1))) { /* only allow regular server-side pixmaps */
#endif
                  /* This is a client-side pixmap! TODO: implement these properly */
                  KHRN_IMAGE_WRAP_T image;
                  if (platform_get_pixmap_info((EGLNativePixmapType)buffer, &image))
                  {
//meego hack          
                     if(image.aux!=0)
                     {
                        //image.aux refers to a server side EGL surface 
                        //that already contains the data we're interested in
                        buf[0] = (uint32_t)image.aux;
                        target = EGL_IMAGE_FROM_SURFACE_BRCM;
                        khrn_platform_release_pixmap_info((EGLNativePixmapType)buffer, &image);                        
                     }
//                                         
                     else
                     {
                        buf[0] = image.width | image.height << 16;
                        target = EGL_NATIVE_PIXMAP_CLIENT_SIDE_BRCM;
                        khrn_platform_release_pixmap_info((EGLNativePixmapType)buffer, &image);
                     }

                  }
                  else
                  {
                     buf_error = true;
                  }
               }
#if EGL_BRCM_image_wrap
            } else if (target == EGL_IMAGE_WRAP_BRCM) {
               KHRN_IMAGE_WRAP_T *wrap_buffer = (KHRN_IMAGE_WRAP_T *)buffer;

               buf[0] = (uint32_t)wrap_buffer->storage;
               buffer_format = wrap_buffer->format;
               buffer_width = wrap_buffer->width;
               buffer_height = wrap_buffer->height;
               buffer_stride = wrap_buffer->stride;
#endif
#if EGL_BRCM_image_wrap_bcg
            } else if (target == EGL_IMAGE_WRAP_BRCM_BCG) {
               EGL_IMAGE_WRAP_BRCM_BCG_IMAGE_T *wrap_buffer = (EGL_IMAGE_WRAP_BRCM_BCG_IMAGE_T *)buffer;

               buf[0] = (uint32_t)wrap_buffer->storage;
               buffer_width = wrap_buffer->width;
               buffer_height = wrap_buffer->height;
               buffer_stride = wrap_buffer->stride;

               switch(wrap_buffer->format)
               {
               case BEGL_BufferFormat_eR8G8B8A8_TFormat:          buffer_format = ABGR_8888_TF;          break;
               case BEGL_BufferFormat_eX8G8B8A8_TFormat:          buffer_format = XBGR_8888_TF;          break;
               case BEGL_BufferFormat_eR5G6B5_TFormat:            buffer_format = RGB_565_TF;            break;
               case BEGL_BufferFormat_eR5G5B5A1_TFormat:          buffer_format = RGBA_5551_TF;          break;
               case BEGL_BufferFormat_eR4G4B4A4_TFormat:          buffer_format = RGBA_4444_TF;          break;

               case BEGL_BufferFormat_eR8G8B8A8_LTFormat:         buffer_format = ABGR_8888_LT;          break;
               case BEGL_BufferFormat_eX8G8B8A8_LTFormat:         buffer_format = XBGR_8888_LT;          break;
               case BEGL_BufferFormat_eR5G6B5_LTFormat:           buffer_format = RGB_565_LT;            break;
               case BEGL_BufferFormat_eR5G5B5A1_LTFormat:         buffer_format = RGBA_5551_LT;          break;
               case BEGL_BufferFormat_eR4G4B4A4_LTFormat:         buffer_format = RGBA_4444_LT;          break;

               default:
                  buf_error = true;
               }
#endif
#if EGL_ANDROID_image_native_buffer
#ifdef KHRN_BCG_ANDROID
            } else if (target == EGL_NATIVE_BUFFER_ANDROID) {
               android_native_buffer_t *android_buffer = (android_native_buffer_t *)buffer;
               vcos_assert(ANDROID_NATIVE_BUFFER_MAGIC == android_buffer->common.magic);
               /* TODO check that handle is a valid gralloc handle */
               /* These are shadow width/height and format, not to be confused with the
                  underlying formats configuration */

               buf[0] = (uint32_t)khrn_hw_unaddr(((struct private_handle_t *)android_buffer->handle)->oglPhysicalAddress);

               buffer_format = ((struct private_handle_t *)android_buffer->handle)->oglFormat;
               buffer_width = android_buffer->width;
               buffer_height = android_buffer->height;
               buffer_stride = ((struct private_handle_t *)android_buffer->handle)->oglStride;

               switch (((struct private_handle_t *)android_buffer->handle)->oglFormat)
               {
                  case BEGL_BufferFormat_eR8G8B8A8_TFormat:       buffer_format = ABGR_8888_TF;    break;
                  case BEGL_BufferFormat_eX8G8B8A8_TFormat:       buffer_format = XBGR_8888_TF;    break;
                  case BEGL_BufferFormat_eR5G6B5_TFormat:         buffer_format = RGB_565_TF;      break;
                  case BEGL_BufferFormat_eR5G5B5A1_TFormat:       buffer_format = RGBA_5551_TF;    break;
                  case BEGL_BufferFormat_eR4G4B4A4_TFormat:       buffer_format = RGBA_4444_TF;    break;
                  case BEGL_BufferFormat_eR8G8B8A8_LTFormat:      buffer_format = ABGR_8888_LT;    break;
                  case BEGL_BufferFormat_eX8G8B8A8_LTFormat:      buffer_format = XBGR_8888_LT;    break;
                  case BEGL_BufferFormat_eR5G6B5_LTFormat:        buffer_format = RGB_565_LT;      break;
                  case BEGL_BufferFormat_eR5G5B5A1_LTFormat:      buffer_format = RGBA_5551_LT;    break;
                  case  BEGL_BufferFormat_eR4G4B4A4_LTFormat:     buffer_format = RGBA_4444_LT;    break;
                  default :                                       buf_error = true;                break;
               }
#else
            } else if (target == EGL_NATIVE_BUFFER_ANDROID) {
               gralloc_private_handle_t *gpriv = gralloc_private_handle_from_client_buffer(buffer);
               int res_type = gralloc_private_handle_get_res_type(gpriv);

               if (res_type == GRALLOC_PRIV_TYPE_GL_RESOURCE) {
                  /* just return the a copy of the EGLImageKHR gralloc created earlier
                     see hardware/broadcom/videocore/components/graphics/gralloc/ */
                  target = EGL_IMAGE_BRCM_DUPLICATE;
                  buf[0] = (uint32_t)gralloc_private_handle_get_egl_image(gpriv);
                  vcos_log_trace("%s: converting buffer %p egl_image %d to EGL_IMAGE_BRCM_DUPLICATE",
                        __FUNCTION__, buffer, buf[0]);
               }
               else if (res_type == GRALLOC_PRIV_TYPE_MM_RESOURCE) {
                  /* MM image is potentially going to be used as a texture so
                   * VC EGL needs to acquire a reference to the underlying vc_image.
                   * So, we create the image in the normal way.
                   * EGL_NATIVE_BUFFER_ANDROID is passed as the target.
                   */
                  if (gpriv->gl_format == GRALLOC_MAGICS_HAL_PIXEL_FORMAT_OPAQUE)
                     target = EGL_IMAGE_BRCM_MULTIMEDIA;
                  else
                     target = EGL_IMAGE_BRCM_RAW_PIXELS;
                  buffer_width = gpriv->w;
                  buffer_height = gpriv->h;
                  buffer_stride = gpriv->stride;

                  buf[0] = gralloc_private_handle_get_vc_handle(gpriv);
                  vcos_log_trace("%s: converting buffer %p handle %u to EGL_IMAGE_BRCM_MULTIMEDIA",
                        __FUNCTION__, buffer, buf[0]);
               }
               else {
                  vcos_log_error("%s: unknown gralloc resource type %x", __FUNCTION__, res_type);
               }
#endif
#else /* Not Android */
            } else if (target == EGL_IMAGE_BRCM_VCSM && egl_init_vcsm()) {
                  struct egl_image_brcm_vcsm_info *info = (struct egl_image_brcm_vcsm_info *) buffer;
                  buf_error = true;
                  vcos_log_info("%s: EGL_IMAGE_BRCM_VCSM", __FUNCTION__); // FIXME

#define IS_POT(X) ((X) && (((X) & (~(X) + 1)) == (X)))
#define VALID_RSO_DIM(X) (IS_POT(X) && (X) >= 64 && (X) <= 2048)

                  // Allocate the VCSM buffer. This could be moved to VideoCore.
                  if (info && VALID_RSO_DIM(info->width) && VALID_RSO_DIM(info->height)) {
                      unsigned int vcsm_handle;
                      buffer_width = info->width;
                      buffer_height = info->height;
                      buffer_format = ABGR_8888_RSO; // Only format supported
                      buffer_stride = buffer_width << 2;

                      vcsm_handle = vcsm_malloc_cache(buffer_stride * buffer_height,
                              VCSM_CACHE_TYPE_HOST, "EGL_IMAGE_BRCM_VCSM");
                      if (vcsm_handle) {
                          buf[0] = vcsm_vc_hdl_from_hdl(vcsm_handle);
                          info->vcsm_handle = vcsm_handle;
                          if (buf[0])
                              buf_error = false;
                          else {
                              vcos_log_error("%s: bad VCSM handle %u", __FUNCTION__, vcsm_handle);
                              vcsm_free(vcsm_handle);
                          }

                         vcos_log_trace("%s: VCSM %u VC %u %ux%u %u", __FUNCTION__, vcsm_handle,
                                 buf[0], buffer_width, buffer_height, buffer_stride);
                      }
                  } else {
                      vcos_log_error("VCSM buffer dimension but be POT between 64 and 2048\n");
                  }
            } else if (target == EGL_IMAGE_BRCM_MULTIMEDIA) {
                  buf[0] = (uint32_t)buffer;
                  vcos_log_trace("%s: converting buffer handle %u to EGL_IMAGE_BRCM_MULTIMEDIA",
                        __FUNCTION__, buf[0]);
            } else if (target == EGL_IMAGE_BRCM_MULTIMEDIA_Y) {
                  buf[0] = (uint32_t)buffer;
                  vcos_log_trace("%s: converting buffer handle %u to EGL_IMAGE_BRCM_MULTIMEDIA_Y",
                        __FUNCTION__, buf[0]);
            } else if (target == EGL_IMAGE_BRCM_MULTIMEDIA_U) {
                  buf[0] = (uint32_t)buffer;
                  vcos_log_trace("%s: converting buffer handle %u to EGL_IMAGE_BRCM_MULTIMEDIA_U",
                        __FUNCTION__, buf[0]);
            } else if (target == EGL_IMAGE_BRCM_MULTIMEDIA_V) {
                  buf[0] = (uint32_t)buffer;
                  vcos_log_trace("%s: converting buffer handle %u to EGL_IMAGE_BRCM_MULTIMEDIA_V",
                        __FUNCTION__, buf[0]);
#endif
            } else {
               vcos_log_trace("%s:target type %x buffer %p handled on server", __FUNCTION__, target, buffer);
               buf[0] = (uint32_t)buffer;
            }
            if (buf_error) {
               thread->error = EGL_BAD_PARAMETER;
            }
            else {
               EGLint texture_level = 0;
               bool attr_error = false;
               if (attr_list) {
                  while (!attr_error && *attr_list != EGL_NONE) {
                     switch (*attr_list++) {
                     case EGL_GL_TEXTURE_LEVEL_KHR:
                        texture_level = *attr_list++;
                        break;
                     case EGL_IMAGE_PRESERVED_KHR:
                     {
                        EGLint preserved = *attr_list++;
                        if ((preserved != EGL_FALSE) && (preserved != EGL_TRUE)) {
                           attr_error = true;
                        } /* else: ignore the actual value -- we always preserve */
                        break;
                     }
                     default:
                        attr_error = true;
                     }
                  }
               }
               if (attr_error) {
                  thread->error = EGL_BAD_PARAMETER;
               }
               else {
#if EGL_BRCM_global_image
                  if ((target == EGL_NATIVE_PIXMAP_KHR) && (buf[1] != (uint32_t)-1)) {
                     if (platform_use_global_image_as_egl_image(buf[0], buf[1], (EGLNativePixmapType)buffer, &thread->error)) {
                        if (!khrn_global_image_map_insert(&process->global_image_egl_images,
                           process->next_global_image_egl_image,
                           buf[0] | ((uint64_t)buf[1] << 32))) {
                           thread->error = EGL_BAD_ALLOC;
                        } else {
                           result = (EGLImageKHR)(uintptr_t)process->next_global_image_egl_image;
                           thread->error = EGL_SUCCESS;
                           do {
                              process->next_global_image_egl_image = (1 << 31) |
                                 (process->next_global_image_egl_image + 1);
                           } while (khrn_global_image_map_lookup(&process->global_image_egl_images,
                              process->next_global_image_egl_image));
                        }
                     }
                  } else
#endif
                  {
                     EGLint results[2];

                     vcos_log_info("%s: width %d height %d target %x buffer %p", __FUNCTION__, buffer_width, buffer_height, target, buffer);
                     RPC_CALL10_OUT_CTRL(eglCreateImageKHR_impl,
                        thread,
                        EGLCREATEIMAGEKHR_ID,
                        RPC_UINT(context ? (context->type == OPENGL_ES_20 ? 2 : 1) : 0),
                        RPC_UINT(context ? context->servercontext : 0),
                        RPC_ENUM(target),
                        RPC_UINT(buf[0]),
                        RPC_UINT(buffer_format),
                        RPC_UINT(buffer_width),
                        RPC_UINT(buffer_height),
                        RPC_UINT(buffer_stride),
                        RPC_INT(texture_level),
                        results);

                     result = (EGLImageKHR)(intptr_t)results[0];
                     thread->error = results[1];

                     if (target == EGL_NATIVE_PIXMAP_CLIENT_SIDE_BRCM || target == EGL_IMAGE_FROM_SURFACE_BRCM)
                     {
                        khrn_platform_bind_pixmap_to_egl_image((EGLNativePixmapType)buffer, result, target == EGL_NATIVE_PIXMAP_CLIENT_SIDE_BRCM);
                     }
                  }
               }
            }
         }
      }
   }

   CLIENT_UNLOCK();
   if (result == EGL_NO_IMAGE_KHR) {
      vcos_log_error("%s:  failed to create image for buffer %p target %d error 0x%x",
            __FUNCTION__, buffer, target, thread->error);
   } else {
      vcos_log_trace("%s: returning EGLImageKHR %p for buffer %p target %d",
            __FUNCTION__, result, buffer, target);
   }
   return result;
}

EGLAPI EGLBoolean EGLAPIENTRY eglDestroyImageKHR (EGLDisplay dpy, EGLImageKHR image)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   EGLBoolean result;

   vcos_log_trace("eglDestroyImageKHR image=%d.\n", (int)image);
   
   CLIENT_LOCK();

   {
      CLIENT_PROCESS_STATE_T *process = client_egl_get_process_state(thread, dpy, EGL_TRUE);
      vcos_log_trace("%s: process %p image %p", __FUNCTION__, process, image);

      if (!process)
         result = EGL_FALSE;
      else {
         khrn_platform_unbind_pixmap_from_egl_image(image);
#if EGL_BRCM_global_image
         if ((uintptr_t)image & (1 << 31)) {
            result = khrn_global_image_map_delete(&process->global_image_egl_images, (uint32_t)(uintptr_t)image) ?
               EGL_TRUE : EGL_FALSE;
         } else
#endif
         {
            vcos_log_trace("%s: process %p image %p calling eglDestroyImageKHR_impl", 
                  __FUNCTION__, process, image);
            result = RPC_BOOLEAN_RES(RPC_CALL1_RES(eglDestroyImageKHR_impl,
               thread,
               EGLDESTROYIMAGEKHR_ID,
               RPC_EGLID(image)));
         }

         if (!result) {
            thread->error = EGL_BAD_PARAMETER;
         }
      }
   }

   CLIENT_UNLOCK();

   return result;
}

void eglIntImageSetColorData(EGLDisplay dpy,
      EGLImageKHR image, KHRN_IMAGE_FORMAT_T format,
      uint32_t x_offset, uint32_t y_offset,
      uint32_t width, uint32_t height,
      int32_t stride, const void *data)
{
   CLIENT_THREAD_STATE_T *thread;
   CLIENT_PROCESS_STATE_T *process;

   if (CLIENT_LOCK_AND_GET_STATES(dpy, &thread, &process)) {

      size_t chunk = KHDISPATCH_WORKSPACE_SIZE / stride;
      const uint8_t *p = (uint8_t *)data + (y_offset*stride);
      size_t remaining = height;
      size_t y = y_offset;

      vcos_log_trace("[%s] egl im %d (%d,%d,%d,%d)",__FUNCTION__, (uint32_t)image, x_offset, y_offset, width, height);

      while (remaining) {
         size_t n = _min(remaining, chunk);
         size_t size = n * stride;

         RPC_CALL8_IN_BULK(eglIntImageSetColorData_impl,
               thread, EGLINTIMAGESETCOLORDATA_ID,
               RPC_UINT(image), format, x_offset, y, width, n, stride,
               p, size);

         p += size;
         y += n;
         remaining -= n;
      }

      CLIENT_UNLOCK();
   }
}
