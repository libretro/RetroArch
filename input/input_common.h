/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef INPUT_COMMON_H__
#define INPUT_COMMON_H__

#include "../driver.h"
#include <stdint.h>

static inline void input_conv_analog_id_to_bind_id(unsigned index, unsigned id,
      unsigned *id_minus, unsigned *id_plus)
{
   switch ((index << 1) | id)
   {
      case (RETRO_DEVICE_INDEX_ANALOG_LEFT << 1) | RETRO_DEVICE_ID_ANALOG_X:
         *id_minus = RARCH_ANALOG_LEFT_X_MINUS;
         *id_plus  = RARCH_ANALOG_LEFT_X_PLUS;
         break;

      case (RETRO_DEVICE_INDEX_ANALOG_LEFT << 1) | RETRO_DEVICE_ID_ANALOG_Y:
         *id_minus = RARCH_ANALOG_LEFT_Y_MINUS;
         *id_plus  = RARCH_ANALOG_LEFT_Y_PLUS;
         break;

      case (RETRO_DEVICE_INDEX_ANALOG_RIGHT << 1) | RETRO_DEVICE_ID_ANALOG_X:
         *id_minus = RARCH_ANALOG_RIGHT_X_MINUS;
         *id_plus  = RARCH_ANALOG_RIGHT_X_PLUS;
         break;

      case (RETRO_DEVICE_INDEX_ANALOG_RIGHT << 1) | RETRO_DEVICE_ID_ANALOG_Y:
         *id_minus = RARCH_ANALOG_RIGHT_Y_MINUS;
         *id_plus  = RARCH_ANALOG_RIGHT_Y_PLUS;
         break;
   }
}

bool input_translate_coord_viewport(int mouse_x, int mouse_y,
      int16_t *res_x, int16_t *res_y, int16_t *res_screen_x, int16_t *res_screen_y);

#ifdef ANDROID
enum back_button_enums
{
   BACK_BUTTON_QUIT = 0,
   BACK_BUTTON_MENU_TOGGLE,
};
#endif

typedef struct rarch_joypad_driver
{
   bool (*init)(void);
   bool (*query_pad)(unsigned);
   void (*destroy)(void);
   bool (*button)(unsigned, uint16_t);
   int16_t (*axis)(unsigned, uint32_t);
   void (*poll)(void);

   const char *ident;
} rarch_joypad_driver_t;

const rarch_joypad_driver_t *input_joypad_find_driver(const char *ident);
const rarch_joypad_driver_t *input_joypad_init_first(void);

bool input_joypad_pressed(const rarch_joypad_driver_t *driver,
      unsigned port, const struct retro_keybind *key);

int16_t input_joypad_analog(const rarch_joypad_driver_t *driver,
      unsigned port, unsigned index, unsigned id, const struct retro_keybind *binds);

int16_t input_joypad_axis_raw(const rarch_joypad_driver_t *driver,
      unsigned joypad, unsigned axis);
bool input_joypad_button_raw(const rarch_joypad_driver_t *driver,
      unsigned joypad, unsigned button);
bool input_joypad_hat_raw(const rarch_joypad_driver_t *driver,
      unsigned joypad, unsigned hat_dir, unsigned hat);

void input_joypad_poll(const rarch_joypad_driver_t *driver);

extern const rarch_joypad_driver_t dinput_joypad;
extern const rarch_joypad_driver_t linuxraw_joypad;
extern const rarch_joypad_driver_t sdl_joypad;


struct rarch_key_map
{
   unsigned sym;
   enum retro_key rk;
};

extern const struct rarch_key_map rarch_key_map_x11[];
extern const struct rarch_key_map rarch_key_map_sdl[];
extern const struct rarch_key_map rarch_key_map_dinput[];

void input_init_keyboard_lut(const struct rarch_key_map *map);
enum retro_key input_translate_keysym_to_rk(unsigned sym);
unsigned input_translate_rk_to_keysym(enum retro_key key);

#endif

