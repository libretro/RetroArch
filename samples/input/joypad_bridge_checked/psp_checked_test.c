/* The PSP and Vita joypad driver (the Vita's built against vitasdk's
 * headers, VITA_HEADERS), held to what the snapshot bridge assumes of a
 * driver it serves (see bridge_view.h). Built on the host against the
 * platform's stand-in headers (tools/platform_stubs) with the SDK calls
 * its poll makes answered here, and its pad state filled by hand as that
 * poll fills it. */
#include <stdarg.h>
#include <stdio.h>

#include "../../../input/drivers_joypad/psp_joypad.c"

#include "bridge_view.h"

/* the SDK's, as the driver's init and poll reach for them; never run.
 * The Vita's are the vitasdk headers' (VITA_HEADERS). */
#if defined(VITA)
int sceCtrlSetSamplingMode(SceCtrlPadInputMode mode) { (void)mode; return 0; }
int sceCtrlPeekBufferPositive(int port, SceCtrlData *pad_data, int count)
{ (void)port; memset(pad_data, 0, sizeof(*pad_data) * (count > 0 ? count : 0)); return 0; }
int sceCtrlPeekBufferPositiveExt2(int port, SceCtrlData *pad_data, int count)
{ (void)port; memset(pad_data, 0, sizeof(*pad_data) * (count > 0 ? count : 0)); return 0; }
int sceCtrlSetActuator(int port, const SceCtrlActuator *state)
{ (void)port; (void)state; return 0; }
int sceCtrlGetControllerPortInfo(SceCtrlPortInfo *info)
{ memset(info, 0, sizeof(*info)); return 0; }
int sceCtrlIsMultiControllerSupported(void) { return 0; }
int sceKernelGetModelForCDialog(void) { return 0; }
int sceTouchPeek(SceUInt32 port, SceTouchData *data, SceUInt32 n)
{ (void)port; memset(data, 0, sizeof(*data) * n); return 0; }
int sceTouchSetSamplingState(SceUInt32 port, SceTouchSamplingState state)
{ (void)port; (void)state; return 0; }
int sceCtrlSetSamplingModeExt(SceCtrlPadInputMode mode) { (void)mode; return 0; }
/* the Vita's printf (psp_defines.h): the verdict is printed with it */
int sceClibPrintf(const char *fmt, ...)
{
   int r;
   va_list ap;
   va_start(ap, fmt);
   r = vfprintf(stdout, fmt, ap);
   va_end(ap);
   return r;
}
/* the frontend's, as the Vita's poll asks for the rear touchpad */
bool input_config_get_backtouch_enable(void) { return false; }
bool input_config_get_backtouch_toggle(void) { return false; }
bool input_driver_native_keyboard_shown(void) { return false; }
#else
int sceCtrlSetSamplingCycle(int cycle) { (void)cycle; return 0; }
int sceCtrlSetSamplingMode(int mode) { (void)mode; return 0; }
int sceCtrlPeekBufferPositive(SceCtrlData *pad_data, int count)
{ memset(pad_data, 0, sizeof(*pad_data) * (count > 0 ? count : 0)); return 0; }
#endif

int main(void)
{
   unsigned n, bad = 0;

   for (n = 0; n < 4000; n++)
   {
      /* as many pads as the platform keeps, up to three */
      unsigned pad   = n % ((DEFAULT_MAX_PADS < 3) ? DEFAULT_MAX_PADS : 3);
      bv_fill_view(&psp_pads[pad], 64, 6);
      bad += bv_compare(&psp_joypad, pad);
      bad += bv_compare_view(&psp_joypad, pad);
   }

   if (bad)
   {
      printf("FAIL psp_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS psp_checked_test: 4000 random pads; what the bridge"
         " would read is what the driver says\n");
   return 0;
}
