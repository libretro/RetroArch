/* The PlayStation 2 joypad driver, held to what the snapshot bridge assumes of a
 * driver it serves (see bridge_view.h). Built on the host against the
 * platform's stand-in headers (tools/platform_stubs) with the SDK calls
 * its poll makes answered here, and its pad state filled by hand as that
 * poll fills it. */
#include "../../../input/drivers_joypad/ps2_joypad.c"

#include "bridge_view.h"

/* the SDK's, as the driver's init and poll reach for them; never run */
int padPortOpen(int port, int slot, void *area) { (void)port; (void)slot; (void)area; return 0; }
int padPortClose(int port, int slot) { (void)port; (void)slot; return 0; }
unsigned char padRead(int port, int slot, struct padButtonStatus *data)
{ (void)port; (void)slot; (void)data; return 0; }
int padGetState(int port, int slot) { (void)port; (void)slot; return 0; }
unsigned char padGetReqState(int port, int slot) { (void)port; (void)slot; return 0; }
int padInfoMode(int port, int slot, int info, int index)
{ (void)port; (void)slot; (void)info; (void)index; return 0; }
int padSetMainMode(int port, int slot, int mode, int lock)
{ (void)port; (void)slot; (void)mode; (void)lock; return 0; }
int mtapPortOpen(int port) { (void)port; return 0; }
int mtapPortClose(int port) { (void)port; return 0; }

int main(void)
{
   unsigned n, bad = 0;

   for (n = 0; n < 4000; n++)
   {
      /* as many pads as the platform keeps, up to three */
      unsigned pad   = n % ((DEFAULT_MAX_PADS < 3) ? DEFAULT_MAX_PADS : 3);
      bv_fill_view(&ps2_pads[pad], 64, 4);
      bad += bv_compare(&ps2_joypad, pad);
      bad += bv_compare_view(&ps2_joypad, pad);
   }

   if (bad)
   {
      printf("FAIL ps2_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS ps2_checked_test: 4000 random pads; what the bridge"
         " would read is what the driver says\n");
   return 0;
}
