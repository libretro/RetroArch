/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "vaapi_hevc_backend.h"
#include <formats/image_yuv_blit.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_LOCK_FREE)                   \
      && defined(RETRO_ATOMIC_HAS_PTR)
#include <pthread.h>
#include <rthreads/retro_eventcount.h>

#ifndef VAAPI_HEVC_NODE_PATH
#define VAAPI_HEVC_NODE_PATH vaapi_hevc_backend_node_path
#if defined(__linux__) && !defined(__ANDROID__) && defined(NEED_DYNAMIC)
#include <sys/sysmacros.h>
#include <xf86drm.h>

static bool vaapi_hevc_backend_node_path(
      unsigned major_id, unsigned minor_id, char *path, size_t capacity)
{
   dylib_t library = dylib_load("libdrm.so.2");
   int (*get_device)(dev_t, uint32_t, drmDevicePtr *);
   void (*free_device)(drmDevicePtr *);
   drmDevicePtr device = NULL;
   function_t symbol;
   size_t length;
   bool result = false;
   if (!library)
      return false;
   symbol = dylib_proc(library, "drmGetDeviceFromDevId");
   if (!symbol || sizeof(symbol) != sizeof(get_device))
      goto finish;
   memcpy(&get_device, &symbol, sizeof(get_device));
   symbol = dylib_proc(library, "drmFreeDevice");
   if (!symbol || sizeof(symbol) != sizeof(free_device))
      goto finish;
   memcpy(&free_device, &symbol, sizeof(free_device));
   if (!get_device(makedev(major_id, minor_id), 0, &device) && device)
   {
      if ((device->available_nodes & (1 << DRM_NODE_RENDER)) && device->nodes
            && device->nodes[DRM_NODE_RENDER])
      {
         length = strlen(device->nodes[DRM_NODE_RENDER]);
         if (length && length < capacity)
         {
            memcpy(path, device->nodes[DRM_NODE_RENDER], length + 1);
            result = true;
         }
      }
   }
   if (device)
      free_device(&device);
finish:
   dylib_close(library);
   return result;
}
#else
static bool vaapi_hevc_backend_node_path(
      unsigned major_id, unsigned minor_id, char *path, size_t capacity)
{
   (void)major_id;
   (void)minor_id;
   (void)path;
   (void)capacity;
   return false;
}
#endif
#endif

typedef struct vaapi_hevc_backend_session
{
   rh265_hw_ops_t native;
   vaapi_hevc_owner_t *retired;
   vaapi_hevc_backend_t *backend;
   retro_eventcount_t wake;
   retro_atomic_int_t state, refs;
} vaapi_hevc_backend_session_t;

static void vaapi_hevc_backend_unref(vaapi_hevc_backend_session_t *session)
{
   vaapi_hevc_backend_t *backend;
   if (retro_atomic_fetch_sub_int(&session->refs, 1) != 1)
      return;
   backend = session->backend;
   retro_eventcount_free(&session->wake);
   free(session);
   retro_atomic_fetch_sub_int(&backend->active, 1);
}

static void vaapi_hevc_backend_release(vaapi_hevc_backend_session_t *session)
{
   retro_atomic_store_release_int(&session->state, 1);
   retro_eventcount_notify(&session->wake);
   vaapi_hevc_backend_unref(session);
}

static void *vaapi_hevc_backend_retire(void *opaque)
{
   vaapi_hevc_backend_session_t *session =
         (vaapi_hevc_backend_session_t *)opaque;
   int key;
   for (;;)
   {
      key = retro_eventcount_prepare_wait(&session->wake);
      if (retro_atomic_load_acquire_int(&session->state))
      {
         retro_eventcount_cancel_wait(&session->wake);
         break;
      }
      retro_eventcount_commit_wait(&session->wake, key);
   }
   /* This is the cold retirement executor, never a frame/UI caller. */
   while (!vaapi_hevc_owner_retire(session->retired))
   {
      key = retro_eventcount_prepare_wait(&session->wake);
      retro_eventcount_commit_wait_timeout(&session->wake, key, 10000);
   }
   vaapi_hevc_backend_unref(session);
   return NULL;
}

static int vaapi_hevc_backend_decode(void *opaque,
      const rh265_video_params_t *params, const rh265_access_unit_t *au,
      const rh265_nal_t *slices, const rh265_hw_picture_t *picture)
{
   vaapi_hevc_backend_session_t *session =
         (vaapi_hevc_backend_session_t *)opaque;
   return session->native.decode(
         session->native.decoder, params, au, slices, picture);
}

static int vaapi_hevc_backend_output(
      void *opaque, unsigned slot, video_hw_frame_t *frame)
{
   vaapi_hevc_backend_session_t *session =
         (vaapi_hevc_backend_session_t *)opaque;
   return session->native.output(session->native.decoder, slot, frame);
}

static int vaapi_hevc_backend_drain(void *opaque)
{
   vaapi_hevc_backend_session_t *session =
         (vaapi_hevc_backend_session_t *)opaque;
   return session->native.drain(session->native.decoder);
}

static void vaapi_hevc_backend_close(rh265_hw_ops_t *ops)
{
   vaapi_hevc_backend_session_t *session;
   if (!ops || ops->decode != vaapi_hevc_backend_decode || !ops->decoder)
      return;
   session          = (vaapi_hevc_backend_session_t *)ops->decoder;
   session->retired = vaapi_hevc_owner_detach(&session->native);
   memset(ops, 0, sizeof(*ops));
   vaapi_hevc_backend_release(session);
}

static bool vaapi_hevc_backend_open(void *opaque, const rh265_video *source,
      const rh265_video_params_t *params, unsigned slots, rh265_hw_ops_t *ops)
{
   vaapi_hevc_backend_t *backend = (vaapi_hevc_backend_t *)opaque;
   vaapi_hevc_backend_session_t *session;
   rh265_video_color_t coded;
   video_color_metadata_t color;
   pthread_attr_t attr;
   pthread_t thread;
   char path[1024];
   unsigned major_id, minor_id, yuv;
   int epoch, active, result;
   if (!ops || ops->decoder || !backend || !source || !params || slots < 2
         || slots > 32 || params->pps_id < 0
         || (params->bit_depth != 8 && params->bit_depth != 10)
         || params->chroma_format_idc != 1)
      return false;
   epoch = retro_atomic_load_acquire_int(&backend->epoch);
   if (epoch <= 0 || !(epoch & 1))
      return false;
   major_id = (unsigned)retro_atomic_load_acquire_int(&backend->major_id);
   minor_id = (unsigned)retro_atomic_load_acquire_int(&backend->minor_id);
   if (retro_atomic_load_acquire_int(&backend->epoch) != epoch
         || rh265_video_get_color(source, (unsigned)params->pps_id, &coded))
      return false;
   memset(&color, 0, sizeof(color));
   color.primaries = coded.primaries;
   color.transfer  = coded.transfer;
   color.matrix    = coded.matrix;
   color.range     = coded.full_range ? 2 : 1;
   yuv             = coded.full_range ? IMAGE_YUV_FLAG_FULL_RANGE : 0;
   if (coded.matrix == 1)
      yuv |= IMAGE_YUV_FLAG_BT709;
   for (;;)
   {
      active = retro_atomic_load_acquire_int(&backend->active);
      if (active < 0 || (unsigned)active >= backend->max_sessions)
         return false;
      if (retro_atomic_cas_int(&backend->active, active, active + 1))
         break;
   }
   if (!VAAPI_HEVC_NODE_PATH(major_id, minor_id, path, sizeof(path))
         || retro_atomic_load_acquire_int(&backend->epoch) != epoch)
   {
      retro_atomic_fetch_sub_int(&backend->active, 1);
      return false;
   }
   session = (vaapi_hevc_backend_session_t *)calloc(1, sizeof(*session));
   if (!session)
   {
      retro_atomic_fetch_sub_int(&backend->active, 1);
      return false;
   }
   session->backend = backend;
   if (!retro_eventcount_init(&session->wake))
   {
      free(session);
      retro_atomic_fetch_sub_int(&backend->active, 1);
      return false;
   }
   retro_atomic_int_init(&session->state, 0);
   retro_atomic_int_init(&session->refs, 1);
   if (pthread_attr_init(&attr))
   {
      vaapi_hevc_backend_unref(session);
      return false;
   }
   if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
   {
      pthread_attr_destroy(&attr);
      vaapi_hevc_backend_unref(session);
      return false;
   }
   retro_atomic_store_release_int(&session->refs, 2);
   result = pthread_create(&thread, &attr, vaapi_hevc_backend_retire, session);
   pthread_attr_destroy(&attr);
   if (result)
   {
      retro_atomic_store_release_int(&session->refs, 1);
      vaapi_hevc_backend_unref(session);
      return false;
   }
   if (!vaapi_hevc_owner_open_source(&session->retired, source, path, major_id,
             minor_id, params, &color, yuv, slots, backend->max_input,
             &session->native))
   {
      vaapi_hevc_backend_release(session);
      return false;
   }
   if (retro_atomic_load_acquire_int(&backend->epoch) != epoch)
   {
      session->retired = vaapi_hevc_owner_detach(&session->native);
      vaapi_hevc_backend_release(session);
      return false;
   }
   memset(ops, 0, sizeof(*ops));
   ops->decoder = session;
   ops->decode  = vaapi_hevc_backend_decode;
   ops->output  = vaapi_hevc_backend_output;
   ops->drain   = vaapi_hevc_backend_drain;
   return true;
}
#endif

bool vaapi_hevc_backend_init(
      vaapi_hevc_backend_t *backend, size_t max_input, unsigned max_sessions)
{
#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_LOCK_FREE)                   \
      && defined(RETRO_ATOMIC_HAS_PTR)
   if (!backend || max_input < 128 || max_input > 64u * 1024u * 1024u
         || !max_sessions || max_sessions > 8)
      return false;
   memset(backend, 0, sizeof(*backend));
   backend->max_input    = max_input;
   backend->max_sessions = max_sessions;
   retro_atomic_int_init(&backend->epoch, 0);
   retro_atomic_int_init(&backend->major_id, 0);
   retro_atomic_int_init(&backend->minor_id, 0);
   retro_atomic_int_init(&backend->active, 0);
   backend->offer.user  = backend;
   backend->offer.open  = vaapi_hevc_backend_open;
   backend->offer.close = vaapi_hevc_backend_close;
   return true;
#else
   (void)backend;
   (void)max_input;
   (void)max_sessions;
   return false;
#endif
}

bool vaapi_hevc_backend_publish(vaapi_hevc_backend_t *backend, void *owner,
      unsigned major_id, unsigned minor_id)
{
   int epoch;
   if (!backend || !backend->offer.open || !owner || major_id > INT_MAX
         || minor_id > INT_MAX)
      return false;
   epoch = retro_atomic_load_acquire_int(&backend->epoch);
   if (epoch < 0)
      return false;
   if (epoch >= INT_MAX - 2)
   {
      retro_atomic_store_release_int(&backend->epoch, -1);
      backend->owner = NULL;
      rh265_unset_hw_backend(&backend->offer);
      return false;
   }
   if (epoch & 1)
      epoch++;
   retro_atomic_store_release_int(&backend->epoch, epoch);
   retro_atomic_store_release_int(&backend->major_id, (int)major_id);
   retro_atomic_store_release_int(&backend->minor_id, (int)minor_id);
   backend->owner = owner;
   retro_atomic_store_release_int(&backend->epoch, epoch + 1);
   rh265_set_hw_backend(&backend->offer);
   return true;
}

bool vaapi_hevc_backend_withdraw(vaapi_hevc_backend_t *backend, void *owner)
{
   int epoch;
   if (!backend || !backend->offer.open || !owner || backend->owner != owner)
      return false;
   epoch = retro_atomic_load_acquire_int(&backend->epoch);
   retro_atomic_store_release_int(
         &backend->epoch, epoch == INT_MAX ? -1 : epoch + 1);
   backend->owner = NULL;
   rh265_unset_hw_backend(&backend->offer);
   return true;
}
