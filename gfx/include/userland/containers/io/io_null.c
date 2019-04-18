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
#include "containers/core/containers_common.h"
#include "containers/core/containers_io.h"
#include "containers/core/containers_uri.h"

VC_CONTAINER_STATUS_T vc_container_io_null_open( VC_CONTAINER_IO_T *, const char *,
   VC_CONTAINER_IO_MODE_T );

/*****************************************************************************/
static VC_CONTAINER_STATUS_T io_null_close( VC_CONTAINER_IO_T *p_ctx )
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static size_t io_null_read(VC_CONTAINER_IO_T *p_ctx, void *buffer, size_t size)
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   VC_CONTAINER_PARAM_UNUSED(buffer);
   VC_CONTAINER_PARAM_UNUSED(size);
   return size;
}

/*****************************************************************************/
static size_t io_null_write(VC_CONTAINER_IO_T *p_ctx, const void *buffer, size_t size)
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   VC_CONTAINER_PARAM_UNUSED(buffer);
   VC_CONTAINER_PARAM_UNUSED(size);
   return size;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T io_null_seek(VC_CONTAINER_IO_T *p_ctx, int64_t offset)
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   VC_CONTAINER_PARAM_UNUSED(offset);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_io_null_open( VC_CONTAINER_IO_T *p_ctx,
   const char *unused, VC_CONTAINER_IO_MODE_T mode )
{
   VC_CONTAINER_PARAM_UNUSED(unused);
   VC_CONTAINER_PARAM_UNUSED(mode);

   /* Check the URI */
   if (!vc_uri_scheme(p_ctx->uri_parts) ||
       (strcasecmp(vc_uri_scheme(p_ctx->uri_parts), "null") &&
        strcasecmp(vc_uri_scheme(p_ctx->uri_parts), "null")))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   p_ctx->pf_close = io_null_close;
   p_ctx->pf_read = io_null_read;
   p_ctx->pf_write = io_null_write;
   p_ctx->pf_seek = io_null_seek;
   return VC_CONTAINER_SUCCESS;
}
