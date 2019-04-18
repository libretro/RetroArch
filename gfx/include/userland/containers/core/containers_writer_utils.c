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

#include "containers/containers.h"
#include "containers/core/containers_private.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_writer_utils.h"
#include "vcos.h"

#include <stdio.h>

/*****************************************************************************/
static VC_CONTAINER_STATUS_T vc_container_writer_extraio_create(VC_CONTAINER_T *context, const char *uri,
   VC_CONTAINER_WRITER_EXTRAIO_T *extraio)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_PARAM_UNUSED(context);

   extraio->io = vc_container_io_open( uri, VC_CONTAINER_IO_MODE_WRITE, &status );
   extraio->refcount = 0;
   extraio->temp = 0;
   return status;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_writer_extraio_create_null(VC_CONTAINER_T *context, VC_CONTAINER_WRITER_EXTRAIO_T *extraio)
{
   return vc_container_writer_extraio_create(context, "null://", extraio);
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_writer_extraio_create_temp(VC_CONTAINER_T *context, VC_CONTAINER_WRITER_EXTRAIO_T *extraio)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   unsigned int length = strlen(context->priv->io->uri) + 5;
   char *uri = malloc(length);
   if(!uri) return VC_CONTAINER_ERROR_OUT_OF_MEMORY;

   snprintf(uri, length, "%s.tmp", context->priv->io->uri);
   status = vc_container_writer_extraio_create(context, uri, extraio);
   free(uri);
   extraio->temp = true;

   if(status == VC_CONTAINER_SUCCESS && !context->priv->tmp_io)
      context->priv->tmp_io = extraio->io;

   return status;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_writer_extraio_delete(VC_CONTAINER_T *context, VC_CONTAINER_WRITER_EXTRAIO_T *extraio)
{
   VC_CONTAINER_STATUS_T status;
   char *uri = extraio->temp ? vcos_strdup(extraio->io->uri) : 0;

   while(extraio->refcount) vc_container_writer_extraio_disable(context, extraio);
   status = vc_container_io_close( extraio->io );

   /* coverity[check_return] On failure the worst case is a file or directory is not removed */
   if(uri) remove(uri);
   if(uri) free(uri);

   if(context->priv->tmp_io == extraio->io)
      context->priv->tmp_io = 0;

   return status;
}

/*****************************************************************************/
int64_t vc_container_writer_extraio_enable(VC_CONTAINER_T *context, VC_CONTAINER_WRITER_EXTRAIO_T *extraio)
{
   VC_CONTAINER_IO_T *tmp;

   if(!extraio->refcount)
   {
      vc_container_io_seek(extraio->io, INT64_C(0));
      tmp = context->priv->io;
      context->priv->io = extraio->io;
      extraio->io = tmp;
   }
   return extraio->refcount++;
}

/*****************************************************************************/
int64_t vc_container_writer_extraio_disable(VC_CONTAINER_T *context, VC_CONTAINER_WRITER_EXTRAIO_T *extraio)
{
   VC_CONTAINER_IO_T *tmp;

   if(extraio->refcount)
   {
      extraio->refcount--;
      if(!extraio->refcount)
      {
         tmp = context->priv->io;
         context->priv->io = extraio->io;
         extraio->io = tmp;
      }
   }
   return extraio->refcount;
}
