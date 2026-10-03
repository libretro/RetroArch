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

extern struct gk_argv gk_loader_argv;  /* crt0: filled in by the loader */

static struct gk_argv  no_args;
struct gk_argv        *__system_argv = &no_args;

#define GK_ARGV_MAGIC 0x5f617267
#define ARGS_LEN      4096
#define ARGS_MAX      32

static struct gk_argv  args;
static char            args_line[ARGS_LEN];
static char           *args_argv[ARGS_MAX + 1];

/* The loader passes NUL-separated strings somewhere the heap will
 * reuse; keep a copy and split it into argv. */
static void args_take(void)
{
   const struct gk_argv *a = &gk_loader_argv;
   uint32_t              p = (uint32_t)a->cmdline;
   int                 len = a->length;
   int                   i = 0;
   int                argc = 0;

   if (a->magic != GK_ARGV_MAGIC || len <= 0)
      return;
   switch (p >> 28)
   {
      case 0x8: case 0x9: case 0xc: case 0xd:
         break;
      default:
         return;
   }
   if (len > ARGS_LEN - 1)
      len = ARGS_LEN - 1;
   memcpy(args_line, a->cmdline, len);
   args_line[len] = '\0';
   while (i < len && argc < ARGS_MAX)
   {
      if (args_line[i])
      {
         args_argv[argc++] = args_line + i;
         i += strlen(args_line + i);
      }
      i++;
   }
   if (!argc)
      return;
   args_argv[argc] = NULL;
   args.magic      = GK_ARGV_MAGIC;
   args.cmdline    = args_line;
   args.length     = len;
   args.argc       = argc;
   args.argv       = args_argv;
   args.end        = args_argv + argc;
   __system_argv   = &args;
}

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
void SYS_PreMain(void) { }

void gk_boot(void)
{
   uint32_t bus = *(volatile uint32_t*)LOMEM_BUS_CLOCK;
   struct gk_thread *m;

   args_take();
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
