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

#ifdef HAVE_CONFIGFILE
#include "../conf/config_file.h"
#include "../conf/config_file_macros.h"
#endif

#ifdef HAVE_ZLIB
#include "rzlib/zlib.h"
#define WRITEBUFFERSIZE (1024 * 512)
#endif

#ifdef _WIN32
#include "../compat/posix_string.h"
#endif

#define MAX_ARGS 32

/*============================================================
	ROM EXTENSIONS
============================================================ */

const char *rarch_console_get_rom_ext(void)
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

void rarch_console_name_from_id(char *name, size_t size)
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
static int rarch_extract_currentfile_in_zip(unzFile uf)
{
   char filename_inzip[PATH_MAX];
   FILE *fout = NULL;

   unz_file_info file_info;
   int err = unzGetCurrentFileInfo(uf,
         &file_info, filename_inzip, sizeof(filename_inzip),
         NULL, 0, NULL, 0);

   if (err != UNZ_OK)
   {
      RARCH_ERR("Error %d with ZIP file in unzGetCurrentFileInfo.\n", err);
      return err;
   }

   size_t size_buf = WRITEBUFFERSIZE;
   void *buf = malloc(size_buf);
   if (!buf)
   {
      RARCH_ERR("Error allocating memory\n");
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
      RARCH_ERR("Error %d with ZIP file in unzOpenCurrentFile.\n", err);
   else
   {
      /* success */
      fout = fopen(write_filename, "wb");

      if (!fout)
         RARCH_ERR("Error opening %s.\n", write_filename);
   }

   if (fout)
   {
      RARCH_LOG("Extracting: %s\n", write_filename);

      do
      {
         err = unzReadCurrentFile(uf, buf, size_buf);
         if (err < 0)
         {
            RARCH_ERR("error %d with ZIP file in unzReadCurrentFile.\n", err);
            break;
         }

         if (err > 0)
         {
            if (fwrite(buf, err, 1, fout) != 1)
            {
               RARCH_ERR("Error in writing extracted file.\n");
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
         RARCH_ERR("Error %d with ZIP file in unzCloseCurrentFile.\n", err);
   }
   else
      unzCloseCurrentFile(uf); 

   free(buf);
   return err;
}

int rarch_extract_zipfile(const char *zip_path)
{
   unzFile uf = unzOpen(zip_path); 

   unz_global_info gi;
   int err = unzGetGlobalInfo(uf, &gi);
   if (err != UNZ_OK)
      RARCH_ERR("error %d with ZIP file in unzGetGlobalInfo \n",err);

   for (unsigned i = 0; i < gi.number_entry; i++)
   {
      if (rarch_extract_currentfile_in_zip(uf) != UNZ_OK)
         break;

      if ((i + 1) < gi.number_entry)
      {
         err = unzGoToNextFile(uf);
         if (err != UNZ_OK)
         {
            RARCH_ERR("error %d with ZIP file in unzGoToNextFile\n",err);
            break;
         }
      }
   }

   if(g_console.info_msg_enable)
      rarch_settings_msg(S_MSG_EXTRACTED_ZIPFILE, S_DELAY_180);

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

uint64_t rarch_default_keybind_lut[RARCH_FIRST_META_KEY];

char rarch_default_libretro_keybind_name_lut[RARCH_FIRST_META_KEY][256] = {
   "RetroPad Button B",          /* RETRO_DEVICE_ID_JOYPAD_B      */
   "RetroPad Button Y",          /* RETRO_DEVICE_ID_JOYPAD_Y      */
   "RetroPad Button Select",     /* RETRO_DEVICE_ID_JOYPAD_SELECT */
   "RetroPad Button Start",      /* RETRO_DEVICE_ID_JOYPAD_START  */
   "RetroPad D-Pad Up",          /* RETRO_DEVICE_ID_JOYPAD_UP     */
   "RetroPad D-Pad Down",        /* RETRO_DEVICE_ID_JOYPAD_DOWN   */
   "RetroPad D-Pad Left",        /* RETRO_DEVICE_ID_JOYPAD_LEFT   */
   "RetroPad D-Pad Right",       /* RETRO_DEVICE_ID_JOYPAD_RIGHT  */
   "RetroPad Button A",          /* RETRO_DEVICE_ID_JOYPAD_A      */
   "RetroPad Button X",          /* RETRO_DEVICE_ID_JOYPAD_X      */
   "RetroPad Button L1",          /* RETRO_DEVICE_ID_JOYPAD_L      */
   "RetroPad Button R1",          /* RETRO_DEVICE_ID_JOYPAD_R      */
   "RetroPad Button L2",         /* RETRO_DEVICE_ID_JOYPAD_L2     */
   "RetroPad Button R2",         /* RETRO_DEVICE_ID_JOYPAD_R2     */
   "RetroPad Button L3",         /* RETRO_DEVICE_ID_JOYPAD_L3     */
   "RetroPad Button R3",         /* RETRO_DEVICE_ID_JOYPAD_R3     */
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

uint64_t rarch_input_find_previous_platform_key(uint64_t joykey)
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

uint64_t rarch_input_find_next_platform_key(uint64_t joykey)
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

const char *rarch_input_find_platform_key_label(uint64_t joykey)
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

void rarch_input_set_keybind(unsigned player, unsigned keybind_action, uint64_t default_retro_joypad_id)
{
   uint64_t *key = &g_settings.input.binds[player][default_retro_joypad_id].joykey;

   switch (keybind_action)
   {
      case KEYBIND_DECREMENT:
         *key = rarch_input_find_previous_platform_key(*key);
         break;

      case KEYBIND_INCREMENT:
         *key = rarch_input_find_next_platform_key(*key);
         break;

      case KEYBIND_DEFAULT:
         *key = rarch_default_keybind_lut[default_retro_joypad_id];
         break;

      default:
         break;
   }
}

void rarch_input_set_default_keybinds(unsigned player)
{
   for (unsigned i = 0; i < RARCH_FIRST_META_KEY; i++)
   {
      g_settings.input.binds[player][i].id = i;
      g_settings.input.binds[player][i].joykey = rarch_default_keybind_lut[i];
   }
   g_settings.input.dpad_emulation[player] = DPAD_EMULATION_LSTICK;
}

void rarch_input_set_default_keybind_names_for_emulator(void)
{
   struct retro_system_info info;
   retro_get_system_info(&info);
   const char *id = info.library_name ? info.library_name : "Unknown";

   // Genesis Plus GX/Next
   if (strstr(id, "Genesis Plus GX"))
   {
      strlcpy(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_B],
            "B button", sizeof(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_B]));
      strlcpy(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_A],
            "C button", sizeof(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_A]));
      strlcpy(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_X],
            "Y button", sizeof(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_X]));
      strlcpy(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_Y],
            "A button", sizeof(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_Y]));
      strlcpy(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_L],
            "X button", sizeof(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_L]));
      strlcpy(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_R],
            "Z button", sizeof(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_R]));
      strlcpy(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_SELECT],
            "Mode button", sizeof(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_SELECT]));
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
   { "Auto",   1.0f },
   { "Custom", 0.0f }
};

char rotation_lut[ASPECT_RATIO_END][PATH_MAX] =
{
	"Normal",
    "Vertical",
	"Flipped",
    "Flipped Rotated"
};

void rarch_set_auto_viewport(unsigned width, unsigned height)
{
   if(width == 0 || height == 0)
      return;

   unsigned aspect_x, aspect_y, len, highest, i;

   len = width < height ? width : height;
   highest = 1;
   for (i = 1; i < len; i++)
   {
      if ((width % i) == 0 && (height % i) == 0)
         highest = i;
   }

   aspect_x = width / highest;
   aspect_y = height / highest;

   snprintf(aspectratio_lut[ASPECT_RATIO_AUTO].name, sizeof(aspectratio_lut[ASPECT_RATIO_AUTO].name), "%d:%d (Auto)", aspect_x, aspect_y);
   aspectratio_lut[ASPECT_RATIO_AUTO].value = (int)aspect_x / (int)aspect_y;
}

/*============================================================
  RetroArch MAIN WRAP
  ============================================================ */

#ifdef HAVE_RARCH_MAIN_WRAP

void rarch_startup (const char * config_path)
{
   if(g_console.initialize_rarch_enable)
   {
      if(g_console.emulator_initialized)
         rarch_main_deinit();

      struct rarch_main_wrap args = {0};

      args.verbose = g_extern.verbose;
      args.config_path = config_path;
      args.sram_path = g_console.default_sram_dir_enable ? g_console.default_sram_dir : NULL,
      args.state_path = g_console.default_savestate_dir_enable ? g_console.default_savestate_dir : NULL,
      args.rom_path = g_console.rom_path;

      int init_ret = rarch_main_init_wrap(&args);
      (void)init_ret;
      g_console.emulator_initialized = 1;
      g_console.initialize_rarch_enable = 0;
   }
}

int rarch_main_init_wrap(const struct rarch_main_wrap *args)
{
   int argc = 0;
   char *argv[MAX_ARGS] = {NULL};

   argv[argc++] = strdup("retroarch");
   
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
   RARCH_LOG("foo\n");
   for(int i = 0; i < argc; i++)
      RARCH_LOG("arg #%d: %s\n", i, argv[i]);
   RARCH_LOG("bar\n");
#endif

   int ret = rarch_main_init(argc, argv);

   char **tmp = argv;
   while (*tmp)
   {
      free(*tmp);
      tmp++;
   }


   return ret;
}

#endif

#ifdef HAVE_RARCH_EXEC

#ifdef __CELLOS_LV2__
#include <cell/sysmodule.h>
#include <sys/process.h>
#include <sysutil/sysutil_common.h>
#include <netex/net.h>
#include <np.h>
#include <np/drm.h>
#endif

void rarch_exec (void)
{
   if(g_console.return_to_launcher)
   {
      RARCH_LOG("Attempt to load executable: [%s].\n", g_console.launch_app_on_exit);
#if defined(_XBOX)
      XLaunchNewImage(g_console.launch_app_on_exit, NULL);
#elif defined(__CELLOS_LV2__)
      char spawn_data[256];
      for(unsigned int i = 0; i < sizeof(spawn_data); ++i)
         spawn_data[i] = i & 0xff;

      char spawn_data_size[16];
      snprintf(spawn_data_size, sizeof(spawn_data_size), "%d", 256);

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
         RARCH_WARN("SELF file is not of NPDRM type, trying another approach to boot it...\n");
	 sys_game_process_exitspawn(g_console.launch_app_on_exit, NULL, NULL, NULL, 0, 1000, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
      }
      sceNpTerm();
      cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_NP);
      cellSysmoduleUnloadModule(CELL_SYSMODULE_NET);
#endif
   }
}

#endif

#ifdef HAVE_RSOUND
bool rarch_console_rsound_start(const char *ip)
{
   strlcpy(g_settings.audio.driver, "rsound", sizeof(g_settings.audio.driver));
   strlcpy(g_settings.audio.device, ip, sizeof(g_settings.audio.device));
   driver.audio_data = NULL;

   // If driver already has started, it must be reinited.
   if (driver.audio_data)
   {
      uninit_audio();
      driver.audio_data = NULL;
      init_drivers_pre();
      init_audio();
   }
   return g_extern.audio_active;
}

void rarch_console_rsound_stop(void)
{
   strlcpy(g_settings.audio.driver, config_get_default_audio(), sizeof(g_settings.audio.driver));

   // If driver already has started, it must be reinited.
   if (driver.audio_data)
   {
      uninit_audio();
      driver.audio_data = NULL;
      init_drivers_pre();
      init_audio();
   }
}
#endif

/*============================================================
  STRING HANDLING
  ============================================================ */

#ifdef _XBOX
void rarch_convert_char_to_wchar(wchar_t *buf, const char * str, size_t size)
{
   unsigned long dwNum = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
   size /= sizeof(wchar_t);
   rarch_assert(size >= dwNum);
   MultiByteToWideChar(CP_ACP, 0, str, -1, buf, dwNum);
}
#endif

const char * rarch_convert_wchar_to_const_char(const wchar_t * wstr)
{
   static char str[256];
   wcstombs(str, wstr, sizeof(str));
   return str;
}

/*============================================================
  CONFIG
  ============================================================ */

#ifdef HAVE_CONFIGFILE
void rarch_config_create_default(const char * conf_name)
{
   FILE * f;
   RARCH_WARN("Config file \"%s\" doesn't exist. Creating...\n", conf_name);
   f = fopen(conf_name, "w");
   fclose(f);
}

void rarch_config_load(const char * conf_name, const char * libretro_dir_path, const char * exe_ext, bool find_libretro_path)
{
   if(!path_file_exists(conf_name))
      rarch_config_create_default(conf_name);
   else
   {
      config_file_t * conf = config_file_new(conf_name);

      // g_settings

#ifdef HAVE_LIBRETRO_MANAGEMENT
      if(find_libretro_path)
      {
         CONFIG_GET_STRING(libretro, "libretro_path");

         if(!strcmp(g_settings.libretro, ""))
         {
            const char *first_file = rarch_manage_libretro_set_first_file(libretro_dir_path, exe_ext);
            if(first_file != NULL)
               strlcpy(g_settings.libretro, first_file, sizeof(g_settings.libretro));
         }
      }
#endif

      CONFIG_GET_STRING(cheat_database, "cheat_database");
      CONFIG_GET_BOOL(rewind_enable, "rewind_enable");
      CONFIG_GET_STRING(video.cg_shader_path, "video_cg_shader");
#ifdef HAVE_FBO
      CONFIG_GET_STRING(video.second_pass_shader, "video_second_pass_shader");
      CONFIG_GET_FLOAT(video.fbo_scale_x, "video_fbo_scale_x");
      CONFIG_GET_FLOAT(video.fbo_scale_y, "video_fbo_scale_y");
      CONFIG_GET_BOOL(video.render_to_texture, "video_render_to_texture");
      CONFIG_GET_BOOL(video.second_pass_smooth, "video_second_pass_smooth");
#endif
#ifdef _XBOX
      CONFIG_GET_BOOL_CONSOLE(gamma_correction_enable, "gamma_correction_enable");
      CONFIG_GET_INT_CONSOLE(color_format, "color_format");
#endif
      CONFIG_GET_BOOL(video.smooth, "video_smooth");
      CONFIG_GET_BOOL(video.vsync, "video_vsync");
      CONFIG_GET_FLOAT(video.aspect_ratio, "video_aspect_ratio");
      CONFIG_GET_STRING(audio.device, "audio_device");

      for (unsigned i = 0; i < 7; i++)
      {
         char cfg[64];
	 snprintf(cfg, sizeof(cfg), "input_dpad_emulation_p%u", i + 1);
	 CONFIG_GET_INT(input.dpad_emulation[i], cfg);
      }

      // g_console

#ifdef HAVE_FBO
      CONFIG_GET_BOOL_CONSOLE(fbo_enabled, "fbo_enabled");
#endif
#ifdef __CELLOS_LV2__
      CONFIG_GET_BOOL_CONSOLE(custom_bgm_enable, "custom_bgm_enable");
#endif
      CONFIG_GET_BOOL_CONSOLE(overscan_enable, "overscan_enable");
      CONFIG_GET_BOOL_CONSOLE(screenshots_enable, "screenshots_enable");
      CONFIG_GET_BOOL_CONSOLE(throttle_enable, "throttle_enable");
      CONFIG_GET_BOOL_CONSOLE(triple_buffering_enable, "triple_buffering_enable");
      CONFIG_GET_BOOL_CONSOLE(info_msg_enable, "info_msg_enable");
      CONFIG_GET_INT_CONSOLE(aspect_ratio_index, "aspect_ratio_index");
      CONFIG_GET_INT_CONSOLE(current_resolution_id, "current_resolution_id");
      CONFIG_GET_INT_CONSOLE(viewports.custom_vp.x, "custom_viewport_x");
      CONFIG_GET_INT_CONSOLE(viewports.custom_vp.y, "custom_viewport_y");
      CONFIG_GET_INT_CONSOLE(viewports.custom_vp.width, "custom_viewport_width");
      CONFIG_GET_INT_CONSOLE(viewports.custom_vp.height, "custom_viewport_height");
      CONFIG_GET_INT_CONSOLE(screen_orientation, "screen_orientation");
      CONFIG_GET_INT_CONSOLE(sound_mode, "sound_mode");
      CONFIG_GET_STRING_CONSOLE(default_rom_startup_dir, "default_rom_startup_dir");
      CONFIG_GET_FLOAT_CONSOLE(menu_font_size, "menu_font_size");
      CONFIG_GET_FLOAT_CONSOLE(overscan_amount, "overscan_amount");

      // g_extern
      CONFIG_GET_INT_EXTERN(state_slot, "state_slot");
      CONFIG_GET_INT_EXTERN(audio_data.mute, "audio_mute");
   }
}

void rarch_config_save(const char * conf_name)
{
   if(!path_file_exists(conf_name))
      rarch_config_create_default(conf_name);
   else
   {
      config_file_t * conf = config_file_new(conf_name);

      if(conf == NULL)
         conf = config_file_new(NULL);

      // g_settings
      config_set_string(conf, "libretro_path", g_settings.libretro);
#ifdef HAVE_XML
      config_set_string(conf, "cheat_database_path", g_settings.cheat_database);
#endif
      config_set_bool(conf, "rewind_enable", g_settings.rewind_enable);
      config_set_string(conf, "video_cg_shader", g_settings.video.cg_shader_path);
      config_set_float(conf, "video_aspect_ratio", g_settings.video.aspect_ratio);
#ifdef HAVE_FBO
      config_set_float(conf, "video_fbo_scale_x", g_settings.video.fbo_scale_x);
      config_set_float(conf, "video_fbo_scale_y", g_settings.video.fbo_scale_y);
      config_set_string(conf, "video_second_pass_shader", g_settings.video.second_pass_shader);
      config_set_bool(conf, "video_render_to_texture", g_settings.video.render_to_texture);
      config_set_bool(conf, "video_second_pass_smooth", g_settings.video.second_pass_smooth);
#endif
      config_set_bool(conf, "video_smooth", g_settings.video.smooth);
      config_set_bool(conf, "video_vsync", g_settings.video.vsync);
      config_set_string(conf, "audio_device", g_settings.audio.device);

      for (unsigned i = 0; i < 7; i++)
      {
         char cfg[64];
	 snprintf(cfg, sizeof(cfg), "input_dpad_emulation_p%u", i + 1);
	 config_set_int(conf, cfg, g_settings.input.dpad_emulation[i]);
      }

#ifdef RARCH_CONSOLE
      config_set_bool(conf, "fbo_enabled", g_console.fbo_enabled);
#ifdef __CELLOS_LV2__
      config_set_bool(conf, "custom_bgm_enable", g_console.custom_bgm_enable);
#endif
      config_set_bool(conf, "overscan_enable", g_console.overscan_enable);
      config_set_bool(conf, "screenshots_enable", g_console.screenshots_enable);
#ifdef _XBOX
      config_set_bool(conf, "gamma_correction_enable", g_console.gamma_correction_enable);
      config_set_int(conf, "color_format", g_console.color_format);
#endif
      config_set_bool(conf, "throttle_enable", g_console.throttle_enable);
      config_set_bool(conf, "triple_buffering_enable", g_console.triple_buffering_enable);
      config_set_bool(conf, "info_msg_enable", g_console.info_msg_enable);
      config_set_int(conf, "sound_mode", g_console.sound_mode);
      config_set_int(conf, "aspect_ratio_index", g_console.aspect_ratio_index);
      config_set_int(conf, "current_resolution_id", g_console.current_resolution_id);
      config_set_int(conf, "custom_viewport_width", g_console.viewports.custom_vp.width);
      config_set_int(conf, "custom_viewport_height", g_console.viewports.custom_vp.height);
      config_set_int(conf, "custom_viewport_x", g_console.viewports.custom_vp.x);
      config_set_int(conf, "custom_viewport_y", g_console.viewports.custom_vp.y);
      config_set_int(conf, "screen_orientation", g_console.screen_orientation);
      config_set_string(conf, "default_rom_startup_dir", g_console.default_rom_startup_dir);
      config_set_float(conf, "menu_font_size", g_console.menu_font_size);
      config_set_float(conf, "overscan_amount", g_console.overscan_amount);
#endif

      // g_extern
      config_set_int(conf, "state_slot", g_extern.state_slot);
      config_set_int(conf, "audio_mute", g_extern.audio_data.mute);

      if (!config_file_write(conf, conf_name))
         RARCH_ERR("Failed to write config file to \"%s\". Check permissions.\n", conf_name);

      free(conf);
   }
}
#endif
