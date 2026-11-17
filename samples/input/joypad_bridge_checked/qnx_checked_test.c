/* The QNX joypad driver, held to what the snapshot bridge assumes of a
 * driver it serves (see bridge_view.h). Its pads are the QNX input
 * driver's devices, as libscreen's events fill them in that driver's
 * poll; griffin builds the two together, and so does this, against the
 * QNX stand-in headers (tools/platform_stubs/qnx), with the devices
 * filled by hand. */
/* what griffin includes ahead of it */
#include "../../../input/input_keymaps.h"

#include "../../../input/drivers/qnx_input.c"
#include "../../../input/drivers_joypad/qnx_joypad.c"

#include "bridge_view.h"

static qnx_input_t test_qnx;
/* the frontend's: the input driver's data the joypad driver reads */
void *input_driver_current_data(void) { return &test_qnx; }

/* BPS's and libscreen's (qnx_sdk/), and the frontend's, as the input
 * driver's poll and init reach for them; never run */
screen_context_t screen_ctx;
int bps_get_event(bps_event_t **event, int timeout_ms)
{ (void)timeout_ms; *event = NULL; return BPS_SUCCESS; }
unsigned bps_event_get_code(bps_event_t *event) { (void)event; return 0; }
int bps_event_get_domain(bps_event_t *event) { (void)event; return 0; }
int screen_get_domain(void) { return -1; }
int navigator_get_domain(void) { return -2; }
screen_event_t screen_event_get_event(bps_event_t *event) { (void)event; return NULL; }
int navigator_event_get_syskey_key(bps_event_t *event) { (void)event; return 0; }
int navigator_event_get_window_state(bps_event_t *event) { (void)event; return 0; }
int screen_get_event_property_iv(screen_event_t ev, int prop, int *v)
{ (void)ev; (void)prop; *v = 0; return 0; }
int screen_get_event_property_pv(screen_event_t ev, int prop, void **v)
{ (void)ev; (void)prop; *v = NULL; return 0; }
int screen_get_device_property_iv(screen_device_t d, int prop, int *v)
{ (void)d; (void)prop; *v = 0; return 0; }
int screen_get_device_property_cv(screen_device_t d, int prop, int len, char *v)
{ (void)d; (void)prop; if (len > 0) *v = '\0'; return 0; }
int screen_get_context_property_iv(screen_context_t c, int prop, int *v)
{ (void)c; (void)prop; *v = 0; return 0; }
int screen_get_context_property_pv(screen_context_t c, int prop, void **v)
{ (void)c; (void)prop; *v = NULL; return 0; }
void input_driver_platform_request(enum input_platform_request req) { (void)req; }
bool input_driver_poll_viewport(struct video_viewport *vp) { (void)vp; return false; }
bool video_driver_translate_coord_viewport(struct video_viewport *vp,
      int x, int y, uint32_t *res_pos, uint32_t *res_screen_pos, bool clamp)
{ (void)vp; (void)x; (void)y; (void)res_pos; (void)res_screen_pos; (void)clamp; return false; }
void input_keyboard_event(bool down, unsigned code, uint32_t character,
      uint16_t mod, unsigned device)
{ (void)down; (void)code; (void)character; (void)mod; (void)device; }
void input_keymaps_init_keyboard_lut(const struct rarch_key_map *map) { (void)map; }
enum retro_key input_keymaps_translate_keysym_to_rk(unsigned sym)
{ (void)sym; return RETROK_UNKNOWN; }
enum retro_key rarch_keysym_lut[RETROK_LAST];
const struct rarch_key_map rarch_key_map_qnx[] = { { 0, RETROK_UNKNOWN } };

int main(void)
{
   unsigned n, i, bad = 0;

   for (n = 0; n < 4000; n++)
   {
      unsigned pad                 = n % ((DEFAULT_MAX_PADS < 3) ? DEFAULT_MAX_PADS : 3);
      qnx_input_device_t *device   = &test_qnx.devices[pad];

      device->buttons = (int)bv_rand();
      for (i = 0; i < 3; i++)
      {
         unsigned r = bv_rand() % 8;
         device->analog0[i] = (r == 0) ? 0 : (r == 1) ? 0x7fff
            : (r == 2) ? -0x7fff : (r == 3) ? -0x8000 : (int16_t)bv_rand();
         r = bv_rand() % 8;
         device->analog1[i] = (r == 0) ? 0 : (r == 1) ? 0x7fff
            : (r == 2) ? -0x7fff : (r == 3) ? -0x8000 : (int16_t)bv_rand();
      }
      /* as the gamepad event leaves it */
      qnx_controller_view(device);
      bad += bv_compare(&qnx_joypad, pad);
      bad += bv_compare_view(&qnx_joypad, pad);
   }

   if (bad)
   {
      printf("FAIL qnx_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS qnx_checked_test: 4000 random pads; what the bridge would"
         " read is what the driver says\n");
   return 0;
}
