/* Processor-interface interrupt controller. */

#include "kernel.h"

#define PI_INTSR 0xcc003000u   /* cause */
#define PI_INTMR 0xcc003004u   /* mask */

static gk_irq_fn handlers[GK_IRQ_COUNT];
static void     *handler_data[GK_IRQ_COUNT];
static uint32_t  mask;

void gk_irq_init(void)
{
   mask = 0;
   GK_REG32(PI_INTMR) = 0;
   /* A pending reset-switch edge from before we started. */
   GK_REG32(PI_INTSR) = 1u << GK_IRQ_RESET;
}

gk_irq_fn gk_irq_set(enum gk_irq irq, gk_irq_fn fn, void *data)
{
   gk_irq_fn old;
   uint32_t level = gk_irq_disable();
   old                = handlers[irq];
   handlers[irq]      = fn;
   handler_data[irq]  = data;
   if (fn)
      mask |= 1u << irq;
   else
      mask &= ~(1u << irq);
   GK_REG32(PI_INTMR) = mask;
   gk_irq_restore(level);
   return old;
}

void gk_irq_mask(enum gk_irq irq)
{
   uint32_t level = gk_irq_disable();
   mask &= ~(1u << irq);
   GK_REG32(PI_INTMR) = mask;
   gk_irq_restore(level);
}

void gk_irq_unmask(enum gk_irq irq)
{
   uint32_t level = gk_irq_disable();
   if (handlers[irq])
      mask |= 1u << irq;
   GK_REG32(PI_INTMR) = mask;
   gk_irq_restore(level);
}

void gk_irq_dispatch(void)
{
   uint32_t pending = GK_REG32(PI_INTSR) & mask;
   while (pending)
   {
      unsigned irq = 31 - __builtin_clz(pending);
      pending &= ~(1u << irq);
      handlers[irq]((enum gk_irq)irq, handler_data[irq]);
   }
}
