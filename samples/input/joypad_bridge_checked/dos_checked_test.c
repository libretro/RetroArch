/* The DOS joypad driver, held to what the snapshot bridge assumes of a
 * driver it serves (see bridge_view.h). It reads no pad yet, so every
 * answer is none; the test holds it to that, and to get_buttons()
 * saying so over whatever it is handed. */
#include "../../../input/drivers_joypad/dos_joypad.c"

#include "bridge_view.h"

int main(void)
{
   unsigned n, bad = 0;

   for (n = 0; n < 4000; n++)
   {
      input_bits_t bits;
      unsigned pad = n % ((DEFAULT_MAX_PADS < 3) ? DEFAULT_MAX_PADS : 3);
      memset(&bits, 0xff, sizeof(bits));
      dos_joypad.get_buttons(pad, &bits);
      if (BIT256_GET(bits, n % 256))
         bad++;
      bad += bv_compare(&dos_joypad, pad);
      bad += bv_compare_view(&dos_joypad, pad);
   }

   if (bad)
   {
      printf("FAIL dos_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS dos_checked_test: 4000 random pads; what the bridge would"
         " read is what the driver says\n");
   return 0;
}
