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

#include "android_general.h"
#include "input_autodetect.h"

uint64_t keycode_lut[LAST_KEYCODE];

bool volume_enable;

static void input_autodetect_get_device_name(void *data, char *buf, size_t size, int id)
{
   struct android_app *android_app = (struct android_app*)data;
   buf[0] = '\0';

   JavaVM *vm = android_app->activity->vm;
   JNIEnv *env = NULL;
   (*vm)->AttachCurrentThread(vm, &env, 0);

   jclass input_device_class = NULL;
   FIND_CLASS(env, input_device_class, "android/view/InputDevice");
   if (!input_device_class)
      goto end;

   jmethodID method = NULL;
   GET_STATIC_METHOD_ID(env, method, input_device_class, "getDevice", "(I)Landroid/view/InputDevice;");
   if (!method)
      goto end;

   jobject device = NULL;
   CALL_OBJ_STATIC_METHOD_PARAM(env, device, input_device_class, method, (jint)id);
   if (!device)
   {
      RARCH_ERR("Failed to find device for ID: %d\n", id);
      goto end;
   }

   jmethodID getName = NULL;
   GET_METHOD_ID(env, getName, input_device_class, "getName", "()Ljava/lang/String;");
   if (!getName)
      goto end;

   jobject name = NULL;
   CALL_OBJ_METHOD(env, name, device, getName);
   if (!name)
   {
      RARCH_ERR("Failed to find name for device ID: %d\n", id);
      goto end;
   }

   const char *str = (*env)->GetStringUTFChars(env, name, 0);
   if (str)
      strlcpy(buf, str, size);
   (*env)->ReleaseStringUTFChars(env, name, str);

end:
   (*vm)->DetachCurrentThread(vm);
}

void input_autodetect_init (void)
{
   for(int j = 0; j < LAST_KEYCODE; j++)
      keycode_lut[j] = 0;

   volume_enable = true;
}

void input_autodetect_setup (void *data, char *msg, size_t sizeof_msg, unsigned port, unsigned id, int source)
{
   struct android_app *android_app = (struct android_app*)data;
   // Hack - we have to add '1' to the bit mask here because
   // RETRO_DEVICE_ID_JOYPAD_B is 0
 
   char name_buf[256];
   name_buf[0] = 0;

   if (port > MAX_PADS)
   {
      snprintf(msg, sizeof_msg, "Max number of pads reached.\n");
      return;
   }

   /* eight 8-bit values are packed into one uint64_t
    * one for each of the 8 pads */
   uint8_t shift = 8 + (port * 8);

   g_settings.input.dpad_emulation[port] = DPAD_EMULATION_LSTICK;

   char *current_ime = android_app->current_ime;

   if (g_settings.input.autodetect_enable)
   {
      input_autodetect_get_device_name(android_app, name_buf, sizeof(name_buf), id);

      if (strstr(name_buf, "Logitech"))
      {
         if (strstr(name_buf, "RumblePad 2"))
         {
            snprintf(msg, sizeof_msg, "RetroPad #%d is: RumblePad 2.\n", port);
            keycode_lut[AKEYCODE_BUTTON_2]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_1]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_9]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_10] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
            keycode_lut[AKEYCODE_BUTTON_3]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_4]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_5]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_6]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_7]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
            keycode_lut[AKEYCODE_BUTTON_8]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
            keycode_lut[AKEYCODE_BUTTON_11] |= ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_12] |= ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
         }
      }
      else if (strstr(name_buf, "HuiJia  USB GamePad"))
      {
         snprintf(msg, sizeof_msg, "RetroPad #%d is: HuiJia USB Gamepad.\n", port);
         keycode_lut[AKEYCODE_BUTTON_3]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_4]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_9] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_10] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_1]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_7] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_8] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
      }
      else if (strstr(name_buf, "Microsoft"))
      {
         if (strstr(name_buf, "Dual Strike"))
         {
            snprintf(msg, sizeof_msg, "RetroPad #%d is: Sidewinder Dual Strike.\n", port);
            keycode_lut[AKEYCODE_BUTTON_4] |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_2] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_6] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_5] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_3] |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_1] |= ((RETRO_DEVICE_ID_JOYPAD_X) << shift);
            keycode_lut[AKEYCODE_BUTTON_7] |= ((RETRO_DEVICE_ID_JOYPAD_L) << shift);
            keycode_lut[AKEYCODE_BUTTON_8] |= ((RETRO_DEVICE_ID_JOYPAD_R) << shift);
            keycode_lut[AKEYCODE_BUTTON_9] |= ((RETRO_DEVICE_ID_JOYPAD_L2) << shift);
         }
         else if (strstr(name_buf, "SideWinder"))
         {
            snprintf(msg, sizeof_msg, "RetroPad #%d is: Sidewinder.\n", port);
            keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_R2] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_L2] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_L1] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_R1] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_Z]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_C]  |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_11] |= ((RETRO_DEVICE_ID_JOYPAD_L3+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_12] |= ((RETRO_DEVICE_ID_JOYPAD_R3+1) << shift);
         }
      }
      else if (strstr(name_buf, "WiseGroup") && strstr(name_buf, "Dual USB Joypad"))
      {
         snprintf(msg, sizeof_msg, "RetroPad #%d is: WiseGroup PS2 to USB.\n", port);
         keycode_lut[AKEYCODE_BUTTON_3]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_4]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_10] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_9]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_1]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_7]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_8]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_5]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_6]  |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_11] |= ((RETRO_DEVICE_ID_JOYPAD_L3+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_12] |= ((RETRO_DEVICE_ID_JOYPAD_R3+1) << shift);
      }
      else if (strstr(name_buf, "PLAYSTATION(R)3") || strstr(name_buf, "Dualshock3")
            || strstr(name_buf,"Sixaxis") ||
            (strstr(name_buf, "Gamepad 0") || strstr(name_buf, "Gamepad 1") || 
               strstr(name_buf, "Gamepad 2") || strstr(name_buf, "Gamepad 3")))
      {
         bool do_invert = (strstr(name_buf, "Gamepad 0") || strstr(name_buf, "Gamepad 1") || 
               strstr(name_buf, "Gamepad 2") || strstr(name_buf, "Gamepad 3"));

         snprintf(msg, sizeof_msg, "RetroPad #%d is: DualShock3/Sixaxis.\n", port);
         g_settings.input.dpad_emulation[port] = DPAD_EMULATION_NONE;
         keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
         keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
         keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
         keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);

         if (do_invert)
         {
            keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
         }
         else
         {
            keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
         }
         keycode_lut[AKEYCODE_BUTTON_SELECT] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)  << shift);

         if (do_invert)
         {
            keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
         }
         else
         {
            keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
         }
         keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_L2] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
         keycode_lut[AKEYCODE_BUTTON_R2] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
         keycode_lut[AKEYCODE_BUTTON_THUMBL] |= ((RETRO_DEVICE_ID_JOYPAD_L3+1)     << shift);
         keycode_lut[AKEYCODE_BUTTON_THUMBR] |= ((RETRO_DEVICE_ID_JOYPAD_R3+1)     << shift);
      }
      else if (strstr(name_buf, "MOGA"))
      {
         snprintf(msg, sizeof_msg, "RetroPad #%d is: MOGA.\n", port);
         g_settings.input.dpad_emulation[port] = DPAD_EMULATION_NONE;
         keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
         keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
         keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
         keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_SELECT] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)  << shift);
         keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
      }
      else if (strstr(name_buf, "Sony Navigation Controller"))
      {
         snprintf(msg, sizeof_msg, "RetroPad #%d is: PS Move Navi.\n", port);
         keycode_lut[AKEYCODE_BUTTON_7]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_8]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_5]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_6]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);

         keycode_lut[AKEYCODE_BUTTON_11]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_9]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_L3+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_15]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_14]  |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);
         keycode_lut[AKEYCODE_UNKNOWN]   |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
      }
      else if (strstr(name_buf, "CYPRESS USB"))
      {
         snprintf(msg, sizeof_msg, "RetroPad #%d is: Saturn USB.\n", port);
         keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_C]  |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_Z]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_L1]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_R1]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_L2]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
      }
      else if (strstr(name_buf, "Mayflash Wii Classic"))
      {
         snprintf(msg, sizeof_msg, "RetroPad #%d is: Mayflash Wii Classic.\n", port);
         g_settings.input.dpad_emulation[port] = DPAD_EMULATION_NONE;
         keycode_lut[AKEYCODE_BUTTON_12] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_14] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_13] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_15] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_4] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_3] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_2] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_1] |= ((RETRO_DEVICE_ID_JOYPAD_A+1)  << shift);
         keycode_lut[AKEYCODE_BUTTON_5] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_6] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_7] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_8] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_9] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_10] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
      }
      else if (strstr(name_buf, "Toodles 2008 ChImp"))
      {
         snprintf(msg, sizeof_msg, "RetroPad #%d is: Chimp Board.\n", port);
         keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_Z] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_C] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_L2] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_R2] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
      }
      else if (strstr(name_buf, "keypad-game-zeus"))
      {
         volume_enable = false;
         snprintf(msg, sizeof_msg, "RetroPad #%d is: Xperia Play.\n", port);
         keycode_lut[AKEYCODE_DPAD_CENTER] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
         keycode_lut[AKEYCODE_BACK] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
         keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
         keycode_lut[AKEYCODE_DPAD_UP] |= ((RETRO_DEVICE_ID_JOYPAD_UP+1) << shift);
         keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1) << shift);
         keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1) << shift);
         keycode_lut[AKEYCODE_DPAD_RIGHT] |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_SELECT] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_L1] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_R1] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
      }
      if (strstr(current_ime, "com.ccpcreations.android.WiiUseAndroid"))
      {
         // Player 1
         snprintf(msg, sizeof_msg, "RetroPad #%d, %d, %d, %d is: Wiimote (IME).\n", 
               port, port + 1, port + 2, port + 3);
         snprintf(name_buf, sizeof(name_buf), "ccpcreations WiiUse");
         g_settings.input.dpad_emulation[port] = DPAD_EMULATION_NONE;
         keycode_lut[AKEYCODE_DPAD_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
         keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
         keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
         keycode_lut[AKEYCODE_DPAD_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
         keycode_lut[AKEYCODE_1]         |= ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
         keycode_lut[AKEYCODE_2]         |= ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
         keycode_lut[AKEYCODE_3]         |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
         keycode_lut[AKEYCODE_4]         |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
         keycode_lut[AKEYCODE_5]         |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
         keycode_lut[AKEYCODE_6]         |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)   << shift);
         keycode_lut[AKEYCODE_M]         |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)  << shift);
         keycode_lut[AKEYCODE_P]         |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
         keycode_lut[AKEYCODE_E]         |= ((RETRO_DEVICE_ID_JOYPAD_Y+1)       << shift);
         keycode_lut[AKEYCODE_B]         |= ((RETRO_DEVICE_ID_JOYPAD_X+1)       << shift);
         keycode_lut[AKEYCODE_F]         |= ((RETRO_DEVICE_ID_JOYPAD_B+1)       << shift);
         keycode_lut[AKEYCODE_G]         |= ((RETRO_DEVICE_ID_JOYPAD_A+1)       << shift);
         keycode_lut[AKEYCODE_C]         |= ((RETRO_DEVICE_ID_JOYPAD_L+1)       << shift);
         keycode_lut[AKEYCODE_LEFT_BRACKET]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1)       << shift);
         keycode_lut[AKEYCODE_RIGHT_BRACKET] |= ((RETRO_DEVICE_ID_JOYPAD_R2+1)       << shift);
         keycode_lut[AKEYCODE_Z]         |= ((RETRO_DEVICE_ID_JOYPAD_R+1)       << shift);
         keycode_lut[AKEYCODE_H]         |= ((RARCH_RESET+1)                    << shift);
         keycode_lut[AKEYCODE_W]         |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
         keycode_lut[AKEYCODE_S]         |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
         keycode_lut[AKEYCODE_A]         |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
         keycode_lut[AKEYCODE_D]         |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
         keycode_lut[AKEYCODE_C]         |= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
         keycode_lut[AKEYCODE_Z]         |= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);

         //player 2
         shift += 8;
         volume_enable = false;
         keycode_lut[AKEYCODE_I]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
         keycode_lut[AKEYCODE_K] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
         keycode_lut[AKEYCODE_J] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
         keycode_lut[AKEYCODE_O]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
         keycode_lut[AKEYCODE_COMMA] |= ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
         keycode_lut[AKEYCODE_PERIOD] |= ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
         keycode_lut[AKEYCODE_VOLUME_UP] |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
         keycode_lut[AKEYCODE_VOLUME_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
         keycode_lut[AKEYCODE_MEDIA_PREVIOUS] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
         keycode_lut[AKEYCODE_MEDIA_NEXT] |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
         keycode_lut[AKEYCODE_MEDIA_PLAY] |= ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
         keycode_lut[AKEYCODE_MEDIA_STOP] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
         keycode_lut[AKEYCODE_ENDCALL] |= ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
         keycode_lut[AKEYCODE_CALL] |= ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
         keycode_lut[AKEYCODE_PLUS] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
         keycode_lut[AKEYCODE_MINUS] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
         keycode_lut[AKEYCODE_BACKSLASH] |= ((RARCH_RESET+1)      << shift);
         keycode_lut[AKEYCODE_L] |= ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
         keycode_lut[AKEYCODE_R] |= ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
         keycode_lut[AKEYCODE_SEARCH] |= ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
         keycode_lut[AKEYCODE_TAB] |= ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);

         //player 3
         shift += 8;
         keycode_lut[AKEYCODE_PAGE_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
         keycode_lut[AKEYCODE_PAGE_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
         keycode_lut[AKEYCODE_MEDIA_REWIND] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
         keycode_lut[AKEYCODE_MEDIA_FAST_FORWARD]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
         keycode_lut[AKEYCODE_SOFT_LEFT]|= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);
         keycode_lut[AKEYCODE_SOFT_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
         keycode_lut[AKEYCODE_BUTTON_THUMBR]|= ((RETRO_DEVICE_ID_JOYPAD_START+1)    << shift);
         keycode_lut[AKEYCODE_BUTTON_THUMBL]|= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)    << shift);
         keycode_lut[AKEYCODE_SPACE]|= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
         keycode_lut[AKEYCODE_SYM]|= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
         keycode_lut[AKEYCODE_EXPLORER]|= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
         keycode_lut[AKEYCODE_ENVELOPE]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
         keycode_lut[AKEYCODE_BUTTON_X]|= ((RETRO_DEVICE_ID_JOYPAD_X+1)    << shift);
         keycode_lut[AKEYCODE_BUTTON_Y]|= ((RETRO_DEVICE_ID_JOYPAD_Y+1)    << shift);
         keycode_lut[AKEYCODE_BUTTON_A]|= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
         keycode_lut[AKEYCODE_BUTTON_B]|= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);
         keycode_lut[AKEYCODE_BUTTON_L1]|= ((RETRO_DEVICE_ID_JOYPAD_L+1)    << shift);
         keycode_lut[AKEYCODE_BUTTON_R1]|= ((RETRO_DEVICE_ID_JOYPAD_R+1)    << shift);
         keycode_lut[AKEYCODE_BUTTON_L2]|= ((RETRO_DEVICE_ID_JOYPAD_L2+1)    << shift);
         keycode_lut[AKEYCODE_BUTTON_R2]|= ((RETRO_DEVICE_ID_JOYPAD_R2+1)    << shift);

         //player 4
         shift += 8;
         keycode_lut[AKEYCODE_N]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
         keycode_lut[AKEYCODE_Q] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
         keycode_lut[AKEYCODE_T] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
         keycode_lut[AKEYCODE_APOSTROPHE]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
         keycode_lut[AKEYCODE_NOTIFICATION]|= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);
         keycode_lut[AKEYCODE_MUTE]|= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
         keycode_lut[AKEYCODE_BUTTON_START]|= ((RETRO_DEVICE_ID_JOYPAD_START+1)    << shift);
         keycode_lut[AKEYCODE_BUTTON_SELECT]|= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)    << shift);
         keycode_lut[AKEYCODE_CLEAR]|= ((RARCH_RESET+1)    << shift);
         keycode_lut[AKEYCODE_CAPS_LOCK]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
         keycode_lut[AKEYCODE_SCROLL_LOCK] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
         //keycode_lut[AKEYCODE_T] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift); -- Left meta
         //keycode_lut[AKEYCODE_APOSTROPHE]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift); -- right meta
         keycode_lut[AKEYCODE_META_FUNCTION_ON] |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
         keycode_lut[AKEYCODE_SYSRQ] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
         keycode_lut[AKEYCODE_BREAK] |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
         keycode_lut[AKEYCODE_MOVE_HOME] |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_C] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
         keycode_lut[AKEYCODE_BUTTON_Z] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
         keycode_lut[AKEYCODE_GRAVE] |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
         keycode_lut[AKEYCODE_MEDIA_PAUSE] |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);
      }
      else if (strstr(current_ime, "SixaxisIME"))
      {
         snprintf(msg, sizeof_msg, "RetroPad #%d is: SixAxis Bluetooth (IME).\n", port);
         snprintf(name_buf, sizeof(name_buf), "dancingpixelstudios.SixAxis");
         g_settings.input.dpad_emulation[port] = DPAD_EMULATION_NONE;
         keycode_lut[AKEYCODE_DPAD_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
         keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
         keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
         keycode_lut[AKEYCODE_DPAD_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
         keycode_lut[AKEYCODE_4] |= ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
         keycode_lut[AKEYCODE_5]         |= ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
         keycode_lut[AKEYCODE_6]         |= ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
         keycode_lut[AKEYCODE_7]         |= ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
         keycode_lut[AKEYCODE_BACK]      |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)  << shift);
         keycode_lut[AKEYCODE_DPAD_CENTER]      |= ((RETRO_DEVICE_ID_JOYPAD_START+1)  << shift);
         keycode_lut[AKEYCODE_8]      |= ((RETRO_DEVICE_ID_JOYPAD_L3+1)  << shift);
         keycode_lut[AKEYCODE_9]      |= ((RETRO_DEVICE_ID_JOYPAD_R3+1)  << shift);
         keycode_lut[AKEYCODE_0]      |= ((RETRO_DEVICE_ID_JOYPAD_Y+1)  << shift);
         keycode_lut[AKEYCODE_3]      |= ((RETRO_DEVICE_ID_JOYPAD_B+1)  << shift);
         keycode_lut[AKEYCODE_2]      |= ((RETRO_DEVICE_ID_JOYPAD_A+1)  << shift);
         keycode_lut[AKEYCODE_1]      |= ((RETRO_DEVICE_ID_JOYPAD_X+1)  << shift);
         keycode_lut[AKEYCODE_HOME]      |= ((RARCH_RESET+1)  << shift);
      }
      else if (source == AINPUT_SOURCE_TOUCHSCREEN)
      {
         snprintf(msg, sizeof_msg, "RetroPad #%d is: Touchscreen.\n", port);
      }
      else if (source == AINPUT_SOURCE_MOUSE)
      {
         snprintf(msg, sizeof_msg, "RetroPad #%d is: Mouse.\n", port);
      }
      else if (source == AINPUT_SOURCE_KEYBOARD)
      {
         snprintf(msg, sizeof_msg, "RetroPad #%d is: Keyboard.\n", port);
         // Keyboard
         // TODO: Map L2/R2/L3/R3

         keycode_lut[AKEYCODE_Z] |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
         keycode_lut[AKEYCODE_A] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
         keycode_lut[AKEYCODE_SHIFT_RIGHT] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
         keycode_lut[AKEYCODE_ENTER] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
         keycode_lut[AKEYCODE_DPAD_UP] |= ((RETRO_DEVICE_ID_JOYPAD_UP+1) << shift);
         keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1) << shift);
         keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1) << shift);
         keycode_lut[AKEYCODE_DPAD_RIGHT] |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1) << shift);
         keycode_lut[AKEYCODE_X] |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
         keycode_lut[AKEYCODE_S] |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
         keycode_lut[AKEYCODE_Q] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
         keycode_lut[AKEYCODE_W] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);

         /* Misc control scheme */
         keycode_lut[AKEYCODE_F2] |= ((RARCH_SAVE_STATE_KEY+1) << shift);
         keycode_lut[AKEYCODE_F4] |= ((RARCH_LOAD_STATE_KEY+1) << shift);
         keycode_lut[AKEYCODE_F7] |= ((RARCH_STATE_SLOT_PLUS+1) << shift);
         keycode_lut[AKEYCODE_F6] |= ((RARCH_STATE_SLOT_MINUS+1) << shift);
         keycode_lut[AKEYCODE_SPACE] |= ((RARCH_FAST_FORWARD_KEY+1) << shift);
         keycode_lut[AKEYCODE_L] |= ((RARCH_FAST_FORWARD_HOLD_KEY+1) << shift);
         keycode_lut[AKEYCODE_BREAK] |= ((RARCH_PAUSE_TOGGLE+1) << shift);
         keycode_lut[AKEYCODE_K] |= ((RARCH_FRAMEADVANCE+1) << shift);
         keycode_lut[AKEYCODE_H] |= ((RARCH_RESET+1) << shift);
         keycode_lut[AKEYCODE_R] |= ((RARCH_REWIND+1) << shift);
         keycode_lut[AKEYCODE_F9] |= ((RARCH_MUTE+1) << shift);

         keycode_lut[AKEYCODE_ESCAPE] |= ((RARCH_QUIT_KEY+1) << shift);
         keycode_lut[AKEYCODE_BACK] |= ((RARCH_QUIT_KEY+1) << shift);
      }
   }

   if (name_buf[0] != 0)
      RARCH_LOG("Device %d: %s, port: %d.\n", id, name_buf, port);

   if (msg[0] == 0 && name_buf[0] != 0)
      snprintf(msg, sizeof_msg, "HID [%s] unbound.\n", name_buf);
}
