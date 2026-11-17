/* The PlayStation 4 joypad driver, held to what the snapshot bridge assumes of a
 * driver it serves (see bridge_view.h). Built on the host against the
 * platform's stand-in headers (tools/platform_stubs) with the SDK calls
 * its poll makes answered here, and its pad state filled by hand as that
 * poll fills it. */
#include "../../../input/drivers_joypad/ps4_joypad.c"

#include "bridge_view.h"

/* the SDK's, as the driver's init and poll reach for them; never run */
OrbisPadConfig *orbisPadGetConf(void) { return NULL; }
int scePadInit(void) { return 0; }
int scePadOpen(SceUserServiceUserId user, int type, int index, uint8_t *param)
{ (void)user; (void)type; (void)index; (void)param; return -1; }
int scePadReadState(int handle, ScePadData *data)
{ (void)handle; memset(data, 0, sizeof(*data)); return 0; }
int sceUserServiceGetLoginUserIdList(SceUserServiceLoginUserIdList *list)
{ (void)list; return -1; }

int main(void)
{
   unsigned n, bad = 0;

   for (n = 0; n < 4000; n++)
   {
      /* as many pads as the platform keeps, up to three */
      unsigned pad   = n % ((DEFAULT_MAX_PADS < 3) ? DEFAULT_MAX_PADS : 3);
      bv_fill_view(&ps4_pads[pad], 64, 4);
      bad += bv_compare(&ps4_joypad, pad);
      bad += bv_compare_view(&ps4_joypad, pad);
   }

   if (bad)
   {
      printf("FAIL ps4_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS ps4_checked_test: 4000 random pads; what the bridge"
         " would read is what the driver says\n");
   return 0;
}
