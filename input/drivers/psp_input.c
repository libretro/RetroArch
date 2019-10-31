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
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#if defined(SN_TARGET_PSP2)
#include <sceerror.h>
#include <kernel.h>
#include <ctrl.h>
#elif defined(VITA)
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/hid.h>
#include <psp2/motion.h>
#define VITA_NUM_SCANCODES 115 /* size of rarch_key_map_vita */
#define VITA_MAX_SCANCODE 0xE7
#define VITA_NUM_MODIFIERS 11 /* number of modifiers reported */
#define MOUSE_MAX_X 960
#define MOUSE_MAX_Y 544
#elif defined(PSP)
#include <pspctrl.h>
#endif

#include <boolean.h>
#include <libretro.h>
#include <retro_miscellaneous.h>

#ifdef HAVE_KERNEL_PRX
#include "../../bootstrap/psp1/kernel_functions.h"
#endif

#include "../../defines/psp_defines.h"

#include "../input_driver.h"
#ifdef VITA
#include "../input_keymaps.h"
uint8_t modifier_lut[VITA_NUM_MODIFIERS][2] =
{
   { 0xE0, 0x01 }, /* LCTRL */
   { 0xE4, 0x10 }, /* RCTRL */
   { 0xE1, 0x02 }, /* LSHIFT */
   { 0xE5, 0x20 }, /* RSHIFT */
   { 0xE2, 0x04 }, /* LALT */
   { 0xE6, 0x40 }, /* RALT */
   { 0xE3, 0x08 }, /* LGUI */
   { 0xE7, 0x80 }, /* RGUI */
   { 0x53, 0x01 }, /* NUMLOCK */
   { 0x39, 0x02 }, /* CAPSLOCK */
   { 0x47, 0x04 }  /* SCROLLOCK */
};
#endif

/* TODO/FIXME -
 * fix game focus toggle */

typedef struct psp_input
{
   const input_device_driver_t *joypad;
#ifdef VITA
   int keyboard_hid_handle;
   uint8_t prev_keys[6];
   bool keyboard_state[VITA_MAX_SCANCODE + 1];

   int mouse_hid_handle;
   int32_t mouse_x;
   int32_t mouse_y;
   int32_t mouse_x_delta;
   int32_t mouse_y_delta;
   bool mouse_button_left;
   bool mouse_button_right;
   bool mouse_button_middle;
   
   bool sensors_enabled;
#endif
} psp_input_t;

static void psp_input_poll(void *data)
{
   psp_input_t *psp = (psp_input_t*)data;

   if (psp && psp->joypad)
      psp->joypad->poll();

#ifdef VITA
   unsigned int i = 0;
   int key_sym = 0;
   unsigned key_code = 0;
   uint8_t mod_code = 0;
   uint16_t mod = 0;
   uint8_t modifiers[2] = { 0, 0 };
   bool key_held = false;
   int numReports = 0;
   int mouse_velocity_x = 0;
   int mouse_velocity_y = 0;
   SceHidKeyboardReport k_reports[SCE_HID_MAX_REPORT];
   SceHidMouseReport m_reports[SCE_HID_MAX_REPORT];

   if (psp->keyboard_hid_handle > 0)
   {
      numReports = sceHidKeyboardRead(psp->keyboard_hid_handle, (SceHidKeyboardReport**)&k_reports, SCE_HID_MAX_REPORT);

      if (numReports < 0) {
         psp->keyboard_hid_handle = 0;
      }
      else if (numReports) {
         modifiers[0] = k_reports[numReports - 1].modifiers[0];
         modifiers[1] = k_reports[numReports - 1].modifiers[1];
         mod = 0;
         if (modifiers[0] & 0x11)
            mod |= RETROKMOD_CTRL;
         if (modifiers[0] & 0x22)
            mod |= RETROKMOD_SHIFT;
         if (modifiers[0] & 0x44)
            mod |= RETROKMOD_ALT;
         if (modifiers[0] & 0x88)
            mod |= RETROKMOD_META;
         if (modifiers[1] & 0x01)
            mod |= RETROKMOD_NUMLOCK;
         if (modifiers[1] & 0x02)
            mod |= RETROKMOD_CAPSLOCK;
         if (modifiers[1] & 0x04)
            mod |= RETROKMOD_SCROLLOCK;

         for (i = 0; i < VITA_NUM_MODIFIERS; i++)
         {
            key_sym = (int) modifier_lut[i][0];
            mod_code = modifier_lut[i][1];
            key_code = input_keymaps_translate_keysym_to_rk(key_sym);
            if (i < 8)
            {
               key_held = (modifiers[0] & mod_code);
            }
            else
            {
               key_held = (modifiers[1] & mod_code);
            }

            if (key_held && !(psp->keyboard_state[key_sym]))
            {
               psp->keyboard_state[key_sym] = true;
               input_keyboard_event(true, key_code, 0, mod, RETRO_DEVICE_KEYBOARD);
            }
            else if (!key_held && (psp->keyboard_state[key_sym]))
            {
               psp->keyboard_state[key_sym] = false;
               input_keyboard_event(false, key_code, 0, mod, RETRO_DEVICE_KEYBOARD);
            }
         }

         for (i = 0; i < 6; i++)
         {
            key_sym = k_reports[numReports - 1].keycodes[i];

            if (key_sym != psp->prev_keys[i])
            {
               if (psp->prev_keys[i])
               {
                  psp->keyboard_state[psp->prev_keys[i]] = false;
                  key_code = input_keymaps_translate_keysym_to_rk(psp->prev_keys[i]);
                  input_keyboard_event(false, key_code, 0, mod, RETRO_DEVICE_KEYBOARD);
               }
               if (key_sym)
               {
                  psp->keyboard_state[key_sym] = true;
                  key_code = input_keymaps_translate_keysym_to_rk(key_sym);
                  input_keyboard_event(true, key_code, 0, mod, RETRO_DEVICE_KEYBOARD);
               }
               psp->prev_keys[i] = key_sym;
            }
         }
      }
   }

   if (psp->mouse_hid_handle > 0)
   {
      numReports = sceHidMouseRead(psp->mouse_hid_handle, (SceHidMouseReport**)&m_reports, SCE_HID_MAX_REPORT);
      if (numReports > 0)
      {
         for (i = 0; i <= numReports - 1; i++)
         {
            uint8_t buttons = m_reports[i].buttons;

            if (buttons & 0x1)
            {
               psp->mouse_button_left = true;
            }
            else
            {
               psp->mouse_button_left = false;
            }

            if (buttons & 0x2)
            {
               psp->mouse_button_right = true;
            }
            else
            {
               psp->mouse_button_right = false;
            }

            if (buttons & 0x4)
            {
               psp->mouse_button_middle = true;
            }
            else
            {
               psp->mouse_button_middle = false;
            }

            mouse_velocity_x += m_reports[i].rel_x;
            mouse_velocity_y += m_reports[i].rel_y;
         }
      }
   }
   psp->mouse_x_delta = mouse_velocity_x;
   psp->mouse_y_delta = mouse_velocity_y;
   psp->mouse_x += mouse_velocity_x;
   psp->mouse_y += mouse_velocity_y;
   if (psp->mouse_x < 0)
   {
      psp->mouse_x = 0;
   }
   else if (psp->mouse_x > MOUSE_MAX_X)
   {
      psp->mouse_x = MOUSE_MAX_X;
   }

   if (psp->mouse_y < 0)
   {
      psp->mouse_y = 0;
   }
   else if (psp->mouse_y > MOUSE_MAX_Y)
   {
      psp->mouse_y = MOUSE_MAX_Y;
   }
#endif
}

#ifdef VITA
static int16_t psp_input_mouse_state(psp_input_t *psp, unsigned id, bool screen)
{
   int val = 0;
   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         val = psp->mouse_button_left;
         break;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         val = psp->mouse_button_right;
         break;
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         val = psp->mouse_button_middle;
         break;
      case RETRO_DEVICE_ID_MOUSE_X:
         if (screen)
         {
            val = psp->mouse_x;
         }
         else
         {
            val = psp->mouse_x_delta;
            psp->mouse_x_delta = 0; /* flush delta after it has been read */
         }
         break;
      case RETRO_DEVICE_ID_MOUSE_Y:
         if (screen)
         {
            val = psp->mouse_y;
         }
         else
         {
            val = psp->mouse_y_delta;
            psp->mouse_y_delta = 0; /* flush delta after it has been read */
         }
         break;
   }

   return val;
}
#endif


static int16_t psp_input_state(void *data,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   psp_input_t *psp           = (psp_input_t*)data;

#if !defined(SN_TARGET_PSP2) && !defined(VITA)
   if (port > 0)
      return 0;
#endif

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            int16_t ret = 0;
            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
            {
               /* Auto-binds are per joypad, not per user. */
               const uint64_t joykey  = (binds[port][i].joykey != NO_BTN)
                  ? binds[port][i].joykey : joypad_info.auto_binds[i].joykey;
               const uint32_t joyaxis = (binds[port][i].joyaxis != AXIS_NONE)
                  ? binds[port][i].joyaxis : joypad_info.auto_binds[i].joyaxis;

               if ((uint16_t)joykey != NO_BTN && psp->joypad->button(joypad_info.joy_idx, (uint16_t)joykey))
               {
                  ret |= (1 << i);
                  continue;
               }
               if (((float)abs(psp->joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
               {
                  ret |= (1 << i);
                  continue;
               }
            }

            return ret;
         }
         else
         {
            /* Auto-binds are per joypad, not per user. */
            const uint64_t joykey  = (binds[port][id].joykey != NO_BTN)
               ? binds[port][id].joykey : joypad_info.auto_binds[id].joykey;
            const uint32_t joyaxis = (binds[port][id].joyaxis != AXIS_NONE)
               ? binds[port][id].joyaxis : joypad_info.auto_binds[id].joyaxis;

            if ((uint16_t)joykey != NO_BTN && psp->joypad->button(joypad_info.joy_idx, (uint16_t)joykey))
               return true;
            if (((float)abs(psp->joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
               return true;
         }
         break;
      case RETRO_DEVICE_ANALOG:
         if (binds[port])
            return input_joypad_analog(psp->joypad, joypad_info, port, idx, id, binds[port]);
         break;
#ifdef VITA
      case RETRO_DEVICE_KEYBOARD:
         return ((id < RETROK_LAST) && psp->keyboard_state[rarch_keysym_lut[(enum retro_key)id]]);
         break;
      case RETRO_DEVICE_MOUSE:
         return psp_input_mouse_state(psp, id, false);
         break;
      case RARCH_DEVICE_MOUSE_SCREEN:
         return psp_input_mouse_state(psp, id, true);
         break;
#endif
   }

   return 0;
}

static void psp_input_free_input(void *data)
{
   psp_input_t *psp = (psp_input_t*)data;

   if (psp && psp->joypad)
      psp->joypad->destroy();

   free(data);
}

static void* psp_input_initialize(const char *joypad_driver)
{
   psp_input_t *psp = (psp_input_t*)calloc(1, sizeof(*psp));
   if (!psp)
      return NULL;

   psp->joypad = input_joypad_init_driver(joypad_driver, psp);

#ifdef VITA
   sceHidKeyboardEnumerate(&(psp->keyboard_hid_handle), 1);
   sceHidMouseEnumerate(&(psp->mouse_hid_handle), 1);

   input_keymaps_init_keyboard_lut(rarch_key_map_vita);
   unsigned int i;
   for (i = 0; i <= VITA_MAX_SCANCODE; i++) {
      psp->keyboard_state[i] = false;
   }
   for (i = 0; i < 6; i++) {
      psp->prev_keys[i] = 0;
   }
   psp->mouse_x = 0;
   psp->mouse_y = 0;
#endif
   return psp;
}

static uint64_t psp_input_get_capabilities(void *data)
{
   (void)data;

   uint64_t caps = (1 << RETRO_DEVICE_JOYPAD) |  (1 << RETRO_DEVICE_ANALOG);

#ifdef VITA
   caps |= (1 << RETRO_DEVICE_KEYBOARD) | (1 << RETRO_DEVICE_MOUSE);
#endif

   return caps;
}

static const input_device_driver_t *psp_input_get_joypad_driver(void *data)
{
   psp_input_t *psp = (psp_input_t*)data;
   if (psp)
      return psp->joypad;
   return NULL;
}

static void psp_input_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool psp_input_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   psp_input_t *psp = (psp_input_t*)data;

   if (psp && psp->joypad)
      return input_joypad_set_rumble(psp->joypad,
         port, effect, strength);
   return false;
}

#ifdef VITA
static bool psp_input_set_sensor_state(void *data, unsigned port,
      enum retro_sensor_action action, unsigned event_rate)
{
   psp_input_t *psp = (psp_input_t*)data;
	
   if(!psp)
      return false;
  
   switch(action)
   {
      case RETRO_SENSOR_ILLUMINANCE_ENABLE:
         return false;

      case RETRO_SENSOR_ILLUMINANCE_DISABLE:
         return true;
		 
      case RETRO_SENSOR_ACCELEROMETER_DISABLE:
      case RETRO_SENSOR_GYROSCOPE_DISABLE:
         if(psp->sensors_enabled)
         {
           psp->sensors_enabled = false;
           sceMotionMagnetometerOff();
           sceMotionStopSampling();
         }
         return true;

      case RETRO_SENSOR_ACCELEROMETER_ENABLE:
      case RETRO_SENSOR_GYROSCOPE_ENABLE:
         if(!psp->sensors_enabled)
         {
           psp->sensors_enabled = true;
           sceMotionStartSampling();
           sceMotionMagnetometerOn();
         }
         return true;
   }
   
   return false;
}

static float psp_input_get_sensor_input(void *data,
      unsigned port, unsigned id)
{
   SceMotionSensorState sixaxis;
   
   psp_input_t *psp = (psp_input_t*)data;
	
   if(!psp || !psp->sensors_enabled)
      return false;

   if(id >= RETRO_SENSOR_ACCELEROMETER_X && id <= RETRO_SENSOR_GYROSCOPE_Z)
   {
      sceMotionGetSensorState(&sixaxis, port);

      switch(id)
      {
         case RETRO_SENSOR_ACCELEROMETER_X:
            return sixaxis.accelerometer.x;
         case RETRO_SENSOR_ACCELEROMETER_Y:
            return sixaxis.accelerometer.y;
         case RETRO_SENSOR_ACCELEROMETER_Z:
            return sixaxis.accelerometer.z;
         case RETRO_SENSOR_GYROSCOPE_X:
            return sixaxis.gyro.x;
         case RETRO_SENSOR_GYROSCOPE_Y:
            return sixaxis.gyro.y;
         case RETRO_SENSOR_GYROSCOPE_Z:
            return sixaxis.gyro.z;
      }

   }

   return 0.0f;
}
	
#endif

input_driver_t input_psp = {
   psp_input_initialize,
   psp_input_poll,
   psp_input_state,
   psp_input_free_input,
#ifdef VITA
   psp_input_set_sensor_state,
   psp_input_get_sensor_input,
#else
   NULL,
   NULL,
#endif
   psp_input_get_capabilities,
#ifdef VITA
   "vita",
#else
   "psp",
#endif

   psp_input_grab_mouse,
   NULL,
   psp_input_set_rumble,
   psp_input_get_joypad_driver,
   NULL,
   false
};
