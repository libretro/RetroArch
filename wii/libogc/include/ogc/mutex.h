/*-------------------------------------------------------------

mutex.h -- Thread subsystem III

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#ifndef __MUTEX_H__
#define __MUTEX_H__

/*! \file mutex.h
\brief Thread subsystem III

*/

#include <gctypes.h>

#define LWP_MUTEX_NULL		0xffffffff

#ifdef __cplusplus
	extern "C" {
#endif

/*! \typedef u32 mutex_t
\brief typedef for the mutex handle
*/
typedef u32 mutex_t;

/*! \fn s32 LWP_MutexInit(mutex_t *mutex,bool use_recursive)
\brief Initializes a mutex lock.
\param[out] mutex pointer to a mutex_t handle.
\param[in] use_recursive whether to allow the thread, whithin the same context, to enter multiple times the lock or not.

\return 0 on success, <0 on error
*/
s32 LWP_MutexInit(mutex_t *mutex,bool use_recursive);

/*! \fn s32 LWP_MutexDestroy(mutex_t mutex)
\brief Close mutex lock, release all threads and handles locked on this mutex.
\param[in] mutex handle to the mutex_t structure.

\return 0 on success, <0 on error
*/
s32 LWP_MutexDestroy(mutex_t mutex);

/*! \fn s32 LWP_MutexLock(mutex_t mutex)
\brief Enter the mutex lock.
\param[in] mutex handle to the mutext_t structure.

\return 0 on success, <0 on error
*/
s32 LWP_MutexLock(mutex_t mutex);

/*! \fn s32 LWP_MutexTryLock(mutex_t mutex)
\brief Try to enter the mutex lock.
\param[in] mutex handle to the mutex_t structure.

\return 0: on first aquire, 1: would lock
*/
s32 LWP_MutexTryLock(mutex_t mutex);

/*! \fn s32 LWP_MutexUnlock(mutex_t mutex)
\brief Release the mutex lock and let other threads process further on this mutex.
\param[in] mutex handle to the mutex_t structure.

\return 0 on success, <0 on error
*/
s32 LWP_MutexUnlock(mutex_t mutex);

#ifdef __cplusplus
	}
#endif

#endif
