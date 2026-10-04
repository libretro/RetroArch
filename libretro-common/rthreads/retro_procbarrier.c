/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_procbarrier.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* See the header for what this is and why each tier is shaped the way it
 * is. This file is the platform plumbing.
 *
 * Every tier below was checked against the kernel that provides it, by
 * reading its source or disassembling it, before being written down here:
 *
 *   Linux membarrier   kernel/sched/membarrier.c: PRIVATE_EXPEDITED IPIs
 *                      each online CPU whose current task shares our mm,
 *                      with ipi_mb(), which is smp_mb(). 4.14 and later.
 *                      4.3-4.13 only have SHARED, an RCU grace period that
 *                      waits for every CPU on the system; that is not
 *                      used here, since it can take milliseconds.
 *   Linux page flip    arch/x86/mm/tlb.c: flush_tlb_mm_range sends an IPI
 *                      to mm_cpumask(mm), back to at least 2.6.32.
 *   Windows FPWB       ntoskrnl 10.0.26100 KeFlushProcessWriteBuffers:
 *                      raise IRQL, skip if one CPU, else KiIpiSendRequestEx
 *                      to the affinity and spin for acks.
 *   Windows page flip  The same IPI, reached through the TLB shootdown a
 *                      protection change forces. All APIs it needs exist
 *                      on Windows 2000.
 *   Darwin page flip   osfmk/x86_64/pmap.c pmap_flush_tlbs:
 *                      i386_signal_cpu(cpu, MP_TLB_FLUSH) per active CPU,
 *                      then a cpus_to_respond wait.
 *   Darwin signal      thread_get_state on another thread:
 *                      thread_hold, then thread_stop, which waits until
 *                      the target is off every CPU (cause_ast_check:
 *                      i386_signal_cpu(MP_AST) on x86, cpu_signal(SIGPast)
 *                      on arm64 and ppc), then thread_unstop and
 *                      thread_release before the call returns. The same
 *                      sequence in the current tree and in xnu-1228.
 *   Linux signal       Signal delivery to a running thread goes through
 *                      kick_process(), which is an IPI.
 *
 * Any number of threads may be inside retro_procbarrier() at once. Every
 * eventcount built on it calls it from its own waiter, so concurrent
 * callers are the normal case, and no tier keeps per-call state anywhere
 * another caller can reach it or makes one caller wait for another.
 */

#include <retro_posix_source.h>

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <retro_atomic.h>
#include <rthreads/retro_procbarrier.h>

/* ------------------------------------------------------------------ */
/* Platform selection                                                  */
/* ------------------------------------------------------------------ */

#if defined(_WIN32) || defined(USE_WIN32_THREADS)
#define PB_WINDOWS 1
#elif defined(__APPLE__)
#define PB_DARWIN 1
#elif defined(__linux__) || defined(__ANDROID__)
#define PB_LINUX 1
#elif defined(__FreeBSD__) && !defined(__ORBIS__) && !defined(ORBIS)
/* The PS4 is FreeBSD underneath and its toolchain defines __FreeBSD__,
 * but Sony's kernel is a 9-era fork with none of the 14.1 membarrier
 * and no process-barrier API of its own. It is a multi-core console and
 * resolves to NONE like the others. */
#define PB_FREEBSD 1
#endif

#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
#define PB_X86 1
#endif

#if defined(PB_WINDOWS)
#include <windows.h>
#endif

#if defined(PB_LINUX) || defined(PB_FREEBSD) || defined(PB_DARWIN)
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/mman.h>
#include <pthread.h>
#endif

#if defined(PB_LINUX)
#include <time.h>
#include <sched.h>
#if defined(__ANDROID__)
#include <sys/system_properties.h>
#endif
#include <sys/syscall.h>
#include <dirent.h>
#include <fcntl.h>
#endif

#if defined(PB_FREEBSD)
#include <sys/param.h>
/* membarrier(2) arrived in FreeBSD 14.1, and so did its header; on
 * anything older the include would fail and the tier is simply absent,
 * leaving the x86 page flip. */
#if defined(__FreeBSD_version) && __FreeBSD_version >= 1401000
#include <sys/membarrier.h>
#define PB_FREEBSD_MEMBARRIER 1
#endif
#endif

#if defined(PB_DARWIN)
#include <mach/mach.h>
#include <mach/thread_info.h>
#include <mach/thread_act.h>
#include <mach/task.h>
#include <mach/thread_status.h>

/* The register state the Darwin tier asks for. Its contents are never
 * read; any flavour costs the same stop, so this is the smallest one
 * each architecture has, and each exists in every SDK that architecture
 * was ever built with. */
#if defined(__arm64__) || defined(__aarch64__)
#define PB_DARWIN_FLAVOR  ARM_THREAD_STATE64
#define PB_DARWIN_COUNT   ARM_THREAD_STATE64_COUNT
typedef arm_thread_state64_t pb_darwin_state_t;
#elif defined(__arm__)
#define PB_DARWIN_FLAVOR  ARM_THREAD_STATE
#define PB_DARWIN_COUNT   ARM_THREAD_STATE_COUNT
typedef arm_thread_state_t pb_darwin_state_t;
#elif defined(__x86_64__)
#define PB_DARWIN_FLAVOR  x86_THREAD_STATE64
#define PB_DARWIN_COUNT   x86_THREAD_STATE64_COUNT
typedef x86_thread_state64_t pb_darwin_state_t;
#elif defined(__i386__)
#define PB_DARWIN_FLAVOR  i386_THREAD_STATE
#define PB_DARWIN_COUNT   i386_THREAD_STATE_COUNT
typedef i386_thread_state_t pb_darwin_state_t;
#elif defined(__ppc64__)
#define PB_DARWIN_FLAVOR  PPC_THREAD_STATE64
#define PB_DARWIN_COUNT   PPC_THREAD_STATE64_COUNT
typedef ppc_thread_state64_t pb_darwin_state_t;
#elif defined(__ppc__)
#define PB_DARWIN_FLAVOR  PPC_THREAD_STATE
#define PB_DARWIN_COUNT   PPC_THREAD_STATE_COUNT
typedef ppc_thread_state_t pb_darwin_state_t;
#else
#define PB_DARWIN_NO_STATE 1
#endif
#endif

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */

/* s_tier holds one more state than the public enum: RESOLVED_NONE, for
 * "probed, and this platform has nothing". The public NONE is what a
 * caller sees in both that case and before init, but the two must not
 * be confused internally, or an unsupported platform re-runs the whole
 * probe -- sigaction, mmap, a /proc walk -- on every call. */
#define PB_RESOLVED_NONE  (-1)

static retro_atomic_int_t s_tier;    /* PB_RESOLVED_NONE or a public tier */
static int                s_signum;

#if defined(PB_WINDOWS)
typedef VOID (WINAPI *pb_fpwb_t)(VOID);
static pb_fpwb_t s_fpwb;
#endif

/* Page-flip storage: only where the tier exists, so a console build is
 * not left with variables nothing references. The gate is defined below
 * with the tier; forward the condition here. */
#if defined(PB_X86) && (defined(PB_WINDOWS) || defined(PB_LINUX) \
   || defined(PB_FREEBSD) || defined(PB_DARWIN))
/* A flip needs a page no other caller is flipping: one caller's
 * no-access protection landing between another's read-write protection
 * and its store faults that store. A caller claims a free slot, and
 * when every slot is taken flips a page of its own for that call, so
 * no caller ever waits for another. */
#define PB_FLIP_SLOTS 4
static void              *s_flip_page[PB_FLIP_SLOTS];
static retro_atomic_int_t s_flip_busy[PB_FLIP_SLOTS];
static retro_atomic_int_t s_flip_next;
#if defined(PB_WINDOWS)
static SIZE_T             s_page_size;
#else
static size_t             s_page_size;
#endif
#endif

#if defined(PB_LINUX)
/* One target of a signal-tier walk. Slots live in a static pool and the
 * queued signal carries a slot's index and generation, never an
 * address: a slot is claimed for one target of one walk, closed by the
 * target's handler or by the walker on finding the thread gone or the
 * signal blocked, and released once the walk is done with its batch. A
 * signal that is delivered late -- to a thread that blocked it and later
 * unblocks -- finds the slot released or claimed again under another
 * generation and is ignored, so the pointer to the walker's count is
 * only ever followed while the walker is still waiting on it.
 *
 * state holds the generation shifted up one bit and the closed bit
 * below it, so one compare-and-swap checks the generation and closes.
 * 0 is a free slot; generation 0 is never issued. */
typedef struct pb_ack_slot
{
   retro_atomic_int_t *acks;
   unsigned long       start;   /* the thread's start time, its identity */
   pid_t               tid;
   retro_atomic_int_t  state;
} pb_ack_slot_t;

#define PB_SLOT_POOL      128
#define PB_SLOT_GEN_BITS  23   /* with the closed bit and the index, an int */
#define PB_SLOT_GEN_MASK  ((1 << PB_SLOT_GEN_BITS) - 1)
#define PB_SLOT_OPEN(gen) ((gen) << 1)
#define PB_SLOT_DONE(gen) (((gen) << 1) | 1)
/* The value a queued signal carries: index low, generation above it. */
#define PB_SLOT_TOKEN(idx, gen) (((gen) << 8) | (idx))

static pb_ack_slot_t      s_slots[PB_SLOT_POOL];
static retro_atomic_int_t s_slot_gen;

/* The probe's count. */
static retro_atomic_int_t s_probe_acks;
#endif

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static unsigned pb_num_cpus(void)
{
#if defined(PB_WINDOWS)
   SYSTEM_INFO si;
   GetSystemInfo(&si);
   return (unsigned)si.dwNumberOfProcessors;
#elif defined(PB_LINUX) || defined(PB_FREEBSD) || defined(PB_DARWIN)
   /* Only on the platforms this file otherwise handles. Keying on
    * _SC_NPROCESSORS_ONLN being defined is not enough: PSP's newlib
    * defines the constant and has no sysconf, and the link fails. */
   long n = sysconf(_SC_NPROCESSORS_ONLN);
   return (n > 0) ? (unsigned)n : 1u;
#elif defined(PSP) || defined(PS2) || defined(GEKKO) \
   || defined(DJGPP) || defined(__DJGPP__)
   /* Single-core consoles: nothing to fence against. Naming them here
    * is what lets the barrier be free there rather than NONE. */
   return 1u;
#else
   return 2u;   /* unknown: assume SMP, which is the safe direction */
#endif
}

/* ------------------------------------------------------------------ */
/* Tier: membarrier                                                    */
/* ------------------------------------------------------------------ */

#if defined(PB_LINUX)
/* ABI constants, spelled out rather than taken from the header: the
 * values are fixed since 4.14 but the header may be older than the
 * kernel that is actually running. */
#ifndef __NR_membarrier
#define PB_NO_MEMBARRIER 1
#endif
#define PB_MEMBARRIER_CMD_QUERY                        0
#define PB_MEMBARRIER_CMD_PRIVATE_EXPEDITED            (1 << 3)
#define PB_MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED   (1 << 4)

#if defined(__ANDROID__)
/* Android's seccomp filter answers a syscall outside its allowlist with
 * SIGSYS, which ends the process, and membarrier joined that allowlist
 * in Android 10 (API 29): on 8.0 through 9 even the query would be the
 * end. The API level decides whether the probe is made at all; an
 * unreadable one counts as too old. */
static int pb_android_api_level(void)
{
   char v[PROP_VALUE_MAX];
   if (__system_property_get("ro.build.version.sdk", v) <= 0)
      return 0;
   return atoi(v);
}
#endif

static int pb_membarrier_try(void)
{
#if defined(PB_NO_MEMBARRIER)
   return 0;
#else
   long q;
#if defined(__ANDROID__)
   if (pb_android_api_level() < 29)
      return 0;
#endif
   q = syscall(__NR_membarrier, PB_MEMBARRIER_CMD_QUERY, 0, 0);
   if (q < 0)
      return 0;   /* ENOSYS before 4.3, or filtered */
   if (!(q & PB_MEMBARRIER_CMD_PRIVATE_EXPEDITED))
      return 0;   /* 4.3-4.13: only the slow SHARED form; not used */
   if (syscall(__NR_membarrier,
            PB_MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED, 0, 0) != 0)
      return 0;
   /* Registration succeeding is not the same as the barrier working: a
    * sandbox can permit the query and the registration and still refuse
    * the expedited call. Issue one now and believe its return value. A
    * tier that would silently fence nothing is worse than no tier. */
   if (syscall(__NR_membarrier, PB_MEMBARRIER_CMD_PRIVATE_EXPEDITED, 0, 0) != 0)
      return 0;
   return 1;
#endif
}

static void pb_membarrier(void)
{
#if !defined(PB_NO_MEMBARRIER)
   syscall(__NR_membarrier, PB_MEMBARRIER_CMD_PRIVATE_EXPEDITED, 0, 0);
#endif
}
#endif

#if defined(PB_FREEBSD_MEMBARRIER)
static int pb_membarrier_try(void)
{
   int q = membarrier(MEMBARRIER_CMD_QUERY, 0, 0);
   if (q < 0 || !(q & MEMBARRIER_CMD_PRIVATE_EXPEDITED))
      return 0;
   if (membarrier(MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED, 0, 0) != 0)
      return 0;
   return 1;
}

static void pb_membarrier(void)
{
   membarrier(MEMBARRIER_CMD_PRIVATE_EXPEDITED, 0, 0);
}
#endif

/* ------------------------------------------------------------------ */
/* Tier: FlushProcessWriteBuffers                                      */
/* ------------------------------------------------------------------ */

#if defined(PB_WINDOWS)
static int pb_fpwb_try(void)
{
   HMODULE k32 = GetModuleHandleA("kernel32.dll");
   if (!k32)
      return 0;
   /* FARPROC to pb_fpwb_t directly: both are function pointers, and
    * routing the cast through void* is what ISO C objects to. */
   s_fpwb = (pb_fpwb_t)GetProcAddress(k32, "FlushProcessWriteBuffers");
   return s_fpwb != NULL;
}
#endif

/* ------------------------------------------------------------------ */
/* Tier: page flip                                                     */
/* ------------------------------------------------------------------ */

/* The page flip needs mmap/mprotect (or the Win32 equivalents), so it
 * exists only on the platforms this file handles, not merely on x86: an
 * x86 console has neither. */
#if defined(PB_X86) && (defined(PB_WINDOWS) || defined(PB_LINUX) \
   || defined(PB_FREEBSD) || defined(PB_DARWIN))
#define PB_HAVE_PAGEFLIP 1
#endif

#if defined(PB_HAVE_PAGEFLIP)
#if !defined(PB_WINDOWS) && !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON   /* SDKs before 10.11 spell it MAP_ANON */
#endif

/* A locked page: one that is paged out has no TLB entry to shoot down. */
static void *pb_page_new(void)
{
#if defined(PB_WINDOWS)
   void *p = VirtualAlloc(NULL, s_page_size, MEM_COMMIT | MEM_RESERVE,
                          PAGE_READWRITE);
   if (p)
      VirtualLock(p, s_page_size);
   return p;
#else
   void *p = mmap(NULL, s_page_size, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
   if (p == MAP_FAILED)
      return NULL;
   mlock(p, s_page_size);
   return p;
#endif
}

static void pb_page_free(void *p)
{
#if defined(PB_WINDOWS)
   VirtualFree(p, 0, MEM_RELEASE);
#else
   munmap(p, s_page_size);
#endif
}

/* The store dirties the entry so no kernel can skip the shootdown as
 * unnecessary; the protection drop is what sends it. */
static void pb_page_flip(void *p)
{
#if defined(PB_WINDOWS)
   DWORD old;
   VirtualProtect(p, s_page_size, PAGE_READWRITE, &old);
   *(volatile char*)p = 0;
   VirtualProtect(p, s_page_size, PAGE_NOACCESS, &old);
#else
   mprotect(p, s_page_size, PROT_READ | PROT_WRITE);
   *(volatile char*)p = 0;
   mprotect(p, s_page_size, PROT_NONE);
#endif
}

static int pb_pageflip_try(void)
{
   int i;
#if defined(PB_WINDOWS)
   SYSTEM_INFO si;
   GetSystemInfo(&si);
   s_page_size = si.dwPageSize;
#else
   long ps     = sysconf(_SC_PAGESIZE);
   s_page_size = (ps > 0) ? (size_t)ps : 4096u;
#endif
   for (i = 0; i < PB_FLIP_SLOTS; i++)
   {
      retro_atomic_int_init(&s_flip_busy[i], 0);
      s_flip_page[i] = pb_page_new();
   }
   /* A slot whose page could not be had is skipped by every caller;
    * none at all means the tier is not there. */
   for (i = 0; i < PB_FLIP_SLOTS; i++)
      if (s_flip_page[i])
         return 1;
   return 0;
}

static void pb_pageflip(void)
{
   int i;
   unsigned first = (unsigned)retro_atomic_fetch_add_int(&s_flip_next, 1);
   for (i = 0; i < PB_FLIP_SLOTS; i++)
   {
      unsigned slot = (first + (unsigned)i) % PB_FLIP_SLOTS;
      if (!s_flip_page[slot])
         continue;
      if (retro_atomic_cas_int(&s_flip_busy[slot], 0, 1))
      {
         pb_page_flip(s_flip_page[slot]);
         retro_atomic_store_release_int(&s_flip_busy[slot], 0);
         return;
      }
   }
   /* Every slot is mid-flip. A page of this call's own; releasing it
    * shoots the entry down again, which is harmless. */
   {
      void *p = pb_page_new();
      if (p)
      {
         pb_page_flip(p);
         pb_page_free(p);
      }
   }
}
#endif

/* ------------------------------------------------------------------ */
/* Tier: signal                                                        */
/* ------------------------------------------------------------------ */

#if defined(PB_LINUX) || defined(PB_DARWIN)

#if defined(PB_LINUX)
/* Each barrier queues its signals carrying the address of its own
 * acknowledgement counter, so concurrent barriers never count each
 * other's acknowledgements, and there is no shared state to reset.
 * Queued real-time signals are never merged, so every one sent is one
 * handled. rt_tgsigqueueinfo is Linux 2.6.31; Android's seccomp policy
 * has allowed it since its first release in 8.0. */
#if defined(__NR_rt_tgsigqueueinfo)
#define PB_HAVE_TGSIGQUEUE 1
#endif

/* The barrier sleeps on its counter instead of spinning: a spinning
 * barrier holds the CPU a runnable target needs to run its handler, and
 * with more threads than CPUs every acknowledgement then costs a
 * scheduler quantum. ABI values, the same from 2.6.22 on. */
#if defined(__NR_futex)
#define PB_FUTEX_WAIT_PRIVATE 128
#define PB_FUTEX_WAKE_PRIVATE 129
#endif

/* What orders the walker's writes to a slot before the handler reads it
 * is the syscall that queues the signal, and ThreadSanitizer does not
 * model a syscall as synchronisation. Under it the edge is stated
 * explicitly; everywhere else these are nothing. */
#if defined(__SANITIZE_THREAD__)
#define PB_TSAN 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define PB_TSAN 1
#endif
#endif
#if defined(PB_TSAN)
void __tsan_acquire(void *addr);
void __tsan_release(void *addr);
#define PB_TSAN_ACQUIRE(p) __tsan_acquire((void*)(p))
#define PB_TSAN_RELEASE(p) __tsan_release((void*)(p))
/* ThreadSanitizer defers a signal into one slot per signal number per
 * thread and drops a second of the same number that arrives before the
 * first is handled (tsan_interceptors_posix.cpp, pending_signals), where
 * the kernel would queue it. Two walks with a target in common would
 * lose an acknowledgement to that, so under the tool walks run one at a
 * time. The tool checks the slot handoff either way. */
static pthread_mutex_t s_tsan_walk_lock = PTHREAD_MUTEX_INITIALIZER;
/* Spun, not blocked on: the tool runs a deferred handler only once the
 * thread is back in code it instruments, and a waiter asleep inside
 * pthread_mutex_lock never is, so the walker holding the lock would wait
 * on that waiter's acknowledgement forever. */
static void pb_tsan_walk_lock(void)
{
   while (pthread_mutex_trylock(&s_tsan_walk_lock) != 0)
      sched_yield();
}
#define PB_TSAN_WALK_LOCK()   pb_tsan_walk_lock()
#define PB_TSAN_WALK_UNLOCK() pthread_mutex_unlock(&s_tsan_walk_lock)
#else
#define PB_TSAN_WALK_LOCK()   ((void)0)
#define PB_TSAN_WALK_UNLOCK() ((void)0)
#define PB_TSAN_ACQUIRE(p) ((void)0)
#define PB_TSAN_RELEASE(p) ((void)0)
#endif

static void pb_ack_handler(int sig, siginfo_t *si, void *uc)
{
   /* Async-signal-safe: getpid, one atomic increment and a futex wake,
    * with errno preserved for the interrupted code. Only a signal this
    * file queued carries a counter; one of the same number from
    * anywhere else is ignored. The release pairs with the acquire in
    * the waiter, so the interrupted thread's earlier stores are ordered
    * before its acknowledgement. */
   int saved_errno = errno;
   (void)sig;
   (void)uc;
   if (si && si->si_code == SI_QUEUE && si->si_pid == getpid())
   {
      int            token = si->si_value.sival_int;
      int            gen   = (token >> 8) & PB_SLOT_GEN_MASK;
      pb_ack_slot_t *slot  = &s_slots[token & (PB_SLOT_POOL - 1)];
      PB_TSAN_ACQUIRE(slot);
      /* One CAS: the slot is still this signal's, and now closed. */
      if (retro_atomic_cas_int(&slot->state, PB_SLOT_OPEN(gen),
               PB_SLOT_DONE(gen)))
      {
         retro_atomic_int_t *acks = slot->acks;
         retro_atomic_fetch_add_int(acks, 1);
#if defined(PB_FUTEX_WAKE_PRIVATE)
         syscall(__NR_futex, (void*)acks, PB_FUTEX_WAKE_PRIVATE, 1,
               NULL, NULL, 0);
#endif
      }
   }
   errno = saved_errno;
}

/* Claims a free slot for a target, returning its index or -1 when every
 * one is taken. Slots are held only while a walk waits on a batch, so a
 * caller that gets -1 has other walks to wait behind. */
static int pb_slot_claim(retro_atomic_int_t *acks, pid_t tid,
      unsigned long start, int *gen_out)
{
   int i;
   int gen = (retro_atomic_fetch_add_int(&s_slot_gen, 1) + 1) & PB_SLOT_GEN_MASK;
   if (!gen)
      gen = (retro_atomic_fetch_add_int(&s_slot_gen, 1) + 1) & PB_SLOT_GEN_MASK;
   for (i = 0; i < PB_SLOT_POOL; i++)
   {
      pb_ack_slot_t *slot = &s_slots[i];
      if (retro_atomic_load_relaxed_int(&slot->state) != 0)
         continue;
      /* The CAS makes the slot this walk's; only then are the fields
       * written, so a walk that lost the race for it never writes over
       * the winner's. Nothing reads them before the signal that is sent
       * after them, and a stale signal cannot match this generation. */
      if (!retro_atomic_cas_int(&slot->state, 0, PB_SLOT_OPEN(gen)))
         continue;
      slot->acks  = acks;
      slot->tid   = tid;
      slot->start = start;
      *gen_out    = gen;
      return i;
   }
   return -1;
}

/* 0 when queued; otherwise -1 with errno set. */
static long pb_send_ack(pid_t pid, pid_t tid, int idx, int gen)
{
#if defined(PB_HAVE_TGSIGQUEUE)
   siginfo_t si;
   memset(&si, 0, sizeof(si));
   si.si_signo           = s_signum;
   si.si_code            = SI_QUEUE;
   si.si_pid             = pid;
   si.si_uid             = getuid();
   si.si_value.sival_int = PB_SLOT_TOKEN(idx, gen);
   PB_TSAN_RELEASE(&s_slots[idx]);
   return syscall(__NR_rt_tgsigqueueinfo, pid, tid, s_signum, &si);
#else
   (void)pid;
   (void)tid;
   (void)idx;
   (void)gen;
   errno = ENOSYS;
   return -1;
#endif
}
#endif

static int pb_signal_try(int signum)
{
#if defined(PB_DARWIN)
   (void)signum;   /* no signal on Darwin; see pb_signal_barrier */
#if defined(PB_DARWIN_NO_STATE)
   return 0;
#endif
   {
      /* Prove task_threads is permitted here; sandboxes can deny it. */
      thread_act_array_t list;
      mach_msg_type_number_t n;
      if (task_threads(mach_task_self(), &list, &n) != KERN_SUCCESS)
         return 0;
      {
         mach_msg_type_number_t i;
         for (i = 0; i < n; i++)
            mach_port_deallocate(mach_task_self(), list[i]);
      }
      vm_deallocate(mach_task_self(), (vm_address_t)list,
                    n * sizeof(thread_act_t));
   }
#else
   {
      struct sigaction sa;
      DIR *d;
      /* Only a real-time signal queues; a standard one sent twice to the
       * same thread is delivered once, and one barrier would wait
       * forever for the acknowledgement the other took. */
#if defined(SIGRTMIN) && defined(SIGRTMAX)
      if (signum < SIGRTMIN || signum > SIGRTMAX)
         return 0;
#else
      return 0;
#endif
      memset(&sa, 0, sizeof(sa));
      sa.sa_sigaction = pb_ack_handler;
      sa.sa_flags     = SA_SIGINFO | SA_RESTART;
      sigemptyset(&sa.sa_mask);
      if (sigaction(signum, &sa, NULL) != 0)
         return 0;
      /* And that /proc/self/task is readable; hidepid exempts self but
       * a hardened container may not. */
      d = opendir("/proc/self/task");
      if (!d)
         return 0;
      closedir(d);
      /* And that a queued signal reaches its handler with the counter:
       * one to this thread is delivered before the syscall returns. A
       * refusal (ENOSYS, EPERM) or a mask that blocks the signal here
       * shows up as no acknowledgement. */
      {
         int idx, gen;
         pid_t tid = (pid_t)syscall(SYS_gettid);
         retro_atomic_store_relaxed_int(&s_probe_acks, 0);
         idx = pb_slot_claim(&s_probe_acks, tid, 0, &gen);
         if (idx < 0)
            return 0;
         if (pb_send_ack(getpid(), tid, idx, gen) != 0)
         {
            retro_atomic_store_release_int(&s_slots[idx].state, 0);
            return 0;
         }
         /* Released whether or not it answered: the signal a blocking
          * mask holds back finds the slot free later and is ignored. */
         retro_atomic_store_release_int(&s_slots[idx].state, 0);
         if (retro_atomic_load_acquire_int(&s_probe_acks) != 1)
            return 0;
      }
   }
#endif
   return 1;
}

#if defined(PB_LINUX)
/* /proc/self/task/<tid><leaf>. Built by hand: snprintf is not C89, and
 * a tid is a small decimal. path must hold 64 bytes. */
static void pb_task_path(char *path, pid_t tid, const char *leaf)
{
   char    *q;
   char     digits[16];
   char    *d  = digits + sizeof(digits);
   unsigned v  = (unsigned)tid;
   strcpy(path, "/proc/self/task/");
   q = path + strlen(path);
   do { *--d = (char)('0' + v % 10u); v /= 10u; } while (v);
   while (d < digits + sizeof(digits))
      *q++ = *d++;
   strcpy(q, leaf);
}

/* Reads /proc/self/task/<tid>/stat. Returns 1 when the thread is on a
 * CPU or waiting for one, or when running is 0 and it merely exists.
 * *start gets its start time in clock ticks since boot, field 22, which
 * with the tid identifies the thread: a tid is reused as soon as its
 * thread is gone, and a later thread under the same number is not the
 * one that was signalled. */
static int pb_thread_stat(pid_t tid, int running, unsigned long *start)
{
   char path[64], buf[512], *p;
   int fd, field;
   ssize_t n;
   /* Field 3, the state, follows the comm in parentheses; the comm may
    * itself contain spaces or parentheses, so scan back from the last
    * ')' rather than forward, and count fields from there. */
   pb_task_path(path, tid, "/stat");
   fd = open(path, O_RDONLY);
   if (fd < 0)
      return 0;
   n = read(fd, buf, sizeof(buf) - 1);
   close(fd);
   if (n <= 0)
      return 0;
   buf[n] = '\0';
   p = strrchr(buf, ')');
   if (!p || p[1] != ' ')
      return 0;
   if (running && p[2] != 'R')
      return 0;
   /* p[1] is the space before field 3; step to the one before field 22,
    * the start time. */
   p++;
   for (field = 3; field < 22; field++)
   {
      p = strchr(p + 1, ' ');
      if (!p)
         return 0;
   }
   *start = strtoul(p + 1, NULL, 10);
   return 1;
}

/* Whether bit sig-1 is set in a /proc hex signal mask: most significant
 * digit first, as many digits as the kernel has signals (16 for 64, 32
 * on MIPS's 128), so it is read from the right without assuming a
 * width. */
static int pb_mask_has(const char *s, int sig)
{
   const char *end;
   unsigned    bit, digit;
   int         c;
   while (*s == ' ' || *s == '\t')
      s++;
   end = s;
   while ((*end >= '0' && *end <= '9') || (*end >= 'a' && *end <= 'f')
         || (*end >= 'A' && *end <= 'F'))
      end++;
   bit = (unsigned)(sig - 1);
   if (sig < 1 || (size_t)(bit / 4u) >= (size_t)(end - s))
      return 0;
   c     = end[-1 - (int)(bit / 4u)];
   digit = (c <= '9') ? (unsigned)(c - '0')
         : (unsigned)((c | 0x20) - 'a' + 10);
   return (int)((digit >> (bit % 4u)) & 1u);
}

/* 1 when the thread has the barrier's signal blocked, or is gone. Its
 * status file runs past a kilobyte and only the SigBlk line matters, so
 * it is read in small pieces and every other line is dropped unseen. A
 * kernel without the line -- none since 2.6 -- is taken as unblocked. */
static int pb_thread_blocks_signal(pid_t tid)
{
   char    path[64], buf[128], line[48];
   int     fd, len = 0, overlong = 0;
   ssize_t n;
   pb_task_path(path, tid, "/status");
   fd = open(path, O_RDONLY);
   if (fd < 0)
      return 1;   /* exited, and fenced by exiting */
   while ((n = read(fd, buf, sizeof(buf))) > 0)
   {
      ssize_t i;
      for (i = 0; i < n; i++)
      {
         if (buf[i] != '\n')
         {
            if (len < (int)sizeof(line) - 1)
               line[len++] = buf[i];
            else
               overlong = 1;
            continue;
         }
         line[len] = '\0';
         if (!overlong && !strncmp(line, "SigBlk:", 7))
         {
            close(fd);
            return pb_mask_has(line + 7, s_signum);
         }
         len      = 0;
         overlong = 0;
      }
   }
   close(fd);
   return 0;
}

/* Walks claim slots in batches of this many. */
#define PB_ACK_BATCH 32

/* How long the walker sleeps before checking whether a target it is
 * still owed has exited or blocked the signal. A thread that exits with
 * the signal queued never runs the handler, and nothing else would wake
 * the walker; every other acknowledgement wakes it at once. The first
 * sleep is short and each one after it twice as long, up to the
 * ceiling: a walk that caught a thread in its last microseconds is over
 * in a fraction of a millisecond, and one owed by a thread that is
 * merely slow to be scheduled costs a syscall every 10 ms rather than a
 * spin. */
#define PB_ACK_RECHECK_FIRST_NS 250000L
#define PB_ACK_RECHECK_MAX_NS   10000000L

/* Wait until every slot in the batch is closed. The walker closes a slot
 * itself when its thread is gone -- or is a thread started later under
 * the same tid -- since an exited thread was fenced by exiting, and when
 * its thread has blocked the signal since the walk looked, since that
 * thread is not one this barrier is for (see the header) and will not
 * answer. */
static void pb_ack_wait(const int *idx, const int *gen, int n,
      retro_atomic_int_t *acks)
{
   long wait_ns = PB_ACK_RECHECK_FIRST_NS;
   for (;;)
   {
      int got = retro_atomic_load_acquire_int(acks);
      int i;
      if (got >= n)
         return;
#if defined(PB_FUTEX_WAIT_PRIVATE)
      {
         struct timespec ts;
         ts.tv_sec  = 0;
         ts.tv_nsec = wait_ns;
         /* Returns at once if an acknowledgement landed since the load. */
         if (syscall(__NR_futex, (void*)acks, PB_FUTEX_WAIT_PRIVATE, got,
                  &ts, NULL, 0) == 0 || errno != ETIMEDOUT)
            continue;
         if (wait_ns < PB_ACK_RECHECK_MAX_NS)
            wait_ns *= 2;
      }
#endif
      for (i = 0; i < n; i++)
      {
         pb_ack_slot_t *slot = &s_slots[idx[i]];
         unsigned long  start;
         if (retro_atomic_load_acquire_int(&slot->state) != PB_SLOT_OPEN(gen[i]))
            continue;
         if (       pb_thread_stat(slot->tid, 0, &start)
               &&   start == slot->start
               &&  !pb_thread_blocks_signal(slot->tid))
            continue;
         if (retro_atomic_cas_int(&slot->state, PB_SLOT_OPEN(gen[i]),
                  PB_SLOT_DONE(gen[i])))
            retro_atomic_fetch_add_int(acks, 1);
      }
   }
}

static void pb_signal_barrier(void)
{
   DIR *d;
   struct dirent *e;
   int   idx[PB_ACK_BATCH], gen[PB_ACK_BATCH];
   /* This walk's count, on its stack: a handler reaches it through a
    * slot, and every slot pointing at it is closed before the batch is
    * left and released before the walk returns. */
   retro_atomic_int_t acks;
   pid_t self = (pid_t)syscall(SYS_gettid);
   pid_t pid  = getpid();

   d = opendir("/proc/self/task");
   if (!d)
      return;
   PB_TSAN_WALK_LOCK();
   do
   {
      int n = 0, i;
      /* A store, not atomic_init: the handlers that update it run on
       * other threads, and the only edge between this and them is the
       * syscall that queues their signal. */
      retro_atomic_store_release_int(&acks, 0);
      while (n < PB_ACK_BATCH && (e = readdir(d)) != NULL)
      {
         unsigned long start;
         pid_t tid = (pid_t)atoi(e->d_name);
         if (tid <= 0 || tid == self)
            continue;
         /* Only threads on a CPU need interrupting; a thread that was
          * switched out was drained by the switch. 'R' is runnable rather
          * than strictly running, so under overcommit this can also wait
          * for a preempted thread to be rescheduled -- bounded by a
          * scheduler quantum, and acceptable on a path about to sleep. */
         if (!pb_thread_stat(tid, 1, &start))
            continue;
         /* A thread with the signal blocked would never acknowledge it.
          * It is one this process did not create for its own protocols
          * -- a GL or Vulkan driver's workers block every signal -- and
          * it touches no eventcount, so it has nothing this barrier
          * needs drained. The header states that contract. */
         if (pb_thread_blocks_signal(tid))
            continue;
         /* Every slot taken means other walks are mid-batch; theirs
          * close as their targets answer, so wait for one to free. */
         while ((idx[n] = pb_slot_claim(&acks, tid, start, &gen[n])) < 0)
            sched_yield();
         /* EAGAIN is a full queue, which drains as its signals are
          * handled; ESRCH is a thread that has exited, which fences it. */
         for (;;)
         {
            if (pb_send_ack(pid, tid, idx[n], gen[n]) == 0)
            {
               n++;
               break;
            }
            if (errno != EAGAIN)
            {
               retro_atomic_store_release_int(&s_slots[idx[n]].state, 0);
               break;
            }
         }
      }
      pb_ack_wait(idx, gen, n, &acks);
      for (i = 0; i < n; i++)
         retro_atomic_store_release_int(&s_slots[idx[i]].state, 0);
   } while (e != NULL);
   PB_TSAN_WALK_UNLOCK();
   closedir(d);
}
#endif

#if defined(PB_DARWIN)
/* Darwin does not deliver a signal here at all. thread_get_state on
 * another thread's Mach port is a synchronous barrier: XNU holds the
 * target, waits in thread_stop until it is off every CPU -- and a
 * context switch drains its store buffer -- and releases it again
 * before the call returns. No pthread_t is needed, and every call used
 * here is in the 10.4 libSystem.
 *
 * The hold is released inside the kernel call. That is what makes
 * concurrent callers safe: a thread only parks on a hold when it
 * returns to user mode, so two callers stopping each other each find
 * the other off its CPU, finish, and release, and neither is ever left
 * parked waiting for the other to run.
 *
 * KERN_ABORTED means the wait in thread_stop was interrupted before the
 * target stopped, so that thread has not been fenced; try it again. Any
 * other failure is a thread that has already exited. */
static void pb_signal_barrier(void)
{
   thread_act_array_t list;
   mach_msg_type_number_t n, i;
   thread_t self = mach_thread_self();

   if (task_threads(mach_task_self(), &list, &n) != KERN_SUCCESS)
   {
      mach_port_deallocate(mach_task_self(), self);
      return;
   }
   for (i = 0; i < n; i++)
   {
      thread_basic_info_data_t info;
      mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;

      if (list[i] == self)
         continue;
      if (thread_info(list[i], THREAD_BASIC_INFO,
               (thread_info_t)&info, &count) != KERN_SUCCESS)
         continue;
      /* Only a thread on a CPU has anything to drain; one that is
       * already off was drained by the switch that took it off. */
      if (info.run_state != TH_STATE_RUNNING)
         continue;
#if !defined(PB_DARWIN_NO_STATE)
      for (;;)
      {
         pb_darwin_state_t      state;
         mach_msg_type_number_t cnt = PB_DARWIN_COUNT;
         if (thread_get_state(list[i], PB_DARWIN_FLAVOR,
                  (thread_state_t)&state, &cnt) != KERN_ABORTED)
            break;
      }
#endif
   }
   for (i = 0; i < n; i++)
      mach_port_deallocate(mach_task_self(), list[i]);
   vm_deallocate(mach_task_self(), (vm_address_t)list,
                 n * sizeof(thread_act_t));
   mach_port_deallocate(mach_task_self(), self);
}
#endif

#endif /* PB_LINUX || PB_DARWIN */

/* ------------------------------------------------------------------ */
/* Resolution                                                          */
/* ------------------------------------------------------------------ */

static int pb_default_signum(void)
{
#if defined(SIGRTMAX)
   return SIGRTMAX - 1;
#elif defined(SIGUSR2)
   return SIGUSR2;
#else
   return 0;
#endif
}

enum retro_procbarrier_tier retro_procbarrier_init(int signum)
{
   enum retro_procbarrier_tier t;

   {
      int cur = retro_atomic_load_acquire_int(&s_tier);
      if (cur == PB_RESOLVED_NONE)
         return RETRO_PROCBARRIER_NONE;
      if (cur != RETRO_PROCBARRIER_NONE)
         return (enum retro_procbarrier_tier)cur;
   }

   s_signum = signum ? signum : pb_default_signum();

   /* RETRO_PROCBARRIER=<tier name> forces a tier for testing, the way
    * RTHREADS_SCOND does for the condvar. A forced tier still has to be
    * available: asking for membarrier on a 4.4 kernel gets the normal
    * resolution, not a broken barrier. */
   {
      const char *force = getenv("RETRO_PROCBARRIER");
      if (force && *force)
      {
#if defined(PB_LINUX) || defined(PB_FREEBSD_MEMBARRIER)
         if (!strcmp(force, "membarrier") && pb_membarrier_try())
         { t = RETRO_PROCBARRIER_MEMBARRIER; goto done; }
#endif
#if defined(PB_WINDOWS)
         if (!strcmp(force, "fpwb") && pb_fpwb_try())
         { t = RETRO_PROCBARRIER_FLUSHWRITEBUFFERS; goto done; }
#endif
#if defined(PB_HAVE_PAGEFLIP)
         if (!strcmp(force, "pageflip") && pb_pageflip_try())
         { t = RETRO_PROCBARRIER_PAGEFLIP; goto done; }
#endif
#if defined(PB_LINUX) || defined(PB_DARWIN)
         if (!strcmp(force, "signal") && s_signum && pb_signal_try(s_signum))
         { t = RETRO_PROCBARRIER_SIGNAL; goto done; }
#endif
      }
   }

   /* One CPU: nothing to fence against. Every tier below already
    * short-circuits this in the kernel, but they pay a ring transition
    * to find out. */
   if (pb_num_cpus() <= 1)
   {
      t = RETRO_PROCBARRIER_UNIPROCESSOR;
      goto done;
   }

#if defined(PB_LINUX) || defined(PB_FREEBSD_MEMBARRIER)
   if (pb_membarrier_try())
   {
      t = RETRO_PROCBARRIER_MEMBARRIER;
      goto done;
   }
#endif

#if defined(PB_WINDOWS)
   if (pb_fpwb_try())
   {
      t = RETRO_PROCBARRIER_FLUSHWRITEBUFFERS;
      goto done;
   }
#endif

#if defined(PB_HAVE_PAGEFLIP)
   /* x86 only, by the #if: the shootdown is not an IPI elsewhere. On
    * Windows this is the whole pre-Vista story. */
   if (pb_pageflip_try())
   {
      t = RETRO_PROCBARRIER_PAGEFLIP;
      goto done;
   }
#endif

#if (defined(PB_LINUX) || defined(PB_DARWIN))
   if (s_signum && pb_signal_try(s_signum))
   {
      t = RETRO_PROCBARRIER_SIGNAL;
      goto done;
   }
#endif

   t = RETRO_PROCBARRIER_NONE;

done:
   /* Cache the outcome either way. A platform with nothing is recorded
    * as such so the probe runs once, not once per call. */
   retro_atomic_store_release_int(&s_tier,
         t == RETRO_PROCBARRIER_NONE ? PB_RESOLVED_NONE : (int)t);
   return t;
}

enum retro_procbarrier_tier retro_procbarrier_tier(void)
{
   int cur = retro_atomic_load_acquire_int(&s_tier);
   return cur == PB_RESOLVED_NONE ? RETRO_PROCBARRIER_NONE
                                  : (enum retro_procbarrier_tier)cur;
}

const char *retro_procbarrier_tier_name(enum retro_procbarrier_tier tier)
{
   switch (tier)
   {
      case RETRO_PROCBARRIER_UNIPROCESSOR:      return "uniprocessor";
      case RETRO_PROCBARRIER_MEMBARRIER:        return "membarrier";
      case RETRO_PROCBARRIER_FLUSHWRITEBUFFERS: return "FlushProcessWriteBuffers";
      case RETRO_PROCBARRIER_PAGEFLIP:          return "page-flip";
      case RETRO_PROCBARRIER_SIGNAL:            return "signal";
      default:                                  return "none";
   }
}

int retro_procbarrier(void)
{
   enum retro_procbarrier_tier t;

   {
      int cur = retro_atomic_load_acquire_int(&s_tier);
      if (cur == PB_RESOLVED_NONE)
         return 0;                       /* probed already; nothing here */
      t = (cur == RETRO_PROCBARRIER_NONE)
            ? retro_procbarrier_init(0)  /* first use without init */
            : (enum retro_procbarrier_tier)cur;
   }

   switch (t)
   {
      case RETRO_PROCBARRIER_UNIPROCESSOR:
         retro_atomic_thread_fence_seq_cst();
         return 1;

#if defined(PB_LINUX) || defined(PB_FREEBSD_MEMBARRIER)
      case RETRO_PROCBARRIER_MEMBARRIER:
         pb_membarrier();
         return 1;
#endif

#if defined(PB_WINDOWS)
      case RETRO_PROCBARRIER_FLUSHWRITEBUFFERS:
         s_fpwb();
         return 1;
#endif

#if defined(PB_HAVE_PAGEFLIP)
      case RETRO_PROCBARRIER_PAGEFLIP:
         pb_pageflip();
         return 1;
#endif

#if defined(PB_LINUX) || defined(PB_DARWIN)
      case RETRO_PROCBARRIER_SIGNAL:
         pb_signal_barrier();
         return 1;
#endif

      default:
         return 0;
   }
}
