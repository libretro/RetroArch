/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

/* We need libxkbcommon to translate raw evdev events to characters
 * which can be passed to keyboard callback in a sensible way. */
#include <xkbcommon/xkbcommon.h>

#include <string/string_list.h>

#include "../input_joypad_driver.h"
#include "../input_keymaps.h"
#include "../input_keyboard.h"
#include "../../configuration.h"

#define MOD_MAP_SIZE 5

static struct xkb_context *xkb_ctx;
static struct xkb_keymap  *xkb_map;
static struct xkb_state   *xkb_state;
static xkb_mod_index_t    *mod_map_idx;
static uint16_t           *mod_map_bit;

void free_xkb(void)
{
   if (mod_map_idx)
      free(mod_map_idx);
   if (mod_map_bit)
      free(mod_map_bit);
   if (xkb_map)
      xkb_keymap_unref(xkb_map);
   if (xkb_ctx)
      xkb_context_unref(xkb_ctx);
   if (xkb_state)
      xkb_state_unref(xkb_state);

   mod_map_idx = NULL;
   mod_map_bit = NULL;
   xkb_map     = NULL;
   xkb_ctx     = NULL;
   xkb_state   = NULL;
}

int init_xkb(void)
{
   settings_t *settings = config_get_ptr();
   mod_map_idx          = (xkb_mod_index_t *)calloc(MOD_MAP_SIZE, sizeof(xkb_mod_index_t));

   if (!mod_map_idx)
      goto error;

   mod_map_bit = (uint16_t*)calloc(MOD_MAP_SIZE, sizeof(uint16_t));

   if (!mod_map_bit)
      goto error;

   xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
   if (xkb_ctx)
   {
      struct string_list *list = NULL;
      struct xkb_rule_names rule = {0};

      rule.rules = "evdev";

      if (*settings->input.keyboard_layout)
      {
         list = string_split(settings->input.keyboard_layout, ":");
         if (list && list->size >= 2)
            rule.variant = list->elems[1].data;
         if (list && list->size >= 1)
            rule.layout = list->elems[0].data;
      }

      xkb_map = xkb_keymap_new_from_names(xkb_ctx, &rule, XKB_MAP_COMPILE_NO_FLAGS);
      if (list)
         string_list_free(list);
   }
   if (xkb_map)
   {
      xkb_mod_index_t *map_idx = (xkb_mod_index_t*)&mod_map_idx[0];
      uint16_t        *map_bit = (uint16_t*)&mod_map_bit[0];

      xkb_state = xkb_state_new(xkb_map);

      *map_idx = xkb_keymap_mod_get_index(xkb_map, XKB_MOD_NAME_CAPS);
      map_idx++;
      *map_bit = RETROKMOD_CAPSLOCK;
      map_bit++;
      *map_idx = xkb_keymap_mod_get_index(xkb_map, XKB_MOD_NAME_SHIFT);
      map_idx++;
      *map_bit = RETROKMOD_SHIFT;
      map_bit++;
      *map_idx = xkb_keymap_mod_get_index(xkb_map, XKB_MOD_NAME_CTRL);
      map_idx++;
      *map_bit = RETROKMOD_CTRL;
      map_bit++;
      *map_idx = xkb_keymap_mod_get_index(xkb_map, XKB_MOD_NAME_ALT);
      map_idx++;
      *map_bit = RETROKMOD_ALT;
      map_bit++;
      *map_idx = xkb_keymap_mod_get_index(xkb_map, XKB_MOD_NAME_LOGO);
      *map_bit = RETROKMOD_META;
   }

   return 0;

error:
   free_xkb();

   return -1;
}

/* FIXME: Don't handle composed and dead-keys properly. 
 * Waiting for support in libxkbcommon ... */
int handle_xkb(int code, int value)
{
   unsigned i;
   const xkb_keysym_t *syms = NULL;
   unsigned num_syms        = 0;
   uint16_t mod             = 0;
   /* Convert Linux evdev to X11 (xkbcommon docs say so at least ...) */
   int xk_code              = code + 8;

   if (!xkb_state)
      return -1;

   if (value == 2) /* Repeat, release first explicitly. */
      xkb_state_update_key(xkb_state, xk_code, XKB_KEY_UP);

   if (value)
      num_syms = xkb_state_key_get_syms(xkb_state, xk_code, &syms);

   if (!syms)
      return -1;

   xkb_state_update_key(xkb_state, xk_code, value ? XKB_KEY_DOWN : XKB_KEY_UP);

   /* Build mod state. */
   for (i = 0; i < MOD_MAP_SIZE; i++)
   {
      xkb_mod_index_t *map_idx = (xkb_mod_index_t*)&mod_map_idx[i];
      uint16_t        *map_bit = (uint16_t       *)&mod_map_bit[i];

      if (*map_idx != XKB_MOD_INVALID)
         mod |= xkb_state_mod_index_is_active(
               xkb_state, *map_idx,
               (enum xkb_state_component)((XKB_STATE_MODS_EFFECTIVE) > 0)) ? *map_bit : 0;
   }

   input_keyboard_event(value, input_keymaps_translate_keysym_to_rk(code),
         num_syms ? xkb_keysym_to_utf32(syms[0]) : 0, mod, RETRO_DEVICE_KEYBOARD);

   for (i = 1; i < num_syms; i++)
      input_keyboard_event(value, RETROK_UNKNOWN,
            xkb_keysym_to_utf32(syms[i]), mod, RETRO_DEVICE_KEYBOARD);

   return 0;
}
