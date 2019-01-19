#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#include <gctypes.h>
#include <lwp_threads.h>

typedef struct {
	vu32 lock;
} spinlock_t;

#define SPIN_LOCK_UNLOCKED		(spinlock_t){0}

#define spin_lock_init(x)		do { *(x) = SPIN_LOCK_UNLOCKED; }while(0)

static __inline__ u32 _test_and_set(u32 *atomic)
{
	 register u32 ret;

	__asm__ __volatile__ ("1: lwarx %0,0,%1\n"
						  "   cmpwi   0,%0,0\n"
						  "   bne-    2f\n"
						  "   stwcx.  %2,0,%1\n"
						  "   bne-    1b\n"
						  "   isync\n"
						  "2:" : "=&r"(ret)
							   : "r"(atomic), "r"(1)
							   : "cr0", "memory");

	return ret;
}

static __inline__ u32 atomic_inc(u32 *pint)
{
	register u32 ret;
	__asm__ __volatile__(
		"1:	lwarx	%0,0,%1\n\
			addi	%0,%0,1\n\
			stwcx.	%0,0,%1\n\
			bne-	1b\n\
			isync\n"
			: "=&r"(ret) : "r"(pint)
			: "cr0", "memory");
	return ret;
}

static __inline__ u32 atomic_dec(u32 *pint)
{
	register u32 ret;
	__asm__ __volatile__(
		"1:	lwarx	%0,0,%1\n\
			addi	%0,%0,-1\n\
			stwcx.	%0,0,%1\n\
			bne-	1b\n\
			isync\n"
			: "=&r"(ret) : "r"(pint)
			: "cr0", "memory");
	return ret;
}

static __inline__ void spin_lock(spinlock_t *lock)
{
        register u32 tmp;

        __asm__ __volatile__(
	   "b       1f                      # spin_lock\n\
2:      lwzx    %0,0,%1\n\
        cmpwi   0,%0,0\n\
        bne+    2b\n\
1:      lwarx   %0,0,%1\n\
        cmpwi   0,%0,0\n\
        bne-    2b\n\
        stwcx.  %2,0,%1\n\
        bne-    2b\n\
        isync"
        : "=&r"(tmp)
        : "r"(lock), "r"(1)
        : "cr0", "memory");
}

static __inline__ void spin_lock_irqsave(spinlock_t *lock,register u32 *p_isr_level)
{
	register u32 level;
    register u32 tmp;

	_CPU_ISR_Disable(level);

    __asm__ __volatile__(
   "	b       1f                      # spin_lock\n\
	2:  lwzx    %0,0,%1\n\
		cmpwi   0,%0,0\n\
		bne+    2b\n\
	1:	lwarx   %0,0,%1\n\
		cmpwi   0,%0,0\n\
		bne-    2b\n\
		stwcx.  %2,0,%1\n\
		bne-    2b\n\
		isync"
    : "=&r"(tmp)
    : "r"(lock), "r"(1)
    : "cr0", "memory");

	*p_isr_level = level;
}

static __inline__ void spin_unlock(spinlock_t *lock)
{
        __asm__ __volatile__("eieio             # spin_unlock": : :"memory");
        lock->lock = 0;
}

static __inline__ void spin_unlock_irqrestore(spinlock_t *lock,register u32 isr_level)
{
    __asm__ __volatile__(
		"eieio             # spin_unlock"
		: : :"memory");
    lock->lock = 0;

	_CPU_ISR_Restore(isr_level);
}

typedef struct {
	vu32 lock;
} rwlock_t;

#define RW_LOCK_UNLOCKED		(rwlock_t){0}

#define read_lock_init(lp)		do { *(lp) = RW_LOCK_UNLOCKED; }while(0)

static __inline__ void read_lock(rwlock_t *rw)
{
        register u32 tmp;

        __asm__ __volatile__(
        "b              2f              # read_lock\n\
1:      lwzx            %0,0,%1\n\
        cmpwi           0,%0,0\n\
        blt+            1b\n\
2:      lwarx           %0,0,%1\n\
        addic.          %0,%0,1\n\
        ble-            1b\n\
        stwcx.          %0,0,%1\n\
        bne-            2b\n\
        isync"
        : "=&r"(tmp)
        : "r"(&rw->lock)
        : "cr0", "memory");
}

static __inline__ void read_unlock(rwlock_t *rw)
{
        register u32 tmp;

        __asm__ __volatile__(
        "eieio                          # read_unlock\n\
1:      lwarx           %0,0,%1\n\
        addic           %0,%0,-1\n\
        stwcx.          %0,0,%1\n\
        bne-            1b"
        : "=&r"(tmp)
        : "r"(&rw->lock)
        : "cr0", "memory");
}

static __inline__ void write_lock(rwlock_t *rw)
{
        register u32 tmp;

        __asm__ __volatile__(
        "b              2f              # write_lock\n\
1:      lwzx            %0,0,%1\n\
        cmpwi           0,%0,0\n\
        bne+            1b\n\
2:      lwarx           %0,0,%1\n\
        cmpwi           0,%0,0\n\
        bne-            1b\n\
        stwcx.          %2,0,%1\n\
        bne-            2b\n\
        isync"
        : "=&r"(tmp)
        : "r"(&rw->lock), "r"(-1)
        : "cr0", "memory");
}

static __inline__ void write_unlock(rwlock_t *rw)
{
        __asm__ __volatile__("eieio             # write_unlock": : :"memory");
        rw->lock = 0;
}

#endif
