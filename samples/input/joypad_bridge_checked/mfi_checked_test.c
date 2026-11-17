/* The Apple GameController (MFi) joypad driver, held to what the
 * snapshot bridge assumes of a driver it serves (see bridge_view.h).
 *
 * The driver is Objective-C against Apple's frameworks, but what the
 * bridge reads - its button(), get_buttons(), axis() and state() - is
 * plain C over the views its poll fills. The Makefile lifts those
 * functions out of mfi_joypad.m as they stand into mfi_extract.inc, and
 * they are built here on the host over views filled by hand as that
 * poll fills them: 32 bits of buttons, the home button past the
 * RetroPad's sixteen, and six axes. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../../../input/input_driver.h"
#include "../../../input/include/input_pad_view.h"

/* As mfi_joypad.m has them */
static input_pad_view_t mfi_pads[MAX_USERS];

#include "mfi_extract.inc"

static bool mfi_query_pad(unsigned port) { return port < DEFAULT_MAX_PADS; }

static input_device_driver_t mfi_lifted;

#include "bridge_view.h"

int main(void)
{
   unsigned n, bad = 0;

   mfi_lifted.query_pad   = mfi_query_pad;
   mfi_lifted.button      = apple_gamecontroller_joypad_button;
   mfi_lifted.state       = apple_gamecontroller_joypad_state;
   mfi_lifted.get_buttons = apple_gamecontroller_joypad_get_buttons;
   mfi_lifted.axis        = apple_gamecontroller_joypad_axis;
   mfi_lifted.view        = apple_gamecontroller_joypad_view;
   mfi_lifted.ident       = "mfi";

   for (n = 0; n < 4000; n++)
   {
      unsigned pad     = n % 3;
      bv_fill_view(&mfi_pads[pad], 32, 6);
      bad += bv_compare(&mfi_lifted, pad);
      bad += bv_compare_view(&mfi_lifted, pad);
   }

   if (bad)
   {
      printf("FAIL mfi_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS mfi_checked_test: 4000 random pads; what the bridge would"
         " read is what the driver says\n");
   return 0;
}
