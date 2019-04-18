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
#include "interface/khronos/include/EGL/egl.h"
#include "interface/khronos/include/EGL/eglext.h"

#if EGL_BRCM_global_image && EGL_KHR_image

static INLINE void acquire_value(uint64_t value)
{
   platform_acquire_global_image((uint32_t)value, (uint32_t)(value >> 32));
}

static INLINE void release_value(uint64_t value)
{
   platform_release_global_image((uint32_t)value, (uint32_t)(value >> 32));
}

#define KHRN_GENERIC_MAP_VALUE_NONE ((uint64_t)0)
#define KHRN_GENERIC_MAP_VALUE_DELETED ((uint64_t)-1)
#define KHRN_GENERIC_MAP_ACQUIRE_VALUE acquire_value
#define KHRN_GENERIC_MAP_RELEASE_VALUE release_value
#define KHRN_GENERIC_MAP_ALLOC khrn_platform_malloc
#define KHRN_GENERIC_MAP_FREE khrn_platform_free

#define CLIENT_GLOBAL_IMAGE_MAP_C
#include "interface/khronos/common/khrn_client_global_image_map.h"

#endif
