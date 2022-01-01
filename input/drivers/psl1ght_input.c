/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2020  Google
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

#include <stdint.h>
#include <stdlib.h>

#include <boolean.h>
#include <libretro.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <defines/ps3_defines.h>

#include "../input_driver.h"

#include <retro_inline.h>

#include "../../config.def.h"

#include "../../tasks/tasks_internal.h"

#ifdef HAVE_MOUSE
#define MAX_MICE 7
#endif

/* TODO/FIXME -
 * fix game focus toggle */

typedef struct
{
   float x;
   float y;
   float z;
} sensor_t;

typedef struct ps3_input
{
   int connected[MAX_KB_PORT_NUM];
#ifdef HAVE_MOUSE
   unsigned mice_connected;
#endif
   KbInfo kbinfo;
   KbData kbdata[MAX_KB_PORT_NUM];
} ps3_input_t;

static int mod_table[] = {
    RETROK_RSUPER,
    RETROK_RALT,
    RETROK_RSHIFT,
    RETROK_RCTRL,
    RETROK_LSUPER,
    RETROK_LALT,
    RETROK_LSHIFT,
    RETROK_LCTRL
};

static void ps3_connect_keyboard(ps3_input_t *ps3, int port)
{
   ioKbSetCodeType(port, KB_CODETYPE_RAW);
   ioKbSetReadMode(port, KB_RMODE_INPUTCHAR);
   ps3->connected[port] = 1;
}

static void ps3_disconnect_keyboard(ps3_input_t *ps3, int port)
{
   ps3->connected[port] = 0;
}

static void ps3_input_poll(void *data)
{
   unsigned i, j;
   ps3_input_t *ps3 = (ps3_input_t*)data;
   KbData last_kbdata[MAX_KB_PORT_NUM];

   ioKbGetInfo(&ps3->kbinfo);

   for (i = 0; i < MAX_KB_PORT_NUM; i++)
   {
      if (ps3->kbinfo.status[i] && !ps3->connected[i])
         ps3_connect_keyboard(ps3, i);
#if 0
      if (!ps3->kbinfo.status[i] && ps3->connected[i])
         ps3_disconnect_keyboard(ps3, i);
#endif
   }

   memcpy(last_kbdata, ps3->kbdata, sizeof(last_kbdata));
   for (i = 0; i < MAX_KB_PORT_NUM; i++)
   {
      if (ps3->kbinfo.status[i])
         ioKbRead(i, &ps3->kbdata[i]);
   }

   for (i = 0; i < MAX_KB_PORT_NUM; i++)
   {
      /* Set keyboard modifier based on shift,ctrl and alt state */
      uint16_t mod          = 0;

      if (     ps3->kbdata[i].mkey._KbMkeyU._KbMkeyS.l_alt 
            || ps3->kbdata[i].mkey._KbMkeyU._KbMkeyS.r_alt)
         mod |= RETROKMOD_ALT;
      if (     ps3->kbdata[i].mkey._KbMkeyU._KbMkeyS.l_ctrl 
            || ps3->kbdata[i].mkey._KbMkeyU._KbMkeyS.r_ctrl)
         mod |= RETROKMOD_CTRL;
      if (     ps3->kbdata[i].mkey._KbMkeyU._KbMkeyS.l_shift
            || ps3->kbdata[i].mkey._KbMkeyU._KbMkeyS.r_shift)
         mod |= RETROKMOD_SHIFT;

      /* TODO: windows keys.  */

      for (j = 0; j < last_kbdata[i].nb_keycode; j++)
      {
         unsigned k;
         int code            = last_kbdata[i].keycode[j];
         int newly_depressed = 1;

         for (k = 0; k < MAX_KB_PORT_NUM; i++)
         {
            if (ps3->kbdata[i].keycode[k] == code)
            {
               newly_depressed = 0;
               break;
            }
         }

         if (newly_depressed)
         {
            unsigned keyboardcode = input_keymaps_translate_keysym_to_rk(code);
            input_keyboard_event(false, keyboardcode, keyboardcode, mod, RETRO_DEVICE_KEYBOARD);
         }
      }

      for (j = 0; j < ps3->kbdata[i].nb_keycode; j++)
      {
         unsigned k;
         int code          = ps3->kbdata[i].keycode[j];
         int newly_pressed = 1;

         for (k = 0; k < MAX_KB_PORT_NUM; i++)
         {
            if (last_kbdata[i].keycode[k] == code)
            {
               newly_pressed = 0;
               break;
            }
         }

         if (newly_pressed)
         {
            unsigned keyboardcode = input_keymaps_translate_keysym_to_rk(code);
            input_keyboard_event(true, keyboardcode, keyboardcode, mod, RETRO_DEVICE_KEYBOARD);
         }
      }
   }
}

static bool psl1ght_keyboard_port_input_pressed(
      ps3_input_t *ps3, unsigned id)
{
   int code;
   unsigned i, j;

   if (id >= RETROK_LAST || id == 0)
      return false;

   for (i = 0; i < 8; i++)
   {
      if (id == mod_table[i])
      {
         for (j = 0; j < MAX_KB_PORT_NUM; j++)
         {
            if (ps3->kbinfo.status[j] 
                  && (ps3->kbdata[j].mkey._KbMkeyU.mkeys & (1 << i)))
               return true;
         }
         return false;
      }
   }

   code = rarch_keysym_lut[id];
   if (code == 0)
      return false;
   for (i = 0; i < MAX_KB_PORT_NUM; i++)
   {
      if (ps3->kbinfo.status[i])
      {
         for (j = 0; j < ps3->kbdata[i].nb_keycode; j++)
         {
            if (ps3->kbdata[i].keycode[j] == code)
               return true;
         }
      }
   }

   return false;
}

static int16_t ps3_input_state(
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      bool keyboard_mapping_blocked,
      unsigned port,
      unsigned device,
      unsigned idx,
      unsigned id)
{
   ps3_input_t *ps3           = (ps3_input_t*)data;

   if (!ps3)
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            int16_t ret = 0;

            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
            {
               if (binds[port][i].valid)
               {
                  if (psl1ght_keyboard_port_input_pressed(
                           ps3, binds[port][i].key))
                     ret |= (1 << i);
               }
            }

            return ret;
         }

         if (binds[port][id].valid)
         {
            if (psl1ght_keyboard_port_input_pressed(
                     ps3, binds[port][id].key))
               return 1;
         }
         break;
      case RETRO_DEVICE_ANALOG:
         break;
      case RETRO_DEVICE_KEYBOARD:
         return psl1ght_keyboard_port_input_pressed(ps3, id);
   }

   return 0;
}

static void ps3_input_free_input(void *data)
{
    ioPadEnd();
    ioKbEnd();
}

static void* ps3_input_init(const char *joypad_driver)
{
   unsigned i;
   ps3_input_t *ps3 = (ps3_input_t*)calloc(1, sizeof(*ps3));
   if (!ps3)
      return NULL;

   /* Keyboard  */

   input_keymaps_init_keyboard_lut(rarch_key_map_psl1ght);

   ioKbInit(MAX_KB_PORT_NUM);
   ioKbGetInfo(&ps3->kbinfo);

   for (i = 0; i < MAX_KB_PORT_NUM; i++)
   {
      if (ps3->kbinfo.status[i])
         ps3_connect_keyboard(ps3, i);
   }

   return ps3;
}

static uint64_t ps3_input_get_capabilities(void *data)
{
   return
#ifdef HAVE_MOUSE
      (1 << RETRO_DEVICE_MOUSE)  |
#endif
      (1 << RETRO_DEVICE_KEYBOARD)  |
      (1 << RETRO_DEVICE_JOYPAD) |
      (1 << RETRO_DEVICE_ANALOG);
}

static bool ps3_input_set_sensor_state(void *data, unsigned port,
      enum retro_sensor_action action, unsigned event_rate) { return false; }

input_driver_t input_ps3 = {
   ps3_input_init,
   ps3_input_poll,
   ps3_input_state,
   ps3_input_free_input,
   ps3_input_set_sensor_state,
   NULL,
   ps3_input_get_capabilities,
   "ps3",

   NULL,                         /* grab_mouse */
   NULL
};

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdint.h>

static padData pad_state[MAX_PADS];
static bool pads_connected[MAX_PADS];

static INLINE int16_t convert_u8_to_s16(uint8_t val)
{
   if (val == 0)
      return -0x7fff;
   return val * 0x0101 - 0x8000;
}

static const char *ps3_joypad_name(unsigned pad)
{
   return "SixAxis Controller";
}

static void ps3_joypad_autodetect_add(unsigned autoconf_pad)
{
   input_autoconfigure_connect(
         ps3_joypad_name(autoconf_pad),
         NULL,
         ps3_joypad.ident,
         autoconf_pad,
         0,
         0
         );
}

static bool ps3_joypad_init(void *data)
{
   (void)data;
   ioPadInit(7);

   return true;
}

static uint16_t transform_buttons(const padData *data)
{
   return (
         (data->BTN_CROSS << RETRO_DEVICE_ID_JOYPAD_B)
         | (data->BTN_SQUARE << RETRO_DEVICE_ID_JOYPAD_Y)
         | (data->BTN_SELECT << RETRO_DEVICE_ID_JOYPAD_SELECT)
         | (data->BTN_START << RETRO_DEVICE_ID_JOYPAD_START)
         | (data->BTN_UP << RETRO_DEVICE_ID_JOYPAD_UP)
         | (data->BTN_DOWN << RETRO_DEVICE_ID_JOYPAD_DOWN)
         | (data->BTN_LEFT << RETRO_DEVICE_ID_JOYPAD_LEFT)
         | (data->BTN_RIGHT << RETRO_DEVICE_ID_JOYPAD_RIGHT)
         | (data->BTN_CIRCLE << RETRO_DEVICE_ID_JOYPAD_A)
         | (data->BTN_TRIANGLE << RETRO_DEVICE_ID_JOYPAD_X)
         | (data->BTN_L1 << RETRO_DEVICE_ID_JOYPAD_L)
         | (data->BTN_R1 << RETRO_DEVICE_ID_JOYPAD_R)
         | (data->BTN_L2 << RETRO_DEVICE_ID_JOYPAD_L2)
         | (data->BTN_R2 << RETRO_DEVICE_ID_JOYPAD_R2)
         | (data->BTN_L3 << RETRO_DEVICE_ID_JOYPAD_L3)
         | (data->BTN_R3 << RETRO_DEVICE_ID_JOYPAD_R3)
         );
}

static int32_t ps3_joypad_button(unsigned port, uint16_t joykey)
{
   uint16_t state                       = 0;
   if (port >= MAX_PADS)
      return 0;
   state                                = transform_buttons(
         &pad_state[port]);
   return (state & (UINT64_C(1) << joykey));
}

static void ps3_joypad_get_buttons(unsigned port, input_bits_t *state)
{
   if (port < MAX_PADS)
   {
      uint16_t v = transform_buttons(&pad_state[port]);
      BITS_COPY16_PTR( state,  v);
   }
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t ps3_joypad_axis_state(unsigned port, uint32_t joyaxis)
{
   int val     = 0x80;
   int axis    = -1;
   bool is_neg = false;
   bool is_pos = false;

   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      axis     = AXIS_NEG_GET(joyaxis);
      is_neg   = true;
   }
   else if (AXIS_POS_GET(joyaxis) < 4)
   {
      axis     = AXIS_POS_GET(joyaxis);
      is_pos   = true;
   }

   switch (axis)
   {
      case 0:
         val   = pad_state[port].ANA_L_H;
         break;
      case 1:
         val   = pad_state[port].ANA_L_V;
         break;
      case 2:
         val   = pad_state[port].ANA_R_H;
         break;
      case 3:
         val   = pad_state[port].ANA_R_V;
         break;
   }

   val         = (val - 0x7f) * 0xff;
   if (is_neg && val > 0)
      val      = 0;
   else if (is_pos && val < 0)
      val      = 0;

   return val;
}

static int16_t ps3_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (port >= DEFAULT_MAX_PADS)
      return 0;
   return ps3_joypad_axis_state(port, joyaxis);
}

static int16_t ps3_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   unsigned i;
   int16_t ret                          = 0;
   uint16_t state                       = 0;
   
   if (port >= DEFAULT_MAX_PADS)
      return 0;

   state                                = transform_buttons(&pad_state[port]);

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if (
               (uint16_t)joykey != NO_BTN 
            && (state & (UINT64_C(1) << (uint16_t)joykey))
         )
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(ps3_joypad_axis_state(port, joyaxis)) 
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static void ps3_joypad_poll(void)
{
   unsigned port;
   padInfo padinfo;

   ioPadGetInfo(&padinfo);

   for (port = 0; port < MAX_PADS; port++)
   {
      if (padinfo.status[port])
         ioPadGetData(port, &pad_state[port]);

      if (!pads_connected[port] && padinfo.status[port])
      {
         ps3_joypad_autodetect_add(port);
         pads_connected[port] = 1;
      }
      else
      {
         input_autoconfigure_disconnect(port, ps3_joypad.ident);
         pads_connected[port] = 0;
      }

      pads_connected[port] = padinfo.status[port];
   }
}

static bool ps3_joypad_query_pad(unsigned pad)
{
  return pad < MAX_USERS && transform_buttons(&pad_state[pad]);
}

static bool ps3_joypad_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength) { return true; }

static void ps3_joypad_destroy(void) { }

input_device_driver_t ps3_joypad = {
   ps3_joypad_init,
   ps3_joypad_query_pad,
   ps3_joypad_destroy,
   ps3_joypad_button,
   ps3_joypad_state,
   ps3_joypad_get_buttons,
   ps3_joypad_axis,
   ps3_joypad_poll,
   ps3_joypad_rumble,
   NULL,
   ps3_joypad_name,
   "ps3",
};
