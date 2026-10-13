/* The DOS joypad driver (input/drivers_joypad/dos_joypad.c), built as
 * DJGPP builds it. DOS input is the keyboard's: the driver has no pad
 * to read. Held to:
 *
 * - no port has a pad;
 * - no button, axis or RetroPad state is ever reported. */

#include <stdio.h>

#include <boolean.h>

#include "input/input_driver.h"
#include "input/drivers_joypad/dos_joypad.c"

static unsigned failures = 0;

static void check(bool cond, const char *what)
{
   if (!cond)
   {
      printf("FAIL: %s\n", what);
      failures++;
   }
}

int main(void)
{
   unsigned p, k;
   uint16_t joykeys[RARCH_FIRST_CUSTOM_BIND];
   uint32_t joyaxes[RARCH_FIRST_CUSTOM_BIND];
   rarch_joypad_info_t info;

   for (k = 0; k < RARCH_FIRST_CUSTOM_BIND; k++)
   {
      joykeys[k] = (uint16_t)k;
      joyaxes[k] = AXIS_POS(k & 3);
   }
   info.auto_binds     = NULL;
   info.axis_threshold = 0.5f;

   check(dos_joypad.init(NULL) != NULL, "init");
   dos_joypad.poll();

   for (p = 0; p < MAX_USERS; p++)
   {
      check(!dos_joypad.query_pad(p), "no pad on any port");
      for (k = 0; k < RARCH_FIRST_CUSTOM_BIND; k++)
         check(dos_joypad.button(p, (uint16_t)k) == 0, "no button");
      check(dos_joypad.axis(p, AXIS_POS(0)) == 0
         && dos_joypad.axis(p, AXIS_NEG(1)) == 0, "no axis");
      info.joy_idx = (uint16_t)p;
      check(dos_joypad.state(&info, joykeys, joyaxes, p) == 0, "no state");
   }

   dos_joypad.destroy();

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("dos_joypad: all checks passed\n");
   return 0;
}
