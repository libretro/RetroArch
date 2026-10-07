/* Exception dispatch, vector installation and the crash report. */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "kernel.h"

/* Where an exception inside exception context saves itself. */
struct gk_thread gk_nested_frame;
uint32_t         gk_scratch[8];

extern const uint32_t gk_vector_stubs[];
extern const uint32_t gk_fpu_stub[];
extern const uint32_t gk_fpu_stub_end[];

static gk_exc_fn exc_handlers[0x18];

gk_exc_fn gk_exception_set(unsigned vector, gk_exc_fn fn)
{
   gk_exc_fn old;
   unsigned i = vector >> 8;
   if (i >= sizeof(exc_handlers) / sizeof(exc_handlers[0]))
      return NULL;
   old = exc_handlers[i];
   exc_handlers[i] = fn;
   return old;
}

static void install(uint32_t vec, const void *code, size_t len)
{
   void *dst = (void*)(0x80000000u + vec);
   memcpy(dst, code, len);
   gk_dcache_flush(dst, len);
   gk_icache_invalidate(dst, len);
}

void gk_vectors_install(void)
{
   const uint32_t *p = gk_vector_stubs;
   while (p[0])
   {
      install(p[0], p + 2, p[1]);
      p += 2 + p[1] / 4;
   }
   install(0x800, gk_fpu_stub,
         (size_t)((const uint8_t*)gk_fpu_stub_end
          - (const uint8_t*)gk_fpu_stub));
   __asm__ __volatile__("mtspr 274,%0" : : "r"(GK_PHYS(gk_scratch)));
}

#define RELOAD_SECONDS 10

/* Leave the report up, then go back to the loader. */
static void halt(void)
{
   uint64_t end;
   gk_console_show_now();
   gk_debug_printf("returning to the loader in %d seconds", RELOAD_SECONDS);
   end = gk_ticks() + (uint64_t)gk_tb_hz * RELOAD_SECONDS;
   while (gk_ticks() < end)
      ;
   gk_exit_after_crash();
}

static const char *vector_name(unsigned v)
{
   switch (v)
   {
      case 0x0100: return "system reset";
      case 0x0200: return "machine check";
      case 0x0300: return "DSI";
      case 0x0400: return "ISI";
      case 0x0600: return "alignment";
      case 0x0700: return "program";
      case 0x0d00: return "trace";
      case 0x0f00: return "performance monitor";
      case 0x1300: return "breakpoint";
      case 0x1700: return "thermal";
   }
   return "exception";
}

static void crash(unsigned vector, struct gk_thread *t)
{
   uint32_t dar, dsisr, *sp;
   int i;
   __asm__ __volatile__("mfdar %0" : "=r"(dar));
   __asm__ __volatile__("mfdsisr %0" : "=r"(dsisr));
   gk_debug_printf("*** %s (0x%04x) in %s %p",
         vector_name(vector), vector,
         t == &gk_nested_frame ? "exception context" : "thread",
         (void*)gk_cur);
   gk_debug_printf("pc %08x msr %08x lr %08x dar %08x dsisr %08x",
         (unsigned)t->ctx.srr0, (unsigned)t->ctx.srr1,
         (unsigned)t->ctx.lr, (unsigned)dar, (unsigned)dsisr);
   for (i = 0; i < 32; i += 4)
      gk_debug_printf("r%-2d %08x %08x %08x %08x", i,
            (unsigned)t->ctx.gpr[i],     (unsigned)t->ctx.gpr[i + 1],
            (unsigned)t->ctx.gpr[i + 2], (unsigned)t->ctx.gpr[i + 3]);
   /* Return addresses from the stack's back chain. */
   sp = (uint32_t*)t->ctx.gpr[1];
   for (i = 0; i < 12 && sp && ((uint32_t)sp >> 28) >= 8
         && !((uint32_t)sp & 3) && sp[1]; i++)
   {
      gk_debug_printf("  called from %08x", (unsigned)sp[1]);
      sp = (uint32_t*)sp[0];
   }
   halt();
}

void gk_panic(const char *fmt, ...)
{
   char msg[160];
   va_list ap;
   uint32_t lr;
   gk_irq_disable();
   va_start(ap, fmt);
   vsnprintf(msg, sizeof(msg), fmt, ap);
   va_end(ap);
   __asm__ __volatile__("mflr %0" : "=r"(lr));
   gk_debug_printf("*** panic: %s (from %08x)", msg, (unsigned)lr);
   halt();
}

struct gk_thread *gk_exception_dispatch(unsigned vector,
      struct gk_thread *t)
{
   if (t == &gk_nested_frame)
      crash(vector, t);
   switch (vector)
   {
      case 0x0500:
         gk_irq_dispatch();
         break;
      case 0x0900:
         gk_timer_tick(gk_ticks());
         break;
      case 0x0c00:
         break;
      default:
         {
            gk_exc_fn fn = exc_handlers[vector >> 8];
            if (!fn || !fn(vector, &t->ctx))
               crash(vector, t);
         }
         break;
   }
   return gk_sched_switch(t);
}
