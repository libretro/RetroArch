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
#include "console_ext.h"
#include "../file.h"

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

const char *ssnes_console_get_rom_ext(void)
{
   const char *retval = NULL;

   struct retro_system_info info;
   retro_get_system_info(&info);

   if (info.valid_extensions)
      retval = info.valid_extensions;
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
#elif defined(GEKKO)
static const struct platform_bind platform_keys[] = {
   { PAD_BUTTON_B, "(NGC) B button" },
   { PAD_BUTTON_A, "(NGC) A button" },
   { PAD_BUTTON_Y, "(NGC) Y button" },
   { PAD_BUTTON_X, "(NGC) X button" },
   { PAD_BUTTON_UP, "(NGC) D-Pad Up" },
   { PAD_BUTTON_DOWN, "(NGC) D-Pad Down" },
   { PAD_BUTTON_LEFT, "(NGC) D-Pad Left" },
   { PAD_BUTTON_RIGHT, "(NGC) D-Pad Right" },
   { PAD_TRIGGER_Z, "(NGC) Z trigger" },
   { PAD_BUTTON_START, "(NGC) Start button" },
   { PAD_TRIGGER_L, "(NGC) Left Trigger" },
   { PAD_TRIGGER_R, "(NGC) Right Trigger" },
   //{ XINPUT_GAMEPAD_LSTICK_LEFT_MASK, "LStick Left" },
   //{ XINPUT_GAMEPAD_LSTICK_RIGHT_MASK, "LStick Right" },
   //{ XINPUT_GAMEPAD_LSTICK_UP_MASK, "LStick Up" },
   //{ XINPUT_GAMEPAD_LSTICK_DOWN_MASK, "LStick Down" },
   //{ XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_LSTICK_LEFT_MASK, "LStick D-Pad Left" },
   //{ XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_LSTICK_RIGHT_MASK, "LStick D-Pad Right" },
   //{ XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_LSTICK_UP_MASK, "LStick D-Pad Up" },
   //{ XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_LSTICK_DOWN_MASK, "LStick D-Pad Down" },
   //{ XINPUT_GAMEPAD_RSTICK_LEFT_MASK, "RStick Left" },
   //{ XINPUT_GAMEPAD_RSTICK_RIGHT_MASK, "RStick Right" },
   //{ XINPUT_GAMEPAD_RSTICK_UP_MASK, "RStick Up" },
   //{ XINPUT_GAMEPAD_RSTICK_DOWN_MASK, "RStick Down" },
   //{ XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_RSTICK_LEFT_MASK, "RStick D-Pad Left" },
   //{ XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_RSTICK_RIGHT_MASK, "RStick D-Pad Right" },
   //{ XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_RSTICK_UP_MASK, "RStick D-Pad Up" },
   //{ XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_RSTICK_DOWN_MASK, "RStick D-Pad Down" },
#ifdef HW_RVL
   { WPAD_CLASSIC_BUTTON_B, "(Wii Classici) B button" },
   { WPAD_CLASSIC_BUTTON_A, "(Wii Classic) A button" },
   { WPAD_CLASSIC_BUTTON_Y, "(Wii Classic) Y button" },
   { WPAD_CLASSIC_BUTTON_X, "(Wii Classic) X button" },
   { WPAD_CLASSIC_BUTTON_UP, "(Wii Classic) D-Pad Up" },
   { WPAD_CLASSIC_BUTTON_DOWN, "(Wii Classic) D-Pad Down" },
   { WPAD_CLASSIC_BUTTON_LEFT, "(Wii Classic) D-Pad Left" },
   { WPAD_CLASSIC_BUTTON_RIGHT, "(Wii Classic) D-Pad Right" },
   { WPAD_CLASSIC_BUTTON_MINUS, "(Wii Classic) Select/Minus button" },
   { WPAD_CLASSIC_BUTTON_PLUS, "(Wii Classic) Start/Plus button" },
   { WPAD_CLASSIC_BUTTON_HOME, "(Wii Classic) Home button" },
   { WPAD_CLASSIC_BUTTON_FULL_L, "(Wii Classic) Left Trigger" },
   { WPAD_CLASSIC_BUTTON_FULL_R, "(Wii Classic) Right Trigger" },
   { WPAD_CLASSIC_BUTTON_ZL, "(Wii Classic) ZL button" },
   { WPAD_CLASSIC_BUTTON_ZR, "(Wii Classic) ZR button" },
#endif
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

void ssnes_input_set_keybind(unsigned player, unsigned keybind_action, uint64_t default_retro_joypad_id)
{
   uint64_t *key = &g_settings.input.binds[player][default_retro_joypad_id].joykey;

   switch (keybind_action)
   {
      case KEYBIND_DECREMENT:
         *key = ssnes_input_find_previous_platform_key(*key);
         break;

      case KEYBIND_INCREMENT:
         *key = ssnes_input_find_next_platform_key(*key);
         break;

      case KEYBIND_DEFAULT:
         *key = ssnes_default_keybind_lut[default_retro_joypad_id];
         break;

      default:
         break;
   }
}

void ssnes_input_set_default_keybinds(unsigned player)
{
   for (unsigned i = 0; i < SSNES_FIRST_META_KEY; i++)
   {
      g_settings.input.binds[player][i].id = i;
      g_settings.input.binds[player][i].joykey = ssnes_default_keybind_lut[i];
   }
   g_settings.input.dpad_emulation[player] = DPAD_EMULATION_LSTICK;
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

/*============================================================
  VIDEO EXTENSIONS
  ============================================================ */

struct aspect_ratio_elem aspectratio_lut[ASPECT_RATIO_END] = {
   { "1:1",    1.0f },
   { "2:1",    2.0f },
   { "3:2",    1.5f },
   { "3:4",    0.75f },
   { "4:1",    4.0f },
   { "4:3",    1.3333f },
   { "4:4",    1.0f },
   { "5:4",    1.25f },
   { "6:5",    1.2f },
   { "7:9",    0.7777f },
   { "8:3",    2.6666f },
   { "8:7",    1.1428f },
   { "16:9",   1.7778f },
   { "16:10",  1.6f },
   { "16:15",  3.2f },
   { "19:12",  1.5833f },
   { "19:14",  1.3571f },
   { "30:17",  1.7647f },
   { "32:9",   3.5555f },
   { "Auto",   0.0f },
   { "Custom", 0.0f }
};

/*============================================================
  LIBRETRO
  ============================================================ */

#ifdef HAVE_LIBRETRO_MANAGEMENT
bool ssnes_manage_libretro_core(const char *full_path, const char *path, const char *exe_ext)
{
   g_extern.verbose = true;
   bool return_code;

   bool set_libretro_path = false;
   char tmp_path2[1024], tmp_pathnewfile[1024];
   SSNES_LOG("Assumed path of CORE executable: [%s]\n", full_path);

   if (path_file_exists(full_path))
   {
      // if CORE executable exists, this means we have just installed
      // a new libretro port and therefore we need to change it to a more
      // sane name.

#if defined(__CELLOS_LV2__)
      CellFsErrno ret;
#else
      int ret;
#endif

      ssnes_console_name_from_id(tmp_path2, sizeof(tmp_path2));
      strlcat(tmp_path2, exe_ext, sizeof(tmp_path2));
      snprintf(tmp_pathnewfile, sizeof(tmp_pathnewfile), "%s%s", path, tmp_path2);

      if (path_file_exists(tmp_pathnewfile))
      {
         // if libretro core already exists, this means we are
         // upgrading the libretro core - so delete pre-existing
         // file first.

         SSNES_LOG("Upgrading emulator core...\n");
#if defined(__CELLOS_LV2__)
         ret = cellFsUnlink(tmp_pathnewfile);
         if (ret == CELL_FS_SUCCEEDED)
#elif defined(_XBOX)
            ret = DeleteFile(tmp_pathnewfile);
         if (ret != 0)
#endif
         {
            SSNES_LOG("Succeeded in removing pre-existing libretro core: [%s].\n", tmp_pathnewfile);
         }
         else
            SSNES_LOG("Failed to remove pre-existing libretro core: [%s].\n", tmp_pathnewfile);
      }

      //now attempt the renaming.
#if defined(__CELLOS_LV2__)
      ret = cellFsRename(full_path, tmp_pathnewfile);

      if (ret != CELL_FS_SUCCEEDED)
#elif defined(_XBOX)
         ret = MoveFileExA(full_path, tmp_pathnewfile, NULL);
      if (ret == 0)
#endif
      {
         SSNES_ERR("Failed to rename CORE executable.\n");
      }
      else
      {
         SSNES_LOG("Libsnes core [%s] renamed to: [%s].\n", full_path, tmp_pathnewfile);
         set_libretro_path = true;
      }
   }
   else
   {
      SSNES_LOG("CORE executable was not found, libretro core path will be loaded from config file.\n");
   }

   if (set_libretro_path)
   {
      // CORE executable has been renamed, libretro path will now be set to the recently
      // renamed new libretro core.
      strlcpy(g_settings.libretro, tmp_pathnewfile, sizeof(g_settings.libretro));
      return_code = 0;
   }
   else
   {
      // There was no CORE executable present, or the CORE executable file was not renamed.
      // The libretro core path will still be loaded from the config file.
      return_code = 1;
   }

   g_extern.verbose = false;

   return return_code;
}
#endif

/*============================================================
  SSNES MAIN WRAP
  ============================================================ */

#ifdef HAVE_SSNES_MAIN_WRAP

void ssnes_startup (const char * config_path)
{
   if(g_console.initialize_ssnes_enable)
   {
      if(g_console.emulator_initialized)
         ssnes_main_deinit();

      struct ssnes_main_wrap args = {0};

      args.verbose = g_extern.verbose;
      args.config_path = config_path;
      args.sram_path = g_console.default_sram_dir_enable ? g_console.default_sram_dir : NULL,
      args.state_path = g_console.default_savestate_dir_enable ? g_console.default_savestate_dir : NULL,
      args.rom_path = g_console.rom_path;

      int init_ret = ssnes_main_init_wrap(&args);
      (void)init_ret;
      g_console.emulator_initialized = 1;
      g_console.initialize_ssnes_enable = 0;
   }
}

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

#ifdef HAVE_FILE_LOGGER
   SSNES_LOG("foo\n");
   for(int i = 0; i < argc; i++)
      SSNES_LOG("arg #%d: %s\n", i, argv[i]);
   SSNES_LOG("bar\n");
#endif

   int ret = ssnes_main_init(argc, argv);

   char **tmp = argv;
   while (*tmp)
   {
      free(*tmp);
      tmp++;
   }


   return ret;
}

#endif

#ifdef HAVE_SSNES_EXEC

#ifdef __CELLOS_LV2__
#include <cell/sysmodule.h>
#include <sys/process.h>
#include <sysutil/sysutil_common.h>
#include <netex/net.h>
#include <np.h>
#include <np/drm.h>
#endif

void ssnes_exec (void)
{
   if(g_console.return_to_launcher)
   {
      SSNES_LOG("Attempt to load executable: [%s].\n", g_console.launch_app_on_exit);
#if defined(_XBOX)
      XLaunchNewImage(g_console.launch_app_on_exit, NULL);
#elif defined(__CELLOS_LV2__)
      char spawn_data[256];
      for(unsigned int i = 0; i < sizeof(spawn_data); ++i)
         spawn_data[i] = i & 0xff;

      char spawn_data_size[16];
      sprintf(spawn_data_size, "%d", 256);

      const char * const spawn_argv[] = {
         spawn_data_size,
         "test argv for",
         "sceNpDrmProcessExitSpawn2()",
         NULL
      };

      SceNpDrmKey * k_licensee = NULL;
      int ret = sceNpDrmProcessExitSpawn2(k_licensee, g_console.launch_app_on_exit, (const char** const)spawn_argv, NULL, (sys_addr_t)spawn_data, 256, 1000, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
      if(ret <  0)
      {
         SSNES_WARN("SELF file is not of NPDRM type, trying another approach to boot it...\n");
	 sys_game_process_exitspawn(g_console.launch_app_on_exit, NULL, NULL, NULL, 0, 1000, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
      }
      sceNpTerm();
      cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_NP);
      cellSysmoduleUnloadModule(CELL_SYSMODULE_NET);
#endif
   }
}

#endif
