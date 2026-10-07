/* The XInput-only joypad driver (UWP, Xbox, builds without
 * DirectInput), held to what the snapshot bridge assumes of a driver
 * it serves (see bridge_view.h). The driver source is included for its
 * state table, which is filled here by hand: XInput itself is not
 * called, so it runs under Wine with no controller. */
#include "../../../input/drivers_joypad/xinput_joypad.c"

#include "bridge_view.h"

int main(void)
{
   unsigned n, i, bad = 0;

   g_xinput_num_buttons = 11;

   for (n = 0; n < 4000; n++)
   {
      unsigned port            = n % 4;
      XINPUT_GAMEPAD *pad      = &g_xinput_states[port].xstate.Gamepad;
      unsigned r               = bv_rand() % 8;

      g_xinput_states[port].connected = true;
      pad->wButtons            = (WORD)bv_rand();
      pad->bLeftTrigger        = (BYTE)bv_rand();
      pad->bRightTrigger       = (BYTE)bv_rand();
      pad->sThumbLX            = (r == 0) ? 0 : (r == 1) ? 32767
                               : (r == 2) ? (SHORT)-32768 : (SHORT)bv_rand();
      pad->sThumbLY            = (SHORT)bv_rand();
      pad->sThumbRX            = (SHORT)bv_rand();
      pad->sThumbRY            = (SHORT)bv_rand();
      bad += bv_compare(&xinput_joypad, port);
   }

   /* a port with no controller: no buttons, no axes, and nothing read
    * from before the table (the port's index used to be -1 there) */
   {
      input_bits_t bits;
      g_xinput_states[2].connected = false;
      xinput_joypad_get_buttons(2, &bits);
      for (i = 0; i < 256; i++)
         if (BIT256_GET(bits, i))
            bad++;
      if (xinput_joypad_button(2, 0) || xinput_joypad_axis(2, AXIS_POS(0)))
         bad++;
      if (xinput_joypad_button(9, 0) || xinput_joypad_axis(9, AXIS_POS(0)))
         bad++;
   }

   if (bad)
   {
      printf("FAIL xinput_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS xinput_checked_test: 4000 random pads; what the bridge would"
         " read is what the driver says\n");
   return 0;
}
