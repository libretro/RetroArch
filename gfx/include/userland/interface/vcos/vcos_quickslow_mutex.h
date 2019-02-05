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

/*=============================================================================
VideoCore OS Abstraction Layer - mutex public header file
=============================================================================*/

#ifndef VCOS_QUICKSLOW_MUTEX_H
#define VCOS_QUICKSLOW_MUTEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vcos/vcos_types.h"
#include "vcos.h"

/**
 * \file vcos_quickslow_mutex.h
 *
 * "Quick/Slow" Mutex API. This is a mutex which supports an additional "quick"
 * (spinlock-based) locking mechanism. While in this quick locked state, other
 * operating system commands will be unavailable and the caller should complete
 * whatever it has to do in a short, bounded length of time (as the spinlock
 * completely locks out other system activity).
 *
 * \sa vcos_mutex.h
 *
 */

/** Create a mutex.
  *
  * @param m      Filled in with mutex on return
  * @param name   A non-null name for the mutex, used for diagnostics.
  *
  * @return VCOS_SUCCESS if mutex was created, or error code.
  */
VCOS_INLINE_DECL
VCOS_STATUS_T vcos_quickslow_mutex_create(VCOS_QUICKSLOW_MUTEX_T *m, const char *name);

/** Delete the mutex.
  */
VCOS_INLINE_DECL
void vcos_quickslow_mutex_delete(VCOS_QUICKSLOW_MUTEX_T *m);

/**
  * \brief Wait to claim the mutex ("slow" mode).
  *
  * Obtain the mutex.
  */
VCOS_INLINE_DECL
void vcos_quickslow_mutex_lock(VCOS_QUICKSLOW_MUTEX_T *m);

/** Release the mutex ("slow" mode).
  */
VCOS_INLINE_DECL
void vcos_quickslow_mutex_unlock(VCOS_QUICKSLOW_MUTEX_T *m);

/**
  * \brief Wait to claim the mutex ("quick" mode).
  *
  * Obtain the mutex. The caller must not call any OS functions or do anything
  * "slow" before the corresponding call to vcos_mutex_quickslow_unlock_quick.
  */
VCOS_INLINE_DECL
void vcos_quickslow_mutex_lock_quick(VCOS_QUICKSLOW_MUTEX_T *m);

/** Release the mutex ("quick" mode).
  */
VCOS_INLINE_DECL
void vcos_quickslow_mutex_unlock_quick(VCOS_QUICKSLOW_MUTEX_T *m);

#ifdef __cplusplus
}
#endif
#endif
