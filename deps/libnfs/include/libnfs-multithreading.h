/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2021 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _LIBNFS_MULTITHREADING_H_
#define _LIBNFS_MULTITHREADING_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_MULTITHREADING

#ifdef WIN32
typedef HANDLE libnfs_thread_t;
typedef HANDLE libnfs_mutex_t;
typedef HANDLE libnfs_sem_t;
typedef DWORD nfs_tid_t;
#elif defined(HAVE_PTHREAD)
#include <pthread.h>
typedef pthread_t libnfs_thread_t;
typedef pthread_mutex_t libnfs_mutex_t;

#if defined(__APPLE__) && defined(HAVE_DISPATCH_DISPATCH_H)
#include <dispatch/dispatch.h>
typedef dispatch_semaphore_t libnfs_sem_t;
#else
#include <semaphore.h>
typedef sem_t libnfs_sem_t;
#endif
#ifdef HAVE_PTHREAD_THREADID_NP
typedef uint64_t nfs_tid_t;
#else
typedef pid_t nfs_tid_t;
#endif
#endif /* HAVE_PTHREAD */

nfs_tid_t nfs_mt_get_tid(void);
int nfs_mt_mutex_init(libnfs_mutex_t *mutex);
int nfs_mt_mutex_destroy(libnfs_mutex_t *mutex);
int nfs_mt_mutex_lock(libnfs_mutex_t *mutex);
int nfs_mt_mutex_unlock(libnfs_mutex_t *mutex);

int nfs_mt_sem_init(libnfs_sem_t *sem, int value);
int nfs_mt_sem_destroy(libnfs_sem_t *sem);
int nfs_mt_sem_post(libnfs_sem_t *sem);
int nfs_mt_sem_wait(libnfs_sem_t *sem);

#endif /* HAVE_MULTITHREADING */

#ifdef __cplusplus
}
#endif

#endif /* !_LIBNFS_MULTITHREADING_H_ */
