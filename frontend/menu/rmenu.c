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
#include "file_browser.h"

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

struct texture_image *menu_texture;
#ifdef HAVE_MENU_PANEL
struct texture_image *menu_panel;
#endif


#if defined(_XBOX1)
const char drive_mappings[][32] = {
   "C:\\",
   "D:\\",
   "E:\\",
   "F:\\",
   "G:\\"
};

size_t drive_mapping_idx = 2;

#define TICKER_LABEL_CHARS_MAX_PER_LINE 16

#elif defined(__CELLOS_LV2__)
const char drive_mappings[][32] = {
   "/app_home/",
   "/dev_hdd0/",
   "/dev_hdd1/",
   "/host_root/"
};

size_t drive_mapping_idx = 1;

#define TICKER_LABEL_CHARS_MAX_PER_LINE 25
#endif

unsigned settings_lut[SETTING_LAST_LAST] = {0};

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
static unsigned shader_choice_set_shader_slot = 0;
static unsigned setting_page_number = 0;

static void menu_stack_pop(unsigned menu_type)
{
   switch(menu_type)
   {
      case LIBRETRO_CHOICE:
      case INGAME_MENU_CORE_OPTIONS:
      case INGAME_MENU_LOAD_GAME_HISTORY:
      case INGAME_MENU_CUSTOM_RATIO:
      case INGAME_MENU_SCREENSHOT:
         rgui->frame_buf_show = true;
         break;
      case INGAME_MENU_SETTINGS:
      case INGAME_MENU_VIDEO_OPTIONS:
      case INGAME_MENU_AUDIO_OPTIONS:
      case INGAME_MENU_INPUT_OPTIONS:
      case INGAME_MENU_PATH_OPTIONS:
         rgui->selection_ptr = FIRST_INGAME_MENU_SETTING;
         rgui->frame_buf_show = true;
         break;
      case INGAME_MENU_SHADER_OPTIONS:
         rgui->selection_ptr = FIRST_VIDEO_SETTING;
         break;
#ifdef HAVE_SHADER_MANAGER
      case CGP_CHOICE:
         rgui->selection_ptr = FIRST_SHADERMAN_SETTING;
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
         rgui->selection_ptr = FIRST_INGAME_MENU_SETTING;
         break;
      case INGAME_MENU_VIDEO_OPTIONS:
         rgui->selection_ptr = FIRST_VIDEO_SETTING;
         break;
#ifdef HAVE_SHADER_MANAGER
      case INGAME_MENU_SHADER_OPTIONS:
         rgui->selection_ptr = FIRST_SHADERMAN_SETTING;
         break;
#endif
      case INGAME_MENU_AUDIO_OPTIONS:
         rgui->selection_ptr = FIRST_AUDIO_SETTING;
         break;
      case INGAME_MENU_INPUT_OPTIONS:
         rgui->selection_ptr = FIRST_CONTROLS_SETTING_PAGE_1;
         break;
      case INGAME_MENU_PATH_OPTIONS:
         rgui->selection_ptr = FIRST_PATH_SETTING;
         break;
      case INGAME_MENU_SETTINGS:
         rgui->selection_ptr = FIRST_SETTING;
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

uint8_t first_setting = FIRST_SETTING;
uint8_t max_settings = 0;
uint8_t items_pages[SETTING_LAST_LAST] = {0};
static unsigned hist_opt_selected = 0;
static unsigned core_opt_selected = 0;

#include "rmenudisp.c"

static int select_file(void *data, uint64_t action)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   
   char path[PATH_MAX];
   bool pop_menu_stack = false;

   switch (action)
   {
      case RGUI_ACTION_OK:
         if (filebrowser_iterate(rgui->browser, FILEBROWSER_ACTION_PATH_ISDIR))
         {
            if (!filebrowser_iterate(rgui->browser, FILEBROWSER_ACTION_OK))
            {
               RARCH_ERR("Failed to open directory.\n");
               msg_queue_push(g_extern.msg_queue, "ERROR - Failed to open directory.", 1, 180);
            }
         }
         else
         {
            strlcpy(path, rgui->browser->current_dir.path, sizeof(path));

            switch(rgui->menu_type)
            {
#ifdef HAVE_SHADER_MANAGER
               case CONFIG_CHOICE:
                  menu_replace_config(path);
                  break;
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
               case LIBRETRO_CHOICE:
                  rarch_environment_cb(RETRO_ENVIRONMENT_SET_LIBRETRO_PATH, (void*)path);
                  g_extern.lifecycle_mode_state |= (1ULL << MODE_EXITSPAWN);
                  return -1;
               case FILE_BROWSER_MENU:
                  strlcpy(g_extern.fullpath, path, sizeof(g_extern.fullpath));
                  g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME);
                  return -1;
            }

            pop_menu_stack = true;
         }
         break;
      case RGUI_ACTION_CANCEL:
         if (rgui->menu_type == LIBRETRO_CHOICE)
            pop_menu_stack = true;
         else
         {
            char tmp_str[PATH_MAX];
            fill_pathname_parent_dir(tmp_str, rgui->browser->current_dir.directory_path, sizeof(tmp_str));

            if (tmp_str[0] == '\0')
               pop_menu_stack = true;
         }
         break;
      case RGUI_ACTION_MAPPING_PREVIOUS:
         if (rgui->menu_type == FILE_BROWSER_MENU)
         {
            const char * drive_map = menu_drive_mapping_previous();
            if (drive_map != NULL)
               filebrowser_set_root_and_ext(rgui->browser, rgui->browser->current_dir.extensions, drive_map);
         }
         break;
      case RGUI_ACTION_MAPPING_NEXT:
         if (rgui->menu_type == FILE_BROWSER_MENU)
         {
            const char * drive_map = menu_drive_mapping_next();
            if (drive_map != NULL)
               filebrowser_set_root_and_ext(rgui->browser, rgui->browser->current_dir.extensions, drive_map);
         }
         break;
   }

   if (pop_menu_stack)
      menu_stack_pop(rgui->menu_type);

   return 0;
}

static int select_directory(void *data, uint64_t action)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   char path[PATH_MAX];
   (void)path;
   bool ret = true;

   bool is_dir = filebrowser_iterate(rgui->browser, FILEBROWSER_ACTION_PATH_ISDIR);
   bool pop_menu_stack = false;

   switch (action)
   {
      case RGUI_ACTION_OK:
#if 1
         if (is_dir)
            ret = filebrowser_iterate(rgui->browser, FILEBROWSER_ACTION_OK);
#else
         /* TODO - extra conditional needed here to recognize if user pressed <Use this directory> entry */
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
         break;
#endif
      case RGUI_ACTION_CANCEL:
         {
            char tmp_str[PATH_MAX];
            fill_pathname_parent_dir(tmp_str, rgui->browser->current_dir.directory_path, sizeof(tmp_str));

            if (tmp_str[0] == '\0')
               pop_menu_stack = true;
         }
         break;
   }

   if (pop_menu_stack)
      menu_stack_pop(rgui->menu_type);

   if (!ret)
      msg_queue_push(g_extern.msg_queue, "ERROR - Failed to open directory.", 1, 180);

   return 0;
}

static void set_keybind_digital(unsigned default_retro_joypad_id, uint64_t action)
{
   if (!driver.input->set_keybinds)
      return;

   unsigned keybind_action = KEYBINDS_ACTION_NONE;

   switch (action)
   {
      case RGUI_ACTION_START:
         keybind_action = (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BIND);
         break;
   }

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
      int num = wcstombs(tmp_str, rgui->oskutil_handle.text_buf, sizeof(tmp_str));
      tmp_str[num] = 0;
      strlcpy(g_settings.audio.device, tmp_str, sizeof(g_settings.audio.device));
      goto do_exit;
   }
   else if (g_extern.lifecycle_mode_state & (1ULL << MODE_OSK_ENTRY_FAIL))
      goto do_exit;

   return false;

do_exit:
   g_extern.lifecycle_mode_state &= ~((1ULL << MODE_OSK_ENTRY_SUCCESS) |
         (1ULL << MODE_OSK_ENTRY_FAIL));
   return true;
}

static bool osk_callback_enter_rsound_init(void *data)
{
   oskutil_write_initial_message(&rgui->oskutil_handle, L"192.168.1.1");
   oskutil_write_message(&rgui->oskutil_handle, L"Enter IP address for the RSound Server.");
   oskutil_start(&rgui->oskutil_handle);

   return true;
}

static bool osk_callback_enter_filename(void *data)
{
   if (g_extern.lifecycle_mode_state & (1ULL << MODE_OSK_ENTRY_SUCCESS))
   {
      RARCH_LOG("OSK - Applying input data.\n");
      char tmp_str[256];
      char filepath[PATH_MAX];
      int num = wcstombs(tmp_str, rgui->oskutil_handle.text_buf, sizeof(tmp_str));
      tmp_str[num] = 0;

      switch(rgui->osk_param)
      {
         case CONFIG_FILE:
            break;
         case SHADER_PRESET_FILE:
            {
               snprintf(filepath, sizeof(filepath), "%s/%s.cgp", g_settings.video.shader_dir, tmp_str);
               RARCH_LOG("[osk_callback_enter_filename]: filepath is: %s.\n", filepath);
               config_file_t *conf = config_file_new(NULL);
               if (!conf)
                  return false;
               gfx_shader_write_conf_cgp(conf, &rgui->shader);
               config_file_write(conf, filepath);
               config_file_free(conf);
            }
            break;
      }

      goto do_exit;
   }
   else if (g_extern.lifecycle_mode_state & (1ULL << MODE_OSK_ENTRY_FAIL))
      goto do_exit;

   return false;
do_exit:
   g_extern.lifecycle_mode_state &= ~((1ULL << MODE_OSK_ENTRY_SUCCESS) |
         (1ULL << MODE_OSK_ENTRY_FAIL));
   return true;
}

static bool osk_callback_enter_filename_init(void *data)
{
   oskutil_write_initial_message(&rgui->oskutil_handle, L"example");
   oskutil_write_message(&rgui->oskutil_handle, L"Enter filename for preset");
   oskutil_start(&rgui->oskutil_handle);

   return true;
}

#endif
#endif

void rgui_init_textures(void)
{
#ifdef HAVE_MENU_PANEL
   texture_image_load("D:\\Media\\menuMainRomSelectPanel.png", menu_panel);
#endif
   texture_image_load(g_extern.menu_texture_path, menu_texture);

   if (driver.video_poke && driver.video_poke->set_texture_frame)
      driver.video_poke->set_texture_frame(driver.video_data, menu_texture->pixels,
            true, menu_texture->width, menu_texture->height, 1.0f);
}

static int set_setting_action(uint8_t menu_type, unsigned switchvalue, uint64_t action_ori)
{
   unsigned action = (unsigned)action_ori;

   switch (switchvalue)
   {
      case SETTING_ASPECT_RATIO:
      case SETTING_HW_TEXTURE_FILTER:
      case SETTING_REFRESH_RATE:
      case SETTING_VIDEO_VSYNC:
      case SETTING_VIDEO_CROP_OVERSCAN:
      case SETTING_EMU_SHOW_DEBUG_INFO_MSG:
      case SETTING_REWIND_ENABLED:
      case SETTING_REWIND_GRANULARITY:
      case SETTING_EMU_AUDIO_MUTE:
      case SETTING_AUDIO_CONTROL_RATE_DELTA:
      case SETTING_CONTROLS_NUMBER:
      case SETTING_CONTROLS_BIND_DEVICE_TYPE:
      case INGAME_MENU_LOAD_STATE:
      case INGAME_MENU_SAVE_STATE:
      case SETTING_ROTATION:
      case INGAME_MENU_RETURN_TO_GAME:
#ifdef HAVE_SHADER_MANAGER
      case SHADERMAN_APPLY_CHANGES:
      case SHADERMAN_SHADER_PASSES:
#endif
      case INGAME_MENU_SAVE_CONFIG:
      case INGAME_MENU_QUIT_RETROARCH:
#ifdef __CELLOS_LV2__
      case SETTING_CHANGE_RESOLUTION:
#endif
         return menu_set_settings(settings_lut[switchvalue], action);
#ifdef __CELLOS_LV2__
      case SETTING_PAL60_MODE:
         switch (action)
         {
            case RGUI_ACTION_LEFT:
            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_PAL_ENABLE))
               {
                  if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE))
                  {
                     g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE);
                  }
                  else
                  {
                     g_extern.lifecycle_mode_state |= (1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE);
                  }

                  driver.video->restart();
                  rgui_init_textures();
               }
               break;
            case RGUI_ACTION_START:
               if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_PAL_ENABLE))
               {
                  g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE);

                  driver.video->restart();
                  rgui_init_textures();
               }
               break;
         }
         break;
#endif
      case INGAME_MENU_CONFIG:
         switch (action)
         {
            case RGUI_ACTION_OK:
               menu_stack_push(CONFIG_CHOICE, true);
               filebrowser_set_root_and_ext(rgui->browser, "cfg", default_paths.core_dir);
               break;
         }
         break;
      case SETTING_EMU_SKIN:
         switch (action)
         {
            case RGUI_ACTION_OK:
               menu_stack_push(BORDER_CHOICE, true);
               filebrowser_set_root_and_ext(rgui->browser, EXT_IMAGES, default_paths.border_dir);
               break;
            case RGUI_ACTION_START:
               if (!texture_image_load(default_paths.menu_border_file, menu_texture))
               {
                  RARCH_ERR("Failed to load texture image for menu.\n");
                  return false;
               }
               break;
         }
         break;
#ifdef _XBOX1
      case SETTING_FLICKER_FILTER:
         switch (action)
         {
            case RGUI_ACTION_LEFT:
               if (g_extern.console.screen.flicker_filter_index > 0)
                  g_extern.console.screen.flicker_filter_index--;
               break;
            case RGUI_ACTION_RIGHT:
               if (g_extern.console.screen.flicker_filter_index < 5)
                  g_extern.console.screen.flicker_filter_index++;
               break;
            case RGUI_ACTION_START:
               g_extern.console.screen.flicker_filter_index = 0;
               break;
         }
         break;
      case SETTING_SOFT_DISPLAY_FILTER:
         switch (action)
         {
            case RGUI_ACTION_LEFT:
            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE))
                  g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);
               else
                  g_extern.lifecycle_mode_state |= (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);
               break;
            case RGUI_ACTION_START:
               g_extern.lifecycle_mode_state |= (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);
               break;
         }
         break;
#endif
      case SETTING_TRIPLE_BUFFERING:
         switch (action)
         {
            case RGUI_ACTION_LEFT:
            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               settings_set(1ULL << S_TRIPLE_BUFFERING);

               driver.video->restart();
               rgui_init_textures();
               break;
            case RGUI_ACTION_START:
               settings_set(1ULL << S_DEF_TRIPLE_BUFFERING);

               if (!(g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_TRIPLE_BUFFERING_ENABLE)))
               {
                  driver.video->restart();
                  rgui_init_textures();
               }
               break;
         }
         break;
      case SETTING_SOUND_MODE:
         switch (action)
         {
            case RGUI_ACTION_LEFT:
               if (g_extern.console.sound.mode != SOUND_MODE_NORMAL)
                  g_extern.console.sound.mode--;
               break;
            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               if (g_extern.console.sound.mode < (SOUND_MODE_LAST-1))
                  g_extern.console.sound.mode++;
               break;
            case RGUI_ACTION_UP:
            case RGUI_ACTION_DOWN:
#ifdef HAVE_RSOUND
               if (g_extern.console.sound.mode != SOUND_MODE_RSOUND)
                  rarch_rsound_stop();
               else
                  rarch_rsound_start(g_settings.audio.device);
#endif
               break;
            case RGUI_ACTION_START:
               g_extern.console.sound.mode = SOUND_MODE_NORMAL;
#ifdef HAVE_RSOUND
               rarch_rsound_stop();
#endif
               break;
         }
         break;
#ifdef HAVE_RSOUND
      case SETTING_RSOUND_SERVER_IP_ADDRESS:
         switch (action)
         {
            case RGUI_ACTION_LEFT:
            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
#ifdef HAVE_OSKUTIL
               rgui->osk_init = osk_callback_enter_rsound_init;
               rgui->osk_callback = osk_callback_enter_rsound;
#endif
               break;
            case RGUI_ACTION_START:
               strlcpy(g_settings.audio.device, "0.0.0.0", sizeof(g_settings.audio.device));
               break;
         }
         break;
#endif
      case SETTING_EMU_SHOW_INFO_MSG:
         switch (action)
         {
            case RGUI_ACTION_LEFT:
            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               settings_set(1ULL << S_INFO_MSG_TOGGLE);
               break;
            case RGUI_ACTION_START:
               settings_set(1ULL << S_DEF_INFO_MSG);
               break;
         }
         break;
#ifdef _XBOX1
      case SETTING_EMU_AUDIO_SOUND_VOLUME_LEVEL:
         switch (action)
         {
            case RGUI_ACTION_LEFT:
            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               g_extern.console.sound.volume_level = !g_extern.console.sound.volume_level;
               if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
                  msg_queue_push(g_extern.msg_queue, "INFO - You need to restart RetroArch.", 1, 180);
               break;
            case RGUI_ACTION_START:
               g_extern.console.sound.volume_level = 0;
               if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
                  msg_queue_push(g_extern.msg_queue, "INFO - You need to restart RetroArch.", 1, 180);
               break;
         }
         break;
#endif
      case SETTING_ENABLE_CUSTOM_BGM:
         switch (action)
         {
            case RGUI_ACTION_LEFT:
            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
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
               break;
            case RGUI_ACTION_START:
#if (CELL_SDK_VERSION > 0x340000)
               g_extern.lifecycle_mode_state |= (1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE);
#endif
               break;
         }
         break;
      case SETTING_PATH_DEFAULT_ROM_DIRECTORY:
         switch (action)
         {
            case RGUI_ACTION_OK:
               menu_stack_push(PATH_DEFAULT_ROM_DIR_CHOICE, true);
               filebrowser_set_root_and_ext(rgui->browser, "empty", default_paths.filesystem_root_dir);
               break;
            case RGUI_ACTION_START:
               strlcpy(g_settings.rgui_browser_directory,
                     default_paths.filesystem_root_dir, sizeof(g_settings.rgui_browser_directory));
               break;
         }
         break;
      case SETTING_PATH_SAVESTATES_DIRECTORY:
         switch (action)
         {
            case RGUI_ACTION_OK:
               menu_stack_push(PATH_SAVESTATES_DIR_CHOICE, true);
               filebrowser_set_root_and_ext(rgui->browser, "empty", default_paths.filesystem_root_dir);
               break;
            case RGUI_ACTION_START:
               strlcpy(g_extern.savestate_dir, default_paths.savestate_dir, sizeof(g_extern.savestate_dir));
               break;
         }
         break;
      case SETTING_PATH_SRAM_DIRECTORY:
         switch (action)
         {
            case RGUI_ACTION_OK:
               menu_stack_push(PATH_SRAM_DIR_CHOICE, true);
               filebrowser_set_root_and_ext(rgui->browser, "empty", default_paths.filesystem_root_dir);
               break;
            case RGUI_ACTION_START:
               strlcpy(g_extern.savefile_dir, default_paths.sram_dir, sizeof(g_extern.savefile_dir));
               break;
         }
         break;
#ifdef HAVE_XML
      case SETTING_PATH_CHEATS:
         switch (action)
         {
            case RGUI_ACTION_OK:
               menu_stack_push(PATH_CHEATS_DIR_CHOICE, true);
               filebrowser_set_root_and_ext(rmenu->browser, "empty", default_paths.filesystem_root_dir);
               break;
            case RGUI_ACTION_START:
               strlcpy(g_settings.cheat_database, default_paths.port_dir, sizeof(g_settings.cheat_database));
               break;
         }
         break;
#endif
      case SETTING_PATH_SYSTEM:
         switch (action)
         {
            case RGUI_ACTION_OK:
               menu_stack_push(PATH_SYSTEM_DIR_CHOICE, true);
               filebrowser_set_root_and_ext(rgui->browser, "empty", default_paths.system_dir);
               break;
            case RGUI_ACTION_START:
               strlcpy(g_settings.system_directory, default_paths.system_dir, sizeof(g_settings.system_directory));
               break;
         }
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_UP:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_UP, action);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_DOWN:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_DOWN, action);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_LEFT:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_LEFT, action);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_RIGHT:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_RIGHT, action);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_A:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_A, action);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_B:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_B, action);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_X:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_X, action);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_Y:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_Y, action);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_SELECT:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_SELECT, action);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_START:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_START, action);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_L, action);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_R, action);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L2:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_L2, action);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R2:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_R2, action);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L3:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_L3, action);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R3:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_R3, action);
         break;
      case SETTING_CONTROLS_DEFAULT_ALL:
         switch (action)
         {
            case RGUI_ACTION_LEFT:
            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
            case RGUI_ACTION_START:
               if (driver.input->set_keybinds)
                  driver.input->set_keybinds(driver.input_data, g_settings.input.device[rgui->current_pad], rgui->current_pad, 0,
                        (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS));
               break;
         }
         break;
      case SETTING_CUSTOM_VIEWPORT:
         if (action == RGUI_ACTION_OK)
            menu_stack_push(INGAME_MENU_CUSTOM_RATIO, false);
         break;
      case INGAME_MENU_CORE_OPTIONS_MODE:
         if (action == RGUI_ACTION_OK)
            menu_stack_push(INGAME_MENU_CORE_OPTIONS, false);
         break;
      case INGAME_MENU_LOAD_GAME_HISTORY_MODE:
         if (action == RGUI_ACTION_OK)
            menu_stack_push(INGAME_MENU_LOAD_GAME_HISTORY, false);
         break;
      case INGAME_MENU_SCREENSHOT_MODE:
         if (action == RGUI_ACTION_OK)
            menu_stack_push(INGAME_MENU_SCREENSHOT, false);
         break;
      case INGAME_MENU_CHANGE_GAME:
         if (action == RGUI_ACTION_OK)
            menu_stack_push(FILE_BROWSER_MENU, false);
         break;
      case INGAME_MENU_SETTINGS_MODE:
         if (action == RGUI_ACTION_OK)
            menu_stack_push(INGAME_MENU_SETTINGS, false);
         break;
      case INGAME_MENU_RESET:
         if (action == RGUI_ACTION_OK)
         {
            rarch_game_reset();
            g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
            return -1;
         }
         break;
      case INGAME_MENU_CHANGE_LIBRETRO_CORE:
         if (action == RGUI_ACTION_OK)
         {
            menu_stack_push(LIBRETRO_CHOICE, true);
            filebrowser_set_root_and_ext(rgui->browser, EXT_EXECUTABLES, default_paths.core_dir);
         }
         break;
#ifdef HAVE_MULTIMAN
      case INGAME_MENU_RETURN_TO_MULTIMAN:
         if (action == RGUI_ACTION_OK)
         {
            g_extern.lifecycle_mode_state &= ~(1ULL << MODE_GAME);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_EXITSPAWN);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_EXITSPAWN_MULTIMAN);
            return -1;
         }
         break;
#endif
      case INGAME_MENU_VIDEO_OPTIONS_MODE:
         if (action == RGUI_ACTION_OK)
            menu_stack_push(INGAME_MENU_VIDEO_OPTIONS, false);
         break;
#ifdef HAVE_SHADER_MANAGER
      case INGAME_MENU_SHADER_OPTIONS_MODE:
         if (action == RGUI_ACTION_OK)
            menu_stack_push(INGAME_MENU_SHADER_OPTIONS, false);
         break;
#endif
      case INGAME_MENU_AUDIO_OPTIONS_MODE:
         if (action == RGUI_ACTION_OK)
            menu_stack_push(INGAME_MENU_AUDIO_OPTIONS, false);
         break;
      case INGAME_MENU_INPUT_OPTIONS_MODE:
         if (action == RGUI_ACTION_OK)
            menu_stack_push(INGAME_MENU_INPUT_OPTIONS, false);
         break;
      case INGAME_MENU_PATH_OPTIONS_MODE:
         if (action == RGUI_ACTION_OK)
            menu_stack_push(INGAME_MENU_PATH_OPTIONS, false);
         break;
#ifdef HAVE_SHADER_MANAGER
      case SHADERMAN_LOAD_CGP:
         switch (action)
         {
            case RGUI_ACTION_OK:
               menu_stack_push(CGP_CHOICE, true);
               filebrowser_set_root_and_ext(rgui->browser, EXT_CGP_PRESETS, g_settings.video.shader_dir);
               break;
            case RGUI_ACTION_START:
               g_settings.video.shader_path[0] = '\0';
               video_set_shader_func(RARCH_SHADER_CG, NULL);
               g_settings.video.shader_enable = false;
               break;
         }
         break;
      case SHADERMAN_SAVE_CGP:
#ifdef HAVE_OSKUTIL
         switch (action)
         {
            case RGUI_ACTION_OK:
               rgui->osk_param = SHADER_PRESET_FILE;
               rgui->osk_init = osk_callback_enter_filename_init;
               rgui->osk_callback = osk_callback_enter_filename;
               break;
         }
#endif
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

            switch (action)
            {
               case RGUI_ACTION_LEFT:
               case RGUI_ACTION_RIGHT:
               case RGUI_ACTION_OK:
                  shader_choice_set_shader_slot = index;
                  menu_stack_push(SHADER_CHOICE, true);
                  filebrowser_set_root_and_ext(rgui->browser, EXT_SHADERS, g_settings.video.shader_dir);
                  break;
               case RGUI_ACTION_START:
                  *pass->source.cg = '\0';
                  break;
            }
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

            switch (action)
            {
               case RGUI_ACTION_RIGHT:
               case RGUI_ACTION_LEFT:
               case RGUI_ACTION_OK:
                  {
                     unsigned delta = (action == RGUI_ACTION_LEFT) ? 2 : 1;
                     rgui->shader.pass[index].filter = (enum gfx_filter_type)((rgui->shader.pass[index].filter + delta) % 3);
                  }
                  break;
               case RGUI_ACTION_START:
                  rgui->shader.pass[index].filter = RARCH_FILTER_UNSPEC;
                  break;
            }
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

            switch (action)
            {
               case RGUI_ACTION_LEFT:
                  if (scale)
                  {
                     rgui->shader.pass[index].fbo.scale_x = rgui->shader.pass[index].fbo.scale_y = scale - 1;
                     rgui->shader.pass[index].fbo.valid = scale - 1;
                  }
                  break;
               case RGUI_ACTION_RIGHT:
               case RGUI_ACTION_OK:
                  if (scale < 5)
                  {
                     rgui->shader.pass[index].fbo.scale_x = rgui->shader.pass[index].fbo.scale_y = scale + 1;
                     rgui->shader.pass[index].fbo.valid = scale + 1;
                  }
                  break;
               case RGUI_ACTION_START:
                  rgui->shader.pass[index].fbo.scale_x = rgui->shader.pass[index].fbo.scale_y = 0;
                  rgui->shader.pass[index].fbo.valid = false;
                  break;
            }
         }
         break;
#endif
      default:
         break;
   }

   return 0;
}

static int select_setting(void *data, uint64_t action)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   int ret = 0;

   switch (rgui->menu_type)
   {
      case INGAME_MENU:
         first_setting = FIRST_INGAME_MENU_SETTING;
         max_settings = MAX_NO_OF_INGAME_MENU_SETTINGS;
         break;
      case INGAME_MENU_SETTINGS:
         first_setting = FIRST_SETTING;
         max_settings = MAX_NO_OF_SETTINGS;
         break;
      case INGAME_MENU_INPUT_OPTIONS:
         first_setting = FIRST_CONTROLS_SETTING_PAGE_1;
         max_settings = MAX_NO_OF_CONTROLS_SETTINGS;
         break;
      case INGAME_MENU_PATH_OPTIONS:
         first_setting = FIRST_PATH_SETTING;
         max_settings = MAX_NO_OF_PATH_SETTINGS;
         break;
      case INGAME_MENU_AUDIO_OPTIONS:
         first_setting = FIRST_AUDIO_SETTING;
         max_settings = MAX_NO_OF_AUDIO_SETTINGS;
         break;
      case INGAME_MENU_VIDEO_OPTIONS:
         first_setting = FIRST_VIDEO_SETTING;
         max_settings = MAX_NO_OF_VIDEO_SETTINGS;
         break;
#ifdef HAVE_SHADER_MANAGER
      case INGAME_MENU_SHADER_OPTIONS:
         first_setting = FIRST_SHADERMAN_SETTING;
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
         break;
#endif
   }

   switch (action)
   {
      case RGUI_ACTION_UP:
         if (rgui->selection_ptr == first_setting)
            rgui->selection_ptr = max_settings-1;
         else
            rgui->selection_ptr--;

         if (items_pages[rgui->selection_ptr] != setting_page_number)
            setting_page_number = items_pages[rgui->selection_ptr];
         break;
      case RGUI_ACTION_DOWN:
         rgui->selection_ptr++;

         if (rgui->selection_ptr >= max_settings)
            rgui->selection_ptr = first_setting; 
         if (items_pages[rgui->selection_ptr] != setting_page_number)
            setting_page_number = items_pages[rgui->selection_ptr];
         break;
      case RGUI_ACTION_CANCEL:
         if (rgui->menu_type == INGAME_MENU)
         {
            g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
            return -1;
         }

         menu_stack_pop(rgui->menu_type);
         break;
      case RGUI_ACTION_LEFT:
      case RGUI_ACTION_RIGHT:
      case RGUI_ACTION_OK:
         ret = set_setting_action(rgui->menu_type, rgui->selection_ptr, action);
         break;
   }

   return ret;
}

static int ingame_menu_resize(void *data, uint64_t action)
{
   (void)data;
   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;

   g_settings.video.aspect_ratio_idx = ASPECT_RATIO_CUSTOM;
   
   if (driver.video_poke->set_aspect_ratio)
      driver.video_poke->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);

   switch (action)
   {
      case RGUI_ACTION_LEFT:
         g_extern.console.screen.viewports.custom_vp.x -= 1;
         break;
      case RGUI_ACTION_RIGHT:
         g_extern.console.screen.viewports.custom_vp.x += 1;
         break;
      case RGUI_ACTION_UP:
         g_extern.console.screen.viewports.custom_vp.y -= 1;
         break;
      case RGUI_ACTION_DOWN:
         g_extern.console.screen.viewports.custom_vp.y += 1;
         break;
      case RGUI_ACTION_SCROLL_UP:
         g_extern.console.screen.viewports.custom_vp.width -= 1;
         break;
      case RGUI_ACTION_SCROLL_DOWN:
         g_extern.console.screen.viewports.custom_vp.width += 1;
         break;
      case RGUI_ACTION_START:
         g_extern.console.screen.viewports.custom_vp.x = 0;
         g_extern.console.screen.viewports.custom_vp.y = 0;
         g_extern.console.screen.viewports.custom_vp.width = device_ptr->win_width;
         g_extern.console.screen.viewports.custom_vp.height = device_ptr->win_height;
         break;
      case RGUI_ACTION_CANCEL:
         menu_stack_pop(rgui->menu_type);
         break;
   }

   return 0;
}

static int ingame_menu_history_options(void *data, uint64_t action)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   switch (action)
   {
      case RGUI_ACTION_OK:
      case RGUI_ACTION_START:
         load_menu_game_history(hist_opt_selected);
         return -1;
      case RGUI_ACTION_UP:
         if (hist_opt_selected == 0)
            hist_opt_selected = rom_history_size(rgui->history) - 1;
         else
            hist_opt_selected--;
         break;
      case RGUI_ACTION_DOWN:
         hist_opt_selected++;

         if (hist_opt_selected >= rom_history_size(rgui->history))
            hist_opt_selected = 0; 
         break;
      case RGUI_ACTION_LEFT:
         if (hist_opt_selected <= 5)
            hist_opt_selected = 0;
         else
            hist_opt_selected -= 5;
         break;
      case FILEBROWSER_ACTION_RIGHT:
         hist_opt_selected = (min(hist_opt_selected + 5, rom_history_size(rgui->history)-1));
         break;
      case RGUI_ACTION_CANCEL:
         menu_stack_pop(rgui->menu_type);
         break;
   }

   return 0;
}

static int ingame_menu_core_options(void *data, uint64_t action)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   switch (action)
   {
      case RGUI_ACTION_LEFT:
         core_option_prev(g_extern.system.core_options, core_opt_selected);
         break;
      case RGUI_ACTION_RIGHT:
      case RGUI_ACTION_OK:
         core_option_next(g_extern.system.core_options, core_opt_selected);
         break;
      case RGUI_ACTION_START:
         core_option_set_default(g_extern.system.core_options, core_opt_selected);
         break;
      case RGUI_ACTION_UP:
         if (core_opt_selected == 0)
            core_opt_selected = core_option_size(g_extern.system.core_options) - 1;
         else
            core_opt_selected--;
         break;
      case RGUI_ACTION_DOWN:
         core_opt_selected++;

         if (core_opt_selected >= core_option_size(g_extern.system.core_options))
            core_opt_selected = 0; 
         break;
      case RGUI_ACTION_CANCEL:
         menu_stack_pop(rgui->menu_type);
         break;
   }

   return 0;
}

static int ingame_menu_screenshot(void *data, uint64_t action)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   rgui->frame_buf_show = false;

   switch (action)
   {
      case RGUI_ACTION_CANCEL:
         menu_stack_pop(rgui->menu_type);
         break;
#ifdef HAVE_SCREENSHOTS
      case RGUI_ACTION_OK:
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
         break;
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

   if ((rgui->trigger_state & (1ULL << RARCH_MENU_TOGGLE)) &&
      g_extern.main_is_init)
   {
      g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
      ret = -1;
   }

   frame_count = 0;
   device_ptr->ctx_driver->check_window(&quit, &resize, &width, &height, frame_count);

   if (quit)
      ret = -1;


   return ret;
}

static int rgui_iterate(void *data, unsigned action)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
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

   filebrowser_update(rgui->browser, action, rgui->browser->current_dir.extensions);

   int ret = -1;

   switch(rgui->menu_type)
   {
      case INGAME_MENU_CUSTOM_RATIO:
         ret = ingame_menu_resize(rgui, action);
         break;
      case INGAME_MENU_CORE_OPTIONS:
         ret = ingame_menu_core_options(rgui, action);
         break;
      case INGAME_MENU_LOAD_GAME_HISTORY:
         ret = ingame_menu_history_options(rgui, action);
         break;
      case INGAME_MENU_SCREENSHOT:
         ret = ingame_menu_screenshot(rgui, action);
         break;
      case FILE_BROWSER_MENU:
      case LIBRETRO_CHOICE:
      case CONFIG_CHOICE:
#ifdef HAVE_SHADER_MANAGER
      case CGP_CHOICE:
      case SHADER_CHOICE:
#endif
      case BORDER_CHOICE:
         ret = select_file(rgui, action);
         break;
      case PATH_DEFAULT_ROM_DIR_CHOICE:
      case PATH_SAVESTATES_DIR_CHOICE:
      case PATH_SRAM_DIR_CHOICE:
#ifdef HAVE_XML
      case PATH_CHEATS_DIR_CHOICE:
#endif
      case PATH_SYSTEM_DIR_CHOICE:
         ret = select_directory(rgui, action);
         break;
      case INGAME_MENU:
      case INGAME_MENU_SETTINGS:
      case INGAME_MENU_VIDEO_OPTIONS:
      case INGAME_MENU_SHADER_OPTIONS:
      case INGAME_MENU_AUDIO_OPTIONS:
      case INGAME_MENU_INPUT_OPTIONS:
      case INGAME_MENU_PATH_OPTIONS:
         ret = select_setting(rgui, action);
         break;
   }

   if (ret == 0)
      render_text(rgui);

   if (ret == -1)
      menu_stack_pop(rgui->menu_type);

   return ret;
}

static void* rgui_init(void)
{
   rgui_handle_t *rgui = (rgui_handle_t*)calloc(1, sizeof(*rgui));


   menu_texture = (struct texture_image*)calloc(1, sizeof(*menu_texture));
#ifdef HAVE_MENU_PANEL
   menu_panel = (struct texture_image*)calloc(1, sizeof(*menu_panel));
#endif

   rgui_init_textures();


#ifdef HAVE_OSKUTIL
   oskutil_params *osk = &rgui->oskutil_handle;
   oskutil_init(osk, 0);
#endif
   
#ifdef __CELLOS_LV2__
   settings_lut[SETTING_CHANGE_RESOLUTION]          = RGUI_SETTINGS_VIDEO_RESOLUTION;
#endif
   settings_lut[SETTING_ASPECT_RATIO]               = RGUI_SETTINGS_VIDEO_ASPECT_RATIO;
   settings_lut[SETTING_HW_TEXTURE_FILTER]          = RGUI_SETTINGS_VIDEO_FILTER;
   settings_lut[SETTING_REFRESH_RATE]               = RGUI_SETTINGS_VIDEO_REFRESH_RATE_AUTO;
   settings_lut[SETTING_EMU_SHOW_DEBUG_INFO_MSG]    = RGUI_SETTINGS_DEBUG_TEXT;
   settings_lut[SETTING_VIDEO_VSYNC]                = RGUI_SETTINGS_VIDEO_VSYNC;
   settings_lut[SETTING_VIDEO_CROP_OVERSCAN]        = RGUI_SETTINGS_VIDEO_CROP_OVERSCAN;
   settings_lut[SETTING_REWIND_ENABLED]             = RGUI_SETTINGS_REWIND_ENABLE;
   settings_lut[SETTING_REWIND_GRANULARITY]         = RGUI_SETTINGS_REWIND_GRANULARITY;
   settings_lut[SETTING_EMU_AUDIO_MUTE]             = RGUI_SETTINGS_AUDIO_MUTE;
   settings_lut[SETTING_AUDIO_CONTROL_RATE_DELTA]   = RGUI_SETTINGS_AUDIO_CONTROL_RATE_DELTA;
   settings_lut[SETTING_CONTROLS_NUMBER]            = RGUI_SETTINGS_BIND_PLAYER;
   settings_lut[SETTING_CONTROLS_BIND_DEVICE_TYPE]  = RGUI_SETTINGS_BIND_DEVICE_TYPE;
   settings_lut[INGAME_MENU_LOAD_STATE]             = RGUI_SETTINGS_SAVESTATE_LOAD;
   settings_lut[INGAME_MENU_SAVE_STATE]             = RGUI_SETTINGS_SAVESTATE_SAVE;
   settings_lut[SETTING_ROTATION]                   = RGUI_SETTINGS_VIDEO_ROTATION;
   settings_lut[INGAME_MENU_RETURN_TO_GAME]         = RGUI_SETTINGS_RESUME_GAME;
#ifdef HAVE_SHADER_MANAGER
   settings_lut[SHADERMAN_APPLY_CHANGES]            = RGUI_SETTINGS_SHADER_APPLY;
   settings_lut[SHADERMAN_SHADER_PASSES]            = RGUI_SETTINGS_SHADER_PASSES;
#endif
   settings_lut[INGAME_MENU_SAVE_CONFIG]            = RGUI_SETTINGS_SAVE_CONFIG;
   settings_lut[INGAME_MENU_QUIT_RETROARCH]         = RGUI_SETTINGS_QUIT_RARCH;
   settings_lut[SETTING_PATH_SAVESTATES_DIRECTORY]  = RGUI_SAVESTATE_DIR_PATH;
   settings_lut[SETTING_PATH_SRAM_DIRECTORY]        = RGUI_SAVEFILE_DIR_PATH;
   settings_lut[SETTING_PATH_SYSTEM]                = RGUI_SYSTEM_DIR_PATH;
   settings_lut[SETTING_PATH_DEFAULT_ROM_DIRECTORY] = RGUI_BROWSER_DIR_PATH;
   settings_lut[INGAME_MENU_CONFIG]                 = RGUI_SETTINGS_CONFIG;

   return rgui;
}

static void rgui_free(void *data)
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

const menu_ctx_driver_t menu_ctx_rmenu = {
   rgui_iterate,
   rgui_init,
   rgui_free,
   "rmenu",
};
