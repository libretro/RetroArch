/* RetroArch's own system layer for the GameCube and Wii.
 *
 * os/gekko runs RetroArch on the bare hardware: boot, exceptions,
 * interrupts, threads and the newlib system calls, with the device
 * drivers built on top.  HW_RVL selects the Wii parts. */

#ifndef GEKKO_GEKKO_H
#define GEKKO_GEKKO_H

#include <stddef.h>
#include <stdint.h>

#include <retro_inline.h>

#ifdef HW_RVL
#define GK_RVL 1
#else
#define GK_RVL 0
#endif

/* Cached and uncached views of physical memory. */
#define GK_CACHED(p)    ((void*)((uint32_t)(p) | 0x80000000u))
#define GK_UNCACHED(p)  ((void*)((uint32_t)(p) | 0xC0000000u))
#define GK_PHYS(p)      ((uint32_t)(p) & 0x3FFFFFFFu)

#define GK_MSR_EE 0x8000u
#define GK_MSR_FP 0x2000u

#define GK_CACHE_LINE 32

/* Hardware registers, by uncached address. */
#define GK_REG32(a)     (*(volatile uint32_t*)(a))
#define GK_REG16(a)     (*(volatile uint16_t*)(a))

static INLINE uint32_t gk_msr_get(void)
{
   uint32_t msr;
   __asm__ __volatile__("mfmsr %0" : "=r"(msr));
   return msr;
}

static INLINE void gk_msr_set(uint32_t msr)
{
   __asm__ __volatile__("mtmsr %0" : : "r"(msr) : "memory");
}

/* Interrupts off; returns the state gk_irq_restore() puts back. */
static INLINE uint32_t gk_irq_disable(void)
{
   uint32_t msr = gk_msr_get();
   gk_msr_set(msr & ~GK_MSR_EE);
   return msr & GK_MSR_EE;
}

static INLINE void gk_irq_restore(uint32_t level)
{
   if (level)
      gk_msr_set(gk_msr_get() | GK_MSR_EE);
}

/* 64-bit time base, read without tearing. */
static INLINE uint64_t gk_ticks(void)
{
   uint32_t hi, lo, hi2;
   do
   {
      __asm__ __volatile__("mftbu %0" : "=r"(hi));
      __asm__ __volatile__("mftb %0"  : "=r"(lo));
      __asm__ __volatile__("mftbu %0" : "=r"(hi2));
   } while (hi != hi2);
   return ((uint64_t)hi << 32) | lo;
}

/* Time base rate in Hz: a quarter of the bus clock. */
extern uint32_t gk_tb_hz;

#define GK_US_TO_TICKS(us) ((uint64_t)(us) * gk_tb_hz / 1000000u)
#define GK_TICKS_TO_US(t)  ((uint64_t)(t) * 1000000u / gk_tb_hz)

/* Ticks to microseconds for any span, uptime included: the short
 * form above overflows after a few days of ticks. */
static INLINE uint64_t gk_ticks_to_us(uint64_t t)
{
   return t / gk_tb_hz * 1000000u + t % gk_tb_hz * 1000000u / gk_tb_hz;
}

/* Cache maintenance over [p, p + len). */
void gk_dcache_flush(const void *p, size_t len);
void gk_dcache_store(const void *p, size_t len);
void gk_dcache_invalidate(void *p, size_t len);
void gk_icache_invalidate(const void *p, size_t len);

/* Memory left to the heap after the program image, MEM1 then (Wii)
 * MEM2.  The heap grows into these; a driver that needs a fixed
 * region takes it from the top before the heap gets there. */
typedef struct gk_arena
{
   uint8_t *lo;
   uint8_t *hi;
} gk_arena_t;

extern gk_arena_t gk_mem1;
extern gk_arena_t gk_mem2;

void *gk_arena_take_top(gk_arena_t *arena, size_t size, size_t align);

/* Debug text to whatever is listening: Dolphin's log, and the
 * screen once the console is up. */
void gk_debug_printf(const char *fmt, ...);

/* Stop with a message and the register state of the caller. */
void gk_panic(const char *fmt, ...);

#endif
