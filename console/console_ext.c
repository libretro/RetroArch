/*  SSNES - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include "../boolean.h"
#include "../compat/strl.h"
#include "../libretro.h"
#include "../general.h"
#include "../compat/strl.h"
#include "main_wrap.h"

#ifdef HAVE_ZLIB
#include "szlib/zlib.h"
#define WRITEBUFFERSIZE (1024 * 512)
#endif

#ifdef _WIN32
#include "../compat/posix_string.h"
#endif

#define MAX_ARGS 32

/*============================================================
	ROM EXTENSIONS
============================================================ */

static char g_rom_ext[1024];

void ssnes_console_set_rom_ext(const char *ext)
{
   strlcpy(g_rom_ext, ext, sizeof(g_rom_ext));
}

const char *ssnes_console_get_rom_ext(void)
{
   const char *retval = NULL;

   if (*g_rom_ext)
      retval = g_rom_ext;
   else
      retval = "ZIP|zip";

   return retval;
}

void ssnes_console_name_from_id(char *name, size_t size)
{
   if (size == 0)
      return;

   struct retro_system_info info;
   retro_get_system_info(&info);
   const char *id = info.library_name ? info.library_name : "Unknown";

   if (!id || strlen(id) >= size)
   {
      name[0] = '\0';
      return;
   }

   name[strlen(id)] = '\0';

   for (size_t i = 0; id[i] != '\0'; i++)
   {
      char c = id[i];
      if (isspace(c) || isblank(c))
         name[i] = '_';
      else
         name[i] = tolower(c);
   }
}

#ifdef HAVE_ZLIB
static int ssnes_extract_currentfile_in_zip(unzFile uf)
{
   char filename_inzip[PATH_MAX];
   FILE *fout = NULL;

   unz_file_info file_info;
   int err = unzGetCurrentFileInfo(uf,
         &file_info, filename_inzip, sizeof(filename_inzip),
         NULL, 0, NULL, 0);

   if (err != UNZ_OK)
   {
      SSNES_ERR("Error %d with zipfile in unzGetCurrentFileInfo.\n", err);
      return err;
   }

   size_t size_buf = WRITEBUFFERSIZE;
   void *buf = malloc(size_buf);
   if (!buf)
   {
      SSNES_ERR("Error allocating memory\n");
      return UNZ_INTERNALERROR;
   }

   char write_filename[PATH_MAX];

#if defined(__CELLOS_LV2__)
   snprintf(write_filename, sizeof(write_filename), "/dev_hdd1/%s", filename_inzip);
#elif defined(_XBOX)
   snprintf(write_filename, sizeof(write_filename), "cache:\\%s", filename_inzip);
#endif

   err = unzOpenCurrentFile(uf);
   if (err != UNZ_OK)
      SSNES_ERR("Error %d with zipfile in unzOpenCurrentFile.\n", err);
   else
   {
      /* success */
      fout = fopen(write_filename, "wb");

      if (!fout)
         SSNES_ERR("Error opening %s.\n", write_filename);
   }

   if (fout)
   {
      SSNES_LOG("Extracting: %s\n", write_filename);

      do
      {
         err = unzReadCurrentFile(uf, buf, size_buf);
         if (err < 0)
         {
            SSNES_ERR("error %d with zipfile in unzReadCurrentFile.\n", err);
            break;
         }

         if (err > 0)
         {
            if (fwrite(buf, err, 1, fout) != 1)
            {
               SSNES_ERR("Error in writing extracted file.\n");
               err = UNZ_ERRNO;
               break;
            }
         }
      } while (err > 0);

      if (fout)
         fclose(fout);
   }

   if (err == UNZ_OK)
   {
      err = unzCloseCurrentFile (uf);
      if (err != UNZ_OK)
         SSNES_ERR("Error %d with zipfile in unzCloseCurrentFile.\n", err);
   }
   else
      unzCloseCurrentFile(uf); 

   free(buf);
   return err;
}

int ssnes_extract_zipfile(const char *zip_path)
{
   unzFile uf = unzOpen(zip_path); 

   unz_global_info gi;
   int err = unzGetGlobalInfo(uf, &gi);
   if (err != UNZ_OK)
      SSNES_ERR("error %d with zipfile in unzGetGlobalInfo \n",err);

   for (unsigned i = 0; i < gi.number_entry; i++)
   {
      if (ssnes_extract_currentfile_in_zip(uf) != UNZ_OK)
         break;

      if ((i + 1) < gi.number_entry)
      {
         err = unzGoToNextFile(uf);
         if (err != UNZ_OK)
         {
            SSNES_ERR("error %d with zipfile in unzGoToNextFile\n",err);
            break;
         }
      }
   }

   return 0;
}

#endif

int ssnes_main_init_wrap(const struct ssnes_main_wrap *args)
{
   int argc = 0;
   char *argv[MAX_ARGS] = {NULL};

   argv[argc++] = strdup("ssnes");
   
   if (args->rom_path)
      argv[argc++] = strdup(args->rom_path);

   if (args->sram_path)
   {
      argv[argc++] = strdup("-s");
      argv[argc++] = strdup(args->sram_path);
   }

   if (args->state_path)
   {
      argv[argc++] = strdup("-S");
      argv[argc++] = strdup(args->state_path);
   }

   if (args->config_path)
   {
      argv[argc++] = strdup("-c");
      argv[argc++] = strdup(args->config_path);
   }

   if (args->verbose)
      argv[argc++] = strdup("-v");

   int ret = ssnes_main_init(argc, argv);

   char **tmp = argv;
   while (*tmp)
   {
      free(*tmp);
      tmp++;
   }

   return ret;
}

/*============================================================
	INPUT EXTENSIONS
============================================================ */

#include "console_ext_input.h"

struct platform_bind
{
   uint64_t joykey;
   const char *label;
};

uint64_t ssnes_default_keybind_lut[SSNES_FIRST_META_KEY];

char ssnes_default_libretro_keybind_name_lut[SSNES_FIRST_META_KEY][256] = {
   "B Button",          /* RETRO_DEVICE_ID_JOYPAD_B      */
   "Y Button",          /* RETRO_DEVICE_ID_JOYPAD_Y      */
   "Select button",     /* RETRO_DEVICE_ID_JOYPAD_SELECT */
   "Start button",      /* RETRO_DEVICE_ID_JOYPAD_START  */
   "D-Pad Up",          /* RETRO_DEVICE_ID_JOYPAD_UP     */
   "D-Pad Down",        /* RETRO_DEVICE_ID_JOYPAD_DOWN   */
   "D-Pad Left",        /* RETRO_DEVICE_ID_JOYPAD_LEFT   */
   "D-Pad Right",       /* RETRO_DEVICE_ID_JOYPAD_RIGHT  */
   "A Button",          /* RETRO_DEVICE_ID_JOYPAD_A      */
   "X Button",          /* RETRO_DEVICE_ID_JOYPAD_X      */
   "L Button",          /* RETRO_DEVICE_ID_JOYPAD_L      */
   "R Button",          /* RETRO_DEVICE_ID_JOYPAD_R      */
};

#if defined(__CELLOS_LV2__)
uint64_t ssnes_platform_keybind_lut[SSNES_LAST_PLATFORM_KEY] = {
   CTRL_CIRCLE_MASK,
   CTRL_CROSS_MASK,
   CTRL_TRIANGLE_MASK,
   CTRL_SQUARE_MASK,
   CTRL_UP_MASK,
   CTRL_DOWN_MASK,
   CTRL_LEFT_MASK,
   CTRL_RIGHT_MASK,
   CTRL_SELECT_MASK,
   CTRL_START_MASK,
   CTRL_L1_MASK,
   CTRL_L2_MASK,
   CTRL_L3_MASK,
   CTRL_R1_MASK,
   CTRL_R2_MASK,
   CTRL_R3_MASK,
   CTRL_LSTICK_LEFT_MASK,
   CTRL_LSTICK_RIGHT_MASK,
   CTRL_LSTICK_UP_MASK,
   CTRL_LSTICK_DOWN_MASK,
   CTRL_LEFT_MASK | CTRL_LSTICK_LEFT_MASK,
   CTRL_RIGHT_MASK | CTRL_LSTICK_RIGHT_MASK,
   CTRL_UP_MASK | CTRL_LSTICK_UP_MASK,
   CTRL_DOWN_MASK | CTRL_LSTICK_DOWN_MASK,
   CTRL_RSTICK_LEFT_MASK,
   CTRL_RSTICK_RIGHT_MASK,
   CTRL_RSTICK_UP_MASK,
   CTRL_RSTICK_DOWN_MASK,
   CTRL_LEFT_MASK | CTRL_RSTICK_LEFT_MASK,
   CTRL_RIGHT_MASK | CTRL_RSTICK_RIGHT_MASK,
   CTRL_UP_MASK | CTRL_RSTICK_UP_MASK,
   CTRL_DOWN_MASK | CTRL_RSTICK_DOWN_MASK,
};

static const struct platform_bind platform_keys[] = {
   { CTRL_CIRCLE_MASK, "Circle button" },
   { CTRL_CROSS_MASK, "Cross button" },
   { CTRL_TRIANGLE_MASK, "Triangle button" },
   { CTRL_SQUARE_MASK, "Square button" },
   { CTRL_UP_MASK, "D-Pad Up" },
   { CTRL_DOWN_MASK, "D-Pad Down" },
   { CTRL_LEFT_MASK, "D-Pad Left" },
   { CTRL_RIGHT_MASK, "D-Pad Right" },
   { CTRL_SELECT_MASK, "Select button" },
   { CTRL_START_MASK, "Start button" },
   { CTRL_L1_MASK, "L1 button" },
   { CTRL_L2_MASK, "L2 button" },
   { CTRL_L3_MASK, "L3 button" },
   { CTRL_R1_MASK, "R1 button" },
   { CTRL_R2_MASK, "R2 button" },
   { CTRL_R3_MASK, "R3 button" },
   { CTRL_LSTICK_LEFT_MASK, "LStick Left" },
   { CTRL_LSTICK_RIGHT_MASK, "LStick Right" },
   { CTRL_LSTICK_UP_MASK, "LStick Up" },
   { CTRL_LSTICK_DOWN_MASK, "LStick Down" },
   { CTRL_LEFT_MASK | CTRL_LSTICK_LEFT_MASK, "LStick D-Pad Left" },
   { CTRL_RIGHT_MASK | CTRL_LSTICK_RIGHT_MASK, "LStick D-Pad Right" },
   { CTRL_UP_MASK | CTRL_LSTICK_UP_MASK, "LStick D-Pad Up" },
   { CTRL_DOWN_MASK | CTRL_LSTICK_DOWN_MASK, "LStick D-Pad Down" },
   { CTRL_RSTICK_LEFT_MASK, "RStick Left" },
   { CTRL_RSTICK_RIGHT_MASK, "RStick Right" },
   { CTRL_RSTICK_UP_MASK, "RStick Up" },
   { CTRL_RSTICK_DOWN_MASK, "RStick Down" },
   { CTRL_LEFT_MASK | CTRL_RSTICK_LEFT_MASK, "RStick D-Pad Left" },
   { CTRL_RIGHT_MASK | CTRL_RSTICK_RIGHT_MASK, "RStick D-Pad Right" },
   { CTRL_UP_MASK | CTRL_RSTICK_UP_MASK, "RStick D-Pad Up" },
   { CTRL_DOWN_MASK | CTRL_RSTICK_DOWN_MASK, "RStick D-Pad Down" },
};
#elif defined(_XBOX)
uint64_t ssnes_platform_keybind_lut[SSNES_LAST_PLATFORM_KEY] = {
   XINPUT_GAMEPAD_B,
   XINPUT_GAMEPAD_A,
   XINPUT_GAMEPAD_Y,
   XINPUT_GAMEPAD_X,
   XINPUT_GAMEPAD_DPAD_UP,
   XINPUT_GAMEPAD_DPAD_DOWN,
   XINPUT_GAMEPAD_DPAD_LEFT,
   XINPUT_GAMEPAD_DPAD_RIGHT,
   XINPUT_GAMEPAD_BACK,
   XINPUT_GAMEPAD_START,
   XINPUT_GAMEPAD_LEFT_SHOULDER,
   XINPUT_GAMEPAD_LEFT_TRIGGER,
   XINPUT_GAMEPAD_LEFT_THUMB,
   XINPUT_GAMEPAD_RIGHT_SHOULDER,
   XINPUT_GAMEPAD_RIGHT_TRIGGER,
   XINPUT_GAMEPAD_RIGHT_THUMB,
   XINPUT_GAMEPAD_LSTICK_LEFT_MASK,
   XINPUT_GAMEPAD_LSTICK_RIGHT_MASK,
   XINPUT_GAMEPAD_LSTICK_UP_MASK,
   XINPUT_GAMEPAD_LSTICK_DOWN_MASK,
   XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_LSTICK_LEFT_MASK,
   XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_LSTICK_RIGHT_MASK,
   XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_LSTICK_UP_MASK,
   XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_LSTICK_DOWN_MASK,
   XINPUT_GAMEPAD_RSTICK_LEFT_MASK,
   XINPUT_GAMEPAD_RSTICK_RIGHT_MASK,
   XINPUT_GAMEPAD_RSTICK_UP_MASK,
   XINPUT_GAMEPAD_RSTICK_DOWN_MASK,
   XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_RSTICK_LEFT_MASK,
   XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_RSTICK_RIGHT_MASK,
   XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_RSTICK_UP_MASK,
   XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_RSTICK_DOWN_MASK,
};

static const struct platform_bind platform_keys[] = {
   { XINPUT_GAMEPAD_B, "B button" },
   { XINPUT_GAMEPAD_A, "A button" },
   { XINPUT_GAMEPAD_Y, "Y button" },
   { XINPUT_GAMEPAD_X, "X button" },
   { XINPUT_GAMEPAD_DPAD_UP, "D-Pad Up" },
   { XINPUT_GAMEPAD_DPAD_DOWN, "D-Pad Down" },
   { XINPUT_GAMEPAD_DPAD_LEFT, "D-Pad Left" },
   { XINPUT_GAMEPAD_DPAD_RIGHT, "D-Pad Right" },
   { XINPUT_GAMEPAD_BACK, "Back button" },
   { XINPUT_GAMEPAD_START, "Start button" },
   { XINPUT_GAMEPAD_LEFT_SHOULDER, "Left Shoulder" },
   { XINPUT_GAMEPAD_LEFT_TRIGGER, "Left Trigger" },
   { XINPUT_GAMEPAD_LEFT_THUMB, "Left Thumb" },
   { XINPUT_GAMEPAD_RIGHT_SHOULDER, "Right Shoulder" },
   { XINPUT_GAMEPAD_RIGHT_TRIGGER, "Right Trigger" },
   { XINPUT_GAMEPAD_RIGHT_THUMB, "Right Thumb" },
   { XINPUT_GAMEPAD_LSTICK_LEFT_MASK, "LStick Left" },
   { XINPUT_GAMEPAD_LSTICK_RIGHT_MASK, "LStick Right" },
   { XINPUT_GAMEPAD_LSTICK_UP_MASK, "LStick Up" },
   { XINPUT_GAMEPAD_LSTICK_DOWN_MASK, "LStick Down" },
   { XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_LSTICK_LEFT_MASK, "LStick D-Pad Left" },
   { XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_LSTICK_RIGHT_MASK, "LStick D-Pad Right" },
   { XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_LSTICK_UP_MASK, "LStick D-Pad Up" },
   { XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_LSTICK_DOWN_MASK, "LStick D-Pad Down" },
   { XINPUT_GAMEPAD_RSTICK_LEFT_MASK, "RStick Left" },
   { XINPUT_GAMEPAD_RSTICK_RIGHT_MASK, "RStick Right" },
   { XINPUT_GAMEPAD_RSTICK_UP_MASK, "RStick Up" },
   { XINPUT_GAMEPAD_RSTICK_DOWN_MASK, "RStick Down" },
   { XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_RSTICK_LEFT_MASK, "RStick D-Pad Left" },
   { XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_RSTICK_RIGHT_MASK, "RStick D-Pad Right" },
   { XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_RSTICK_UP_MASK, "RStick D-Pad Up" },
   { XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_RSTICK_DOWN_MASK, "RStick D-Pad Down" },
};
#endif

uint64_t ssnes_input_find_previous_platform_key(uint64_t joykey)
{
   size_t arr_size = sizeof(platform_keys) / sizeof(platform_keys[0]);

   if (platform_keys[0].joykey == joykey)
      return joykey;

   for (size_t i = 1; i < arr_size; i++)
   {
      if (platform_keys[i].joykey == joykey)
         return platform_keys[i - 1].joykey;
   }

   return NO_BTN;
}

uint64_t ssnes_input_find_next_platform_key(uint64_t joykey)
{
   size_t arr_size = sizeof(platform_keys) / sizeof(platform_keys[0]);
   if (platform_keys[arr_size - 1].joykey == joykey)
      return joykey;

   for (size_t i = 0; i < arr_size - 1; i++)
   {
      if (platform_keys[i].joykey == joykey)
         return platform_keys[i + 1].joykey;
   }

   return NO_BTN;
}

const char *ssnes_input_find_platform_key_label(uint64_t joykey)
{
   if (joykey == NO_BTN)
      return "No button";

   size_t arr_size = sizeof(platform_keys) / sizeof(platform_keys[0]);
   for (size_t i = 0; i < arr_size; i++)
   {
      if (platform_keys[i].joykey == joykey)
         return platform_keys[i].label;
   }

   return "Unknown";
}

void ssnes_input_set_default_keybind_names_for_emulator(void)
{
   struct retro_system_info info;
   retro_get_system_info(&info);
   const char *id = info.library_name ? info.library_name : "Unknown";

   // Genesis Plus GX/Next
   if (strstr(id, "Genesis Plus GX"))
   {
      strlcpy(ssnes_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_B],
            "B button", sizeof(ssnes_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_B]));
      strlcpy(ssnes_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_A],
            "C button", sizeof(ssnes_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_A]));
      strlcpy(ssnes_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_X],
            "Y button", sizeof(ssnes_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_X]));
      strlcpy(ssnes_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_Y],
            "A button", sizeof(ssnes_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_Y]));
      strlcpy(ssnes_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_L],
            "X button", sizeof(ssnes_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_L]));
      strlcpy(ssnes_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_R],
            "Z button", sizeof(ssnes_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_R]));
      strlcpy(ssnes_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_SELECT],
            "Mode button", sizeof(ssnes_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_SELECT]));
   }
}
