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
#ifndef KHRN_PID_MAP_VALUE_H
#define KHRN_PID_MAP_VALUE_H

typedef struct KHRN_PID_MAP_VALUE_S {
   MEM_HANDLE_T handle;
   uint64_t pid;
} KHRN_PID_MAP_VALUE_T;

static INLINE KHRN_PID_MAP_VALUE_T khrn_pid_map_value_get_none(void)
{
   KHRN_PID_MAP_VALUE_T x = {MEM_INVALID_HANDLE, 0L};
   return x;
}

static INLINE KHRN_PID_MAP_VALUE_T khrn_pid_map_value_get_deleted(void)
{
   KHRN_PID_MAP_VALUE_T x = {(MEM_HANDLE_T)(MEM_INVALID_HANDLE - 1), 0L};
   return x;
}

static INLINE void khrn_pid_map_value_acquire(KHRN_PID_MAP_VALUE_T value)
{
   mem_acquire(value.handle);
}

static INLINE void khrn_pid_map_value_release(KHRN_PID_MAP_VALUE_T value)
{
   mem_release(value.handle);
}

static INLINE bool khrn_pid_map_value_cmp(
   KHRN_PID_MAP_VALUE_T x,
   KHRN_PID_MAP_VALUE_T y)
{
   return (x.handle == y.handle) && (x.pid == y.pid);
}

#endif
