/* The linuxraw joypad driver, held to what the snapshot bridge assumes
 * of a driver it serves (see bridge_view.h). The driver source is
 * included for its pad table, which is filled here by hand. */
#include "../../../input/drivers_joypad/linuxraw_joypad.c"

#include "bridge_view.h"

int main(void)
{
   unsigned n, i, bad = 0;

   for (n = 0; n < 4000; n++)
   {
      struct linuxraw_joypad *pad = &linuxraw_pads[n % 3];

      pad->fd      = 1;
      /* all thirty-two: the upper sixteen are the ones get_buttons()
       * used to leave out */
      pad->buttons = bv_rand();
      for (i = 0; i < NUM_AXES; i++)
      {
         unsigned r   = bv_rand() % 8;
         pad->axes[i] = (r == 0) ? 0
                      : (r == 1) ? 0x7fff
                      : (r == 2) ? -0x7fff
                      : (int16_t)bv_rand();
      }
      bad += bv_compare(&linuxraw_joypad, n % 3);
   }

   if (bad)
   {
      printf("FAIL linuxraw_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS linuxraw_checked_test: 4000 random pads; what the bridge would"
         " read is what the driver says\n");
   return 0;
}
