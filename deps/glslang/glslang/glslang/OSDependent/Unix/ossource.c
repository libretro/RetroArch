/*
 * Copyright (C) 2002-2005  3Dlabs Inc. Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 *    Neither the name of 3Dlabs Inc. Ltd. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

 /* This file contains the Linux-specific functions */
#include "../osinclude.h"
#include "../../../OGLCompilersDLL/InitializeDll.h"

#include <pthread.h>
#include <semaphore.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>

static pthread_mutex_t glslang_global_lock;

 /* Thread cleanup */

 /* Thread Local Storage Operations */
#define TLS_INDEX_TO_PTHREAD_KEY(nIndex) ((pthread_key_t)((uintptr_t)(nIndex) - 1))

OS_TLSIndex OS_AllocTLSIndex(void)
{
    pthread_key_t pPoolIndex;

    /* Create global pool key. */
    if ((pthread_key_create(&pPoolIndex, NULL)) != 0)
        return OS_INVALID_TLS_INDEX;
    return (OS_TLSIndex)((uintptr_t)pPoolIndex + 1);
}

bool OS_SetTLSValue(OS_TLSIndex nIndex, void *lpvValue)
{
    if (nIndex == OS_INVALID_TLS_INDEX)
        return false;
    if (pthread_setspecific(TLS_INDEX_TO_PTHREAD_KEY(nIndex), lpvValue) != 0)
        return false;
    return true;
}

void *OS_GetTLSValue(OS_TLSIndex nIndex)
{
    /* This function should return 0 if nIndex is invalid. */
    assert(nIndex != OS_INVALID_TLS_INDEX);
    return pthread_getspecific(TLS_INDEX_TO_PTHREAD_KEY(nIndex));
}

bool OS_FreeTLSIndex(OS_TLSIndex nIndex)
{
    if (nIndex == OS_INVALID_TLS_INDEX)
        return false;

    /* Delete the global pool key. */
    if (pthread_key_delete(TLS_INDEX_TO_PTHREAD_KEY(nIndex)) != 0)
        return false;
    return true;
}

void InitGlobalLock(void)
{
  pthread_mutexattr_t mutexattr;
  pthread_mutexattr_init(&mutexattr);
  pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&glslang_global_lock, &mutexattr);
}

void GetGlobalLock(void)
{
  pthread_mutex_lock(&glslang_global_lock);
}

void ReleaseGlobalLock(void)
{
  pthread_mutex_unlock(&glslang_global_lock);
}
