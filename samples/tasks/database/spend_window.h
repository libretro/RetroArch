#ifndef __SCAN_SPEND_WINDOW_H
#define __SCAN_SPEND_WINDOW_H

#include <boolean.h>

#include "../../../tasks/tasks_internal.h"

/* Holds the next open of the shared I/O window to its floor of one
 * step (link with -Wl,--wrap=task_nbio_slice_open).  Charging the
 * window does not hold: it resets on the wall clock, and a reset
 * between the charge and the scanner's open grants the whole window. */
static bool window_spend_next;
/* Called before each open, when set. */
static void (*window_open_hook)(void);

void __real_task_nbio_slice_open(nbio_budget_t *b);
void __wrap_task_nbio_slice_open(nbio_budget_t *b)
{
   if (window_open_hook)
      window_open_hook();
   __real_task_nbio_slice_open(b);
   if (window_spend_next)
   {
      window_spend_next = false;
      b->allowance      = 0;
   }
}

static void spend_window(void)
{
   window_spend_next = true;
}

#endif
