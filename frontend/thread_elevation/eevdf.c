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

/* A short EEVDF slice for the calling thread. Since Linux 6.12 a
 * time-shared thread may ask for its own slice through sched_setattr(),
 * without privilege or any rlimit; a thread that asks for a shorter one
 * than the tasks it shares a CPU with gets an earlier deadline, and so
 * the CPU sooner when it wakes. Its share of the CPU is unchanged. This
 * is a scheduler hint, not a priority: the backend is additive, applied
 * whatever else the chain grants. It leaves a real-time, batch or idle
 * thread alone, and counts only when the slice reads back as asked -
 * kernels before 6.12 accept the call and ignore the field. The slice
 * is inherited by threads this one creates afterwards. */

#include "../thread_elevation.h"

#if defined(__linux__) && !defined(__ANDROID__) && !defined(_WIN32)

#include <stdint.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>
#include <sys/syscall.h>

/* The prototype every Linux libc uses; spelled out so a strict C89
 * build, where glibc hides it, sees the same one. */
extern long syscall(long number, ...);

#if defined(SYS_sched_setattr) && defined(SYS_sched_getattr)

/* The kernel's floor; asked for, it reads back as it is. */
#define THREAD_ELEVATION_EEVDF_SLICE_NS 100000ULL

/* struct sched_attr as of its first version, which every kernel with
 * the calls accepts; not every libc declares it. */
typedef struct thread_elevation_sched_attr
{
   uint32_t size;
   uint32_t sched_policy;
   uint64_t sched_flags;
   int32_t  sched_nice;
   uint32_t sched_priority;
   uint64_t sched_runtime;
   uint64_t sched_deadline;
   uint64_t sched_period;
} thread_elevation_sched_attr_t;

static bool thread_elevation_eevdf_get(thread_elevation_sched_attr_t *a)
{
   memset(a, 0, sizeof(*a));
   return syscall(SYS_sched_getattr, 0, a, (unsigned)sizeof(*a), 0) == 0;
}

static enum thread_elevation_result thread_elevation_eevdf_raise(
      uint64_t tid, unsigned next)
{
   thread_elevation_sched_attr_t a;

   (void)tid;
   (void)next;
   if (     !thread_elevation_eevdf_get(&a)
         || a.sched_policy != SCHED_OTHER)
      return THREAD_ELEVATION_REFUSED;

   /* The same policy and nice value, a slice of its own */
   a.size          = (uint32_t)sizeof(a);
   a.sched_flags   = 0;
   a.sched_runtime = THREAD_ELEVATION_EEVDF_SLICE_NS;
   if (syscall(SYS_sched_setattr, 0, &a, 0) != 0)
      return THREAD_ELEVATION_REFUSED;

   return (     thread_elevation_eevdf_get(&a)
             && a.sched_runtime == THREAD_ELEVATION_EEVDF_SLICE_NS)
      ? THREAD_ELEVATION_GRANTED : THREAD_ELEVATION_REFUSED;
}

#else

/* Headers too old to name the calls: the kernel is too. */
static enum thread_elevation_result thread_elevation_eevdf_raise(
      uint64_t tid, unsigned next)
{
   (void)tid;
   (void)next;
   return THREAD_ELEVATION_REFUSED;
}

#endif

const thread_elevation_backend_t thread_elevation_eevdf = {
   thread_elevation_eevdf_raise,
   "a 0.1 ms EEVDF scheduler slice",
   false,
   true
};

#endif
