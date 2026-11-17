/* The parallel port joypad driver, held to what the snapshot bridge
 * assumes of a driver it serves (see bridge_view.h). The driver source
 * is included for its pad table, which is filled here by hand: no port
 * is opened. */
#include "../../../input/drivers_joypad/parport_joypad.c"

#include "bridge_view.h"

int main(void)
{
   unsigned n, i, bad = 0;

   for (n = 0; n < 4000; n++)
   {
      struct parport_joypad *pad = &parport_pads[n % 3];

      pad->fd      = 1;
      /* the thirteen the poll fills */
      bv_fill_view(&pad->view, PARPORT_NUM_BUTTONS, 0);
      bad += bv_compare(&parport_joypad, n % 3);
      bad += bv_compare_view(&parport_joypad, n % 3);
   }

   /* a port button() reads none of has no buttons, and no view */
   {
      input_bits_t bits;
      parport_pads[DEFAULT_MAX_PADS].view.buttons.data[0] = 0xffffffff;
      if (parport_joypad.view(DEFAULT_MAX_PADS))
         bad++;
      memset(&bits, 0xff, sizeof(bits));
      parport_joypad_get_buttons(DEFAULT_MAX_PADS, &bits);
      for (i = 0; i < 256; i++)
         if (BIT256_GET(bits, i))
            bad++;
   }

   if (bad)
   {
      printf("FAIL parport_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS parport_checked_test: 4000 random pads; what the bridge would"
         " read is what the driver says\n");
   return 0;
}
