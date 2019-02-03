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
VideoCore OS Abstraction Layer - public header file
=============================================================================*/

#ifndef VCOS_SEMAPHORE_H
#define VCOS_SEMAPHORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vcos/vcos_types.h"
#ifndef VCOS_PLATFORM_H
#include "vcos.h"
#endif

/**
 * \file vcos_semaphore.h
 *
 * \section sem Semaphores
 *
 * This provides counting semaphores. Semaphores are not re-entrant. On sensible
 * operating systems a semaphore can always be posted but can only be taken in
 * thread (not interrupt) context. Under Nucleus, a LISR cannot post a semaphore,
 * although it would not be hard to lift this restriction.
 *
 * \subsection timeout Timeout
 *
 * On both Nucleus and ThreadX a semaphore can be taken with a timeout. This is
 * not supported by VCOS because it makes the non-timeout code considerably more
 * complicated (and hence slower). In the unlikely event that you need a timeout
 * with a semaphore, and you cannot simply redesign your code to avoid it, use
 * an event flag (vcos_event_flags.h).
 *
 * \subsection sem_nucleus Changes from Nucleus:
 *
 *  Semaphores are always "FIFO" - i.e. sleeping threads are woken in FIFO order. That's
 *  because:
 *  \arg there's no support for NU_PRIORITY in threadx (though it can be emulated, slowly)
 *  \arg we don't appear to actually consciously use it - for example, Dispmanx uses
 *  it, but all threads waiting are the same priority.
 *
 */

/**
  * \brief Create a semaphore.
  *
  * Create a semaphore.
  *
  * @param sem   Pointer to memory to be initialized
  * @param name  A name for this semaphore. The name may be truncated internally.
  * @param count The initial count for the semaphore.
  *
  * @return VCOS_SUCCESS if the semaphore was created.
  *
  */
VCOS_INLINE_DECL
VCOS_STATUS_T vcos_semaphore_create(VCOS_SEMAPHORE_T *sem, const char *name, VCOS_UNSIGNED count);

/**
  * \brief Wait on a semaphore.
  *
  * There is no timeout option on a semaphore, as adding this will slow down
  * implementations on some platforms. If you need that kind of behaviour, use
  * an event group.
  *
  * On most platforms this always returns VCOS_SUCCESS, and so would ideally be
  * a void function, however some platforms allow a wait to be interrupted so
  * it remains non-void.
  *
  * @param sem Semaphore to wait on
  * @return VCOS_SUCCESS - semaphore was taken.
  *         VCOS_EAGAIN  - could not take semaphore
  *
  */
VCOS_INLINE_DECL
VCOS_STATUS_T vcos_semaphore_wait(VCOS_SEMAPHORE_T *sem);

/**
  * \brief Wait on a semaphore with a timeout.
  *
  * Note that this function may not be implemented on all
  * platforms, and may not be efficient on all platforms
  * (see comment in vcos_semaphore_wait)
  *
  * Try to obtain the semaphore. If it is already taken, return
  * VCOS_EAGAIN.
  * @param sem Semaphore to wait on
  * @param timeout Number of milliseconds to wait before
  *                returning if the semaphore can't be acquired.
  * @return VCOS_SUCCESS - semaphore was taken.
  *         VCOS_EAGAIN - could not take semaphore (i.e. timeout
  *         expired)
  *         VCOS_EINVAL - Some other error (most likely bad
  *         parameters).
  */
VCOS_INLINE_DECL
VCOS_STATUS_T vcos_semaphore_wait_timeout(VCOS_SEMAPHORE_T *sem, VCOS_UNSIGNED timeout);

/**
  * \brief Try to wait for a semaphore.
  *
  * Try to obtain the semaphore. If it is already taken, return VCOS_TIMEOUT.
  * @param sem Semaphore to wait on
  * @return VCOS_SUCCESS - semaphore was taken.
  *         VCOS_EAGAIN - could not take semaphore
  */
VCOS_INLINE_DECL
VCOS_STATUS_T vcos_semaphore_trywait(VCOS_SEMAPHORE_T *sem);

/**
  * \brief Post a semaphore.
  *
  * @param sem Semaphore to wait on
  */
VCOS_INLINE_DECL
VCOS_STATUS_T vcos_semaphore_post(VCOS_SEMAPHORE_T *sem);

/**
  * \brief Delete a semaphore, releasing any resources consumed by it.
  *
  * @param sem Semaphore to wait on
  */
VCOS_INLINE_DECL
void vcos_semaphore_delete(VCOS_SEMAPHORE_T *sem);

#ifdef __cplusplus
}
#endif
#endif
