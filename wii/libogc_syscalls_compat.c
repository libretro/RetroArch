/* Internal-libogc syscall bridge for modern devkitPPC newlibs.
 *
 * The in-tree prebuilt libogc installs its heap, lock, exit and
 * time-of-day handlers into newlib's __syscalls dispatch table at
 * SYS_Init (system.c), and the devkitPPC libsysbase of its era routed
 * everything through that table.  devkitPPC r38 and later removed the
 * table: libsysbase dispatches through per-symbol weak __syscall_*
 * hooks instead, and later releases dropped the sbrk hook as well,
 * leaving a weak _sbrk_r that places the heap by linker symbols
 * instead of libogc's MEM1 arena, colliding with SYS_GetArena1Lo
 * allocations.
 *
 * This file defines the table the prebuilt SYS_Init writes into, a
 * strong _sbrk_r and strong __syscall_* hooks that forward to it, so
 * the modern libsysbase routes exactly where the old one did.  The
 * table layout is fixed by the prebuilt binary, not by any header we
 * control: SYS_Init stores its nine handlers at offsets 0..32 in
 * declaration order (verified against the archive's relocations), so
 * the slots are mirrored here, with the old libsysbase's pre-SYS_Init
 * behaviour kept for NULL slots.  Compiled only for EXTERNAL_LIBOGC=0
 * GameCube/Wii builds; external libogc pairs with its own toolchain's
 * libsysbase and never sees this file. */

#ifdef INTERNAL_LIBOGC

#include <stddef.h>
#include <errno.h>
#include <ctype.h>
#include <reent.h>
#include <sys/time.h>

/* From the prebuilt libogc; its headers are not C89. */
unsigned int IRQ_Disable(void);
void IRQ_Restore(unsigned int level);
int LWP_MutexTryLock(unsigned int mutex);

/* Slot layout written by the prebuilt SYS_Init.  Offsets are
 * load-bearing; the trailing reserve absorbs whatever the era's
 * iosupport.h declared beyond the slots libogc touches. */
struct __syscalls_compat
{
   void *(*sbrk_r)(struct _reent *r, ptrdiff_t incr);   /* +0  */
   int   (*lock_init)(int *lock, int recursive);        /* +4  */
   int   (*lock_close)(int *lock);                      /* +8  */
   int   (*lock_release)(int *lock);                    /* +12 */
   int   (*lock_acquire)(int *lock);                    /* +16 */
   void  (*malloc_lock)(struct _reent *r);              /* +20 */
   void  (*malloc_unlock)(struct _reent *r);            /* +24 */
   void  (*exit)(int rc);                               /* +28 */
   int   (*gettod_r)(struct _reent *r,
                     struct timeval *tv, struct timezone *tz); /* +32 */
   void *reserved[23];
};

struct __syscalls_compat __syscalls;

void *_sbrk_r(struct _reent *r, ptrdiff_t incr)
{
   if (__syscalls.sbrk_r)
      return __syscalls.sbrk_r(r, incr);
   r->_errno = ENOMEM;
   return (void*)-1;
}

/* Before SYS_Init there is one context and no lwp threads, so a
 * missing lock handler is a safe no-op, exactly as the old dispatch
 * behaved with a zeroed table.  A lock word holds the lwp mutex libogc
 * made for it, 0 until then: the r38 libsysbase creates it through
 * lock_init, later ones zero-initialise locks and never call it, so
 * the first acquire makes it.  A recursive lock's word is the first
 * member of its struct. */
int __syscall_lock_init(int *lock, int recursive)
{
   if (__syscalls.lock_init)
      return __syscalls.lock_init(lock, recursive);
   return 0;
}

static int compat_lock_word(int *lock, int recursive)
{
   if (!*lock && __syscalls.lock_init)
   {
      unsigned int level = IRQ_Disable();
      if (!*lock)
         __syscalls.lock_init(lock, recursive);
      IRQ_Restore(level);
   }
   return *lock;
}

static void compat_lock_acquire(int *lock, int recursive)
{
   if (compat_lock_word(lock, recursive) && __syscalls.lock_acquire)
      __syscalls.lock_acquire(lock);
}

static int compat_lock_try_acquire(int *lock, int recursive)
{
   if (!compat_lock_word(lock, recursive))
      return 0;
   return LWP_MutexTryLock((unsigned int)*lock) ? 1 : 0;
}

static void compat_lock_release(int *lock)
{
   if (*lock && __syscalls.lock_release)
      __syscalls.lock_release(lock);
}

static void compat_lock_close(int *lock)
{
   if (*lock && __syscalls.lock_close)
      __syscalls.lock_close(lock);
}

void __syscall_lock_acquire(int *lock)
{
   compat_lock_acquire(lock, 0);
}

int __syscall_lock_try_acquire(int *lock)
{
   return compat_lock_try_acquire(lock, 0);
}

void __syscall_lock_release(int *lock)
{
   compat_lock_release(lock);
}

void __syscall_lock_close(int *lock)
{
   compat_lock_close(lock);
}

void __syscall_lock_acquire_recursive(int *lock)
{
   compat_lock_acquire(lock, 1);
}

int __syscall_lock_try_acquire_recursive(int *lock)
{
   return compat_lock_try_acquire(lock, 1);
}

void __syscall_lock_release_recursive(int *lock)
{
   compat_lock_release(lock);
}

void __syscall_lock_close_recursive(int *lock)
{
   compat_lock_close(lock);
}

void __syscall_malloc_lock(struct _reent *r)
{
   if (__syscalls.malloc_lock)
      __syscalls.malloc_lock(r);
}

void __syscall_malloc_unlock(struct _reent *r)
{
   if (__syscalls.malloc_unlock)
      __syscalls.malloc_unlock(r);
}

int __syscall_gettod_r(struct _reent *r, struct timeval *tv, struct timezone *tz)
{
   if (__syscalls.gettod_r)
      return __syscalls.gettod_r(r, tv, tz);
   r->_errno = ENOSYS;
   return -1;
}

void __syscall_exit(int rc)
{
   if (__syscalls.exit)
      __syscalls.exit(rc);
   for (;;)
      ;
}

/* The prebuilt Wii ipc.o calls the ctype accessor newlib exports only
 * with __HAVE_LOCALE_INFO__; elsewhere ctype.h makes it a macro over
 * the C locale table. */
#ifdef __locale_ctype_ptr
#undef __locale_ctype_ptr
const char *__locale_ctype_ptr(void)
{
   return _ctype_;
}
#endif

#endif /* INTERNAL_LIBOGC */
