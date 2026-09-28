/* Internal-libogc syscall bridge for modern devkitPPC newlibs.
 *
 * The in-tree prebuilt libogc installs its heap, lock, exit and
 * time-of-day handlers into newlib's __syscalls dispatch table at
 * SYS_Init (system.c), and the devkitPPC libsysbase of its era routed
 * everything through that table.  devkitPPC r38 and later removed the
 * table: libsysbase now dispatches through per-symbol weak
 * __syscall_* hooks instead, so the internal-libogc link fails on the
 * undefined __syscalls -- and merely defining storage for it would be
 * worse than the link error, because libsysbase's default sbrk would
 * place the heap by linker symbols instead of libogc's MEM1 arena,
 * colliding with SYS_GetArena1Lo allocations, and malloc would run
 * unlocked under lwp threads.
 *
 * This file defines the table the prebuilt SYS_Init writes into and
 * provides strong __syscall_* hooks that forward to it, so the modern
 * libsysbase routes exactly where the old one did.  The table layout
 * is fixed by the prebuilt binary, not by any header we control:
 * SYS_Init stores its nine handlers at offsets 0..32 in declaration
 * order (verified against the archive's relocations), so the slots are
 * mirrored here, with the old libsysbase's pre-SYS_Init behaviour kept
 * for NULL slots.  Compiled only for EXTERNAL_LIBOGC=0 GameCube/Wii
 * builds; external libogc pairs with its own toolchain's libsysbase
 * and never sees this file. */

#ifdef INTERNAL_LIBOGC

#include <stddef.h>
#include <errno.h>
#include <reent.h>
#include <sys/time.h>

/* Slot layout written by the prebuilt SYS_Init.  Offsets are
 * load-bearing; the trailing reserve absorbs whatever the era's
 * iosupport.h declared beyond the slots libogc touches. */
struct __syscalls_compat
{
   void *(*sbrk_r)(struct _reent *r, ptrdiff_t incr);   /* +0  */
   int   (*lock_init)(int *lock);                       /* +4  */
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

void *__syscall_sbrk_r(struct _reent *r, ptrdiff_t incr)
{
   if (__syscalls.sbrk_r)
      return __syscalls.sbrk_r(r, incr);
   r->_errno = ENOMEM;
   return (void*)-1;
}

/* Before SYS_Init there is one context and no lwp threads, so a
 * missing lock handler is a safe no-op, exactly as the old dispatch
 * behaved with a zeroed table. */
void __syscall_lock_init(int *lock)
{
   if (__syscalls.lock_init)
      __syscalls.lock_init(lock);
}

void __syscall_lock_close(int *lock)
{
   if (__syscalls.lock_close)
      __syscalls.lock_close(lock);
}

void __syscall_lock_acquire(int *lock)
{
   if (__syscalls.lock_acquire)
      __syscalls.lock_acquire(lock);
}

void __syscall_lock_release(int *lock)
{
   if (__syscalls.lock_release)
      __syscalls.lock_release(lock);
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

#endif /* INTERNAL_LIBOGC */
