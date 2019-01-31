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

#ifndef VG_IMAGE_H
#define VG_IMAGE_H

#include "middleware/khronos/common/khrn_image.h"
#include "middleware/khronos/common/khrn_mem.h"
#include "interface/khronos/include/VG/openvg.h"

/******************************************************************************
image
******************************************************************************/

typedef struct {
   VGImageFormat format;
   uint32_t allowed_quality;
   int32_t width;
   int32_t height;
} VG_IMAGE_BPRINT_T;

extern void vg_image_bprint_from_stem(
   MEM_HANDLE_T handle,
   VGImageFormat format,
   uint32_t allowed_quality,
   int32_t width, int32_t height);

extern void vg_image_bprint_term(void *, uint32_t);

static INLINE bool vg_is_image_bprint(MEM_HANDLE_T handle)
{
   return mem_get_term(handle) == vg_image_bprint_term;
}

typedef struct {
   uint32_t allowed_quality;

   MEM_HANDLE_T image; /* KHRN_IMAGE_T */
   uint32_t image_width;
   uint32_t image_height;
   KHRN_IMAGE_FORMAT_T image_format;
} VG_IMAGE_T;

extern bool vg_image_from_bprint(MEM_HANDLE_T handle);
extern VGImageFormat vg_image_get_external_format(KHRN_IMAGE_FORMAT_T format); /* (VGImageFormat)-1 if internal format isn't permitted */
extern MEM_HANDLE_T vg_image_alloc_from_image(MEM_HANDLE_T src_handle);
extern void vg_image_from_stem_and_image(MEM_HANDLE_T handle, MEM_HANDLE_T src_handle);

extern void vg_image_term(void *, uint32_t);

static INLINE bool vg_is_image(MEM_HANDLE_T handle)
{
   return mem_get_term(handle) == vg_image_term;
}

extern VG_IMAGE_T *vg_image_lock_adam(
   uint32_t *x, uint32_t *y, uint32_t *width, uint32_t *height,
   MEM_HANDLE_T *handle);

static INLINE bool vg_image_share_storage(VG_IMAGE_T *image, VG_IMAGE_T *other)
{
   return image->image == other->image;
}

extern bool vg_image_leak(VG_IMAGE_T *image);

/******************************************************************************
child image
******************************************************************************/

/*
   the same structure is used for both blueprints and instantiated child images

   in the blueprint case, parent is either a VG_IMAGE_T, a VG_IMAGE_BPRINT_T, or
   a VG_CHILD_IMAGE_T (of either type)

   in the instantiated case, parent is either a VG_IMAGE_T or an
   instantiated-type VG_CHILD_IMAGE_T
*/

typedef struct {
   MEM_HANDLE_T parent;
   uint16_t x, y, width, height;
} VG_CHILD_IMAGE_T;

extern void vg_child_image_bprint_from_stem(
   MEM_HANDLE_T handle,
   MEM_HANDLE_T parent_handle,
   int32_t x, int32_t y,
   int32_t width, int32_t height);

extern void vg_child_image_bprint_term(void *, uint32_t);

static INLINE bool vg_is_child_image_bprint(MEM_HANDLE_T handle)
{
   return mem_get_term(handle) == vg_child_image_bprint_term;
}

extern bool vg_child_image_from_bprint(MEM_HANDLE_T handle);

extern void vg_child_image_term(void *, uint32_t);

static INLINE bool vg_is_child_image(MEM_HANDLE_T handle)
{
   return mem_get_term(handle) == vg_child_image_term;
}

#endif
