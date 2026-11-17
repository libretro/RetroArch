/* The 3DS joypad driver, held to what the snapshot bridge assumes of a
 * driver it serves (see bridge_view.h). Built on the host against the
 * libctru stand-in headers (tools/platform_stubs/ctr) with the calls
 * its poll makes answered here, and its pad state filled by hand as
 * that poll fills it. */
#include "../../../input/drivers_joypad/ctr_joypad.c"

#include "bridge_view.h"

/* libctru's, as the driver's poll reaches for them; never run */
void hidScanInput(void) { }
u32  hidKeysHeld(void) { return 0; }
void hidTouchRead(touchPosition *pos) { memset(pos, 0, sizeof(*pos)); }
void hidCircleRead(circlePosition *pos) { memset(pos, 0, sizeof(*pos)); }
void irrstCstickRead(circlePosition *pos) { memset(pos, 0, sizeof(*pos)); }

int main(void)
{
   unsigned n, bad = 0;

   for (n = 0; n < 4000; n++)
   {
      unsigned pad = n % ((DEFAULT_MAX_PADS < 3) ? DEFAULT_MAX_PADS : 3);
      bv_fill_view(&ctr_pad, 32, 4);
      bad += bv_compare(&ctr_joypad, pad);
      bad += bv_compare_view(&ctr_joypad, pad);
   }

   if (bad)
   {
      printf("FAIL ctr_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS ctr_checked_test: 4000 random pads; what the bridge would"
         " read is what the driver says\n");
   return 0;
}
