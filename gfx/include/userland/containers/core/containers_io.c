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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "containers/containers.h"
#include "containers/core/containers_io.h"
#include "containers/core/containers_common.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_uri.h"

#define MAX_NUM_CACHED_AREAS 16
#define MAX_NUM_MEMORY_AREAS 4
#define NUM_TMP_MEMORY_AREAS 2
#define MEM_CACHE_READ_MAX_SIZE (32*1024) /* Needs to be a power of 2 */
#define MEM_CACHE_WRITE_MAX_SIZE (128*1024) /* Needs to be a power of 2 */
#define MEM_CACHE_TMP_MAX_SIZE (32*1024) /* Needs to be a power of 2 */
#define MEM_CACHE_ALIGNMENT (1*1024) /* Needs to be a power of 2 */
#define MEM_CACHE_AREA_READ_MAX_SIZE (4*1024*1024) /* Needs to be a power of 2 */

typedef struct VC_CONTAINER_IO_PRIVATE_CACHE_T
{
   int64_t start; /**< Offset to the start of the cached area in the stream */
   int64_t end;    /**< Offset to the end of the cached area in the stream */

   int64_t offset; /**< Offset of the currently cached data in the stream */
   size_t size;    /**< Size of the cached area */
   bool dirty;     /**< Whether the cache is dirty and needs to be written back */

   size_t position; /**< Current position in the cache */

   uint8_t *buffer;          /**< Pointer to the start of the valid cache area */
   uint8_t *buffer_end;      /**< Pointer to the end of the cache */

   unsigned int mem_max_size; /**< Maximum size of the memory cache */
   unsigned int mem_size; /**< Size of the memory cache */
   uint8_t *mem;          /**< Pointer to the memory cache */

   VC_CONTAINER_IO_T *io;

} VC_CONTAINER_IO_PRIVATE_CACHE_T;

typedef struct VC_CONTAINER_IO_PRIVATE_T
{
   VC_CONTAINER_IO_PRIVATE_CACHE_T *cache; /**< Current cache */

   unsigned int caches_num;
   VC_CONTAINER_IO_PRIVATE_CACHE_T caches;

   unsigned int cached_areas_num;
   VC_CONTAINER_IO_PRIVATE_CACHE_T cached_areas[MAX_NUM_CACHED_AREAS];

   int64_t actual_offset;

   struct VC_CONTAINER_IO_ASYNC_T *async_io;

} VC_CONTAINER_IO_PRIVATE_T;

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_io_file_open( VC_CONTAINER_IO_T *p_ctx, const char *uri,
                                                 VC_CONTAINER_IO_MODE_T mode );
VC_CONTAINER_STATUS_T vc_container_io_null_open( VC_CONTAINER_IO_T *p_ctx, const char *uri,
                                                 VC_CONTAINER_IO_MODE_T mode );
VC_CONTAINER_STATUS_T vc_container_io_net_open( VC_CONTAINER_IO_T *p_ctx, const char *uri,
                                                 VC_CONTAINER_IO_MODE_T mode );
VC_CONTAINER_STATUS_T vc_container_io_pktfile_open( VC_CONTAINER_IO_T *p_ctx, const char *uri,
                                                 VC_CONTAINER_IO_MODE_T mode );
VC_CONTAINER_STATUS_T vc_container_io_http_open( VC_CONTAINER_IO_T *p_ctx, const char *uri,
                                                 VC_CONTAINER_IO_MODE_T mode );
static VC_CONTAINER_STATUS_T io_seek_not_seekable(VC_CONTAINER_IO_T *p_ctx, int64_t offset);

static size_t vc_container_io_cache_read( VC_CONTAINER_IO_T *p_ctx,
   VC_CONTAINER_IO_PRIVATE_CACHE_T *cache, uint8_t *data, size_t size );
static int32_t vc_container_io_cache_write( VC_CONTAINER_IO_T *p_ctx,
   VC_CONTAINER_IO_PRIVATE_CACHE_T *cache, const uint8_t *data, size_t size );
static VC_CONTAINER_STATUS_T vc_container_io_cache_seek( VC_CONTAINER_IO_T *p_ctx,
   VC_CONTAINER_IO_PRIVATE_CACHE_T *cache, int64_t offset );
static size_t vc_container_io_cache_refill( VC_CONTAINER_IO_T *p_ctx,
   VC_CONTAINER_IO_PRIVATE_CACHE_T *cache );
static size_t vc_container_io_cache_flush( VC_CONTAINER_IO_T *p_ctx,
   VC_CONTAINER_IO_PRIVATE_CACHE_T *cache, int complete );

static struct VC_CONTAINER_IO_ASYNC_T *async_io_start( VC_CONTAINER_IO_T *io, int num_areas, VC_CONTAINER_STATUS_T * );
static VC_CONTAINER_STATUS_T async_io_stop( struct VC_CONTAINER_IO_ASYNC_T *ctx );
static int async_io_write( struct VC_CONTAINER_IO_ASYNC_T *ctx, VC_CONTAINER_IO_PRIVATE_CACHE_T *cache );
static VC_CONTAINER_STATUS_T async_io_wait_complete( struct VC_CONTAINER_IO_ASYNC_T *ctx,
                                                     VC_CONTAINER_IO_PRIVATE_CACHE_T *cache, int complete  );
static void async_io_stats_initialise( struct VC_CONTAINER_IO_ASYNC_T *ctx, int enable );
static void async_io_stats_get( struct VC_CONTAINER_IO_ASYNC_T *ctx, VC_CONTAINER_WRITE_STATS_T *stats );

/*****************************************************************************/
static VC_CONTAINER_IO_T *vc_container_io_open_core( const char *uri, VC_CONTAINER_IO_MODE_T mode,
                                                     VC_CONTAINER_IO_CAPABILITIES_T capabilities,
                                                     bool b_open, VC_CONTAINER_STATUS_T *p_status )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_IO_T *p_ctx = 0;
   VC_CONTAINER_IO_PRIVATE_T *private = 0;
   unsigned int uri_length, caches = 0, cache_max_size, num_areas = MAX_NUM_MEMORY_AREAS;

   /* XXX */
   uri_length = strlen(uri) + 1;

   /* Allocate our context before trying out the different io modules */
   p_ctx = malloc( sizeof(*p_ctx) + sizeof(*private) + uri_length);
   if(!p_ctx) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(p_ctx, 0, sizeof(*p_ctx) + sizeof(*private) + uri_length );
   p_ctx->priv = private = (VC_CONTAINER_IO_PRIVATE_T *)&p_ctx[1];
   p_ctx->uri = (char *)&private[1];
   memcpy((char *)p_ctx->uri, uri, uri_length);
   p_ctx->uri_parts = vc_uri_create();
   if(!p_ctx->uri_parts) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   vc_uri_parse(p_ctx->uri_parts, uri);

   if (b_open)
   {
      /* Open the actual i/o module */
      status = vc_container_io_null_open(p_ctx, uri, mode);
      if(status) status = vc_container_io_net_open(p_ctx, uri, mode);
      if(status) status = vc_container_io_pktfile_open(p_ctx, uri, mode);
#ifdef ENABLE_CONTAINER_IO_HTTP
      if(status) status = vc_container_io_http_open(p_ctx, uri, mode);
#endif
      if(status) status = vc_container_io_file_open(p_ctx, uri, mode);
      if(status != VC_CONTAINER_SUCCESS) goto error;

      if(!p_ctx->pf_seek || (p_ctx->capabilities & VC_CONTAINER_IO_CAPS_CANT_SEEK))
      {
         p_ctx->capabilities |= VC_CONTAINER_IO_CAPS_CANT_SEEK;
         p_ctx->pf_seek = io_seek_not_seekable;
      }
   }
   else
   {
      /* We're only creating an empty container i/o */
      p_ctx->capabilities = capabilities;
   }

   if(p_ctx->capabilities & VC_CONTAINER_IO_CAPS_NO_CACHING)
      caches = 1;

   if(mode == VC_CONTAINER_IO_MODE_WRITE) cache_max_size = MEM_CACHE_WRITE_MAX_SIZE;
   else cache_max_size = MEM_CACHE_READ_MAX_SIZE;

   if(mode == VC_CONTAINER_IO_MODE_WRITE &&
      vc_uri_path_extension(p_ctx->uri_parts) &&
      !strcasecmp(vc_uri_path_extension(p_ctx->uri_parts), "tmp"))
   {
      caches = 1;
      cache_max_size = MEM_CACHE_TMP_MAX_SIZE;
      num_areas = NUM_TMP_MEMORY_AREAS;
   }

   /* Check if the I/O needs caching */
   if(caches)
   {
      VC_CONTAINER_IO_PRIVATE_CACHE_T *cache = &p_ctx->priv->caches;
      cache->mem_max_size = cache_max_size;
      cache->mem_size = cache->mem_max_size;
      cache->io = p_ctx;
      cache->mem = malloc(p_ctx->priv->caches.mem_size);
      if(cache->mem)
      {      
         cache->buffer = cache->mem;
         cache->buffer_end = cache->mem + cache->mem_size;
         p_ctx->priv->caches_num = 1;
      }
   }

   if(p_ctx->priv->caches_num)
      p_ctx->priv->cache = &p_ctx->priv->caches;


   /* Try to start an asynchronous io if we're in write mode and we've got at least 2 cache memory areas */
   if(mode == VC_CONTAINER_IO_MODE_WRITE && p_ctx->priv->cache && num_areas >= 2)
      p_ctx->priv->async_io = async_io_start( p_ctx, num_areas, 0 );

 end:
   if(p_status) *p_status = status;
   return p_ctx;

 error:
   if(p_ctx) vc_uri_release(p_ctx->uri_parts);
   if(p_ctx) free(p_ctx);
   p_ctx = 0;
   goto end;
}

/*****************************************************************************/
VC_CONTAINER_IO_T *vc_container_io_open( const char *uri, VC_CONTAINER_IO_MODE_T mode,
                                         VC_CONTAINER_STATUS_T *p_status )
{
   return vc_container_io_open_core( uri, mode, 0, true, p_status );
}

/*****************************************************************************/
VC_CONTAINER_IO_T *vc_container_io_create( const char *uri, VC_CONTAINER_IO_MODE_T mode,
                                           VC_CONTAINER_IO_CAPABILITIES_T capabilities,
                                           VC_CONTAINER_STATUS_T *p_status )
{
   return vc_container_io_open_core( uri, mode, capabilities, false, p_status );
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_io_close( VC_CONTAINER_IO_T *p_ctx )
{
   unsigned int i;

   if(p_ctx)
   {
      if(p_ctx->priv)
      {
         if(p_ctx->priv->caches_num)
         {
            if(p_ctx->priv->caches.dirty)
               vc_container_io_cache_flush( p_ctx, &p_ctx->priv->caches, 1 );
         }
         
         if(p_ctx->priv->async_io)
            async_io_stop( p_ctx->priv->async_io );
         else if(p_ctx->priv->caches_num)
            free(p_ctx->priv->caches.mem);

         for(i = 0; i < p_ctx->priv->cached_areas_num; i++)
            free(p_ctx->priv->cached_areas[i].mem);
         
         if(p_ctx->pf_close)
            p_ctx->pf_close(p_ctx);
      }
      vc_uri_release(p_ctx->uri_parts);
      free(p_ctx);
   }
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
size_t vc_container_io_peek(VC_CONTAINER_IO_T *p_ctx, void *buffer, size_t size)
{
   size_t ret;

   if(p_ctx->priv->cache)
   {
      /* FIXME: do something a bit more clever than this */
      int64_t offset = p_ctx->offset;
      ret = vc_container_io_read(p_ctx, buffer, size);
      vc_container_io_seek(p_ctx, offset);
      return ret;
   }

   if (p_ctx->capabilities & VC_CONTAINER_IO_CAPS_CANT_SEEK)
      return 0;

   ret = p_ctx->pf_read(p_ctx, buffer, size);
   p_ctx->pf_seek(p_ctx, p_ctx->offset);
   return ret;
}

/*****************************************************************************/
size_t vc_container_io_read(VC_CONTAINER_IO_T *p_ctx, void *buffer, size_t size)
{
   size_t ret;

   if(p_ctx->priv->cache)
      ret = vc_container_io_cache_read( p_ctx, p_ctx->priv->cache, (uint8_t*)buffer, size );
   else
   {
      ret = p_ctx->pf_read(p_ctx, buffer, size);
      p_ctx->priv->actual_offset += ret;
   }

   p_ctx->offset += ret;
   return ret;
}

/*****************************************************************************/
size_t vc_container_io_write(VC_CONTAINER_IO_T *p_ctx, const void *buffer, size_t size)
{
   int32_t ret;

   if(p_ctx->priv->cache)
      ret = vc_container_io_cache_write( p_ctx, p_ctx->priv->cache, (const uint8_t*)buffer, size );
   else
   {
      ret = p_ctx->pf_write(p_ctx, buffer, size);
      p_ctx->priv->actual_offset += ret;
   }

   p_ctx->offset += ret;
   return ret < 0 ? 0 : ret;
}

/*****************************************************************************/
size_t vc_container_io_skip(VC_CONTAINER_IO_T *p_ctx, size_t size)
{
   if(!size) return 0;

   if(size < 8)
   {
      uint8_t value[8];
      return vc_container_io_read(p_ctx, value, size);
   }

   if(p_ctx->priv->cache)
   {
      if(vc_container_io_cache_seek(p_ctx, p_ctx->priv->cache, p_ctx->offset + size)) return 0;
      p_ctx->offset += size;
      return size;
   }

   if(vc_container_io_seek(p_ctx, p_ctx->offset + size)) return 0;
   return size;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_io_seek(VC_CONTAINER_IO_T *p_ctx, int64_t offset)
{
   VC_CONTAINER_STATUS_T status;
   unsigned int i;

   /* Check if the requested position is in one of the cached areas */
   for(i = 0; i < p_ctx->priv->cached_areas_num; i++)
   {
      VC_CONTAINER_IO_PRIVATE_CACHE_T *cache = &p_ctx->priv->cached_areas[i];
      if(offset >= cache->start && offset < cache->end)
      {
         p_ctx->priv->cache = cache;
         break;
      }
   }
   if(i == p_ctx->priv->cached_areas_num)
      p_ctx->priv->cache = p_ctx->priv->caches_num ? &p_ctx->priv->caches : 0;

   if(p_ctx->priv->cache)
   {
      status = vc_container_io_cache_seek( p_ctx, p_ctx->priv->cache, offset );
      if(status == VC_CONTAINER_SUCCESS) p_ctx->offset = offset;
      return status;
   }

   if(p_ctx->status == VC_CONTAINER_SUCCESS &&
      offset == p_ctx->offset) return VC_CONTAINER_SUCCESS;

   status = p_ctx->pf_seek(p_ctx, offset);
   if(status == VC_CONTAINER_SUCCESS) p_ctx->offset = offset;
   p_ctx->priv->actual_offset = p_ctx->offset;
   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T io_seek_not_seekable(VC_CONTAINER_IO_T *p_ctx, int64_t offset)
{
   VC_CONTAINER_IO_PRIVATE_T *private = p_ctx->priv;

   vc_container_assert(offset >= private->actual_offset);
   if(offset == private->actual_offset)  return VC_CONTAINER_SUCCESS;

   if(offset < private->actual_offset)
   {
      p_ctx->status = VC_CONTAINER_ERROR_EOS;
      return p_ctx->status;
   }

   offset -= private->actual_offset;
   while(offset && !p_ctx->status)
   {
      uint8_t value[64];
      unsigned int ret, size = MIN(offset, 64);
      ret = p_ctx->pf_read(p_ctx, value, size);
      if(ret != size) p_ctx->status = VC_CONTAINER_ERROR_EOS;
      offset -= ret;
   }
   return p_ctx->status;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_io_control_list(VC_CONTAINER_IO_T *context, VC_CONTAINER_CONTROL_T operation, va_list args)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;

   if (context->pf_control)
      status = context->pf_control(context, operation, args);

   /* Option to add generic I/O control here */

   if(operation == VC_CONTAINER_CONTROL_IO_FLUSH && context->priv->cache)
   {
      status = VC_CONTAINER_SUCCESS;
      (void)vc_container_io_cache_flush( context, context->priv->cache, 1 );
   }

   if(operation == VC_CONTAINER_CONTROL_SET_IO_PERF_STATS && context->priv->async_io)
   {
      status = VC_CONTAINER_SUCCESS;
      async_io_stats_initialise(context->priv->async_io, va_arg(args, int));
   }

   if(operation == VC_CONTAINER_CONTROL_GET_IO_PERF_STATS && context->priv->async_io)
   {
      status = VC_CONTAINER_SUCCESS;
      async_io_stats_get(context->priv->async_io, va_arg(args, VC_CONTAINER_WRITE_STATS_T *));
   }

   return status;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_io_control(VC_CONTAINER_IO_T *context, VC_CONTAINER_CONTROL_T operation, ...)
{
   VC_CONTAINER_STATUS_T result;
   va_list args;

   va_start(args, operation);
   result = vc_container_io_control_list(context, operation, args);
   va_end(args);

   return result;
}

/*****************************************************************************/
size_t vc_container_io_cache(VC_CONTAINER_IO_T *p_ctx, size_t size)
{
   VC_CONTAINER_IO_PRIVATE_T *private = p_ctx->priv;
   VC_CONTAINER_IO_PRIVATE_CACHE_T *cache, *main_cache;
   VC_CONTAINER_STATUS_T status;

   /* Sanity checking */
   if(private->cached_areas_num >= MAX_NUM_CACHED_AREAS) return 0;

   cache = &private->cached_areas[private->cached_areas_num];
   cache->start = p_ctx->offset;
   cache->end = cache->start + size;
   cache->offset = p_ctx->offset;
   cache->position = 0;
   cache->size = 0;
   cache->io = p_ctx;

   /* Set the size of the cache area depending on the capabilities of the i/o */
   if(p_ctx->capabilities & VC_CONTAINER_IO_CAPS_CANT_SEEK)
      cache->mem_max_size = MEM_CACHE_AREA_READ_MAX_SIZE;
   else if((p_ctx->capabilities & VC_CONTAINER_IO_CAPS_SEEK_SLOW) &&
           size <= MEM_CACHE_AREA_READ_MAX_SIZE)
      cache->mem_max_size = MEM_CACHE_AREA_READ_MAX_SIZE;
   else
      cache->mem_max_size = MEM_CACHE_READ_MAX_SIZE;

   cache->mem_size = size;
   if(cache->mem_size > cache->mem_max_size) cache->mem_size = cache->mem_max_size;
   cache->mem = malloc(cache->mem_size);

   cache->buffer = cache->mem;
   cache->buffer_end = cache->mem + cache->mem_size;

   if(!cache->mem) return 0;
   private->cached_areas_num++;

   /* Copy any data we've got in the current cache into the new cache */
   main_cache = p_ctx->priv->cache;
   if(main_cache && main_cache->position < main_cache->size)
   {
      cache->size = main_cache->size - main_cache->position;
      if(cache->size > cache->mem_size) cache->size = cache->mem_size;
      memcpy(cache->buffer, main_cache->buffer + main_cache->position, cache->size);
      main_cache->position += cache->size;
   }

   /* Read the rest of the cache directly from the stream */
   if(cache->mem_size > cache->size)
   {
      size_t ret = cache->io->pf_read(cache->io, cache->buffer + cache->size,
                                      cache->mem_size - cache->size);
      cache->size += ret;
      cache->io->priv->actual_offset = cache->offset + cache->size;
   }

   status = vc_container_io_seek(p_ctx, cache->end);
   if(status != VC_CONTAINER_SUCCESS)
      return 0;

   if(p_ctx->capabilities & VC_CONTAINER_IO_CAPS_CANT_SEEK)
      return cache->size;
   else
      return size;
}

/*****************************************************************************/
static size_t vc_container_io_cache_refill( VC_CONTAINER_IO_T *p_ctx,
   VC_CONTAINER_IO_PRIVATE_CACHE_T *cache )
{
   size_t ret = vc_container_io_cache_flush( p_ctx, cache, 1 );

   if(ret) return 0; /* TODO what should we do there ? */

   if(p_ctx->priv->actual_offset != cache->offset)
   {
      if(cache->io->pf_seek(cache->io, cache->offset) != VC_CONTAINER_SUCCESS)
         return 0;
   }

   ret = cache->io->pf_read(cache->io, cache->buffer, cache->buffer_end - cache->buffer);
   cache->size = ret;
   cache->position = 0;
   cache->io->priv->actual_offset = cache->offset + ret;
   return ret;
}

/*****************************************************************************/
static size_t vc_container_io_cache_refill_bypass( VC_CONTAINER_IO_T *p_ctx,
   VC_CONTAINER_IO_PRIVATE_CACHE_T *cache, uint8_t *buffer, size_t size )
{
   size_t ret = vc_container_io_cache_flush( p_ctx, cache, 1 );

   if(ret) return 0; /* TODO what should we do there ? */

   if(p_ctx->priv->actual_offset != cache->offset)
   {
      if(cache->io->pf_seek(cache->io, cache->offset) != VC_CONTAINER_SUCCESS)
         return 0;
   }

   ret = cache->io->pf_read(cache->io, buffer, size);
   cache->size = cache->position = 0;
   cache->offset += ret;
   cache->io->priv->actual_offset = cache->offset;
   return ret;
}

/*****************************************************************************/
static size_t vc_container_io_cache_read( VC_CONTAINER_IO_T *p_ctx,
   VC_CONTAINER_IO_PRIVATE_CACHE_T *cache, uint8_t *data, size_t size )
{
   size_t read = 0, bytes, ret;

   while(size)
   {
      bytes = cache->size - cache->position; /* Bytes left in cache */

#if 1 // FIXME Only if stream is seekable
      /* Try to read directly from the stream if the cache just gets in the way */
      if(!bytes && size > cache->mem_size)
      {
         bytes = cache->mem_size;
         ret = vc_container_io_cache_refill_bypass( p_ctx, cache, data + read, bytes);
         read += ret;

         if(ret != bytes) /* We didn't read as many bytes as we had hoped */
            goto end;

         size -= bytes;
         continue;
      }
#endif

      /* Refill the cache if it is empty */
      if(!bytes) bytes = vc_container_io_cache_refill( p_ctx, cache );
      if(!bytes) goto end;

      /* We do have some data in the cache so override the status */
      p_ctx->status = VC_CONTAINER_SUCCESS;

      /* Read data directly from the cache */
      if(bytes > size) bytes = size;
      memcpy(data + read, cache->buffer + cache->position, bytes);
      cache->position += bytes;
      read += bytes;
      size -= bytes;
   }

 end:
   vc_container_assert(cache->offset + cache->position == p_ctx->offset + read);
   return read;
}

/*****************************************************************************/
static int32_t vc_container_io_cache_write( VC_CONTAINER_IO_T *p_ctx,
   VC_CONTAINER_IO_PRIVATE_CACHE_T *cache, const uint8_t *data, size_t size )
{
   int32_t written = 0;
   size_t bytes, ret;

   /* If we do not have a write cache then we need to flush it */
   if(cache->size && !cache->dirty)
   {
      ret = vc_container_io_cache_flush( p_ctx, cache, 1 );
      if(ret) return -(int32_t)ret;
   }

   while(size)
   {
      bytes = (cache->buffer_end - cache->buffer) - cache->position; /* Space left in cache */

      /* Flush the cache if it is full */
      if(!bytes)
      {
         /* Cache full, flush it */
         ret = vc_container_io_cache_flush( p_ctx, cache, 0 );
         if(ret)
         {
            written -= ret;
            return written;
         }
         continue;
      }

      if(bytes > size) bytes = size;

      if(!p_ctx->priv->async_io && bytes == cache->mem_size)
      {
         /* Write directly from the buffer */
         ret = cache->io->pf_write(cache->io, data + written, bytes);
         cache->offset += ret;
         cache->io->priv->actual_offset += ret;
      }
      else
      {
         /* Write in the cache */
         memcpy(cache->buffer + cache->position, data + written, bytes);
         cache->position += bytes;
         cache->dirty = 1;
         ret = bytes;
      }

      written += ret;
      if(ret != bytes) goto end;

      size -= bytes;
   }

 end:
   vc_container_assert(cache->offset + (int64_t)cache->position == p_ctx->offset + written);
   if(cache->position > cache->size) cache->size = cache->position;
   return written;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T vc_container_io_cache_seek(VC_CONTAINER_IO_T *p_ctx,
   VC_CONTAINER_IO_PRIVATE_CACHE_T *cache, int64_t offset)
{
   VC_CONTAINER_STATUS_T status;
   size_t shift, ret;

   /* Check if the seek position is within our cache */
   if(offset >= cache->offset && offset < cache->offset + (int64_t)cache->size)
   {
      cache->position = offset - cache->offset;
      return VC_CONTAINER_SUCCESS;
   }

   shift = cache->buffer - cache->mem;
   if(!cache->dirty && shift && cache->size &&
      offset >= cache->offset - (int64_t)shift && offset < cache->offset)
   {
      /* We need to refill the partial bit of the cache that we didn't take care of last time */
      status = cache->io->pf_seek(cache->io, cache->offset - shift);
      if(status != VC_CONTAINER_SUCCESS) return status;
      cache->offset -= shift;
      cache->buffer -= shift;

      ret = cache->io->pf_read(cache->io, cache->buffer, shift);
      vc_container_assert(ret == shift); /* FIXME: ret must = shift */
      cache->size += shift;
      cache->position = offset - cache->offset;
      cache->io->priv->actual_offset = cache->offset + ret;
      return VC_CONTAINER_SUCCESS;
   }

   if(cache->dirty) vc_container_io_cache_flush( p_ctx, cache, 1 );
   // FIXME: what if all the data couldn't be flushed ?

   if(p_ctx->priv->async_io) async_io_wait_complete( p_ctx->priv->async_io, cache, 1 );

   status = cache->io->pf_seek(cache->io, offset);
   if(status != VC_CONTAINER_SUCCESS) return status;

   vc_container_io_cache_flush( p_ctx, cache, 1 );

   cache->offset = offset;
   cache->io->priv->actual_offset = offset;
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static size_t vc_container_io_cache_flush( VC_CONTAINER_IO_T *p_ctx,
   VC_CONTAINER_IO_PRIVATE_CACHE_T *cache, int complete )
{
   size_t ret = 0, shift;

   if(cache->position > cache->size) cache->size = cache->position;

   if(cache->dirty && cache->size)
   {
      if(p_ctx->priv->actual_offset != cache->offset)
      {
         if(p_ctx->priv->async_io) async_io_wait_complete( p_ctx->priv->async_io, cache, complete );

         if(cache->io->pf_seek(cache->io, cache->offset) != VC_CONTAINER_SUCCESS)
            return 0;
      }

      if(p_ctx->priv->async_io)
      {
         ret = async_io_write( p_ctx->priv->async_io, cache );
         if(async_io_wait_complete( p_ctx->priv->async_io, cache, complete ) != VC_CONTAINER_SUCCESS)
            ret = 0;
      }
      else
         ret = cache->io->pf_write(cache->io, cache->buffer, cache->size);

      cache->io->priv->actual_offset = cache->offset + ret;
      ret = cache->position - ret;
   }
   cache->dirty = 0;

   cache->offset += cache->size;
   if(cache->mem_size == cache->mem_max_size)
   {
      shift = cache->offset &(MEM_CACHE_ALIGNMENT-1);
      cache->buffer = cache->mem + shift;
   }

   cache->position = cache->size = 0;
   return ret;
}

/*****************************************************************************
 * Asynchronous I/O.
 * This is here to keep the I/O as busy as possible by allowing the writer
 * to continue its work while the I/O is taking place in the background.
 *****************************************************************************/

#ifdef ENABLE_CONTAINERS_ASYNC_IO
#include "vcos.h"

#define NUMPC(c,n,s) ((c) < (1<<(s)) ? (n) : ((n) / (c >> (s))))

static void stats_initialise(VC_CONTAINER_STATS_T *st, uint32_t shift)
{
   memset(st, 0, sizeof(VC_CONTAINER_STATS_T));
   st->shift = shift;
}

static void stats_add_value(VC_CONTAINER_STATS_T *st, uint32_t count, uint32_t num)
{
   uint32_t numpc;
   int i, j;

   if(count == 0) 
      return;

   numpc = NUMPC(count, num, st->shift);
   // insert in the right place
   i=0;
   while(i < VC_CONTAINER_STATS_BINS && st->record[i].count != 0 && st->record[i].numpc > numpc)
      i++;

   if(st->record[i].count != 0 && st->record[i].numpc == numpc)
   {
      // equal numpc, can merge now
      st->record[i].count += count;
      st->record[i].num += num;
   }
   else
   {
      // shift higher records up
      for(j=VC_CONTAINER_STATS_BINS; j>i; j--)
         st->record[j] = st->record[j-1];

      // write record in
      st->record[i].count = count;
      st->record[i].num = num;
      st->record[i].numpc = numpc;

      // if full, join the two closest records
      if(st->record[VC_CONTAINER_STATS_BINS].count)
      {
         uint32_t min_diff = 0;
         j = -1;

         // find closest, based on difference between numpc
         for(i=0; i<VC_CONTAINER_STATS_BINS; i++)
         {
            uint32_t diff = st->record[i].numpc - st->record[i+1].numpc;
            if(j == -1 || diff < min_diff)
            {
               j = i;
               min_diff = diff;
            }
         }

         // merge these records
         st->record[j].count += st->record[j+1].count;
         st->record[j].num += st->record[j+1].num;
         st->record[j].numpc = NUMPC(st->record[j].count, st->record[j].num, st->shift);

         // shift down higher records
         while(++j < VC_CONTAINER_STATS_BINS)
            st->record[j] = st->record[j+1];

         // zero the free top record
         st->record[VC_CONTAINER_STATS_BINS].count = 0;
         st->record[VC_CONTAINER_STATS_BINS].num = 0;
         st->record[VC_CONTAINER_STATS_BINS].numpc = 0;
      }
   }
}

typedef struct VC_CONTAINER_IO_ASYNC_T
{
   VC_CONTAINER_IO_T *io;
   VCOS_THREAD_T thread;
   VCOS_SEMAPHORE_T spare_sema;
   VCOS_SEMAPHORE_T queue_sema;
   VCOS_EVENT_T wake_event;
   int quit;

   unsigned int num_area;
   uint8_t *mem[MAX_NUM_MEMORY_AREAS];    /**< Base address of memory areas */
   uint8_t *buffer[MAX_NUM_MEMORY_AREAS]; /**< When queued for writing, pointer to start of valid cache area */
   size_t size[MAX_NUM_MEMORY_AREAS];     /**< When queued for writing, size of valid area to write */
   unsigned int cur_area;

   unsigned char stack[3000];
   int error;

   int stats_enable;
   VC_CONTAINER_WRITE_STATS_T stats;

} VC_CONTAINER_IO_ASYNC_T;

/*****************************************************************************/
static void async_io_stats_initialise( struct VC_CONTAINER_IO_ASYNC_T *ctx, int enable )
{
   ctx->stats_enable = enable;
   stats_initialise(&ctx->stats.write, 8);
   stats_initialise(&ctx->stats.wait, 0);
   stats_initialise(&ctx->stats.flush, 0);
}

static void async_io_stats_get( struct VC_CONTAINER_IO_ASYNC_T *ctx, VC_CONTAINER_WRITE_STATS_T *stats )
{
   *stats = ctx->stats;
}

static void *async_io_thread(VOID *argv)
{
   VC_CONTAINER_IO_ASYNC_T *ctx = argv;
   unsigned int write_area = 0;

   while (1)
   {
      unsigned long time = 0;

      vcos_event_wait(&ctx->wake_event);
      if(ctx->quit) break;

      while(vcos_semaphore_trywait(&ctx->queue_sema) == VCOS_SUCCESS)
      {
         uint8_t *buffer = ctx->buffer[write_area];
         size_t size = ctx->size[write_area];

         if(ctx->stats_enable)
            time = vcos_getmicrosecs();

         if(ctx->io->pf_write(ctx->io, buffer, size) != size)
            ctx->error = 1;

         if(ctx->stats_enable)
            stats_add_value(&ctx->stats.write, size, vcos_getmicrosecs() - time);

         /* Signal that the write is done */
         vcos_semaphore_post(&ctx->spare_sema);

         if(++write_area == ctx->num_area)
            write_area = 0;
      }
   }

   return NULL;
}

static int async_io_write( VC_CONTAINER_IO_ASYNC_T *ctx, VC_CONTAINER_IO_PRIVATE_CACHE_T *cache )
{
   unsigned long time = 0;
   unsigned int offset;

   if(ctx->stats_enable)
      time = vcos_getmicrosecs();

   /* post the current area */
   ctx->buffer[ctx->cur_area] = cache->buffer;
   ctx->size[ctx->cur_area] = cache->size;
   vcos_semaphore_post(&ctx->queue_sema);
   vcos_event_signal(&ctx->wake_event);

   /* now we need to grab another area */
   vcos_semaphore_wait(&ctx->spare_sema);
   if(++ctx->cur_area == ctx->num_area)
      ctx->cur_area = 0;

   if(ctx->stats_enable)
      stats_add_value(&ctx->stats.wait, 1, vcos_getmicrosecs() - time);

   /* alter cache mem to point to the new cur_area */
   offset = cache->buffer - cache->mem;
   cache->mem = ctx->mem[ctx->cur_area];
   cache->buffer = cache->mem + offset;
   cache->buffer_end = cache->mem + cache->mem_size;

   return ctx->error ? 0 : cache->size;
}

static VC_CONTAINER_STATUS_T async_io_wait_complete( struct VC_CONTAINER_IO_ASYNC_T *ctx,
                                                     VC_CONTAINER_IO_PRIVATE_CACHE_T *cache, int complete )
{
   unsigned int time = 0;

   if(ctx->stats_enable)
      time = vcos_getmicrosecs();

   if(complete)
   {
      int num;
      /* Need to make sure that all memory areas have been written out, so should have num-1 spare */
      for(num=0; num<ctx->num_area-1; num++)
         vcos_semaphore_wait(&ctx->spare_sema);

      for(num=0; num<ctx->num_area-1; num++)
         vcos_semaphore_post(&ctx->spare_sema);
   }
   else
   {
      /* Need to make sure we can acquire one memory area */
      vcos_semaphore_wait(&ctx->spare_sema);
      vcos_semaphore_post(&ctx->spare_sema);
   }
   
   if(ctx->stats_enable)
      stats_add_value(&ctx->stats.flush, 1, vcos_getmicrosecs() - time);

   return ctx->error ? VC_CONTAINER_ERROR_FAILED : VC_CONTAINER_SUCCESS;
}

static VC_CONTAINER_IO_ASYNC_T *async_io_start( VC_CONTAINER_IO_T *io, int num_areas, VC_CONTAINER_STATUS_T *status )
{
   VC_CONTAINER_IO_ASYNC_T *ctx = 0;
   VCOS_UNSIGNED pri = 0;

   /* Allocate our context  */
   ctx = malloc(sizeof(*ctx));
   if(!ctx) goto error_spare_sema;
   memset(ctx, 0, sizeof(*ctx));
   ctx->io = io;

   ctx->mem[0] = io->priv->cache->mem;

   for(ctx->num_area = 1; ctx->num_area < num_areas; ctx->num_area++)
   {
      ctx->mem[ctx->num_area] = malloc(io->priv->cache->mem_size);
      if(!ctx->mem[ctx->num_area])
         break;
   }

   if(ctx->num_area == 1) // no real benefit in asynchronous writes
      goto error_spare_sema;

   async_io_stats_initialise(ctx, 0);

   if(vcos_semaphore_create(&ctx->spare_sema, "async_spare_sem", ctx->num_area-1) != VCOS_SUCCESS)
      goto error_spare_sema;

   if(vcos_semaphore_create(&ctx->queue_sema, "async_queue_sem", 0) != VCOS_SUCCESS)
      goto error_queue_sema;

   if (vcos_event_create(&ctx->wake_event, "async_wake_event") != VCOS_SUCCESS)
      goto error_event;

   // run this thread at a slightly higher priority than the calling thread - that means that
   // we prefer to write to the SD card rather than filling the memory buffer.
   pri = vcos_thread_get_priority(vcos_thread_current());
   if(vcos_thread_create_classic(&ctx->thread, "async_io", async_io_thread, ctx,
         ctx->stack, sizeof(ctx->stack), pri-1, 10, VCOS_START) != VCOS_SUCCESS)
      goto error_thread;

   if(status) *status = VC_CONTAINER_SUCCESS;
   return ctx;

 error_thread:
   vcos_event_delete(&ctx->wake_event);
 error_event:
   vcos_semaphore_delete(&ctx->queue_sema);
 error_queue_sema:
   vcos_semaphore_delete(&ctx->spare_sema);
 error_spare_sema:
   if(ctx) free(ctx);
   if(status) *status = VC_CONTAINER_ERROR_FAILED;
   return 0;
}

static VC_CONTAINER_STATUS_T async_io_stop( VC_CONTAINER_IO_ASYNC_T *ctx )
{
   /* Block if a write operation is already in progress */
   //vcos_semaphore_wait(&ctx->sema);
   // XXX block until all done

   ctx->quit = 1;
   vcos_event_signal(&ctx->wake_event);
   vcos_thread_join(&ctx->thread,NULL);
   vcos_event_delete(&ctx->wake_event);
   vcos_semaphore_delete(&ctx->queue_sema);
   vcos_semaphore_delete(&ctx->spare_sema);

   while(ctx->num_area > 0)
      free(ctx->mem[--ctx->num_area]);

   free(ctx);
   return VC_CONTAINER_SUCCESS;
}
#else

static struct VC_CONTAINER_IO_ASYNC_T *async_io_start( VC_CONTAINER_IO_T *io, int num_areas, VC_CONTAINER_STATUS_T *status )
{
   VC_CONTAINER_PARAM_UNUSED(io);
   VC_CONTAINER_PARAM_UNUSED(num_areas);
   if(status) *status = VC_CONTAINER_ERROR_FAILED;
   return 0;
}

static int async_io_write( struct VC_CONTAINER_IO_ASYNC_T *ctx, VC_CONTAINER_IO_PRIVATE_CACHE_T *cache )
{
   VC_CONTAINER_PARAM_UNUSED(ctx);
   VC_CONTAINER_PARAM_UNUSED(cache);
   return 0;
}

static VC_CONTAINER_STATUS_T async_io_wait_complete( struct VC_CONTAINER_IO_ASYNC_T *ctx,
                                                     VC_CONTAINER_IO_PRIVATE_CACHE_T *cache, int complete )
{
   VC_CONTAINER_PARAM_UNUSED(ctx);
   VC_CONTAINER_PARAM_UNUSED(cache);
   VC_CONTAINER_PARAM_UNUSED(complete);
   return 0;
}

static VC_CONTAINER_STATUS_T async_io_stop( struct VC_CONTAINER_IO_ASYNC_T *ctx )
{
   VC_CONTAINER_PARAM_UNUSED(ctx);
   return VC_CONTAINER_SUCCESS;
}

static void async_io_stats_initialise( struct VC_CONTAINER_IO_ASYNC_T *ctx, int enable )
{
   VC_CONTAINER_PARAM_UNUSED(ctx);
   VC_CONTAINER_PARAM_UNUSED(enable);
}

static void async_io_stats_get( struct VC_CONTAINER_IO_ASYNC_T *ctx, VC_CONTAINER_WRITE_STATS_T *stats )
{
   VC_CONTAINER_PARAM_UNUSED(ctx);
   VC_CONTAINER_PARAM_UNUSED(stats);
}


#endif
