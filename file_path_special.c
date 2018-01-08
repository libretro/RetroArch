/*  RetroArch - A frontend for libretro.
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

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "configuration.h"
#include "file_path_special.h"

#include "paths.h"
#include "verbosity.h"

void fill_pathname_expand_special(char *out_path,
      const char *in_path, size_t size)
{
#if !defined(RARCH_CONSOLE)
   if (*in_path == '~')
   {
      const char *home = getenv("HOME");
      if (home)
      {
         size_t src_size = strlcpy(out_path, home, size);
         retro_assert(src_size < size);

         out_path  += src_size;
         size      -= src_size;
         in_path++;
      }
   }
   else if ((in_path[0] == ':') &&
         (
         (in_path[1] == '/')
#ifdef _WIN32
         || (in_path[1] == '\\')
#endif
         )
            )
   {
      size_t src_size;
      char *application_dir = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

      application_dir[0]    = '\0';

      fill_pathname_application_path(application_dir,
            PATH_MAX_LENGTH * sizeof(char));
      path_basedir_wrapper(application_dir);

      src_size   = strlcpy(out_path, application_dir, size);
      retro_assert(src_size < size);

      free(application_dir);

      out_path  += src_size;
      size      -= src_size;
      in_path   += 2;
   }
#endif

   retro_assert(strlcpy(out_path, in_path, size) < size);
}


void fill_pathname_abbreviate_special(char *out_path,
      const char *in_path, size_t size)
{
#if !defined(RARCH_CONSOLE)
   unsigned i;
   const char *candidates[3];
   const char *notations[3];
   char *application_dir     = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   const char *home          = getenv("HOME");

   application_dir[0] = '\0';

   /* application_dir could be zero-string. Safeguard against this.
    *
    * Keep application dir in front of home, moving app dir to a
    * new location inside home would break otherwise. */

   /* ugly hack - use application_dir pointer
    * before filling it in. C89 reasons */
   candidates[0] = application_dir;
   candidates[1] = home;
   candidates[2] = NULL;

   notations [0] = ":";
   notations [1] = "~";
   notations [2] = NULL;

   fill_pathname_application_path(application_dir,
         PATH_MAX_LENGTH * sizeof(char));
   path_basedir_wrapper(application_dir);

   for (i = 0; candidates[i]; i++)
   {
      if (!string_is_empty(candidates[i]) &&
            strstr(in_path, candidates[i]) == in_path)
      {
         size_t src_size  = strlcpy(out_path, notations[i], size);

         retro_assert(src_size < size);

         out_path        += src_size;
         size            -= src_size;
         in_path         += strlen(candidates[i]);

         if (!path_char_is_slash(*in_path))
         {
            retro_assert(strlcpy(out_path,
                     path_default_slash(), size) < size);
            out_path++;
            size--;
         }

         break; /* Don't allow more abbrevs to take place. */
      }
   }

   free(application_dir);
#endif

   retro_assert(strlcpy(out_path, in_path, size) < size);
}

bool fill_pathname_application_data(char *s, size_t len)
{
#if defined(_WIN32) && !defined(_XBOX)
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

   /* XDG_CONFIG_HOME falls back to $HOME/.config. */
   if (xdg)
   {
      fill_pathname_join(s, xdg, "retroarch", len);
      return true;
   }

   if (appdata)
   {
#ifdef __HAIKU__
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

#if !defined(RARCH_CONSOLE)
void fill_pathname_application_path(char *s, size_t len)
{
   size_t i;
#ifdef __APPLE__
  CFBundleRef bundle = CFBundleGetMainBundle();
#endif
#ifdef _WIN32
   DWORD ret;
   wchar_t wstr[PATH_MAX_LENGTH] = {0};
#endif
#ifdef __HAIKU__
   image_info info;
   int32_t cookie = 0;
#endif
   (void)i;

   if (!len)
      return;

#ifdef _WIN32
#ifdef LEGACY_WIN32
   ret    = GetModuleFileNameA(GetModuleHandle(NULL), s, len);
#else
   ret    = GetModuleFileNameW(GetModuleHandle(NULL), wstr, ARRAY_SIZE(wstr));

   if (*wstr)
   {
      char *str = utf16_to_utf8_string_alloc(wstr);

      if (str)
      {
         strlcpy(s, str, len);
         free(str);
      }
   }
#endif
   s[ret] = '\0';
#elif defined(__APPLE__)
   if (bundle)
   {
      CFURLRef bundle_url = CFBundleCopyBundleURL(bundle);
      CFStringRef bundle_path = CFURLCopyPath(bundle_url);
      CFStringGetCString(bundle_path, s, len, kCFStringEncodingUTF8);
      CFRelease(bundle_path);
      CFRelease(bundle_url);

      retro_assert(strlcat(s, "nobin", len) < len);
      return;
   }
#elif defined(__HAIKU__)
   while (get_next_image_info(0, &cookie, &info) == B_OK)
   {
      if (info.type == B_APP_IMAGE)
      {
         strlcpy(s, info.name, len);
         return;
      }
   }
#elif defined(__QNX__)
   char *buff = malloc(len);

   if(_cmdname(buff))
      strlcpy(s, buff, len);

   free(buff);
#else
   {
      pid_t pid;
      static const char *exts[] = { "exe", "file", "path/a.out" };
      char link_path[255];

      link_path[0] = *s = '\0';
      pid       = getpid();

      /* Linux, BSD and Solaris paths. Not standardized. */
      for (i = 0; i < ARRAY_SIZE(exts); i++)
      {
         ssize_t ret;

         snprintf(link_path, sizeof(link_path), "/proc/%u/%s",
               (unsigned)pid, exts[i]);
         ret = readlink(link_path, s, len - 1);

         if (ret >= 0)
         {
            s[ret] = '\0';
            return;
         }
      }
   }

   RARCH_ERR("Cannot resolve application path! This should not happen.\n");
#endif
}
#endif

#ifdef HAVE_XMB
const char* xmb_theme_ident(void);
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
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_ZARCH_ICONS:
#ifdef HAVE_ZARCH
         {
         }
#endif
         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_ZARCH_FONT:
#ifdef HAVE_ZARCH
         {
            char *s1 = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
            s1[0]    = '\0';

            fill_pathname_application_special(s1,
                  PATH_MAX_LENGTH * sizeof(char),
                  APPLICATION_SPECIAL_DIRECTORY_ASSETS_ZARCH);
            fill_pathname_join(s,
                  s1, "Roboto-Condensed.ttf", len);

            free(s1);
         }
#endif
         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_ZARCH:
#ifdef HAVE_ZARCH
         {
            settings_t *settings     = config_get_ptr();
            fill_pathname_join(s,
                  settings->paths.directory_assets,
                  "zarch",
                  len);
         }
#endif
         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_ICONS:
#ifdef HAVE_XMB
         {
            char *s1 = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
            char *s2 = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

            s1[0] = s2[0] = '\0';

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
               fill_pathname_join(s, s1,
                     file_path_str(FILE_PATH_BACKGROUND_IMAGE),
                     len);
               free(s1);
            }
         }
#endif
         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB:
#ifdef HAVE_XMB
         {
            char *s1 = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
            char *s2 = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
            settings_t *settings     = config_get_ptr();

            s1[0] = s2[0] = '\0';

            fill_pathname_join(
                  s1,
                  settings->paths.directory_assets,
                  "xmb",
                  PATH_MAX_LENGTH * sizeof(char)
                  );
            fill_pathname_join(s2,
                  s1, xmb_theme_ident(),
                  PATH_MAX_LENGTH * sizeof(char)
                  );
            strlcpy(s, s2, len);
            free(s1);
            free(s2);
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
               fill_pathname_join(s, s1,
                     file_path_str(FILE_PATH_TTF_FONT),
                     len);

               free(s1);
            }
         }
#endif
         break;
      case APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES:
      {
        char *s1 = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
        char *s2 = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
        settings_t *settings     = config_get_ptr();

        s1[0] = s2[0] = '\0';

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
   char *path_short = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
#ifdef HAVE_COMPRESSION
   char *last_slash = NULL;
#endif

   path_short[0] = '\0';

   fill_pathname(path_short, path_basename(in_path), "",
         PATH_MAX_LENGTH * sizeof(char)
         );

#ifdef HAVE_COMPRESSION
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
      retro_assert(strlen(last_slash) > 1);
      strlcpy(out_rep, last_slash + 1, size);
      free(path_short);
      return;
   }
#endif

   fill_short_pathname_representation(out_rep, in_path, size);
   free(path_short);
}

/**
 * path_basedir:
 * @path               : path
 *
 * Extracts base directory by mutating path.
 * Keeps trailing '/'.
 **/
void path_basedir_wrapper(char *path)
{
   char *last = NULL;
   if (strlen(path) < 2)
      return;

#ifdef HAVE_COMPRESSION
   /* We want to find the directory with the archive in basedir. */
   last = (char*)path_get_archive_delim(path);
   if (last)
      *last = '\0';
#endif

   last = find_last_slash(path);

   if (last)
      last[1] = '\0';
   else
      snprintf(path, 3, ".%s", path_default_slash());
}
