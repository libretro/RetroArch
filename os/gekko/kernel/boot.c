/* From crt0 to main(): memory, vectors, the scheduler, arguments. */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <gekko/console.h>

#include "kernel.h"

/* Low-memory globals the loader and the system leave behind. */
#define LOMEM_MEM1_SIZE   0x80000028u
#define LOMEM_BUS_CLOCK   0x800000f8u
#define LOMEM_MEM2_LO     0x80003124u
#define LOMEM_MEM2_HI     0x80003128u

extern uint8_t  __gk_image_end[];
extern uint32_t __gk_stack_lo[];
extern uint32_t gk_loader_argv;   /* crt0: argv block from the loader */

void __crtmain(void);

gk_arena_t gk_mem1;
gk_arena_t gk_mem2;

/* The argument block loaders fill in (crtmain reads argc, argv). */
struct gk_argv
{
   int    magic;
   char  *cmdline;
   int    length;
   int    argc;
   char **argv;
   char **end;
};

static struct gk_argv  no_args;
struct gk_argv        *__system_argv = &no_args;

#define GK_ARGV_MAGIC 0x5f617267

void gk_arena_init(void)
{
   uint32_t mem1 = *(volatile uint32_t*)LOMEM_MEM1_SIZE;
   if (mem1 < 0x01800000u || mem1 > 0x04000000u)
      mem1 = 0x01800000u;
   gk_mem1.lo = (uint8_t*)(((uint32_t)__gk_image_end + 31) & ~31u);
   gk_mem1.hi = (uint8_t*)(0x80000000u + mem1);
#if GK_RVL
   {
      uint32_t lo = *(volatile uint32_t*)LOMEM_MEM2_LO;
      uint32_t hi = *(volatile uint32_t*)LOMEM_MEM2_HI;
      if ((lo >> 28) != 9 || (hi >> 28) != 9 || hi <= lo)
      {
         lo = 0x90000800u;
         hi = 0x933e0000u;
      }
      gk_mem2.lo = (uint8_t*)((lo + 31) & ~31u);
      gk_mem2.hi = (uint8_t*)(hi & ~31u);
   }
#endif
}

void *gk_arena_take_top(gk_arena_t *arena, size_t size, size_t align)
{
   uint32_t top;
   uint32_t level = gk_irq_disable();
   top = ((uint32_t)arena->hi - size) & ~(uint32_t)(align - 1);
   if (top < (uint32_t)arena->lo)
      top = 0;
   else
      arena->hi = (uint8_t*)top;
   gk_irq_restore(level);
   return (void*)top;
}

/* Dolphin logs calls to a function of this name; elsewhere it is a
 * no-op the console can hook. */
void OSReport(const char *fmt, ...) __attribute__((noinline));
volatile const char *gk_last_report;
void OSReport(const char *fmt, ...)
{
   gk_last_report = fmt;
   __asm__ __volatile__("" ::: "memory");
}

void gk_debug_printf(const char *fmt, ...)
{
   static char buf[256];
   va_list ap;
   uint32_t level = gk_irq_disable();
   va_start(ap, fmt);
   vsnprintf(buf, sizeof(buf), fmt, ap);
   va_end(ap);
   OSReport("%s\n", buf);
   gk_console_write(buf, strlen(buf));
   gk_console_write("\n", 1);
   gk_irq_restore(level);
}

/* crtmain calls this before main(). */
void SYS_PreMain(void)
{
   struct gk_argv *a = (struct gk_argv*)gk_loader_argv;
   if (a && a->magic == GK_ARGV_MAGIC && a->argc > 0)
      __system_argv = a;
}

void gk_boot(void)
{
   uint32_t bus = *(volatile uint32_t*)LOMEM_BUS_CLOCK;
   struct gk_thread *m;

   if (bus < 100000000u || bus > 300000000u)
      bus = GK_RVL ? 243000000u : 162000000u;
   gk_tb_hz = bus / 4;

   gk_irq_init();
   gk_arena_init();
   gk_sched_init(__gk_stack_lo);
   m = gk_sched_main();
   __asm__ __volatile__("mtspr 272,%0" : : "r"(m));
   gk_vectors_install();
   gk_newlib_init();

   __asm__ __volatile__("mtdec %0" : : "r"(0x7fffffff));
   gk_msr_set(gk_msr_get() | GK_MSR_EE);

   __crtmain();
}
