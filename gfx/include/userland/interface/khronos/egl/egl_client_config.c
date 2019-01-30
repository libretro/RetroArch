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

#include "interface/khronos/common/khrn_int_common.h"
#include "interface/khronos/common/khrn_client_platform.h"
#include "interface/khronos/include/EGL/eglext.h"

#include "interface/khronos/egl/egl_client_config.h"

//#define BGR_FB

typedef uint32_t FEATURES_T;

#define FEATURES_PACK(r, g, b, a, d, s, m, mask, lockable) ((FEATURES_T)((((uint32_t)(r)) << 28 | (g) << 24 | (b) << 20 | (a) << 16 | (d) << 8 | (s) << 4 | (m) << 3 | ((mask) >> 3) << 2) | (lockable) << 1))

typedef struct {
   FEATURES_T features;
   KHRN_IMAGE_FORMAT_T color, depth, multisample, mask;
} FEATURES_AND_FORMATS_T;

/*
   formats

   For 0 <= id < EGL_MAX_CONFIGS:
      formats[id].features is valid
*/

static FEATURES_AND_FORMATS_T formats[EGL_MAX_CONFIGS] = {
//                                        LOCKABLE
//                                    MASK |
//                R  G  B  A   D  S  M  |  |   COLOR      DEPTH                 MULTISAMPLE           MASK
   {FEATURES_PACK(8, 8, 8, 8, 24, 8, 0, 0, 0), ABGR_8888, DEPTH_32_TLBD,        IMAGE_FORMAT_INVALID, IMAGE_FORMAT_INVALID},
   {FEATURES_PACK(8, 8, 8, 0, 24, 8, 0, 0, 0), XBGR_8888, DEPTH_32_TLBD,        IMAGE_FORMAT_INVALID, IMAGE_FORMAT_INVALID},
   {FEATURES_PACK(8, 8, 8, 8, 24, 0, 0, 0, 0), ABGR_8888, DEPTH_32_TLBD,        IMAGE_FORMAT_INVALID, IMAGE_FORMAT_INVALID},
   {FEATURES_PACK(8, 8, 8, 0, 24, 0, 0, 0, 0), XBGR_8888, DEPTH_32_TLBD,        IMAGE_FORMAT_INVALID, IMAGE_FORMAT_INVALID},
   {FEATURES_PACK(8, 8, 8, 8,  0, 8, 0, 0, 0), ABGR_8888, DEPTH_32_TLBD,        IMAGE_FORMAT_INVALID, IMAGE_FORMAT_INVALID},
   {FEATURES_PACK(8, 8, 8, 0,  0, 8, 0, 0, 0), XBGR_8888, DEPTH_32_TLBD,        IMAGE_FORMAT_INVALID, IMAGE_FORMAT_INVALID},
   {FEATURES_PACK(8, 8, 8, 8,  0, 0, 0, 0, 0), ABGR_8888, IMAGE_FORMAT_INVALID, IMAGE_FORMAT_INVALID, IMAGE_FORMAT_INVALID},
   {FEATURES_PACK(8, 8, 8, 0,  0, 0, 0, 0, 0), XBGR_8888, IMAGE_FORMAT_INVALID, IMAGE_FORMAT_INVALID, IMAGE_FORMAT_INVALID},

   {FEATURES_PACK(8, 8, 8, 8, 24, 8, 1, 0, 0), ABGR_8888, DEPTH_32_TLBD /*?*/,  DEPTH_COL_64_TLBD,    IMAGE_FORMAT_INVALID},
   {FEATURES_PACK(8, 8, 8, 0, 24, 8, 1, 0, 0), XBGR_8888, DEPTH_32_TLBD /*?*/,  DEPTH_COL_64_TLBD,    IMAGE_FORMAT_INVALID},
   {FEATURES_PACK(8, 8, 8, 8, 24, 0, 1, 0, 0), ABGR_8888, DEPTH_32_TLBD /*?*/,  DEPTH_COL_64_TLBD,    IMAGE_FORMAT_INVALID},
   {FEATURES_PACK(8, 8, 8, 0, 24, 0, 1, 0, 0), XBGR_8888, DEPTH_32_TLBD /*?*/,  DEPTH_COL_64_TLBD,    IMAGE_FORMAT_INVALID},
   {FEATURES_PACK(8, 8, 8, 8,  0, 8, 1, 0, 0), ABGR_8888, DEPTH_32_TLBD /*?*/,  DEPTH_COL_64_TLBD,    IMAGE_FORMAT_INVALID},
   {FEATURES_PACK(8, 8, 8, 0,  0, 8, 1, 0, 0), XBGR_8888, DEPTH_32_TLBD /*?*/,  DEPTH_COL_64_TLBD,    IMAGE_FORMAT_INVALID},
   {FEATURES_PACK(8, 8, 8, 8,  0, 0, 1, 0, 0), ABGR_8888, IMAGE_FORMAT_INVALID, COL_32_TLBD,          IMAGE_FORMAT_INVALID},
   {FEATURES_PACK(8, 8, 8, 0,  0, 0, 1, 0, 0), XBGR_8888, IMAGE_FORMAT_INVALID, COL_32_TLBD,          IMAGE_FORMAT_INVALID},

   {FEATURES_PACK(5, 6, 5, 0, 24, 8, 0, 0, 0), RGB_565,   DEPTH_32_TLBD,        IMAGE_FORMAT_INVALID, IMAGE_FORMAT_INVALID},
   {FEATURES_PACK(5, 6, 5, 0, 24, 0, 0, 0, 0), RGB_565,   DEPTH_32_TLBD,        IMAGE_FORMAT_INVALID, IMAGE_FORMAT_INVALID},
   {FEATURES_PACK(5, 6, 5, 0,  0, 8, 0, 0, 0), RGB_565,   DEPTH_32_TLBD,        IMAGE_FORMAT_INVALID, IMAGE_FORMAT_INVALID},
   {FEATURES_PACK(5, 6, 5, 0,  0, 0, 0, 0, 0), RGB_565,   IMAGE_FORMAT_INVALID, IMAGE_FORMAT_INVALID, IMAGE_FORMAT_INVALID},

   {FEATURES_PACK(5, 6, 5, 0, 24, 8, 1, 0, 0), RGB_565,   DEPTH_32_TLBD /*?*/,  DEPTH_COL_64_TLBD,    IMAGE_FORMAT_INVALID},
   {FEATURES_PACK(5, 6, 5, 0, 24, 0, 1, 0, 0), RGB_565,   DEPTH_32_TLBD /*?*/,  DEPTH_COL_64_TLBD,    IMAGE_FORMAT_INVALID},
   {FEATURES_PACK(5, 6, 5, 0,  0, 8, 1, 0, 0), RGB_565,   DEPTH_32_TLBD /*?*/,  DEPTH_COL_64_TLBD,    IMAGE_FORMAT_INVALID},
   {FEATURES_PACK(5, 6, 5, 0,  0, 0, 1, 0, 0), RGB_565,   IMAGE_FORMAT_INVALID, COL_32_TLBD,          IMAGE_FORMAT_INVALID},

#ifndef EGL_NO_ALPHA_MASK_CONFIGS
   {FEATURES_PACK(8, 8, 8, 8,  0, 0, 0, 8, 0), ABGR_8888, IMAGE_FORMAT_INVALID, IMAGE_FORMAT_INVALID, A_8_RSO},
   {FEATURES_PACK(8, 8, 8, 0,  0, 0, 0, 8, 0), XBGR_8888, IMAGE_FORMAT_INVALID, IMAGE_FORMAT_INVALID, A_8_RSO},
   {FEATURES_PACK(5, 6, 5, 0,  0, 0, 0, 8, 0), RGB_565,   IMAGE_FORMAT_INVALID, IMAGE_FORMAT_INVALID, A_8_RSO},
#endif

   {FEATURES_PACK(5, 6, 5, 0, 16, 0, 0, 0, 0), RGB_565,   DEPTH_32_TLBD,        IMAGE_FORMAT_INVALID, IMAGE_FORMAT_INVALID},

};

void egl_config_install_configs(int type)
{
   uint32_t i;
   for (i = 0; i != ARR_COUNT(formats); ++i) {
      formats[i].color = (type == 0) ?
         khrn_image_to_rso_format(formats[i].color) :
         khrn_image_to_tf_format(formats[i].color);
   }
}

static bool bindable_rgb(FEATURES_T features);
static bool bindable_rgba(FEATURES_T features);

#include "interface/khronos/egl/egl_client_config_cr.c"

/*
   KHRN_IMAGE_FORMAT_T egl_config_get_color_format(int id)

   Implementation notes:

   We may return an image format which cannot be rendered to.

   Preconditions:

   0 <= id < EGL_MAX_CONFIGS

   Postconditions:

   Return value is a hardware framebuffer-supported uncompressed color KHRN_IMAGE_FORMAT_T
*/

KHRN_IMAGE_FORMAT_T egl_config_get_color_format(int id)
{
   vcos_assert(id >= 0 && id < EGL_MAX_CONFIGS);

   return formats[id].color;
}

/*
   KHRN_IMAGE_FORMAT_T egl_config_get_depth_format(int id)

   Preconditions:

   0 <= id < EGL_MAX_CONFIGS

   Postconditions:

   Return value is a hardware framebuffer-supported depth KHRN_IMAGE_FORMAT_T or IMAGE_FORMAT_INVALID
*/

KHRN_IMAGE_FORMAT_T egl_config_get_depth_format(int id)
{
   vcos_assert(id >= 0 && id < EGL_MAX_CONFIGS);

   return formats[id].depth;
}

/*
   KHRN_IMAGE_FORMAT_T egl_config_get_mask_format(int id)

   Preconditions:

   0 <= id < EGL_MAX_CONFIGS

   Postconditions:

   Return value is a hardware framebuffer-supported mask KHRN_IMAGE_FORMAT_T or IMAGE_FORMAT_INVALID
*/

KHRN_IMAGE_FORMAT_T egl_config_get_mask_format(int id)
{
   vcos_assert(id >= 0 && id < EGL_MAX_CONFIGS);

   return formats[id].mask;
}

/*
   KHRN_IMAGE_FORMAT_T egl_config_get_multisample_format(int id)

   Preconditions:

   0 <= id < EGL_MAX_CONFIGS

   Postconditions:

   Return value is a hardware framebuffer-supported multisample color format or IMAGE_FORMAT_INVALID
*/

KHRN_IMAGE_FORMAT_T egl_config_get_multisample_format(int id)
{
   vcos_assert(id >= 0 && id < EGL_MAX_CONFIGS);

   return formats[id].multisample;
}

/*
   bool egl_config_get_multisample(int id)

   Preconditions:

   0 <= id < EGL_MAX_CONFIGS

   Postconditions:

   -
*/

bool egl_config_get_multisample(int id)
{
   vcos_assert(id >= 0 && id < EGL_MAX_CONFIGS);

   return FEATURES_UNPACK_MULTI(formats[id].features);
}

/*
   bool bindable_rgb(FEATURES_T features)
   bool bindable_rgba(FEATURES_T features)

   Preconditions:

   features is a valid FEATURES_T

   Postconditions:

   -
*/

static bool bindable_rgb(FEATURES_T features)
{
   return !FEATURES_UNPACK_MULTI(features) && !FEATURES_UNPACK_ALPHA(features);
}

static bool bindable_rgba(FEATURES_T features)
{
   return !FEATURES_UNPACK_MULTI(features);
}

bool egl_config_bindable(int id, EGLenum format)
{
   vcos_assert(id >= 0 && id < EGL_MAX_CONFIGS);
   switch (format) {
   case EGL_NO_TEXTURE:
      return true;
   case EGL_TEXTURE_RGB:
      return bindable_rgb(formats[id].features);
   case EGL_TEXTURE_RGBA:
      return bindable_rgba(formats[id].features);
   default:
      UNREACHABLE();
      return false;
   }
}

/*
   bool egl_config_match_pixmap_info(int id, KHRN_IMAGE_WRAP_T *image)

   TODO: decide how tolerant we should be when matching to native pixmaps.
   At present they match if the red, green, blue and alpha channels
   all have the same bit depths.

   Preconditions:

   0 <= id < EGL_MAX_CONFIGS
   image is a pointer to a valid KHRN_IMAGE_WRAP_T, possibly with null data pointer
   match is a bitmask which is a subset of PLATFORM_PIXMAP_MATCH_NONRENDERABLE|PLATFORM_PIXMAP_MATCH_RENDERABLE

   Postconditions:

   -
*/

bool egl_config_match_pixmap_info(int id, KHRN_IMAGE_WRAP_T *image)
{
   FEATURES_T features = formats[id].features;
   KHRN_IMAGE_FORMAT_T format = image->format;

   vcos_assert(id >= 0 && id < EGL_MAX_CONFIGS);

   return
      khrn_image_get_red_size(format)   == FEATURES_UNPACK_RED(features) &&
      khrn_image_get_green_size(format) == FEATURES_UNPACK_GREEN(features) &&
      khrn_image_get_blue_size(format)  == FEATURES_UNPACK_BLUE(features) &&
      khrn_image_get_alpha_size(format) == FEATURES_UNPACK_ALPHA(features);
}

/*
   uint32_t egl_config_get_api_support(int id)

   Preconditions:

   0 <= id < EGL_MAX_CONFIGS

   Postconditions:

   Result is a bitmap which is a subset of (EGL_OPENGL_ES_BIT | EGL_OPENVG_BIT | EGL_OPENGL_ES2_BIT)
*/

uint32_t egl_config_get_api_support(int id)
{
   /* no configs are api-specific (ie if you can use a config with gl, you can
    * use it with vg too, and vice-versa). however, some configs have color
    * buffer formats that are incompatible with the hardware, and so can't be
    * used with any api. such configs may still be useful eg with the surface
    * locking extension... */

#if EGL_KHR_lock_surface
   /* to reduce confusion, just say no for all lockable configs. this #if can be
    * safely commented out -- the color buffer format check below will catch
    * lockable configs we actually can't use */
   if (egl_config_is_lockable(id)) {
      return 0;
   }
#endif

   switch (egl_config_get_color_format(id)) {
   case ABGR_8888_RSO: case ABGR_8888_TF: case ABGR_8888_LT:
   case XBGR_8888_RSO: case XBGR_8888_TF: case XBGR_8888_LT:
   case ARGB_8888_RSO: case ARGB_8888_TF: case ARGB_8888_LT:
   case XRGB_8888_RSO: case XRGB_8888_TF: case XRGB_8888_LT:
   case RGB_565_RSO: case RGB_565_TF: case RGB_565_LT:
#ifndef NO_OPENVG
      return (uint32_t)(EGL_OPENGL_ES_BIT | EGL_OPENVG_BIT | EGL_OPENGL_ES2_BIT);
#else
      return (uint32_t)(EGL_OPENGL_ES_BIT | EGL_OPENGL_ES2_BIT);
#endif
   default:
      break;
   }
   return 0;
}

/*
   uint32_t egl_config_get_api_conformance(int id)

   Preconditions:

   0 <= id < EGL_MAX_CONFIGS

   Postconditions:

   Result is a bitmap which is a subset of (EGL_OPENGL_ES_BIT | EGL_OPENVG_BIT | EGL_OPENGL_ES2_BIT)
*/

uint32_t egl_config_get_api_conformance(int id)
{
   /* vg doesn't support multisampled surfaces properly */
   return egl_config_get_api_support(id) & ~(FEATURES_UNPACK_MULTI(formats[id].features) ? EGL_OPENVG_BIT : 0);
}

bool egl_config_bpps_match(int id0, int id1) /* bpps of all buffers match */
{
   FEATURES_T config0 = formats[id0].features;
   FEATURES_T config1 = formats[id1].features;

   return
      FEATURES_UNPACK_RED(config0)     == FEATURES_UNPACK_RED(config1) &&
      FEATURES_UNPACK_GREEN(config0)   == FEATURES_UNPACK_GREEN(config1) &&
      FEATURES_UNPACK_BLUE(config0)    == FEATURES_UNPACK_BLUE(config1) &&
      FEATURES_UNPACK_ALPHA(config0)   == FEATURES_UNPACK_ALPHA(config1) &&
      FEATURES_UNPACK_DEPTH(config0)   == FEATURES_UNPACK_DEPTH(config1) &&
      FEATURES_UNPACK_STENCIL(config0) == FEATURES_UNPACK_STENCIL(config1) &&
      FEATURES_UNPACK_MASK(config0)    == FEATURES_UNPACK_MASK(config1);
}

#if EGL_KHR_lock_surface

/*
   KHRN_IMAGE_FORMAT_T egl_config_get_mapped_format(int id)

   Returns the format of the mapped buffer when an EGL surface is locked.

   Preconditions:

   0 <= id < EGL_MAX_CONFIGS
   egl_config_is_lockable(id)

   Postconditions:

   Return value is RGB_565_RSO or ARGB_8888_RSO
*/

KHRN_IMAGE_FORMAT_T egl_config_get_mapped_format(int id)
{
   KHRN_IMAGE_FORMAT_T result;

   vcos_assert(id >= 0 && id < EGL_MAX_CONFIGS);
   vcos_assert(FEATURES_UNPACK_LOCKABLE(formats[id].features));

   /* If any t-format images were lockable, we would convert to raster format here */
   result = egl_config_get_color_format(id);
   vcos_assert(khrn_image_is_rso(result));
   return result;
}

/*
   bool egl_config_is_lockable(int id)

   Preconditions:

   0 <= id < EGL_MAX_CONFIGS

   Postconditions:

   -
*/

bool egl_config_is_lockable(int id)
{
   vcos_assert(id >= 0 && id < EGL_MAX_CONFIGS);
   return FEATURES_UNPACK_LOCKABLE(formats[id].features);
}

#endif




