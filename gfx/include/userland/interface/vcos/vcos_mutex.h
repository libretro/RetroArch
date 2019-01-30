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

#ifndef VCOS_MUTEX_H
#define VCOS_MUTEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vcos/vcos_types.h"
#include "vcos.h"

/**
 * \file vcos_mutex.h
 *
 * Mutex API. Mutexes are not re-entrant, as supporting this adds extra code
 * that slows down clients which have been written sensibly.
 *
 * \sa vcos_reentrant_mutex.h
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
VCOS_STATUS_T vcos_mutex_create(VCOS_MUTEX_T *m, const char *name);

/** Delete the mutex.
  */
VCOS_INLINE_DECL
void vcos_mutex_delete(VCOS_MUTEX_T *m);

/**
  * \brief Wait to claim the mutex.
  *
  * On most platforms this always returns VCOS_SUCCESS, and so would ideally be
  * a void function, however some platforms allow a wait to be interrupted so
  * it remains non-void.
  *
  * Try to obtain the mutex.
  * @param m   Mutex to wait on
  * @return VCOS_SUCCESS - mutex was taken.
  *         VCOS_EAGAIN  - could not take mutex.
  */
#ifndef vcos_mutex_lock
VCOS_INLINE_DECL
VCOS_STATUS_T vcos_mutex_lock(VCOS_MUTEX_T *m);

/** Release the mutex.
  */
VCOS_INLINE_DECL
void vcos_mutex_unlock(VCOS_MUTEX_T *m);
#endif

/** Test if the mutex is already locked.
  *
  * @return 1 if mutex is locked, 0 if it is unlocked.
  */
VCOS_INLINE_DECL
int vcos_mutex_is_locked(VCOS_MUTEX_T *m);

/** Obtain the mutex if possible.
  *
  * @param m  the mutex to try to obtain
  *
  * @return VCOS_SUCCESS if mutex is successfully obtained, or VCOS_EAGAIN
  * if it is already in use by another thread.
  */
#ifndef vcos_mutex_trylock
VCOS_INLINE_DECL
VCOS_STATUS_T vcos_mutex_trylock(VCOS_MUTEX_T *m);
#endif


#ifdef __cplusplus
}
#endif
#endif
