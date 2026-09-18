/* Copyright  (C) 2010-2024 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (faulthandler.c).
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1   /* REG_RIP in <ucontext.h> */
#endif

#include <string.h>
#include <faulthandler.h>
#include <retro_atomic.h>
#include <memmap.h>
#include <rthreads/rthreads.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#if !defined(__APPLE__)
#include <ucontext.h>
#endif
#if defined(__APPLE__)
#include <sys/ucontext.h>
#include <TargetConditionals.h>
/* tvOS marks the mach task API unavailable, and the SDK does not ship
 * the headers for it. Nothing below needs them there. */
#if !defined(TARGET_OS_TV) || !TARGET_OS_TV
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/task.h>
#endif
#endif
#endif

/* The state, and why each piece is shaped as it is:
 *
 * - callback: a single pointer, released by install (under the
 *   registration lock) and acquire-loaded by the filter. Remove runs
 *   only with the faulting threads quiesced, which is the lifecycle
 *   guarantee that makes the plain pointer swap safe.
 * - thread slots: threads that run faulting code register an identity
 *   here and the filter linear-scans it. A fault on an unregistered
 *   thread is not ours and chains to the old handler. The identity is
 *   a uintptr: the thread id on Windows, pthread_self elsewhere, which
 *   is scalar on every supported libc.
 *   NOT a thread-local: a core is usually a shared object, so its
 *   thread-locals are global-dynamic and __tls_get_addr may allocate
 *   on a thread's first access -- inside a signal handler, at exactly
 *   the wrong moment.
 * - per-slot in-handler flag: a recursion guard, so a fault inside the
 *   callback (a genuine crash) chains out for a core dump instead of
 *   looping.
 */
/* A function pointer, not an atomic void*: C89 forbids the conversion
 * between the two, and a JIT core compiles under that lane. The
 * publish/consume pair is an int flag beside it, so the filter reads a
 * pointer only after an acquire load says it was fully stored. */
static retro_fault_handler_t s_callback;
static retro_atomic_int_t    s_callback_live;
static retro_atomic_int_t s_slot_claimed[RETRO_FAULT_MAX_THREADS];
static retro_atomic_ptr_t s_slot_id[RETRO_FAULT_MAX_THREADS];
static retro_atomic_int_t s_slot_inhandler[RETRO_FAULT_MAX_THREADS];

#if defined(_WIN32)
static void *s_veh_handle;
#else
#if defined(__APPLE__) || defined(__aarch64__)
static struct sigaction s_old_sigbus;
#endif
#if !defined(__APPLE__) || defined(__aarch64__)
static struct sigaction s_old_sigsegv;
#endif
#endif

/* Registration and removal are cold and may block; the filter itself
 * takes no lock. A plain static slock, created once. */
static slock_t *s_reg_lock;
static retro_atomic_int_t s_reg_lock_init;

static slock_t *fault_reg_lock(void)
{
   /* Callers are cold-path and serialised by the VM lifecycle; the
    * double-check is for the first two ever racing. */
   if (!retro_atomic_load_acquire_int(&s_reg_lock_init))
   {
      slock_t *l = slock_new();
      if (!l)
         return NULL;
      if (retro_atomic_cas_int(&s_reg_lock_init, 0, 1))
      {
         s_reg_lock = l;
         retro_atomic_store_release_int(&s_reg_lock_init, 2);
      }
      else
      {
         slock_free(l);
         while (retro_atomic_load_acquire_int(&s_reg_lock_init) != 2) { }
      }
   }
   while (retro_atomic_load_acquire_int(&s_reg_lock_init) != 2) { }
   return s_reg_lock;
}

static uintptr_t fault_thread_identity(void)
{
#if defined(_WIN32)
   return (uintptr_t)GetCurrentThreadId();
#else
   return (uintptr_t)pthread_self();
#endif
}

/* Async-signal-safe: a linear scan of at most RETRO_FAULT_MAX_THREADS
 * acquire loads, no allocation, no lock. */
static int fault_slot_lookup(uintptr_t self)
{
   int i;
   for (i = 0; i < RETRO_FAULT_MAX_THREADS; i++)
      if ((uintptr_t)retro_atomic_load_acquire_ptr(&s_slot_id[i]) == self)
         return i;
   return -1;
}

bool retro_faulthandler_register_thread(void)
{
   uintptr_t self = fault_thread_identity();
   int i;
   for (i = 0; i < RETRO_FAULT_MAX_THREADS; i++)
      if ((uintptr_t)retro_atomic_load_acquire_ptr(&s_slot_id[i]) == self)
         return true;   /* already registered */
   for (i = 0; i < RETRO_FAULT_MAX_THREADS; i++)
   {
      if (retro_atomic_cas_int(&s_slot_claimed[i], 0, 1))
      {
         retro_atomic_store_release_int(&s_slot_inhandler[i], 0);
         retro_atomic_store_release_ptr(&s_slot_id[i], (void*)self);
         return true;
      }
   }
   return false;
}

void retro_faulthandler_unregister_thread(void)
{
   uintptr_t self = fault_thread_identity();
   int i;
   for (i = 0; i < RETRO_FAULT_MAX_THREADS; i++)
   {
      if ((uintptr_t)retro_atomic_load_acquire_ptr(&s_slot_id[i]) == self)
      {
         /* Clear the identity before the claim: the filter matches on
          * the identity, so after this store no fault finds this slot,
          * and only then may another thread take it. */
         retro_atomic_store_release_ptr(&s_slot_id[i], (void*)0);
         retro_atomic_store_release_int(&s_slot_inhandler[i], 0);
         retro_atomic_store_release_int(&s_slot_claimed[i], 0);
         return;
      }
   }
}

#if defined(_WIN32)

static LONG __stdcall fault_veh(EXCEPTION_POINTERS *eps)
{
   int slot = fault_slot_lookup(fault_thread_identity());
   if (slot >= 0 && !retro_atomic_exchange_int(&s_slot_inhandler[slot], 1))
   {
      bool handled = false;
      if (eps->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
      {
         retro_fault_info_t info;
         retro_fault_handler_t cb;
#if defined(_M_AMD64) || defined(__x86_64__)
         info.pc   = (uintptr_t)eps->ContextRecord->Rip;
#elif defined(_M_ARM64) || defined(__aarch64__)
         info.pc   = (uintptr_t)eps->ContextRecord->Pc;
#else
         info.pc   = 0;
#endif
         info.addr = (uintptr_t)eps->ExceptionRecord->ExceptionInformation[1];
         cb        = retro_atomic_load_acquire_int(&s_callback_live) ? s_callback : NULL;
         handled   = cb ? cb(&info) : false;
      }
      /* Balance the recursion claim on every exit path. */
      retro_atomic_store_release_int(&s_slot_inhandler[slot], 0);
      if (handled)
         return EXCEPTION_CONTINUE_EXECUTION;
   }
   return EXCEPTION_CONTINUE_SEARCH;
}

#else

/* Chain to whatever handled this signal before us. */
static void fault_chain(int sig, siginfo_t *si, void *ctx)
{
#if defined(__APPLE__) || defined(__aarch64__)
#if !defined(__APPLE__) || defined(__aarch64__)
   const struct sigaction *sa = (sig == SIGBUS) ? &s_old_sigbus : &s_old_sigsegv;
#else
   const struct sigaction *sa = &s_old_sigbus;
#endif
#else
   const struct sigaction *sa = &s_old_sigsegv;
#endif
   if ((sa->sa_flags & SA_SIGINFO) && sa->sa_sigaction)
      sa->sa_sigaction(sig, si, ctx);
   else if (sa->sa_handler == SIG_DFL)
   {
      /* Restore the default and return: the instruction re-executes
       * and faults again, this time fatally, with the register state
       * a debugger and a core dump want. */
      signal(sig, SIG_DFL);
   }
   else if (sa->sa_handler != SIG_IGN && sa->sa_handler)
      sa->sa_handler(sig);
}

static void fault_filter(int sig, siginfo_t *si, void *ctx)
{
   retro_fault_info_t info;
   retro_fault_handler_t cb;
   void *pc;
   bool handled;
   size_t page = mempagesize();
   int slot    = fault_slot_lookup(fault_thread_identity());

   if (slot < 0)
   {
      fault_chain(sig, si, ctx);
      return;
   }
   /* A fault inside our own callback is a genuine crash; chain out
    * rather than loop. */
   if (retro_atomic_exchange_int(&s_slot_inhandler[slot], 1))
   {
      fault_chain(sig, si, ctx);
      return;
   }

   /* No stdio, no allocation, nothing that takes a lock, from here. */
#if defined(__APPLE__) && defined(__x86_64__)
   pc = (void*)((ucontext_t*)ctx)->uc_mcontext->__ss.__rip;
#elif defined(__APPLE__) && defined(__aarch64__)
   pc = (void*)((ucontext_t*)ctx)->uc_mcontext->__ss.__pc;
#elif defined(__FreeBSD__) && defined(__x86_64__)
   pc = (void*)((ucontext_t*)ctx)->uc_mcontext.mc_rip;
#elif defined(__x86_64__)
   pc = (void*)((ucontext_t*)ctx)->uc_mcontext.gregs[REG_RIP];
#elif defined(__aarch64__)
   pc = (void*)((ucontext_t*)ctx)->uc_mcontext.pc;
#else
   pc = NULL;
#endif
   info.pc   = (uintptr_t)pc;
   info.addr = (uintptr_t)si->si_addr & ~(uintptr_t)(page - 1);
   cb        = retro_atomic_load_acquire_int(&s_callback_live) ? s_callback : NULL;
   handled   = cb ? cb(&info) : false;

   retro_atomic_store_release_int(&s_slot_inhandler[slot], 0);
   if (handled)
      return;   /* re-executes the faulting instruction */
   fault_chain(sig, si, ctx);
}

#endif

bool retro_faulthandler_install(retro_fault_handler_t handler)
{
   slock_t *lock = fault_reg_lock();
   bool ok       = true;
   if (!handler)
      return false;
   if (lock)
      slock_lock(lock);
   if (retro_atomic_load_acquire_int(&s_callback_live))
   {
      if (lock)
         slock_unlock(lock);
      return false;   /* one at a time */
   }
#if defined(_WIN32)
   if (!s_veh_handle)
   {
      s_veh_handle = AddVectoredExceptionHandler(TRUE, fault_veh);
      if (!s_veh_handle)
         ok = false;
   }
#else
   {
      struct sigaction sa;
      memset(&sa, 0, sizeof(sa));
      sigemptyset(&sa.sa_mask);
      sa.sa_flags     = SA_SIGINFO;
      sa.sa_sigaction = fault_filter;
#if defined(__linux__)
      /* Do not block the signal inside the handler: chaining to the
       * old one must be able to raise it again. */
      sa.sa_flags    |= SA_NODEFER;
#endif
#if defined(__APPLE__) || defined(__aarch64__)
      /* Darwin reports a permission violation as SIGBUS, and so does
       * ARM64. */
      if (sigaction(SIGBUS, &sa, &s_old_sigbus) != 0)
         ok = false;
#endif
#if !defined(__APPLE__) || defined(__aarch64__)
      if (ok && sigaction(SIGSEGV, &sa, &s_old_sigsegv) != 0)
         ok = false;
#endif
/* Not && !TARGET_OS_TV: on a platform that never includes
 * TargetConditionals.h the name is undefined, and an undefined name in
 * a preprocessor expression is a warning under -Wundef and a silent
 * zero otherwise. Ask whether it is defined first. */
#if defined(__APPLE__) && defined(__aarch64__) \
 && (!defined(TARGET_OS_TV) || !TARGET_OS_TV)
      /* Keeps a debugger out of an EXC_BAD_ACCESS loop when the fault
       * is one we are going to handle; tvOS has neither the API nor a
       * debugger to keep out. */
      if (ok)
         task_set_exception_ports(mach_task_self(), EXC_MASK_BAD_ACCESS,
               MACH_PORT_NULL, EXCEPTION_DEFAULT, 0);
#endif
   }
#endif
   if (ok)
   {
      s_callback = handler;
      retro_atomic_store_release_int(&s_callback_live, 1);
   }
   if (lock)
      slock_unlock(lock);
   return ok;
}

void retro_faulthandler_remove(retro_fault_handler_t handler)
{
   slock_t *lock = fault_reg_lock();
   if (lock)
      slock_lock(lock);
   if (!retro_atomic_load_acquire_int(&s_callback_live) || s_callback != handler)
   {
      if (lock)
         slock_unlock(lock);
      return;
   }
   retro_atomic_store_release_int(&s_callback_live, 0);
   s_callback = NULL;
#if defined(_WIN32)
   if (s_veh_handle)
   {
      RemoveVectoredExceptionHandler(s_veh_handle);
      s_veh_handle = NULL;
   }
#else
   {
      struct sigaction sa;
#if defined(__APPLE__) || defined(__aarch64__)
      sigaction(SIGBUS, &s_old_sigbus, &sa);
#endif
#if !defined(__APPLE__) || defined(__aarch64__)
      sigaction(SIGSEGV, &s_old_sigsegv, &sa);
#endif
   }
#endif
   if (lock)
      slock_unlock(lock);
}
