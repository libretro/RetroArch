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
    settings_t *settings = config_get_ptr();

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
             if (string_is_empty(s))
                strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
             break;
       }

       return 0;
    }

    switch (msg)
    {
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
        case MENU_ENUM_LABEL_CHECK_FOR_MISSING_FIRMWARE:
            strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_CHECK_FOR_MISSING_FIRMWARE), len);
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
        case MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR:
            strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_INPUT_POLL_TYPE_BEHAVIOR), len);
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
        case MENU_ENUM_LABEL_VIDEO_SHADER_FILTER_PASS:
            strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_VIDEO_SHADER_FILTER_PASS), len);
            break;
        case MENU_ENUM_LABEL_AUTOSAVE_INTERVAL:
            strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUTOSAVE_INTERVAL), len);
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
        default:
            if (string_is_empty(s))
                strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
            return -1;
    }

    return 0;
}
#endif

#ifdef HAVE_MENU
static const char *menu_hash_to_str_us_label_enum(enum msg_hash_enums msg)
{
   if (msg <= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_END &&
         msg >= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN)
   {
      static char hotkey_lbl[128] = {0};
      unsigned idx = msg - MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN;
      snprintf(hotkey_lbl, sizeof(hotkey_lbl), "input_hotkey_binds_%d", idx);
      return hotkey_lbl;
   }

   switch (msg)
   {
#include "msg_hash_lbl.h"
      default:
#if 0
         RARCH_LOG("Unimplemented: [%d]\n", msg);
#endif
         break;
   }

   return "null";
}
#endif

const char *msg_hash_to_str_us(enum msg_hash_enums msg)
{
#ifdef HAVE_MENU
    const char *ret = menu_hash_to_str_us_label_enum(msg);

    if (ret && !string_is_equal(ret, "null"))
       return ret;
#endif

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

    return "null";
}
