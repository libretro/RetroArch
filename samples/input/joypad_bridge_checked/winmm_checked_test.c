/* The WinMM joypad driver, held to what the snapshot bridge assumes of
 * a driver it serves (see bridge_view.h). The driver source is included
 * for its pad table, which is filled here by hand: no joystick is asked
 * for, so it runs under Wine with none. */
#include "../../../input/drivers_joypad/winmm_joypad.c"

#include "bridge_view.h"

int main(void)
{
   static const uint16_t hats[] = {
      0, HAT_UP_MASK, HAT_DOWN_MASK, HAT_LEFT_MASK, HAT_RIGHT_MASK,
      HAT_UP_MASK | HAT_LEFT_MASK, HAT_UP_MASK | HAT_RIGHT_MASK,
      HAT_DOWN_MASK | HAT_LEFT_MASK, HAT_DOWN_MASK | HAT_RIGHT_MASK };
   unsigned n, i, bad = 0;

   for (n = 0; n < 4000; n++)
   {
      winmm_joypad_t *pad = &winmm_pads[n % 3];

      pad->present = true;
      pad->buttons = bv_rand();
      pad->hat     = hats[bv_rand() % (sizeof(hats) / sizeof(hats[0]))];
      for (i = 0; i < WINMM_JOYPAD_AXES; i++)
      {
         unsigned r   = bv_rand() % 8;
         pad->axes[i] = (r == 0) ? 0
                      : (r == 1) ? 0x7fff
                      : (r == 2) ? -0x7fff
                      : (r == 3) ? -0x8000
                      : (int16_t)bv_rand();
      }
      bad += bv_compare(&winmm_joypad, n % 3);
   }

   /* a port past the pads the driver keeps has no buttons */
   {
      input_bits_t bits;
      memset(&bits, 0xff, sizeof(bits));
      winmm_joypad_get_buttons(WINMM_JOYPAD_MAX, &bits);
      for (i = 0; i < 256; i++)
         if (BIT256_GET(bits, i))
            bad++;
   }

   if (bad)
   {
      printf("FAIL winmm_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS winmm_checked_test: 4000 random pads; what the bridge would"
         " read is what the driver says\n");
   return 0;
}
