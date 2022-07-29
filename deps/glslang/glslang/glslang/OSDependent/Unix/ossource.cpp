//
// Copyright (C) 2002-2005  3Dlabs Inc. Ltd.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//
//    Neither the name of 3Dlabs Inc. Ltd. nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

//
// This file contains the Linux-specific functions
//
#include "../osinclude.h"
#include "../../../OGLCompilersDLL/InitializeDll.h"

#include <pthread.h>
#include <semaphore.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <cstdio>
#include <sys/time.h>
#include <sys/resource.h>

namespace glslang {

//
// Thread cleanup
//

//
// Wrapper for Linux call to DetachThread.  This is required as pthread_cleanup_push() expects
// the cleanup routine to return void.
//
static void DetachThreadLinux(void *)
{
    DetachThread();
}

//
// Thread Local Storage Operations
//
inline OS_TLSIndex PthreadKeyToTLSIndex(pthread_key_t key)
{
    return (OS_TLSIndex)((uintptr_t)key + 1);
}

inline pthread_key_t TLSIndexToPthreadKey(OS_TLSIndex nIndex)
{
    return (pthread_key_t)((uintptr_t)nIndex - 1);
}

OS_TLSIndex OS_AllocTLSIndex()
{
    pthread_key_t pPoolIndex;

    //
    // Create global pool key.
    //
    if ((pthread_key_create(&pPoolIndex, NULL)) != 0)
        return OS_INVALID_TLS_INDEX;
    return PthreadKeyToTLSIndex(pPoolIndex);
}

bool OS_SetTLSValue(OS_TLSIndex nIndex, void *lpvValue)
{
    if (nIndex == OS_INVALID_TLS_INDEX)
        return false;
    if (pthread_setspecific(TLSIndexToPthreadKey(nIndex), lpvValue) != 0)
        return false;
    return true;
}

void* OS_GetTLSValue(OS_TLSIndex nIndex)
{
    // This function should return 0 if nIndex is invalid.
    assert(nIndex != OS_INVALID_TLS_INDEX);
    return pthread_getspecific(TLSIndexToPthreadKey(nIndex));
}

bool OS_FreeTLSIndex(OS_TLSIndex nIndex)
{
    if (nIndex == OS_INVALID_TLS_INDEX)
        return false;

    // Delete the global pool key.
    if (pthread_key_delete(TLSIndexToPthreadKey(nIndex)) != 0)
        return false;
    return true;
}

namespace {
    pthread_mutex_t gMutex;
}

void InitGlobalLock()
{
  pthread_mutexattr_t mutexattr;
  pthread_mutexattr_init(&mutexattr);
  pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&gMutex, &mutexattr);
}

void GetGlobalLock()
{
  pthread_mutex_lock(&gMutex);
}

void ReleaseGlobalLock()
{
  pthread_mutex_unlock(&gMutex);
}

} // end namespace glslang
