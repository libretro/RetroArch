/* The Switch joypad driver, held to what the snapshot bridge assumes of
 * a driver it serves (see bridge_view.h). Built on the host against the
 * libnx stand-in header (tools/platform_stubs/libnx) with the calls its
 * poll makes answered here, and its pad state filled by hand as that
 * poll fills it. */
#include "../../../input/drivers_joypad/switch_joypad.c"

#include "bridge_view.h"

/* libnx's, and the frontend's, as the driver's init, poll and rumble
 * reach for them; never run */
void padConfigureInput(u32 max_players, u32 style_set)
{ (void)max_players; (void)style_set; }
void padInitialize(PadState *pad, int id) { (void)pad; (void)id; }
void padInitializeDefault(PadState *pad) { (void)pad; }
void padUpdate(PadState *pad) { (void)pad; }
bool padIsHandheld(const PadState *pad) { (void)pad; return false; }
bool padIsNpadActive(const PadState *pad, HidNpadIdType id)
{ (void)pad; (void)id; return false; }
bool padIsConnected(const PadState *pad) { (void)pad; return false; }
u64  padGetButtons(const PadState *pad) { (void)pad; return 0; }
HidAnalogStickState padGetStickPos(const PadState *pad, unsigned i)
{ HidAnalogStickState st; (void)pad; (void)i; memset(&st, 0, sizeof(st)); return st; }
void hidSetNpadJoyHoldType(int type) { (void)type; }
u32  hidGetNpadDeviceType(HidNpadIdType id) { (void)id; return 0; }
Result hidSetNpadJoyAssignmentModeSingleByDefault(HidNpadIdType id)
{ (void)id; return 0; }
Result hidSetNpadJoyAssignmentModeDual(HidNpadIdType id) { (void)id; return 0; }
Result hidMergeSingleJoyAsDualJoy(HidNpadIdType a, HidNpadIdType b)
{ (void)a; (void)b; return 0; }
Result hidInitializeVibrationDevices(HidVibrationDeviceHandle *handles,
      s32 total, HidNpadIdType id, u32 style)
{ (void)handles; (void)total; (void)id; (void)style; return 0; }
Result hidSendVibrationValues(const HidVibrationDeviceHandle *handles,
      const HidVibrationValue *values, s32 count)
{ (void)handles; (void)values; (void)count; return 0; }
unsigned input_config_get_split_joycon(unsigned port) { (void)port; return 0; }

int main(void)
{
   unsigned n, bad = 0;

   for (n = 0; n < 4000; n++)
   {
      unsigned pad      = n % ((DEFAULT_MAX_PADS < 3) ? DEFAULT_MAX_PADS : 3);
      bv_fill_view(&switch_pads[pad], 16, 4);
      bad += bv_compare(&switch_joypad, pad);
      bad += bv_compare_view(&switch_joypad, pad);
   }

   if (bad)
   {
      printf("FAIL switch_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS switch_checked_test: 4000 random pads; what the bridge would"
         " read is what the driver says\n");
   return 0;
}
