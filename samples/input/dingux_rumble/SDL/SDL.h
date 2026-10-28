/* What of SDL 1.2 the Dingux joypad driver uses, stood in for: the
 * test answers the calls, and builds where no SDL 1.2 is installed. */
#ifndef DINGUX_RUMBLE_FAKE_SDL_H
#define DINGUX_RUMBLE_FAKE_SDL_H

#include <stdint.h>

#define SDL_INIT_JOYSTICK 0x00000200

typedef enum
{
   SDLK_BACKSPACE = 8, SDLK_TAB = 9, SDLK_RETURN = 13, SDLK_ESCAPE = 27,
   SDLK_SPACE = 32, SDLK_KP_PERIOD = 266, SDLK_KP_DIVIDE = 267,
   SDLK_UP = 273, SDLK_DOWN = 274, SDLK_RIGHT = 275, SDLK_LEFT = 276,
   SDLK_HOME = 278, SDLK_END = 279, SDLK_PAGEUP = 280, SDLK_PAGEDOWN = 281,
   SDLK_RSHIFT = 303, SDLK_LSHIFT = 304, SDLK_RCTRL = 305, SDLK_LCTRL = 306,
   SDLK_RALT = 307, SDLK_LALT = 308
} SDLKey;

enum { SDL_KEYDOWN = 2, SDL_KEYUP = 3 };

typedef struct { SDLKey sym; } SDL_keysym;
typedef struct { uint8_t type; SDL_keysym keysym; } SDL_KeyboardEvent;
typedef union { uint8_t type; SDL_KeyboardEvent key; } SDL_Event;
typedef struct SDL_Joystick SDL_Joystick;

uint32_t SDL_WasInit(uint32_t flags);
int SDL_Init(uint32_t flags);
int SDL_InitSubSystem(uint32_t flags);
int SDL_NumJoysticks(void);
SDL_Joystick *SDL_JoystickOpen(int index);
int SDL_JoystickNumAxes(SDL_Joystick *joystick);
int16_t SDL_JoystickGetAxis(SDL_Joystick *joystick, int axis);
void SDL_JoystickUpdate(void);
void SDL_JoystickClose(SDL_Joystick *joystick);
int SDL_PollEvent(SDL_Event *event);

#endif
