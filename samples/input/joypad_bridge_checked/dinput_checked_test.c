/* The DirectInput joypad driver, held to what the snapshot bridge
 * assumes of a driver it serves (see bridge_view.h). The driver source
 * is included for its pad table, which is filled here by hand: no
 * device is opened, so it runs under Wine with none. */
#include "../../../input/drivers_joypad/dinput_joypad.c"

#include "bridge_view.h"

int main(void)
{
   static const DWORD povs[] = {
      0xFFFFFFFF, 0, 4500, 9000, 13500, 18000, 22500, 27000, 31500,
      100, 36000 };
   unsigned n, i, bad = 0;

   for (n = 0; n < 4000; n++)
   {
      struct dinput_joypad_data *pad = &g_pads[n % 3];
      LONG *axes[8];

      pad->joypad = (LPDIRECTINPUTDEVICE8)(uintptr_t)1; /* never called through */
      for (i = 0; i < sizeof(pad->joy_state.rgbButtons); i++)
         pad->joy_state.rgbButtons[i] = (bv_rand() & 1) ? 0x80 : 0;
      for (i = 0; i < 4; i++)
         pad->joy_state.rgdwPOV[i] = povs[bv_rand() % (sizeof(povs) / sizeof(povs[0]))];
      axes[0] = &pad->joy_state.lX;   axes[1] = &pad->joy_state.lY;
      axes[2] = &pad->joy_state.lZ;   axes[3] = &pad->joy_state.lRx;
      axes[4] = &pad->joy_state.lRy;  axes[5] = &pad->joy_state.lRz;
      axes[6] = &pad->joy_state.rglSlider[0];
      axes[7] = &pad->joy_state.rglSlider[1];
      for (i = 0; i < 8; i++)
      {
         unsigned r = bv_rand() % 8;
         *axes[i]   = (r == 0) ? 0
                    : (r == 1) ? 0x7fff
                    : (r == 2) ? -0x7fff
                    : (r == 3) ? -0x8000
                    : (LONG)(int16_t)bv_rand();
      }
      bad += bv_compare(&dinput_joypad, n % 3);
   }

   /* a port with no pad on it has no buttons */
   {
      input_bits_t bits;
      g_pads[5].joypad = NULL;
      g_pads[5].joy_state.rgbButtons[3] = 0x80;
      dinput_joypad_get_buttons(5, &bits);
      for (i = 0; i < 256; i++)
         if (BIT256_GET(bits, i))
            bad++;
   }

   if (bad)
   {
      printf("FAIL dinput_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS dinput_checked_test: 4000 random pads; what the bridge would"
         " read is what the driver says\n");
   return 0;
}
