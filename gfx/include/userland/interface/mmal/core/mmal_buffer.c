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

#include "mmal.h"
#include "mmal_buffer.h"
#include "core/mmal_buffer_private.h"
#include "mmal_logging.h"

#define ROUND_UP(s,align) ((((unsigned long)(s)) & ~((align)-1)) + (align))
#define DEFAULT_COMMAND_SIZE 256 /**< 256 bytes of space for commands */
#define ALIGN  8

/** Acquire a buffer header */
void mmal_buffer_header_acquire(MMAL_BUFFER_HEADER_T *header)
{
#ifdef ENABLE_MMAL_EXTRA_LOGGING
   LOG_TRACE("%p (%i)", header, (int)header->priv->refcount+1);
#endif
   header->priv->refcount++;
}

/** Reset a buffer header */
void mmal_buffer_header_reset(MMAL_BUFFER_HEADER_T *header)
{
   header->length = 0;
   header->offset = 0;
   header->flags = 0;
   header->pts = MMAL_TIME_UNKNOWN;
   header->dts = MMAL_TIME_UNKNOWN;
}

/** Release a buffer header */
void mmal_buffer_header_release(MMAL_BUFFER_HEADER_T *header)
{
#ifdef ENABLE_MMAL_EXTRA_LOGGING
   LOG_TRACE("%p (%i)", header, (int)header->priv->refcount-1);
#endif

   if(--header->priv->refcount != 0)
      return;

   if (header->priv->pf_pre_release)
   {
      if (header->priv->pf_pre_release(header, header->priv->pre_release_userdata))
         return; /* delay releasing the buffer */
   }
   mmal_buffer_header_release_continue(header);
}

/** Finalise buffer release following a pre-release event */
void mmal_buffer_header_release_continue(MMAL_BUFFER_HEADER_T *header)
{
   mmal_buffer_header_reset(header);
   if (header->priv->reference)
      mmal_buffer_header_release(header->priv->reference);
   header->priv->reference = 0;
   header->priv->pf_release(header);
}

/** Replicate a buffer header */
MMAL_STATUS_T mmal_buffer_header_replicate(MMAL_BUFFER_HEADER_T *dest,
   MMAL_BUFFER_HEADER_T *src)
{
#ifdef ENABLE_MMAL_EXTRA_LOGGING
   LOG_TRACE("dest: %p src: %p", dest, src);
#endif

   if (!dest || !src || dest->priv->reference)
      return MMAL_EINVAL;

   mmal_buffer_header_acquire(src);
   dest->priv->reference = src;

   /* Copy all the relevant fields */
   dest->cmd        = src->cmd;
   dest->alloc_size = src->alloc_size;
   dest->data       = src->data;
   dest->offset     = src->offset;
   dest->length     = src->length;
   dest->flags      = src->flags;
   dest->pts        = src->pts;
   dest->dts        = src->dts;
   *dest->type      = *src->type;
   return MMAL_SUCCESS;
}

/** Get the size in bytes of a fully initialised MMAL_BUFFER_HEADER_T */
unsigned int mmal_buffer_header_size(MMAL_BUFFER_HEADER_T *header)
{
   unsigned int header_size;

   header_size = ROUND_UP(sizeof(*header), ALIGN);
   header_size += ROUND_UP(sizeof(*header->type), ALIGN);
   header_size += ROUND_UP(DEFAULT_COMMAND_SIZE, ALIGN);
   header_size += ROUND_UP(sizeof(*header->priv), ALIGN);
   return header_size;
}

/** Initialise a MMAL_BUFFER_HEADER_T */
MMAL_BUFFER_HEADER_T *mmal_buffer_header_initialise(void *mem, unsigned int length)
{
   MMAL_BUFFER_HEADER_T *header;
   unsigned int header_size = mmal_buffer_header_size(0);

   if(length < header_size)
      return 0;

   memset(mem, 0, header_size);

   header = (MMAL_BUFFER_HEADER_T *)mem;
   header->type = (void *)&header[1];
   header->priv = (MMAL_BUFFER_HEADER_PRIVATE_T *)&header->type[1];
   return header;
}

/** Return a pointer to the area reserved for the driver */
MMAL_DRIVER_BUFFER_T *mmal_buffer_header_driver_data(MMAL_BUFFER_HEADER_T *header)
{
   return (MMAL_DRIVER_BUFFER_T *)header->priv->driver_area;
}

/** Return a pointer to a referenced buffer header */
MMAL_BUFFER_HEADER_T *mmal_buffer_header_reference(MMAL_BUFFER_HEADER_T *header)
{
   return header->priv->reference;
}

#ifdef __VIDEOCORE__
# include "vcfw/rtos/common/rtos_common_mem.h"
#endif

/** Lock the data buffer contained in the buffer header */
MMAL_STATUS_T mmal_buffer_header_mem_lock(MMAL_BUFFER_HEADER_T *header)
{
#ifdef __VIDEOCORE__
   uint8_t *data = mem_lock((MEM_HANDLE_T)header->data);
   if (!data)
      return MMAL_EINVAL;
   header->priv->payload_handle = (void *)header->data;
   header->data = data;
#else
   MMAL_PARAM_UNUSED(header);
#endif

   return MMAL_SUCCESS;
}

/** Unlock the data buffer contained in the buffer header */
void mmal_buffer_header_mem_unlock(MMAL_BUFFER_HEADER_T *header)
{
#ifdef __VIDEOCORE__
   mem_unlock((MEM_HANDLE_T)header->priv->payload_handle);
   header->data = header->priv->payload_handle;
#else
   MMAL_PARAM_UNUSED(header);
#endif
}

/** Set a pre-release callback for a buffer header */
void mmal_buffer_header_pre_release_cb_set(MMAL_BUFFER_HEADER_T *header, MMAL_BH_PRE_RELEASE_CB_T cb, void *userdata)
{
   header->priv->pf_pre_release = cb;
   header->priv->pre_release_userdata = userdata;
}
