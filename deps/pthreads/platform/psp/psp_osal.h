/*
 * psp_osal.h
 *
 * Description:
 *
 * --------------------------------------------------------------------------
 *
 *      Pthreads-embedded (PTE) - POSIX Threads Library for embedded systems
 *      Copyright(C) 2008 Jason Schmidlapp
 *
 *      Contact Email: jschmidlapp@users.sourceforge.net
 *
 *      This library is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU Lesser General Public
 *      License as published by the Free Software Foundation; either
 *      version 2 of the License, or (at your option) any later version.
 *
 *      This library is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *      Lesser General Public License for more details.
 *
 *      You should have received a copy of the GNU Lesser General Public
 *      License along with this library in the file COPYING.LIB;
 *      if not, write to the Free Software Foundation, Inc.,
 *      59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <pspsdk.h>

typedef SceUID pte_osThreadHandle;

typedef SceUID pte_osSemaphoreHandle;

typedef SceUID pte_osMutexHandle;


#define OS_IS_HANDLE_VALID(x) ((x) > 0)

#define OS_MAX_SIMUL_THREADS 10

#define OS_DEFAULT_PRIO 11

#define OS_MIN_PRIO 17
#define OS_MAX_PRIO 32

#if 0
#define HAVE_THREAD_SAFE_ERRNO
#endif

#define POLLING_DELAY_IN_us 100

#define OS_MAX_SEM_VALUE 254

int PspInterlockedExchange(int *ptarg, int val);
int PspInterlockedCompareExchange(int *pdest, int exchange, int comp);
int  PspInterlockedExchangeAdd(int volatile* pAddend, int value);
int PspInterlockedDecrement(int *pdest);
int PspInterlockedIncrement(int *pdest);




