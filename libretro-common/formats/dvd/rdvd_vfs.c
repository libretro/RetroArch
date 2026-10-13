/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include <retro_atomic.h>
#include <formats/rdvd_vfs.h>
#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_HAS_PTR)
#include <retro_spsc.h>
#define RDVD_VFS_ENABLED

typedef struct rdvd_vfs_request
{
   RFILE *file;
   void *destination;
   uint64_t offset, ticket;
   size_t bytes;
} rdvd_vfs_request_t;
typedef struct rdvd_vfs_result { uint64_t ticket; int64_t bytes; } rdvd_vfs_result_t;
struct rdvd_vfs
{
   rdvd_source_backend_t backend;
   retro_spsc_t requests, results;
   void *files[9];
   uint64_t sizes[9];
   retro_atomic_int_t scheduled;
   bool owns_files;
};
static bool rdvd_vfs_submit(void *context, void *file, uint64_t offset,
      void *destination, size_t bytes, uint64_t ticket)
{
   rdvd_vfs_t *vfs = (rdvd_vfs_t*)context;
   rdvd_vfs_request_t request;
   if (offset > INT64_MAX || retro_spsc_write_avail(&vfs->requests) < sizeof(request))
      return false;
   memset(&request, 0, sizeof(request));
   request.file = (RFILE*)file; request.destination = destination;
   request.offset = offset; request.bytes = bytes; request.ticket = ticket;
   return retro_spsc_write(&vfs->requests, &request, sizeof(request)) == sizeof(request);
}
static enum rdvd_backend_status rdvd_vfs_poll(void *context, uint64_t *ticket, int64_t *bytes)
{
   rdvd_vfs_t *vfs = (rdvd_vfs_t*)context;
   rdvd_vfs_result_t result;
   if (retro_atomic_load_acquire_int(&vfs->scheduled)) return RDVD_BACKEND_PENDING;
   if (retro_spsc_read_avail(&vfs->results) < sizeof(result)) return RDVD_BACKEND_PENDING;
   if (retro_spsc_read(&vfs->results, &result, sizeof(result)) != sizeof(result))
      return RDVD_BACKEND_PENDING;
   *ticket = result.ticket; *bytes = result.bytes;
   return result.bytes < 0 ? RDVD_BACKEND_ERROR : RDVD_BACKEND_COMPLETE;
}
#endif

rdvd_vfs_t *rdvd_vfs_bind(RFILE *const *files, const uint64_t *sizes, unsigned count)
{
#ifdef RDVD_VFS_ENABLED
   rdvd_vfs_t *vfs;
   rdvd_vob_map_t map;
   rdvd_backend_ops_t ops;
   unsigned i;
   if (!files || !rdvd_vob_map_read(sizes, count, &map)) return NULL;
   for (i = 0; i < count; i++)
      if (!files[i] || sizes[i] > INT64_MAX) return NULL;
   vfs = (rdvd_vfs_t*)calloc(1, sizeof(*vfs));
   if (!vfs) return NULL;
   retro_atomic_int_init(&vfs->scheduled, 0);
   if (!retro_spsc_init(&vfs->requests, sizeof(rdvd_vfs_request_t))
         || !retro_spsc_init(&vfs->results, sizeof(rdvd_vfs_result_t)))
   {
      retro_spsc_free(&vfs->requests); retro_spsc_free(&vfs->results);
      free(vfs);
      return NULL;
   }
   for (i = 0; i < count; i++)
   {
      vfs->files[i] = files[i]; vfs->sizes[i] = sizes[i];
   }
   ops.submit = rdvd_vfs_submit; ops.poll = rdvd_vfs_poll;
   if (!rdvd_source_backend_init(&vfs->backend, &ops, vfs, vfs->files, vfs->sizes, count))
   {
      rdvd_vfs_close(&vfs);
      return NULL;
   }
   return vfs;
#else
   (void)files; (void)sizes; (void)count;
   return NULL;
#endif
}
rdvd_source_backend_t *rdvd_vfs_backend(rdvd_vfs_t *vfs)
{
#ifdef RDVD_VFS_ENABLED
   return vfs ? &vfs->backend : NULL;
#else
   (void)vfs;
   return NULL;
#endif
}
bool rdvd_vfs_close(rdvd_vfs_t **vfs)
{
#ifdef RDVD_VFS_ENABLED
   bool clean = true;
   unsigned i;
#endif
   if (!vfs) return false;
   if (!*vfs) return true;
#ifdef RDVD_VFS_ENABLED
   if ((*vfs)->backend.owner || retro_atomic_load_acquire_int(&(*vfs)->scheduled)
         || retro_spsc_read_avail(&(*vfs)->requests)
         || retro_spsc_read_avail(&(*vfs)->results)) return false;
   if ((*vfs)->owns_files)
      for (i = 0; i < (*vfs)->backend.map.segments; i++)
         if (filestream_close((RFILE*)(*vfs)->files[i]) != 0) clean = false;
   retro_spsc_free(&(*vfs)->requests); retro_spsc_free(&(*vfs)->results);
   free(*vfs); *vfs = NULL;
   return clean;
#else
   return false;
#endif
}


bool rdvd_vfs_service(rdvd_vfs_t *vfs)
{
#ifdef RDVD_VFS_ENABLED
   rdvd_vfs_request_t request;
   rdvd_vfs_result_t result;
   size_t bytes;
   if (!vfs || retro_spsc_write_avail(&vfs->results) < sizeof(result)
         || retro_spsc_read_avail(&vfs->requests) < sizeof(request)) return false;
   if (retro_spsc_read(&vfs->requests, &request, sizeof(request)) != sizeof(request))
      return false;
   memset(&result, 0, sizeof(result));
   result.ticket = request.ticket; result.bytes = -1;
   bytes = request.bytes < 65536 ? request.bytes : 65536;
   if (filestream_seek(request.file, (int64_t)request.offset,
            RETRO_VFS_SEEK_POSITION_START) == 0
         && filestream_tell(request.file) == (int64_t)request.offset)
   {
      result.bytes = filestream_read(request.file, request.destination, (int64_t)bytes);
      if (result.bytes > (int64_t)bytes) result.bytes = -1;
   }
   retro_spsc_write(&vfs->results, &result, sizeof(result));
   return true;
#else
   (void)vfs;
   return false;
#endif
}


rdvd_vfs_t *rdvd_vfs_open(const char *const *paths, unsigned count)
{
#ifdef RDVD_VFS_ENABLED
   RFILE *files[9];
   uint64_t sizes[9];
   rdvd_vfs_t *vfs = NULL;
   unsigned i, opened = 0;
   if (!paths || !count || count > 9) return NULL;
   memset(files, 0, sizeof(files));
   for (i = 0; i < count; i++)
   {
      int64_t size;
      if (!paths[i] || !*paths[i]) break;
      files[i] = filestream_open(paths[i], RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
      if (!files[i]) break;
      opened++;
      size = filestream_get_size(files[i]);
      if (size <= 0 || size % 2048) break;
      sizes[i] = (uint64_t)size;
   }
   if (i == count) vfs = rdvd_vfs_bind(files, sizes, count);
   if (vfs) vfs->owns_files = true;
   else
      for (i = 0; i < opened; i++) filestream_close(files[i]);
   return vfs;
#else
   (void)paths; (void)count;
   return NULL;
#endif
}


#ifdef RDVD_VFS_ENABLED
static void rdvd_vfs_job(void *context)
{
   rdvd_vfs_t *vfs = (rdvd_vfs_t*)context;
   rdvd_vfs_service(vfs);
   retro_atomic_store_release_int(&vfs->scheduled, 0);
}
#endif
bool rdvd_vfs_schedule(rdvd_vfs_t *vfs, tpool_t *pool)
{
#ifdef RDVD_VFS_ENABLED
   if (!vfs || !pool || !vfs->backend.owner
         || retro_spsc_read_avail(&vfs->requests) < sizeof(rdvd_vfs_request_t)
         || !retro_atomic_cas_int(&vfs->scheduled, 0, 1)) return false;
   if (!tpool_try_add_work(pool, rdvd_vfs_job, vfs))
   {
      retro_atomic_store_release_int(&vfs->scheduled, 0);
      return false;
   }
   return true;
#else
   (void)vfs; (void)pool;
   return false;
#endif
}
