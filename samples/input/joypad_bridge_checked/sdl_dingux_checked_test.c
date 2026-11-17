/* The OpenDingux joypad driver, held to what the snapshot bridge
 * assumes of a driver it serves (see bridge_view.h). Built as for the
 * devices with analog sticks; its pad state is filled here by hand as
 * its poll fills it from SDL's key events and stick, and SDL itself is
 * not linked. */
#include "../../../input/drivers_joypad/sdl_dingux_joypad.c"

#include "bridge_view.h"

/* what the driver's init and poll reach for; never run */
SDL_Joystick *SDL_JoystickOpen(int i) { (void)i; return NULL; }
void SDL_JoystickClose(SDL_Joystick *j) { (void)j; }
int SDL_JoystickNumAxes(SDL_Joystick *j) { (void)j; return 0; }
Sint16 SDL_JoystickGetAxis(SDL_Joystick *j, int a) { (void)j; (void)a; return 0; }
void SDL_JoystickUpdate(void) { }
int SDL_NumJoysticks(void) { return 0; }
int SDL_PollEvent(SDL_Event *e) { (void)e; return 0; }
int SDL_InitSubSystem(Uint32 f) { (void)f; return 0; }
void SDL_QuitSubSystem(Uint32 f) { (void)f; }
Uint32 SDL_WasInit(Uint32 f) { (void)f; return 0; }
int SDL_Init(Uint32 f) { (void)f; return 0; }
/* the frontend's; the menu button is pressed in the poll alone */
void input_driver_set_platform_menu_button(bool down) { (void)down; }

int main(void)
{
   unsigned n, bad = 0;

   for (n = 0; n < 4000; n++)
   {
      dingux_joypad.connected = true;
      bv_fill_view(&dingux_joypad.view, 16, 4);
      /* the one pad there is, and the port past it */
      bad += bv_compare(&sdl_dingux_joypad, 0);
      bad += bv_compare(&sdl_dingux_joypad, 1);
      bad += bv_compare_view(&sdl_dingux_joypad, 0);
      if (sdl_dingux_joypad.view(1))
         bad++;
   }

   if (bad)
   {
      printf("FAIL sdl_dingux_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS sdl_dingux_checked_test: 4000 random pad states; what the"
         " bridge would read is what the driver says\n");
   return 0;
}
