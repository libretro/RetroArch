/* The udev joypad driver, held to what the snapshot bridge assumes of
 * a driver it serves (see bridge_view.h). The driver source is
 * included for its pad table, which is filled here by hand: no
 * device is opened. */
#include "../../../input/drivers_joypad/udev_joypad.c"

#include "bridge_view.h"

int main(void)
{
   unsigned n, i, bad = 0;

   for (n = 0; n < 4000; n++)
   {
      struct udev_joypad *pad = &udev_pads[n % 3];

      pad->fd      = 1; /* as a connected pad has one */
      pad->buttons = ((uint64_t)bv_rand() << 32) | bv_rand();
      for (i = 0; i < NUM_HATS; i++)
      {
         pad->hats[i][0] = (int8_t)((int)(bv_rand() % 3) - 1);
         pad->hats[i][1] = (int8_t)((int)(bv_rand() % 3) - 1);
      }
      for (i = 0; i < NUM_AXES; i++)
      {
         unsigned r = bv_rand() % 8;
         pad->axes[i]        = (r == 0) ? 0
                             : (r == 1) ? 0x7fff
                             : (r == 2) ? -0x7fff
                             : (r == 3) ? (int16_t)-0x8000
                             : (int16_t)bv_rand();
         pad->neg_trigger[i] = (bv_rand() & 1) != 0;
      }
      bad += bv_compare(&udev_joypad, n % 3);
   }

   if (bad)
   {
      printf("FAIL udev_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS udev_checked_test: 4000 random pads; what the bridge would"
         " read is what the driver says\n");
   return 0;
}
