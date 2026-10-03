/* Power and reset buttons, power off, restart, return to the system. */

#ifndef GEKKO_POWER_H
#define GEKKO_POWER_H

#include <gekko/gekko.h>

/* Button presses; both run in interrupt context. */
typedef void (*gk_button_fn)(void *data);
void gk_power_set_callbacks(gk_button_fn on_power, gk_button_fn on_reset,
      void *data);

/* Wii: switch the console off.  GameCube: restart. */
void gk_power_off(void);
/* Restart the console. */
void gk_power_reset(void);
/* Back to the loader if one stayed resident, else the system menu
 * (Wii) or the boot menu (GameCube). */
void gk_exit_to_loader(void);

#endif
