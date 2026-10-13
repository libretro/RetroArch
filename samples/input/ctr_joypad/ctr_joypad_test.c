/* The 3DS joypad driver (input/drivers_joypad/ctr_joypad.c), against
 * the tree's 3DS stubs, with the HID reads defined by the test. Held to:
 *
 * - the console's own controls are a pad on port 0, held or not, and on
 *   no other port;
 * - buttons and both sticks read from it, and from no other port. */

#include <stdio.h>
#include <string.h>

#include <boolean.h>

#include <3ds.h>

/* ---- the stand-ins ---- */
static u32            t_keys;
static circlePosition t_cpad, t_cstick;

void hidScanInput(void)                  { }
u32  hidKeysHeld(void)                   { return t_keys; }
void hidTouchRead(touchPosition *pos)    { memset(pos, 0, sizeof(*pos)); }
void hidCircleRead(circlePosition *pos)  { *pos = t_cpad; }
void irrstCstickRead(circlePosition *pos){ *pos = t_cstick; }

#include "input/input_driver.h"
#include "input/drivers_joypad/ctr_joypad.c"

bool input_autoconfigure_connect(const char *name, const char *display_name,
      const char *phys, const char *driver, unsigned port,
      unsigned vid, unsigned pid)
{
   (void)name; (void)display_name; (void)phys; (void)driver;
   (void)port; (void)vid; (void)pid;
   return true;
}

/* ---- the checks ---- */
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
   unsigned p;

   check(ctr_joypad.init(NULL) != NULL, "init");

   /* Nothing held: the pad is still there, on port 0 only. */
   t_keys = 0;
   ctr_joypad.poll();
   check(ctr_joypad.query_pad(0), "idle: there on port 0");
   for (p = 1; p < MAX_USERS; p++)
      if (ctr_joypad.query_pad(p))
         break;
   check(p == MAX_USERS, "idle: no other port");

   /* Held: still only port 0. */
   t_keys      = KEY_B | KEY_DDOWN;
   t_cpad.dx   = 0;
   t_cpad.dy   = -127;
   t_cstick.dx = 127;
   t_cstick.dy = 0;
   ctr_joypad.poll();
   check(ctr_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_B) != 0,    "B held");
   check(ctr_joypad.button(0, RETRO_DEVICE_ID_JOYPAD_DOWN) != 0, "down held");
   check(ctr_joypad.axis(0, AXIS_POS(1)) > 0x7e00, "circle pad down");
   check(ctr_joypad.axis(0, AXIS_POS(2)) > 0x7e00, "c-stick right");
   for (p = 1; p < MAX_USERS; p++)
      if (ctr_joypad.query_pad(p) || ctr_joypad.button(p, RETRO_DEVICE_ID_JOYPAD_B))
         break;
   check(p == MAX_USERS, "held: no other port");

   ctr_joypad.destroy();

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("ctr_joypad: all checks passed\n");
   return 0;
}
