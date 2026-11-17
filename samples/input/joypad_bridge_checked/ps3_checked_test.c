/* The PlayStation 3 joypad driver, held to what the snapshot bridge assumes of a
 * driver it serves (see bridge_view.h). Built on the host against the
 * platform's stand-in headers (tools/platform_stubs) with the SDK calls
 * its poll makes answered here, and its pad state filled by hand as that
 * poll fills it. */
/* what griffin includes ahead of it, through ps3_input.c */
#include <defines/ps3_defines.h>
#include "../../../input/input_driver.h"

#include "../../../input/drivers_joypad/ps3_joypad.c"

#include "bridge_view.h"

/* the SDK's, as the driver's init and poll reach for them; never run */
s32 ioPadInit(u32 max) { (void)max; return 0; }
s32 ioPadEnd(void) { return 0; }
s32 ioPadGetInfo2(padInfo2 *info) { (void)info; return 0; }
s32 ioPadGetData(u32 port, padData *data) { (void)port; (void)data; return 0; }
u32 ioPadSetActDirect(u32 port, padActParam *act) { (void)port; (void)act; return 0; }

int main(void)
{
   unsigned n, bad = 0;

   for (n = 0; n < 4000; n++)
   {
      /* as many pads as the platform keeps, up to three */
      unsigned pad   = n % ((DEFAULT_MAX_PADS < 3) ? DEFAULT_MAX_PADS : 3);
      bv_fill_view(&ps3_pads[pad], 64, 4);
      bad += bv_compare(&ps3_joypad, pad);
      bad += bv_compare_view(&ps3_joypad, pad);
   }

   if (bad)
   {
      printf("FAIL ps3_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS ps3_checked_test: 4000 random pads; what the bridge"
         " would read is what the driver says\n");
   return 0;
}
