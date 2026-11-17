/* The SDL 1.2 joypad driver, held to what the snapshot bridge assumes
 * of a driver it serves (see bridge_view.h). SDL 1.2 has no virtual
 * joysticks, so the joystick calls the driver makes are answered here
 * from a table filled by hand, and SDL itself is not linked. */
#include "../../../input/drivers_joypad/sdl1_joypad.c"

#include "bridge_view.h"

#define T_BUTTONS 40
#define T_AXES    8
#define T_HATS    3

struct _SDL_Joystick
{
   Uint8  buttons[T_BUTTONS];
   Sint16 axes[T_AXES];
   Uint8  hats[T_HATS];
};

static struct _SDL_Joystick test_sticks[3];

Uint8 SDL_JoystickGetButton(SDL_Joystick *j, int b)
{ return (b >= 0 && b < T_BUTTONS) ? j->buttons[b] : 0; }
Sint16 SDL_JoystickGetAxis(SDL_Joystick *j, int a)
{ return (a >= 0 && a < T_AXES) ? j->axes[a] : 0; }
Uint8 SDL_JoystickGetHat(SDL_Joystick *j, int h)
{ return (h >= 0 && h < T_HATS) ? j->hats[h] : 0; }

/* what the driver's init, connect and poll reach for; never run */
SDL_Joystick *SDL_JoystickOpen(int i) { (void)i; return NULL; }
void SDL_JoystickClose(SDL_Joystick *j) { (void)j; }
const char *SDL_JoystickName(int i) { (void)i; return NULL; }
int SDL_JoystickNumAxes(SDL_Joystick *j) { (void)j; return 0; }
int SDL_JoystickNumButtons(SDL_Joystick *j) { (void)j; return 0; }
int SDL_JoystickNumHats(SDL_Joystick *j) { (void)j; return 0; }
void SDL_JoystickUpdate(void) { }
int SDL_NumJoysticks(void) { return 0; }
int SDL_Init(Uint32 f) { (void)f; return 0; }
int SDL_InitSubSystem(Uint32 f) { (void)f; return 0; }
Uint32 SDL_WasInit(Uint32 f) { (void)f; return 0; }
char *SDL_GetError(void) { return (char*)""; }

int main(void)
{
   static const Uint8 hats[] = { SDL_HAT_CENTERED, SDL_HAT_UP,
      SDL_HAT_RIGHT, SDL_HAT_DOWN, SDL_HAT_LEFT, SDL_HAT_RIGHTUP,
      SDL_HAT_RIGHTDOWN, SDL_HAT_LEFTUP, SDL_HAT_LEFTDOWN };
   unsigned n, i, bad = 0;

   for (n = 0; n < 4000; n++)
   {
      unsigned pad             = n % 3;
      struct _SDL_Joystick *j  = &test_sticks[pad];
      sdl1_joypad_t *p         = &sdl1_pads[pad];

      p->joypad      = j;
      /* fewer than the stick holds, as a pad may say */
      p->num_buttons = bv_rand() % (T_BUTTONS + 1);
      p->num_axes    = bv_rand() % (T_AXES + 1);
      p->num_hats    = bv_rand() % (T_HATS + 1);
      for (i = 0; i < T_BUTTONS; i++)
         j->buttons[i] = (Uint8)(bv_rand() & 1);
      for (i = 0; i < T_HATS; i++)
         j->hats[i]    = hats[bv_rand() % (sizeof(hats) / sizeof(hats[0]))];
      for (i = 0; i < T_AXES; i++)
      {
         unsigned r = bv_rand() % 8;
         j->axes[i] = (r == 0) ? 0
                    : (r == 1) ? 0x7fff
                    : (r == 2) ? -0x7fff
                    : (r == 3) ? -0x8000
                    : (Sint16)bv_rand();
      }
      bad += bv_compare(&sdl1_joypad, pad);
   }

   /* a slot with no joystick open, and a port past the slots, have no
    * buttons */
   {
      input_bits_t bits;
      sdl1_pads[1].joypad = NULL;
      memset(&bits, 0xff, sizeof(bits));
      sdl1_joypad_get_buttons(1, &bits);
      for (i = 0; i < 256; i++)
         if (BIT256_GET(bits, i))
            bad++;
      memset(&bits, 0xff, sizeof(bits));
      sdl1_joypad_get_buttons(MAX_USERS, &bits);
      for (i = 0; i < 256; i++)
         if (BIT256_GET(bits, i))
            bad++;
      if (sdl1_joypad_button(MAX_USERS, 0) || sdl1_joypad_axis(MAX_USERS, AXIS_POS(0)))
         bad++;
   }

   if (bad)
   {
      printf("FAIL sdl1_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS sdl1_checked_test: 4000 random pads; what the bridge would"
         " read is what the driver says\n");
   return 0;
}
