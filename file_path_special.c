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

/* Assume W-functions do not work below Win2K and Xbox platforms */
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500 || defined(_XBOX)

#ifndef LEGACY_WIN32
#define LEGACY_WIN32
#endif

#endif

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#ifdef OSX
#include <CoreFoundation/CoreFoundation.h>
#endif

#ifdef __QNX__
#include <libgen.h>
#endif

#ifdef __HAIKU__
#include <kernel/image.h>
#endif

#if defined(DINGUX)
#include "dingux/dingux_utils.h"
#endif

#include <stdlib.h>
#include <boolean.h>
#include <string.h>
#include <time.h>

#include <file/file_path.h>
#include <string/stdstring.h>

#include <compat/strl.h>
#include <compat/posix_string.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>
#include <encodings/utf.h>

#ifdef HAVE_MENU
#include "menu/menu_driver.h"
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "configuration.h"
#include "file_path_special.h"

#include "msg_hash.h"
#include "paths.h"
#include "verbosity.h"

bool fill_pathname_application_data(char *s, size_t len)
{
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#ifdef LEGACY_WIN32
   const char *appdata = getenv("APPDATA");

   if (appdata)
   {
      strlcpy(s, appdata, len);
      return true;
   }
#else
   const wchar_t *appdataW = _wgetenv(L"APPDATA");

   if (appdataW)
   {
      char *appdata = utf16_to_utf8_string_alloc(appdataW);

      if (appdata)
      {
         strlcpy(s, appdata, len);
         free(appdata);
         return true;
      }
   }
#endif

#elif defined(OSX)
   CFBundleRef bundle = CFBundleGetMainBundle();
   if (!bundle)
      return false;

   /* get the directory containing the app */
   CFStringRef parent_path;
   CFURLRef bundle_url, parent_url;
   bundle_url  = CFBundleCopyBundleURL(bundle);
   parent_url  = CFURLCreateCopyDeletingLastPathComponent(NULL, bundle_url);
   parent_path = CFURLCopyFileSystemPath(parent_url, kCFURLPOSIXPathStyle);
   CFStringGetCString(parent_path, s, len, kCFStringEncodingUTF8);
   CFRelease(parent_path);
   CFRelease(parent_url);
   CFRelease(bundle_url);

#if HAVE_STEAM
   return true;
#else
   /* if portable.txt exists next to the app then we use that directory */
   char portable_buf[PATH_MAX_LENGTH] = {0};
   fill_pathname_join(portable_buf, s, "portable.txt", sizeof(portable_buf));
   if (path_is_valid(portable_buf))
      return true;

   /* if the app itself says it's portable we obey that as well */
   CFStringRef key = CFStringCreateWithCString(NULL, "RAPortableInstall", kCFStringEncodingUTF8);
   if (key)
   {
      CFBooleanRef val = CFBundleGetValueForInfoDictionaryKey(bundle, key);
      CFRelease(key);
      if (val)
      {
         bool portable = CFBooleanGetValue(val);
         CFRelease(val);
         if (portable)
            return true;
      }
   }

   /* otherwise we use ~/Library/Application Support/RetroArch */
   const char *appdata = getenv("HOME");
   if (appdata)
   {
      fill_pathname_join(s, appdata,
            "Library/Application Support/RetroArch", len);
      return true;
   }
#endif
#elif defined(RARCH_UNIX_CWD_ENV)
   getcwd(s, len);
   return true;
#elif defined(DINGUX)
   dingux_get_base_path(s, len);
   return true;
#elif !defined(RARCH_CONSOLE)
   const char *xdg     = getenv("XDG_CONFIG_HOME");
   const char *appdata = getenv("HOME");

   /* XDG_CONFIG_HOME falls back to $HOME/.config with most Unix systems */
   /* On Haiku, it is set by default to /home/user/config/settings */
   if (xdg)
   {
      fill_pathname_join(s, xdg, "retroarch/", len);
      return true;
   }

   if (appdata)
   {
#ifdef __HAIKU__
      /* in theory never used as Haiku has XDG_CONFIG_HOME set by default */
      fill_pathname_join(s, appdata,
            "config/settings/retroarch/", len);
#else
      fill_pathname_join(s, appdata,
            ".config/retroarch/", len);
#endif
      return true;
   }
#endif

   return false;
}

#ifdef HAVE_XMB
const char* xmb_theme_ident(void);
#endif

size_t fill_pathname_application_special(char *s,
      size_t len, enum application_special_type type)
{
   size_t _len = 0;
   switch (type)
   {
      case APPLICATION_SPECIAL_DIRECTORY_CONFIG:
         {
            settings_t *settings        = config_get_ptr();
            const char *dir_menu_config = settings->paths.directory_menu_config;

            /* Try config directory setting first,
             * fallback to the location of the current configuration file. */
            if (!string_is_empty(dir_menu_config))
               _len = strlcpy(s, dir_menu_config, len);
            else if (!path_is_empty(RARCH_PATH_CONFIG))
               _len = fill_pathname_basedir(s, path_get(RARCH_PATH_CONFIG), len);
         }
         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_ICONS:
#ifdef HAVE_XMB
         {
            char tmp_path[PATH_MAX_LENGTH];
            char tmp_dir[DIR_MAX_LENGTH];
            settings_t *settings     = config_get_ptr();
            const char *dir_assets   = settings->paths.directory_assets;
            fill_pathname_join_special(tmp_dir, dir_assets, "xmb", sizeof(tmp_dir));
            fill_pathname_join_special(tmp_path, tmp_dir, xmb_theme_ident(), sizeof(tmp_path));
            _len = fill_pathname_join_special(s, tmp_path, "png", len);
         }
#endif
         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_BG:
#ifdef HAVE_XMB
         {
            settings_t *settings            = config_get_ptr();
            const char *path_menu_wallpaper = settings->paths.path_menu_wallpaper;

            if (!string_is_empty(path_menu_wallpaper))
               _len = strlcpy(s, path_menu_wallpaper, len);
            else
            {
               char tmp_dir[DIR_MAX_LENGTH];
               char tmp_dir2[DIR_MAX_LENGTH];
               char tmp_path[PATH_MAX_LENGTH];
               const char *dir_assets   = settings->paths.directory_assets;
               fill_pathname_join_special(tmp_dir, dir_assets, "xmb", sizeof(tmp_dir));
               fill_pathname_join_special(tmp_dir2,  tmp_dir, xmb_theme_ident(), sizeof(tmp_dir2));
               fill_pathname_join_special(tmp_path, tmp_dir2, "png", sizeof(tmp_path));
               _len = fill_pathname_join_special(s, tmp_path, FILE_PATH_BACKGROUND_IMAGE, len);
            }
         }
#endif
         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_SOUNDS:
         {
#ifdef HAVE_MENU
            settings_t *settings   = config_get_ptr();
#if defined(HAVE_XMB) || defined(HAVE_MATERIALUI) || defined(HAVE_OZONE)
            const char *menu_ident = settings->arrays.menu_driver;
#endif
            const char *dir_assets = settings->paths.directory_assets;

#ifdef HAVE_XMB
            if (string_is_equal(menu_ident, "xmb"))
            {
               char tmp_dir[DIR_MAX_LENGTH];
               char tmp_path[PATH_MAX_LENGTH];
               fill_pathname_join_special(tmp_dir, dir_assets, menu_ident, sizeof(tmp_dir));
               fill_pathname_join_special(tmp_path, tmp_dir, xmb_theme_ident(), sizeof(tmp_path));
               _len = fill_pathname_join_special(s, tmp_path, "sounds", len);
            }
            else
#endif
#if defined(HAVE_MATERIALUI) || defined(HAVE_OZONE)
            if (     string_is_equal(menu_ident, "glui")
                  || string_is_equal(menu_ident, "ozone"))
            {
               char tmp_dir[DIR_MAX_LENGTH];
               fill_pathname_join_special(tmp_dir, dir_assets, menu_ident, sizeof(tmp_dir));
               _len = fill_pathname_join_special(s, tmp_dir, "sounds", len);
            }
            else
#endif
            {
               _len = fill_pathname_join_special(
                     s, dir_assets, "sounds", len);
            }
#endif
         }

         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_SYSICONS:
         {
#ifdef HAVE_MENU
#if defined(HAVE_XMB) || defined(HAVE_MATERIALUI) || defined(HAVE_OZONE)
            settings_t *settings   = config_get_ptr();
            const char *menu_ident = settings->arrays.menu_driver;
#endif

#ifdef HAVE_XMB
            if (string_is_equal(menu_ident, "xmb"))
            {
               char tmp_dir[DIR_MAX_LENGTH];
               char tmp_path[PATH_MAX_LENGTH];
               const char *dir_assets   = settings->paths.directory_assets;
               fill_pathname_join_special(tmp_dir, dir_assets, menu_ident, sizeof(tmp_dir));
               fill_pathname_join_special(tmp_path, tmp_dir, xmb_theme_ident(), sizeof(tmp_path));
               _len = fill_pathname_join_special(s, tmp_path, "png", len);
            }
            else
#endif
#if defined(HAVE_OZONE) || defined(HAVE_MATERIALUI)
		    if (    string_is_equal(menu_ident, "ozone")
               || string_is_equal(menu_ident, "glui"))
            {
               char tmp_dir[DIR_MAX_LENGTH];
               char tmp_path[PATH_MAX_LENGTH];
               const char *dir_assets   = settings->paths.directory_assets;

#if defined(WIIU) || defined(VITA)
               /* Smaller 46x46 icons look better on low-DPI devices */
               fill_pathname_join_special(tmp_dir, dir_assets, "ozone", sizeof(tmp_dir));
               fill_pathname_join_special(tmp_path, "png", "icons", sizeof(tmp_path));
#else
               /* Otherwise, use large 256x256 icons */
               fill_pathname_join_special(tmp_dir, dir_assets, "xmb", sizeof(tmp_dir));
               fill_pathname_join_special(tmp_path, "monochrome", "png", sizeof(tmp_path));
#endif
               _len = fill_pathname_join_special(s, tmp_dir, tmp_path, len);
            }
            else
#endif
               if (len) s[0] = '\0';
#endif
         }

         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_OZONE_ICONS:
#ifdef HAVE_OZONE
         {
            char tmp_dir[DIR_MAX_LENGTH];
            char tmp_path[PATH_MAX_LENGTH];
            settings_t *settings     = config_get_ptr();
            const char *dir_assets   = settings->paths.directory_assets;
#if defined(WIIU) || defined(VITA)
            /* Smaller 46x46 icons look better on low-DPI devices */
            fill_pathname_join_special(tmp_dir, dir_assets, "ozone", sizeof(tmp_dir));
            fill_pathname_join_special(tmp_path, "png", "icons", sizeof(tmp_path));
#else
            /* Otherwise, use large 256x256 icons */
            fill_pathname_join_special(tmp_dir, dir_assets, "xmb", sizeof(tmp_dir));
            fill_pathname_join_special(tmp_path, "monochrome", "png", sizeof(tmp_path));
#endif
            _len = fill_pathname_join_special(s, tmp_dir, tmp_path, len);
         }
#endif
         break;

      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_RGUI_FONT:
#ifdef HAVE_RGUI
         {
            char tmp_dir[DIR_MAX_LENGTH];
            settings_t *settings     = config_get_ptr();
            const char *dir_assets   = settings->paths.directory_assets;
            fill_pathname_join_special(tmp_dir, dir_assets, "rgui", sizeof(tmp_dir));
            _len = fill_pathname_join_special(s, tmp_dir, "font", len);
         }
#endif
         break;

      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB:
#ifdef HAVE_XMB
         {
            char tmp_dir[DIR_MAX_LENGTH];
            settings_t *settings     = config_get_ptr();
            const char *dir_assets   = settings->paths.directory_assets;
            fill_pathname_join_special(tmp_dir, dir_assets, "xmb", sizeof(tmp_dir));
            _len = fill_pathname_join_special(s, tmp_dir, xmb_theme_ident(), len);
         }
#endif
         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_FONT:
#ifdef HAVE_XMB
         {
            settings_t           *settings = config_get_ptr();
            const char *path_menu_xmb_font = settings->paths.path_menu_xmb_font;

            if (!string_is_empty(path_menu_xmb_font))
               _len = strlcpy(s, path_menu_xmb_font, len);
            else
            {
               char tmp_dir[DIR_MAX_LENGTH];

               switch (*msg_hash_get_uint(MSG_HASH_USER_LANGUAGE))
               {
                  case RETRO_LANGUAGE_ARABIC:
                  case RETRO_LANGUAGE_PERSIAN:
                     fill_pathname_join_special(tmp_dir,
                           settings->paths.directory_assets, "pkg", sizeof(tmp_dir));
                     _len = fill_pathname_join_special(s, tmp_dir, "fallback-font.ttf", len);
                     break;
                  case RETRO_LANGUAGE_CHINESE_SIMPLIFIED:
                  case RETRO_LANGUAGE_CHINESE_TRADITIONAL:
                     fill_pathname_join_special(tmp_dir,
                           settings->paths.directory_assets, "pkg", sizeof(tmp_dir));
                     _len = fill_pathname_join_special(s, tmp_dir, "chinese-fallback-font.ttf", len);
                     break;
                  case RETRO_LANGUAGE_KOREAN:
                     fill_pathname_join_special(tmp_dir,
                           settings->paths.directory_assets, "pkg", sizeof(tmp_dir));
                     _len = fill_pathname_join_special(s, tmp_dir, "korean-fallback-font.ttf", len);
                     break;
                  default:
                     {
                        char tmp_dir2[DIR_MAX_LENGTH];
                        settings_t *settings     = config_get_ptr();
                        const char *dir_assets   = settings->paths.directory_assets;
                        fill_pathname_join_special(tmp_dir2, dir_assets, "xmb", sizeof(tmp_dir2));
                        fill_pathname_join_special(tmp_dir, tmp_dir2, xmb_theme_ident(), sizeof(tmp_dir));
                        _len = fill_pathname_join_special(s, tmp_dir, FILE_PATH_TTF_FONT, len);
                     }
                     break;
               }
            }
         }
#endif
         break;
      case APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_DISCORD_AVATARS:
      {
        char tmp_dir[DIR_MAX_LENGTH];
        settings_t *settings       = config_get_ptr();
        const char *dir_thumbnails = settings->paths.directory_thumbnails;
        fill_pathname_join_special(tmp_dir, dir_thumbnails, "discord", sizeof(tmp_dir));
        _len = fill_pathname_join_special(s, tmp_dir, "avatars", len);
      }
      break;

      case APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES:
      {
        char tmp_dir[DIR_MAX_LENGTH];
        settings_t *settings       = config_get_ptr();
        const char *dir_thumbnails = settings->paths.directory_thumbnails;
        fill_pathname_join_special(tmp_dir, dir_thumbnails, "cheevos", sizeof(tmp_dir));
        _len = fill_pathname_join_special(s, tmp_dir, "badges", len);
      }
      break;

      case APPLICATION_SPECIAL_NONE:
      default:
         break;
   }
   return _len;
}
