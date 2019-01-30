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

#ifndef KHRN_CLIENT_PLATFORM_FILLER_DIRECT_H
#define KHRN_CLIENT_PLATFORM_FILLER_DIRECT_H

#include "interface/vcos/vcos.h"

typedef int KHR_STATUS_T;
#define KHR_SUCCESS  0
/*
   mutex
*/

typedef struct PLATFORM_MUTEX_T
{
   /* nothing needed here */
#if defined(_MSC_VER) || defined(__CC_ARM)
   char dummy;    /* empty structures are not allowed */
#endif
} PLATFORM_MUTEX_T;


VCOS_STATIC_INLINE
VCOS_STATUS_T platform_mutex_create(PLATFORM_MUTEX_T *mutex) {
   UNUSED(mutex);
   // Nothing to do
   return VCOS_SUCCESS;
}

VCOS_STATIC_INLINE
void platform_mutex_destroy(PLATFORM_MUTEX_T *mutex)
{
   UNUSED(mutex);
   /* Nothing to do */
}

VCOS_STATIC_INLINE
void platform_mutex_acquire(PLATFORM_MUTEX_T *mutex)
{
   UNUSED(mutex);
   /* Nothing to do */
}

VCOS_STATIC_INLINE
void platform_mutex_release(PLATFORM_MUTEX_T *mutex)
{
   UNUSED(mutex);
   /* Nothing to do */
}

/*
   named counting semaphore
*/

typedef VCOS_NAMED_SEMAPHORE_T PLATFORM_SEMAPHORE_T;

/*
   VCOS_STATUS_T khronos_platform_semaphore_create(PLATFORM_SEMAPHORE_T *sem, int name[3], int count);

   Preconditions:
      sem is a valid pointer to an uninitialised variable
      name is a valid pointer to three elements

   Postconditions:
      If return value is KHR_SUCCESS then sem contains a valid PLATFORM_SEMAPHORE_T
*/

extern VCOS_STATUS_T khronos_platform_semaphore_create(PLATFORM_SEMAPHORE_T *sem, int name[3], int count);
extern void khronos_platform_semaphore_destroy(PLATFORM_SEMAPHORE_T *sem);
extern void khronos_platform_semaphore_acquire(PLATFORM_SEMAPHORE_T *sem);
extern void khronos_platform_semaphore_release(PLATFORM_SEMAPHORE_T *sem);

/*
   thread-local storage
*/

typedef void *PLATFORM_TLS_T;

extern VCOS_STATUS_T platform_tls_create(PLATFORM_TLS_T *tls);
extern void platform_tls_destroy(PLATFORM_TLS_T tls);
extern void platform_tls_set(PLATFORM_TLS_T tls, void *v);
extern void *platform_tls_get(PLATFORM_TLS_T tls);
extern void* platform_tls_get_check(PLATFORM_TLS_T tls);
extern void platform_tls_remove(PLATFORM_TLS_T tls);

#endif


