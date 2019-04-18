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

#ifndef KHRN_INTERLOCK_H
#define KHRN_INTERLOCK_H

#include "middleware/khronos/common/khrn_mem.h"
#include "middleware/khronos/egl/egl_disp.h"

/* should define KHRN_INTERLOCK_USER_T, KHRN_INTERLOCK_USER_NONE,
 * KHRN_INTERLOCK_USER_TEMP, KHRN_INTERLOCK_USER_WRITING, and
 * KHRN_INTERLOCK_EXTRA_T */
#include "middleware/khronos/common/2708/khrn_interlock_filler_4.h"

typedef struct {
   EGL_DISP_IMAGE_HANDLE_T disp_image_handle;
   KHRN_INTERLOCK_USER_T users;
   KHRN_INTERLOCK_EXTRA_T extra;
} KHRN_INTERLOCK_T;

/*
   platform-independent implementations
*/

extern void khrn_interlock_init(KHRN_INTERLOCK_T *interlock);
extern void khrn_interlock_term(KHRN_INTERLOCK_T *interlock);

extern bool khrn_interlock_read(KHRN_INTERLOCK_T *interlock, KHRN_INTERLOCK_USER_T user); /* user allowed to be KHRN_INTERLOCK_USER_NONE */
extern bool khrn_interlock_write(KHRN_INTERLOCK_T *interlock, KHRN_INTERLOCK_USER_T user); /* user allowed to be KHRN_INTERLOCK_USER_NONE */
extern KHRN_INTERLOCK_USER_T khrn_interlock_get_writer(KHRN_INTERLOCK_T *interlock);
extern bool khrn_interlock_release(KHRN_INTERLOCK_T *interlock, KHRN_INTERLOCK_USER_T user);

extern bool khrn_interlock_write_would_block(KHRN_INTERLOCK_T *interlock);

extern void khrn_interlock_invalidate(KHRN_INTERLOCK_T *interlock);
extern bool khrn_interlock_is_invalid(KHRN_INTERLOCK_T *interlock);

/*
   platform-dependent implementations
*/

extern void khrn_interlock_extra_init(KHRN_INTERLOCK_T *interlock);
extern void khrn_interlock_extra_term(KHRN_INTERLOCK_T *interlock);

extern void khrn_interlock_read_immediate(KHRN_INTERLOCK_T *interlock);
extern void khrn_interlock_write_immediate(KHRN_INTERLOCK_T *interlock);

extern void khrn_interlock_flush(KHRN_INTERLOCK_USER_T user);

#endif
