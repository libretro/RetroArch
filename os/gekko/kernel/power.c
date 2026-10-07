/* Buttons and leaving the program, both consoles. */

#include <gekko/irq.h>
#include <gekko/power.h>

#include "kernel.h"

#define PI_INTSR    0xcc003000u
#define PI_INTMR    0xcc003004u
#define PI_RESET    0xcc003024u
#define PI_RSW_UP   (1u << 16)   /* reset switch released */

/* Loaders that stay resident leave a return stub here. */
#define LOADER_STUB       0x80001800u
#define LOADER_STUB_MAGIC 0x80001804u
/* mtspr 1011 (HID4) from any register, and a nop. */
#define MTSPR_HID4        0x7c13fba6u
#define PPC_NOP           0x60000000u

static struct gk_exit_hook *exit_hooks;
static gk_button_fn on_power_fn;
static gk_button_fn on_reset_fn;
static void        *button_data;

#if GK_RVL
void gk_stm_watch(int on);
int  gk_stm_power_off(void);
int  gk_stm_reset(void);
int  gk_es_launch_system_menu(void);

/* stm.c, interrupt context. */
void gk_power_event(int power)
{
   gk_button_fn fn = power ? on_power_fn : on_reset_fn;
   if (fn)
      fn(button_data);
}
#else
static void reset_irq(enum gk_irq irq, void *data)
{
   (void)irq;
   (void)data;
   GK_REG32(PI_INTSR) = 1u << GK_IRQ_RESET;
   /* One call per press, on the edge with the switch down. */
   if (!(GK_REG32(PI_INTSR) & PI_RSW_UP) && on_reset_fn)
      on_reset_fn(button_data);
}
#endif

void gk_power_set_callbacks(gk_button_fn on_power, gk_button_fn on_reset,
      void *data)
{
   uint32_t level = gk_irq_disable();
   on_power_fn = on_power;
   on_reset_fn = on_reset;
   button_data = data;
   gk_irq_restore(level);
#if GK_RVL
   gk_stm_watch(on_power || on_reset);
#else
   gk_irq_set(GK_IRQ_RESET, on_reset ? reset_irq : NULL, NULL);
#endif
}

void gk_exit_hook_add(struct gk_exit_hook *h)
{
   uint32_t level = gk_irq_disable();
   h->next    = exit_hooks;
   exit_hooks = h;
   gk_irq_restore(level);
}

void gk_shutdown(void)
{
   struct gk_exit_hook *h;
   uint32_t level = gk_irq_disable();
   h          = exit_hooks;
   exit_hooks = NULL;
   gk_irq_restore(level);
   for (; h; h = h->next)
      h->fn();
#if GK_RVL
   gk_stm_watch(0);
#endif
}

void gk_quiesce(void)
{
   gk_irq_disable();
   GK_REG32(PI_INTMR) = 0;
}

void gk_power_reset(void)
{
#if GK_RVL
   gk_stm_reset();
#endif
   gk_quiesce();
   GK_REG32(PI_RESET) = 0;
   for (;;)
      ;
}

void gk_power_off(void)
{
#if GK_RVL
   gk_stm_power_off();
#endif
   gk_power_reset();
}

static int loader_resident(void)
{
   return *(volatile uint32_t*)LOADER_STUB_MAGIC       == 0x53545542u
       && *(volatile uint32_t*)(LOADER_STUB_MAGIC + 4) == 0x48415858u;
}

static void enter_loader(void)
{
   void (*stub)(void) = (void (*)(void))LOADER_STUB;
#if GK_RVL
   /* The L2 enhancements cannot be turned off without a reset: the
    * loader's own HID4 write goes. */
   volatile uint32_t *op;
   for (op = (volatile uint32_t*)(LOADER_STUB_MAGIC + 8);
         op < (volatile uint32_t*)0x80003000u; op++)
      if ((*op & 0xfc1fffffu) == MTSPR_HID4)
         *op = PPC_NOP;
   gk_dcache_flush((void*)LOADER_STUB, 0x1800);
#endif
   gk_quiesce();
   gk_icache_invalidate((void*)LOADER_STUB, 0x1800);
   stub();
}

void gk_exit_to_loader(void)
{
   gk_shutdown();
   if (loader_resident())
      enter_loader();
#if GK_RVL
   gk_es_launch_system_menu();
#endif
   gk_power_reset();
}

/* After a crash: nothing that needs interrupts or IOS. */
void gk_exit_after_crash(void)
{
   if (loader_resident())
      enter_loader();
   gk_quiesce();
   GK_REG32(PI_RESET) = 0;
   for (;;)
      ;
}
