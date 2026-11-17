/* The native GameCube/Wii joypad driver (os/gekko), held to what the
 * snapshot bridge assumes of a driver it serves (see bridge_view.h).
 * The driver source is included for its pad state, which is filled
 * here by hand as its poll fills it from the pads and Wiimotes. */
#include "../../../input/drivers_joypad/gekko_joypad.c"

#include "bridge_view.h"

/* what the driver's init, poll and rumble reach for; never run */
void gk_pad_init(void) { }
void gk_pad_read(gk_pad_t pads[GK_PAD_PORTS]) { (void)pads; }
void gk_pad_rumble(unsigned port, int on) { (void)port; (void)on; }
#ifdef HW_RVL
int  gk_wiimote_init(void) { return 0; }
int  gk_wiimote_read(unsigned slot, gk_wiimote_t *out)
{ (void)slot; (void)out; return 0; }
void gk_wiimote_rumble(unsigned slot, int on) { (void)slot; (void)on; }
#endif
void gk_power_set_callbacks(gk_button_fn on_power, gk_button_fn on_reset,
      void *data)
{ (void)on_power; (void)on_reset; (void)data; }
void input_driver_platform_request(enum input_platform_request req)
{ (void)req; }
void input_driver_set_platform_menu_button(bool held) { (void)held; }
#ifdef HW_RVL
bool video_driver_get_viewport_info(struct video_viewport *vp)
{ (void)vp; return false; }
#endif

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
      printf("FAIL gekko_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS gekko_checked_test: 4000 random pads; what the bridge would"
         " read is what the driver says\n");
   return 0;
}
