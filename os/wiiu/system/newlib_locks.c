/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* devkitPPC's newlib guards stdio, the environment, the time zone and
 * the atexit lists through weak __syscall_lock_* hooks and does no
 * locking when nothing defines them. These give it Cafe OS mutexes,
 * so libc state shared between the frontend's threads is serialised.
 *
 * Its lock word (_LOCK_T, and _LOCK_RECURSIVE_T alike) is a plain int
 * that newlib's static locks start at 0, so a mutex is created on
 * first use and its address published into the word by compare-and-
 * swap - retro_atomic's, which on the Wii U is Cafe OS's own and so
 * holds across the cores. OSMutex is recursive, which serves both kinds
 * of lock.
 *
 * The hooks' shape changed in newlib 4.3; <stdlib.h> brings in
 * __NEWLIB__ and __NEWLIB_MINOR__, which pick the set below, as
 * os/gekko/kernel/newlib.c does. */

#include <stdlib.h>
#include <stdint.h>

#include <retro_atomic.h>
#include <wiiu/os/mutex.h>

typedef int wiiu_newlib_lock_t;

static OSMutex *wiiu_newlib_mutex(wiiu_newlib_lock_t *lock)
{
   OSMutex *mutex  = (OSMutex*)(uintptr_t)
      retro_atomic_load_acquire_int((retro_atomic_int_t*)lock);

   if (mutex)
      return mutex;

   if (!(mutex = (OSMutex*)malloc(sizeof(*mutex))))
      return NULL;
   OSInitMutex(mutex);

   if (!retro_atomic_cas_int((retro_atomic_int_t*)lock, 0,
            (int)(uintptr_t)mutex))
   {
      /* Another thread published its mutex first. */
      free(mutex);
      mutex = (OSMutex*)(uintptr_t)
         retro_atomic_load_acquire_int((retro_atomic_int_t*)lock);
   }
   return mutex;
}

static void wiiu_newlib_lock_free(wiiu_newlib_lock_t *lock)
{
   OSMutex *mutex = (OSMutex*)(uintptr_t)
      retro_atomic_exchange_int((retro_atomic_int_t*)lock, 0);
   free(mutex);
}

static void wiiu_newlib_lock_acquire(wiiu_newlib_lock_t *lock)
{
   OSMutex *mutex = wiiu_newlib_mutex(lock);
   if (mutex)
      OSLockMutex(mutex);
}

static int wiiu_newlib_lock_try_acquire(wiiu_newlib_lock_t *lock)
{
   /* newlib's convention: 0 when the lock was taken. */
   OSMutex *mutex = wiiu_newlib_mutex(lock);
   if (!mutex)
      return 0;
   return OSTryLockMutex(mutex) ? 0 : 1;
}

static void wiiu_newlib_lock_release(wiiu_newlib_lock_t *lock)
{
   OSMutex *mutex = (OSMutex*)(uintptr_t)
      retro_atomic_load_acquire_int((retro_atomic_int_t*)lock);
   if (mutex)
      OSUnlockMutex(mutex);
}

#if defined(__NEWLIB__) && (__NEWLIB__ < 4 \
      || (__NEWLIB__ == 4 && __NEWLIB_MINOR__ < 3))
/* newlib before 4.3: four hooks, one kind of lock word - init says
 * whether it is recursive, and every hook returns an int newlib does
 * not read. OSMutex is recursive, so the flag needs nothing. */
int __syscall_lock_init(wiiu_newlib_lock_t *lock, int recursive)
{
   (void)recursive;
   *lock = 0;
   return 0;
}

int __syscall_lock_acquire(wiiu_newlib_lock_t *lock)
{
   wiiu_newlib_lock_acquire(lock);
   return 0;
}

int __syscall_lock_try_acquire(wiiu_newlib_lock_t *lock)
{
   return wiiu_newlib_lock_try_acquire(lock);
}

int __syscall_lock_release(wiiu_newlib_lock_t *lock)
{
   wiiu_newlib_lock_release(lock);
   return 0;
}

int __syscall_lock_close(wiiu_newlib_lock_t *lock)
{
   wiiu_newlib_lock_free(lock);
   return 0;
}
#else
/* newlib 4.3 and later: separate hooks for the recursive locks. */
void __syscall_lock_init(wiiu_newlib_lock_t *lock)
{
   *lock = 0;
}

void __syscall_lock_acquire(wiiu_newlib_lock_t *lock)
{
   wiiu_newlib_lock_acquire(lock);
}

int __syscall_lock_try_acquire(wiiu_newlib_lock_t *lock)
{
   return wiiu_newlib_lock_try_acquire(lock);
}

void __syscall_lock_release(wiiu_newlib_lock_t *lock)
{
   wiiu_newlib_lock_release(lock);
}

void __syscall_lock_close(wiiu_newlib_lock_t *lock)
{
   wiiu_newlib_lock_free(lock);
}

void __syscall_lock_init_recursive(wiiu_newlib_lock_t *lock)
{
   *lock = 0;
}

void __syscall_lock_acquire_recursive(wiiu_newlib_lock_t *lock)
{
   wiiu_newlib_lock_acquire(lock);
}

int __syscall_lock_try_acquire_recursive(wiiu_newlib_lock_t *lock)
{
   return wiiu_newlib_lock_try_acquire(lock);
}

void __syscall_lock_release_recursive(wiiu_newlib_lock_t *lock)
{
   wiiu_newlib_lock_release(lock);
}

void __syscall_lock_close_recursive(wiiu_newlib_lock_t *lock)
{
   wiiu_newlib_lock_free(lock);
}
#endif
