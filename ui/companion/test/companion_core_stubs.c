/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

/* The RetroArch state the companion core reaches for, replaced by
 * fixtures so companion_core.c + playlist.c link against libretro-common
 * alone. The test sets up @test_settings and @test_runloop. Everything
 * that would launch content, run a task or talk to the menu records the
 * call instead. */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <boolean.h>
#include <retro_miscellaneous.h>

#include "../../../configuration.h"
#include "../../../runloop.h"
#include "../../../core_info.h"
#include "../../../msg_hash.h"
#include "../../../command.h"
#include "../../../content.h"
#include "../../../tasks/task_content.h"
#include "../../../tasks/tasks_internal.h"
#include "../../../menu/menu_driver.h"
#include "../../../input/input_driver.h"
#include "../../../gfx/video_driver.h"
#include "../../../paths.h"

settings_t test_settings;
runloop_state_t test_runloop;
static input_driver_state_t test_input;
static video_driver_state_t test_video;
static struct menu_state test_menu;

/* What the core asked RetroArch to do. */
int  stub_calls_load_with_new_core;
int  stub_calls_load_with_current_core;
int  stub_calls_start_core;
int  stub_calls_dbscan;
int  stub_calls_command;
char stub_last_content[PATH_MAX_LENGTH];
char stub_last_core[PATH_MAX_LENGTH];
char stub_last_scan_dir[PATH_MAX_LENGTH];

settings_t *config_get_ptr(void) { return &test_settings; }
runloop_state_t *runloop_state_get_ptr(void) { return &test_runloop; }
input_driver_state_t *input_state_get_ptr(void) { return &test_input; }
video_driver_state_t *video_state_get_ptr(void) { return &test_video; }
struct menu_state *menu_state_get_ptr(void) { return &test_menu; }
bool menu_driver_ctl(enum rarch_menu_ctl_state state, void *data) { (void)state; (void)data; return true; }
uint8_t content_get_flags(void) { return 0; }

/* RetroArch's path slots: one string per type, enough for the core. */
static char test_paths[16][PATH_MAX_LENGTH];
const char *path_get(enum rarch_path_type type) { return (unsigned)type < 16 ? test_paths[type] : ""; }
bool path_set(enum rarch_path_type type, const char *path)
{
   if ((unsigned)type >= 16) return false;
   strlcpy(test_paths[type], path ? path : "", PATH_MAX_LENGTH);
   return true;
}
void path_clear(enum rarch_path_type type) { if ((unsigned)type < 16) test_paths[type][0] = '\0'; }

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }

/* No cores installed: the core-info world is empty. */
bool core_info_find(const char *core_path, core_info_t **core_info) { (void)core_path; if (core_info) *core_info = NULL; return false; }
bool core_info_get_list(core_info_list_t **list) { if (list) *list = NULL; return false; }
bool core_info_init_current_core(void) { return false; }
void core_info_list_get_supported_cores(core_info_list_t *l, const char *p,
      const core_info_t **infos, size_t *num) { (void)l; (void)p; if (infos) *infos = NULL; if (num) *num = 0; }
bool core_info_list_update_missing_firmware(core_info_ctx_firmware_t *info) { (void)info; return false; }
bool core_info_core_file_id_is_equal(const char *a, const char *b) { (void)a; (void)b; return false; }

const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   /* Only what the tests compare against by name. */
   switch (msg)
   {
      case MENU_ENUM_LABEL_VALUE_QT_ALL_PLAYLISTS: return "All Playlists";
      case MENU_ENUM_LABEL_VALUE_HISTORY_TAB:      return "History";
      case MENU_ENUM_LABEL_VALUE_FAVORITES_TAB:    return "Favorites";
      case MENU_ENUM_LABEL_VALUE_IMAGES_TAB:       return "Images";
      case MENU_ENUM_LABEL_VALUE_MUSIC_TAB:        return "Music";
      case MENU_ENUM_LABEL_VALUE_VIDEO_TAB:        return "Videos";
      case MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST:  return "List";
      case MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS: return "Icons";
      default:
      {
         /* Distinct per message: a popup drops a second item with the
          * same title, so identical placeholders would collapse menus. */
         static char buf[8][32];
         static unsigned k;
         char *b = buf[k++ & 7];
         snprintf(b, 32, "msg#%u", (unsigned)msg);
         return b;
      }
   }
}

/* retroarch_ctl (core_option_manager asks whether a core is running). */
bool retroarch_ctl(enum rarch_ctl_state state, void *data)
{
   (void)state; (void)data;
   return false;
}

/* The menu shader, fabricated: two parameters. */
#include "../../../gfx/video_shader_parse.h"
static struct video_shader test_shader;
struct video_shader *menu_shader_get(void)
{
   if (!test_shader.num_parameters)
   {
      strlcpy(test_shader.path, "/shaders/crt.slangp", sizeof(test_shader.path));
      test_shader.num_parameters = 2;
      strlcpy(test_shader.parameters[0].id, "SCANLINE", 64);
      strlcpy(test_shader.parameters[0].desc, "Scanline strength", 64);
      test_shader.parameters[0].minimum = 0.0f; test_shader.parameters[0].maximum = 1.0f;
      test_shader.parameters[0].step = 0.05f; test_shader.parameters[0].initial = 0.5f;
      test_shader.parameters[0].current = 0.5f;
      strlcpy(test_shader.parameters[1].id, "CURV", 64);
      test_shader.parameters[1].desc[0] = '\0';   /* no desc: id shows */
      test_shader.parameters[1].minimum = 0.0f; test_shader.parameters[1].maximum = 10.0f;
      test_shader.parameters[1].step = 1.0f; test_shader.parameters[1].initial = 2.0f;
      test_shader.parameters[1].current = 2.0f;
   }
   return &test_shader;
}

int stub_calls_shader_apply;

bool command_event(enum event_command cmd, void *data)
{
   (void)data;
   stub_calls_command++;
   if (cmd == CMD_EVENT_SHADERS_APPLY_CHANGES)
      stub_calls_shader_apply++;
   return true;
}

bool task_push_load_content_with_new_core_from_companion_ui(
      const char *core_path, const char *fullpath, const char *label,
      const char *db_name, const char *crc32, content_ctx_info_t *ci,
      retro_task_callback_t cb, void *ud)
{
   (void)label; (void)db_name; (void)crc32; (void)ci; (void)cb; (void)ud;
   stub_calls_load_with_new_core++;
   strlcpy(stub_last_core, core_path ? core_path : "", sizeof(stub_last_core));
   strlcpy(stub_last_content, fullpath ? fullpath : "", sizeof(stub_last_content));
   return true;
}

bool task_push_load_content_with_current_core_from_companion_ui(
      const char *fullpath, content_ctx_info_t *ci, enum rarch_core_type type,
      retro_task_callback_t cb, void *ud)
{
   (void)ci; (void)type; (void)cb; (void)ud;
   stub_calls_load_with_current_core++;
   strlcpy(stub_last_content, fullpath ? fullpath : "", sizeof(stub_last_content));
   return true;
}

bool task_push_start_current_core(content_ctx_info_t *ci) { (void)ci; stub_calls_start_core++; return true; }

bool task_push_dbscan(const char *playlist_dir, const char *content_db,
      const char *path, bool directory, bool show_hidden, retro_task_callback_t cb)
{
   (void)playlist_dir; (void)content_db; (void)directory; (void)show_hidden; (void)cb;
   stub_calls_dbscan++;
   strlcpy(stub_last_scan_dir, path ? path : "", sizeof(stub_last_scan_dir));
   return true;
}
