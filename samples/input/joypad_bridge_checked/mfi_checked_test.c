/* The Apple GameController (MFi) joypad driver, held to what the
 * snapshot bridge assumes of a driver it serves (see bridge_view.h).
 *
 * The driver is Objective-C against Apple's frameworks, but what the
 * bridge reads - its button(), get_buttons(), axis() and state() - is
 * plain C over the two arrays its poll fills. The Makefile lifts those
 * four functions out of mfi_joypad.m as they stand into mfi_extract.inc,
 * and they are built here on the host over arrays filled by hand as
 * that poll fills them: 32 bits of buttons, the home button past the
 * RetroPad's sixteen, and six axes. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../../../input/input_driver.h"

/* As mfi_joypad.m has them */
#define MAX_MFI_AXES 6
static uint32_t mfi_buttons[MAX_USERS];
static int16_t  mfi_axes[MAX_USERS][MAX_MFI_AXES];

#include "mfi_extract.inc"

static bool mfi_query_pad(unsigned port) { return port < DEFAULT_MAX_PADS; }

static input_device_driver_t mfi_lifted;

#include "bridge_view.h"

int main(void)
{
   unsigned n, i, bad = 0;

   mfi_lifted.query_pad   = mfi_query_pad;
   mfi_lifted.button      = apple_gamecontroller_joypad_button;
   mfi_lifted.state       = apple_gamecontroller_joypad_state;
   mfi_lifted.get_buttons = apple_gamecontroller_joypad_get_buttons;
   mfi_lifted.axis        = apple_gamecontroller_joypad_axis;
   mfi_lifted.ident       = "mfi";

   for (n = 0; n < 4000; n++)
   {
      unsigned pad     = n % 3;
      mfi_buttons[pad] = bv_rand();
      for (i = 0; i < MAX_MFI_AXES; i++)
      {
         unsigned r = bv_rand() % 8;
         mfi_axes[pad][i] = (r == 0) ? 0
            : (r == 1) ? 0x7fff
            : (r == 2) ? -0x7fff
            : (r == 3) ? -0x8000
            : (int16_t)bv_rand();
      }
      bad += bv_compare(&mfi_lifted, pad);
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
