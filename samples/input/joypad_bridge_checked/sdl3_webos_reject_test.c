/* The SDL3 joypad driver as built for webOS, which turns away the
 * system's 0x9999:0x9999 pseudo-pad. A turned-away pad must leave its
 * slot free and closed, and the slot must keep the sensor state asked
 * of it, as a disconnect does. */
#define WEBOS
#include "../../../input/drivers_joypad/sdl3_joypad.c"

/* No video thread here: the driver pumps the queue itself. */
bool sdl3_pump_input_events(void) { SDL_PumpEvents(); return true; }

static SDL_JoystickID attach_pad(Uint16 vendor, Uint16 product)
{
   SDL_VirtualJoystickDesc desc;
   SDL_INIT_INTERFACE(&desc);
   desc.type       = SDL_JOYSTICK_TYPE_GAMEPAD;
   desc.vendor_id  = vendor;
   desc.product_id = product;
   desc.naxes      = 6;
   desc.nbuttons   = 15;
   return SDL_AttachVirtualJoystick(&desc);
}

int main(void)
{
   SDL_JoystickID rejected, kept;
   unsigned i, held = 0;

   SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

   if (!sdl3_joypad_init(NULL))
   {
      printf("FAIL sdl3_webos_reject_test: the driver did not start\n");
      return 1;
   }

   sdl3_joypad_set_sensor_state(0, RETRO_SENSOR_GYROSCOPE_ENABLE, 0);

   if (!(rejected = attach_pad(0x9999, 0x9999)))
   {
      printf("FAIL sdl3_webos_reject_test: SDL has no virtual joysticks (%s)\n", SDL_GetError());
      return 1;
   }
   sdl3_joypad_poll();

   for (i = 0; i < MAX_USERS; i++)
      if (sdl3_joypads[i].jid || sdl3_joypads[i].gamepad || sdl3_joypads[i].joypad)
         held++;
   if (held)
   {
      printf("FAIL sdl3_webos_reject_test: the turned-away pad holds %u slot(s)\n", held);
      return 1;
   }
   if (SDL_GetGamepadFromID(rejected) || SDL_GetJoystickFromID(rejected))
   {
      printf("FAIL sdl3_webos_reject_test: the turned-away pad was left open\n");
      return 1;
   }
   if (!sdl3_joypads[0].sensor_gyro)
   {
      printf("FAIL sdl3_webos_reject_test: turning a pad away lost slot 0's sensor state\n");
      return 1;
   }

   /* Gone from SDL too, so its player index is free for the next pad */
   SDL_DetachVirtualJoystick(rejected);
   sdl3_joypad_poll();

   if (!(kept = attach_pad(0x045e, 0x028e)))
   {
      printf("FAIL sdl3_webos_reject_test: SDL could not attach a second pad (%s)\n", SDL_GetError());
      return 1;
   }
   sdl3_joypad_poll();

   if (sdl3_joypads[0].jid != kept || !sdl3_joypads[0].gamepad)
   {
      printf("FAIL sdl3_webos_reject_test: the next pad did not take slot 0\n");
      return 1;
   }
   if (!sdl3_joypads[0].sensor_gyro)
   {
      printf("FAIL sdl3_webos_reject_test: slot 0's sensor state did not reach the next pad\n");
      return 1;
   }

   sdl3_joypad_destroy();
   printf("PASS sdl3_webos_reject_test: a turned-away pad leaves its slot free and closed,"
         " and the slot keeps its sensor state\n");
   return 0;
}
