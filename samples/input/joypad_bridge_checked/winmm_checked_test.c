/* The WinMM joypad driver, held to what the snapshot bridge assumes of
 * a driver it serves (see bridge_view.h). The driver source is included
 * for its pad table, which is filled here by hand: no joystick is asked
 * for, so it runs under Wine with none. */
#include "../../../input/drivers_joypad/winmm_joypad.c"

#include "bridge_view.h"

int main(void)
{
   /* a hat's position, hundredths of a degree from up, and the
    * directions it holds */
   static const struct { DWORD pov; uint16_t up, down, left, right; } povs[] = {
      { 0,     1, 0, 0, 0 }, { 4500,  1, 0, 0, 1 }, { 9000,  0, 0, 0, 1 },
      { 13500, 0, 1, 0, 1 }, { 18000, 0, 1, 0, 0 }, { 22500, 0, 1, 1, 0 },
      { 27000, 0, 0, 1, 0 }, { 31500, 1, 0, 1, 0 }, { 35999, 1, 0, 0, 0 },
      { 36000, 0, 0, 0, 0 }, { 65535, 0, 0, 0, 0 } };
   unsigned n, i, bad = 0;

   for (n = 0; n < 4000; n++)
   {
      winmm_joypad_t *pad = &winmm_pads[n % 3];

      unsigned k   = bv_rand() % (sizeof(povs) / sizeof(povs[0]));
      pad->present = true;
      bv_fill_view(&pad->view, 32, WINMM_JOYPAD_AXES);
      /* the hat as the poll reads it */
      pad->view.hats = winmm_joypad_hat(povs[k].pov);
      if (     !winmm_joypad.button(n % 3, HAT_MAP(0, HAT_UP_MASK))    != !povs[k].up
            || !winmm_joypad.button(n % 3, HAT_MAP(0, HAT_DOWN_MASK))  != !povs[k].down
            || !winmm_joypad.button(n % 3, HAT_MAP(0, HAT_LEFT_MASK))  != !povs[k].left
            || !winmm_joypad.button(n % 3, HAT_MAP(0, HAT_RIGHT_MASK)) != !povs[k].right)
         bad++;
      bad += bv_compare(&winmm_joypad, n % 3);
      bad += bv_compare_view(&winmm_joypad, n % 3);
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
