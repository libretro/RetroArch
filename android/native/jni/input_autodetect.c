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

#include "../../../frontend/frontend_android.h"
#include "jni_macros.h"
#include "input_autodetect.h"

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

void input_autodetect_setup(void *data, char *msg, size_t sizeof_msg, unsigned port, unsigned id, int source, bool *primary)
{
   struct android_app *android_app = (struct android_app*)data;
 
   unsigned device;
   char name_buf[256];
   name_buf[0] = 0;

   if (port > MAX_PADS)
   {
      snprintf(msg, sizeof_msg, "Max number of pads reached.\n");
      return;
   }

   char *current_ime = android_app->current_ime;
   input_autodetect_get_device_name(android_app, name_buf, sizeof(name_buf), id);
   RARCH_LOG("device name: %s\n", name_buf);

   /* Shitty hack put back in again */
   if (strstr(name_buf, "keypad-game-zeus") || strstr(name_buf, "keypad-zeus"))
   {
      if (zeus_id < 0)
      {
         RARCH_LOG("zeus_pad 1 detected: %d\n", id);
         zeus_id = id;
         zeus_port = port;
      }
      else
      {
         RARCH_LOG("zeus_pad 2 detected: %d\n", id);
         zeus_second_id = id;
      }
   }

   device = 0;

   if (strstr(name_buf,"Logitech") && strstr(name_buf, "RumblePad 2"))
      device = DEVICE_LOGITECH_RUMBLEPAD2;
   else if (strstr(name_buf, "Logitech") && strstr(name_buf, "Dual Action"))
      device = DEVICE_LOGITECH_DUAL_ACTION;
   else if (strstr(name_buf, "Logitech") && strstr(name_buf, "Precision"))
      device = DEVICE_LOGITECH_PRECISION_GAMEPAD;
   else if (strstr(name_buf, "iControlPad-")) // followed by a 4 (hex) char HW id
      device = DEVICE_ICONTROLPAD_HID_JOYSTICK;
   else if (strstr(name_buf, "SEGA VIRTUA STICK High Grade"))
      device = DEVICE_SEGA_VIRTUA_STICK_HIGH_GRADE;
   else if (strstr(name_buf, "TTT THT Arcade console 2P USB Play"))
      device = DEVICE_TTT_THT_ARCADE;
   else if (strstr(name_buf, "TOMMO NEOGEOX Arcade Stick"))
      device = DEVICE_TOMMO_NEOGEOX_ARCADE;
   else if (strstr(name_buf, "Onlive Wireless Controller"))
      device = DEVICE_ONLIVE_WIRELESS_CONTROLLER;
   else if (strstr(name_buf, "MadCatz") && strstr(name_buf, "PC USB Wired Stick"))
      device = DEVICE_MADCATZ_PC_USB_STICK;
   else if (strstr(name_buf, "Logicool") && strstr(name_buf, "RumblePad 2"))
      device = DEVICE_LOGICOOL_RUMBLEPAD2;
   else if (strstr(name_buf, "Sun4i-keypad"))
      device = DEVICE_IDROID_X360;
   else if (strstr(name_buf, "Zeemote") && strstr(name_buf, "Steelseries free"))
      device = DEVICE_ZEEMOTE_STEELSERIES;
   else if (strstr(name_buf, "HuiJia  USB GamePad"))
      device = DEVICE_HUIJIA_USB_SNES;
   else if (strstr(name_buf, "Smartjoy Family Super Smartjoy 2"))
      device = DEVICE_SUPER_SMARTJOY;
   else if (strstr(name_buf, "Jess Tech Dual Analog Rumble Pad"))
      device = DEVICE_SAITEK_RUMBLE_P480;
   else if (strstr(name_buf, "mtk-kpd"))
      device = DEVICE_MUCH_IREADGO_I5;
   else if (strstr(name_buf, "Microsoft"))
   {
      if (strstr(name_buf, "Dual Strike"))
         device = DEVICE_MS_SIDEWINDER_DUAL_STRIKE;
      else if (strstr(name_buf, "SideWinder"))
         device = DEVICE_MS_SIDEWINDER;
      else if (strstr(name_buf, "X-Box 360") || strstr(name_buf, "X-Box")
            || strstr(name_buf, "Xbox 360 Wireless Receiver"))
         device = DEVICE_MS_XBOX;
   }
   else if (strstr(name_buf, "WiseGroup"))
   {
      if (strstr(name_buf, "TigerGame") || strstr(name_buf, "Game Controller Adapter")
            || strstr(name_buf, "JC-PS102U") || strstr(name_buf, "Dual USB Joypad"))
      {
         if (strstr(name_buf, "WiseGroup"))
            device = DEVICE_WISEGROUP_PLAYSTATION2;
         else if (strstr(name_buf, "JC-PS102U"))
            device = DEVICE_JCPS102_PLAYSTATION2;
         else
            device = DEVICE_GENERIC_PLAYSTATION2_CONVERTER;
      }
   }
   else if (strstr(name_buf, "PLAYSTATION(R)3") || strstr(name_buf, "Dualshock3")
         || strstr(name_buf,"Sixaxis") || strstr(name_buf, "Gasia,Co") ||
         (strstr(name_buf, "Gamepad 0") || strstr(name_buf, "Gamepad 1") || 
          strstr(name_buf, "Gamepad 2") || strstr(name_buf, "Gamepad 3")))
   {
      if (strstr(name_buf, "Gamepad 0") || strstr(name_buf, "Gamepad 1") || 
            strstr(name_buf, "Gamepad 2") || strstr(name_buf, "Gamepad 3"))
         device = DEVICE_PLAYSTATION3_VERSION1;
      else
         device = DEVICE_PLAYSTATION3_VERSION2;
   }
   else if (strstr(name_buf, "MOGA"))
      device = DEVICE_MOGA;
   else if (strstr(name_buf, "Sony Navigation Controller"))
      device = DEVICE_PSMOVE_NAVI;
   else if (strstr(name_buf, "OUYA Game Controller"))
      device = DEVICE_OUYA;
   else if (strstr(name_buf, "adc joystick"))
      device = DEVICE_JXD_S7300B;
   else if (strstr(name_buf, "idroid:con"))
      device = DEVICE_IDROID_CON;
   else if (strstr(name_buf, "NYKO PLAYPAD PRO"))
      device = DEVICE_NYKO_PLAYPAD_PRO;
   else if (strstr(name_buf, "2-Axis, 8-Button"))
      device = DEVICE_GENIUS_MAXFIRE_G08XU;
   else if (strstr(name_buf, "USB,2-axis 8-button gamepad"))
      device = DEVICE_USB_2_AXIS_8_BUTTON_GAMEPAD;
   else if (strstr(name_buf, "BUFFALO BGC-FC801"))
      device = DEVICE_BUFFALO_BGC_FC801;
   else if (strstr(name_buf, "RetroUSB.com RetroPad"))
      device = DEVICE_RETROUSB_RETROPAD;
   else if (strstr(name_buf, "RetroUSB.com SNES RetroPort"))
      device = DEVICE_RETROUSB_SNES_RETROPORT;
   else if (strstr(name_buf, "CYPRESS USB"))
      device = DEVICE_CYPRESS_USB;
   else if (strstr(name_buf, "Mayflash Wii Classic"))
      device = DEVICE_MAYFLASH_WII_CLASSIC;
   else if (strstr(name_buf, "SZMy-power LTD CO.  Dual Box WII"))
      device = DEVICE_SZMY_POWER_DUAL_BOX_WII;
   else if (strstr(name_buf, "Toodles 2008 ChImp"))
      device = DEVICE_TOODLES_2008_CHIMP;
   else if (strstr(name_buf, "joy_key"))
      device = DEVICE_ARCHOS_GAMEPAD;
   else if (strstr(name_buf, "matrix_keyboard"))
      device = DEVICE_JXD_S5110;
   else if (strstr(name_buf, "keypad-zeus") || (strstr(name_buf, "keypad-game-zeus")))
      device = DEVICE_XPERIA_PLAY;
   else if (strstr(name_buf, "Broadcom Bluetooth HID"))
      device = DEVICE_BROADCOM_BLUETOOTH_HID;
   else if (strstr(name_buf, "USB Gamepad"))
      device = DEVICE_THRUST_PREDATOR;
   else if (strstr(name_buf, "DragonRise"))
      device = DEVICE_DRAGONRISE;
   else if (strstr(name_buf, "Thrustmaster T Mini"))
      device = DEVICE_THRUSTMASTER_T_MINI;
   else if (strstr(name_buf, "2Axes 11Keys Game  Pad"))
      device = DEVICE_TOMEE_NES_USB;
   else if (strstr(name_buf, "rk29-keypad") || strstr(name_buf, "GAMEMID"))
      device = DEVICE_GAMEMID;
   else if (strstr(name_buf, "USB Gamepad"))
      device = DEVICE_DEFENDER_GAME_RACER_CLASSIC;
   else if (strstr(name_buf, "HOLTEK JC - U912F vibration game"))
      device = DEVICE_HOLTEK_JC_U912F;
   else if (strstr(name_buf, "NVIDIA Controller"))
   {
      device = DEVICE_NVIDIA_SHIELD;
      port = 0; // Shield is always player 1.
      *primary = true;
   }

   if (strstr(current_ime, "net.obsidianx.android.mogaime"))
   {
      device = DEVICE_MOGA_IME;
      snprintf(name_buf, sizeof(name_buf), "MOGA IME");
   }
   else if (strstr(current_ime, "com.ccpcreations.android.WiiUseAndroid"))
   {
      device = DEVICE_CCPCREATIONS_WIIUSE_IME;
      snprintf(name_buf, sizeof(name_buf), "ccpcreations WiiUse");
   }
   else if (strstr(current_ime, "com.hexad.bluezime"))
   {
      device = DEVICE_ICONTROLPAD_BLUEZ_IME;
      snprintf(name_buf, sizeof(name_buf), "iControlpad SPP mode (using Bluez IME)");
   }

   if (source == AINPUT_SOURCE_KEYBOARD && device != DEVICE_XPERIA_PLAY)
      device = DEVICE_KEYBOARD_RETROPAD;

   if (driver.input->set_keybinds)
      driver.input->set_keybinds(driver.input_data, device, port, id,
            (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS));

   if (name_buf[0] != 0)
      snprintf(msg, sizeof_msg, "Port %d: %s.\n", port, name_buf);
}
