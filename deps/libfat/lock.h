/*
 lock.h

 Copyright (c) 2008 Sven Peter <svpe@gmx.net>

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation and/or
     other materials provided with the distribution.
  3. The name of the author may not be used to endorse or promote products derived
     from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef _LOCK_H
#define _LOCK_H

#include "common.h"

#ifdef USE_LWP_LOCK

static inline void _FAT_lock_init(mutex_t *mutex)
{
	LWP_MutexInit(mutex, false);
}

static inline void _FAT_lock_deinit(mutex_t *mutex)
{
	LWP_MutexDestroy(*mutex);
}

static inline void _FAT_lock(mutex_t *mutex)
{
	LWP_MutexLock(*mutex);
}

static inline void _FAT_unlock(mutex_t *mutex)
{
	LWP_MutexUnlock(*mutex);
}

#else

/* We still need a blank lock type */
#ifndef mutex_t
typedef int mutex_t;
#endif

void _FAT_lock_init(mutex_t *mutex);
void _FAT_lock_deinit(mutex_t *mutex);
void _FAT_lock(mutex_t *mutex);
void _FAT_unlock(mutex_t *mutex);

#endif /* USE_LWP_LOCK */


#endif /* _LOCK_H */

