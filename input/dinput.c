/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "SDL.h"
#include "SDL_syswm.h"
#include <stdbool.h>
#include "ssnes_dinput.h"
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

struct sdl_dinput_map
{
   int sdl;
   int di;
};

static const struct sdl_dinput_map key_map[] = {
   { SDLK_a, DIK_A },
   { SDLK_b, DIK_B },
   { SDLK_c, DIK_C },
   { SDLK_d, DIK_D },
   { SDLK_e, DIK_E },
   { SDLK_f, DIK_F },
   { SDLK_g, DIK_G },
   { SDLK_h, DIK_H },
   { SDLK_i, DIK_I },
   { SDLK_j, DIK_J },
   { SDLK_k, DIK_K },
   { SDLK_l, DIK_L },
   { SDLK_m, DIK_M },
   { SDLK_n, DIK_N },
   { SDLK_o, DIK_O },
   { SDLK_p, DIK_P },
   { SDLK_q, DIK_Q },
   { SDLK_r, DIK_R },
   { SDLK_s, DIK_S },
   { SDLK_t, DIK_T },
   { SDLK_u, DIK_U },
   { SDLK_v, DIK_V },
   { SDLK_w, DIK_W },
   { SDLK_x, DIK_X },
   { SDLK_y, DIK_Y },
   { SDLK_z, DIK_Z },
   { SDLK_LEFT, DIK_LEFT },
   { SDLK_RIGHT, DIK_RIGHT },
   { SDLK_UP, DIK_UP },
   { SDLK_DOWN, DIK_DOWN },
   { SDLK_ESCAPE, DIK_ESCAPE },
   { SDLK_RETURN, DIK_RETURN },
   { SDLK_BACKSPACE, DIK_BACKSPACE },
};

static void sdl_dinput_free(void *data)
{
   sdl_dinput_t *di = data;
   if (di)
   {
      if (di->keyboard)
      {
         IDirectInputDevice8_Unacquire(di->keyboard);
         IDirectInputDevice8_Release(di->keyboard);
      }

      for (unsigned i = 0; i < MAX_PLAYERS; i++)
      {
         if (di->joypad[i])
         {
            IDirectInputDevice8_Unacquire(di->joypad[i]);
            IDirectInputDevice8_Release(di->joypad[i]);
         }
      }

      if (di->ctx)
         IDirectInput8_Release(di->ctx);

      free(di);
   }
}

static BOOL CALLBACK enum_axes_cb(const DIDEVICEOBJECTINSTANCE *inst, void *p)
{
   IDirectInputDevice8 *joypad = p;

   DIPROPRANGE range;
   memset(&range, 0, sizeof(range));
   range.diph.dwSize = sizeof(DIPROPRANGE);
   range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
   range.diph.dwHow = DIPH_BYID;
   range.diph.dwObj = inst->dwType;
   range.lMin = -32768;
   range.lMax = 32767;
   IDirectInputDevice8_SetProperty(joypad, DIPROP_RANGE, &range.diph);

   return DIENUM_CONTINUE;
}

static BOOL CALLBACK enum_joypad_cb(const DIDEVICEINSTANCE *inst, void *p)
{
   sdl_dinput_t *di = p;

   unsigned active = 0;
   unsigned n;
   for (n = 0; n < MAX_PLAYERS; n++)
   {
      if (!di->joypad[n] && g_settings.input.joypad_map[n] == di->joypad_cnt)
         break;

      if (di->joypad[n])
         active++;
   }

   if (active == MAX_PLAYERS) return DIENUM_STOP;

   if (FAILED(IDirectInput8_CreateDevice(di->ctx, &inst->guidInstance, &di->joypad[n], NULL)))
      return DIENUM_CONTINUE;

   di->joypad_cnt++;

   IDirectInputDevice8_SetDataFormat(di->joypad[n], &c_dfDIJoystick2);
   IDirectInputDevice8_SetCooperativeLevel(di->joypad[n], di->hWnd,
         DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);

   IDirectInputDevice8_EnumObjects(di->joypad[n], enum_axes_cb, 
         di->joypad[n], DIDFT_ABSAXIS);

   return DIENUM_CONTINUE; 
}

static void *sdl_dinput_init(void)
{
   sdl_dinput_t *di = calloc(1, sizeof(*di));
   if (!di)
      return NULL;

   CoInitialize(NULL);

   SDL_SysWMinfo info;
   if (!SDL_GetWMInfo(&info))
   {
      SSNES_ERR("Failed to get SysWM info.\n");
      goto error;
   }
   di->hWnd = info.window;

   if (FAILED(DirectInput8Create(
      GetModuleHandle(NULL), 0x0800, &IID_IDirectInput8,
      (void**)&di->ctx, NULL)))
   {
      SSNES_ERR("Failed to init DirectInput.\n");
      goto error;
   }

   if (FAILED(IDirectInput8_CreateDevice(di->ctx, &GUID_SysKeyboard, &di->keyboard, NULL)))
   {
      SSNES_ERR("Failed to init DirectInput keyboard.\n");
      goto error;
   }
   
   IDirectInputDevice8_SetDataFormat(di->keyboard, &c_dfDIKeyboard);
   IDirectInputDevice8_SetCooperativeLevel(di->keyboard, di->hWnd,
         DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);

   if (FAILED(IDirectInputDevice8_Acquire(di->keyboard)))
   {
      SSNES_ERR("Failed to acquire keyboard.\n");
      goto error;
   }

   IDirectInput8_EnumDevices(di->ctx, DI8DEVCLASS_GAMECTRL, enum_joypad_cb, di, DIEDFL_ATTACHEDONLY);

   return di;

error:
   sdl_dinput_free(di);
   return NULL;
}

static void sdl_poll_mouse(sdl_dinput_t *di)
{
   int _x, _y;
   Uint8 btn = SDL_GetRelativeMouseState(&_x, &_y);
   di->mouse_x = _x;
   di->mouse_y = _y;
   di->mouse_l = SDL_BUTTON(SDL_BUTTON_LEFT) & btn ? 1 : 0;
   di->mouse_r = SDL_BUTTON(SDL_BUTTON_RIGHT) & btn ? 1 : 0;
   di->mouse_m = SDL_BUTTON(SDL_BUTTON_MIDDLE) & btn ? 1 : 0;
}

static void sdl_poll(sdl_dinput_t *di)
{
   SDL_PumpEvents();
   sdl_poll_mouse(di);

   SDL_Event event;
   while (SDL_PollEvent(&event))
   {
      switch (event.type)
      {
         case SDL_QUIT:
            if (di->quitting)
            {
               *di->quitting = true;
               return;
            }
            break;

         case SDL_VIDEORESIZE:
            if (di->should_resize)
            {
               *di->new_width = event.resize.w;
               *di->new_height = event.resize.h;
               *di->should_resize = true;
            }
            break;

         default:
            break;
      }
   }
}

static bool dinput_joykey_pressed(sdl_dinput_t *di, int port_num, uint16_t joykey)
{
   if (joykey == NO_BTN)
      return false;

   // Check hat.
   if (GET_HAT_DIR(joykey))
   {
      unsigned hat = GET_HAT(joykey);
      
      unsigned elems = sizeof(di->joy_state[0].rgdwPOV) / sizeof(di->joy_state[0].rgdwPOV[0]);
      if (hat >= elems)
         return false;

      unsigned pov = di->joy_state[port_num].rgdwPOV[hat];

      // Magic numbers I'm not sure where originate from.
      if (pov < 36000)
      {
         switch (GET_HAT_DIR(joykey))
         {
            case HAT_UP_MASK:
               return (pov >= 31500) || (pov <= 4500);
            case HAT_RIGHT_MASK:
               return (pov >= 4500) && (pov <= 13500);
            case HAT_DOWN_MASK:
               return (pov >= 13500) && (pov <= 22500);
            case HAT_LEFT_MASK:
               return (pov >= 22500) && (pov <= 31500);
         }
      }

      return false;
   }
   else
   {
      unsigned elems = sizeof(di->joy_state[port_num].rgbButtons) / sizeof(di->joy_state[port_num].rgbButtons[0]);

      if (joykey < elems)
         return di->joy_state[port_num].rgbButtons[joykey];
   }

   return false;
}

static bool dinput_joyaxis_pressed(sdl_dinput_t *di, int port_num, uint32_t joyaxis)
{
   if (joyaxis == AXIS_NONE)
      return false;

   int min = -32678 * g_settings.input.axis_threshold;
   int max = 32677 * g_settings.input.axis_threshold;

   switch (AXIS_NEG_GET(joyaxis))
   {
      case 0:
         return di->joy_state[port_num].lX <= min;
      case 1:
         return di->joy_state[port_num].lY <= min;
      case 2:
         return di->joy_state[port_num].lZ <= min;
      case 3:
         return di->joy_state[port_num].lRx <= min;
      case 4:
         return di->joy_state[port_num].lRy <= min;
      case 5:
         return di->joy_state[port_num].lRz <= min;
   }

   switch (AXIS_POS_GET(joyaxis))
   {
      case 0:
         return di->joy_state[port_num].lX >= max;
      case 1:
         return di->joy_state[port_num].lY >= max;
      case 2:
         return di->joy_state[port_num].lZ >= max;
      case 3:
         return di->joy_state[port_num].lRx >= max;
      case 4:
         return di->joy_state[port_num].lRy >= max;
      case 5:
         return di->joy_state[port_num].lRz >= max;
   }

   return false;
}

static int16_t sdl_mouse_device_state(sdl_dinput_t *sdl, bool port, unsigned id)
{
   // Might implement support for joypad mapping later.
   (void)port;
   switch (id)
   {
      case SNES_DEVICE_ID_MOUSE_LEFT:
         return sdl->mouse_l;
      case SNES_DEVICE_ID_MOUSE_RIGHT:
         return sdl->mouse_r;
      case SNES_DEVICE_ID_MOUSE_X:
         return sdl->mouse_x;
      case SNES_DEVICE_ID_MOUSE_Y:
         return sdl->mouse_y;
      default:
         return 0;
   }
}

// TODO: Missing some controllers, but hey :)
static int16_t sdl_scope_device_state(sdl_dinput_t *sdl, unsigned id)
{
   switch (id)
   {
      case SNES_DEVICE_ID_SUPER_SCOPE_X:
         return sdl->mouse_x;
      case SNES_DEVICE_ID_SUPER_SCOPE_Y:
         return sdl->mouse_y;
      case SNES_DEVICE_ID_SUPER_SCOPE_TRIGGER:
         return sdl->mouse_l;
      case SNES_DEVICE_ID_SUPER_SCOPE_CURSOR:
         return sdl->mouse_m;
      case SNES_DEVICE_ID_SUPER_SCOPE_TURBO:
         return sdl->mouse_r;
      default:
         return 0;
   }
}

// TODO: Support two players.
static int16_t sdl_justifier_device_state(sdl_dinput_t *sdl, unsigned index, unsigned id)
{
   if (index == 0)
   {
      switch (id)
      {
         case SNES_DEVICE_ID_JUSTIFIER_X:
            return sdl->mouse_x;
         case SNES_DEVICE_ID_JUSTIFIER_Y:
            return sdl->mouse_y;
         case SNES_DEVICE_ID_JUSTIFIER_TRIGGER:
            return sdl->mouse_l;
         case SNES_DEVICE_ID_JUSTIFIER_START:
            return sdl->mouse_r;
         default:
            return 0;
      }
   }
   else
      return 0;
}

static bool dinput_key_pressed(sdl_dinput_t *di, int key)
{
   for (unsigned i = 0; i < sizeof(key_map) / sizeof(struct sdl_dinput_map); i++)
   {
      if (key == key_map[i].sdl)
         return di->di_state[key_map[i].di] & 0x80;
   }

   return false;
}

static bool dinput_is_pressed(sdl_dinput_t *di, int port_num, const struct snes_keybind *key)
{
   if (dinput_key_pressed(di, key->key))
      return true;
   if (di->joypad[port_num] == NULL)
      return false;
   if (dinput_joykey_pressed(di, port_num, key->joykey))
      return true;
   if (dinput_joyaxis_pressed(di, port_num, key->joyaxis))
      return true;

   return false;
}

static int16_t joypad_device_state(sdl_dinput_t *di, const struct snes_keybind **binds, int port_num, int id)
{
   const struct snes_keybind *snes_keybinds = binds[port_num];

   for (int i = 0; snes_keybinds[i].id != -1; i++)
   {
      if (snes_keybinds[i].id == id)
         return dinput_is_pressed(di, port_num, &snes_keybinds[i]) ? 1 : 0;
   }

   return 0;
}

static bool sdl_dinput_pressed(void *data, int key)
{
   const struct snes_keybind *binds = g_settings.input.binds[0];
   for (int i = 0; binds[i].id != -1; i++)
   {
      if (binds[i].key == key)
         return dinput_is_pressed(data, 0, &binds[i]);
   }
   return false;
}

static int16_t sdl_dinput_state(void *data, const struct snes_keybind **binds, bool port, unsigned device, unsigned index, unsigned id)
{
   switch (device)
   {
      case SNES_DEVICE_JOYPAD:
         return joypad_device_state(data, binds, (port == SNES_PORT_1) ? 0 : 1, id);
      case SNES_DEVICE_MULTITAP:
         return joypad_device_state(data, binds, (port == SNES_PORT_2) ? 1 + index : 0, id);

      case SNES_DEVICE_MOUSE:
         return sdl_mouse_device_state(data, port, id);
      case SNES_DEVICE_SUPER_SCOPE:
         return sdl_scope_device_state(data, id);
      case SNES_DEVICE_JUSTIFIER:
      case SNES_DEVICE_JUSTIFIERS:
         return sdl_justifier_device_state(data, index, id);

      default:
         return 0;
   }
}

static void sdl_dinput_poll(void *data)
{
   sdl_dinput_t *di = data;

   sdl_poll(di); // Poll SDL specific things needed for SDL/GL driver.

   // Keyboard
   memset(di->di_state, 0, sizeof(di->di_state));
   if (FAILED(IDirectInputDevice8_GetDeviceState(di->keyboard, sizeof(di->di_state), di->di_state)))
   {
      IDirectInputDevice8_Acquire(di->keyboard);
      if (FAILED(IDirectInputDevice8_GetDeviceState(di->keyboard, sizeof(di->di_state), di->di_state)))
         memset(di->di_state, 0, sizeof(di->di_state));
   }

   memset(di->joy_state, 0, sizeof(di->joy_state));
   for (unsigned i = 0; i < MAX_PLAYERS; i++)
   {
      if (di->joypad[i])
      {
         if (FAILED(IDirectInputDevice8_Poll(di->joypad[i])))
         {
            IDirectInputDevice8_Acquire(di->joypad[i]);
            continue;
         }

         IDirectInputDevice8_GetDeviceState(di->joypad[i], sizeof(DIJOYSTATE2), &di->joy_state[i]);
      }
   }
}

const input_driver_t input_dinput = {
   .init = sdl_dinput_init,
   .poll = sdl_dinput_poll,
   .input_state = sdl_dinput_state,
   .key_pressed = sdl_dinput_pressed,
   .free = sdl_dinput_free,
   .ident = "dinput"
};
