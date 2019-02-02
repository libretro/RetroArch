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
#include <limits.h>

#include "containers/containers.h"
#include "containers/core/containers_common.h"
#include "containers/core/containers_io.h"
#include "containers/core/containers_uri.h"

/** Native byte order word */
#define NATIVE_BYTE_ORDER  0x50415753
/** Reverse of native byte order - need to swap bytes around */
#define SWAP_BYTE_ORDER    0x53574150

typedef struct VC_CONTAINER_IO_MODULE_T
{
   FILE *stream;
   bool is_native_order;
} VC_CONTAINER_IO_MODULE_T;

/** List of recognised schemes.
 * Note: always use lower case for the scheme name. */
static const char * recognised_schemes[] = {
   "rtp", "rtppkt", "rtsp", "rtsppkt", "pktfile",
};

VC_CONTAINER_STATUS_T vc_container_io_pktfile_open( VC_CONTAINER_IO_T *, const char *,
   VC_CONTAINER_IO_MODE_T );

/*****************************************************************************/
static bool recognise_scheme(const char *scheme)
{
   size_t ii;

   if (!scheme)
      return false;

   for (ii = 0; ii < countof(recognised_schemes); ii++)
   {
      if (strcmp(recognised_schemes[ii], scheme) == 0)
         return true;
   }

   return false;
}

/*****************************************************************************/
static uint32_t swap_byte_order( uint32_t value )
{
   /* Reverse the order of the bytes in the word */
   return ((value << 24) | ((value & 0xFF00) << 8) | ((value >> 8) & 0xFF00) | (value >> 24));
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T io_pktfile_close( VC_CONTAINER_IO_T *p_ctx )
{
   VC_CONTAINER_IO_MODULE_T *module = p_ctx->module;
   fclose(module->stream);
   free(module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static size_t io_pktfile_read(VC_CONTAINER_IO_T *p_ctx, void *buffer, size_t size)
{
   VC_CONTAINER_IO_MODULE_T *module = p_ctx->module;
   uint32_t length = 0;
   size_t ret;
   
   ret = fread(&length, 1, sizeof(length), module->stream);
   if (ret != sizeof(length))
   {
      if( feof(module->stream) ) p_ctx->status = VC_CONTAINER_ERROR_EOS;
      else p_ctx->status = VC_CONTAINER_ERROR_FAILED;
      return 0;
   }

   if (!module->is_native_order)
      length = swap_byte_order(length);

   if (length > 1<<20)
   {
      p_ctx->status = VC_CONTAINER_ERROR_FAILED;
      return 0;
   }

   if (size > length)
      size = length;
   ret = fread(buffer, 1, size, module->stream);
   if(ret != size)
   {
      if( feof(module->stream) ) p_ctx->status = VC_CONTAINER_ERROR_EOS;
      else p_ctx->status = VC_CONTAINER_ERROR_FAILED;
   }
   else if (length > size)
   {
      /* Not enough space to read all the packet, so skip to the next one. */
      length -= size;
      vc_container_assert((long)length > 0);
      fseek(module->stream, (long)length, SEEK_CUR);
   }

   return ret;
}

/*****************************************************************************/
static size_t io_pktfile_write(VC_CONTAINER_IO_T *p_ctx, const void *buffer, size_t size)
{
   uint32_t size_word;
   size_t ret;
   
   if (size >= 0xFFFFFFFFUL)
      size_word = 0xFFFFFFFFUL;
   else
      size_word = (uint32_t)size;

   ret = fwrite(&size_word, 1, sizeof(size_word), p_ctx->module->stream);
   if (ret != sizeof(size_word))
   {
      p_ctx->status = VC_CONTAINER_ERROR_FAILED;
      return 0;
   }

   ret = fwrite(buffer, 1, size_word, p_ctx->module->stream);
   if (ret != size_word)
      p_ctx->status = VC_CONTAINER_ERROR_FAILED;
   if (fflush(p_ctx->module->stream) != 0)
      p_ctx->status = VC_CONTAINER_ERROR_FAILED;
   
   return ret;
}

/*****************************************************************************/
static FILE *open_file(VC_CONTAINER_IO_T *ctx, VC_CONTAINER_IO_MODE_T mode,
   VC_CONTAINER_STATUS_T *p_status)
{
   const char *psz_mode = mode == VC_CONTAINER_IO_MODE_WRITE ? "wb+" : "rb";
   FILE *stream = 0;
   const char *port, *path;

   /* Treat empty port or path strings as not defined */
   port = vc_uri_port(ctx->uri_parts);
   if (port && !*port)
      port = NULL;

   path = vc_uri_path(ctx->uri_parts);
   if (path && !*path)
      path = NULL;

   /* Require the port to be undefined and the path to be defined */
   if (port || !path) { *p_status = VC_CONTAINER_ERROR_URI_OPEN_FAILED; goto error; }

   if (!recognise_scheme(vc_uri_scheme(ctx->uri_parts)))
   { *p_status = VC_CONTAINER_ERROR_URI_NOT_FOUND; goto error; }

   stream = fopen(path, psz_mode);
   if(!stream) { *p_status = VC_CONTAINER_ERROR_URI_NOT_FOUND; goto error; }

   *p_status = VC_CONTAINER_SUCCESS;
   return stream;

error:
   return NULL;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T write_byte_order(FILE *stream)
{
   /* Simple byte order header word */
   uint32_t value = NATIVE_BYTE_ORDER;

   if (fwrite(&value, 1, sizeof(value), stream) != sizeof(value))
      return VC_CONTAINER_ERROR_OUT_OF_SPACE;

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T read_byte_order(FILE *stream, bool *is_native)
{
   uint32_t value;

   if (fread(&value, 1, sizeof(value), stream) != sizeof(value))
      return VC_CONTAINER_ERROR_EOS;

   switch (value)
   {
   case NATIVE_BYTE_ORDER: *is_native = true; break;
   case SWAP_BYTE_ORDER:   *is_native = false; break;
   default: return VC_CONTAINER_ERROR_CORRUPTED;
   }

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_io_pktfile_open( VC_CONTAINER_IO_T *p_ctx,
   const char *unused, VC_CONTAINER_IO_MODE_T mode )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_IO_MODULE_T *module = 0;
   FILE *stream = 0;
   bool is_native_order = true;
   VC_CONTAINER_PARAM_UNUSED(unused);

   stream = open_file(p_ctx, mode, &status);
   if (status != VC_CONTAINER_SUCCESS) goto error;

   if (mode == VC_CONTAINER_IO_MODE_WRITE)
      status = write_byte_order(stream);
   else
      status = read_byte_order(stream, &is_native_order);
   if (status != VC_CONTAINER_SUCCESS) goto error;

   module = malloc( sizeof(*module) );
   if(!module) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(module, 0, sizeof(*module));

   p_ctx->module = module;
   module->stream = stream;
   module->is_native_order = is_native_order;
   p_ctx->pf_close = io_pktfile_close;
   p_ctx->pf_read = io_pktfile_read;
   p_ctx->pf_write = io_pktfile_write;

   /* Do not allow caching by I/O core, as this will merge packets in the cache. */
   p_ctx->capabilities = VC_CONTAINER_IO_CAPS_CANT_SEEK;
   return VC_CONTAINER_SUCCESS;

error:
   if(stream) fclose(stream);
   return status;
}
