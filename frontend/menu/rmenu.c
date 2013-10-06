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
#define CORE_MSG_POSITION_X FONT_SIZE
#define CORE_MSG_POSITION_Y (MSG_PREV_NEXT_Y_POSITION + 0.01f)
#define CORE_MSG_FONT_SIZE FONT_SIZE
#define MSG_QUEUE_X_POSITION POSITION_X
#define MSG_QUEUE_Y_POSITION (Y_POSITION - ((POSITION_Y_INCREMENT/2) * 7) + 10)
#define MSG_QUEUE_FONT_SIZE HARDCODE_FONT_SIZE
#define MSG_PREV_NEXT_Y_POSITION 24
#define CURRENT_PATH_Y_POSITION (POSITION_Y_START - ((POSITION_Y_INCREMENT/2)))
#define CURRENT_PATH_FONT_SIZE 21

#define FONT_SIZE 21 

#define NUM_ENTRY_PER_PAGE 15
#elif defined(__CELLOS_LV2__)
#define HARDCODE_FONT_SIZE 0.91f
#define FONT_SIZE_VARIABLE g_settings.video.font_size
#define POSITION_X 0.09f
#define POSITION_X_CENTER 0.5f
#define POSITION_Y_START 0.17f
#define POSITION_Y_INCREMENT 0.035f
#define POSITION_Y_BEGIN (POSITION_Y_START + POSITION_Y_INCREMENT)
#define COMMENT_POSITION_Y 0.82f
#define CORE_MSG_POSITION_X 0.3f
#define CORE_MSG_POSITION_Y 0.06f
#define CORE_MSG_FONT_SIZE COMMENT_POSITION_Y

#define MSG_QUEUE_X_POSITION g_settings.video.msg_pos_x
#define MSG_QUEUE_Y_POSITION 0.90f
#define MSG_QUEUE_FONT_SIZE 1.03f

#define MSG_PREV_NEXT_Y_POSITION 0.03f
#define CURRENT_PATH_Y_POSITION 0.15f
#define CURRENT_PATH_FONT_SIZE (g_settings.video.font_size)

#define NUM_ENTRY_PER_PAGE 18
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

static void render_text(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   font_params_t font_parms = {0};

   char msg[128];
   char label[64];

   font_parms.x = POSITION_X;
   font_parms.y = CURRENT_PATH_Y_POSITION;
   font_parms.scale = CURRENT_PATH_FONT_SIZE;
   font_parms.color = WHITE;

   switch(rgui->menu_type)
   {
#ifdef HAVE_SHADER_MANAGER
      case SHADER_CHOICE:
      case CGP_CHOICE:
#endif
      case BORDER_CHOICE:
      case LIBRETRO_CHOICE:
      case PATH_SAVESTATES_DIR_CHOICE:
      case PATH_DEFAULT_ROM_DIR_CHOICE:
#ifdef HAVE_XML
      case PATH_CHEATS_DIR_CHOICE:
#endif
      case PATH_SRAM_DIR_CHOICE:
      case PATH_SYSTEM_DIR_CHOICE:
      case FILE_BROWSER_MENU:
         if (rgui->menu_type == LIBRETRO_CHOICE)
            strlcpy(label, "CORE SELECTION", sizeof(label));
         else
            strlcpy(label, "PATH", sizeof(label));
         snprintf(msg, sizeof(msg), "%s %s", label, rgui->browser->current_dir.directory_path);
         break;
      case INGAME_MENU_LOAD_GAME_HISTORY:
         strlcpy(msg, "LOAD HISTORY", sizeof(msg));
         break;
      case INGAME_MENU:
         strlcpy(msg, "MENU", sizeof(msg));
         break;
      case INGAME_MENU_CORE_OPTIONS:
         strlcpy(msg, "CORE OPTIONS", sizeof(msg));
         break;
      case INGAME_MENU_VIDEO_OPTIONS:
      case INGAME_MENU_VIDEO_OPTIONS_MODE:
         strlcpy(msg, "VIDEO OPTIONS", sizeof(msg));
         break;
#ifdef HAVE_SHADER_MANAGER
      case INGAME_MENU_SHADER_OPTIONS:
      case INGAME_MENU_SHADER_OPTIONS_MODE:
         strlcpy(msg, "SHADER OPTIONS", sizeof(msg));
         break;
#endif
      case INGAME_MENU_INPUT_OPTIONS:
      case INGAME_MENU_INPUT_OPTIONS_MODE:
         strlcpy(msg, "INPUT OPTIONS", sizeof(msg));
         break;
      case INGAME_MENU_CUSTOM_RATIO:
         strlcpy(msg, "CUSTOM RATIO", sizeof(msg));
         break;
      case INGAME_MENU_SETTINGS:
      case INGAME_MENU_SETTINGS_MODE:
         strlcpy(msg, "MENU SETTINGS", sizeof(msg));
         break;
      case INGAME_MENU_AUDIO_OPTIONS:
      case INGAME_MENU_AUDIO_OPTIONS_MODE:
         strlcpy(msg, "AUDIO OPTIONS", sizeof(msg));
         break;
      case INGAME_MENU_PATH_OPTIONS:
      case INGAME_MENU_PATH_OPTIONS_MODE:
         strlcpy(msg, "PATH OPTIONS", sizeof(msg));
         break;
   }

   if (driver.video_poke->set_osd_msg && msg[0] != '\0')
      driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

   font_parms.x = POSITION_X;
   font_parms.y = CORE_MSG_POSITION_Y;
   font_parms.scale = CORE_MSG_FONT_SIZE;
   font_parms.color = WHITE;

   snprintf(msg, sizeof(msg), "%s - %s %s", PACKAGE_VERSION, rgui->info.library_name, rgui->info.library_version);

   if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

   bool render_browser = false;
   bool render_ingame_menu_resize = false;

   switch(rgui->menu_type)
   {
      case FILE_BROWSER_MENU:
      case LIBRETRO_CHOICE:
#ifdef HAVE_SHADER_MANAGER
      case CGP_CHOICE:
      case SHADER_CHOICE:
#endif
      case BORDER_CHOICE:
      case PATH_DEFAULT_ROM_DIR_CHOICE:
      case PATH_SAVESTATES_DIR_CHOICE:
      case PATH_SRAM_DIR_CHOICE:
#ifdef HAVE_XML
      case PATH_CHEATS_DIR_CHOICE:
#endif
      case PATH_SYSTEM_DIR_CHOICE:
         render_browser = true;
         break;
      case INGAME_MENU_CUSTOM_RATIO:
         render_ingame_menu_resize = true;
         break;
   }

   if (render_browser)
   {
      font_params_t font_parms = {0};
      font_parms.scale = FONT_SIZE_VARIABLE;

      if (rgui->browser->list->size)
      {
         unsigned file_count = rgui->browser->list->size;
         unsigned current_index = 0;
         unsigned page_number = 0;
         unsigned page_base = 0;
         unsigned i;
         float y_increment = POSITION_Y_START;

         current_index = rgui->browser->current_dir.ptr;
         page_number = current_index / NUM_ENTRY_PER_PAGE;
         page_base = page_number * NUM_ENTRY_PER_PAGE;


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
            font_parms.color = i == current_index ? YELLOW : rgui->browser->list->elems[i].attr.b ? GREEN : WHITE;

            if (driver.video_poke->set_osd_msg)
               driver.video_poke->set_osd_msg(driver.video_data, fname_tmp, &font_parms);
         }
      }
      else
      {
         char entry[128];
         font_parms.x = POSITION_X; 
         font_parms.y = POSITION_Y_START + POSITION_Y_INCREMENT;
         font_parms.color = WHITE;
         strlcpy(entry, "No entries available.", sizeof(entry));

         if (driver.video_poke->set_osd_msg)
            driver.video_poke->set_osd_msg(driver.video_data, entry, &font_parms);
      }
   }

   if (render_ingame_menu_resize && rgui->frame_buf_show)
   {
      char viewport[32];
      snprintf(viewport, sizeof(viewport), "Viewport X: #%d Y: %d (%dx%d)", g_extern.console.screen.viewports.custom_vp.x, g_extern.console.screen.viewports.custom_vp.y, g_extern.console.screen.viewports.custom_vp.width,
            g_extern.console.screen.viewports.custom_vp.height);

      font_parms.x = POSITION_X; 
      font_parms.y = POSITION_Y_BEGIN;
      font_parms.scale = HARDCODE_FONT_SIZE;
      font_parms.color = WHITE;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, viewport, &font_parms);
   }
}

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
      case SETTING_EMU_SHOW_DEBUG_INFO_MSG:
      case SETTING_REWIND_ENABLED:
      case SETTING_REWIND_GRANULARITY:
      case SETTING_EMU_AUDIO_MUTE:
      case SETTING_CONTROLS_NUMBER:
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
      case SETTING_EMU_SKIN:
         switch (action)
         {
            case RGUI_ACTION_LEFT:
            case RGUI_ACTION_RIGHT:
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
   static uint8_t first_setting = FIRST_SETTING;
   uint8_t items_pages[SETTING_LAST_LAST] = {0};
   uint8_t max_settings = 0;

   font_params_t font_parms = {0};

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

   float y_increment = POSITION_Y_START;
   uint8_t i = 0;
   uint8_t j = 0;
   uint8_t item_page = 0;

   for(i = first_setting; i < max_settings; i++)
   {
      char fname[PATH_MAX];
      char text[PATH_MAX];
      char setting_text[PATH_MAX];
      unsigned w;
      (void)fname;

      switch (i)
      {
#ifdef __CELLOS_LV2__
         case SETTING_CHANGE_RESOLUTION:
            strlcpy(text, "Resolution", sizeof(text));
            menu_set_settings_label(setting_text, sizeof(setting_text), &w, settings_lut[i]);
            break;
         case SETTING_PAL60_MODE:
            strlcpy(text, "PAL60 Mode", sizeof(text));
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE))
               strlcpy(setting_text, "ON", sizeof(setting_text));
            else
               strlcpy(setting_text, "OFF", sizeof(setting_text));
            break;
#endif
         case SETTING_EMU_SKIN:
            fill_pathname_base(fname, g_extern.menu_texture_path, sizeof(fname));
            strlcpy(text, "Menu Skin", sizeof(text));
            strlcpy(setting_text, fname, sizeof(setting_text));
            break;
         case SETTING_HW_TEXTURE_FILTER:
            strlcpy(text, "Default Filter", sizeof(text));
            menu_set_settings_label(setting_text, sizeof(setting_text), &w, settings_lut[i]);
            break;
#ifdef _XBOX1
         case SETTING_FLICKER_FILTER:
            strlcpy(text, "Flicker Filter", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), "%d", g_extern.console.screen.flicker_filter_index);
            break;
         case SETTING_SOFT_DISPLAY_FILTER:
            strlcpy(text, "Soft Display Filter", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE)) ? "ON" : "OFF");
            break;
#endif
         case SETTING_REFRESH_RATE:
            strlcpy(text, "Estimated Monitor FPS", sizeof(text));
            menu_set_settings_label(setting_text, sizeof(setting_text), &w, settings_lut[i]);
            break;
         case SETTING_TRIPLE_BUFFERING:
            strlcpy(text, "Triple Buffering", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_TRIPLE_BUFFERING_ENABLE)) ? "ON" : "OFF");
            break;
         case SETTING_SOUND_MODE:
            strlcpy(text, "Sound Output", sizeof(text));
            switch(g_extern.console.sound.mode)
            {
               case SOUND_MODE_NORMAL:
                  strlcpy(setting_text, "Normal", sizeof(setting_text));
                  break;
#ifdef HAVE_RSOUND
               case SOUND_MODE_RSOUND:
                  strlcpy(setting_text, "RSound", sizeof(setting_text));
                  break;
#endif
#ifdef HAVE_HEADSET
               case SOUND_MODE_HEADSET:
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
            break;
#endif
         case SETTING_EMU_SHOW_DEBUG_INFO_MSG:
            strlcpy(text, "Debug Info Messages", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_FPS_DRAW)) ? "ON" : "OFF");
            break;
         case SETTING_EMU_SHOW_INFO_MSG:
            strlcpy(text, "Info Messages", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW)) ? "ON" : "OFF");
            break;
         case SETTING_REWIND_ENABLED:
            strlcpy(text, "Rewind", sizeof(text));
            menu_set_settings_label(setting_text, sizeof(setting_text), &w, settings_lut[i]);
            break;
         case SETTING_REWIND_GRANULARITY:
            strlcpy(text, "Rewind Granularity", sizeof(text));
            menu_set_settings_label(setting_text, sizeof(setting_text), &w, settings_lut[i]);
            break;
         case SETTING_EMU_AUDIO_MUTE:
            strlcpy(text, "Mute Audio", sizeof(text));
            menu_set_settings_label(setting_text, sizeof(setting_text), &w, settings_lut[i]);
            break;
#ifdef _XBOX1
         case SETTING_EMU_AUDIO_SOUND_VOLUME_LEVEL:
            strlcpy(text, "Volume Level", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), g_extern.console.sound.volume_level ? "Loud" : "Normal");
            break;
#endif
         case SETTING_ENABLE_CUSTOM_BGM:
            strlcpy(text, "Custom BGM Option", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE)) ? "ON" : "OFF");
            break;
         case SETTING_PATH_DEFAULT_ROM_DIRECTORY:
            strlcpy(text, "Browser Directory", sizeof(text));
            menu_set_settings_label(setting_text, sizeof(setting_text), &w, settings_lut[i]);
            break;
         case SETTING_PATH_SAVESTATES_DIRECTORY:
            strlcpy(text, "Savestate Directory", sizeof(text));
            menu_set_settings_label(setting_text, sizeof(setting_text), &w, settings_lut[i]);
            break;
         case SETTING_PATH_SRAM_DIRECTORY:
            strlcpy(text, "Savefile Directory", sizeof(text));
            menu_set_settings_label(setting_text, sizeof(setting_text), &w, settings_lut[i]);
            break;
#ifdef HAVE_XML
         case SETTING_PATH_CHEATS:
            strlcpy(text, "Cheatfile Directory", sizeof(text));
            strlcpy(setting_text, g_settings.cheat_database, sizeof(setting_text));
            break;
#endif
         case SETTING_PATH_SYSTEM:
            strlcpy(text, "System Directory", sizeof(text));
            menu_set_settings_label(setting_text, sizeof(setting_text), &w, settings_lut[i]);
            break;
         case SETTING_CONTROLS_NUMBER:
            strlcpy(text, "Player", sizeof(text));
            menu_set_settings_label(setting_text, sizeof(setting_text), &w, settings_lut[i]);
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
               strlcpy(setting_text, key_label.desc, sizeof(setting_text));
            }
            break;
         case SETTING_CONTROLS_DEFAULT_ALL:
            strlcpy(text, "DEFAULTS", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            break;
         case INGAME_MENU_LOAD_STATE:
            strlcpy(text, "Load State", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), "%d", g_extern.state_slot);
            break;
         case INGAME_MENU_SAVE_STATE:
            strlcpy(text, "Save State", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), "%d", g_extern.state_slot);
            break;
         case SETTING_ASPECT_RATIO:
            strlcpy(text, "Aspect Ratio", sizeof(text));
            menu_set_settings_label(setting_text, sizeof(setting_text), &w, settings_lut[i]);
            break;
         case SETTING_ROTATION:
            strlcpy(text, "Rotation", sizeof(text));
            menu_set_settings_label(setting_text, sizeof(setting_text), &w, settings_lut[i]);
            break;
         case SETTING_CUSTOM_VIEWPORT:
            strlcpy(text, "Custom Ratio", sizeof(text));
            strlcpy(setting_text, "...", sizeof(setting_text));
            break;
         case INGAME_MENU_CORE_OPTIONS_MODE:
            strlcpy(text, "Core Options", sizeof(text));
            strlcpy(setting_text, "...", sizeof(setting_text));
            break;
#ifdef HAVE_SHADER_MANAGER
         case INGAME_MENU_SHADER_OPTIONS_MODE:
            strlcpy(text, "Shader Options", sizeof(text));
            strlcpy(setting_text, "...", sizeof(setting_text));
            break;
#endif
         case INGAME_MENU_LOAD_GAME_HISTORY_MODE:
            strlcpy(text, "Load Game (History)", sizeof(text));
            strlcpy(setting_text, "...", sizeof(setting_text));
            break;
         case INGAME_MENU_VIDEO_OPTIONS_MODE:
            strlcpy(text, "Video Options", sizeof(text));
            strlcpy(setting_text, "...", sizeof(setting_text));
            break;
         case INGAME_MENU_AUDIO_OPTIONS_MODE:
            strlcpy(text, "Audio Options", sizeof(text));
            strlcpy(setting_text, "...", sizeof(setting_text));
            break;
         case INGAME_MENU_INPUT_OPTIONS_MODE:
            strlcpy(text, "Input Options", sizeof(text));
            strlcpy(setting_text, "...", sizeof(setting_text));
            break;
         case INGAME_MENU_PATH_OPTIONS_MODE:
            strlcpy(text, "Path Options", sizeof(text));
            strlcpy(setting_text, "...", sizeof(setting_text));
            break;
         case INGAME_MENU_SETTINGS_MODE:
            strlcpy(text, "Settings", sizeof(text));
            strlcpy(setting_text, "...", sizeof(setting_text));
            break;
         case INGAME_MENU_SCREENSHOT_MODE:
            strlcpy(text, "Take Screenshot", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            break;
         case INGAME_MENU_RESET:
            strlcpy(text, "Restart Game", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            break;
         case INGAME_MENU_RETURN_TO_GAME:
            strlcpy(text, "Resume Game", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            break;
         case INGAME_MENU_CHANGE_GAME:
            strlcpy(text, "Load Game", sizeof(text));
            strlcpy(setting_text, "...", sizeof(setting_text));
            break;
         case INGAME_MENU_CHANGE_LIBRETRO_CORE:
            strlcpy(text, "Core", sizeof(text));
            strlcpy(setting_text, "...", sizeof(setting_text));
            break;
#ifdef HAVE_MULTIMAN
         case INGAME_MENU_RETURN_TO_MULTIMAN:
            strlcpy(text, "Return to multiMAN", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            break;
#endif
         case INGAME_MENU_SAVE_CONFIG:
            strlcpy(text, "Save Config", sizeof(text));
            strlcpy(setting_text, "...", sizeof(setting_text));
            break;
         case INGAME_MENU_QUIT_RETROARCH:
            strlcpy(text, "Quit RetroArch", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            break;
         default:
            break;
#ifdef HAVE_SHADER_MANAGER
         case SHADERMAN_LOAD_CGP:
            strlcpy(text, "Load Shader Preset", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            break;
         case SHADERMAN_SAVE_CGP:
            strlcpy(text, "Save Shader Preset", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
            break;
         case SHADERMAN_SHADER_PASSES:
            strlcpy(text, "Shader Passes", sizeof(text));
            snprintf(setting_text, sizeof(setting_text), "%u", rgui->shader.passes);
            break;
         case SHADERMAN_APPLY_CHANGES:
            strlcpy(text, "Apply Shader Changes", sizeof(text));
            strlcpy(setting_text, "", sizeof(setting_text));
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
            }
            break;
#endif
      }

      char setting_text_buf[256];
      menu_ticker_line(setting_text_buf, TICKER_LABEL_CHARS_MAX_PER_LINE, g_extern.frame_count / 15, setting_text, i == rgui->selection_ptr);

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
      font_parms.color = (i == rgui->selection_ptr) ? YELLOW : WHITE;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, text, &font_parms);

      font_parms.x = POSITION_X_CENTER;
      font_parms.color = WHITE;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, setting_text_buf, &font_parms);

      if (i != rgui->selection_ptr)
         continue;

#ifdef HAVE_MENU_PANEL
      menu_panel->y = y_increment;
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
   size_t history_size = rom_history_size(rgui->history);
   static unsigned hist_opt_selected = 0;
   float y_increment = POSITION_Y_START;

   y_increment += POSITION_Y_INCREMENT;

   font_params_t font_parms = {0};
   font_parms.x = POSITION_X; 
   font_parms.y = y_increment;
   font_parms.scale = CURRENT_PATH_FONT_SIZE;
   font_parms.color = WHITE;

   if (history_size)
   {
      size_t opts = history_size;
      for (size_t i = 0; i < opts; i++)
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

         font_parms.y += POSITION_Y_INCREMENT;
      }
   }
   else if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, "No history available.", &font_parms);

   switch (action)
   {
      case RGUI_ACTION_OK:
      case RGUI_ACTION_START:
         load_menu_game_history(hist_opt_selected);
         return -1;
      case RGUI_ACTION_UP:
         if (hist_opt_selected == 0)
            hist_opt_selected = history_size - 1;
         else
            hist_opt_selected--;
         break;
      case RGUI_ACTION_DOWN:
         hist_opt_selected++;

         if (hist_opt_selected >= history_size)
            hist_opt_selected = 0; 
         break;
      case RGUI_ACTION_LEFT:
         if (hist_opt_selected <= 5)
            hist_opt_selected = 0;
         else
            hist_opt_selected -= 5;
         break;
      case FILEBROWSER_ACTION_RIGHT:
         hist_opt_selected = (min(hist_opt_selected + 5, history_size-1));
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
   static unsigned core_opt_selected = 0;
   float y_increment = POSITION_Y_START;

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
   }
   else if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, "No options available.", &font_parms);

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
   settings_lut[SETTING_REWIND_ENABLED]             = RGUI_SETTINGS_REWIND_ENABLE;
   settings_lut[SETTING_REWIND_GRANULARITY]         = RGUI_SETTINGS_REWIND_GRANULARITY;
   settings_lut[SETTING_EMU_AUDIO_MUTE]             = RGUI_SETTINGS_AUDIO_MUTE;
   settings_lut[SETTING_CONTROLS_NUMBER]            = RGUI_SETTINGS_BIND_PLAYER;
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
