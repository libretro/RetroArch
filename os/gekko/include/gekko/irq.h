/* Processor-interface interrupts.
 *
 * Handlers run with interrupts off on the interrupt stack and must
 * clear the cause at their device.  They may wake threads; a woken
 * thread of higher priority runs as soon as the handler returns. */

#ifndef GEKKO_IRQ_H
#define GEKKO_IRQ_H

#include <gekko/gekko.h>

enum gk_irq
{
   GK_IRQ_GP_ERROR = 0,
   GK_IRQ_RESET    = 1,
   GK_IRQ_DVD      = 2,
   GK_IRQ_SI       = 3,
   GK_IRQ_EXI      = 4,
   GK_IRQ_AI       = 5,
   GK_IRQ_DSP      = 6,
   GK_IRQ_MEM      = 7,
   GK_IRQ_VI       = 8,
   GK_IRQ_PE_TOKEN = 9,
   GK_IRQ_PE_DONE  = 10,
   GK_IRQ_CP_FIFO  = 11,
   GK_IRQ_DEBUG    = 12,
   GK_IRQ_HSP      = 13,
   GK_IRQ_IPC      = 14,
   GK_IRQ_COUNT    = 15
};

typedef void (*gk_irq_fn)(enum gk_irq irq, void *data);

/* Install (or with NULL remove) the handler for a source and unmask
 * it.  Returns the previous handler. */
gk_irq_fn gk_irq_set(enum gk_irq irq, gk_irq_fn fn, void *data);
void      gk_irq_mask(enum gk_irq irq);
void      gk_irq_unmask(enum gk_irq irq);

/* Integer state of an interrupted thread, as exception handlers see
 * and may change it (srr0 is the resume address). */
struct gk_ctx
{
   uint32_t gpr[32];
   uint32_t cr;
   uint32_t lr;
   uint32_t ctr;
   uint32_t xer;
   uint32_t srr0;
   uint32_t srr1;
   uint32_t pad[2];
};

/* Processor exceptions a driver may claim (alignment, program, ...).
 * The handler returns nonzero if it dealt with the fault. */
typedef int (*gk_exc_fn)(unsigned vector, struct gk_ctx *ctx);
gk_exc_fn gk_exception_set(unsigned vector, gk_exc_fn fn);

#endif
