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

#ifndef KHRN_INTERLOCK_FILLER_4_H
#define KHRN_INTERLOCK_FILLER_4_H

#include "interface/khronos/common/khrn_int_util.h"

/* users are render states. user = 1 << render state index */
typedef enum {
   KHRN_INTERLOCK_USER_NONE    = 0,
   KHRN_INTERLOCK_USER_INVALID = 1 << 29, /* <= 29 render states, so can use top 3 bits for temp/writing/inv */
   KHRN_INTERLOCK_USER_TEMP    = 1 << 30, 
   KHRN_INTERLOCK_USER_WRITING = 1 << 31
} KHRN_INTERLOCK_USER_T;
static INLINE KHRN_INTERLOCK_USER_T khrn_interlock_user(uint32_t i) { return (KHRN_INTERLOCK_USER_T)(1 << i); }
static INLINE uint32_t khrn_interlock_render_state_i(KHRN_INTERLOCK_USER_T user) { return _msb(user); }

typedef struct {
   /* top bits: who? */
   uint64_t hw_read_pos;
   uint64_t worker_read_pos;
   uint64_t write_pos;
} KHRN_INTERLOCK_EXTRA_T;

#endif
