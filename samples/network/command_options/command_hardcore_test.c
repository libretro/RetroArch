/* Input held from a command (SET_INPUT, a RUN_FRAMES press) under
 * achievements hardcore mode, which forbids it: what was held before
 * hardcore began does not reach the core, and is let go at the next
 * poll, so it is not there again when hardcore ends. The real
 * input_driver.c, with what it reaches of the rest stood in for. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "../../../configuration.h"
#include "../../../command.h"
#include "../../../retroarch.h"
#include "../../../runloop.h"
#include "../../../verbosity.h"
#include "../../../gfx/video_driver.h"
#include "../../../input/input_driver.h"
#include "../../../network/netplay/netplay.h"
#include "../../../cheevos/cheevos.h"

static settings_t      settings;
static runloop_state_t runloop_st;
static bool            hardcore;

bool rcheevos_hardcore_active(void) { return hardcore; }
settings_t *config_get_ptr(void) { return &settings; }
runloop_state_t *runloop_state_get_ptr(void) { return &runloop_st; }
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
bool command_event(enum event_command cmd, void *data)
{ (void)cmd; (void)data; return false; }
bool command_interfaces_held(void) { return false; }
bool netplay_driver_ctl(enum rarch_netplay_ctl_state state, void *data)
{ (void)state; (void)data; return false; }
bool retroarch_ctl(enum rarch_ctl_state state, void *data)
{ (void)state; (void)data; return false; }
bool video_driver_has_focus(void) { return true; }
#if defined(_WIN32)
void win32_text_entry(bool active) { (void)active; }
#endif
bool video_driver_headset_focused(void) { return false; }
bool video_driver_get_viewport_info(struct video_viewport *vp)
{ (void)vp; return false; }
bool video_driver_translate_coord_viewport(struct video_viewport *vp,
      int x, int y, uint32_t *pos, uint32_t *screen_pos, bool oob)
{ (void)vp; (void)x; (void)y; (void)pos; (void)screen_pos; (void)oob;
  return false; }

int main(void)
{
   unsigned failures            = 0;
   input_driver_state_t *st     = input_state_get_ptr();
   const unsigned x             = RETRO_DEVICE_ID_JOYPAD_X;
   unsigned p;

   /* no physical port mapped to any core port: only the hold is read */
   for (p = 0; p < MAX_USERS; p++)
      settings.uints.input_remap_port_map[p][0] = MAX_USERS;
   settings.uints.input_max_users = 1;

   input_driver_command_hold(0, x, 1);
   input_driver_command_hold(0, 17, 1234);   /* left stick Y */
   if (input_driver_state_wrapper(0, RETRO_DEVICE_JOYPAD, 0, x) != 1)
   {
      puts("FAIL: a held button does not reach the core");
      failures++;
   }

   /* hardcore begins: the hold no longer reaches the core */
   hardcore = true;
   if (     input_driver_state_wrapper(0, RETRO_DEVICE_JOYPAD, 0, x)
         || input_driver_state_wrapper(0, RETRO_DEVICE_JOYPAD, 0,
               RETRO_DEVICE_ID_JOYPAD_MASK)
         || input_driver_state_wrapper(0, RETRO_DEVICE_ANALOG,
               RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y))
   {
      puts("FAIL: a hold made before hardcore reaches the core under it");
      failures++;
   }

   /* ... and is let go at the next poll */
   input_driver_poll();
   if (st->cmd_input)
   {
      puts("FAIL: the poll under hardcore did not let the hold go");
      failures++;
   }

   /* hardcore ends: nothing held comes back */
   hardcore = false;
   if (input_driver_state_wrapper(0, RETRO_DEVICE_JOYPAD, 0, x))
   {
      puts("FAIL: a hold from before hardcore came back after it");
      failures++;
   }

   if (failures)
      return 1;
   puts("command_hardcore: PASS");
   return 0;
}
