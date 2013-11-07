/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#ifndef __PSL1GHT__
#include <sdk_version.h>
#endif

#include <sys/memory.h>

#include "sdk_defines.h"

#include "ps3_input.h"
#include "../driver.h"
#include "../libretro.h"
#include "../general.h"

#ifdef HAVE_MOUSE
#ifndef __PSL1GHT__
#define MAX_MICE 7
#endif
#endif

/*============================================================
  PS3 PAD
  ============================================================ */

typedef struct
{
   float x;
   float y;
   float z;
} sensor_t;

const struct platform_bind platform_keys[] = {
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_B), "Cross button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_Y), "Square button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT), "Select button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_START), "Start button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_UP), "D-Pad Up" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN), "D-Pad Down" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT), "D-Pad Left" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT), "D-Pad Right" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_A), "Circle button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_X), "Triangle button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_L), "L1 button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_R), "R1 button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_L2), "L2 button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_R2), "R2 button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_L3), "L3 button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_R3), "R3 button" },
   { (1ULL << RARCH_TURBO_ENABLE), "Turbo button (unmapped)" },
};

typedef struct ps3_input
{
   uint64_t state[MAX_PADS];
   unsigned pads_connected;
#ifdef HAVE_MOUSE
   unsigned mice_connected;
#endif
   sensor_t accelerometer_state[MAX_PADS];
} ps3_input_t;

uint8_t analog_state[MAX_PADS][2][2];

static void ps3_input_poll(void *data)
{
   CellPadInfo2 pad_info;
   ps3_input_t *ps3 = (ps3_input_t*)data;

   for (unsigned port = 0; port < MAX_PADS; port++)
   {
      static CellPadData state_tmp;
      cellPadGetData(port, &state_tmp);

      if (state_tmp.len != 0)
      {
         uint64_t *state_cur = &ps3->state[port];
         *state_cur = 0;
         analog_state[0][0][0] = analog_state[0][0][1] = analog_state[0][1][0] = analog_state[0][1][1] = 0;
         analog_state[1][0][0] = analog_state[1][0][1] = analog_state[1][1][0] = analog_state[1][1][1] = 0;
         analog_state[2][0][0] = analog_state[2][0][1] = analog_state[2][1][0] = analog_state[2][1][1] = 0;
         analog_state[3][0][0] = analog_state[3][0][1] = analog_state[3][1][0] = analog_state[3][1][1] = 0;
#ifdef __PSL1GHT__
         *state_cur |= (state_tmp.BTN_LEFT)     ? (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
         *state_cur |= (state_tmp.BTN_DOWN)     ? (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
         *state_cur |= (state_tmp.BTN_RIGHT)    ? (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
         *state_cur |= (state_tmp.BTN_UP)       ? (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
         *state_cur |= (state_tmp.BTN_START)    ? (1ULL << RETRO_DEVICE_ID_JOYPAD_START) : 0;
         *state_cur |= (state_tmp.BTN_R3)       ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R3) : 0;
         *state_cur |= (state_tmp.BTN_L3)       ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L3) : 0;
         *state_cur |= (state_tmp.BTN_SELECT)   ? (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
         *state_cur |= (state_tmp.BTN_TRIANGLE) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_X) : 0;
         *state_cur |= (state_tmp.BTN_SQUARE)   ? (1ULL << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
         *state_cur |= (state_tmp.BTN_CROSS)    ? (1ULL << RETRO_DEVICE_ID_JOYPAD_B) : 0;
         *state_cur |= (state_tmp.BTN_CIRCLE)   ? (1ULL << RETRO_DEVICE_ID_JOYPAD_A) : 0;
         *state_cur |= (state_tmp.BTN_R1)       ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R) : 0;
         *state_cur |= (state_tmp.BTN_L1)       ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L) : 0;
         *state_cur |= (state_tmp.BTN_R2)       ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R2) : 0;
         *state_cur |= (state_tmp.BTN_L2)       ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L2) : 0;
#else
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_LEFT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_DOWN) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_RIGHT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_UP) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_START) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_START) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_R3) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R3) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_L3) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L3) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_SELECT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_TRIANGLE) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_X) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_SQUARE) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CROSS) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_B) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CIRCLE) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_A) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R1) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_L1) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R2) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R2) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_L2) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L2) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_L2) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L2) : 0;
         analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT ][RETRO_DEVICE_ID_ANALOG_X] = state_tmp.button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X];
         analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT ][RETRO_DEVICE_ID_ANALOG_Y] = state_tmp.button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y];
         analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X] = state_tmp.button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X];
         analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y] = state_tmp.button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y];
         ps3->accelerometer_state[port].x = state_tmp.button[CELL_PAD_BTN_OFFSET_SENSOR_X];
         ps3->accelerometer_state[port].y = state_tmp.button[CELL_PAD_BTN_OFFSET_SENSOR_Y];
         ps3->accelerometer_state[port].z = state_tmp.button[CELL_PAD_BTN_OFFSET_SENSOR_Z];
#endif
      }
   }

   uint64_t *state_p1 = &ps3->state[0];
   uint64_t *lifecycle_state = &g_extern.lifecycle_state;

   *lifecycle_state &= ~((1ULL << RARCH_MENU_TOGGLE));

   if ((*state_p1 & (1ULL << RETRO_DEVICE_ID_JOYPAD_L3)) && (*state_p1 & (1ULL << RETRO_DEVICE_ID_JOYPAD_R3)))
      *lifecycle_state |= (1ULL << RARCH_MENU_TOGGLE);

   cellPadGetInfo2(&pad_info);
   ps3->pads_connected = pad_info.now_connect; 

#ifdef HAVE_MOUSE
   CellMouseInfo mouse_info;
   cellMouseGetInfo(&mouse_info);
   ps3->mice_connected = mouse_info.now_connect;
#endif
}

#ifdef HAVE_MOUSE

static int16_t ps3_mouse_device_state(void *data, unsigned player, unsigned id)
{
   ps3_input_t *ps3 = (ps3_input_t*)data;
   CellMouseData mouse_state;
   cellMouseGetData(id, &mouse_state);

   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return (!ps3->mice_connected ? 0 : mouse_state.buttons & CELL_MOUSE_BUTTON_1);
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return (!ps3->mice_connected ? 0 : mouse_state.buttons & CELL_MOUSE_BUTTON_2);
      case RETRO_DEVICE_ID_MOUSE_X:
         return (!ps3->mice_connected ? 0 : mouse_state.x_axis);
      case RETRO_DEVICE_ID_MOUSE_Y:
         return (!ps3->mice_connected ? 0 : mouse_state.y_axis);
      default:
         return 0;
   }
}

#endif

static int16_t ps3_input_state(void *data, const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   ps3_input_t *ps3 = (ps3_input_t*)data;
   uint64_t button;
   int16_t retval = 0;

   if (port < ps3->pads_connected)
   {
      switch (device)
      {
         case RETRO_DEVICE_JOYPAD:
            button = binds[port][id].joykey;
            retval = (ps3->state[port] & button) ? 1 : 0;
            break;
         case RETRO_DEVICE_ANALOG:
            {
               int analog = (analog_state[port][index][id] - 128) * 0x0101;
               if (analog < -0x7fff)
                  analog = -0x7fff;
               retval = analog;
            }
            break;
         case RETRO_DEVICE_SENSOR_ACCELEROMETER:
            switch (id)
            {
               // fixed range of 0x000 - 0x3ff
               case RETRO_DEVICE_ID_SENSOR_ACCELEROMETER_X:
                  retval = ps3->accelerometer_state[port].x;
               case RETRO_DEVICE_ID_SENSOR_ACCELEROMETER_Y:
                  retval = ps3->accelerometer_state[port].y;
               case RETRO_DEVICE_ID_SENSOR_ACCELEROMETER_Z:
                  retval = ps3->accelerometer_state[port].z;
               default:
                  retval = 0;
            }
            break;
#ifdef HAVE_MOUSE
         case RETRO_DEVICE_MOUSE:
            retval = ps3_mouse_device_state(data, port, id);
            break;
#endif
         default:
            return 0;
      }
   }

   return retval;
}

/*============================================================
  ON-SCREEN KEYBOARD UTILITY
  ============================================================ */

#ifdef HAVE_OSKUTIL

#define OSK_IN_USE 1

void oskutil_init(oskutil_params *params, unsigned containersize)
{
   params->flags = 0;
   if (containersize)
      params->osk_memorycontainer =  containersize; 
   else
      params->osk_memorycontainer =  1024*1024*2;
}

static bool oskutil_enable_key_layout (void)
{
   int ret = pOskSetKeyLayoutOption(CELL_OSKDIALOG_10KEY_PANEL | \
         CELL_OSKDIALOG_FULLKEY_PANEL);
   if (ret < 0)
      return (false);
   else
      return (true);
}

static void oskutil_create_activation_parameters(oskutil_params *params)
{
   params->dialogParam.controlPoint.x = 0.0;
   params->dialogParam.controlPoint.y = 0.0;

   int32_t LayoutMode = CELL_OSKDIALOG_LAYOUTMODE_X_ALIGN_CENTER | CELL_OSKDIALOG_LAYOUTMODE_Y_ALIGN_TOP;
   pOskSetLayoutMode(LayoutMode);

   params->dialogParam.osk_allowed_panels = 
      CELL_OSKDIALOG_PANELMODE_ALPHABET |
      CELL_OSKDIALOG_PANELMODE_NUMERAL | 
      CELL_OSKDIALOG_PANELMODE_NUMERAL_FULL_WIDTH |
      CELL_OSKDIALOG_PANELMODE_ENGLISH;

   params->dialogParam.firstViewPanel = CELL_OSKDIALOG_PANELMODE_ENGLISH;
   params->dialogParam.osk_prohibit_flags = 0;
}

void oskutil_write_message(oskutil_params *params, const wchar_t* msg)
{
   params->inputFieldInfo.osk_inputfield_message = (uint16_t*)msg;
}

void oskutil_write_initial_message(oskutil_params *params, const wchar_t* msg)
{
   params->inputFieldInfo.osk_inputfield_starttext = (uint16_t*)msg;
}

bool oskutil_start(oskutil_params *params) 
{
   if (params->flags & OSK_IN_USE)
   {
      RARCH_WARN("OSK util already initialized and in use\n");
      return true;
   }

   int ret = 0;

   ret = sys_memory_container_create(&params->containerid, params->osk_memorycontainer);

   if (ret < 0)
      goto do_deinit;

   params->outputInfo.osk_callback_return_param = CELL_OSKDIALOG_INPUT_FIELD_RESULT_OK;
   params->outputInfo.osk_callback_num_chars = 256;
   params->outputInfo.osk_callback_return_string = (uint16_t *)params->text_buf;

   memset(params->text_buf, 0, sizeof(*params->text_buf));

   params->inputFieldInfo.osk_inputfield_max_length = CELL_OSKDIALOG_STRING_SIZE;	

   oskutil_create_activation_parameters(params);

   if (!oskutil_enable_key_layout())
      return (false);

   ret = pOskLoadAsync(params->containerid, &params->dialogParam, &params->inputFieldInfo);

   if (ret < 0)
      goto do_deinit;

   params->flags |= OSK_IN_USE;

   return true;

do_deinit:
   RARCH_ERR("Could not properly initialize OSK util.\n");
   return false;
}
#endif

/*============================================================
  RetroArch PS3 INPUT DRIVER 
  ============================================================ */

static void ps3_input_free_input(void *data)
{
   if (!data)
      return;

#ifndef __CELLOS_LV2__
   cellPadEnd();
#endif
#ifdef HAVE_MOUSE
   //cellMouseEnd();
#endif
}

static void ps3_input_set_keybinds(void *data, unsigned device,
      unsigned port, unsigned id, unsigned keybind_action)
{
   uint64_t *key = &g_settings.input.binds[port][id].joykey;
   //uint64_t joykey = *key;
   size_t arr_size = sizeof(platform_keys) / sizeof(platform_keys[0]);

   (void)device;

   if (keybind_action & (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BIND))
      *key = g_settings.input.binds[port][id].def_joykey;

   if (keybind_action & (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS))
   {
      for (int i = 0; i < RARCH_CUSTOM_BIND_LIST_END; i++)
      {
         g_settings.input.binds[port][i].id = i;
         g_settings.input.binds[port][i].def_joykey = platform_keys[i].joykey;
         g_settings.input.binds[port][i].joykey = g_settings.input.binds[port][i].def_joykey;
      }
   }
   
   if (keybind_action & (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL))
   {
      struct platform_bind *ret = (struct platform_bind*)data;

      if (ret->joykey == NO_BTN)
         strlcpy(ret->desc, "No button", sizeof(ret->desc));
      else
      {
         for (size_t i = 0; i < arr_size; i++)
         {
            if (platform_keys[i].joykey == ret->joykey)
            {
               strlcpy(ret->desc, platform_keys[i].desc, sizeof(ret->desc));
               return;
            }
         }
         strlcpy(ret->desc, "Unknown", sizeof(ret->desc));
      }
   }
}

static void* ps3_input_init(void)
{
   ps3_input_t *ps3 = (ps3_input_t*)calloc(1, sizeof(*ps3));
   if (!ps3)
      return NULL;

   cellPadInit(MAX_PADS);
#ifdef HAVE_MOUSE
   cellMouseInit(MAX_MICE);
#endif

   return ps3;
}

static bool ps3_input_key_pressed(void *data, int key)
{
   return (g_extern.lifecycle_state & (1ULL << key));
}

static uint64_t ps3_input_get_capabilities(void *data)
{
   uint64_t caps = 0;

   caps |= (1 << RETRO_DEVICE_JOYPAD);
#ifdef HAVE_MOUSE
   caps |= (1 << RETRO_DEVICE_MOUSE);
#endif
   caps |= (1 << RETRO_DEVICE_ANALOG);
   caps |= (1 << RETRO_DEVICE_SENSOR_ACCELEROMETER);

   return caps;
}

static bool ps3_input_set_sensor_state(void *data, unsigned port, enum retro_sensor_action action, unsigned event_rate)
{
   CellPadInfo2 pad_info;
   (void)event_rate;

   switch (action)
   {
      case RETRO_SENSOR_ACCELEROMETER_ENABLE:
         cellPadGetInfo2(&pad_info);
         if ((pad_info.device_capability[port] & CELL_PAD_CAPABILITY_SENSOR_MODE) != CELL_PAD_CAPABILITY_SENSOR_MODE)
            return false;

         cellPadSetPortSetting(port, CELL_PAD_SETTING_SENSOR_ON);
         return true;
      case RETRO_SENSOR_ACCELEROMETER_DISABLE:
         cellPadSetPortSetting(port, 0);
         return true;

      default:
         return false;
   }
}

static bool ps3_input_set_rumble(void *data, unsigned port, enum retro_rumble_effect effect, uint16_t strength)
{
   CellPadActParam params;

   switch (effect)
   {
      case RETRO_RUMBLE_WEAK:
         if (strength > 1)
            strength = 1;
         params.motor[0] = strength;
         break;
      case RETRO_RUMBLE_STRONG:
         if (strength > 255)
            strength = 255;
         params.motor[1] = strength;
   }

   cellPadSetActDirect(port, &params);

   return true;
}

const input_driver_t input_ps3 = {
   ps3_input_init,
   ps3_input_poll,
   ps3_input_state,
   ps3_input_key_pressed,
   ps3_input_free_input,
   ps3_input_set_keybinds,
   ps3_input_set_sensor_state,
   ps3_input_get_capabilities,
   "ps3",

   NULL,
   ps3_input_set_rumble,
   NULL,
};
