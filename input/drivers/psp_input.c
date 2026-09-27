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
#include <psp2/touch.h>
#include <psp2/ime_dialog.h>
#define VITA_NUM_SCANCODES 115 /* size of rarch_key_map_vita */
#define VITA_MAX_SCANCODE 0xE7
#define VITA_NUM_MODIFIERS 11 /* number of modifiers reported */
#define MOUSE_MAX_X 960
#define MOUSE_MAX_Y 544
/* Update to SCE_TOUCH_PORT_MAX_NUM to enable back touch polling */
#define VITA_MAX_TOUCH SCE_TOUCH_PORT_FRONT+1
#elif defined(PSP)
#include <pspctrl.h>
#endif

#include <string.h>

#include <boolean.h>
#include <libretro.h>
#include <retro_miscellaneous.h>
#include <encodings/utf.h>
#include <string/stdstring.h>

#include <defines/psp_defines.h>

#include "../input_driver.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../gfx/video_driver.h"

#if defined(VITA) && defined(HAVE_MENU)
#include "../../menu/menu_driver.h"
/* The system keyboard's text limit, in UTF-16 units. */
#define VITA_IME_TEXT_MAX 512
#endif

/* TODO/FIXME -
 * fix game focus toggle */

#if defined(SN_TARGET_PSP2) || defined(VITA)
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

typedef struct psp_input
{
   int keyboard_hid_handle;
   int mouse_hid_handle;
   int32_t mouse_x;
   int32_t mouse_y;
   int32_t mouse_x_delta;
   int32_t mouse_y_delta;
   uint8_t prev_keys[6];
#ifdef VITA
   SceTouchData touch[SCE_TOUCH_PORT_MAX_NUM];
   SceTouchPanelInfo panelInfo[SCE_TOUCH_PORT_MAX_NUM];
#ifdef HAVE_MENU
   SceWChar16 ime_title[SCE_IME_DIALOG_MAX_TITLE_LENGTH + 1];
   SceWChar16 ime_initial[1];
   SceWChar16 ime_text[VITA_IME_TEXT_MAX + 1];
   /* A surrogate pair is 4 bytes for 2 units, anything else at most
    * 3 bytes per unit, so this holds any conversion of ime_text. */
   char ime_utf8[VITA_IME_TEXT_MAX * 3 + 1];
   bool ime_open;       /* sceImeDialogInit succeeded, Term not yet run */
   bool ime_aborting;   /* the menu dropped the dialog while it was up */
   bool ime_declined;   /* could not open for this dialog: built-in OSK */
   bool ime_touch_hold; /* swallow touches until every finger lifts */
#endif
#endif
   bool keyboard_state[VITA_MAX_SCANCODE + 1];
   bool mouse_button_left;
   bool mouse_button_right;
   bool mouse_button_middle;
   bool sensors_enabled;
} psp_input_t;

#if defined(VITA) && defined(HAVE_MENU)
/* Native system keyboard (SceImeDialog) for menu text entry.
 *
 * The dialog is modal but not blocking: the OS animates and draws it
 * into our back buffer from the video driver's sceCommonDialogUpdate()
 * call (gxm_gfx.c), and it finishes on its own once the user presses
 * Enter or Close. It is driven from here, once per poll on the main
 * thread, and publishes INP_FLAG_NATIVE_KB_SHOWN while it is up so the
 * built-in OSK neither draws nor consumes input, the pad reads as idle
 * (psp_joypad.c) and the video driver knows to update the dialog.
 *
 * The typed text is handed over only on Enter, the same way the libnx
 * swkbd path does it: into the keyboard line, then a Return through
 * input_keyboard_event(). Close sends the Return with the line empty,
 * which is how the menu already treats Cancel under a native panel. */

static bool vita_ime_video_ready(void)
{
   /* Only the GXM driver makes the per-frame dialog update. Opened
    * under anything else, the panel would never appear and never
    * finish, leaving the menu waiting on it with the pad masked. */
   return string_is_equal(video_driver_get_ident(), "vita2d");
}

static void vita_ime_utf8_to_utf16(SceWChar16 *out, size_t out_units,
      const char *in)
{
   size_t pos = 0;

   while (*in && pos + 1 < out_units)
   {
      uint32_t c = utf8_walk(&in);

      if (c >= 0x10000)
      {
         if (c > 0x10FFFF || pos + 2 >= out_units)
            break;
         c         -= 0x10000;
         out[pos++] = (SceWChar16)(0xD800 | (c >> 10));
         out[pos++] = (SceWChar16)(0xDC00 | (c & 0x3FF));
      }
      else
         out[pos++] = (SceWChar16)c;
   }

   out[pos] = 0;
}

static void vita_ime_set_shown(bool shown)
{
   input_driver_state_t *input_st = input_state_get_ptr();

   if (shown)
      input_st->flags |=  INP_FLAG_NATIVE_KB_SHOWN;
   else
      input_st->flags &= ~INP_FLAG_NATIVE_KB_SHOWN;
}

static bool vita_ime_open(psp_input_t *psp)
{
   SceImeDialogParam param;
   struct menu_state *menu_st = menu_state_get_ptr();

   sceImeDialogParamInit(&param);

   vita_ime_utf8_to_utf16(psp->ime_title, ARRAY_SIZE(psp->ime_title),
         menu_st->input_dialog_kb_label);
   psp->ime_initial[0]  = 0;
   psp->ime_text[0]     = 0;

   param.type           = SCE_IME_TYPE_DEFAULT;
   param.dialogMode     = SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL;
   param.textBoxMode    = SCE_IME_DIALOG_TEXTBOX_MODE_DEFAULT;

   switch (menu_input_dialog_get_kb_text_type())
   {
      case MENU_INPUT_DIALOG_KB_TYPE_PASSWORD:
         param.textBoxMode = SCE_IME_DIALOG_TEXTBOX_MODE_PASSWORD;
         break;
      case MENU_INPUT_DIALOG_KB_TYPE_NUMBER:
         param.type        = SCE_IME_TYPE_NUMBER;
         break;
      default:
         break;
   }

   param.title           = psp->ime_title;
   param.maxTextLength   = VITA_IME_TEXT_MAX;
   param.initialText     = psp->ime_initial;
   param.inputTextBuffer = psp->ime_text;

   return sceImeDialogInit(&param) >= 0;
}

static void vita_ime_commit(psp_input_t *psp)
{
   size_t units                   = 0;
   size_t bytes                   = 0;
   input_driver_state_t *input_st = input_state_get_ptr();

   while (units < VITA_IME_TEXT_MAX && psp->ime_text[units])
      units++;

   /* A malformed surrogate stops the conversion; keep the valid
    * prefix it reports rather than dropping the whole entry. */
   utf16_conv_utf8((uint8_t*)psp->ime_utf8, &bytes,
         (const uint16_t*)psp->ime_text, units);

   input_keyboard_line_clear(input_st);
   if (bytes)
      input_keyboard_line_append(&input_st->keyboard_line,
            psp->ime_utf8, bytes);
}

static void vita_ime_poll(psp_input_t *psp)
{
   SceImeDialogResult result;
   input_driver_state_t *input_st = input_state_get_ptr();
   bool video_ready               = vita_ime_video_ready();
   bool want                      = menu_input_dialog_get_display_kb()
         && input_st->keyboard_line.enabled;

   if (video_ready)
      input_st->flags |=  INP_FLAG_NATIVE_KB_AVAIL;
   else
      input_st->flags &= ~INP_FLAG_NATIVE_KB_AVAIL;

   if (!psp->ime_open)
   {
      if (!want)
         psp->ime_declined = false;
      else if (!psp->ime_declined)
      {
         if (video_ready && vita_ime_open(psp))
         {
            psp->ime_open       = true;
            psp->ime_aborting   = false;
            psp->ime_touch_hold = true;
            vita_ime_set_shown(true);
         }
         else
            psp->ime_declined   = true;
      }
      return;
   }

   if (sceImeDialogGetStatus() == SCE_COMMON_DIALOG_STATUS_RUNNING)
   {
      /* The menu closed the dialog under us: dismiss the panel and
       * discard whatever it returns. */
      if (!want && !psp->ime_aborting)
      {
         sceImeDialogAbort();
         psp->ime_aborting = true;
      }
      return;
   }

   /* FINISHED, or NONE if the dialog went away without finishing. */
   memset(&result, 0, sizeof(result));
   sceImeDialogGetResult(&result);
   sceImeDialogTerm();
   psp->ime_open = false;
   vita_ime_set_shown(false);

   if (!want || psp->ime_aborting)
      return;

   if (result.button == SCE_IME_DIALOG_BUTTON_ENTER)
      vita_ime_commit(psp);

   input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);
}

static void vita_ime_free(psp_input_t *psp)
{
   if (psp->ime_open)
   {
      sceImeDialogAbort();
      sceImeDialogTerm();
      psp->ime_open = false;
   }
   input_state_get_ptr()->flags &=
      ~(INP_FLAG_NATIVE_KB_SHOWN | INP_FLAG_NATIVE_KB_AVAIL);
}
#endif

static void vita_input_poll(void *data)
{
   psp_input_t *psp     = (psp_input_t*)data;
   unsigned int i       = 0;
   int port             = 0;
   int key_sym          = 0;
   unsigned key_code    = 0;
   uint8_t mod_code     = 0;
   uint16_t mod         = 0;
   uint8_t modifiers[2] = { 0, 0 };
   bool key_held        = false;
   bool ime_open        = false;
   int mouse_velocity_x = 0;
   int mouse_velocity_y = 0;
   SceHidKeyboardReport k_reports[SCE_HID_MAX_REPORT];
   SceHidMouseReport m_reports[SCE_HID_MAX_REPORT];

#ifdef HAVE_MENU
   vita_ime_poll(psp);
   ime_open             = psp->ime_open;
#endif

   if (psp->keyboard_hid_handle > 0)
   {
      int numReports = sceHidKeyboardRead(
            psp->keyboard_hid_handle,
            (SceHidKeyboardReport**)&k_reports, SCE_HID_MAX_REPORT);

      if (numReports < 0)
         psp->keyboard_hid_handle = 0;
      else if (numReports)
      {
         modifiers[0] = k_reports[numReports - 1].modifiers[0];
         modifiers[1] = k_reports[numReports - 1].modifiers[1];
         mod          = 0;
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
            key_sym     = (int)modifier_lut[i][0];
            mod_code    = modifier_lut[i][1];
            key_code    = input_keymaps_translate_keysym_to_rk(key_sym);
            if (i < 8)
               key_held = (modifiers[0] & mod_code);
            else
               key_held = (modifiers[1] & mod_code);

            if (key_held && !(psp->keyboard_state[key_sym]))
            {
               psp->keyboard_state[key_sym] = true;
               if (!ime_open)
                  input_keyboard_event(true, key_code, 0, mod,
                        RETRO_DEVICE_KEYBOARD);
            }
            else if (!key_held && (psp->keyboard_state[key_sym]))
            {
               psp->keyboard_state[key_sym] = false;
               input_keyboard_event(false, key_code, 0, mod,
                     RETRO_DEVICE_KEYBOARD);
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
                  key_code = 
                     input_keymaps_translate_keysym_to_rk(
                           psp->prev_keys[i]);
                  input_keyboard_event(false, key_code, 0, mod,
                        RETRO_DEVICE_KEYBOARD);
               }
               if (key_sym)
               {
                  psp->keyboard_state[key_sym] = true;
                  key_code = 
                     input_keymaps_translate_keysym_to_rk(
                           key_sym);
                  /* The system keyboard owns text entry while it
                   * is up; a key typed on a USB/BT keyboard must not
                   * reach the keyboard line behind it. */
                  if (!ime_open)
                     input_keyboard_event(true, key_code, 0, mod,
                           RETRO_DEVICE_KEYBOARD);
               }
               psp->prev_keys[i] = key_sym;
            }
         }
      }
   }

   if (psp->mouse_hid_handle > 0)
   {
      int numReports = sceHidMouseRead(psp->mouse_hid_handle,
            (SceHidMouseReport**)&m_reports, SCE_HID_MAX_REPORT);

      if (numReports > 0)
      {
         for (i = 0; i <= numReports - 1; i++)
         {
            uint8_t buttons = m_reports[i].buttons;

            if (buttons & 0x1)
               psp->mouse_button_left = true;
            else
               psp->mouse_button_left = false;

            if (buttons & 0x2)
               psp->mouse_button_right = true;
            else
               psp->mouse_button_right = false;

            if (buttons & 0x4)
               psp->mouse_button_middle = true;
            else
               psp->mouse_button_middle = false;

            mouse_velocity_x += m_reports[i].rel_x;
            mouse_velocity_y += m_reports[i].rel_y;
         }
      }
   }

   psp->mouse_x_delta  = mouse_velocity_x;
   psp->mouse_y_delta  = mouse_velocity_y;
   psp->mouse_x       += mouse_velocity_x;
   psp->mouse_y       += mouse_velocity_y;
   if (psp->mouse_x < 0)
      psp->mouse_x     = 0;
   else if (psp->mouse_x > MOUSE_MAX_X)
      psp->mouse_x     = MOUSE_MAX_X;

   if (psp->mouse_y < 0)
      psp->mouse_y     = 0;
   else if (psp->mouse_y > MOUSE_MAX_Y)
      psp->mouse_y     = MOUSE_MAX_Y;

   for(port = 0; port < VITA_MAX_TOUCH; port++){
      sceTouchPeek(port, &psp->touch[port], 1);
   }

#ifdef HAVE_MENU
   /* Touches on the system keyboard are its own. Keep them from the
    * menu while it is up, and after it closes until every finger has
    * lifted, or the tap on its Enter key lands on a menu entry. */
   if (psp->ime_touch_hold)
   {
      bool touching = false;
      for (port = 0; port < VITA_MAX_TOUCH; port++)
      {
         if (psp->touch[port].reportNum)
            touching = true;
         psp->touch[port].reportNum = 0;
      }
      if (!ime_open && !touching)
         psp->ime_touch_hold = false;
   }
#endif
}

static int16_t vita_input_state(
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
   psp_input_t *psp           = (psp_input_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
      case RETRO_DEVICE_ANALOG:
         break;
#ifdef VITA
      case RETRO_DEVICE_KEYBOARD:
         return ((id < RETROK_LAST) && 
               psp->keyboard_state[rarch_keysym_lut[(enum retro_key)id]]);
      case RETRO_DEVICE_MOUSE:
      case RARCH_DEVICE_MOUSE_SCREEN:
         {
            bool screen = device == RARCH_DEVICE_MOUSE_SCREEN;
            int val     = 0;
            switch (id)
            {
               case RETRO_DEVICE_ID_MOUSE_LEFT:
                  return psp->mouse_button_left;
               case RETRO_DEVICE_ID_MOUSE_RIGHT:
                  return psp->mouse_button_right;
               case RETRO_DEVICE_ID_MOUSE_MIDDLE:
                  return psp->mouse_button_middle;
               case RETRO_DEVICE_ID_MOUSE_X:
                  if (screen)
                     return psp->mouse_x;

                  val                = psp->mouse_x_delta;
                  psp->mouse_x_delta = 0;
                  /* flush delta after it has been read */
                  break;
               case RETRO_DEVICE_ID_MOUSE_Y:
                  if (screen)
                     return psp->mouse_y;
                  val                = psp->mouse_y_delta;
                  psp->mouse_y_delta = 0;
                  /* flush delta after it has been read */
                  break;
            }
            return val;
         }
         break;

      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         {
            /* Same pointer state is reported for all ports. */
            if (idx < SCE_TOUCH_MAX_REPORT)
            {
               struct video_viewport vp    = {0};
               bool screen                 =
                  (device == RARCH_DEVICE_POINTER_SCREEN);
               int16_t res_x               = 0;
               int16_t res_y               = 0;
               int16_t res_screen_x        = 0;
               int16_t res_screen_y        = 0;
               float tmp_x, tmp_y;

               video_driver_get_viewport_info(&vp);
               tmp_x = (psp->touch[0].report[idx].x - psp->panelInfo[0].minAaX) * VIDEO_SCALE_W(vp.dims)/(psp->panelInfo[0].maxAaX - psp->panelInfo[0].minAaX);
               tmp_y = (psp->touch[0].report[idx].y - psp->panelInfo[0].minAaY) * VIDEO_SCALE_H(vp.dims)/(psp->panelInfo[0].maxAaY - psp->panelInfo[0].minAaY);

               if (video_driver_translate_coord_viewport_confined_wrap(
                        &vp,
                        (int)tmp_x,
                        (int)tmp_y,
                        &res_x, &res_y, &res_screen_x, &res_screen_y))
               {
                  if (screen)
                  {
                     res_x = res_screen_x;
                     res_y = res_screen_y;
                  }

                  switch (id)
                  {
                     case RETRO_DEVICE_ID_POINTER_X:
                        return res_x;
                     case RETRO_DEVICE_ID_POINTER_Y:
                        return res_y;
                     case RETRO_DEVICE_ID_POINTER_PRESSED:
                        return (idx < psp->touch[0].reportNum);
                     case RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN:
                        return input_driver_pointer_is_offscreen(res_x, res_y);
                     case RETRO_DEVICE_ID_POINTER_COUNT:
                        return psp->touch[0].reportNum;
                  }
               }
            }
         }
         break;

#endif
   }

   return 0;
}
#else
typedef struct psp_input
{
   void *empty;
} psp_input_t;
#endif

static void psp_input_free_input(void *data)
{
#if defined(VITA) && defined(HAVE_MENU)
   if (data)
      vita_ime_free((psp_input_t*)data);
#endif
   free(data);
}

static uint64_t psp_input_get_capabilities(void *data)
{
   return
#ifdef VITA
          (1 << RETRO_DEVICE_KEYBOARD)
        | (1 << RETRO_DEVICE_MOUSE)
        | (1 << RETRO_DEVICE_POINTER) |
#endif
          (1 << RETRO_DEVICE_JOYPAD)
        | (1 << RETRO_DEVICE_ANALOG);
}

#ifdef VITA
static bool psp_input_set_sensor_state(void *data, unsigned port,
      enum retro_sensor_action action, unsigned event_rate)
{
   psp_input_t *psp = (psp_input_t*)data;
	
   if (psp)
   {
      switch (action)
      {
         case RETRO_SENSOR_ILLUMINANCE_DISABLE:
            return true;
         case RETRO_SENSOR_ACCELEROMETER_DISABLE:
         case RETRO_SENSOR_GYROSCOPE_DISABLE:
            if (psp->sensors_enabled)
            {
               psp->sensors_enabled = false;
               sceMotionMagnetometerOff();
               sceMotionStopSampling();
            }
            return true;
         case RETRO_SENSOR_ACCELEROMETER_ENABLE:
         case RETRO_SENSOR_GYROSCOPE_ENABLE:
            if (!psp->sensors_enabled)
            {
               psp->sensors_enabled = true;
               sceMotionStartSampling();
               sceMotionMagnetometerOn();
            }
            return true;
         case RETRO_SENSOR_DUMMY:
         case RETRO_SENSOR_ILLUMINANCE_ENABLE:
            break;
      }
   }
   
   return false;
}

static float psp_input_get_sensor_input(void *data,
      unsigned port, unsigned id)
{
   SceMotionSensorState sixaxis;
   
   psp_input_t *psp = (psp_input_t*)data;
	
   if (!psp || !psp->sensors_enabled)
      return 0.0f;

   if (id >= RETRO_SENSOR_ACCELEROMETER_X && id <= RETRO_SENSOR_GYROSCOPE_Z)
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

static void *vita_input_initialize(const char *joypad_driver)
{
   unsigned i;
   psp_input_t *psp = (psp_input_t*)calloc(1, sizeof(*psp));
   if (!psp)
      return NULL;

   sceHidKeyboardEnumerate(&(psp->keyboard_hid_handle), 1);
   sceHidMouseEnumerate(&(psp->mouse_hid_handle), 1);

   input_keymaps_init_keyboard_lut(rarch_key_map_vita);
   for (i = 0; i <= VITA_MAX_SCANCODE; i++)
      psp->keyboard_state[i] = false;
   for (i = 0; i < 6; i++)
      psp->prev_keys[i]      = 0;
   psp->mouse_x              = 0;
   psp->mouse_y              = 0;

   for(i = 0; i < SCE_TOUCH_PORT_MAX_NUM; i++){
      if (i < VITA_MAX_TOUCH)
         sceTouchSetSamplingState(i, 1);
      else
         sceTouchSetSamplingState(i, 0);
      sceTouchDisableTouchForce(i);
      /*sceTouchEnableTouchForce(i);*/
      sceTouchGetPanelInfo(i, &psp->panelInfo[i]);
      RARCH_DBG("[vita]: touch panel %d info, active x min/max %d/%d \n",
                i, psp->panelInfo[i].minAaX, psp->panelInfo[i].maxAaX);
      RARCH_DBG("[vita]: touch panel %d info, active y min/max %d/%d \n",
                i, psp->panelInfo[i].minAaY, psp->panelInfo[i].maxAaY);
      RARCH_DBG("[vita]: touch panel %d info, display x min/max %d/%d \n",
                i, psp->panelInfo[i].minDispX, psp->panelInfo[i].maxDispX);
      RARCH_DBG("[vita]: touch panel %d info, display y min/max %d/%d \n",
                i, psp->panelInfo[i].minDispY, psp->panelInfo[i].maxDispY);
   }

   return psp;
}
#else
static void* psp_input_initialize(const char *joypad_driver)
{
   psp_input_t *psp = (psp_input_t*)calloc(1, sizeof(*psp));
   if (!psp)
      return NULL;
   return psp;
}
#endif

input_driver_t input_psp = {
#ifdef VITA
   vita_input_initialize,
   vita_input_poll,
   vita_input_state,
#else
   psp_input_initialize,
   NULL,                         /* poll */
   NULL,                         /* input_state */
#endif
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

   NULL,                         /* grab_mouse */
   NULL,
   NULL
};
