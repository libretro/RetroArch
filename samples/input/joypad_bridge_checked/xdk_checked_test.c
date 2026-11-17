/* The original Xbox joypad driver, held to what the snapshot bridge
 * assumes of a driver it serves (see bridge_view.h). Built on the host
 * with xdk_sdk/ naming what it reaches for of the XDK, which is not
 * public, and its pad state filled by hand as its poll fills it. */
#include <xtl.h>

#include "../../../input/drivers_joypad/xdk_joypad.c"

#include "bridge_view.h"

/* the XDK's, as the driver's init and poll reach for them; never run */
void XInitDevices(DWORD count, const XDEVICE_PREALLOC_TYPE *types)
{ (void)count; (void)types; }
int XGetDeviceChanges(void *type, DWORD *inserted, DWORD *removed)
{ (void)type; *inserted = 0; *removed = 0; return 0; }
HANDLE XInputOpen(void *type, DWORD port, DWORD slot,
      XINPUT_POLLING_PARAMETERS *params)
{ (void)type; (void)port; (void)slot; (void)params; return NULL; }
void XInputClose(HANDLE h) { (void)h; }
DWORD XInputGetState(HANDLE h, XINPUT_STATE *state)
{ (void)h; memset(state, 0, sizeof(*state)); return 0; }
DWORD XInputPoll(HANDLE h) { (void)h; return 0; }

int main(void)
{
   unsigned n, i, bad = 0;

   for (n = 0; n < 4000; n++)
   {
      unsigned pad        = n % ((DEFAULT_MAX_PADS < 3) ? DEFAULT_MAX_PADS : 3);
      XINPUT_GAMEPAD *gp  = &g_xinput_states[pad].xstate.Gamepad;
      SHORT *thumbs[4];

      gp->wButtons = (WORD)bv_rand();
      for (i = 0; i < 8; i++)
         gp->bAnalogButtons[i] = (BYTE)bv_rand();
      thumbs[0] = &gp->sThumbLX; thumbs[1] = &gp->sThumbLY;
      thumbs[2] = &gp->sThumbRX; thumbs[3] = &gp->sThumbRY;
      for (i = 0; i < 4; i++)
      {
         unsigned r = bv_rand() % 8;
         *thumbs[i] = (r == 0) ? 0 : (r == 1) ? 0x7fff : (r == 2) ? -0x7fff
                    : (r == 3) ? -0x8000 : (SHORT)bv_rand();
      }
      bad += bv_compare(&xdk_joypad, pad);
   }

   if (bad)
   {
      printf("FAIL xdk_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS xdk_checked_test: 4000 random pads; what the bridge would"
         " read is what the driver says\n");
   return 0;
}
