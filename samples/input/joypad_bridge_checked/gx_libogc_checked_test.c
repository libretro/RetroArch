/* The libogc GameCube/Wii joypad driver, held to what the snapshot
 * bridge assumes of a driver it serves (see bridge_view.h). Built
 * against libogc's headers (LIBOGC_DIR) with the pad calls its poll
 * makes answered here, and its pad state filled by hand as that poll
 * fills it. */
#include "../../../input/drivers_joypad/gx_joypad_libogc.c"

#include "bridge_view.h"

/* libogc's, and the frontend's, as the poll and init reach for them;
 * never run */
u32 PAD_Init(void) { return 0; }
u32 PAD_ScanPads(void) { return 0; }
u16 PAD_ButtonsHeld(int pad) { (void)pad; return 0; }
s8 PAD_SubStickX(int pad) { (void)pad; return 0; }
s8 PAD_SubStickY(int pad) { (void)pad; return 0; }
s8 PAD_StickX(int pad) { (void)pad; return 0; }
s8 PAD_StickY(int pad) { (void)pad; return 0; }
u8 PAD_TriggerL(int pad) { (void)pad; return 0; }
u8 PAD_TriggerR(int pad) { (void)pad; return 0; }
s32 WPAD_Init(void) { return 0; }
s32 WPAD_ReadPending(s32 chan, WPADDataCallback datacb)
{ (void)chan; (void)datacb; return 0; }
s32 WPAD_SetDataFormat(s32 chan, s32 fmt) { (void)chan; (void)fmt; return 0; }
s32 WPAD_SetVRes(s32 chan, u32 xres, u32 yres)
{ (void)chan; (void)xres; (void)yres; return 0; }
s32 WPAD_Probe(s32 chan, u32 *type) { (void)chan; (void)type; return -1; }
WPADData *WPAD_Data(int chan) { (void)chan; return NULL; }
void WPAD_IR(int chan, struct ir_t *ir) { (void)chan; (void)ir; }
resetcallback SYS_SetResetCallback(resetcallback cb) { (void)cb; return NULL; }
powercallback SYS_SetPowerCallback(powercallback cb) { (void)cb; return NULL; }
void input_config_set_joypad_index(unsigned port, unsigned idx)
{ (void)port; (void)idx; }
void input_config_set_device_name(unsigned port, const char *name)
{ (void)port; (void)name; }
void input_driver_platform_request(enum input_platform_request req)
{ (void)req; }
void input_driver_set_platform_menu_button(bool held) { (void)held; }

int main(void)
{
   unsigned n, bad = 0;

   for (n = 0; n < 4000; n++)
   {
      /* as many pads as the platform keeps, up to three */
      unsigned pad   = n % ((DEFAULT_MAX_PADS < 3) ? DEFAULT_MAX_PADS : 3);
      bv_fill_view(&gx_pads[pad], 64, 4);
      bad += bv_compare(&gx_joypad, pad);
      bad += bv_compare_view(&gx_joypad, pad);
   }

   if (bad)
   {
      printf("FAIL gx_libogc_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS gx_libogc_checked_test: 4000 random pads; what the bridge"
         " would read is what the driver says\n");
   return 0;
}
