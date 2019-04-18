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

#ifndef VC_IMAGE_METADATA_H
#define VC_IMAGE_METADATA_H

#include "vcfw/rtos/rtos.h"
#include "helpers/vc_image/metadata_fourcc.h"

#define VC_METADATA_ALIGN 3

/******************************************************************************
Types of metadata.
******************************************************************************/
typedef unsigned int VC_METADATA_TYPE_T;

/******************************************************************************
Metadata and item headers.
******************************************************************************/
typedef struct vc_metadata_item_s {
   VC_METADATA_TYPE_T type;
   int len;
} VC_METADATA_ITEM_T;

typedef struct vc_metadata_header_s {
   int size;
#ifdef VCMODS_LCC
   unsigned char readonly;
#else
   unsigned char readonly:1;
#endif
   int offset_next;
   RTOS_LATCH_T latch;
} VC_METADATA_HEADER_T;

/******************************************************************************
Public declarations.
******************************************************************************/
// Initialises a metadata header structure
int vc_metadata_initialise(VC_METADATA_HEADER_T *header, int size);
// Add a metadata entry to the buffer
int vc_metadata_add(VC_METADATA_HEADER_T *header, void *data, VC_METADATA_TYPE_T type, int len);
// Get a (pointer to a) particular metadata item from the buffer (optionally copy the data to dest), and return the length of the item
void *vc_metadata_get_with_length(void *dest, VC_METADATA_HEADER_T *header, VC_METADATA_TYPE_T type, int *retcode, int *len);
// Get a (pointer to a) particular metadata item from the buffer (optionally copy the data to dest)
void *vc_metadata_get(void *dest, VC_METADATA_HEADER_T *header, VC_METADATA_TYPE_T type, int *retcode);
// Clear the metadata buffer and reset the header (clears readonly flag)
int vc_metadata_clear(VC_METADATA_HEADER_T *header);
// Set or clear the read-only flag in the metadata buffer
int vc_metadata_set_readonly(VC_METADATA_HEADER_T *header, int value);
// Copies the contents of one metadata buffer to the other (appends the destination buffer)
int vc_metadata_copy(VC_METADATA_HEADER_T *dest, VC_METADATA_HEADER_T *src);
// Overwrites an item in the metadata buffer (caller must make sure the sizes match!)
int vc_metadata_overwrite(VC_METADATA_HEADER_T *header, void *data, VC_METADATA_TYPE_T type);
// Flush the metadata buffer out of the cache
int vc_metadata_cache_flush(VC_METADATA_HEADER_T *header);
// Locks the vc_metadata library semaphore
int vc_metadata_lock(VC_METADATA_HEADER_T *header);
// Unlocks the vc_metadata library semaphore
int vc_metadata_unlock(VC_METADATA_HEADER_T *header);

/******************************************************************************
Wrappers for the above functions using VC_IMAGEs.
******************************************************************************/
#define vc_image_metadata_initialise(i, s)         vc_metadata_initialise((i)->metadata, s)
#define vc_image_metadata_add(i, d, t, l)          vc_metadata_add((i)->metadata, d, t, l)
#define vc_image_metadata_get(d, i, t, r)          vc_metadata_get(d, (i)->metadata, t, r)
#define vc_image_metadata_clear(i)                 vc_metadata_clear((i)->metadata)
#define vc_image_metadata_set_readonly(i, v)       vc_metadata_set_readonly((i)->metadata, v)
#define vc_image_metadata_copy(d, s)               vc_metadata_copy((d)->metadata, (s)->metadata)
#define vc_image_metadata_overwrite(i, d, t)       vc_metadata_overwrite((i)->metadata, d, t)
#define vc_image_metadata_cache_flush(i)           vc_metadata_cache_flush((i)->metadata)
#endif
