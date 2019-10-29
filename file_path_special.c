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

#ifdef __QNX__
#include <libgen.h>
#endif

#ifdef __HAIKU__
#include <kernel/image.h>
#endif

#include <stdlib.h>
#include <boolean.h>
#include <string.h>
#include <time.h>
#include <errno.h>

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
   const char *appdata = getenv("HOME");

   if (appdata)
   {
      fill_pathname_join(s, appdata,
            "Library/Application Support/RetroArch", len);
      return true;
   }
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

#ifdef HAVE_STRIPES
const char* stripes_theme_ident(void);
#endif

void fill_pathname_application_special(char *s,
      size_t len, enum application_special_type type)
{
   switch (type)
   {
      case APPLICATION_SPECIAL_DIRECTORY_AUTOCONFIG:
         {
            settings_t *settings     = config_get_ptr();
            fill_pathname_join(s,
                  settings->paths.directory_autoconfig,
                  settings->arrays.input_joypad_driver,
                  len);
         }
         break;
      case APPLICATION_SPECIAL_DIRECTORY_CONFIG:
         {
            settings_t *settings     = config_get_ptr();

            /* Try config directory setting first,
             * fallback to the location of the current configuration file. */
            if (!string_is_empty(settings->paths.directory_menu_config))
               strlcpy(s, settings->paths.directory_menu_config, len);
            else if (!path_is_empty(RARCH_PATH_CONFIG))
               fill_pathname_basedir(s, path_get(RARCH_PATH_CONFIG), len);
         }
         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_ICONS:
#ifdef HAVE_XMB
         {
            char *s1 = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
            char *s2 = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

            s1[0]    = '\0';
            s2[0]    = '\0';

            fill_pathname_application_special(s1,
                  PATH_MAX_LENGTH * sizeof(char),
                  APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB);
            fill_pathname_join(s2, s1, "png",
                  PATH_MAX_LENGTH * sizeof(char)
                  );
            fill_pathname_slash(s2,
                  PATH_MAX_LENGTH * sizeof(char)
                  );
            strlcpy(s, s2, len);
            free(s1);
            free(s2);
         }
#endif
         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_BG:
#ifdef HAVE_XMB
         {
            settings_t *settings     = config_get_ptr();

            if (!string_is_empty(settings->paths.path_menu_wallpaper))
               strlcpy(s, settings->paths.path_menu_wallpaper, len);
            else
            {
               char *s1 = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

               s1[0] = '\0';

               fill_pathname_application_special(s1,
                     PATH_MAX_LENGTH * sizeof(char),
                     APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_ICONS);
               fill_pathname_join(s, s1, "bg.png", len);
               free(s1);
            }
         }
#endif
         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_SOUNDS:
         {
#ifdef HAVE_MENU
            settings_t *settings = config_get_ptr();
            const char *menu_ident = settings->arrays.menu_driver;
            char *s1 = (char*)calloc(1, PATH_MAX_LENGTH * sizeof(char));

            if (string_is_equal(menu_ident, "xmb"))
            {
               fill_pathname_application_special(s1, PATH_MAX_LENGTH * sizeof(char), APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB);

               if (!string_is_empty(s1))
                  strlcat(s1, "/sounds", PATH_MAX_LENGTH * sizeof(char));
            }
            else if (string_is_equal(menu_ident, "glui"))
            {
               fill_pathname_application_special(s1, PATH_MAX_LENGTH * sizeof(char), APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI);

               if (!string_is_empty(s1))
                  strlcat(s1, "/sounds", PATH_MAX_LENGTH * sizeof(char));
            }
            else if (string_is_equal(menu_ident, "ozone"))
            {
               fill_pathname_application_special(s1, PATH_MAX_LENGTH * sizeof(char), APPLICATION_SPECIAL_DIRECTORY_ASSETS_OZONE);

               if (!string_is_empty(s1))
                  strlcat(s1, "/sounds", PATH_MAX_LENGTH * sizeof(char));
            }

            if (string_is_empty(s1))
            {
               fill_pathname_join(
                     s1,
                     settings->paths.directory_assets,
                     "sounds",
                     PATH_MAX_LENGTH * sizeof(char)
               );
            }

            strlcpy(s, s1, len);
            free(s1);
#endif
         }

         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_OZONE:
#ifdef HAVE_OZONE
         {
            char *s1 = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
            settings_t *settings     = config_get_ptr();

            s1[0] = '\0';

            fill_pathname_join(
                  s1,
                  settings->paths.directory_assets,
                  "ozone",
                  PATH_MAX_LENGTH * sizeof(char)
                  );
            strlcpy(s, s1, len);
            free(s1);
         }
#endif
         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB:
#ifdef HAVE_XMB
         {
            char *s1 = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
            settings_t *settings     = config_get_ptr();

            s1[0] = '\0';

            fill_pathname_join(
                  s1,
                  settings->paths.directory_assets,
                  "xmb",
                  PATH_MAX_LENGTH * sizeof(char)
                  );
            fill_pathname_join(s,
                  s1, xmb_theme_ident(), len);
            free(s1);
         }
#endif
         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI:
#ifdef HAVE_MATERIALUI
         {
            settings_t *settings = config_get_ptr();

            fill_pathname_join(
                  s,
                  settings->paths.directory_assets,
                  "glui",
                  len);
         }
#endif
         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI_ICONS:
#ifdef HAVE_MATERIALUI
         {
            char *s1 = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

            s1[0] = '\0';

            fill_pathname_application_special(s1,
                  PATH_MAX_LENGTH * sizeof(char),
                  APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI);
            fill_pathname_slash(s1,
                  PATH_MAX_LENGTH * sizeof(char)
                  );
            strlcpy(s, s1, len);

            free(s1);
         }
#endif
         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI_FONT:
#ifdef HAVE_MATERIALUI
         {
            char *s1 = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

            s1[0] = '\0';

            fill_pathname_application_special(s1,
                  PATH_MAX_LENGTH * sizeof(char),
                  APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI);
            fill_pathname_join(s, s1, "font.ttf", len);

            free(s1);
         }
#endif
         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_FONT:
#ifdef HAVE_XMB
         {
            settings_t *settings = config_get_ptr();

            if (!string_is_empty(settings->paths.path_menu_xmb_font))
               strlcpy(s, settings->paths.path_menu_xmb_font, len);
            else
            {
               char *s1 = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

               s1[0] = '\0';

               fill_pathname_application_special(s1,
                     PATH_MAX_LENGTH * sizeof(char),
                     APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB);
               fill_pathname_join(s, s1, "font.ttf", len);
               free(s1);
            }
         }
#endif
         break;
      case APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_DISCORD_AVATARS:
      {
        char *s1             = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
        char *s2             = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
        settings_t *settings = config_get_ptr();

        s1[0]                = '\0';
        s2[0]                = '\0';

        fill_pathname_join(s1,
              settings->paths.directory_thumbnails,
              "discord",
              len);
        fill_pathname_join(s2,
              s1, "avatars",
              PATH_MAX_LENGTH * sizeof(char)
              );
        fill_pathname_slash(s2,
              PATH_MAX_LENGTH * sizeof(char)
              );
        strlcpy(s, s2, len);
        free(s1);
        free(s2);
      }
      break;

      case APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES:
      {
        char *s1             = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
        char *s2             = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
        settings_t *settings = config_get_ptr();

        s1[0]                = '\0';
        s2[0]                = '\0';

        fill_pathname_join(s1,
              settings->paths.directory_thumbnails,
              "cheevos",
              len);
        fill_pathname_join(s2,
              s1, "badges",
              PATH_MAX_LENGTH * sizeof(char)
              );
        fill_pathname_slash(s2,
              PATH_MAX_LENGTH * sizeof(char)
              );
        strlcpy(s, s2, len);
        free(s1);
        free(s2);
      }
      break;

      case APPLICATION_SPECIAL_NONE:
      default:
         break;
   }
}

/**
 * fill_short_pathname_representation:
 * @out_rep            : output representation
 * @in_path            : input path
 * @size               : size of output representation
 *
 * Generates a short representation of path. It should only
 * be used for displaying the result; the output representation is not
 * binding in any meaningful way (for a normal path, this is the same as basename)
 * In case of more complex URLs, this should cut everything except for
 * the main image file.
 *
 * E.g.: "/path/to/game.img" -> game.img
 *       "/path/to/myarchive.7z#folder/to/game.img" -> game.img
 */
void fill_short_pathname_representation_wrapper(char* out_rep,
      const char *in_path, size_t size)
{
#ifdef HAVE_COMPRESSION
   char *path_short = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   char *last_slash = NULL;

   path_short[0]    = '\0';

   fill_pathname(path_short, path_basename(in_path), "",
         PATH_MAX_LENGTH * sizeof(char)
         );

   last_slash  = find_last_slash(path_short);

   if (last_slash != NULL)
   {
      /* We handle paths like:
       * /path/to/file.7z#mygame.img
       * short_name: mygame.img:
       *
       * We check whether something is actually
       * after the hash to avoid going over the buffer.
       */
      strlcpy(out_rep, last_slash + 1, size);
      free(path_short);
      return;
   }

   free(path_short);
#endif

   fill_short_pathname_representation(out_rep, in_path, size);
}
