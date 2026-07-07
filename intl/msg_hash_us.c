/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include <compat/strl.h>
#include <string/stdstring.h>

#include "../msg_hash.h"
#include "../verbosity.h"

#ifdef RARCH_INTERNAL
#include "../configuration.h"
#include "../config.def.h"

int msg_hash_get_help_us_enum(enum msg_hash_enums msg, char *s, size_t len)
{
    if (msg <= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_END &&
        msg >= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN)
    {
       unsigned idx = msg - MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN;

       switch (idx)
       {
          case RARCH_ENABLE_HOTKEY:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_ENABLE_HOTKEY), len);
             break;
          default:
             if (!s || !*s)
                strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
             break;
       }
    }
    else
    {
       settings_t *settings = config_get_ptr();

       switch (msg)
       {
          case MENU_ENUM_LABEL_INPUT_RETROPAD_BINDS:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_INPUT_RETROPAD_BINDS), len);
             break;
          case MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_INPUT_POLL_TYPE_BEHAVIOR), len);
             break;
          case MENU_ENUM_LABEL_DISK_IMAGE_APPEND:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_DISK_IMAGE_APPEND), len);
             break;
          case MENU_ENUM_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_ACCOUNTS_RETRO_ACHIEVEMENTS), len);
             break;
          case MENU_ENUM_LABEL_USER_LANGUAGE:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_USER_LANGUAGE), len);
             break;
          case MENU_ENUM_LABEL_FILE_BROWSER_CONFIG:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_FILE_BROWSER_CONFIG), len);
             break;
          case MENU_ENUM_LABEL_FILE_BROWSER_COMPRESSED_ARCHIVE:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_FILE_BROWSER_COMPRESSED_ARCHIVE), len);
             break;
          case MENU_ENUM_LABEL_FILE_BROWSER_RECORD_CONFIG:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_FILE_BROWSER_RECORD_CONFIG), len);
             break;
          case MENU_ENUM_LABEL_FILE_BROWSER_CURSOR:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_FILE_BROWSER_CURSOR), len);
             break;
          case MENU_ENUM_LABEL_FILE_CONFIG:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_FILE_CONFIG), len);
             break;
          case MENU_ENUM_LABEL_SCAN_THIS_DIRECTORY:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_SCAN_THIS_DIRECTORY), len);
             break;
          case MENU_ENUM_LABEL_USE_THIS_DIRECTORY:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_USE_THIS_DIRECTORY), len);
             break;
          case MENU_ENUM_LABEL_DUMMY_ON_CORE_SHUTDOWN:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_DUMMY_ON_CORE_SHUTDOWN), len);
             break;
          case MENU_ENUM_LABEL_CORE_INFO_SAVESTATE_BYPASS:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_CORE_INFO_SAVESTATE_BYPASS), len);
             break;
          case MENU_ENUM_LABEL_PARENT_DIRECTORY:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_PARENT_DIRECTORY), len);
             break;
          case MENU_ENUM_LABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_FILE_BROWSER_OPEN_UWP_PERMISSIONS), len);
             break;
          case MENU_ENUM_LABEL_FILE_BROWSER_SHADER_PRESET:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER_PRESET), len);
             break;
          case MENU_ENUM_LABEL_FILE_BROWSER_SHADER:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER), len);
             break;
          case MENU_ENUM_LABEL_FILE_BROWSER_REMAP:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_FILE_BROWSER_REMAP), len);
             break;
          case MENU_ENUM_LABEL_FILE_BROWSER_CHEAT:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_FILE_BROWSER_CHEAT), len);
             break;
          case MENU_ENUM_LABEL_FILE_BROWSER_OVERLAY:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_FILE_BROWSER_OVERLAY), len);
             break;
          case MENU_ENUM_LABEL_FILE_BROWSER_RDB:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_FILE_BROWSER_RDB), len);
             break;
          case MENU_ENUM_LABEL_FILE_BROWSER_FONT:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_FILE_BROWSER_FONT), len);
             break;
          case MENU_ENUM_LABEL_FILE_BROWSER_PLAIN_FILE:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_FILE_BROWSER_PLAIN_FILE), len);
             break;
          case MENU_ENUM_LABEL_FILE_BROWSER_MOVIE_OPEN:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_FILE_BROWSER_MOVIE_OPEN), len);
             break;
          case MENU_ENUM_LABEL_FILE_BROWSER_MUSIC_OPEN:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_FILE_BROWSER_MUSIC_OPEN), len);
             break;
          case MENU_ENUM_LABEL_FILE_BROWSER_IMAGE:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE), len);
             break;
          case MENU_ENUM_LABEL_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER), len);
             break;
          case MENU_ENUM_LABEL_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION), len);
             break;
          case MENU_ENUM_LABEL_FILE_BROWSER_CORE:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE), len);
             break;
          case MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_FILE_BROWSER_DIRECTORY), len);
             break;
          case MENU_ENUM_LABEL_CORE_LIST:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_CORE_LIST), len);
             break;
          case MENU_ENUM_LABEL_INPUT_DRIVER:
             {
                const char *lbl = settings ? settings->arrays.input_driver : NULL;

                if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DRIVER_UDEV)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_INPUT_DRIVER_UDEV), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DRIVER_LINUXRAW)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_INPUT_DRIVER_LINUXRAW), len);
                else
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_INPUT_DRIVER_NO_DETAILS), len);
             }
             break;
          case MENU_ENUM_LABEL_JOYPAD_DRIVER:
             {
                const char *lbl = settings ? settings->arrays.input_joypad_driver : NULL;

                if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_JOYPAD_DRIVER_UDEV)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_UDEV), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_JOYPAD_DRIVER_LINUXRAW)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_LINUXRAW), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_JOYPAD_DRIVER_DINPUT)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_DINPUT), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_JOYPAD_DRIVER_XINPUT)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_XINPUT), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_JOYPAD_DRIVER_SDL)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_SDL), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_JOYPAD_DRIVER_PARPORT)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_PARPORT), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_JOYPAD_DRIVER_HID)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_HID), len);
             }
             break;
          case MENU_ENUM_LABEL_MENU_DRIVER:
             {
                const char *lbl = settings ? settings->arrays.menu_driver : NULL;

                if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_MENU_DRIVER_XMB)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_MENU_DRIVER_XMB), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_MENU_DRIVER_OZONE)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_MENU_DRIVER_OZONE), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_MENU_DRIVER_RGUI)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_MENU_DRIVER_RGUI), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_MENU_DRIVER_MATERIALUI)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_MENU_DRIVER_MATERIALUI), len);
             }
             break;

          case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_LOAD_CONTENT_LIST), len);
             break;
          case MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_LOAD_CONTENT_HISTORY), len);
             break;
          case MENU_ENUM_LABEL_VIDEO_DRIVER:
             {
                const char *lbl = settings ? settings->arrays.video_driver : NULL;

                if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DRIVER_GL1)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL1), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DRIVER_GL)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DRIVER_GL_CORE)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL_CORE), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DRIVER_VULKAN)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VULKAN), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DRIVER_SDL1)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL1), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DRIVER_SDL2)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL2), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DRIVER_METAL)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_METAL), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DRIVER_D3D8)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D8), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DRIVER_D3D9_CG)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_CG), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DRIVER_D3D9_HLSL)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_HLSL), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DRIVER_D3D10)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D10), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DRIVER_D3D11)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D11), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DRIVER_D3D12)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D12), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DRIVER_DISPMANX)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DISPMANX), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DRIVER_CACA)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_CACA), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DRIVER_EXYNOS)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_EXYNOS), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DRIVER_DRM)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DRM), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DRIVER_SUNXI)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SUNXI), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DRIVER_WIIU)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_WIIU), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DRIVER_SWITCH)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SWITCH), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DRIVER_VG)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VG), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DRIVER_GDI)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GDI), len);
                else
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_NO_DETAILS), len);
             }
             break;
          case MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER:
             {
                const char *lbl = settings ? settings->arrays.audio_resampler : NULL;

                if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER_SINC)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_SINC), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER_CC)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_CC), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER_NEAREST)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_NEAREST), len);
                else
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
             }
             break;
#ifdef HAVE_MICROPHONE
          case MENU_ENUM_LABEL_MICROPHONE_RESAMPLER_DRIVER:
             {
                const char *lbl = settings ? settings->arrays.microphone_resampler : NULL;

                if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER_SINC)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_SINC), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER_CC)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_CC), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER_NEAREST)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_NEAREST), len);
                else
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
             }
             break;
#endif
          case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PRESET), len);
             break;
          case MENU_ENUM_LABEL_VIDEO_SHADER_SCALE_PASS:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SCALE_PASS), len);
             break;
          case MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_SHADER_NUM_PASSES), len);
             break;
          case MENU_ENUM_LABEL_VIDEO_SHADER_PASS:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PASS), len);
             break;
          case MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_CONFIG_SAVE_ON_EXIT), len);
             break;
          case MENU_ENUM_LABEL_QUIT_RETROARCH:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_QUIT_RETROARCH), len);
             break;
          case MENU_ENUM_LABEL_VIDEO_SHADER_FILTER_PASS:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_SHADER_FILTER_PASS), len);
             break;
          case MENU_ENUM_LABEL_AUTOSAVE_INTERVAL:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUTOSAVE_INTERVAL), len);
             break;
          case MENU_ENUM_LABEL_REPLAY_CHECKPOINT_INTERVAL:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_REPLAY_CHECKPOINT_INTERVAL), len);
             break;
          case MENU_ENUM_LABEL_REPLAY_CHECKPOINT_DESERIALIZE:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_REPLAY_CHECKPOINT_DESERIALIZE), len);
             break;
          case MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_INPUT_ADC_TYPE), len);
             break;
          case MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_LIBRETRO_LOG_LEVEL), len);
             break;
          case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_SHADER_APPLY_CHANGES), len);
             break;
          case MENU_ENUM_LABEL_SHADER_WATCH_FOR_CHANGES:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_SHADER_WATCH_FOR_CHANGES), len);
             break;
          case MENU_ENUM_LABEL_VIDEO_FILTER:
#ifdef HAVE_FILTERS_BUILTIN
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_FILTER_BUILTIN), len);
#else
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_FILTER), len);
#endif
             break;
          case MENU_ENUM_LABEL_AUDIO_DRIVER:
             {
                const char *lbl = settings ? settings->arrays.audio_driver : NULL;

                if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DRIVER_ALSA)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSA), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DRIVER_ALSATHREAD)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSATHREAD), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DRIVER_TINYALSA)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_TINYALSA), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DRIVER_RSOUND)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_RSOUND), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DRIVER_OSS)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_OSS), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DRIVER_ROAR)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ROAR), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DRIVER_AL)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_AL), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DRIVER_SL)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_SL), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DRIVER_DSOUND)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_DSOUND), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DRIVER_WASAPI)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_WASAPI), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DRIVER_PULSE)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PULSE), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DRIVER_PIPEWIRE)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PIPEWIRE), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DRIVER_JACK)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_JACK), len);
                else
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
             }
             break;
          case MENU_ENUM_LABEL_AUDIO_DEVICE:
             {
                /* Device help is audio driver dependent. */
                const char *lbl = settings ? settings->arrays.audio_driver : NULL;

                if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DRIVER_ALSA)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_ALSA), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DRIVER_ALSATHREAD)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_ALSA), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DRIVER_TINYALSA)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_ALSA), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DRIVER_OSS)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_OSS), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DRIVER_JACK)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_JACK), len);
                else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DRIVER_RSOUND)))
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_RSOUND), len);
                else
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_DEVICE), len);
             }
             break;
          case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_REFRESH_RATE_AUTO), len);
             break;
          case MENU_ENUM_LABEL_FASTFORWARD_RATIO:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_FASTFORWARD_RATIO), len);
             break;
          case MENU_ENUM_LABEL_VRR_RUNLOOP_ENABLE:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VRR_RUNLOOP_ENABLE), len);
             break;
          case MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_MONITOR_INDEX), len);
             break;
          case MENU_ENUM_LABEL_AUDIO_VOLUME:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_VOLUME), len);
             break;
          case MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_RATE_CONTROL_DELTA), len);
             break;
          case MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_MAX_TIMING_SKEW), len);
             break;
          case MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_DISABLE_COMPOSITION), len);
             break;
          case MENU_ENUM_LABEL_VIDEO_THREADED:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_THREADED), len);
             break;
          case MENU_ENUM_LABEL_VIDEO_FRAME_DELAY:
             snprintf(s, len, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY),
                   MAXIMUM_FRAME_DELAY);
             break;
          case MENU_ENUM_LABEL_VIDEO_FRAME_DELAY_AUTO:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY_AUTO), len);
             break;
          case MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_HARD_SYNC_FRAMES), len);
             break;
          case MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_BLACK_FRAME_INSERTION), len);
             break;
          case MENU_ENUM_LABEL_VIDEO_BFI_DARK_FRAMES:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_BFI_DARK_FRAMES), len);
             break;
          case MENU_ENUM_LABEL_VIDEO_SHADER_SUBFRAMES:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SUBFRAMES), len);
             break;
          case MENU_ENUM_LABEL_VIDEO_SCAN_SUBFRAMES:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_SCAN_SUBFRAMES), len);
             break;
          case MENU_ENUM_LABEL_SAVEFILE_DIRECTORY:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_SAVEFILE_DIRECTORY), len);
             break;
          case MENU_ENUM_LABEL_INPUT_PREFER_FRONT_TOUCH:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_INPUT_PREFER_FRONT_TOUCH), len);
             break;
          case MENU_ENUM_LABEL_SUSPEND_SCREENSAVER_ENABLE:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_SUSPEND_SCREENSAVER_ENABLE), len);
             break;
          case MENU_ENUM_LABEL_NETPLAY_START_AS_SPECTATOR:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_NETPLAY_START_AS_SPECTATOR), len);
             break;
          case MENU_ENUM_LABEL_NETPLAY_CHECK_FRAMES:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_NETPLAY_CHECK_FRAMES), len);
             break;
          case MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_MIN), len);
             break;
          case MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_RANGE), len);
             break;
          case MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_MAX_SWAPCHAIN_IMAGES), len);
             break;
          case MENU_ENUM_LABEL_UI_COMPANION_START_ON_BOOT:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_UI_COMPANION_START_ON_BOOT), len);
             break;
          case MENU_ENUM_LABEL_CHEAT_START_OR_CONT:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_CHEAT_START_OR_CONT), len);
             break;
          case MENU_ENUM_LABEL_SAVE_STATE:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_SAVE_STATE), len);
             break;
          case MENU_ENUM_LABEL_LOAD_STATE:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_LOAD_STATE), len);
             break;
#ifdef HAVE_LAKKA
          case MENU_ENUM_LABEL_TIMEZONE:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_TIMEZONE), len);
             break;
#endif
          case MENU_ENUM_LABEL_MIDI_INPUT:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_MIDI_INPUT), len);
             break;
          case MENU_ENUM_LABEL_MIDI_OUTPUT:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_MIDI_OUTPUT), len);
             break;
#ifdef __linux__
          case MENU_ENUM_LABEL_GAMEMODE_ENABLE:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_GAMEMODE_ENABLE), len);
             break;
#endif

#ifdef ANDROID
          case MENU_ENUM_LABEL_INPUT_SELECT_PHYSICAL_KEYBOARD:
             strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_SELECT_PHYSICAL_KEYBOARD), len);
             break;
#endif
          case MENU_ENUM_LABEL_INPUT_TURBO_MODE:
             {
                unsigned mode = settings ? settings->uints.input_turbo_mode : INPUT_TURBO_MODE_LAST;
                if (mode == INPUT_TURBO_MODE_CLASSIC)
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_TURBO_MODE_CLASSIC), len);
                else if (mode == INPUT_TURBO_MODE_CLASSIC_TOGGLE)
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_TURBO_MODE_CLASSIC_TOGGLE), len);
                else if (mode == INPUT_TURBO_MODE_SINGLEBUTTON)
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON), len);
                else if (mode == INPUT_TURBO_MODE_SINGLEBUTTON_HOLD)
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON_HOLD), len);
                else
                   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
             }
             break;
          default:
             if (!s || !*s)
                strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
             return -1;
       }
    }

    return 0;
}
#endif

#ifdef HAVE_MENU
#if defined(MSG_HASH_HAVE_STRTAB)
#undef MSG_HASH
#define MSG_HASH(Id, str) str,
static const char *const msg_hash_us_lbl_strs[] = {
#include "msg_hash_lbl.h"
#define SETTINGS_DEF_STRINGS_PASS
#define S_BOOL(f, T, n, d, sd, df, c, us, sub) n,
#define S_BOOL_NS(f, T, n, d, sd, df, c, us) n,
#define S_UINT(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us, sub) n,
#define S_UINT_NS(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us) n,
#define S_INT(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us, sub) n,
#define S_INT_NS(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us) n,
#define S_FLOAT(f, T, n, d, rnd, sd, df, c, mn, mx, st, ok, rp, us, sub) n,
#define S_FLOAT_NS(f, T, n, d, rnd, sd, df, c, mn, mx, st, ok, rp, us) n,
#define S_STRING(f, T, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us, sub) n,
#define S_STRING_NS(f, T, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us) n,
#define S_DIR(f, T, n, d, el, sd, c, sta, us, sub) n,
#define S_DIR_NS(f, T, n, d, el, sd, c, sta, us) n,
#define S_STRING_P(f, T, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us, sub) n,
#define S_STRING_P_NS(f, T, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us) n,
#define S_PATH(f, T, n, d, sd, c, vals, rp, ui, us, sub) n,
#define S_PATH_NS(f, T, n, d, sd, c, vals, rp, ui, us) n,
#define S_PATH_DS(f, T, n, df2, sd, c, vals, rp, ui, us, sub) n,
#define S_PATH_DS_NS(f, T, n, df2, sd, c, vals, rp, ui, us) n,
#define S_ACTION(T, n, us, sub) n,
#define S_ACTION_NS(T, n, us) n,
#define S_BOOL_LV(f, T, TV, n, d, sd, df, c, us, sub) n,
#define S_BOOL_LV_NS(f, T, TV, n, d, sd, df, c, us) n,
#define S_FLOAT_LV(f, T, TV, n, d, rnd, sd, df, c, us, sub) n,
#define S_FLOAT_LV_NS(f, T, TV, n, d, rnd, sd, df, c, us) n,
#define S_STRING_LV(f, T, TV, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us, sub) n,
#define S_STRING_LV_NS(f, T, TV, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us) n,
#define S_ACTION_LV(T, TV, n, sd, ok, rp, c, us, sub) n,
#define S_ACTION_LV_NS(T, TV, n, sd, ok, rp, c, us) n,
#define S_BOOL_EX(f, T, n, d, sd, df, c, ok, rp, sta, sel, lf, rt, ui, us, sub) n,
#define S_BOOL_EX_NS(f, T, n, d, sd, df, c, ok, rp, sta, sel, lf, rt, ui, us) n,
#define S_UINT_EX(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui, us, sub) n,
#define S_UINT_EX_NS(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui, us) n,
#define S_INT_EX(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui, us, sub) n,
#define S_INT_EX_NS(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui, us) n,
#define S_FLOAT_EX(f, T, n, d, rnd, sd, df, c, mn, mx, st, ok, rp, sta, sel, lf, rt, ui, us, sub) n,
#define S_FLOAT_EX_NS(f, T, n, d, rnd, sd, df, c, mn, mx, st, ok, rp, sta, sel, lf, rt, ui, us) n,
#define S_ACTION_EX(T, n, sd, ok, rp, c, us, sub) n,
#define S_ACTION_EX_NS(T, n, sd, ok, rp, c, us) n,
#include "../settings/settings_def_video_fullscreen.h"
#include "../settings/settings_def_video_sync.h"
#include "../settings/settings_def_menu_show_restart.h"
#include "../settings/settings_def_quit_restart.h"
#include "../settings/settings_def_menu_throttle.h"
#include "../settings/settings_def_video_ctx_scaling.h"
#include "../settings/settings_def_input_sensors_extra.h"
#include "../settings/settings_def_netplay_advanced.h"
#include "../settings/settings_def_menu_main_state.h"
#include "../settings/settings_def_playlist_management.h"
#include "../settings/settings_def_menu_privacy.h"
#include "../settings/settings_def_menu_landscape.h"
#include "../settings/settings_def_cloud_sync_general.h"
#include "../settings/settings_def_overlay_appearance.h"
#include "../settings/settings_def_notification_views.h"
#include "../settings/settings_def_ozone_typography.h"
#include "../settings/settings_def_saving.h"
#include "../settings/settings_def_notification_positions.h"
#include "../settings/settings_def_menu_main_lists_4.h"
#include "../settings/settings_def_menu_main_lists_3.h"
#include "../settings/settings_def_ozone_extras.h"
#include "../settings/settings_def_menu_main_lists_2.h"
#include "../settings/settings_def_overlay_mouse.h"
#include "../settings/settings_def_overlay_lightgun.h"
#include "../settings/settings_def_input_turbo_fire.h"
#include "../settings/settings_def_ui_appearance.h"
#include "../settings/settings_def_menu_entry_display.h"
#include "../settings/settings_def_crt_switchres.h"
#include "../settings/settings_def_audio_state.h"
#include "../settings/settings_def_analog_deadzone.h"
#include "../settings/settings_def_desktop_menu.h"
#include "../settings/settings_def_audio_device.h"
#include "../settings/settings_def_ai_service_options.h"
#include "../settings/settings_def_video_output_misc.h"
#include "../settings/settings_def_netplay_visibility.h"
#include "../settings/settings_def_microphone_general.h"
#include "../settings/settings_def_menu_startup.h"
#include "../settings/settings_def_accessibility.h"
#include "../settings/settings_def_video_refresh_rate.h"
#include "../settings/settings_def_menu_header_footer.h"
#include "../settings/settings_def_logging.h"
#include "../settings/settings_def_frame_throttle_general.h"
#include "../settings/settings_def_refresh_autoswitch.h"
#include "../settings/settings_def_audio_skew.h"
#include "../settings/settings_def_wifi.h"
#include "../settings/settings_def_steam_presence.h"
#include "../settings/settings_def_playlist_sorting.h"
#include "../settings/settings_def_playlist_flags.h"
#include "../settings/settings_def_notification_enable.h"
#include "../settings/settings_def_rgui_particle_effect.h"
#include "../settings/settings_def_menu_savestate_resume.h"
#include "../settings/settings_def_menu_framebuffer_opacity.h"
#include "../settings/settings_def_input_haptics.h"
#include "../settings/settings_def_input_general.h"
#include "../settings/settings_def_ozone_appearance.h"
#include "../settings/settings_def_rgui_appearance.h"
#include "../settings/settings_def_playlist_display.h"
#include "../settings/settings_def_cheevos_general.h"
#include "../settings/settings_def_menu_appearance.h"
#include "../settings/settings_def_menu_visibility.h"
#include "../settings/settings_def_netplay_sync.h"
#include "../settings/settings_def_mic_wasapi.h"
#include "../settings/settings_def_ozone_sidebar.h"
#include "../settings/settings_def_input_bind_timeouts.h"
#include "../settings/settings_def_video_hdr_toggles.h"
#include "../settings/settings_def_rewind.h"
#include "../settings/settings_def_playlist_history.h"
#include "../settings/settings_def_menu_scroll.h"
#include "../settings/settings_def_menu_thumbnails.h"
#include "../settings/settings_def_input_turbo.h"
#include "../settings/settings_def_frame_delay.h"
#include "../settings/settings_def_audio_wasapi.h"
#include "../settings/settings_def_audio_resampler_quality.h"
#include "../settings/settings_def_wallpaper_opacity.h"
#include "../settings/settings_def_overlay_opacity.h"
#include "../settings/settings_def_widget_scale_windowed.h"
#include "../settings/settings_def_video_hdr.h"
#include "../settings/settings_def_video_window_offset.h"
#include "../settings/settings_def_vulkan_gpu_index.h"
#include "../settings/settings_def_smb_client_auth.h"
#include "../settings/settings_def_midi_volume.h"
#include "../settings/settings_def_netplay_ports.h"
#include "../settings/settings_def_rgui_thumbnail_downscale.h"
#include "../settings/settings_def_thumbnail_upscale.h"
#include "../settings/settings_def_xmb_color_theme.h"
#include "../settings/settings_def_xmb_shader_pipeline.h"
#include "../settings/settings_def_xmb_animations.h"
#include "../settings/settings_def_rgui_color_theme.h"
#include "../settings/settings_def_rgui_aspect.h"
#include "../settings/settings_def_record_threads.h"
#include "../settings/settings_def_stream_quality.h"
#include "../settings/settings_def_record_quality.h"
#include "../settings/settings_def_input_touch_scale.h"
#include "../settings/settings_def_input_mouse_scale.h"
#include "../settings/settings_def_black_frame_insertion.h"
#include "../settings/settings_def_shader_delay.h"
#include "../settings/settings_def_screen_brightness.h"
#include "../settings/settings_def_video_rotation.h"
#include "../settings/settings_def_video_monitor_index.h"
#include "../settings/settings_def_rewind_step.h"
#include "../settings/settings_def_frame_throttle_slowmotion.h"
#include "../settings/settings_def_frame_throttle_fastforward.h"
#include "../settings/settings_def_dingux_rs90.h"
#include "../settings/settings_def_dingux_refresh_rate.h"
#include "../settings/settings_def_dingux_ipu.h"
#include "../settings/settings_def_ai_service.h"
#include "../settings/settings_def_video_window_custom_size.h"
#include "../settings/settings_def_video_window_save_position.h"
#include "../settings/settings_def_user_language_action.h"
#include "../settings/settings_def_3ds_bottom_lcd.h"
#include "../settings/settings_def_overlay_auto_scale.h"
#include "../settings/settings_def_notification_widgets.h"
#include "../settings/settings_def_smb_client.h"
#include "../settings/settings_def_netplay_stateless.h"
#include "../settings/settings_def_audio_format.h"
#include "../settings/settings_def_menu_main_actions_12.h"
#include "../settings/settings_def_menu_main_actions_11.h"
#include "../settings/settings_def_menu_main_actions_10.h"
#include "../settings/settings_def_audio_asio_action.h"
#include "../settings/settings_def_netplay_nat.h"
#include "../settings/settings_def_menu_ex_pilot.h"
#include "../settings/settings_def_accounts_streaming.h"
#include "../settings/settings_def_accounts_cheevos.h"
#include "../settings/settings_def_power_action.h"
#include "../settings/settings_def_netplay_action.h"
#include "../settings/settings_def_video_actions_5.h"
#include "../settings/settings_def_video_actions_3.h"
#include "../settings/settings_def_input_actions.h"
#include "../settings/settings_def_video_actions_2.h"
#include "../settings/settings_def_video_actions_1.h"
#include "../settings/settings_def_saving_actions.h"
#include "../settings/settings_def_menu_main_actions_9.h"
#include "../settings/settings_def_menu_main_actions_8.h"
#include "../settings/settings_def_menu_main_actions_7.h"
#include "../settings/settings_def_menu_main_actions_6.h"
#include "../settings/settings_def_menu_main_actions_5.h"
#include "../settings/settings_def_menu_main_actions_4.h"
#include "../settings/settings_def_menu_main_actions_2.h"
#include "../settings/settings_def_menu_main_actions_3.h"
#include "../settings/settings_def_menu_main_actions_1.h"
#include "../settings/settings_def_user_identity.h"
#include "../settings/settings_def_netplay_passwords.h"
#include "../settings/settings_def_netplay_server.h"
#include "../settings/settings_def_recording_paths.h"
#include "../settings/settings_def_streaming_paths.h"
#include "../settings/settings_def_settings_password.h"
#include "../settings/settings_def_kiosk_password.h"
#include "../settings/settings_def_ozone_font_path.h"
#include "../settings/settings_def_xmb_font_path.h"
#include "../settings/settings_def_rgui_theme_path.h"
#include "../settings/settings_def_overlay_preset_path.h"
#include "../settings/settings_def_notification_font_path.h"
#include "../settings/settings_def_menu_wallpaper_path.h"
#include "../settings/settings_def_audio_dsp_path.h"
#include "../settings/settings_def_video_filter_path.h"
#include "../settings/settings_def_dir_user.h"
#include "../settings/settings_def_dir_core.h"
#include "../settings/settings_def_dir_cache_log.h"
#include "../settings/settings_def_cloud_sync_s3.h"
#include "../settings/settings_def_cloud_sync_webdav.h"
#include "../settings/settings_def_microphone_block.h"
#include "../settings/settings_def_updater_backup.h"
#include "../settings/settings_def_updater_experimental.h"
#include "../settings/settings_def_updater_extract.h"
#include "../settings/settings_def_gamemode.h"
#include "../settings/settings_def_sustained_performance.h"
#include "../settings/settings_def_quick_menu_shaders_view.h"
#include "../settings/settings_def_menu_online_updater_view.h"
#include "../settings/settings_def_settings_show_smb.h"
#include "../settings/settings_def_settings_show_steam.h"
#include "../settings/settings_def_network_ondemand_thumbnails.h"
#include "../settings/settings_def_network_stdin_cmd.h"
#include "../settings/settings_def_privacy_discord.h"
#include "../settings/settings_def_privacy_location.h"
#include "../settings/settings_def_privacy_camera.h"
#include "../settings/settings_def_menu_core_updater_view.h"
#include "../settings/settings_def_menu_restart_view.h"
#include "../settings/settings_def_menu_thumbnail_background.h"
#include "../settings/settings_def_menu_start_screen.h"
#include "../settings/settings_def_menu_content_settings_view.h"
#include "../settings/settings_def_menu_threaded_data.h"
#include "../settings/settings_def_menu_wraparound.h"
#include "../settings/settings_def_menu_horizontal_animation.h"
#include "../settings/settings_def_menu_rgui_transparency.h"
#include "../settings/settings_def_menu_wallpaper.h"
#include "../settings/settings_def_runahead_warnings.h"
#include "../settings/settings_def_input_auto_mouse_grab.h"
#include "../settings/settings_def_input_nowinkey.h"
#include "../settings/settings_def_overlay_enable.h"
#include "../settings/settings_def_microphone.h"
#include "../settings/settings_def_audio_sync.h"
#include "../settings/settings_def_audio_enable.h"
#include "../settings/settings_def_ui_menubar.h"
#include "../settings/settings_def_video_window_decorations.h"
#include "../settings/settings_def_video_srgb.h"
#include "../settings/settings_def_video_wiiu_drc.h"
#include "../settings/settings_def_shader_watch.h"
#include "../settings/settings_def_video_dingux_ipu.h"
#include "../settings/settings_def_video_notch.h"
#include "../settings/settings_def_menu_steam.h"
#include "../settings/settings_def_input_android_workaround.h"
#include "../settings/settings_def_video_frame_time_sample.h"
#include "../settings/settings_def_video_adaptive_vsync.h"
#include "../settings/settings_def_video_smooth.h"
#include "../settings/settings_def_frame_time_counter.h"
#include "../settings/settings_def_menu_filebrowser.h"
#include "../settings/settings_def_video_filter_rotation.h"
#include "../settings/settings_def_cheevos.h"
#include "../settings/settings_def_video_suspend_screensaver.h"
#include "../settings/settings_def_cheevos_visibility.h"
#include "../settings/settings_def_ui_focus.h"
#include "../settings/settings_def_multimedia.h"
#include "../settings/settings_def_menu_rgui_thumbnails.h"
#include "../settings/settings_def_menu_power_views.h"
#include "../settings/settings_def_recording_video.h"
#include "../settings/settings_def_input_backtouch.h"
#include "../settings/settings_def_menu_rgui_layout.h"
#include "../settings/settings_def_cheats_apply.h"
#include "../settings/settings_def_shader_preset.h"
#include "../settings/settings_def_input_vmouse.h"
#include "../settings/settings_def_menu_desktop.h"
#include "../settings/settings_def_video_gamecube.h"
#include "../settings/settings_def_menu_sounds.h"
#include "../settings/settings_def_menu_main_views.h"
#include "../settings/settings_def_menu_quick_views.h"
#include "../settings/settings_def_menu_settings_views.h"
#include "../settings/settings_def_video_bias.h"
#include "../settings/settings_def_video_window.h"
#undef S_BOOL
#undef S_BOOL_NS
#undef S_BOOL_H
#undef S_BOOL_NS_H
#undef S_UINT
#undef S_UINT_NS
#undef S_UINT_H
#undef S_UINT_NS_H
#undef S_INT
#undef S_INT_NS
#undef S_INT_H
#undef S_INT_NS_H
#undef S_FLOAT
#undef S_FLOAT_NS
#undef S_FLOAT_H
#undef S_FLOAT_NS_H
#undef S_STRING
#undef S_STRING_NS
#undef S_STRING_H
#undef S_STRING_NS_H
#undef S_DIR
#undef S_DIR_NS
#undef S_DIR_H
#undef S_DIR_NS_H
#undef S_STRING_P
#undef S_STRING_P_NS
#undef S_STRING_P_H
#undef S_STRING_P_NS_H
#undef S_PATH
#undef S_PATH_NS
#undef S_PATH_H
#undef S_PATH_NS_H
#undef S_PATH_DS
#undef S_PATH_DS_NS
#undef S_PATH_DS_H
#undef S_PATH_DS_NS_H
#undef S_ACTION
#undef S_ACTION_NS
#undef S_ACTION_H
#undef S_ACTION_NS_H
#undef S_BOOL_EX
#undef S_BOOL_EX_NS
#undef S_BOOL_EX_H
#undef S_BOOL_EX_NS_H
#undef S_UINT_EX
#undef S_UINT_EX_NS
#undef S_UINT_EX_H
#undef S_UINT_EX_NS_H
#undef S_INT_EX
#undef S_INT_EX_NS
#undef S_INT_EX_H
#undef S_INT_EX_NS_H
#undef S_FLOAT_EX
#undef S_FLOAT_EX_NS
#undef S_FLOAT_EX_H
#undef S_FLOAT_EX_NS_H
#undef S_ACTION_EX
#undef S_ACTION_EX_NS
#undef S_ACTION_EX_H
#undef S_ACTION_EX_NS_H
#undef S_BOOL_LV
#undef S_BOOL_LV_NS
#undef S_BOOL_LV_H
#undef S_BOOL_LV_NS_H
#undef S_FLOAT_LV
#undef S_FLOAT_LV_NS
#undef S_FLOAT_LV_H
#undef S_FLOAT_LV_NS_H
#undef S_STRING_LV
#undef S_STRING_LV_NS
#undef S_STRING_LV_H
#undef S_STRING_LV_NS_H
#undef S_ACTION_LV
#undef S_ACTION_LV_NS
#undef S_ACTION_LV_H
#undef S_ACTION_LV_NS_H
#undef SETTINGS_DEF_STRINGS_PASS
};
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_us_lbl_ids[] = {
#include "msg_hash_lbl.h"
#define SETTINGS_DEF_STRINGS_PASS
#define S_BOOL(f, T, n, d, sd, df, c, us, sub) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_BOOL_NS(f, T, n, d, sd, df, c, us) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_UINT(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us, sub) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_UINT_NS(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_INT(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us, sub) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_INT_NS(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_FLOAT(f, T, n, d, rnd, sd, df, c, mn, mx, st, ok, rp, us, sub) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_FLOAT_NS(f, T, n, d, rnd, sd, df, c, mn, mx, st, ok, rp, us) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_STRING(f, T, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us, sub) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_STRING_NS(f, T, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_DIR(f, T, n, d, el, sd, c, sta, us, sub) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_DIR_NS(f, T, n, d, el, sd, c, sta, us) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_STRING_P(f, T, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us, sub) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_STRING_P_NS(f, T, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_PATH(f, T, n, d, sd, c, vals, rp, ui, us, sub) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_PATH_NS(f, T, n, d, sd, c, vals, rp, ui, us) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_PATH_DS(f, T, n, df2, sd, c, vals, rp, ui, us, sub) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_PATH_DS_NS(f, T, n, df2, sd, c, vals, rp, ui, us) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_ACTION(T, n, us, sub) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_ACTION_NS(T, n, us) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_BOOL_LV(f, T, TV, n, d, sd, df, c, us, sub) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_BOOL_LV_NS(f, T, TV, n, d, sd, df, c, us) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_FLOAT_LV(f, T, TV, n, d, rnd, sd, df, c, us, sub) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_FLOAT_LV_NS(f, T, TV, n, d, rnd, sd, df, c, us) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_STRING_LV(f, T, TV, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us, sub) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_STRING_LV_NS(f, T, TV, n, d, sd, c, ok, rp, sta, sel, lf, rt, ui, us) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_ACTION_LV(T, TV, n, sd, ok, rp, c, us, sub) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_ACTION_LV_NS(T, TV, n, sd, ok, rp, c, us) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_BOOL_EX(f, T, n, d, sd, df, c, ok, rp, sta, sel, lf, rt, ui, us, sub) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_BOOL_EX_NS(f, T, n, d, sd, df, c, ok, rp, sta, sel, lf, rt, ui, us) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_UINT_EX(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui, us, sub) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_UINT_EX_NS(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui, us) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_INT_EX(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui, us, sub) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_INT_EX_NS(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui, us) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_FLOAT_EX(f, T, n, d, rnd, sd, df, c, mn, mx, st, ok, rp, sta, sel, lf, rt, ui, us, sub) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_FLOAT_EX_NS(f, T, n, d, rnd, sd, df, c, mn, mx, st, ok, rp, sta, sel, lf, rt, ui, us) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_ACTION_EX(T, n, sd, ok, rp, c, us, sub) (uint32_t)MENU_ENUM_LABEL_##T,
#define S_ACTION_EX_NS(T, n, sd, ok, rp, c, us) (uint32_t)MENU_ENUM_LABEL_##T,
#include "../settings/settings_def_video_fullscreen.h"
#include "../settings/settings_def_video_sync.h"
#include "../settings/settings_def_menu_show_restart.h"
#include "../settings/settings_def_quit_restart.h"
#include "../settings/settings_def_menu_throttle.h"
#include "../settings/settings_def_video_ctx_scaling.h"
#include "../settings/settings_def_input_sensors_extra.h"
#include "../settings/settings_def_netplay_advanced.h"
#include "../settings/settings_def_menu_main_state.h"
#include "../settings/settings_def_playlist_management.h"
#include "../settings/settings_def_menu_privacy.h"
#include "../settings/settings_def_menu_landscape.h"
#include "../settings/settings_def_cloud_sync_general.h"
#include "../settings/settings_def_overlay_appearance.h"
#include "../settings/settings_def_notification_views.h"
#include "../settings/settings_def_ozone_typography.h"
#include "../settings/settings_def_saving.h"
#include "../settings/settings_def_notification_positions.h"
#include "../settings/settings_def_menu_main_lists_4.h"
#include "../settings/settings_def_menu_main_lists_3.h"
#include "../settings/settings_def_ozone_extras.h"
#include "../settings/settings_def_menu_main_lists_2.h"
#include "../settings/settings_def_overlay_mouse.h"
#include "../settings/settings_def_overlay_lightgun.h"
#include "../settings/settings_def_input_turbo_fire.h"
#include "../settings/settings_def_ui_appearance.h"
#include "../settings/settings_def_menu_entry_display.h"
#include "../settings/settings_def_crt_switchres.h"
#include "../settings/settings_def_audio_state.h"
#include "../settings/settings_def_analog_deadzone.h"
#include "../settings/settings_def_desktop_menu.h"
#include "../settings/settings_def_audio_device.h"
#include "../settings/settings_def_ai_service_options.h"
#include "../settings/settings_def_video_output_misc.h"
#include "../settings/settings_def_netplay_visibility.h"
#include "../settings/settings_def_microphone_general.h"
#include "../settings/settings_def_menu_startup.h"
#include "../settings/settings_def_accessibility.h"
#include "../settings/settings_def_video_refresh_rate.h"
#include "../settings/settings_def_menu_header_footer.h"
#include "../settings/settings_def_logging.h"
#include "../settings/settings_def_frame_throttle_general.h"
#include "../settings/settings_def_refresh_autoswitch.h"
#include "../settings/settings_def_audio_skew.h"
#include "../settings/settings_def_wifi.h"
#include "../settings/settings_def_steam_presence.h"
#include "../settings/settings_def_playlist_sorting.h"
#include "../settings/settings_def_playlist_flags.h"
#include "../settings/settings_def_notification_enable.h"
#include "../settings/settings_def_rgui_particle_effect.h"
#include "../settings/settings_def_menu_savestate_resume.h"
#include "../settings/settings_def_menu_framebuffer_opacity.h"
#include "../settings/settings_def_input_haptics.h"
#include "../settings/settings_def_input_general.h"
#include "../settings/settings_def_ozone_appearance.h"
#include "../settings/settings_def_rgui_appearance.h"
#include "../settings/settings_def_playlist_display.h"
#include "../settings/settings_def_cheevos_general.h"
#include "../settings/settings_def_menu_appearance.h"
#include "../settings/settings_def_menu_visibility.h"
#include "../settings/settings_def_netplay_sync.h"
#include "../settings/settings_def_mic_wasapi.h"
#include "../settings/settings_def_ozone_sidebar.h"
#include "../settings/settings_def_input_bind_timeouts.h"
#include "../settings/settings_def_video_hdr_toggles.h"
#include "../settings/settings_def_rewind.h"
#include "../settings/settings_def_playlist_history.h"
#include "../settings/settings_def_menu_scroll.h"
#include "../settings/settings_def_menu_thumbnails.h"
#include "../settings/settings_def_input_turbo.h"
#include "../settings/settings_def_frame_delay.h"
#include "../settings/settings_def_audio_wasapi.h"
#include "../settings/settings_def_audio_resampler_quality.h"
#include "../settings/settings_def_wallpaper_opacity.h"
#include "../settings/settings_def_overlay_opacity.h"
#include "../settings/settings_def_widget_scale_windowed.h"
#include "../settings/settings_def_video_hdr.h"
#include "../settings/settings_def_video_window_offset.h"
#include "../settings/settings_def_vulkan_gpu_index.h"
#include "../settings/settings_def_smb_client_auth.h"
#include "../settings/settings_def_midi_volume.h"
#include "../settings/settings_def_netplay_ports.h"
#include "../settings/settings_def_rgui_thumbnail_downscale.h"
#include "../settings/settings_def_thumbnail_upscale.h"
#include "../settings/settings_def_xmb_color_theme.h"
#include "../settings/settings_def_xmb_shader_pipeline.h"
#include "../settings/settings_def_xmb_animations.h"
#include "../settings/settings_def_rgui_color_theme.h"
#include "../settings/settings_def_rgui_aspect.h"
#include "../settings/settings_def_record_threads.h"
#include "../settings/settings_def_stream_quality.h"
#include "../settings/settings_def_record_quality.h"
#include "../settings/settings_def_input_touch_scale.h"
#include "../settings/settings_def_input_mouse_scale.h"
#include "../settings/settings_def_black_frame_insertion.h"
#include "../settings/settings_def_shader_delay.h"
#include "../settings/settings_def_screen_brightness.h"
#include "../settings/settings_def_video_rotation.h"
#include "../settings/settings_def_video_monitor_index.h"
#include "../settings/settings_def_rewind_step.h"
#include "../settings/settings_def_frame_throttle_slowmotion.h"
#include "../settings/settings_def_frame_throttle_fastforward.h"
#include "../settings/settings_def_dingux_rs90.h"
#include "../settings/settings_def_dingux_refresh_rate.h"
#include "../settings/settings_def_dingux_ipu.h"
#include "../settings/settings_def_ai_service.h"
#include "../settings/settings_def_video_window_custom_size.h"
#include "../settings/settings_def_video_window_save_position.h"
#include "../settings/settings_def_user_language_action.h"
#include "../settings/settings_def_3ds_bottom_lcd.h"
#include "../settings/settings_def_overlay_auto_scale.h"
#include "../settings/settings_def_notification_widgets.h"
#include "../settings/settings_def_smb_client.h"
#include "../settings/settings_def_netplay_stateless.h"
#include "../settings/settings_def_audio_format.h"
#include "../settings/settings_def_menu_main_actions_12.h"
#include "../settings/settings_def_menu_main_actions_11.h"
#include "../settings/settings_def_menu_main_actions_10.h"
#include "../settings/settings_def_audio_asio_action.h"
#include "../settings/settings_def_netplay_nat.h"
#include "../settings/settings_def_menu_ex_pilot.h"
#include "../settings/settings_def_accounts_streaming.h"
#include "../settings/settings_def_accounts_cheevos.h"
#include "../settings/settings_def_power_action.h"
#include "../settings/settings_def_netplay_action.h"
#include "../settings/settings_def_video_actions_5.h"
#include "../settings/settings_def_video_actions_3.h"
#include "../settings/settings_def_input_actions.h"
#include "../settings/settings_def_video_actions_2.h"
#include "../settings/settings_def_video_actions_1.h"
#include "../settings/settings_def_saving_actions.h"
#include "../settings/settings_def_menu_main_actions_9.h"
#include "../settings/settings_def_menu_main_actions_8.h"
#include "../settings/settings_def_menu_main_actions_7.h"
#include "../settings/settings_def_menu_main_actions_6.h"
#include "../settings/settings_def_menu_main_actions_5.h"
#include "../settings/settings_def_menu_main_actions_4.h"
#include "../settings/settings_def_menu_main_actions_2.h"
#include "../settings/settings_def_menu_main_actions_3.h"
#include "../settings/settings_def_menu_main_actions_1.h"
#include "../settings/settings_def_user_identity.h"
#include "../settings/settings_def_netplay_passwords.h"
#include "../settings/settings_def_netplay_server.h"
#include "../settings/settings_def_recording_paths.h"
#include "../settings/settings_def_streaming_paths.h"
#include "../settings/settings_def_settings_password.h"
#include "../settings/settings_def_kiosk_password.h"
#include "../settings/settings_def_ozone_font_path.h"
#include "../settings/settings_def_xmb_font_path.h"
#include "../settings/settings_def_rgui_theme_path.h"
#include "../settings/settings_def_overlay_preset_path.h"
#include "../settings/settings_def_notification_font_path.h"
#include "../settings/settings_def_menu_wallpaper_path.h"
#include "../settings/settings_def_audio_dsp_path.h"
#include "../settings/settings_def_video_filter_path.h"
#include "../settings/settings_def_dir_user.h"
#include "../settings/settings_def_dir_core.h"
#include "../settings/settings_def_dir_cache_log.h"
#include "../settings/settings_def_cloud_sync_s3.h"
#include "../settings/settings_def_cloud_sync_webdav.h"
#include "../settings/settings_def_microphone_block.h"
#include "../settings/settings_def_updater_backup.h"
#include "../settings/settings_def_updater_experimental.h"
#include "../settings/settings_def_updater_extract.h"
#include "../settings/settings_def_gamemode.h"
#include "../settings/settings_def_sustained_performance.h"
#include "../settings/settings_def_quick_menu_shaders_view.h"
#include "../settings/settings_def_menu_online_updater_view.h"
#include "../settings/settings_def_settings_show_smb.h"
#include "../settings/settings_def_settings_show_steam.h"
#include "../settings/settings_def_network_ondemand_thumbnails.h"
#include "../settings/settings_def_network_stdin_cmd.h"
#include "../settings/settings_def_privacy_discord.h"
#include "../settings/settings_def_privacy_location.h"
#include "../settings/settings_def_privacy_camera.h"
#include "../settings/settings_def_menu_core_updater_view.h"
#include "../settings/settings_def_menu_restart_view.h"
#include "../settings/settings_def_menu_thumbnail_background.h"
#include "../settings/settings_def_menu_start_screen.h"
#include "../settings/settings_def_menu_content_settings_view.h"
#include "../settings/settings_def_menu_threaded_data.h"
#include "../settings/settings_def_menu_wraparound.h"
#include "../settings/settings_def_menu_horizontal_animation.h"
#include "../settings/settings_def_menu_rgui_transparency.h"
#include "../settings/settings_def_menu_wallpaper.h"
#include "../settings/settings_def_runahead_warnings.h"
#include "../settings/settings_def_input_auto_mouse_grab.h"
#include "../settings/settings_def_input_nowinkey.h"
#include "../settings/settings_def_overlay_enable.h"
#include "../settings/settings_def_microphone.h"
#include "../settings/settings_def_audio_sync.h"
#include "../settings/settings_def_audio_enable.h"
#include "../settings/settings_def_ui_menubar.h"
#include "../settings/settings_def_video_window_decorations.h"
#include "../settings/settings_def_video_srgb.h"
#include "../settings/settings_def_video_wiiu_drc.h"
#include "../settings/settings_def_shader_watch.h"
#include "../settings/settings_def_video_dingux_ipu.h"
#include "../settings/settings_def_video_notch.h"
#include "../settings/settings_def_menu_steam.h"
#include "../settings/settings_def_input_android_workaround.h"
#include "../settings/settings_def_video_frame_time_sample.h"
#include "../settings/settings_def_video_adaptive_vsync.h"
#include "../settings/settings_def_video_smooth.h"
#include "../settings/settings_def_frame_time_counter.h"
#include "../settings/settings_def_menu_filebrowser.h"
#include "../settings/settings_def_video_filter_rotation.h"
#include "../settings/settings_def_cheevos.h"
#include "../settings/settings_def_video_suspend_screensaver.h"
#include "../settings/settings_def_cheevos_visibility.h"
#include "../settings/settings_def_ui_focus.h"
#include "../settings/settings_def_multimedia.h"
#include "../settings/settings_def_menu_rgui_thumbnails.h"
#include "../settings/settings_def_menu_power_views.h"
#include "../settings/settings_def_recording_video.h"
#include "../settings/settings_def_input_backtouch.h"
#include "../settings/settings_def_menu_rgui_layout.h"
#include "../settings/settings_def_cheats_apply.h"
#include "../settings/settings_def_shader_preset.h"
#include "../settings/settings_def_input_vmouse.h"
#include "../settings/settings_def_menu_desktop.h"
#include "../settings/settings_def_video_gamecube.h"
#include "../settings/settings_def_menu_sounds.h"
#include "../settings/settings_def_menu_main_views.h"
#include "../settings/settings_def_menu_quick_views.h"
#include "../settings/settings_def_menu_settings_views.h"
#include "../settings/settings_def_video_bias.h"
#include "../settings/settings_def_video_window.h"
#undef S_BOOL
#undef S_BOOL_NS
#undef S_BOOL_H
#undef S_BOOL_NS_H
#undef S_UINT
#undef S_UINT_NS
#undef S_UINT_H
#undef S_UINT_NS_H
#undef S_INT
#undef S_INT_NS
#undef S_INT_H
#undef S_INT_NS_H
#undef S_FLOAT
#undef S_FLOAT_NS
#undef S_FLOAT_H
#undef S_FLOAT_NS_H
#undef S_STRING
#undef S_STRING_NS
#undef S_STRING_H
#undef S_STRING_NS_H
#undef S_DIR
#undef S_DIR_NS
#undef S_DIR_H
#undef S_DIR_NS_H
#undef S_STRING_P
#undef S_STRING_P_NS
#undef S_STRING_P_H
#undef S_STRING_P_NS_H
#undef S_PATH
#undef S_PATH_NS
#undef S_PATH_H
#undef S_PATH_NS_H
#undef S_PATH_DS
#undef S_PATH_DS_NS
#undef S_PATH_DS_H
#undef S_PATH_DS_NS_H
#undef S_ACTION
#undef S_ACTION_NS
#undef S_ACTION_H
#undef S_ACTION_NS_H
#undef S_BOOL_EX
#undef S_BOOL_EX_NS
#undef S_BOOL_EX_H
#undef S_BOOL_EX_NS_H
#undef S_UINT_EX
#undef S_UINT_EX_NS
#undef S_UINT_EX_H
#undef S_UINT_EX_NS_H
#undef S_INT_EX
#undef S_INT_EX_NS
#undef S_INT_EX_H
#undef S_INT_EX_NS_H
#undef S_FLOAT_EX
#undef S_FLOAT_EX_NS
#undef S_FLOAT_EX_H
#undef S_FLOAT_EX_NS_H
#undef S_ACTION_EX
#undef S_ACTION_EX_NS
#undef S_ACTION_EX_H
#undef S_ACTION_EX_NS_H
#undef S_BOOL_LV
#undef S_BOOL_LV_NS
#undef S_BOOL_LV_H
#undef S_BOOL_LV_NS_H
#undef S_FLOAT_LV
#undef S_FLOAT_LV_NS
#undef S_FLOAT_LV_H
#undef S_FLOAT_LV_NS_H
#undef S_STRING_LV
#undef S_STRING_LV_NS
#undef S_STRING_LV_H
#undef S_STRING_LV_NS_H
#undef S_ACTION_LV
#undef S_ACTION_LV_NS
#undef S_ACTION_LV_H
#undef S_ACTION_LV_NS_H
#undef SETTINGS_DEF_STRINGS_PASS
};
#undef MSG_HASH
#define MSG_HASH(Id, str) case Id: return str;

static const msg_hash_strtab_t msg_hash_us_lbl_strtab =
{ msg_hash_us_lbl_ids, msg_hash_us_lbl_strs,
  (uint32_t)(sizeof(msg_hash_us_lbl_ids) / sizeof(msg_hash_us_lbl_ids[0])),
  NULL };

static msg_hash_strtab_index_t msg_hash_us_lbl_index;
#endif

static const char *menu_hash_to_str_us_label_enum(enum msg_hash_enums msg)
{
   if (   msg <= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_END
       && msg >= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN)
   {
      static char hotkey_lbl[128] = {0};
      unsigned idx = msg - MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN;
      snprintf(hotkey_lbl, sizeof(hotkey_lbl), "input_hotkey_binds_%d", idx);
      return hotkey_lbl;
   }

#if defined(MSG_HASH_HAVE_STRTAB)
   {
      const char *ret = msg_hash_strtab_lookup(&msg_hash_us_lbl_strtab,
            &msg_hash_us_lbl_index, (uint32_t)msg);
      if (ret)
         return ret;
   }
#else
   switch (msg)
   {
#include "msg_hash_lbl.h"
      default:
#if 0
         RARCH_LOG("Unimplemented: [%d]\n", msg);
#endif
         break;
   }
#endif

   return "null";
}
#endif

#if defined(MSG_HASH_HAVE_STRTAB)
/* The hand-maintained MSG_HASH rows are expanded three times: once
 * into exactly-sized members of a packed blob struct (named by the
 * id token, so mutually exclusive guarded duplicates stay legal),
 * once into its initializer, and once into the id array.  This
 * keeps the file freely hand-editable and h2json-parseable while
 * eliminating the string pointer array and its relocations, the
 * same layout the generated translation headers use. */
#undef MSG_HASH
#define MSG_HASH(Id, str) char m_##Id[sizeof(str)];
static const struct msg_hash_us_blob_s
{
#include "msg_hash_us.h"
}
#undef MSG_HASH
#define MSG_HASH(Id, str) str,
msg_hash_us_blob =
{
#include "msg_hash_us.h"
};
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_us_ids[] = {
#include "msg_hash_us.h"
};
#undef MSG_HASH
#define MSG_HASH(Id, str) case Id: return str;

static const msg_hash_strtab_t msg_hash_us_strtab =
{ msg_hash_us_ids, NULL,
  (uint32_t)(sizeof(msg_hash_us_ids) / sizeof(msg_hash_us_ids[0])),
  (const char*)&msg_hash_us_blob };

static msg_hash_strtab_index_t msg_hash_us_index;

void msg_hash_us_index_init(void)
{
   if (msg_hash_us_index.tab != &msg_hash_us_strtab)
      msg_hash_strtab_index_build(&msg_hash_us_index,
            &msg_hash_us_strtab);
#ifdef HAVE_MENU
   if (msg_hash_us_lbl_index.tab != &msg_hash_us_lbl_strtab)
      msg_hash_strtab_index_build(&msg_hash_us_lbl_index,
            &msg_hash_us_lbl_strtab);
#endif
}
#endif

const char *msg_hash_to_str_us(enum msg_hash_enums msg)
{
#ifdef HAVE_MENU
    const char *ret = menu_hash_to_str_us_label_enum(msg);

    if (ret && !string_is_equal(ret, "null"))
       return ret;
#endif

#if defined(MSG_HASH_HAVE_STRTAB)
    {
       const char *s = msg_hash_strtab_lookup(&msg_hash_us_strtab,
             &msg_hash_us_index, (uint32_t)msg);
       if (s)
          return s;
    }
#else
    switch (msg)
    {
#include "msg_hash_us.h"
        default:
#if 0
            RARCH_LOG("Unimplemented: [%d]\n", msg);
            {
               RARCH_LOG("[%d] : %s\n", msg - 1, msg_hash_to_str(((enum msg_hash_enums)(msg - 1))));
            }
#endif
            break;
    }
#endif

    return "null";
}
