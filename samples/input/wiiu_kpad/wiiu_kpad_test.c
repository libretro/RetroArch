/* The Wii U Wiimote driver (input/drivers_joypad/wiiu/kpad_driver.c),
 * against the tree's Wii U SDK headers, with KPADRead and the slot
 * bookkeeping replaced by the test's. Held to:
 *
 * - a Nunchuk gives its stick and the Wiimote's buttons;
 * - a device type the driver does not read holds nothing and reads
 *   centred, whatever the type before it held;
 * - a Nunchuk read again after it is read again. */

#include <stdio.h>
#include <string.h>

#include <boolean.h>

#include "input/drivers_joypad/wiiu/kpad_driver.c"
#include "input/drivers_joypad/wiiu/pad_functions.c"

/* ---- the stand-ins ---- */
wiiu_joypad_t   joypad_state;
static KPADData t_kpad;

s32 KPADRead(s32 chan, void *data, u32 size)
{
   (void)size;
   if (chan != 0)
      return 0;
   memcpy(data, &t_kpad, sizeof(t_kpad));
   return 1;
}

int pad_connection_find_vacant_pad(joypad_connection_t *joyconn)
{
   (void)joyconn;
   return 0;
}
void pad_connection_release_slot(joypad_connection_t *joyconn) { (void)joyconn; }
void input_pad_connect(unsigned port, input_device_driver_t *driver)
{
   (void)port; (void)driver;
}
bool input_autoconfigure_disconnect(unsigned port, const char *ident)
{
   (void)port; (void)ident;
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

static bool all_centred(void)
{
   unsigned a;
   for (a = 0; a < 4; a++)
      if (kpad_driver.axis(0, AXIS_POS(a)) || kpad_driver.axis(0, AXIS_NEG(a)))
         return false;
   return true;
}

static void report(uint8_t type, uint32_t btns, float sx, float sy)
{
   memset(&t_kpad, 0, sizeof(t_kpad));
   t_kpad.device_type      = type;
   t_kpad.btns_h           = btns;
   t_kpad.nunchuck.stick_x = sx;
   t_kpad.nunchuck.stick_y = sy;
}

int main(void)
{
   report(WIIMOTE_TYPE_NONE, 0, 0, 0);
   check(kpad_driver.init(NULL) != NULL, "init");

   report(WIIMOTE_TYPE_NUNCHUK, 1u << 3, 0.0f, -1.0f);
   kpad_driver.poll();
   check(!all_centred(), "nunchuk: stick held");
   check(kpad_driver.button(0, 3) != 0, "nunchuk: button held");

   /* A device type the driver does not read (MotionPlus with a
    * Nunchuk), the same stick and button reported. */
   report(6, 1u << 3, 0.0f, -1.0f);
   kpad_driver.poll();
   check(all_centred(), "unread type: sticks centred");
   check(kpad_driver.button(0, 3) == 0, "unread type: nothing held");

   report(WIIMOTE_TYPE_NUNCHUK, 0, 0.0f, -1.0f);
   kpad_driver.poll();
   check(!all_centred(), "nunchuk again: stick read");

   kpad_driver.destroy();

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("wiiu_kpad: all checks passed\n");
   return 0;
}
