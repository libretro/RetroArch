/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "vaapi_vp9_backend.h"
#include <formats/rvp9_color.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_LOCK_FREE)                   \
      && defined(RETRO_ATOMIC_HAS_PTR)
#include <pthread.h>
#include <rthreads/retro_eventcount.h>

#ifndef VAAPI_VP9_NODE_PATH
#define VAAPI_VP9_NODE_PATH vaapi_vp9_backend_node_path
#if defined(__linux__) && !defined(__ANDROID__) && defined(NEED_DYNAMIC)
#include <sys/sysmacros.h>
#include <xf86drm.h>

static bool vaapi_vp9_backend_node_path(
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
static bool vaapi_vp9_backend_node_path(
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

typedef struct vaapi_vp9_backend_session
{
   rvp9_hw_ops_t native;
   vaapi_vp9_owner_t *retired;
   vaapi_vp9_backend_t *backend;
   retro_eventcount_t wake;
   retro_atomic_int_t state, refs;
} vaapi_vp9_backend_session_t;

static void vaapi_vp9_backend_unref(vaapi_vp9_backend_session_t *session)
{
   vaapi_vp9_backend_t *backend;
   if (retro_atomic_fetch_sub_int(&session->refs, 1) != 1)
      return;
   backend = session->backend;
   retro_eventcount_free(&session->wake);
   free(session);
   retro_atomic_fetch_sub_int(&backend->active, 1);
}

static void vaapi_vp9_backend_release(vaapi_vp9_backend_session_t *session)
{
   retro_atomic_store_release_int(&session->state, 1);
   retro_eventcount_notify(&session->wake);
   vaapi_vp9_backend_unref(session);
}

static void *vaapi_vp9_backend_retire(void *opaque)
{
   vaapi_vp9_backend_session_t *session = (vaapi_vp9_backend_session_t *)opaque;
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
   while (!vaapi_vp9_owner_retire(session->retired))
   {
      key = retro_eventcount_prepare_wait(&session->wake);
      retro_eventcount_commit_wait_timeout(&session->wake, key, 10000);
   }
   vaapi_vp9_backend_unref(session);
   return NULL;
}

static int vaapi_vp9_backend_decode(void *opaque, const uint8_t *packet,
      size_t size, const rvp9_hw_picture_t *picture)
{
   vaapi_vp9_backend_session_t *session = (vaapi_vp9_backend_session_t *)opaque;
   return session->native.decode(
         session->native.decoder, packet, size, picture);
}

static int vaapi_vp9_backend_output(
      void *opaque, unsigned slot, video_hw_frame_t *frame)
{
   vaapi_vp9_backend_session_t *session = (vaapi_vp9_backend_session_t *)opaque;
   return session->native.output(session->native.decoder, slot, frame);
}

static void vaapi_vp9_backend_close(rvp9_hw_ops_t *ops)
{
   vaapi_vp9_backend_session_t *session;
   if (!ops || ops->decode != vaapi_vp9_backend_decode || !ops->decoder)
      return;
   session = (vaapi_vp9_backend_session_t *)ops->decoder;
   session->retired = vaapi_vp9_owner_detach(&session->native);
   memset(ops, 0, sizeof(*ops));
   vaapi_vp9_backend_release(session);
}

static bool vaapi_vp9_backend_open_config(void *opaque, const rvp9_hdr *header,
      const video_color_metadata_t *container, const rvp9_config_t *record,
      unsigned slots, rvp9_hw_ops_t *ops)
{
   vaapi_vp9_backend_t *backend = (vaapi_vp9_backend_t *)opaque;
   vaapi_vp9_backend_session_t *session;
   video_color_metadata_t color, configured;
   pthread_attr_t attr;
   pthread_t thread;
   char path[1024];
   unsigned major_id, minor_id;
   int epoch, active, result;
   if (!ops || ops->decoder || !backend || !header || slots < 10 || slots > 32
         || header->frame_type || header->show_existing_frame)
      return false;
   if (record)
   {
      if (record->profile != header->profile
            || record->bit_depth != header->bit_depth || record->chroma != 0
            || !rvp9_config_apply_color(record, container, &configured))
         return false;
      container = &configured;
   }
   if (!rvp9_color_resolve(header, container, &color))
      return false;
   epoch = retro_atomic_load_acquire_int(&backend->epoch);
   if (epoch <= 0 || !(epoch & 1))
      return false;
   major_id = (unsigned)retro_atomic_load_acquire_int(&backend->major_id);
   minor_id = (unsigned)retro_atomic_load_acquire_int(&backend->minor_id);
   if (retro_atomic_load_acquire_int(&backend->epoch) != epoch)
      return false;
   for (;;)
   {
      active = retro_atomic_load_acquire_int(&backend->active);
      if (active < 0 || (unsigned)active >= backend->max_sessions)
         return false;
      if (retro_atomic_cas_int(&backend->active, active, active + 1))
         break;
   }
   if (!VAAPI_VP9_NODE_PATH(major_id, minor_id, path, sizeof(path))
         || retro_atomic_load_acquire_int(&backend->epoch) != epoch)
   {
      retro_atomic_fetch_sub_int(&backend->active, 1);
      return false;
   }
   session = (vaapi_vp9_backend_session_t *)calloc(1, sizeof(*session));
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
      vaapi_vp9_backend_unref(session);
      return false;
   }
   if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
   {
      pthread_attr_destroy(&attr);
      vaapi_vp9_backend_unref(session);
      return false;
   }
   retro_atomic_store_release_int(&session->refs, 2);
   result = pthread_create(&thread, &attr, vaapi_vp9_backend_retire, session);
   pthread_attr_destroy(&attr);
   if (result)
   {
      retro_atomic_store_release_int(&session->refs, 1);
      vaapi_vp9_backend_unref(session);
      return false;
   }
   if (!vaapi_vp9_owner_open(&session->retired, path, major_id, minor_id,
             header, &color, slots, &session->native))
   {
      vaapi_vp9_backend_release(session);
      return false;
   }
   if (retro_atomic_load_acquire_int(&backend->epoch) != epoch)
   {
      session->retired = vaapi_vp9_owner_detach(&session->native);
      vaapi_vp9_backend_release(session);
      return false;
   }
   memset(ops, 0, sizeof(*ops));
   ops->decoder = session;
   ops->decode = vaapi_vp9_backend_decode;
   ops->output = vaapi_vp9_backend_output;
   return true;
}

static bool vaapi_vp9_backend_open_color(void *opaque, const rvp9_hdr *header,
      const video_color_metadata_t *color, unsigned slots, rvp9_hw_ops_t *ops)
{
   return vaapi_vp9_backend_open_config(
         opaque, header, color, NULL, slots, ops);
}

static bool vaapi_vp9_backend_open(
      void *opaque, const rvp9_hdr *header, unsigned slots, rvp9_hw_ops_t *ops)
{
   return vaapi_vp9_backend_open_color(opaque, header, NULL, slots, ops);
}
#endif

bool vaapi_vp9_backend_init(vaapi_vp9_backend_t *backend, unsigned max_sessions)
{
#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_LOCK_FREE)                   \
      && defined(RETRO_ATOMIC_HAS_PTR)
   if (!backend || !max_sessions || max_sessions > 8)
      return false;
   memset(backend, 0, sizeof(*backend));
   backend->max_sessions = max_sessions;
   retro_atomic_int_init(&backend->epoch, 0);
   retro_atomic_int_init(&backend->major_id, 0);
   retro_atomic_int_init(&backend->minor_id, 0);
   retro_atomic_int_init(&backend->active, 0);
   backend->offer.user = backend;
   backend->offer.open = vaapi_vp9_backend_open;
   backend->offer.close = vaapi_vp9_backend_close;
   backend->offer.open_color = vaapi_vp9_backend_open_color;
   backend->offer.open_config = vaapi_vp9_backend_open_config;
   return true;
#else
   (void)backend;
   (void)max_sessions;
   return false;
#endif
}

bool vaapi_vp9_backend_publish(vaapi_vp9_backend_t *backend, void *owner,
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
      rvp9_unset_hw_backend(&backend->offer);
      return false;
   }
   if (epoch & 1)
      epoch++;
   retro_atomic_store_release_int(&backend->epoch, epoch);
   retro_atomic_store_release_int(&backend->major_id, (int)major_id);
   retro_atomic_store_release_int(&backend->minor_id, (int)minor_id);
   backend->owner = owner;
   retro_atomic_store_release_int(&backend->epoch, epoch + 1);
   rvp9_set_hw_backend(&backend->offer);
   return true;
}

bool vaapi_vp9_backend_withdraw(vaapi_vp9_backend_t *backend, void *owner)
{
   int epoch;
   if (!backend || !backend->offer.open || !owner || backend->owner != owner)
      return false;
   epoch = retro_atomic_load_acquire_int(&backend->epoch);
   retro_atomic_store_release_int(
         &backend->epoch, epoch == INT_MAX ? -1 : epoch + 1);
   backend->owner = NULL;
   rvp9_unset_hw_backend(&backend->offer);
   return true;
}
