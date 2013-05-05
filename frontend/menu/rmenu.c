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

#include "rmenu.h"
#include "utils/file_browser.h"

#if defined(__CELLOS_LV2__)
#include <sdk_version.h>

#if (CELL_SDK_VERSION > 0x340000)
#include <sysutil/sysutil_bgmplayback.h>
#endif
#endif

#include "../../console/rarch_console.h"

#include "../../gfx/image.h"

#include "../../gfx/gfx_common.h"
#include "../../gfx/gfx_context.h"

#include "../../file.h"
#include "../../driver.h"
#include "../../general.h"

#ifdef HAVE_SHADER_MANAGER
#include "../../gfx/shader_parse.h"

#define EXT_SHADERS "cg"
#define EXT_CGP_PRESETS "cgp"
#endif

#ifdef _XBOX1
#define JPEG_FORMATS ""
#else
#define JPEG_FORMATS "|jpg|JPG|JPEG|jpeg"
#endif

#define EXT_IMAGES "png|PNG"JPEG_FORMATS
#define EXT_INPUT_PRESETS "cfg|CFG"

struct texture_image *menu_texture;
#ifdef HAVE_MENU_PANEL
struct texture_image *menu_panel;
#endif

static const struct retro_keybind _rmenu_nav_binds[] = {
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) | (1ULL << RARCH_ANALOG_LEFT_Y_DPAD_UP), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN) | (1ULL << RARCH_ANALOG_LEFT_Y_DPAD_DOWN), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) | (1ULL << RARCH_ANALOG_LEFT_X_DPAD_LEFT), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) | (1ULL << RARCH_ANALOG_LEFT_X_DPAD_RIGHT), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RARCH_ANALOG_LEFT_Y_DPAD_UP), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RARCH_ANALOG_LEFT_Y_DPAD_DOWN), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RARCH_ANALOG_LEFT_X_DPAD_LEFT), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RARCH_ANALOG_LEFT_X_DPAD_RIGHT), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RARCH_ANALOG_RIGHT_Y_DPAD_UP), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RARCH_ANALOG_RIGHT_Y_DPAD_DOWN), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RARCH_ANALOG_RIGHT_X_DPAD_LEFT), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RARCH_ANALOG_RIGHT_X_DPAD_RIGHT), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_B), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_A), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_X), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_Y), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_START), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_L), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_R), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_L2), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_R2), 0 },
};

static const struct retro_keybind *rmenu_nav_binds[] = {
   _rmenu_nav_binds
};

const char drive_mappings[][32] = {
#if defined(_XBOX1)
   "C:",
   "D:",
   "E:",
   "F:",
   "G:"
#elif defined(__CELLOS_LV2__)
   "/app_home/",
   "/dev_hdd0/",
   "/dev_hdd1/",
   "/host_root/"
#endif
};

#if defined__CELLOS_LV2__
size_t drive_mapping_idx = 1;
#elif defined(_XBOX1)
size_t drive_mapping_idx = 2;
#else
size_t drive_mapping_idx = 0;
#endif

static const char *menu_drive_mapping_previous(void)
{
   if (drive_mapping_idx > 0)
      drive_mapping_idx--;
   return drive_mappings[drive_mapping_idx];
}

static const char *menu_drive_mapping_next(void)
{
   size_t arr_size = sizeof(drive_mappings) / sizeof(drive_mappings[0]);

   if ((drive_mapping_idx + 1) < arr_size)
      drive_mapping_idx++;

   return drive_mappings[drive_mapping_idx];
}

#if defined(_XBOX1)
#define HARDCODE_FONT_SIZE 21
#define FONT_SIZE_VARIABLE FONT_SIZE

#define POSITION_X 60
#define POSITION_X_CENTER (POSITION_X + 350)
#define POSITION_Y_START 80
#define Y_POSITION 430
#define POSITION_Y_BEGIN (POSITION_Y_START + POSITION_Y_INCREMENT)
#define POSITION_Y_INCREMENT 20
#define COMMENT_POSITION_Y (Y_POSITION - ((POSITION_Y_INCREMENT/2) * 3))
#define COMMENT_TWO_POSITION_Y (Y_POSITION - ((POSITION_Y_INCREMENT/2) * 1))
#define CORE_MSG_POSITION_X FONT_SIZE
#define CORE_MSG_POSITION_Y (MSG_PREV_NEXT_Y_POSITION + 0.01f)
#define CORE_MSG_FONT_SIZE FONT_SIZE
#define MSG_QUEUE_X_POSITION POSITION_X
#define MSG_QUEUE_Y_POSITION (Y_POSITION - ((POSITION_Y_INCREMENT/2) * 7) + 5)
#define MSG_QUEUE_FONT_SIZE HARDCODE_FONT_SIZE
#define MSG_PREV_NEXT_Y_POSITION 24
#define CURRENT_PATH_Y_POSITION (POSITION_Y_START - ((POSITION_Y_INCREMENT/2)))
#define CURRENT_PATH_FONT_SIZE 21

#define FONT_SIZE 21 

#define NUM_ENTRY_PER_PAGE 12
#elif defined(__CELLOS_LV2__)
#define HARDCODE_FONT_SIZE 0.91f
#define FONT_SIZE_VARIABLE g_settings.video.font_size
#define POSITION_X 0.09f
#define POSITION_X_CENTER 0.5f
#define POSITION_Y_START 0.17f
#define POSITION_Y_INCREMENT 0.035f
#define POSITION_Y_BEGIN (POSITION_Y_START + POSITION_Y_INCREMENT)
#define COMMENT_TWO_POSITION_Y 0.91f
#define COMMENT_POSITION_Y 0.82f
#define CORE_MSG_POSITION_X 0.3f
#define CORE_MSG_POSITION_Y 0.06f
#define CORE_MSG_FONT_SIZE COMMENT_POSITION_Y

#define MSG_QUEUE_X_POSITION g_settings.video.msg_pos_x
#define MSG_QUEUE_Y_POSITION 0.76f
#define MSG_QUEUE_FONT_SIZE 1.03f

#define MSG_PREV_NEXT_Y_POSITION 0.03f
#define CURRENT_PATH_Y_POSITION 0.15f
#define CURRENT_PATH_FONT_SIZE (g_settings.video.font_size)

#define NUM_ENTRY_PER_PAGE 15
#endif

#ifdef HAVE_SHADER_MANAGER
static void shader_manager_get_str_filter(char *type_str,
      size_t sizeof_type_str, unsigned pass)
{
   switch (rgui->shader.pass[pass].filter)
   {
      case RARCH_FILTER_LINEAR:
         strlcpy(type_str, "Linear", sizeof_type_str);
         break;

      case RARCH_FILTER_NEAREST:
         strlcpy(type_str, "Nearest", sizeof_type_str);
         break;

      case RARCH_FILTER_UNSPEC:
         strlcpy(type_str, "Don't care", sizeof_type_str);
         break;
   }
}
#endif

/*============================================================
  MENU STACK
  ============================================================ */

static unsigned char menu_stack_enum_array[10];
static unsigned stack_idx = 0;
static uint8_t selected = 0;
static unsigned shader_choice_set_shader_slot = 0;
static unsigned setting_page_number = 0;

static void menu_stack_pop(unsigned menu_type)
{
   switch(menu_type)
   {
      case GENERAL_VIDEO_MENU:
         selected = FIRST_INGAME_MENU_SETTING;
         break;
      case GENERAL_AUDIO_MENU:
         selected = FIRST_VIDEO_SETTING;
         break;
      case EMU_GENERAL_MENU:
         selected = FIRST_AUDIO_SETTING;
         break;
      case EMU_VIDEO_MENU:
         selected = FIRST_EMU_SETTING;
         break;
      case EMU_AUDIO_MENU:
         selected = FIRST_EMU_VIDEO_SETTING;
         break;
      case LIBRETRO_CHOICE:
      case INGAME_MENU_CORE_OPTIONS:
      case INGAME_MENU_LOAD_GAME_HISTORY:
      case INGAME_MENU_RESIZE:
      case INGAME_MENU_SCREENSHOT:
         rgui->frame_buf_show = true;
         break;
      case INGAME_MENU_VIDEO_OPTIONS:
      case INGAME_MENU_INPUT_OPTIONS:
      case INGAME_MENU_PATH_OPTIONS:
         selected = FIRST_INGAME_MENU_SETTING;
         rgui->frame_buf_show = true;
         break;
#ifdef HAVE_SHADER_MANAGER
      case CGP_CHOICE:
         selected = FIRST_SHADERMAN_SETTING;
         break;
#endif
      default:
         break;
   }

   setting_page_number = 0;

   if (rgui->browser->prev_dir.directory_path[0] != '\0')
   {
      memcpy(&rgui->browser->current_dir, &rgui->browser->prev_dir, sizeof(*(&rgui->browser->current_dir)));
      filebrowser_iterate(rgui->browser, FILEBROWSER_ACTION_RESET_CURRENT_DIR);
      rgui->browser->current_dir.ptr = rgui->browser->prev_dir.ptr;
      strlcpy(rgui->browser->current_dir.path, rgui->browser->prev_dir.path,
            sizeof(rgui->browser->current_dir.path));
      memset(&rgui->browser->prev_dir, 0, sizeof(*(&rgui->browser->prev_dir)));
   }

   if (stack_idx > 1)
      stack_idx--;
}

static void menu_stack_push(unsigned menu_type, bool prev_dir)
{
   switch (menu_type)
   {
      case INGAME_MENU:
         selected = FIRST_INGAME_MENU_SETTING;
         break;
      case INGAME_MENU_VIDEO_OPTIONS:
         selected = FIRST_SHADERMAN_SETTING;
         break;
      case INGAME_MENU_INPUT_OPTIONS:
         selected = FIRST_CONTROLS_SETTING_PAGE_1;
         break;
      case INGAME_MENU_PATH_OPTIONS:
         selected = FIRST_PATH_SETTING;
         break;
      case GENERAL_VIDEO_MENU:
         selected = FIRST_VIDEO_SETTING;
         break;
      case GENERAL_AUDIO_MENU:
         selected = FIRST_AUDIO_SETTING;
         break;
      case EMU_GENERAL_MENU:
         selected = FIRST_EMU_SETTING;
         break;
      case EMU_VIDEO_MENU:
         selected = FIRST_EMU_VIDEO_SETTING;
         break;
      case EMU_AUDIO_MENU:
         selected = FIRST_EMU_AUDIO_SETTING;
         break;
      default:
         break;
   }

   setting_page_number = 0;

   if (prev_dir)
   {
      memcpy(&rgui->browser->prev_dir, &rgui->browser->current_dir, sizeof(*(&rgui->browser->prev_dir)));
      rgui->browser->prev_dir.ptr = rgui->browser->current_dir.ptr;
   }

   menu_stack_enum_array[stack_idx] = menu_type;
   stack_idx++;
}

/*============================================================
  EVENT CALLBACKS (AND RELATED)
  ============================================================ */

static void display_menubar(uint8_t menu_type)
{
   char title[32];
   char msg[128];
   font_params_t font_parms = {0};

   font_parms.x = POSITION_X; 
   font_parms.y = CURRENT_PATH_Y_POSITION;
   font_parms.scale = CURRENT_PATH_FONT_SIZE;
   font_parms.color = WHITE;

   switch(menu_type)
   {
#ifdef HAVE_SHADER_MANAGER
      case SHADER_CHOICE:
         strlcpy(title, "Shaders", sizeof(title));
         break;
      case CGP_CHOICE:
         strlcpy(title, "CGP", sizeof(title));
         break;
#endif
      case BORDER_CHOICE:
         strlcpy(title, "Borders", sizeof(title));
         break;
      case LIBRETRO_CHOICE:
         strlcpy(title, "Libretro", sizeof(title));
         break;
      case INPUT_PRESET_CHOICE:
         strlcpy(title, "Input", sizeof(title));
         break;
      case PATH_SAVESTATES_DIR_CHOICE:
      case PATH_DEFAULT_ROM_DIR_CHOICE:
#ifdef HAVE_XML
      case PATH_CHEATS_DIR_CHOICE:
#endif
      case PATH_SRAM_DIR_CHOICE:
      case PATH_SYSTEM_DIR_CHOICE:
         strlcpy(title, "Path", sizeof(title));
         break;
      case INGAME_MENU:
         strlcpy(title, "Menu", sizeof(title));
         break;
      case INGAME_MENU_CORE_OPTIONS:
         strlcpy(title, "Core Options", sizeof(title));
         break;
      case INGAME_MENU_LOAD_GAME_HISTORY:
         strlcpy(title, "History", sizeof(title));
         break;
      case INGAME_MENU_VIDEO_OPTIONS:
      case INGAME_MENU_VIDEO_OPTIONS_MODE:
         strlcpy(title, "Video Options", sizeof(title));
         break;
      case INGAME_MENU_INPUT_OPTIONS:
      case INGAME_MENU_INPUT_OPTIONS_MODE:
         strlcpy(title, "Input Options", sizeof(title));
         break;
      case INGAME_MENU_RESIZE:
         strlcpy(title, "Resize Menu", sizeof(title));
         break;
      case INGAME_MENU_SCREENSHOT:
         strlcpy(title, "Menu", sizeof(title));
         break;
      case FILE_BROWSER_MENU:
         strlcpy(title, "Filebrowser", sizeof(title));
         break;
      case GENERAL_VIDEO_MENU:
         strlcpy(title, "Video", sizeof(title));
         break;
      case GENERAL_AUDIO_MENU:
         strlcpy(title, "Audio", sizeof(title));
         break;
      case EMU_GENERAL_MENU:
         strlcpy(title, "Retro", sizeof(title));
         break;
      case EMU_VIDEO_MENU:
         strlcpy(title, "Retro Video", sizeof(title));
         break;
      case EMU_AUDIO_MENU:
         strlcpy(title, "Retro Audio", sizeof(title));
         break;
      case INGAME_MENU_PATH_OPTIONS:
      case INGAME_MENU_PATH_OPTIONS_MODE:
         strlcpy(title, "Path Options", sizeof(title));
         break;
   }

   switch(menu_type)
   {
#ifdef HAVE_SHADER_MANAGER
      case SHADER_CHOICE:
      case CGP_CHOICE:
#endif
      case BORDER_CHOICE:
      case LIBRETRO_CHOICE:
      case INPUT_PRESET_CHOICE:
      case PATH_SAVESTATES_DIR_CHOICE:
      case PATH_DEFAULT_ROM_DIR_CHOICE:
#ifdef HAVE_XML
      case PATH_CHEATS_DIR_CHOICE:
#endif
      case PATH_SRAM_DIR_CHOICE:
      case PATH_SYSTEM_DIR_CHOICE:
      case FILE_BROWSER_MENU:
         snprintf(msg, sizeof(msg), "PATH: %s", rgui->browser->current_dir.directory_path);

         if (driver.video_poke->set_osd_msg)
            driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);
         break;
   }
   
   font_parms.x = CORE_MSG_POSITION_X;
   font_parms.y = CORE_MSG_POSITION_Y;
   font_parms.scale = CORE_MSG_FONT_SIZE;
   font_parms.color = WHITE;

   if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, rgui->info.library_name, &font_parms);
#ifdef __CELLOS_LV2__

   font_parms.x = POSITION_X; 
   font_parms.y = 0.05f;
   font_parms.scale = 1.4f;
   font_parms.color = WHITE;

   if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, title, &font_parms);

   font_parms.x = 0.80f;
   font_parms.y = 0.015f;
   font_parms.scale = 0.82f;
   font_parms.color = WHITE;
   snprintf(msg, sizeof(msg), "v%s", PACKAGE_VERSION);

   if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);
#endif
}

static void browser_render(void *data)
{
   unsigned file_count = rgui->browser->list->size;
   unsigned current_index = 0;
   unsigned page_number = 0;
   unsigned page_base = 0;
   unsigned i;
   float y_increment = POSITION_Y_START;
   font_params_t font_parms = {0};

   current_index = rgui->browser->current_dir.ptr;
   page_number = current_index / NUM_ENTRY_PER_PAGE;
   page_base = page_number * NUM_ENTRY_PER_PAGE;

   font_parms.scale = FONT_SIZE_VARIABLE;

   for (i = page_base; i < file_count && i < page_base + NUM_ENTRY_PER_PAGE; ++i)
   {
      char fname_tmp[128];
      fill_pathname_base(fname_tmp, rgui->browser->list->elems[i].data, sizeof(fname_tmp));
      y_increment += POSITION_Y_INCREMENT;

#ifdef HAVE_MENU_PANEL
      //check if this is the currently selected file
      if (strcmp(rgui->browser->current_dir.path, rgui->browser->list->elems[i].data) == 0)
         menu_panel->y = y_increment;
#endif

      font_parms.x = POSITION_X; 
      font_parms.y = y_increment;
      font_parms.color = i == current_index ? RED : rgui->browser->list->elems[i].attr.b ? GREEN : WHITE;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, fname_tmp, &font_parms);
   }
}


static int select_file(void *data, uint64_t input)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   
   char extensions[128];
   char comment[128];
   char path[PATH_MAX];
   bool ret = true;
   bool pop_menu_stack = false;
   font_params_t font_parms = {0};

   switch(rgui->menu_type)
   {
#ifdef HAVE_SHADER_MANAGER
      case SHADER_CHOICE:
         strlcpy(extensions, EXT_SHADERS, sizeof(extensions));
         strlcpy(comment, "INFO - Select a shader.", sizeof(comment));
         break;
      case CGP_CHOICE:
         strlcpy(extensions, EXT_CGP_PRESETS, sizeof(extensions));
         strlcpy(comment, "INFO - Select a CGP file.", sizeof(comment));
         break;
#endif
      case INPUT_PRESET_CHOICE:
         strlcpy(extensions, EXT_INPUT_PRESETS, sizeof(extensions));
         strlcpy(comment, "INFO - Select an input preset.", sizeof(comment));
         break;
      case BORDER_CHOICE:
         strlcpy(extensions, EXT_IMAGES, sizeof(extensions));
         strlcpy(comment, "INFO - Select a border image file.", sizeof(comment));
         break;
      case LIBRETRO_CHOICE:
         strlcpy(extensions, EXT_EXECUTABLES, sizeof(extensions));
         strlcpy(comment, "INFO - Select a Libretro core.", sizeof(comment));
         break;
   }

   filebrowser_update(rgui->browser, input, extensions);

   if (input & (1ULL << DEVICE_NAV_B))
   {
      if (filebrowser_iterate(rgui->browser, FILEBROWSER_ACTION_PATH_ISDIR))
         ret = filebrowser_iterate(rgui->browser, FILEBROWSER_ACTION_OK);
      else
      {
         strlcpy(path, rgui->browser->current_dir.path, sizeof(path));

         switch(rgui->menu_type)
         {
#ifdef HAVE_SHADER_MANAGER
            case SHADER_CHOICE:
               strlcpy(rgui->shader.pass[shader_choice_set_shader_slot].source.cg, path,
                     sizeof(rgui->shader.pass[shader_choice_set_shader_slot].source.cg));
               break;
            case CGP_CHOICE:
               {
                  config_file_t *conf = NULL;

                  strlcpy(g_settings.video.shader_path, path, sizeof(g_settings.video.shader_path));

                  conf = config_file_new(path);
                  if (conf)
                     gfx_shader_read_conf_cgp(conf, &rgui->shader);
                  config_file_free(conf);

                  if (video_set_shader_func(RARCH_SHADER_CG, path))
                     g_settings.video.shader_enable = true;
                  else
                  {
                     RARCH_ERR("Setting CGP failed.\n");
                     g_settings.video.shader_enable = false;
                  }
               }
               break;
#endif
            case INPUT_PRESET_CHOICE:
               strlcpy(g_extern.file_state.input_cfg_path, path, sizeof(g_extern.file_state.input_cfg_path));
               config_read_keybinds(path);
               break;
            case BORDER_CHOICE:
               if (menu_texture)
               {
#ifdef _XBOX
                  if (menu_texture->vertex_buf)
                  {
                     menu_texture->vertex_buf->Release();
                     menu_texture->vertex_buf = NULL;
                  }
                  if (menu_texture->pixels)
                  {
                     menu_texture->pixels->Release();
                     menu_texture->pixels = NULL;
                  }
#else
                  free(menu_texture->pixels);
                  menu_texture->pixels = NULL;
#endif
                  menu_texture = (struct texture_image*)calloc(1, sizeof(*menu_texture));
               }
               texture_image_load(path, menu_texture);
               strlcpy(g_extern.menu_texture_path, path, sizeof(g_extern.menu_texture_path));

               if (driver.video_poke && driver.video_poke->set_texture_frame)
                  driver.video_poke->set_texture_frame(driver.video_data, menu_texture->pixels,
                        true, menu_texture->width, menu_texture->height, 1.0f);
               break;
         }

         if (rgui->menu_type == LIBRETRO_CHOICE)
         {
            strlcpy(g_settings.libretro, path, sizeof(g_settings.libretro));
            g_extern.lifecycle_mode_state |= (1ULL << MODE_EXIT);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_EXITSPAWN);
            return -1;
         }

         pop_menu_stack = true;
      }

      if (!ret)
         msg_queue_push(g_extern.msg_queue, "INFO - You need to restart RetroArch.", 1, 180);
   }
   else if ((input & (1ULL << DEVICE_NAV_X)) || (input & (1ULL << DEVICE_NAV_MENU)))
      pop_menu_stack = true;

   if (pop_menu_stack)
      menu_stack_pop(rgui->menu_type);

   font_parms.x = POSITION_X; 
   font_parms.y = COMMENT_POSITION_Y;
   font_parms.scale = HARDCODE_FONT_SIZE;
   font_parms.color = WHITE;

   if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, comment, &font_parms);

   display_menubar(rgui->menu_type);
   browser_render(rgui->browser);

   return 0;
}

static int select_directory(void *data, uint64_t input)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   font_params_t font_parms = {0};

   char path[PATH_MAX];
   char msg[256];
   bool ret = true;

   bool is_dir = filebrowser_iterate(rgui->browser, FILEBROWSER_ACTION_PATH_ISDIR);
   bool pop_menu_stack = false;

   filebrowser_update(rgui->browser, input, "empty");

   if (input & (1ULL << DEVICE_NAV_Y))
   {
      if (is_dir)
      {
         strlcpy(path, rgui->browser->current_dir.path, sizeof(path));

         switch(rgui->menu_type)
         {
            case PATH_SAVESTATES_DIR_CHOICE:
               strlcpy(g_extern.savestate_dir, path, sizeof(g_extern.savestate_dir));
               break;
            case PATH_SRAM_DIR_CHOICE:
               strlcpy(g_extern.savefile_dir, path, sizeof(g_extern.savefile_dir));
               break;
            case PATH_DEFAULT_ROM_DIR_CHOICE:
               strlcpy(g_settings.rgui_browser_directory, path, sizeof(g_settings.rgui_browser_directory));
               break;
#ifdef HAVE_XML
            case PATH_CHEATS_DIR_CHOICE:
               strlcpy(g_settings.cheat_database, path, sizeof(g_settings.cheat_database));
               break;
#endif
            case PATH_SYSTEM_DIR_CHOICE:
               strlcpy(g_settings.system_directory, path, sizeof(g_settings.system_directory));
               break;
         }
         pop_menu_stack = true;
      }
   }
   else if (input & (1ULL << DEVICE_NAV_X))
   {
      strlcpy(path, default_paths.port_dir, sizeof(path));
      switch(rgui->menu_type)
      {
         case PATH_SAVESTATES_DIR_CHOICE:
            strlcpy(g_extern.savestate_dir, path, sizeof(g_extern.savestate_dir));
            break;
         case PATH_SRAM_DIR_CHOICE:
            strlcpy(g_extern.savefile_dir, path, sizeof(g_extern.savefile_dir));
            break;
         case PATH_DEFAULT_ROM_DIR_CHOICE:
            strlcpy(g_settings.rgui_browser_directory, path, sizeof(g_settings.rgui_browser_directory));
            break;
#ifdef HAVE_XML
         case PATH_CHEATS_DIR_CHOICE:
            strlcpy(g_settings.cheat_database, path, sizeof(g_settings.cheat_database));
            break;
#endif
         case PATH_SYSTEM_DIR_CHOICE:
            strlcpy(g_settings.system_directory, path, sizeof(g_settings.system_directory));
            break;
      }
      pop_menu_stack = true;
   }
   else if (input & (1ULL << DEVICE_NAV_B))
   {
      if (is_dir)
         ret = filebrowser_iterate(rgui->browser, FILEBROWSER_ACTION_OK);
   }
   else if ((input & (1ULL << DEVICE_NAV_MENU)))
      pop_menu_stack = true;

   if (pop_menu_stack)
      menu_stack_pop(rgui->menu_type);

   if (!ret)
      msg_queue_push(g_extern.msg_queue, "ERROR - Failed to open directory.", 1, 180);

   struct platform_bind key_label_y = {0};

   strlcpy(key_label_y.desc, "Unknown", sizeof(key_label_y.desc));
   key_label_y.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_Y;

   if (driver.input->set_keybinds)
      driver.input->set_keybinds(&key_label_y, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));

   font_parms.x = POSITION_X; 
   font_parms.y = COMMENT_POSITION_Y;
   font_parms.scale = HARDCODE_FONT_SIZE;
   font_parms.color = WHITE;

   snprintf(msg, sizeof(msg), "INFO - Select a dir as path by pressing\n[%s].", key_label_y.desc);

   if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

   display_menubar(rgui->menu_type);
   browser_render(rgui->browser);

   return 0;
}

static void set_keybind_digital(unsigned default_retro_joypad_id, uint64_t input)
{
   if (!driver.input->set_keybinds)
      return;

   unsigned keybind_action = KEYBINDS_ACTION_NONE;

   if (input & (1ULL << DEVICE_NAV_LEFT))
      keybind_action = (1ULL << KEYBINDS_ACTION_DECREMENT_BIND);

   if ((input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
      keybind_action = (1ULL << KEYBINDS_ACTION_INCREMENT_BIND);

   if (input & (1ULL << DEVICE_NAV_START))
      keybind_action = (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BIND);


   if (keybind_action)
      driver.input->set_keybinds(driver.input_data, NULL, rgui->current_pad,
            default_retro_joypad_id, keybind_action);
}

#if defined(HAVE_OSKUTIL)
#ifdef __CELLOS_LV2__

static bool osk_callback_enter_rsound(void *data)
{
   if (g_extern.lifecycle_mode_state & (1ULL << MODE_OSK_ENTRY_SUCCESS))
   {
      RARCH_LOG("OSK - Applying input data.\n");
      char tmp_str[256];
      int num = wcstombs(tmp_str, g_extern.console.misc.oskutil_handle.text_buf, sizeof(tmp_str));
      tmp_str[num] = 0;
      strlcpy(g_settings.audio.device, tmp_str, sizeof(g_settings.audio.device));
      goto do_exit;
   }
   else if (g_extern.lifecycle_mode_state & (1ULL << MODE_OSK_ENTRY_FAIL))
      goto do_exit;

   return false;

do_exit:
   g_extern.lifecycle_mode_state &= ~((1ULL << MODE_OSK_DRAW) | (1ULL << MODE_OSK_ENTRY_SUCCESS) |
         (1ULL << MODE_OSK_ENTRY_FAIL));
   return true;
}

static bool osk_callback_enter_rsound_init(void *data)
{
   oskutil_write_initial_message(&g_extern.console.misc.oskutil_handle, L"192.168.1.1");
   oskutil_write_message(&g_extern.console.misc.oskutil_handle, L"Enter IP address for the RSound Server.");
   oskutil_start(&g_extern.console.misc.oskutil_handle);

   return true;
}

static bool osk_callback_enter_filename(void *data)
{
   if (g_extern.lifecycle_mode_state & (1ULL << MODE_OSK_ENTRY_SUCCESS))
   {
      RARCH_LOG("OSK - Applying input data.\n");
      char tmp_str[256];
      char filepath[PATH_MAX];
      int num = wcstombs(tmp_str, g_extern.console.misc.oskutil_handle.text_buf, sizeof(tmp_str));
      tmp_str[num] = 0;

      switch(rgui->osk_param)
      {
         case CONFIG_FILE:
            break;
         case SHADER_PRESET_FILE:
            snprintf(filepath, sizeof(filepath), "%s/%s.cgp", g_settings.video.shader_dir, tmp_str);
            RARCH_LOG("[osk_callback_enter_filename]: filepath is: %s.\n", filepath);
            /* TODO - stub */
            break;
         case INPUT_PRESET_FILE:
            snprintf(filepath, sizeof(filepath), "%s/%s.cfg", default_paths.input_presets_dir, tmp_str);
            RARCH_LOG("[osk_callback_enter_filename]: filepath is: %s.\n", filepath);
            config_save_keybinds(filepath);
            break;
      }

      goto do_exit;
   }
   else if (g_extern.lifecycle_mode_state & (1ULL << MODE_OSK_ENTRY_FAIL))
      goto do_exit;

   return false;
do_exit:
   g_extern.lifecycle_mode_state &= ~((1ULL << MODE_OSK_DRAW) | (1ULL << MODE_OSK_ENTRY_SUCCESS) |
         (1ULL << MODE_OSK_ENTRY_FAIL));
   return true;
}

static bool osk_callback_enter_filename_init(void *data)
{
   oskutil_write_initial_message(&g_extern.console.misc.oskutil_handle, L"example");
   oskutil_write_message(&g_extern.console.misc.oskutil_handle, L"Enter filename for preset");
   oskutil_start(&g_extern.console.misc.oskutil_handle);

   return true;
}

#endif
#endif

static void rgui_init_textures(void)
{
#ifdef HAVE_MENU_PANEL
   texture_image_load("D:\\Media\\menuMainRomSelectPanel.png", menu_panel);
#endif
   texture_image_load(g_extern.menu_texture_path, menu_texture);

   if (driver.video_poke && driver.video_poke->set_texture_frame)
      driver.video_poke->set_texture_frame(driver.video_data, menu_texture->pixels,
            true, menu_texture->width, menu_texture->height, 1.0f);
}

static int set_setting_action(uint8_t menu_type, unsigned switchvalue, uint64_t input)
{
   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;

   switch (switchvalue)
   {
#ifdef __CELLOS_LV2__
      case SETTING_CHANGE_RESOLUTION:
         if (input & (1ULL << DEVICE_NAV_RIGHT))
            settings_set(1ULL << S_RESOLUTION_NEXT);
         if (input & (1ULL << DEVICE_NAV_LEFT))
            settings_set(1ULL << S_RESOLUTION_PREVIOUS);
         if (input & (1ULL << DEVICE_NAV_B))
         {
            if (g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.current.idx] == CELL_VIDEO_OUT_RESOLUTION_576)
            {
               if (g_extern.console.screen.pal_enable)
                  g_extern.lifecycle_mode_state |= (1ULL<< MODE_VIDEO_PAL_ENABLE);
            }
            else
            {
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_PAL_ENABLE);
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE);
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_PAL_VSYNC_BLOCK);
            }


            driver.video->restart();
            rgui_init_textures();
         }
         break;
      case SETTING_PAL60_MODE:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         {
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_PAL_ENABLE))
            {
               if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE))
               {
                  g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE);
                  g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_PAL_VSYNC_BLOCK);
               }
               else
               {
                  g_extern.lifecycle_mode_state |= (1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE);
                  g_extern.lifecycle_mode_state |= (1ULL << MODE_VIDEO_PAL_VSYNC_BLOCK);
               }

               driver.video->restart();
               rgui_init_textures();
            }
         }

         if (input & (1ULL << DEVICE_NAV_START))
         {
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_PAL_ENABLE))
            {
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE);
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_PAL_VSYNC_BLOCK);

               driver.video->restart();
               rgui_init_textures();
            }
         }
         break;
#endif
      case SETTING_EMU_SKIN:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         {
            menu_stack_push(BORDER_CHOICE, true);
            filebrowser_set_root_and_ext(rgui->browser, EXT_IMAGES, default_paths.border_dir);
         }
         if (input & (1ULL << DEVICE_NAV_START))
         {
            if (!texture_image_load(default_paths.menu_border_file, menu_texture))
            {
               RARCH_ERR("Failed to load texture image for menu.\n");
               return false;
            }
         }
         break;
      case SETTING_FONT_SIZE:
         if (input & (1ULL << DEVICE_NAV_LEFT))
         {
            if (g_settings.video.font_size > 0) 
               g_settings.video.font_size -= 0.01f;
         }
         if ((input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         {
            if ((g_settings.video.font_size < 2.0f))
               g_settings.video.font_size += 0.01f;
         }
         if (input & (1ULL << DEVICE_NAV_START))
            g_settings.video.font_size = 1.0f;
         break;
      case SETTING_ASPECT_RATIO:
         if (input & (1ULL << DEVICE_NAV_LEFT))
         {
            settings_set(1ULL << S_ASPECT_RATIO_DECREMENT);

            if (driver.video_poke->set_aspect_ratio)
               driver.video_poke->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);
         }
         if (input & (1ULL << DEVICE_NAV_RIGHT))
         {
            settings_set(1ULL << S_ASPECT_RATIO_INCREMENT);

            if (driver.video_poke->set_aspect_ratio)
               driver.video_poke->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);
         }
         if (input & (1ULL << DEVICE_NAV_START))
         {
            settings_set(1ULL << S_DEF_ASPECT_RATIO);

            if (driver.video_poke->set_aspect_ratio)
               driver.video_poke->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);
         }
         break;
      case SETTING_HW_TEXTURE_FILTER:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         {
            settings_set(1ULL << S_HW_TEXTURE_FILTER);

            if (driver.video_poke->set_filtering)
               driver.video_poke->set_filtering(driver.video_data, 1, g_settings.video.smooth);
         }
         if (input & (1ULL << DEVICE_NAV_START))
         {
            settings_set(1ULL << S_DEF_HW_TEXTURE_FILTER);

            if (driver.video_poke->set_filtering)
               driver.video_poke->set_filtering(driver.video_data, 1, g_settings.video.smooth);
         }
         break;
#ifdef _XBOX1
      case SETTING_FLICKER_FILTER:
         if (input & (1ULL << DEVICE_NAV_LEFT))
         {
            if (g_extern.console.screen.flicker_filter_index > 0)
               g_extern.console.screen.flicker_filter_index--;
         }
         if (input & (1ULL << DEVICE_NAV_RIGHT))
         {
            if (g_extern.console.screen.flicker_filter_index < 5)
               g_extern.console.screen.flicker_filter_index++;
         }
         if (input & (1ULL << DEVICE_NAV_START))
            g_extern.console.screen.flicker_filter_index = 0;
         break;
      case SETTING_SOFT_DISPLAY_FILTER:
         if (input & (1ULL << DEVICE_NAV_LEFT) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         {
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE))
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);
            else
               g_extern.lifecycle_mode_state |= (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);
         }
         if (input & (1ULL << DEVICE_NAV_START))
            g_extern.lifecycle_mode_state |= (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);
         break;
#endif
      case SETTING_REFRESH_RATE:
         if (input & (1ULL << DEVICE_NAV_LEFT))
         {
            settings_set(1ULL << S_REFRESH_RATE_DECREMENT);
            driver_set_monitor_refresh_rate(g_settings.video.refresh_rate);
         }
         if ((input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         {
            settings_set(1ULL << S_REFRESH_RATE_INCREMENT);
            driver_set_monitor_refresh_rate(g_settings.video.refresh_rate);
         }
         if (input & (1ULL << DEVICE_NAV_START))
         {
            settings_set(1ULL << S_DEF_REFRESH_RATE);
            driver_set_monitor_refresh_rate(g_settings.video.refresh_rate);
         }
         break;
      case SETTING_THROTTLE_MODE:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         {
            if (!(g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_PAL_VSYNC_BLOCK)))
            {
               settings_set(1ULL << S_THROTTLE);
               device_ptr->ctx_driver->swap_interval((g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_THROTTLE_ENABLE)) ? true : false);
            }
         }
         if (input & (1ULL << DEVICE_NAV_START))
         {
            if (!(g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_PAL_VSYNC_BLOCK)))
            {
               settings_set(1ULL << S_DEF_THROTTLE);
               device_ptr->ctx_driver->swap_interval((g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_THROTTLE_ENABLE)) ? true : false);
            }
         }
         break;
      case SETTING_TRIPLE_BUFFERING:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         {
            settings_set(1ULL << S_TRIPLE_BUFFERING);

            driver.video->restart();
            rgui_init_textures();
         }
         if (input & (1ULL << DEVICE_NAV_START))
         {
            settings_set(1ULL << S_DEF_TRIPLE_BUFFERING);

            if (!(g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_TRIPLE_BUFFERING_ENABLE)))
            {
               driver.video->restart();
               rgui_init_textures();
            }
         }
         break;
      case SETTING_DEFAULT_VIDEO_ALL:
         if (input & (1ULL << DEVICE_NAV_START))
         {
         }
         break;
      case SETTING_SOUND_MODE:
         if (input & (1ULL << DEVICE_NAV_LEFT))
         {
            if (g_extern.console.sound.mode != SOUND_MODE_NORMAL)
               g_extern.console.sound.mode--;
         }
         if ((input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         {
            if (g_extern.console.sound.mode < (SOUND_MODE_LAST-1))
               g_extern.console.sound.mode++;
         }
         if ((input & (1ULL << DEVICE_NAV_UP)) || (input & (1ULL << DEVICE_NAV_DOWN)))
         {
#ifdef HAVE_RSOUND
            if (g_extern.console.sound.mode != SOUND_MODE_RSOUND)
               rarch_rsound_stop();
            else
               rarch_rsound_start(g_settings.audio.device);
#endif
         }
         if (input & (1ULL << DEVICE_NAV_START))
         {
            g_extern.console.sound.mode = SOUND_MODE_NORMAL;
#ifdef HAVE_RSOUND
            rarch_rsound_stop();
#endif
         }
         break;
#ifdef HAVE_RSOUND
      case SETTING_RSOUND_SERVER_IP_ADDRESS:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         {
#ifdef HAVE_OSKUTIL
            rgui->osk_init = osk_callback_enter_rsound_init;
            rgui->osk_callback = osk_callback_enter_rsound;
#endif
         }
         if (input & (1ULL << DEVICE_NAV_START))
            strlcpy(g_settings.audio.device, "0.0.0.0", sizeof(g_settings.audio.device));
         break;
#endif
      case SETTING_DEFAULT_AUDIO_ALL:
         break;
      case SETTING_EMU_SHOW_DEBUG_INFO_MSG:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
            settings_set(1ULL << S_INFO_DEBUG_MSG_TOGGLE);
         if (input & (1ULL << DEVICE_NAV_START))
            settings_set(1ULL << S_DEF_INFO_DEBUG_MSG);
         break;
      case SETTING_EMU_SHOW_INFO_MSG:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
            settings_set(1ULL << S_INFO_MSG_TOGGLE);
         if (input & (1ULL << DEVICE_NAV_START))
            settings_set(1ULL << S_DEF_INFO_MSG);
         break;
      case INGAME_MENU_REWIND_ENABLED:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         {
            settings_set(1ULL << S_REWIND);

            if (g_settings.rewind_enable)
               rarch_init_rewind();
            else
               rarch_deinit_rewind();
         }

         if (input & (1ULL << DEVICE_NAV_START))
         {
            if (g_settings.rewind_enable)
            {
               g_settings.rewind_enable = false;
               rarch_deinit_rewind();
            }
         }
         break;
      case INGAME_MENU_REWIND_GRANULARITY:
         if (input & (1ULL << DEVICE_NAV_LEFT))
         {
            if (g_settings.rewind_granularity > 1)
               g_settings.rewind_granularity--;
         }
         if ((input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
            g_settings.rewind_granularity++;
         if (input & (1ULL << DEVICE_NAV_START))
            g_settings.rewind_granularity = 1;
         break;
      case SETTING_EMU_AUDIO_MUTE:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
            settings_set(1ULL << S_AUDIO_MUTE);

         if (input & (1ULL << DEVICE_NAV_START))
            settings_set(1ULL << S_DEF_AUDIO_MUTE);
         break;
#ifdef _XBOX1
      case SETTING_EMU_AUDIO_SOUND_VOLUME_LEVEL:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         {
            g_extern.console.sound.volume_level = !g_extern.console.sound.volume_level;
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
               msg_queue_push(g_extern.msg_queue, "INFO - You need to restart RetroArch.", 1, 180);
         }

         if (input & (1ULL << DEVICE_NAV_START))
         {
            g_extern.console.sound.volume_level = 0;
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
               msg_queue_push(g_extern.msg_queue, "INFO - You need to restart RetroArch.", 1, 180);
         }
         break;
#endif
      case SETTING_ENABLE_CUSTOM_BGM:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         {
#if (CELL_SDK_VERSION > 0x340000)
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE))
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE);
            else
               g_extern.lifecycle_mode_state |= (1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE);
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE))
               cellSysutilEnableBgmPlayback();
            else
               cellSysutilDisableBgmPlayback();

#endif
         }
         if (input & (1ULL << DEVICE_NAV_START))
         {
#if (CELL_SDK_VERSION > 0x340000)
            g_extern.lifecycle_mode_state |= (1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE);
#endif
         }
         break;
      case SETTING_EMU_VIDEO_DEFAULT_ALL:
         break;
      case SETTING_EMU_AUDIO_DEFAULT_ALL:
         break;
      case SETTING_PATH_DEFAULT_ROM_DIRECTORY:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         {
            menu_stack_push(PATH_DEFAULT_ROM_DIR_CHOICE, true);
            filebrowser_set_root_and_ext(rgui->browser, "empty", default_paths.filesystem_root_dir);
         }

         if (input & (1ULL << DEVICE_NAV_START))
            strlcpy(g_settings.rgui_browser_directory,
                  default_paths.filesystem_root_dir, sizeof(g_settings.rgui_browser_directory));
         break;
      case SETTING_PATH_SAVESTATES_DIRECTORY:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         {
            menu_stack_push(PATH_SAVESTATES_DIR_CHOICE, true);
            filebrowser_set_root_and_ext(rgui->browser, "empty", default_paths.filesystem_root_dir);
         }

         if (input & (1ULL << DEVICE_NAV_START))
            strlcpy(g_extern.savestate_dir, default_paths.savestate_dir, sizeof(g_extern.savestate_dir));

         break;
      case SETTING_PATH_SRAM_DIRECTORY:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         {
            menu_stack_push(PATH_SRAM_DIR_CHOICE, true);
            filebrowser_set_root_and_ext(rgui->browser, "empty", default_paths.filesystem_root_dir);
         }

         if (input & (1ULL << DEVICE_NAV_START))
            strlcpy(g_extern.savefile_dir, default_paths.sram_dir, sizeof(g_extern.savefile_dir));
         break;
#ifdef HAVE_XML
      case SETTING_PATH_CHEATS:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         {
            menu_stack_push(PATH_CHEATS_DIR_CHOICE, true);
            filebrowser_set_root_and_ext(rmenu->browser, "empty", default_paths.filesystem_root_dir);
         }

         if (input & (1ULL << DEVICE_NAV_START))
            strlcpy(g_settings.cheat_database, default_paths.port_dir, sizeof(g_settings.cheat_database));
         break;
#endif
      case SETTING_PATH_SYSTEM:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         {
            menu_stack_push(PATH_SYSTEM_DIR_CHOICE, true);
            filebrowser_set_root_and_ext(rgui->browser, "empty", default_paths.system_dir);
         }

         if (input & (1ULL << DEVICE_NAV_START))
            strlcpy(g_settings.system_directory, default_paths.system_dir, sizeof(g_settings.system_directory));
         break;
      case SETTING_ENABLE_SRAM_PATH:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)))
         {
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE))
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE);
            else
               g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE);
         }

         if (input & (1ULL << DEVICE_NAV_START))
            g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE);
         break;
      case SETTING_ENABLE_STATE_PATH:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)))
         {
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE))
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE);
            else
               g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE);
         }
         if (input & (1ULL << DEVICE_NAV_START))
            g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE);
         break;
      case SETTING_PATH_DEFAULT_ALL:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)) || (input & (1ULL << DEVICE_NAV_START)))
         {
            strlcpy(g_settings.rgui_browser_directory, default_paths.filebrowser_startup_dir,
                  sizeof(g_settings.rgui_browser_directory));
            strlcpy(g_extern.savestate_dir, default_paths.port_dir, sizeof(g_extern.savestate_dir));
#ifdef HAVE_XML
            strlcpy(g_settings.cheat_database, default_paths.port_dir, sizeof(g_settings.cheat_database));
#endif
            strlcpy(g_extern.savefile_dir, "", sizeof(g_extern.savefile_dir));
         }
         break;
      case SETTING_CONTROLS_SCHEME:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)) || (input & (1ULL << DEVICE_NAV_START)))
         {
            menu_stack_push(INPUT_PRESET_CHOICE, true);
            filebrowser_set_root_and_ext(rgui->browser, EXT_INPUT_PRESETS, default_paths.input_presets_dir);
         }
         break;
      case SETTING_CONTROLS_NUMBER:
         if (input & (1ULL << DEVICE_NAV_LEFT))
         {
            if (rgui->current_pad != 0)
               rgui->current_pad--;
         }

         if ((input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         {
            if (rgui->current_pad < 6)
               rgui->current_pad++;
         }

         if (input & (1ULL << DEVICE_NAV_START))
            rgui->current_pad = 0;
         break; 
      case SETTING_DPAD_EMULATION:
         if (input & (1ULL << DEVICE_NAV_LEFT))
         {
            if (driver.input->set_keybinds)
            {
               unsigned keybind_action = 0;

               switch(g_settings.input.dpad_emulation[rgui->current_pad])
               {
                  case ANALOG_DPAD_NONE:
                     break;
                  case ANALOG_DPAD_LSTICK:
                     keybind_action = (1ULL << KEYBINDS_ACTION_SET_ANALOG_DPAD_NONE);
                     break;
                  case ANALOG_DPAD_RSTICK:
                     keybind_action = (1ULL << KEYBINDS_ACTION_SET_ANALOG_DPAD_LSTICK);
                     break;
                  default:
                     break;
               }

               if (keybind_action)
                  driver.input->set_keybinds(driver.input_data, g_settings.input.device[rgui->current_pad], rgui->current_pad, 0, keybind_action);
            }
         }

         if ((input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         {
            if (driver.input->set_keybinds)
            {
               unsigned keybind_action = 0;

               switch(g_settings.input.dpad_emulation[rgui->current_pad])
               {
                  case ANALOG_DPAD_NONE:
                     keybind_action = (1ULL << KEYBINDS_ACTION_SET_ANALOG_DPAD_LSTICK);
                     break;
                  case ANALOG_DPAD_LSTICK:
                     keybind_action = (1ULL << KEYBINDS_ACTION_SET_ANALOG_DPAD_RSTICK);
                     break;
                  case ANALOG_DPAD_RSTICK:
                     break;
               }

               if (keybind_action)
                  driver.input->set_keybinds(driver.input_data, g_settings.input.device[rgui->current_pad], rgui->current_pad, 0, keybind_action);
            }
         }

         if (input & (1ULL << DEVICE_NAV_START))
            if (driver.input->set_keybinds)
                  driver.input->set_keybinds(driver.input_data, g_settings.input.device[rgui->current_pad], rgui->current_pad, 0, (1ULL << KEYBINDS_ACTION_SET_ANALOG_DPAD_LSTICK));
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_UP:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_UP, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_DOWN:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_DOWN, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_LEFT:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_LEFT, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_RIGHT:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_RIGHT, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_A:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_A, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_B:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_B, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_X:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_X, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_Y:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_Y, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_SELECT:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_SELECT, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_START:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_START, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_L, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_R, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L2:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_L2, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R2:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_R2, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L3:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_L3, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R3:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_R3, input);
         break;
#ifdef HAVE_OSKUTIL
      case SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)) || (input & (1ULL << DEVICE_NAV_START)))
         {
            rgui->osk_param = INPUT_PRESET_FILE;
            rgui->osk_init = osk_callback_enter_filename_init;
            rgui->osk_callback = osk_callback_enter_filename;
         }
         break;
#endif
      case SETTING_CONTROLS_DEFAULT_ALL:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)) || (input & (1ULL << DEVICE_NAV_START)))
            if (driver.input->set_keybinds)
               driver.input->set_keybinds(driver.input_data, g_settings.input.device[rgui->current_pad], rgui->current_pad, 0,
                     (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS));
         break;
      case INGAME_MENU_LOAD_STATE:
         if (input & (1ULL << DEVICE_NAV_B))
         {
            rarch_load_state();
            g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_INGAME_EXIT);
            return -1;
         }
         if (input & (1ULL << DEVICE_NAV_LEFT))
            rarch_state_slot_decrease();
         if (input & (1ULL << DEVICE_NAV_RIGHT))
            rarch_state_slot_increase();

         break;
      case INGAME_MENU_SAVE_STATE:
         if (input & (1ULL << DEVICE_NAV_B))
         {
            rarch_save_state();
            g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_INGAME_EXIT);
            return -1;
         }

         if (input & (1ULL << DEVICE_NAV_LEFT))
            rarch_state_slot_decrease();
         if (input & (1ULL << DEVICE_NAV_RIGHT))
            rarch_state_slot_increase();

         break;
      case SETTING_ROTATION:
         if (input & (1ULL << DEVICE_NAV_LEFT))
         {
            settings_set(1ULL << S_ROTATION_DECREMENT);
            video_set_rotation_func(g_extern.console.screen.orientation);
         }

         if ((input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         {
            settings_set(1ULL << S_ROTATION_INCREMENT);
            video_set_rotation_func(g_extern.console.screen.orientation);
         }

         if (input & (1ULL << DEVICE_NAV_START))
         {
            settings_set(1ULL << S_DEF_ROTATION);
            video_set_rotation_func(g_extern.console.screen.orientation);
         }
         break;
      case INGAME_MENU_FRAME_ADVANCE:
         if ((input & (1ULL << DEVICE_NAV_B)) || (input & (1ULL << DEVICE_NAV_R2)) || (input & (1ULL << DEVICE_NAV_L2)))
         {
            g_extern.lifecycle_state |= (1ULL << RARCH_FRAMEADVANCE);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_INGAME_EXIT);
            settings_set(1ULL << S_FRAME_ADVANCE);
            return -1;
         }
         break;
      case SETTING_CUSTOM_VIEWPORT:
         if (input & (1ULL << DEVICE_NAV_B))
            menu_stack_push(INGAME_MENU_RESIZE, false);
         break;
      case INGAME_MENU_CORE_OPTIONS_MODE:
         if (input & (1ULL << DEVICE_NAV_B))
            menu_stack_push(INGAME_MENU_CORE_OPTIONS, false);
         break;
      case INGAME_MENU_LOAD_GAME_HISTORY_MODE:
         if (input & (1ULL << DEVICE_NAV_B))
            menu_stack_push(INGAME_MENU_LOAD_GAME_HISTORY, false);
         break;
      case INGAME_MENU_SCREENSHOT_MODE:
         if (input & (1ULL << DEVICE_NAV_B))
            menu_stack_push(INGAME_MENU_SCREENSHOT, false);
         break;
      case INGAME_MENU_RETURN_TO_GAME:
         if (input & (1ULL << DEVICE_NAV_B))
         {
            g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_INGAME_EXIT);
            return -1;
         }
         break;
      case INGAME_MENU_CHANGE_GAME:
         if (input & (1ULL << DEVICE_NAV_B))
            menu_stack_push(FILE_BROWSER_MENU, false);
         break;
      case INGAME_MENU_SETTINGS:
         if (input & (1ULL << DEVICE_NAV_B))
            menu_stack_push(GENERAL_VIDEO_MENU, false);
         break;
      case INGAME_MENU_RESET:
         if (input & (1ULL << DEVICE_NAV_B))
         {
            rarch_game_reset();
            g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_INGAME_EXIT);
            return -1;
         }
         break;
      case INGAME_MENU_CHANGE_LIBRETRO_CORE:
         if (input & (1ULL << DEVICE_NAV_B))
         {
            menu_stack_push(LIBRETRO_CHOICE, true);
            filebrowser_set_root_and_ext(rgui->browser, EXT_EXECUTABLES, default_paths.core_dir);
         }
         break;
#ifdef HAVE_MULTIMAN
      case INGAME_MENU_RETURN_TO_MULTIMAN:
         if (input & (1ULL << DEVICE_NAV_B))
         {
            g_extern.lifecycle_mode_state &= ~(1ULL << MODE_GAME);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_INGAME_EXIT);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_EXIT);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_EXITSPAWN);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_EXITSPAWN_MULTIMAN);
            return -1;
         }
         break;
#endif
      case INGAME_MENU_QUIT_RETROARCH:
         if (input & (1ULL << DEVICE_NAV_B))
         {
            g_extern.lifecycle_mode_state &= ~(1ULL << MODE_GAME);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_EXIT);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_INGAME_EXIT);
            return -1;
         }
         break;
      case INGAME_MENU_VIDEO_OPTIONS_MODE:
         if (input & (1ULL << DEVICE_NAV_B))
            menu_stack_push(INGAME_MENU_VIDEO_OPTIONS, false);
         break;
      case INGAME_MENU_INPUT_OPTIONS_MODE:
         if (input & (1ULL << DEVICE_NAV_B))
            menu_stack_push(INGAME_MENU_INPUT_OPTIONS, false);
         break;
      case INGAME_MENU_PATH_OPTIONS_MODE:
         if (input & (1ULL << DEVICE_NAV_B))
            menu_stack_push(INGAME_MENU_PATH_OPTIONS, false);
         break;
#ifdef HAVE_SHADER_MANAGER
      case SHADERMAN_LOAD_CGP:
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         {
            menu_stack_push(CGP_CHOICE, true);
            filebrowser_set_root_and_ext(rgui->browser, EXT_CGP_PRESETS, g_settings.video.shader_dir);
         }
         if (input & (1ULL << DEVICE_NAV_START))
         {
            g_settings.video.shader_path[0] = '\0';
            video_set_shader_func(RARCH_SHADER_CG, NULL);
            g_settings.video.shader_enable = false;
         }
         break;
      case SHADERMAN_SAVE_CGP:
#ifdef HAVE_OSKUTIL
         if ((input & (1ULL << DEVICE_NAV_LEFT)) || (input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         {
            rgui->osk_param = SHADER_PRESET_FILE;
            rgui->osk_init = osk_callback_enter_filename_init;
            rgui->osk_callback = osk_callback_enter_filename;
         }
#endif
         break;
      case SHADERMAN_SHADER_PASSES:
         if (input & (1ULL << DEVICE_NAV_LEFT))
            if (rgui->shader.passes)
               rgui->shader.passes--;
         if ((input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
            if (rgui->shader.passes < RGUI_MAX_SHADERS)
               rgui->shader.passes++;
         if (input & (1ULL << DEVICE_NAV_START))
            rgui->shader.passes= 0;
         break;
      case SHADERMAN_AUTOSTART_CGP_ON_STARTUP:
         break;
      case SHADERMAN_SHADER_0:
      case SHADERMAN_SHADER_1:
      case SHADERMAN_SHADER_2:
      case SHADERMAN_SHADER_3:
      case SHADERMAN_SHADER_4:
      case SHADERMAN_SHADER_5:
      case SHADERMAN_SHADER_6:
      case SHADERMAN_SHADER_7:
         {
            uint8_t index = (switchvalue - SHADERMAN_SHADER_0) / 3;
            struct gfx_shader_pass *pass = &rgui->shader.pass[index];

            if ((input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)) ||
                  (input & (1ULL << DEVICE_NAV_LEFT)))
            {
               shader_choice_set_shader_slot = index;
               menu_stack_push(SHADER_CHOICE, true);
               filebrowser_set_root_and_ext(rgui->browser, EXT_SHADERS, g_settings.video.shader_dir);
            }

            if (input & (1ULL << DEVICE_NAV_START))
               *pass->source.cg = '\0';
         }
         break;
      case SHADERMAN_SHADER_0_FILTER:
      case SHADERMAN_SHADER_1_FILTER:
      case SHADERMAN_SHADER_2_FILTER:
      case SHADERMAN_SHADER_3_FILTER:
      case SHADERMAN_SHADER_4_FILTER:
      case SHADERMAN_SHADER_5_FILTER:
      case SHADERMAN_SHADER_6_FILTER:
      case SHADERMAN_SHADER_7_FILTER:
         {
            uint8_t index = (switchvalue - SHADERMAN_SHADER_0) / 3;

            if ((input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)) ||
                  (input & (1ULL << DEVICE_NAV_LEFT)))
            {
               unsigned delta = (input & (1ULL << DEVICE_NAV_LEFT)) ? 2 : 1;
               rgui->shader.pass[index].filter = (enum gfx_filter_type)((rgui->shader.pass[index].filter + delta) % 3);
            }

            if (input & (1ULL << DEVICE_NAV_START))
               rgui->shader.pass[index].filter = RARCH_FILTER_UNSPEC;
         }
         break;
      case SHADERMAN_SHADER_0_SCALE:
      case SHADERMAN_SHADER_1_SCALE:
      case SHADERMAN_SHADER_2_SCALE:
      case SHADERMAN_SHADER_3_SCALE:
      case SHADERMAN_SHADER_4_SCALE:
      case SHADERMAN_SHADER_5_SCALE:
      case SHADERMAN_SHADER_6_SCALE:
      case SHADERMAN_SHADER_7_SCALE:
         {
            uint8_t index = (switchvalue - SHADERMAN_SHADER_0) / 3;
            unsigned scale = rgui->shader.pass[index].fbo.scale_x;

            if (input & (1ULL << DEVICE_NAV_LEFT))
            {
               if (scale)
               {
                  rgui->shader.pass[index].fbo.scale_x = rgui->shader.pass[index].fbo.scale_y = scale - 1;
                  rgui->shader.pass[index].fbo.valid = scale - 1;
               }
            }

            if ((input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
            {
               if (scale < 5)
               {
                  rgui->shader.pass[index].fbo.scale_x = rgui->shader.pass[index].fbo.scale_y = scale + 1;
                  rgui->shader.pass[index].fbo.valid = scale + 1;
               }
            }

            if (input & (1ULL << DEVICE_NAV_START))
            {
               rgui->shader.pass[index].fbo.scale_x = rgui->shader.pass[index].fbo.scale_y = 0;
               rgui->shader.pass[index].fbo.valid = false;
            }
         }
         break;
      case SHADERMAN_APPLY_CHANGES:
         if ((input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)) ||
               (input & (1ULL << DEVICE_NAV_START)) || (input & (1ULL << DEVICE_NAV_LEFT)))
         {
            bool ret = false;
            char cgp_path[PATH_MAX];

            if (rgui->shader.passes)
            {
               const char *shader_dir = *g_settings.video.shader_dir ?
                  g_settings.video.shader_dir : g_settings.system_directory;
               fill_pathname_join(cgp_path, shader_dir, "rgui.cgp", sizeof(cgp_path));
               config_file_t *conf = config_file_new(NULL);
               if (!conf)
                  return 0;
               gfx_shader_write_conf_cgp(conf, &rgui->shader);
               config_file_write(conf, cgp_path);
               config_file_free(conf);
            }
            else
               cgp_path[0] = '\0';

            ret = video_set_shader_func(RARCH_SHADER_CG, (cgp_path[0] != '\0') ? cgp_path : NULL); 

            if (ret)
               g_settings.video.shader_enable = true;
            else
            {
               RARCH_ERR("Setting RGUI CGP failed.\n");
               g_settings.video.shader_enable = false;
            }
         }
         break;
#endif
   }

   return 0;
}

static int select_setting(void *data, uint64_t input)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   static uint8_t first_setting = FIRST_VIDEO_SETTING;
   uint8_t items_pages[SETTING_LAST_LAST] = {0};
   uint8_t max_settings = 0;

   font_params_t font_parms = {0};

   int ret = 0;

   switch (rgui->menu_type)
   {
      case GENERAL_VIDEO_MENU:
         first_setting = FIRST_VIDEO_SETTING;
         max_settings = MAX_NO_OF_VIDEO_SETTINGS;
         break;
      case GENERAL_AUDIO_MENU:
         first_setting = FIRST_AUDIO_SETTING;
         max_settings = MAX_NO_OF_AUDIO_SETTINGS;
         break;
      case EMU_GENERAL_MENU:
         first_setting = FIRST_EMU_SETTING;
         max_settings = MAX_NO_OF_EMU_SETTINGS;
         break;
      case EMU_VIDEO_MENU:
         first_setting = FIRST_EMU_VIDEO_SETTING;
         max_settings = MAX_NO_OF_EMU_VIDEO_SETTINGS;
         break;
      case EMU_AUDIO_MENU:
         first_setting = FIRST_EMU_AUDIO_SETTING;
         max_settings = MAX_NO_OF_EMU_AUDIO_SETTINGS;
         break;
      case INGAME_MENU:
         first_setting = FIRST_INGAME_MENU_SETTING;
         max_settings = MAX_NO_OF_INGAME_MENU_SETTINGS;
         break;
      case INGAME_MENU_INPUT_OPTIONS:
         first_setting = FIRST_CONTROLS_SETTING_PAGE_1;
         max_settings = MAX_NO_OF_CONTROLS_SETTINGS;
         break;
      case INGAME_MENU_PATH_OPTIONS:
         first_setting = FIRST_PATH_SETTING;
         max_settings = MAX_NO_OF_PATH_SETTINGS;
         break;
      case INGAME_MENU_VIDEO_OPTIONS:
         first_setting = FIRST_SHADERMAN_SETTING;
         max_settings = SHADERMAN_SHADER_LAST;

#ifdef HAVE_SHADER_MANAGER
         switch (rgui->shader.passes)
         {
            case 0:
               max_settings = MAX_NO_OF_SHADERMAN_SETTINGS;
               break;
            case 1:
               max_settings = SHADERMAN_SHADER_0_SCALE+1;
               break;
            case 2:
               max_settings = SHADERMAN_SHADER_1_SCALE+1;
               break;
            case 3:
               max_settings = SHADERMAN_SHADER_2_SCALE+1;
               break;
            case 4:
               max_settings = SHADERMAN_SHADER_3_SCALE+1;
               break;
            case 5:
               max_settings = SHADERMAN_SHADER_4_SCALE+1;
               break;
            case 6:
               max_settings = SHADERMAN_SHADER_5_SCALE+1;
               break;
            case 7:
               max_settings = SHADERMAN_SHADER_6_SCALE+1;
               break;
            case 8:
               max_settings = SHADERMAN_SHADER_7_SCALE+1;
               break;
         }
#endif
         break;
   }

   float y_increment = POSITION_Y_START;
   uint8_t i = 0;
   uint8_t j = 0;
   uint8_t item_page = 0;

   for(i = first_setting; i < max_settings; i++)
   {
      char fname[PATH_MAX];
      char text[PATH_MAX];
      char comment[PATH_MAX];
      char setting_text[PATH_MAX];
      (void)fname;

      switch (i)
      {
#ifdef __CELLOS_LV2__
         case SETTING_CHANGE_RESOLUTION:
            {
               unsigned width = gfx_ctx_get_resolution_width(g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.current.idx]);
               unsigned height = gfx_ctx_get_resolution_height(g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.current.idx]);
               strlcpy(text, "Resolution", sizeof(text));
               strlcpy(comment, "INFO - Change the display resolution.", sizeof(comment));
               snprintf(setting_text, sizeof(setting_text), "%dx%d", width, height);
            }
            break;
         case SETTING_PAL60_MODE:
            strlcpy(text, "PAL60 Mode", sizeof(text));
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE))
            {
               strlcpy(setting_text, "ON", sizeof(setting_text));
               strlcpy(comment, "INFO - [PAL60 Mode is set to 'ON'.\nconverts frames from 60Hz to 50Hz.", sizeof(comment));
            }
            else
            {
               strlcpy(setting_text, "OFF", sizeof(setting_text));
               strlcpy(comment, "INFO - [PAL60 Mode is set to 'OFF'.\nframes are not converted.", sizeof(comment));
            }
            break;
#endif
         case SETTING_EMU_SKIN:
            fill_pathname_base(fname, g_extern.menu_texture_path, sizeof(fname));
            strlcpy(text, "Menu Skin", sizeof(text));
            strlcpy(setting_text, fname, sizeof(setting_text));
            strlcpy(comment, "INFO - Select a skin for the menu.", sizeof(comment));
            break;
         case SETTING_FONT_SIZE:
            strlcpy(text, "Font Size", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), "%f", g_settings.video.font_size);
            strlcpy(comment, "INFO - Increase or decrease the [Font Size].", sizeof(comment));
            break;
         case SETTING_HW_TEXTURE_FILTER:
            strlcpy(text, "Default Filter", sizeof(text));
            if (g_settings.video.smooth)
            {
               strlcpy(setting_text, "Linear", sizeof(setting_text));
               strlcpy(comment, "INFO - Default Filter is set to Linear.",
                     sizeof(comment));
            }
            else
            {
               strlcpy(setting_text, "Nearest", sizeof(setting_text));
               strlcpy(comment, "INFO - Default Filter is set to Nearest.",
                     sizeof(comment));
            }
            break;
#ifdef _XBOX1
         case SETTING_FLICKER_FILTER:
            strlcpy(text, "Flicker Filter", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), "%d", g_extern.console.screen.flicker_filter_index);
            strlcpy(comment, "INFO - Toggle the [Flicker Filter].", sizeof(comment));
            break;
         case SETTING_SOFT_DISPLAY_FILTER:
            strlcpy(text, "Soft Display Filter", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE)) ? "ON" : "OFF");
            strlcpy(comment, "INFO - Toggle the [Soft Display Filter].", sizeof(comment));
            break;
#endif
         case SETTING_REFRESH_RATE:
            strlcpy(text, "Refresh rate", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), "%fHz", g_settings.video.refresh_rate);
            strlcpy(comment, "INFO - Adjust or decrease [Refresh Rate].", sizeof(comment));
            break;
         case SETTING_THROTTLE_MODE:
            strlcpy(text, "Throttle Mode", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_THROTTLE_ENABLE)) ? "ON" : "OFF");
            snprintf(comment, sizeof(comment), (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_THROTTLE_ENABLE)) ? "INFO - [Throttle Mode] is 'ON' - Vsync is enabled." : "INFO - [Throttle Mode] is 'OFF' - Vsync is disabled.");
            break;
         case SETTING_TRIPLE_BUFFERING:
            strlcpy(text, "Triple Buffering", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_TRIPLE_BUFFERING_ENABLE)) ? "ON" : "OFF");
            snprintf(comment, sizeof(comment), (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_TRIPLE_BUFFERING_ENABLE)) ? "INFO - [Triple Buffering] is set to 'ON'." : "INFO - [Triple Buffering] is set to 'OFF'.");
            break;
         case SETTING_DEFAULT_VIDEO_ALL:
            strlcpy(text, "DEFAULTS", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            strlcpy(comment, "INFO - Reset all these settings.", sizeof(comment));
            break;
         case SETTING_SOUND_MODE:
            strlcpy(text, "Sound Output", sizeof(text));
            switch(g_extern.console.sound.mode)
            {
               case SOUND_MODE_NORMAL:
                  strlcpy(comment, "INFO - [Sound Output] is set to 'Normal'.", sizeof(comment));
                  strlcpy(setting_text, "Normal", sizeof(setting_text));
                  break;
#ifdef HAVE_RSOUND
               case SOUND_MODE_RSOUND:
                  strlcpy(comment, "INFO - [Sound Output] is set to 'RSound'.", sizeof(comment));
                  strlcpy(setting_text, "RSound", sizeof(setting_text));
                  break;
#endif
#ifdef HAVE_HEADSET
               case SOUND_MODE_HEADSET:
                  strlcpy(comment, "INFO - [Sound Output] is set to USB/Bluetooth Headset.", sizeof(comment));
                  strlcpy(setting_text, "USB/Bluetooth Headset", sizeof(setting_text));
                  break;
#endif
               default:
                  break;
            }
            break;
#ifdef HAVE_RSOUND
         case SETTING_RSOUND_SERVER_IP_ADDRESS:
            strlcpy(text, "RSound Server IP Address", sizeof(text));
            strlcpy(setting_text, g_settings.audio.device, sizeof(setting_text));
            strlcpy(comment, "INFO - Enter the IP Address of the [RSound Audio Server]. IP address\nmust be an IPv4 32-bits address, eg: '192.168.1.7'.", sizeof(comment));
            break;
#endif
         case SETTING_DEFAULT_AUDIO_ALL:
            strlcpy(text, "DEFAULTS", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            strlcpy(comment, "INFO - Reset all these settings.", sizeof(comment));
            break;
            /* emu-specific */
         case SETTING_EMU_SHOW_DEBUG_INFO_MSG:
            strlcpy(text, "Debug info messages", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_FPS_DRAW)) ? "ON" : "OFF");
            strlcpy(comment, "INFO - Show onscreen debug messages.", sizeof(comment));
            break;
         case SETTING_EMU_SHOW_INFO_MSG:
            strlcpy(text, "Info messages", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW)) ? "ON" : "OFF");
            strlcpy(comment, "INFO - Show onscreen info messages in the menu.", sizeof(comment));
            break;
         case INGAME_MENU_REWIND_ENABLED:
            strlcpy(text, "Rewind", sizeof(text));
            if (g_settings.rewind_enable)
            {
               strlcpy(setting_text, "ON", sizeof(setting_text));
               strlcpy(comment, "INFO - [Rewind] feature is set to 'ON'.",
                     sizeof(comment));
            }
            else
            {
               strlcpy(setting_text, "OFF", sizeof(setting_text));
               strlcpy(comment, "INFO - [Rewind] feature is set to 'OFF'.",
                     sizeof(comment));
            }
            break;
         case INGAME_MENU_REWIND_GRANULARITY:
            strlcpy(text, "Rewind Granularity", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), "%d", g_settings.rewind_granularity);
            strlcpy(comment, "INFO - Set the amount of frames to 'rewind'.", sizeof(comment));
            break;
         case SETTING_EMU_AUDIO_MUTE:
            strlcpy(text, "Mute Audio", sizeof(text));
            if (g_extern.audio_data.mute)
            {
               strlcpy(comment, "INFO - the game audio will be muted.", sizeof(comment));
               strlcpy(setting_text, "ON", sizeof(setting_text));
            }
            else
            {
               strlcpy(comment, "INFO - game audio will be on.", sizeof(comment));
               strlcpy(setting_text, "OFF", sizeof(setting_text));
            }
            break;
#ifdef _XBOX1
         case SETTING_EMU_AUDIO_SOUND_VOLUME_LEVEL:
            strlcpy(text, "Volume Level", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), g_extern.console.sound.volume_level ? "Loud" : "Normal");
            if (g_extern.audio_data.mute)
               strlcpy(comment, "INFO - Volume level is set to Loud.", sizeof(comment));
            else
               strlcpy(comment, "INFO - Volume level is set to Normal.", sizeof(comment));
            break;
#endif
         case SETTING_ENABLE_CUSTOM_BGM:
            strlcpy(text, "Custom BGM Option", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE)) ? "ON" : "OFF");
            snprintf(comment, sizeof(comment), "INFO - [Custom BGM] is set to '%s'.", (g_extern.lifecycle_mode_state & (1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE)) ? "ON" : "OFF");
            break;
         case SETTING_PATH_DEFAULT_ROM_DIRECTORY:
            strlcpy(text, "Browser directory", sizeof(text));
            strlcpy(setting_text, g_settings.rgui_browser_directory, sizeof(setting_text));
            strlcpy(comment, "INFO - Set the default startup browser directory path.", sizeof(comment));
            break;
         case SETTING_PATH_SAVESTATES_DIRECTORY:
            strlcpy(text, "Savestate Directory", sizeof(text));
            strlcpy(setting_text, g_extern.savestate_dir, sizeof(setting_text));
            strlcpy(comment, "INFO - Directory where savestates will be saved to.", sizeof(comment));
            break;
         case SETTING_PATH_SRAM_DIRECTORY:
            strlcpy(text, "Savefile Directory", sizeof(text));
            strlcpy(setting_text, g_extern.savefile_dir, sizeof(setting_text));
            strlcpy(comment, "INFO - Set the default SaveRAM directory path.", sizeof(comment));
            break;
#ifdef HAVE_XML
         case SETTING_PATH_CHEATS:
            strlcpy(text, "Cheatfile Directory", sizeof(text));
            strlcpy(setting_text, g_settings.cheat_database, sizeof(setting_text));
            strlcpy(comment, "INFO - Set the default Cheatfile directory path.", sizeof(comment));
            break;
#endif
         case SETTING_PATH_SYSTEM:
            strlcpy(text, "System Directory", sizeof(text));
            strlcpy(setting_text, g_settings.system_directory, sizeof(setting_text));
            strlcpy(comment, "INFO - Set the default [System directory] path.", sizeof(comment));
            break;
         case SETTING_ENABLE_SRAM_PATH:
            snprintf(text, sizeof(text), "Custom SRAM Dir Enable");
            snprintf(setting_text, sizeof(setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE)) ? "ON" : "OFF");
            snprintf(comment, sizeof(comment), "INFO - [Custom SRAM Dir Path] is set to '%s'.", (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE)) ? "ON" : "OFF");
            break;
         case SETTING_ENABLE_STATE_PATH:
            snprintf(text, sizeof(text), "Custom Savestate Dir Enable");
            snprintf(setting_text, sizeof(setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE)) ? "ON" : "OFF");
            snprintf(comment, sizeof(comment), "INFO - [Custom Savestate Dir Path] is set to '%s'.", (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE)) ? "ON" : "OFF");
            break;
         case SETTING_CONTROLS_SCHEME:
            strlcpy(text, "Control Scheme Preset", sizeof(text));
            snprintf(comment, sizeof(comment), "INFO - Input scheme preset [%s] is selected.", g_extern.file_state.input_cfg_path);
            strlcpy(setting_text, g_extern.file_state.input_cfg_path, sizeof(setting_text));
            break;
         case SETTING_CONTROLS_NUMBER:
            strlcpy(text, "Player", sizeof(text));
            snprintf(comment, sizeof(comment), "Player %d is currently selected.", rgui->current_pad+1);
            snprintf(setting_text, sizeof(setting_text), "%d", rgui->current_pad+1);
            break;
         case SETTING_DPAD_EMULATION:
            strlcpy(text, "D-Pad Emulation", sizeof(text));
            switch(g_settings.input.dpad_emulation[rgui->current_pad])
            {
               case ANALOG_DPAD_NONE:
                  snprintf(comment, sizeof(comment), "[%s] from Controller %d is mapped to D-pad.", "None", rgui->current_pad+1);
                  strlcpy(setting_text, "None", sizeof(setting_text));
                  break;
               case ANALOG_DPAD_LSTICK:
                  snprintf(comment, sizeof(comment), "[%s] from Controller %d is mapped to D-pad.", "Left Stick", rgui->current_pad+1);
                  strlcpy(setting_text, "Left Stick", sizeof(setting_text));
                  break;
               case ANALOG_DPAD_RSTICK:
                  snprintf(comment, sizeof(comment), "[%s] from Controller %d is mapped to D-pad.", "Right Stick", rgui->current_pad+1);
                  strlcpy(setting_text, "Right Stick", sizeof(setting_text));
                  break;
            }
            break;
         case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_B:
         case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_Y:
         case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_SELECT:
         case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_START:
         case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_UP:
         case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_DOWN:
         case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_LEFT:
         case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_RIGHT:
         case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_A:
         case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_X:
         case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L:
         case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R:
         case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L2:
         case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R2:
         case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L3:
         case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R3:
            {
               unsigned id = i - FIRST_CONTROL_BIND;
               struct platform_bind key_label;
               strlcpy(key_label.desc, "Unknown", sizeof(key_label.desc));
               key_label.joykey = g_settings.input.binds[rgui->current_pad][id].joykey;

               if (driver.input->set_keybinds)
                  driver.input->set_keybinds(&key_label, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
               strlcpy(text, g_settings.input.binds[rgui->current_pad][id].desc, sizeof(text));
               snprintf(comment, sizeof(comment), "INFO - [%s] is mapped to action:\n[%s].", text, key_label.desc);
               strlcpy(setting_text, key_label.desc, sizeof(setting_text));
            }
            break;
         case SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS:
            strlcpy(text, "SAVE CUSTOM CONTROLS", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            strlcpy(comment, "INFO - Save the [Custom Controls] settings to file.",  sizeof(comment));
            break;
         case SETTING_CONTROLS_DEFAULT_ALL:
            strlcpy(text, "DEFAULTS", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            strlcpy(comment, "INFO - Set all Controls settings to defaults.", sizeof(comment));
            break;
         case SETTING_EMU_VIDEO_DEFAULT_ALL:
            strlcpy(text, "DEFAULTS", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            strlcpy(comment, "INFO - Set all Video settings to defaults.", sizeof(comment));
            break;
         case SETTING_EMU_AUDIO_DEFAULT_ALL:
            strlcpy(text, "DEFAULTS", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            strlcpy(comment, "INFO - Set all Audio settings to defaults.", sizeof(comment));
            break;
         case SETTING_PATH_DEFAULT_ALL:
            strlcpy(text, "DEFAULTS", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            strlcpy(comment, "INFO - Set all Path settings to defaults.", sizeof(comment));
            break;
         case SETTING_EMU_DEFAULT_ALL:
            strlcpy(text, "DEFAULTS", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            strlcpy(comment, "INFO - Set all RetroArch settings to defaults.", sizeof(comment));
            break;
         case INGAME_MENU_LOAD_STATE:
            strlcpy(text, "Load State", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), "%d", g_extern.state_slot);
            strlcpy(comment, "Load from current state slot.", sizeof(comment));
            break;
         case INGAME_MENU_SAVE_STATE:
            strlcpy(text, "Save State", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), "%d", g_extern.state_slot);
            strlcpy(comment, "Save to current state slot.", sizeof(comment));
            break;
         case SETTING_ASPECT_RATIO:
            strlcpy(text, "Aspect Ratio", sizeof(text));
            strlcpy(setting_text, aspectratio_lut[g_settings.video.aspect_ratio_idx].name, sizeof(setting_text));
            strlcpy(comment, "Change the aspect ratio of the screen.", sizeof(comment));
            break;
         case SETTING_ROTATION:
            strlcpy(text, "Rotation", sizeof(text));
            strlcpy(setting_text, rotation_lut[g_extern.console.screen.orientation], sizeof(setting_text));
            strlcpy(comment, "Change orientation of the screen.", sizeof(comment));
            break;
         case SETTING_CUSTOM_VIEWPORT:
            strlcpy(text, "Custom Ratio", sizeof(text));
            strlcpy(setting_text, "...", sizeof(setting_text));
            strlcpy(comment, "Allows you to resize the screen.", sizeof(comment));
            break;
         case INGAME_MENU_CORE_OPTIONS_MODE:
            strlcpy(text, "Core Options", sizeof(text));
            strlcpy(setting_text, "...", sizeof(setting_text));
            strlcpy(comment, "Set core-specific options.", sizeof(comment));
            break;
         case INGAME_MENU_LOAD_GAME_HISTORY_MODE:
            strlcpy(text, "Load Game (History)", sizeof(text));
            strlcpy(setting_text, "...", sizeof(setting_text));
            strlcpy(comment, "Select a game from the history list.", sizeof(comment));
            break;
         case INGAME_MENU_VIDEO_OPTIONS_MODE:
            strlcpy(text, "Video Options", sizeof(text));
            strlcpy(setting_text, "...", sizeof(setting_text));
            strlcpy(comment, "Set and manage video options.", sizeof(comment));
            break;
         case INGAME_MENU_INPUT_OPTIONS_MODE:
            strlcpy(text, "Input Options", sizeof(text));
            strlcpy(setting_text, "...", sizeof(setting_text));
            strlcpy(comment, "Set and manage input options.", sizeof(comment));
            break;
         case INGAME_MENU_PATH_OPTIONS_MODE:
            strlcpy(text, "Path Options", sizeof(text));
            strlcpy(setting_text, "...", sizeof(setting_text));
            strlcpy(comment, "Set and manage path options.", sizeof(comment));
            break;
         case INGAME_MENU_FRAME_ADVANCE:
            strlcpy(text, "Frame Advance", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            strlcpy(comment, "Press a button to step one frame.", sizeof(comment));
            break;
         case INGAME_MENU_SCREENSHOT_MODE:
            strlcpy(text, "Screenshot Mode", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            strlcpy(comment, "Take a screenshot.", sizeof(comment));
            break;
         case INGAME_MENU_RESET:
            strlcpy(text, "Restart Game", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            strlcpy(comment, "Reset the game.", sizeof(comment));
            break;
         case INGAME_MENU_RETURN_TO_GAME:
            strlcpy(text, "Resume Game", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            strlcpy(comment, "Resume the currently loaded game.", sizeof(comment));
            break;
         case INGAME_MENU_CHANGE_GAME:
            strlcpy(text, "Load Game", sizeof(text));
            strlcpy(setting_text, "...", sizeof(setting_text));
            strlcpy(comment, "Select a game to be loaded.", sizeof(comment));
            break;
         case INGAME_MENU_CHANGE_LIBRETRO_CORE:
            strlcpy(text, "Core", sizeof(text));
            strlcpy(setting_text, "...", sizeof(setting_text));
            strlcpy(comment, "Choose another libretro core.", sizeof(comment));
            break;
         case INGAME_MENU_SETTINGS:
            strlcpy(text, "Settings", sizeof(text));
            strlcpy(setting_text, "...", sizeof(setting_text));
            strlcpy(comment, "Change RetroArch settings.", sizeof(comment));
            break;
#ifdef HAVE_MULTIMAN
         case INGAME_MENU_RETURN_TO_MULTIMAN:
            strlcpy(text, "Return to multiMAN", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            strlcpy(comment, "Quit RetroArch and return to multiMAN.", sizeof(comment));
            break;
#endif
         case INGAME_MENU_QUIT_RETROARCH:
            strlcpy(text, "Quit RetroArch", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            strlcpy(comment, "Quit RetroArch.", sizeof(comment));
            break;
         default:
            break;
#ifdef HAVE_SHADER_MANAGER
         case SHADERMAN_LOAD_CGP:
            strlcpy(text, "Load Shader Preset", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            strlcpy(comment, "INFO - Select a CGP file.", sizeof(comment));
            break;
         case SHADERMAN_AUTOSTART_CGP_ON_STARTUP:
            strlcpy(text, "Autostart CGP at startup", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            strlcpy(comment, "INFO - Auto-load at startup the current shader settings.", sizeof(comment));
            break;
         case SHADERMAN_SAVE_CGP:
            strlcpy(text, "Save Shader Preset", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            strlcpy(comment, "INFO - Save current shader settings to a CGP file.", sizeof(comment));
            break;
         case SHADERMAN_SHADER_PASSES:
            strlcpy(text, "Shader Passes", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), "%u", rgui->shader.passes);
            strlcpy(comment, "INFO - Set the amount of shader passes.", sizeof(comment));
            break;
         case SHADERMAN_APPLY_CHANGES:
            strlcpy(text, "Apply Shader Changes", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            strlcpy(comment, "INFO - Apply the changes made below.", sizeof(comment));
            break;
         case SHADERMAN_SHADER_0:
         case SHADERMAN_SHADER_1:
         case SHADERMAN_SHADER_2:
         case SHADERMAN_SHADER_3:
         case SHADERMAN_SHADER_4:
         case SHADERMAN_SHADER_5:
         case SHADERMAN_SHADER_6:
         case SHADERMAN_SHADER_7:
            {
               char type_str[256];
               uint8_t index = (i - SHADERMAN_SHADER_0) / 3;
               if (*rgui->shader.pass[index].source.cg)
                  fill_pathname_base(type_str,
                        rgui->shader.pass[index].source.cg, sizeof(type_str));
               else
                  strlcpy(type_str, "N/A", sizeof(type_str));
               snprintf(text, sizeof(text), "Shader #%d", index);
               strlcpy(setting_text, type_str, sizeof(setting_text));
               strlcpy(comment, "INFO - Select the shader.", sizeof(comment));
            }
            break;
         case SHADERMAN_SHADER_0_FILTER:
         case SHADERMAN_SHADER_1_FILTER:
         case SHADERMAN_SHADER_2_FILTER:
         case SHADERMAN_SHADER_3_FILTER:
         case SHADERMAN_SHADER_4_FILTER:
         case SHADERMAN_SHADER_5_FILTER:
         case SHADERMAN_SHADER_6_FILTER:
         case SHADERMAN_SHADER_7_FILTER:
            {
               char type_str[256];
               uint8_t index = (i - SHADERMAN_SHADER_0) / 3;
               snprintf(text, sizeof(text), "Shader #%d filter", index);
               shader_manager_get_str_filter(type_str, sizeof(type_str), index);
               strlcpy(setting_text, type_str, sizeof(setting_text));
               strlcpy(comment, "INFO - Select the filtering.", sizeof(comment));
            }
            break;
         case SHADERMAN_SHADER_0_SCALE:
         case SHADERMAN_SHADER_1_SCALE:
         case SHADERMAN_SHADER_2_SCALE:
         case SHADERMAN_SHADER_3_SCALE:
         case SHADERMAN_SHADER_4_SCALE:
         case SHADERMAN_SHADER_5_SCALE:
         case SHADERMAN_SHADER_6_SCALE:
         case SHADERMAN_SHADER_7_SCALE:
            {
               char type_str[256];
               uint8_t index = (i - SHADERMAN_SHADER_0) / 3;
               unsigned scale = rgui->shader.pass[index].fbo.scale_x;

               snprintf(text, sizeof(text), "Shader #%d scale", index);

               if (!scale)
                  strlcpy(type_str, "Don't care", sizeof(type_str));
               else
                  snprintf(type_str, sizeof(type_str), "%ux", scale);

               strlcpy(setting_text, type_str, sizeof(setting_text));
               strlcpy(comment, "INFO - Select the scaling factor of this pass.", sizeof(comment));
            }
            break;
#endif
      }

      if (!(j < NUM_ENTRY_PER_PAGE))
      {
         j = 0;
         item_page++;
      }

      items_pages[i] = item_page;
      j++;

      if (item_page != setting_page_number)
         continue;

      y_increment += POSITION_Y_INCREMENT;

      font_parms.x = POSITION_X; 
      font_parms.y = y_increment;
      font_parms.scale = FONT_SIZE_VARIABLE;
      font_parms.color = (i == selected) ? YELLOW : WHITE;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, text, &font_parms);

      font_parms.x = POSITION_X_CENTER;
      font_parms.color = WHITE;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, setting_text, &font_parms);

      if (i != selected)
         continue;

#ifdef HAVE_MENU_PANEL
      menu_panel->y = y_increment;
#endif

      font_parms.x = POSITION_X; 
      font_parms.y = COMMENT_POSITION_Y;
      font_parms.scale = HARDCODE_FONT_SIZE;
      font_parms.color = WHITE;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, comment, &font_parms);
   }

   if (rgui->menu_type == INGAME_MENU)
   {
      if (input & (1ULL << DEVICE_NAV_A))
      {
         g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_INGAME_EXIT);
         g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
         return -1;
      }
   }

   if (input & (1ULL << DEVICE_NAV_UP))
   {
      if (selected == first_setting)
         selected = max_settings-1;
      else
         selected--;

      if (items_pages[selected] != setting_page_number)
         setting_page_number = items_pages[selected];
   }
      
   if (input & (1ULL << DEVICE_NAV_DOWN))
   {
      selected++;

      if (selected >= max_settings)
         selected = first_setting; 
      if (items_pages[selected] != setting_page_number)
         setting_page_number = items_pages[selected];
   }

   /* back to ROM menu if CIRCLE is pressed */
   if ((input & (1ULL << DEVICE_NAV_A))
         || (input & (1ULL << DEVICE_NAV_MENU)))
      menu_stack_pop(rgui->menu_type);
   else if (input & (1ULL << DEVICE_NAV_R1))
   {

      if (rgui->menu_type != INGAME_MENU_INPUT_OPTIONS
            || rgui->menu_type != INGAME_MENU_VIDEO_OPTIONS
            || rgui->menu_type != INGAME_MENU_PATH_OPTIONS
            || rgui->menu_type != INGAME_MENU
            )
         menu_stack_push(rgui->menu_type + 1, false);
   }

   ret = set_setting_action(rgui->menu_type, selected, input);

   if (ret != 0)
      return ret;

   display_menubar(rgui->menu_type);

   return 0;
}

static int select_rom(void *data, uint64_t input)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   font_params_t font_parms = {0};
   char msg[128];

   struct platform_bind key_label_b = {0};

   strlcpy(key_label_b.desc, "Unknown", sizeof(key_label_b.desc));
   key_label_b.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_B;

   if (driver.input->set_keybinds)
      driver.input->set_keybinds(&key_label_b, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));

   filebrowser_update(rgui->browser, input, rgui->browser->current_dir.extensions);

   if (input & (1ULL << DEVICE_NAV_B))
   {
      if (filebrowser_iterate(rgui->browser, FILEBROWSER_ACTION_PATH_ISDIR))
      {
         bool ret = filebrowser_iterate(rgui->browser, FILEBROWSER_ACTION_OK);

         if (!ret)
            msg_queue_push(g_extern.msg_queue, "ERROR - Failed to open directory.", 1, 180);
      }
      else
      {
         strlcpy(g_extern.fullpath,
               rgui->browser->current_dir.path, sizeof(g_extern.fullpath));
         g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME);
         return -1;
      }
   }
   else if (input & (1ULL << DEVICE_NAV_L1))
   {
      const char * drive_map = menu_drive_mapping_previous();
      if (drive_map != NULL)
         filebrowser_set_root_and_ext(rgui->browser, rgui->browser->current_dir.extensions, drive_map);
   }
   else if (input & (1ULL << DEVICE_NAV_R1))
   {
      const char * drive_map = menu_drive_mapping_next();
      if (drive_map != NULL)
         filebrowser_set_root_and_ext(rgui->browser, rgui->browser->current_dir.extensions, drive_map);
   }
   else if ((input & (1ULL << DEVICE_NAV_X) ||
            (input & (1ULL << DEVICE_NAV_MENU))))
      menu_stack_pop(rgui->menu_type);

   if (filebrowser_iterate(rgui->browser, FILEBROWSER_ACTION_PATH_ISDIR))
      snprintf(msg, sizeof(msg), "INFO - Press [%s] to enter the directory.", key_label_b.desc);
   else
      snprintf(msg, sizeof(msg), "INFO - Press [%s] to load the game.", key_label_b.desc);

   font_parms.x = POSITION_X; 
   font_parms.y = COMMENT_POSITION_Y;
   font_parms.scale = HARDCODE_FONT_SIZE;
   font_parms.color = WHITE;

   if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

   display_menubar(rgui->menu_type);
   browser_render(rgui->browser);

   return 0;
}

static int ingame_menu_resize(void *data, uint64_t input)
{
   (void)data;
   font_params_t font_parms = {0};

   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;

   g_settings.video.aspect_ratio_idx = ASPECT_RATIO_CUSTOM;
   
   if (driver.video_poke->set_aspect_ratio)
      driver.video_poke->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);

   if (input & (1ULL << DEVICE_NAV_LEFT_ANALOG_L))
   {
#ifdef _XBOX
      if (g_extern.console.screen.viewports.custom_vp.x >= 4)
#endif
         g_extern.console.screen.viewports.custom_vp.x -= 4;
   }
   else if (input & (1ULL << DEVICE_NAV_LEFT) && (input & ~(1ULL << DEVICE_NAV_LEFT_ANALOG_L)))
   {
#ifdef _XBOX
      if (g_extern.console.screen.viewports.custom_vp.x > 0)
#endif
         g_extern.console.screen.viewports.custom_vp.x -= 1;
   }

   if (input & (1ULL << DEVICE_NAV_RIGHT_ANALOG_L))
      g_extern.console.screen.viewports.custom_vp.x += 4;
   else if (input & (1ULL << DEVICE_NAV_RIGHT) && (input & ~(1ULL << DEVICE_NAV_RIGHT_ANALOG_L)))
      g_extern.console.screen.viewports.custom_vp.x += 1;

   if (input & (1ULL << DEVICE_NAV_UP_ANALOG_L))
      g_extern.console.screen.viewports.custom_vp.y += 4;
   else if (input & (1ULL << DEVICE_NAV_UP) && (input & ~(1ULL << DEVICE_NAV_UP_ANALOG_L)))
      g_extern.console.screen.viewports.custom_vp.y += 1;

   if (input & (1ULL << DEVICE_NAV_DOWN_ANALOG_L))
   {
#ifdef _XBOX
      if (g_extern.console.screen.viewports.custom_vp.y >= 4)
#endif
         g_extern.console.screen.viewports.custom_vp.y -= 4;
   }
   else if (input & (1ULL << DEVICE_NAV_DOWN) && (input & ~(1ULL << DEVICE_NAV_DOWN_ANALOG_L)))
   {
#ifdef _XBOX
      if (g_extern.console.screen.viewports.custom_vp.y > 0)
#endif
         g_extern.console.screen.viewports.custom_vp.y -= 1;
   }

   if (input & (1ULL << DEVICE_NAV_LEFT_ANALOG_R))
      g_extern.console.screen.viewports.custom_vp.width -= 4;
   else if (input & (1ULL << DEVICE_NAV_L1) && (input && ~(1ULL << DEVICE_NAV_LEFT_ANALOG_R)))
      g_extern.console.screen.viewports.custom_vp.width -= 1;

   if (input & (1ULL << DEVICE_NAV_RIGHT_ANALOG_R))
      g_extern.console.screen.viewports.custom_vp.width += 4;
   else if (input & (1ULL << DEVICE_NAV_R1) && (input & ~(1ULL << DEVICE_NAV_RIGHT_ANALOG_R)))
      g_extern.console.screen.viewports.custom_vp.width += 1;

   if (input & (1ULL << DEVICE_NAV_UP_ANALOG_R))
      g_extern.console.screen.viewports.custom_vp.height += 4;
   else if (input & (1ULL << DEVICE_NAV_L2) && (input & ~(1ULL << DEVICE_NAV_UP_ANALOG_R)))
      g_extern.console.screen.viewports.custom_vp.height += 1;

   if (input & (1ULL << DEVICE_NAV_DOWN_ANALOG_R))
      g_extern.console.screen.viewports.custom_vp.height -= 4;
   else if (input & (1ULL << DEVICE_NAV_R2) && (input & ~(1ULL << DEVICE_NAV_DOWN_ANALOG_R)))
      g_extern.console.screen.viewports.custom_vp.height -= 1;

   if (input & (1ULL << DEVICE_NAV_X))
   {
      g_extern.console.screen.viewports.custom_vp.x = 0;
      g_extern.console.screen.viewports.custom_vp.y = 0;
      g_extern.console.screen.viewports.custom_vp.width = device_ptr->win_width;
      g_extern.console.screen.viewports.custom_vp.height = device_ptr->win_height;
   }

   if ((input & (1ULL << DEVICE_NAV_A) ||
            (input & (1ULL << DEVICE_NAV_MENU))))
      menu_stack_pop(rgui->menu_type);

   if ((input & (1ULL << DEVICE_NAV_Y)))
      rgui->frame_buf_show = !rgui->frame_buf_show;

   if (rgui->frame_buf_show)
   {
      char viewport[32];
      char msg[128];
      struct platform_bind key_label_b = {0};
      struct platform_bind key_label_a = {0};
      struct platform_bind key_label_y = {0};
      struct platform_bind key_label_x = {0};
      struct platform_bind key_label_l1 = {0};
      struct platform_bind key_label_l2 = {0};
      struct platform_bind key_label_r1 = {0};
      struct platform_bind key_label_r2 = {0};
      struct platform_bind key_label_dpad_left = {0};
      struct platform_bind key_label_dpad_right = {0};
      struct platform_bind key_label_dpad_up = {0};
      struct platform_bind key_label_dpad_down = {0};
      
      strlcpy(key_label_b.desc, "Unknown", sizeof(key_label_b.desc));
      key_label_b.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_B;
      strlcpy(key_label_a.desc, "Unknown", sizeof(key_label_a.desc));
      key_label_a.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_A;
      strlcpy(key_label_x.desc, "Unknown", sizeof(key_label_x.desc));
      key_label_x.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_X;
      strlcpy(key_label_y.desc, "Unknown", sizeof(key_label_y.desc));
      key_label_y.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_Y;
      strlcpy(key_label_l1.desc, "Unknown", sizeof(key_label_l1.desc));
      key_label_l1.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_L;
      strlcpy(key_label_r1.desc, "Unknown", sizeof(key_label_r1.desc));
      key_label_r1.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_R;
      strlcpy(key_label_l2.desc, "Unknown", sizeof(key_label_l2.desc));
      key_label_l2.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_L2;
      strlcpy(key_label_r2.desc, "Unknown", sizeof(key_label_r2.desc));
      key_label_r2.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_R2;
      strlcpy(key_label_dpad_left.desc, "Unknown", sizeof(key_label_dpad_left.desc));
      key_label_dpad_left.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT;
      strlcpy(key_label_dpad_right.desc, "Unknown", sizeof(key_label_dpad_left.desc));
      key_label_dpad_right.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT;
      strlcpy(key_label_dpad_up.desc, "Unknown", sizeof(key_label_dpad_up.desc));
      key_label_dpad_up.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_UP;
      strlcpy(key_label_dpad_down.desc, "Unknown", sizeof(key_label_dpad_down.desc));
      key_label_dpad_down.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN;

      if (driver.input->set_keybinds)
      {
         driver.input->set_keybinds(&key_label_l1, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         driver.input->set_keybinds(&key_label_r1, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         driver.input->set_keybinds(&key_label_l2, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         driver.input->set_keybinds(&key_label_r2, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         driver.input->set_keybinds(&key_label_b, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         driver.input->set_keybinds(&key_label_a, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         driver.input->set_keybinds(&key_label_y, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         driver.input->set_keybinds(&key_label_x, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         driver.input->set_keybinds(&key_label_dpad_left, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         driver.input->set_keybinds(&key_label_dpad_right, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         driver.input->set_keybinds(&key_label_dpad_up, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         driver.input->set_keybinds(&key_label_dpad_down, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
      }

      display_menubar(rgui->menu_type);

      snprintf(viewport, sizeof(viewport), "Viewport X: #%d Y: %d (%dx%d)", g_extern.console.screen.viewports.custom_vp.x, g_extern.console.screen.viewports.custom_vp.y, g_extern.console.screen.viewports.custom_vp.width,
            g_extern.console.screen.viewports.custom_vp.height);

      font_parms.x = POSITION_X; 
      font_parms.y = POSITION_Y_BEGIN;
      font_parms.scale = HARDCODE_FONT_SIZE;
      font_parms.color = GREEN;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, viewport, &font_parms);

      snprintf(msg, sizeof(msg), "[%s]", key_label_dpad_left.desc);

      font_parms.y = POSITION_Y_BEGIN + (POSITION_Y_INCREMENT * 4);
      font_parms.color = WHITE;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

      font_parms.x = POSITION_X_CENTER;
      font_parms.y = POSITION_Y_BEGIN + (POSITION_Y_INCREMENT * 4);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "- Viewport X--", &font_parms);

      snprintf(msg, sizeof(msg), "[%s]", key_label_dpad_right.desc);

      font_parms.x = POSITION_X; 
      font_parms.y = POSITION_Y_BEGIN + (POSITION_Y_INCREMENT * 5);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

      font_parms.x = POSITION_X_CENTER;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "- Viewport X++", &font_parms);

      snprintf(msg, sizeof(msg), "[%s]", key_label_dpad_up.desc);
      font_parms.x = POSITION_X; 
      font_parms.y = POSITION_Y_BEGIN + (POSITION_Y_INCREMENT * 6);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

      font_parms.x = POSITION_X_CENTER;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "- Viewport Y++", &font_parms);

      snprintf(msg, sizeof(msg), "[%s]", key_label_dpad_down.desc);

      font_parms.x = POSITION_X; 
      font_parms.y = POSITION_Y_BEGIN + (POSITION_Y_INCREMENT * 7);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

      font_parms.x = POSITION_X_CENTER;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "- Viewport Y--", &font_parms);

      snprintf(msg, sizeof(msg), "[%s]", key_label_l1.desc);

      font_parms.x = POSITION_X; 
      font_parms.y = POSITION_Y_BEGIN + (POSITION_Y_INCREMENT * 8);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

      font_parms.x = POSITION_X_CENTER;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "- Viewport W--", &font_parms);

      snprintf(msg, sizeof(msg), "[%s]", key_label_r1.desc);

      font_parms.x = POSITION_X; 
      font_parms.y = POSITION_Y_BEGIN + (POSITION_Y_INCREMENT * 9);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

      font_parms.x = POSITION_X_CENTER;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "- Viewport W++", &font_parms);

      snprintf(msg, sizeof(msg), "[%s]", key_label_l2.desc);

      font_parms.x = POSITION_X; 
      font_parms.y = POSITION_Y_BEGIN + (POSITION_Y_INCREMENT * 10);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

      font_parms.x = POSITION_X_CENTER;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "- Viewport H++", &font_parms);

      snprintf(msg, sizeof(msg), "[%s]", key_label_r2.desc);

      font_parms.x = POSITION_X; 
      font_parms.y = POSITION_Y_BEGIN + (POSITION_Y_INCREMENT * 11);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

      font_parms.x = POSITION_X_CENTER;
      
      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "- Viewport H--", &font_parms);

      snprintf(msg, sizeof(msg), "[%s]", key_label_x.desc);

      font_parms.x = POSITION_X; 
      font_parms.y = POSITION_Y_BEGIN + (POSITION_Y_INCREMENT * 12);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

      font_parms.x = POSITION_X_CENTER;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "- Reset To Defaults", &font_parms);

      snprintf(msg, sizeof(msg), "[%s]", key_label_y.desc);

      font_parms.x = POSITION_X; 
      font_parms.y = POSITION_Y_BEGIN + (POSITION_Y_INCREMENT * 13);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

      font_parms.x = POSITION_X_CENTER;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "- Show Game", &font_parms);

      snprintf(msg, sizeof(msg), "[%s]", key_label_a.desc);
      font_parms.x = POSITION_X; 
      font_parms.y = POSITION_Y_BEGIN + (POSITION_Y_INCREMENT * 14);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

      font_parms.x = POSITION_X_CENTER;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "- Go back", &font_parms);

      snprintf(msg, sizeof(msg), "Press [%s] to reset to defaults.", key_label_x.desc);
      font_parms.x = POSITION_X; 
      font_parms.y = COMMENT_POSITION_Y;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);
   }

   return 0;
}

static int ingame_menu_history_options(void *data, uint64_t input)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   size_t history_size = rom_history_size(rgui->history);
   static unsigned hist_opt_selected = 0;
   float y_increment = POSITION_Y_START;

   if ((input & (1ULL << DEVICE_NAV_A)) || (input & (1ULL << DEVICE_NAV_MENU)))
      menu_stack_pop(rgui->menu_type);

   y_increment += POSITION_Y_INCREMENT;

   font_params_t font_parms = {0};
   font_parms.x = POSITION_X; 
   font_parms.y = y_increment;
   font_parms.scale = CURRENT_PATH_FONT_SIZE;
   font_parms.color = WHITE;

   if (history_size)
   {
      size_t opts = history_size;
      for (size_t i = 0; i < opts; i++, font_parms.y += POSITION_Y_INCREMENT)
      {
         const char *path = NULL;
         const char *core_path = NULL;
         const char *core_name = NULL;

         rom_history_get_index(rgui->history, i,
               &path, &core_path, &core_name);

         char path_short[PATH_MAX];
         fill_pathname(path_short, path_basename(path), "", sizeof(path_short));

         char fill_buf[PATH_MAX];
         snprintf(fill_buf, sizeof(fill_buf), "%s (%s)",
               path_short, core_name);

         /* not on same page? */
         if ((i / NUM_ENTRY_PER_PAGE) != (hist_opt_selected / NUM_ENTRY_PER_PAGE))
            continue;

#ifdef HAVE_MENU_PANEL
         //check if this is the currently selected option
         if (i == hist_opt_selected)
            menu_panel->y = font_parms.y;
#endif

         font_parms.x = POSITION_X; 
         font_parms.color = (hist_opt_selected == i) ? YELLOW : WHITE;

         if (driver.video_poke->set_osd_msg)
            driver.video_poke->set_osd_msg(driver.video_data,
                  fill_buf, &font_parms);
      }

      if ((input & (1ULL << DEVICE_NAV_START)) ||
            (input & (1ULL << DEVICE_NAV_B))
            )
      {
         load_menu_game_history(hist_opt_selected);
         return -1;
      }

      if (input & (1ULL << DEVICE_NAV_UP))
      {
         if (hist_opt_selected == 0)
            hist_opt_selected = history_size - 1;
         else
            hist_opt_selected--;
      }
      
      if (input & (1ULL << DEVICE_NAV_DOWN))
      {
         hist_opt_selected++;

         if (hist_opt_selected >= history_size)
            hist_opt_selected = 0; 
      }
   }
   else if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, "No history available.", &font_parms);

   display_menubar(rgui->menu_type);

   return 0;
}

static int ingame_menu_core_options(void *data, uint64_t input)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   static unsigned core_opt_selected = 0;
   float y_increment = POSITION_Y_START;

   if ((input & (1ULL << DEVICE_NAV_A)) || (input & (1ULL << DEVICE_NAV_MENU)))
      menu_stack_pop(rgui->menu_type);

   y_increment += POSITION_Y_INCREMENT;

   font_params_t font_parms = {0};
   font_parms.x = POSITION_X; 
   font_parms.y = y_increment;
   font_parms.scale = CURRENT_PATH_FONT_SIZE;
   font_parms.color = WHITE;

   if (g_extern.system.core_options)
   {
      size_t opts = core_option_size(g_extern.system.core_options);
      for (size_t i = 0; i < opts; i++, font_parms.y += POSITION_Y_INCREMENT)
      {
         char type_str[256];

         /* not on same page? */
         if ((i / NUM_ENTRY_PER_PAGE) != (core_opt_selected / NUM_ENTRY_PER_PAGE))
            continue;

#ifdef HAVE_MENU_PANEL
         //check if this is the currently selected option
         if (i == core_opt_selected)
            menu_panel->y = font_parms.y;
#endif

         font_parms.x = POSITION_X; 
         font_parms.color = (core_opt_selected == i) ? YELLOW : WHITE;

         if (driver.video_poke->set_osd_msg)
            driver.video_poke->set_osd_msg(driver.video_data,
                  core_option_get_desc(g_extern.system.core_options, i), &font_parms);

         font_parms.x = POSITION_X_CENTER;
         font_parms.color = WHITE;

         strlcpy(type_str, core_option_get_val(g_extern.system.core_options, i), sizeof(type_str));

         if (driver.video_poke->set_osd_msg)
            driver.video_poke->set_osd_msg(driver.video_data, type_str, &font_parms);
      }

      if (input & (1ULL << DEVICE_NAV_LEFT))
         core_option_prev(g_extern.system.core_options, core_opt_selected);

      if ((input & (1ULL << DEVICE_NAV_RIGHT)) || (input & (1ULL << DEVICE_NAV_B)))
         core_option_next(g_extern.system.core_options, core_opt_selected);

      if (input & (1ULL << DEVICE_NAV_START))
         core_option_set_default(g_extern.system.core_options, core_opt_selected);

      if (input & (1ULL << DEVICE_NAV_UP))
      {
         if (core_opt_selected == 0)
            core_opt_selected = core_option_size(g_extern.system.core_options) - 1;
         else
            core_opt_selected--;
      }
      
      if (input & (1ULL << DEVICE_NAV_DOWN))
      {
         core_opt_selected++;

         if (core_opt_selected >= core_option_size(g_extern.system.core_options))
            core_opt_selected = 0; 
      }
   }
   else if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, "No options available.", &font_parms);

   display_menubar(rgui->menu_type);

   return 0;
}

static int ingame_menu_screenshot(void *data, uint64_t input)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   rgui->frame_buf_show = false;

   if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_INGAME))
   {
      if ((input & (1ULL << DEVICE_NAV_A)) || (input & (1ULL << DEVICE_NAV_MENU)))
         menu_stack_pop(rgui->menu_type);

#ifdef HAVE_SCREENSHOTS
      if (input & (1ULL << DEVICE_NAV_B))
      {
         const uint16_t *data = (const uint16_t*)g_extern.frame_cache.data;
         unsigned width       = g_extern.frame_cache.width;
         unsigned height      = g_extern.frame_cache.height;
         int pitch            = g_extern.frame_cache.pitch;

         // Negative pitch is needed as screenshot takes bottom-up,
         // but we use top-down.
         screenshot_dump(g_settings.screenshot_directory,
               data + (height - 1) * (pitch >> 1), 
               width, height, -pitch, false);
      }
#endif
   }

   return 0;
}

int rgui_input_postprocess(void *data, uint64_t old_state)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   bool quit = false;
   bool resize = false;
   unsigned width;
   unsigned height;
   unsigned frame_count;
   int ret = 0;

   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;

   if ((rgui->trigger_state & (1ULL << DEVICE_NAV_MENU)) &&
      g_extern.main_is_init)
   {
      if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_INGAME))
         g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_INGAME_EXIT);

      g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);

      ret = -1;
   }

   frame_count = 0;
   device_ptr->ctx_driver->check_window(&quit, &resize, &width, &height, frame_count);

   if (quit)
   {
      g_extern.lifecycle_mode_state |= (1ULL << MODE_EXIT);
      ret = -1;
   }

   if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_INGAME_EXIT) &&
         g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_INGAME))
   {
      menu_stack_pop(rgui->menu_type);
      g_extern.lifecycle_mode_state &= ~((1ULL << MODE_MENU_INGAME) | (1ULL << MODE_MENU_INGAME_EXIT));
   }

   return ret;
}

int rgui_iterate(rgui_handle_t *rgui)
{
   rgui->menu_type = menu_stack_enum_array[stack_idx - 1];

   if (rgui->need_refresh)
   {
      menu_stack_push(INGAME_MENU, false);
      rgui->need_refresh = false;
   }

#ifdef HAVE_OSKUTIL
   if (rgui->osk_init != NULL)
   {
      if (rgui->osk_init(rgui))
         rgui->osk_init = NULL;
   }

   if (rgui->osk_callback != NULL)
   {
      if (rgui->osk_callback(rgui))
         rgui->osk_callback = NULL;
   }
#endif

   switch(rgui->menu_type)
   {
      case INGAME_MENU_RESIZE:
         return ingame_menu_resize(rgui, rgui->trigger_state);
      case INGAME_MENU_CORE_OPTIONS:
         return ingame_menu_core_options(rgui, rgui->trigger_state);
      case INGAME_MENU_LOAD_GAME_HISTORY:
         return ingame_menu_history_options(rgui, rgui->trigger_state);
      case INGAME_MENU_SCREENSHOT:
         return ingame_menu_screenshot(rgui, rgui->trigger_state);
      case FILE_BROWSER_MENU:
         return select_rom(rgui, rgui->trigger_state);
      case LIBRETRO_CHOICE:
#ifdef HAVE_SHADER_MANAGER
      case CGP_CHOICE:
      case SHADER_CHOICE:
#endif
      case INPUT_PRESET_CHOICE:
      case BORDER_CHOICE:
         return select_file(rgui, rgui->trigger_state);
      case PATH_DEFAULT_ROM_DIR_CHOICE:
      case PATH_SAVESTATES_DIR_CHOICE:
      case PATH_SRAM_DIR_CHOICE:
#ifdef HAVE_XML
      case PATH_CHEATS_DIR_CHOICE:
#endif
      case PATH_SYSTEM_DIR_CHOICE:
         return select_directory(rgui, rgui->trigger_state);
      case GENERAL_VIDEO_MENU:
      case GENERAL_AUDIO_MENU:
      case EMU_GENERAL_MENU:
      case EMU_VIDEO_MENU:
      case EMU_AUDIO_MENU:
      case INGAME_MENU:
      case INGAME_MENU_VIDEO_OPTIONS:
      case INGAME_MENU_INPUT_OPTIONS:
      case INGAME_MENU_PATH_OPTIONS:
         return select_setting(rgui, rgui->trigger_state);
   }

   RARCH_WARN("Menu type %d not implemented, exiting...\n", rgui->menu_type);
   return -1;
}


rgui_handle_t *rgui_init(void)
{
   rgui_handle_t *rgui = (rgui_handle_t*)calloc(1, sizeof(*rgui));


   menu_texture = (struct texture_image*)calloc(1, sizeof(*menu_texture));
#ifdef HAVE_MENU_PANEL
   menu_panel = (struct texture_image*)calloc(1, sizeof(*menu_panel));
#endif

   rgui_init_textures();


#ifdef HAVE_OSKUTIL
   oskutil_params *osk = &g_extern.console.misc.oskutil_handle;
   oskutil_init(osk, 0);
#endif

   return rgui;
}

void rgui_free(rgui_handle_t *rgui)
{
#ifdef _XBOX1
#ifdef HAVE_MENU_PANEL
   if (menu_panel->vertex_buf)
   {
      menu_panel->vertex_buf->Release();
      menu_panel->vertex_buf = NULL;
   }
   if (menu_panel->pixels)
   {
      menu_panel->pixels->Release();
      menu_panel->pixels = NULL;
   }
#endif
   if (menu_texture->vertex_buf)
   {
      menu_texture->vertex_buf->Release();
      menu_texture->vertex_buf = NULL;
   }
   if (menu_texture->pixels)
   {
      menu_texture->pixels->Release();
      menu_texture->pixels = NULL;
   }
#else
#ifdef HAVE_MENU_PANEL
   if (menu_panel)
   {
      free(menu_panel->pixels);
      menu_panel->pixels = NULL;
   }
#endif

   if (menu_texture)
   {
      free(menu_texture->pixels);
      menu_texture->pixels = NULL;
   }
#endif
}

uint64_t rgui_input(void)
{
   uint64_t input_state = 0;

   for (unsigned i = 0; i < (DEVICE_NAV_LAST - 1); i++)
      input_state |= driver.input->input_state(driver.input_data, rmenu_nav_binds, 0,
            RETRO_DEVICE_JOYPAD, 0, i) ? (1ULL << i) : 0;

   input_state |= driver.input->key_pressed(driver.input_data, RARCH_MENU_TOGGLE) ? (1ULL << DEVICE_NAV_MENU) : 0;

   //set first button input frame as trigger
   rgui->trigger_state = input_state & ~rgui->old_input_state;

   bool keys_pressed = (input_state & (
            (1ULL << DEVICE_NAV_LEFT_ANALOG_L) |
            (1ULL << DEVICE_NAV_RIGHT_ANALOG_L) |
            (1ULL << DEVICE_NAV_UP_ANALOG_L) |
            (1ULL << DEVICE_NAV_DOWN_ANALOG_L) |
            (1ULL << DEVICE_NAV_LEFT_ANALOG_R) |
            (1ULL << DEVICE_NAV_RIGHT_ANALOG_R) |
            (1ULL << DEVICE_NAV_UP_ANALOG_R) |
            (1ULL << DEVICE_NAV_DOWN_ANALOG_R)));
   bool shoulder_buttons_pressed = (input_state & (
            (1ULL << DEVICE_NAV_L2) |
            (1ULL << DEVICE_NAV_R2)
            ));
   rgui->do_held = (keys_pressed || shoulder_buttons_pressed) &&
   !(input_state & DEVICE_NAV_MENU);

   return input_state;
}
