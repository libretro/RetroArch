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
 *   Darwin signal      cause_ast_check: i386_signal_cpu(MP_AST) on x86,
 *                      cpu_signal(SIGPast) on arm64 and on ppc (xnu-1228),
 *                      the ppc path bracketed by sync; isync.
 *   Linux signal       Signal delivery to a running thread goes through
 *                      kick_process(), which is an IPI.
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

#if defined(PB_LINUX) || defined(PB_FREEBSD_MEMBARRIER) || defined(PB_DARWIN)
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/mman.h>
#include <pthread.h>
#endif

#if defined(PB_LINUX)
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
#if defined(PB_WINDOWS)
static void     *s_page;
static SIZE_T    s_page_size;
#else
static void     *s_page;
static size_t    s_page_size;
#endif
#endif

#if defined(PB_LINUX)
static retro_atomic_int_t s_acks;
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

static int pb_membarrier_try(void)
{
#if defined(PB_NO_MEMBARRIER)
   return 0;
#else
   long q = syscall(__NR_membarrier, PB_MEMBARRIER_CMD_QUERY, 0, 0);
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
static int pb_pageflip_try(void)
{
#if defined(PB_WINDOWS)
   SYSTEM_INFO si;
   GetSystemInfo(&si);
   s_page_size = si.dwPageSize;
   s_page      = VirtualAlloc(NULL, s_page_size, MEM_COMMIT | MEM_RESERVE,
                              PAGE_READWRITE);
   if (!s_page)
      return 0;
   /* A page that is paged out has no TLB entry to shoot down. */
   VirtualLock(s_page, s_page_size);
   return 1;
#else
   long ps = sysconf(_SC_PAGESIZE);
   s_page_size = (ps > 0) ? (size_t)ps : 4096u;
   s_page = mmap(NULL, s_page_size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
   if (s_page == MAP_FAILED)
   {
      s_page = NULL;
      return 0;
   }
   mlock(s_page, s_page_size);
   return 1;
#endif
}

static void pb_pageflip(void)
{
#if defined(PB_WINDOWS)
   DWORD old;
   VirtualProtect(s_page, s_page_size, PAGE_READWRITE, &old);
   *(volatile char*)s_page = 0;
   VirtualProtect(s_page, s_page_size, PAGE_NOACCESS, &old);
#else
   mprotect(s_page, s_page_size, PROT_READ | PROT_WRITE);
   *(volatile char*)s_page = 0;
   mprotect(s_page, s_page_size, PROT_NONE);
#endif
}
#endif

/* ------------------------------------------------------------------ */
/* Tier: signal                                                        */
/* ------------------------------------------------------------------ */

#if defined(PB_LINUX) || defined(PB_DARWIN)

#if defined(PB_LINUX)
static void pb_ack_handler(int sig)
{
   (void)sig;
   /* Async-signal-safe: one atomic increment. The release pairs with the
    * acquire in the waiter, so the interrupted thread's earlier stores
    * are ordered before its acknowledgement. */
   retro_atomic_fetch_add_int(&s_acks, 1);
}
#endif

static int pb_signal_try(int signum)
{
#if defined(PB_DARWIN)
   (void)signum;   /* no signal on Darwin; see pb_signal_barrier */
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
      memset(&sa, 0, sizeof(sa));
      sa.sa_handler = pb_ack_handler;
      sa.sa_flags   = SA_RESTART;
      sigemptyset(&sa.sa_mask);
      if (sigaction(signum, &sa, NULL) != 0)
         return 0;
      /* And that /proc/self/task is readable; hidepid exempts self but
       * a hardened container may not. */
      d = opendir("/proc/self/task");
      if (!d)
         return 0;
      closedir(d);
   }
#endif
   return 1;
}

#if defined(PB_LINUX)
static int pb_thread_running(pid_t tid)
{
   char path[64], buf[256], *p, *q;
   int fd;
   ssize_t n;
   unsigned v;
   /* /proc/self/task/<tid>/stat: field 3 is the state, after the comm
    * in parentheses; the comm may itself contain spaces or parentheses,
    * so scan back from the last ')' rather than forward. The path is
    * built by hand: snprintf is not C89, and a tid is a small decimal. */
   strcpy(path, "/proc/self/task/");
   q = path + strlen(path);
   {
      char digits[16];
      int  nd = 0;
      v = (unsigned)tid;
      do { digits[nd++] = (char)('0' + v % 10u); v /= 10u; } while (v);
      while (nd) *q++ = digits[--nd];
   }
   strcpy(q, "/stat");
   fd = open(path, O_RDONLY);
   if (fd < 0)
      return 0;
   n = read(fd, buf, sizeof(buf) - 1);
   close(fd);
   if (n <= 0)
      return 0;
   buf[n] = '\0';
   p = strrchr(buf, ')');
   return p && p[1] == ' ' && p[2] == 'R';
}

static void pb_signal_barrier(void)
{
   DIR *d;
   struct dirent *e;
   pid_t self = (pid_t)syscall(SYS_gettid);
   pid_t pid  = getpid();
   int sent   = 0;

   retro_atomic_store_relaxed_int(&s_acks, 0);
   d = opendir("/proc/self/task");
   if (!d)
      return;
   while ((e = readdir(d)) != NULL)
   {
      pid_t tid = (pid_t)atoi(e->d_name);
      if (tid <= 0 || tid == self)
         continue;
      /* Only threads on a CPU need interrupting; a thread that was
       * switched out was drained by the switch. 'R' is runnable rather
       * than strictly running, so under overcommit this can also wait
       * for a preempted thread to be rescheduled -- bounded by a
       * scheduler quantum, and acceptable on a path about to sleep. */
      if (!pb_thread_running(tid))
         continue;
      if (syscall(SYS_tgkill, pid, tid, s_signum) == 0)
         sent++;
   }
   closedir(d);
   while (retro_atomic_load_acquire_int(&s_acks) < sent)
      ;
}
#endif

#if defined(PB_DARWIN)
/* Darwin does not deliver the signal here at all. thread_suspend on a
 * Mach port is already a synchronous barrier: XNU's thread_wait loops
 * while the target's state has TH_RUN, and blocks while it is the active
 * thread on any processor, so thread_suspend does not return until the
 * target has been switched off its CPU -- and a context switch drains
 * its store buffer. Checked in the current tree and in xnu-1228, the
 * last PowerPC kernel. thread_resume then puts it back.
 *
 * This is why no pthread_t is needed. The earlier version converted
 * each port with pthread_from_mach_thread_np and sent a signal, and that
 * function is 10.6 and later; RetroArch's PowerPC floor is 10.4. The
 * function itself is a walk of libpthread's private thread list under
 * its private lock, so there is nothing to reimplement against an older
 * libpthread that would not be a layout guess.
 *
 * Nothing is called between the suspend and the resume. A suspended
 * thread may hold any lock, including malloc's, and the window has to
 * be one in which this thread needs none of them. */
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
      if (thread_suspend(list[i]) == KERN_SUCCESS)
         thread_resume(list[i]);
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
