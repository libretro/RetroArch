/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

item rmenu_items[MAX_NO_OF_CONTROLS_SETTINGS] =
{
#ifdef __CELLOS_LV2__
   {
      SETTING_CHANGE_RESOLUTION,                                        /* enum ID of item */
      "Resolution",                                                     /* item label */
      "",                                                               /* setting label */
      "INFO - Change the display resolution.",				/* item comment */
   },
#endif
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
   {
      SETTING_SHADER_PRESETS,
      "Shader Preset (CGP)",
      "",
      "INFO - Select a [CG Preset] script.",
   },
   {
      SETTING_SHADER,
      "Selected shader #1",
      "",
      "INFO - Select a shader as [Shader #1]. NOTE: Some shaders might be\ntoo slow at 1080p. If you experience any slowdown, try another shader.",
   },
   {
      SETTING_SHADER_2,
      "Selected shader #2",
      "",
      "INFO - Select a shader as [Shader #2]. NOTE: Some shaders might be\ntoo slow at 1080p. If you experience any slowdown, try another shader.",
   },
#endif
   {
      SETTING_FONT_SIZE,
      "Font Size",
      "",
      "INFO - Increase or decrease the font size.",
   },
   {
      SETTING_KEEP_ASPECT_RATIO,
      "Aspect Ratio",
      "",
      "INFO - [Aspect Ratio] is set to 'Scaled (4:3)'.",
   },
   {
      SETTING_HW_TEXTURE_FILTER,
      "Hardware Filtering #1",
      "",
      "INFO - Hardware filtering #1 is set to 'Bilinear'.",
   },
#ifdef HAVE_FBO
   {
      SETTING_HW_TEXTURE_FILTER_2,
      "Hardware Filtering #2",
      "",
      "INFO - Hardware filtering #2 is set to 'Bilinear'.",
   },
   {
      SETTING_SCALE_ENABLED,
      "Custom Scaling/Dual Shaders",
      "",
      "INFO - [Custom Scaling] is set to 'ON' - 2x shaders will look much\nbetter, and you can select a shader for [Shader #2].",
   },
   {
      SETTING_SCALE_FACTOR,
      "Custom Scaling Factor",
      "",
      "INFO - [Custom Scaling Factor] is set to '2x'.",
   },
#endif
#ifdef _XBOX1
   {
      SETTING_FLICKER_FILTER,
      "Flicker Filter",
      "",
      "INFO - Toggle the [Flicker Filter].",
   },
   {
      SETTING_SOFT_DISPLAY_FILTER,
      "Soft Display Filter",
      "",
      "INFO - Toggle the [Soft Display Filter].",
   },
#endif
   {
      SETTING_HW_OVERSCAN_AMOUNT,
      "Overscan",
      "",
      "INFO - Adjust or decrease [Overscan]. Set this to higher than 0.000\nif the screen doesn't fit on your TV/monitor.",
   },
   {
      SETTING_THROTTLE_MODE,
      "Throttle Mode",
      "",
      "INFO - [Throttle Mode] is set to 'ON' - VSync is enabled and sound\nis turned on.",
   },
   {
      SETTING_TRIPLE_BUFFERING,
      "Triple Buffering",
      "",
      "INFO - [Triple Buffering] is set to 'ON' - faster graphics/shaders at\nthe possible expense of input lag.",
   },
   {
      SETTING_ENABLE_SCREENSHOTS,
      "Screenshots Feature",
      "",
      "INFO - [Screenshots] feature is set to 'OFF'.",
   },
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
   {
      SETTING_SAVE_SHADER_PRESET,
      "SAVE SETTINGS AS CGP PRESET ",
      "",
      "INFO - Save the current video settings to a [CG Preset] (CGP) file.",
   },
   {
      SETTING_APPLY_SHADER_PRESET_ON_STARTUP,
      "APPLY SHADER PRESET ON STARTUP",
      "",
      "INFO - Automatically load the currently selected [CG Preset] file on startup.",
   },
#endif
   {
      SETTING_DEFAULT_VIDEO_ALL,
      "DEFAULT",
      "",
      "INFO - Set all [General Video Settings] back to their 'DEFAULT' values.",
   },
   {
      SETTING_SOUND_MODE,
      "Sound Output",
      "",
      "INFO - [Sound Output] is set to 'Normal' - normal audio output will be\nused.",
   },
#ifdef HAVE_RSOUND
   {
      SETTING_RSOUND_SERVER_IP_ADDRESS,
      "RSound Server IP Address",
      "",
      "INFO - Enter the IP Address of the [RSound Audio Server]. IP address\nmust be an IPv4 32-bits address, eg: '192.168.1.7'.",
   },
#endif
   {
      SETTING_ENABLE_CUSTOM_BGM,
      "Custom BGM Feature",
      "",
      "INFO - [Custom BGM] feature is set to 'ON'.",
   },
   {
      SETTING_DEFAULT_AUDIO_ALL,
      "DEFAULT",
      "",
      "INFO - Set all [General Audio Settings] back to their 'DEFAULT' values.",
   },
   {
      SETTING_EMU_CURRENT_SAVE_STATE_SLOT, 
      "Current save state slot", 
      "",
      "INFO - Set the current savestate slot (can also be configured ingame).",
   },
      {
      SETTING_EMU_SHOW_DEBUG_INFO_MSG, 
      "Debug Info messages", 
      "",
      "INFO - Show onscreen debug messages.",
   },
   {
      SETTING_EMU_SHOW_INFO_MSG, 
      "Info messages", 
      "",
      "INFO - Show onscreen info messages in the menu.",
   },
   {
      SETTING_ZIP_EXTRACT, 
      "ZIP extract", 
      "",
      "INFO - Select the [ZIP Extract] mode. This setting controls how ZIP files are extracted.",
   },
   {
      SETTING_RARCH_DEFAULT_EMU,
      "Default emulator core",
      "",
      "INFO - Select a default emulator core to launch at start-up.",
   },
   {
      SETTING_EMU_DEFAULT_ALL,
      "DEFAULT",
      "",
      "INFO - Set [all RetroArch settings] back to their 'DEFAULT' values.",
   },
   {
      SETTING_EMU_REWIND_ENABLED,
      "Rewind",
      "",
      "INFO - [Rewind] feature is set to 'OFF'.",
   },
   {
      SETTING_EMU_VIDEO_DEFAULT_ALL,
      "DEFAULT",
      "",
      "INFO - Set [all RetroArch Video settings] back to their 'DEFAULT' values.",
   },
   {
      SETTING_EMU_AUDIO_MUTE,
      "Mute Audio",
      "",
      "INFO - [Mute Audio] is set to 'OFF'.",
   },
   {
      SETTING_EMU_AUDIO_DEFAULT_ALL,
      "DEFAULT",
      "",
      "INFO - Set [all RetroArch Audio settings] back to their 'DEFAULT' values.",
   },
   {
      SETTING_PATH_DEFAULT_ROM_DIRECTORY,
      "Startup ROM Directory",
      "",
      "INFO - Set the default [Startup ROM directory]. NOTE: You will have to\nrestart the emulator for this change to have any effect.",
   },
   {
      SETTING_PATH_SAVESTATES_DIRECTORY,
      "Savestate Directory",
      "",
      "INFO - Set the default path where all the savestate files will be saved to.",
   },
   {
      SETTING_PATH_SRAM_DIRECTORY,
      "SRAM Directory",
      "",
      "INFO - Set the default SRAM (SaveRAM) directory path. All the\nbattery backup saves will be stored in this directory.",
   },
#ifdef HAVE_XML
   {
      SETTING_PATH_CHEATS,
      "Cheatfile Directory",
      "",
      "INFO - Set the default [Cheatfile directory] path. All CHT (cheat) files\nwill be stored here.",
   },
#endif
   {
      SETTING_PATH_SYSTEM,
      "System Directory",
      "",
      "INFO - Set the default [System directory] path. System files like\nBIOS files, etc. will be stored here.",
   },
   {
      SETTING_ENABLE_SRAM_PATH,
      "Custom SRAM Dir Path",
      "",
      "INFO - [Custom SRAM Dir Path] feature is set to 'OFF'.",
      1
   },
   {
      SETTING_ENABLE_STATE_PATH,
      "Custom Save State Dir Path",
      "",
      "INFO - [Custom Save State Dir Path] feature is set to 'OFF'.",
   },
   {
      SETTING_PATH_DEFAULT_ALL,
      "DEFAULT",
      "",
      "INFO - Set [all Path settings] back to their 'DEFAULT' values.",
   },
   {
      SETTING_CONTROLS_SCHEME,
      "Control Scheme Preset",
      "",
      "",
   },
   {
      SETTING_CONTROLS_NUMBER,
      "Controller No",
      "",
      "",
   },
   {
      SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_B,
      "B Button",
      "",
      "",
   },
   {
      SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_Y,
      "Y Button",
      "",
      "",
   },
   {
      SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_SELECT,
      "Select button",
      "",
      "",
   },
   {
      SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_START,
      "Start button",
      "",
      "",
   },
   {
      SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_UP,
      "D-Pad Up",
      "",
      "",
   },
   {
      SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_DOWN,
      "D-Pad Down",
      "",
      "",
   },
   {
      SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_LEFT,
      "D-Pad Left",
      "",
      "",
   },
   {
      SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_RIGHT,
      "D-Pad Right",
      "",
      "",
   },
   {
      SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_A,
      "A Button",
      "",
      "",
   },
   {
      SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_X,
      "X Button",
      "",
      "",
   },
   {
      SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L,
      "L Button",
      "",
      "",
   },
   {
      SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R,
      "R Button",
      "",
      "",
   },
   {
      SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L2,
      "L2 Button",
      "",
      "",
   },
   {
      SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R2,
      "R2 Button",
      "",
      "",
   },
   {
      SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L3,
      "L3 Button",
      "",
      "",
   },
   {
      SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R3,
      "R3 Button",
      "",
      "",
   },
   {
      SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS,
      "SAVE CUSTOM CONTROLS",
      "",
      "INFO - Save the custom control settings.",
   },
   {
      SETTING_CONTROLS_DEFAULT_ALL,
      "DEFAULT",
      "",
      "INFO - Set all [Controls] back to their 'DEFAULT' values.",
   }
};
