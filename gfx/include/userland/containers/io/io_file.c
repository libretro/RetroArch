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

typedef struct VC_CONTAINER_IO_MODULE_T
{
   FILE *stream;

} VC_CONTAINER_IO_MODULE_T;

VC_CONTAINER_STATUS_T vc_container_io_file_open( VC_CONTAINER_IO_T *, const char *,
   VC_CONTAINER_IO_MODE_T );

/*****************************************************************************/
static VC_CONTAINER_STATUS_T io_file_close( VC_CONTAINER_IO_T *p_ctx )
{
   VC_CONTAINER_IO_MODULE_T *module = p_ctx->module;
   fclose(module->stream);
   free(module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static size_t io_file_read(VC_CONTAINER_IO_T *p_ctx, void *buffer, size_t size)
{
   size_t ret = fread(buffer, 1, size, p_ctx->module->stream);
   if(ret != size)
   {
      /* Sanity check return value. Some platforms (e.g. Android) can return -1 */
      if( ((int)ret) < 0 ) ret = 0;

      if( feof(p_ctx->module->stream) ) p_ctx->status = VC_CONTAINER_ERROR_EOS;
      else p_ctx->status = VC_CONTAINER_ERROR_FAILED;
   }
   return ret;
}

/*****************************************************************************/
static size_t io_file_write(VC_CONTAINER_IO_T *p_ctx, const void *buffer, size_t size)
{
   return fwrite(buffer, 1, size, p_ctx->module->stream);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T io_file_seek(VC_CONTAINER_IO_T *p_ctx, int64_t offset)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   int ret;

   //FIXME: large file support
#ifdef _VIDEOCORE
   extern int fseek64(FILE *fp, int64_t offset, int whence);
   ret = fseek64(p_ctx->module->stream, offset, SEEK_SET);
#else
   if (offset > (int64_t)UINT_MAX)
   {
      p_ctx->status = VC_CONTAINER_ERROR_EOS;
      return VC_CONTAINER_ERROR_EOS;
   }
   ret = fseek(p_ctx->module->stream, (long)offset, SEEK_SET);
#endif
   if(ret)
   {
      if( feof(p_ctx->module->stream) ) status = VC_CONTAINER_ERROR_EOS;
      else status = VC_CONTAINER_ERROR_FAILED;
   }

   p_ctx->status = status;
   return status;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_io_file_open( VC_CONTAINER_IO_T *p_ctx,
   const char *unused, VC_CONTAINER_IO_MODE_T mode )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_IO_MODULE_T *module = 0;
   const char *psz_mode = mode == VC_CONTAINER_IO_MODE_WRITE ? "wb+" : "rb";
   const char *uri = p_ctx->uri;
   FILE *stream = 0;
   VC_CONTAINER_PARAM_UNUSED(unused);

   if(vc_uri_path(p_ctx->uri_parts))
      uri = vc_uri_path(p_ctx->uri_parts);

   stream = fopen(uri, psz_mode);
   if(!stream) { status = VC_CONTAINER_ERROR_URI_NOT_FOUND; goto error; }

   /* Turn off buffering. The container layer will provide its own cache */
   setvbuf(stream, NULL, _IONBF, 0);

   module = malloc( sizeof(*module) );
   if(!module) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(module, 0, sizeof(*module));

   p_ctx->module = module;
   module->stream = stream;
   p_ctx->pf_close = io_file_close;
   p_ctx->pf_read = io_file_read;
   p_ctx->pf_write = io_file_write;
   p_ctx->pf_seek = io_file_seek;

   if(mode == VC_CONTAINER_IO_MODE_WRITE)
   {
      p_ctx->max_size = (1UL<<31)-1; /* For now limit to 2GB */
   }
   else
   {
      //FIXME: large file support, platform-specific file size
      fseek(p_ctx->module->stream, 0, SEEK_END);
      p_ctx->size = ftell(p_ctx->module->stream);
      fseek(p_ctx->module->stream, 0, SEEK_SET);
   }

   p_ctx->capabilities = VC_CONTAINER_IO_CAPS_NO_CACHING;
   return VC_CONTAINER_SUCCESS;

 error:
   if(stream) fclose(stream);
   return status;
}
