/*-------------------------------------------------------------

semaphore.h -- Thread subsystem IV

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

#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__

/*! \file semaphore.h
\brief Thread subsystem IV

*/

#include <gctypes.h>

#define LWP_SEM_NULL			0xffffffff

#ifdef __cplusplus
extern "C" {
#endif

/*! \typedef u32 sem_t
\brief typedef for the semaphore handle
*/
typedef u32 sem_t;

/*! \fn s32 LWP_SemInit(sem_t *sem,u32 start,u32 max)
\brief Initializes a semaphore.
\param[out] sem pointer to a sem_t handle.
\param[in] start start count of the semaphore
\param[in] max maximum count of the semaphore

\return 0 on success, <0 on error
*/
s32 LWP_SemInit(sem_t *sem,u32 start,u32 max);

/*! \fn s32 LWP_SemDestroy(sem_t sem)
\brief Close and destroy a semaphore, release all threads and handles locked on this semaphore.
\param[in] sem handle to the sem_t structure.

\return 0 on success, <0 on error
*/
s32 LWP_SemDestroy(sem_t sem);

/*! \fn s32 LWP_SemWait(sem_t sem)
\brief Count down semaphore counter and enter lock if counter <=0
\param[in] sem handle to the sem_t structure.

\return 0 on success, <0 on error
*/
s32 LWP_SemWait(sem_t sem);

/*! \fn s32 LWP_SemPost(sem_t sem)
\brief Count up semaphore counter and release lock if counter >0
\param[in] sem handle to the sem_t structure.

\return 0 on success, <0 on error
*/
s32 LWP_SemPost(sem_t sem);

#ifdef __cplusplus
	}
#endif

#endif
