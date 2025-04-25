/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2017 - Daniel De Matteis
 * Copyright (C) 2012-2015 - Michael Lelli
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <file/config_file.h>
#include <queues/task_queue.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <retro_timers.h>
#include <gfx/video_frame.h>
#include <glsym/glsym.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifndef IS_SALAMANDER
#include <lists/file_list.h>
#endif

#include "../frontend.h"
#include "../frontend_driver.h"
#include "../../configuration.h"
#include "../../content.h"
#include "../../command.h"
#include "../../defaults.h"
#include "../../file_path_special.h"
#include "../../paths.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../tasks/tasks_internal.h"
#include "../../cheat_manager.h"
#include "../../audio/audio_driver.h"

#ifdef HAVE_EXTRA_WASMFS
#include <emscripten/wasmfs.h>
#endif

#ifdef PROXY_TO_PTHREAD
#include <emscripten/wasm_worker.h>
#include <emscripten/threading.h>
#include <emscripten/proxying.h>
#include <emscripten/atomic.h>
#define PLATFORM_SETVAL(type, addr, val) emscripten_atomic_store_##type(addr, val)
#define PLATFORM_GETVAL(type, addr) emscripten_atomic_load_##type(addr)
#else
#define PLATFORM_SETVAL(type, addr, val) *addr = val
#define PLATFORM_GETVAL(type, addr) *addr
#endif

#include "platform_emscripten.h"

void emscripten_mainloop(void);
void PlatformEmscriptenWatchCanvasSizeAndDpr(double *dpr);
void PlatformEmscriptenWatchWindowVisibility(void);
void PlatformEmscriptenPowerStateInit(void);
void PlatformEmscriptenMemoryUsageInit(void);

typedef struct
{
#ifdef PROXY_TO_PTHREAD
   pthread_t program_thread_id;
   emscripten_lock_t raf_lock;
   emscripten_condvar_t raf_cond;
#endif
   uint64_t memory_used;
   uint64_t memory_limit;
   double device_pixel_ratio;
   int raf_interval;
   int canvas_width;
   int canvas_height;
   int power_state_discharge_time;
   float power_state_level;
   bool has_async_atomics;
   volatile bool power_state_charging;
   volatile bool power_state_supported;
   volatile bool window_hidden;
   volatile bool command_flag;
} emscripten_platform_data_t;

static emscripten_platform_data_t *emscripten_platform_data = NULL;

/* begin exported functions */

/* saves and states */

void cmd_savefiles(void)
{
   command_event(CMD_EVENT_SAVE_FILES, NULL);
}

void cmd_save_state(void)
{
   command_event(CMD_EVENT_SAVE_STATE, NULL);
}

void cmd_load_state(void)
{
   command_event(CMD_EVENT_LOAD_STATE, NULL);
}

void cmd_undo_save_state(void)
{
   command_event(CMD_EVENT_UNDO_SAVE_STATE, NULL);
}

void cmd_undo_load_state(void)
{
   command_event(CMD_EVENT_UNDO_LOAD_STATE, NULL);
}

/* misc */

void cmd_take_screenshot(void)
{
   command_event(CMD_EVENT_TAKE_SCREENSHOT, NULL);
}

void cmd_toggle_menu(void)
{
   command_event(CMD_EVENT_MENU_TOGGLE, NULL);
}

void cmd_reload_config(void)
{
   command_event(CMD_EVENT_RELOAD_CONFIG, NULL);
}

void cmd_toggle_grab_mouse(void)
{
   command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL);
}

void cmd_toggle_game_focus(void)
{
   command_event(CMD_EVENT_GAME_FOCUS_TOGGLE, NULL);
}

void cmd_reset(void)
{
   command_event(CMD_EVENT_RESET, NULL);
}

void cmd_toggle_pause(void)
{
   command_event(CMD_EVENT_PAUSE_TOGGLE, NULL);
}

void cmd_pause(void)
{
   command_event(CMD_EVENT_PAUSE, NULL);
}

void cmd_unpause(void)
{
   command_event(CMD_EVENT_UNPAUSE, NULL);
}

void cmd_set_volume(float volume)
{
   audio_set_float(AUDIO_ACTION_VOLUME_GAIN, volume);
}

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
bool cmd_set_shader(const char *path)
{
   return command_set_shader(NULL, path);
}
#endif

/* cheats */

void cmd_cheat_set_code(unsigned index, const char *str)
{
   cheat_manager_set_code(index, str);
}

const char *cmd_cheat_get_code(unsigned index)
{
   return cheat_manager_get_code(index);
}

void cmd_cheat_toggle_index(bool apply_cheats_after_toggle, unsigned index)
{
   cheat_manager_toggle_index(apply_cheats_after_toggle,
         config_get_ptr()->bools.notification_show_cheats_applied,
         index);
}

bool cmd_cheat_get_code_state(unsigned index)
{
   return cheat_manager_get_code_state(index);
}

bool cmd_cheat_realloc(unsigned new_size)
{
   return cheat_manager_realloc(new_size, CHEAT_HANDLER_TYPE_EMU);
}

unsigned cmd_cheat_get_size(void)
{
   return cheat_manager_get_size();
}

void cmd_cheat_apply_cheats(void)
{
   cheat_manager_apply_cheats(
         config_get_ptr()->bools.notification_show_cheats_applied);
}

/* javascript callbacks */

void platform_emscripten_update_canvas_dimensions(int width, int height, double *dpr)
{
   printf("[INFO] Setting real canvas size: %d x %d\n", width, height);
   emscripten_set_canvas_element_size("#canvas", width, height);
   if (!emscripten_platform_data)
      return;
   PLATFORM_SETVAL(u32, &emscripten_platform_data->canvas_width,        width);
   PLATFORM_SETVAL(u32, &emscripten_platform_data->canvas_height,       height);
   PLATFORM_SETVAL(f64, &emscripten_platform_data->device_pixel_ratio, *dpr);
}

void platform_emscripten_update_window_hidden(bool hidden)
{
   if (!emscripten_platform_data)
      return;
   emscripten_platform_data->window_hidden = hidden;
}

void platform_emscripten_update_power_state(bool supported, int discharge_time, float level, bool charging)
{
   if (!emscripten_platform_data)
      return;
   emscripten_platform_data->power_state_supported      = supported;
   emscripten_platform_data->power_state_charging       = charging;
   PLATFORM_SETVAL(u32, &emscripten_platform_data->power_state_discharge_time, discharge_time);
   PLATFORM_SETVAL(f32, &emscripten_platform_data->power_state_level,          level);
}

void platform_emscripten_update_memory_usage(uint32_t used1, uint32_t used2, uint32_t limit1, uint32_t limit2)
{
   if (!emscripten_platform_data)
      return;
   PLATFORM_SETVAL(u64, &emscripten_platform_data->memory_used,  used1 | ((uint64_t)used2 << 32));
   PLATFORM_SETVAL(u64, &emscripten_platform_data->memory_limit, limit1 | ((uint64_t)limit2 << 32));
}

void platform_emscripten_command_raise_flag()
{
   if (!emscripten_platform_data)
      return;
   emscripten_platform_data->command_flag = true;
}

/* platform specific c helpers */
/* see platform_emscripten.h for documentation. */

void platform_emscripten_run_on_browser_thread_sync(void (*func)(void*), void* arg)
{
#ifdef PROXY_TO_PTHREAD
   emscripten_proxy_sync(emscripten_proxy_get_system_queue(), emscripten_main_runtime_thread_id(), func, arg);
#else
   func(arg);
#endif
}

void platform_emscripten_run_on_browser_thread_async(void (*func)(void*), void* arg)
{
#ifdef PROXY_TO_PTHREAD
   emscripten_proxy_async(emscripten_proxy_get_system_queue(), emscripten_main_runtime_thread_id(), func, arg);
#else
   emscripten_async_call(func, arg, 0);
#endif
}

void platform_emscripten_run_on_program_thread_async(void (*func)(void*), void* arg)
{
#ifdef PROXY_TO_PTHREAD
   emscripten_proxy_async(emscripten_proxy_get_system_queue(), emscripten_platform_data->program_thread_id, func, arg);
#else
   emscripten_async_call(func, arg, 0);
#endif
}

void platform_emscripten_command_reply(const char *msg, size_t len)
{
   MAIN_THREAD_EM_ASM({
      var message = UTF8ToString($0, $1);
      RPE.command_reply_queue.push(message);
   }, msg, len);
}

size_t platform_emscripten_command_read(char **into, size_t max_len)
{
   if (!emscripten_platform_data || !emscripten_platform_data->command_flag)
      return 0;
   return MAIN_THREAD_EM_ASM_INT({
      var next_command = RPE.command_queue.shift();
      var length = lengthBytesUTF8(next_command);
      if (length > $2) {
         console.error("[CMD] Command too long, skipping", next_command);
         return 0;
      }
      stringToUTF8(next_command, $1, $2);
      if (RPE.command_queue.length == 0) {
         setValue($0, 0, 'i8');
      }
      return length;
    }, &emscripten_platform_data->command_flag, into, max_len);
}

void platform_emscripten_get_canvas_size(int *width, int *height)
{
   if (!emscripten_platform_data)
      goto error;

   *width  = PLATFORM_GETVAL(u32, &emscripten_platform_data->canvas_width);
   *height = PLATFORM_GETVAL(u32, &emscripten_platform_data->canvas_height);

   if (*width != 0 || *height != 0)
      return;

error:
   *width  = 800;
   *height = 600;
   RARCH_ERR("[EMSCRIPTEN]: Could not get screen dimensions!\n");
}

double platform_emscripten_get_dpr(void)
{
   return PLATFORM_GETVAL(f64, &emscripten_platform_data->device_pixel_ratio);
}

bool platform_emscripten_has_async_atomics(void)
{
   return emscripten_platform_data->has_async_atomics;
}

bool platform_emscripten_is_window_hidden(void)
{
   return emscripten_platform_data->window_hidden;
}

bool platform_emscripten_should_drop_iter(void)
{
   return (emscripten_platform_data->window_hidden && emscripten_platform_data->raf_interval);
}

#ifdef PROXY_TO_PTHREAD

static void set_raf_interval(void *data)
{
   emscripten_set_main_loop_timing(EM_TIMING_RAF, (int)data);
}

void platform_emscripten_wait_for_frame(void)
{
   if (emscripten_platform_data->raf_interval)
      emscripten_condvar_waitinf(&emscripten_platform_data->raf_cond, &emscripten_platform_data->raf_lock);
}

#else

void platform_emscripten_enter_fake_block(int ms)
{
   if (ms == 0)
      emscripten_set_main_loop_timing(EM_TIMING_SETIMMEDIATE, 0);
   else
      emscripten_set_main_loop_timing(EM_TIMING_SETTIMEOUT, ms);
}

void platform_emscripten_exit_fake_block(void)
{
   command_event(CMD_EVENT_VIDEO_SET_BLOCKING_STATE, NULL);
}

#endif

void platform_emscripten_set_main_loop_interval(int interval)
{
   emscripten_platform_data->raf_interval = interval;
#ifdef PROXY_TO_PTHREAD
   if (interval != 0)
      platform_emscripten_run_on_browser_thread_sync(set_raf_interval, (void *)interval);
#else
   if (interval == 0)
      emscripten_set_main_loop_timing(EM_TIMING_SETIMMEDIATE, 0);
   else
      emscripten_set_main_loop_timing(EM_TIMING_RAF, interval);
#endif
}

/* frontend driver impl */

static void frontend_emscripten_get_env(int *argc, char *argv[],
      void *args, void *params_data)
{
   char base_path[PATH_MAX];
   char user_path[PATH_MAX];
   char bundle_path[PATH_MAX];
   const char *home = getenv("HOME");

   if (home)
   {
      size_t _len = strlcpy(base_path, home, sizeof(base_path));
      strlcpy(base_path + _len, "/retroarch", sizeof(base_path) - _len);
#ifndef HAVE_EXTRA_WASMFS
      /* can be removed when the new web player replaces the old one */
      _len = strlcpy(user_path, home, sizeof(user_path));
      strlcpy(user_path + _len, "/retroarch/userdata", sizeof(user_path) - _len);
      _len = strlcpy(bundle_path, home, sizeof(bundle_path));
      strlcpy(bundle_path + _len, "/retroarch/bundle", sizeof(bundle_path) - _len);
#else
      _len = strlcpy(user_path, home, sizeof(user_path));
      strlcpy(user_path + _len, "/retroarch", sizeof(user_path) - _len);
      _len = strlcpy(bundle_path, home, sizeof(bundle_path));
      strlcpy(bundle_path + _len, "/retroarch", sizeof(bundle_path) - _len);
#endif
   }
   else
   {
      strlcpy(base_path, "retroarch", sizeof(base_path));
#ifndef HAVE_EXTRA_WASMFS
      /* can be removed when the new web player replaces the old one */
      strlcpy(user_path, "retroarch/userdata", sizeof(user_path));
      strlcpy(bundle_path, "retroarch/bundle", sizeof(bundle_path));
#else
      strlcpy(user_path, "retroarch", sizeof(user_path));
      strlcpy(bundle_path, "retroarch", sizeof(bundle_path));
#endif
   }

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE], base_path,
         "cores", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));

   /* bundle data */
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS], bundle_path,
         "assets", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG], bundle_path,
         "autoconfig", sizeof(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_DATABASE], bundle_path,
         "database/rdb", sizeof(g_defaults.dirs[DEFAULT_DIR_DATABASE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO], bundle_path,
         "info", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OVERLAY], bundle_path,
         "overlays", sizeof(g_defaults.dirs[DEFAULT_DIR_OVERLAY]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OSK_OVERLAY], bundle_path,
         "overlays/keyboards", sizeof(g_defaults.dirs[DEFAULT_DIR_OSK_OVERLAY]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SHADER], bundle_path,
         "shaders", sizeof(g_defaults.dirs[DEFAULT_DIR_SHADER]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER], bundle_path,
         "filters/audio", sizeof(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER], bundle_path,
         "filters/video", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]));

   /* user data dirs */
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CHEATS], user_path,
         "cheats", sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], user_path,
         "config", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONTENT], user_path,
         "content", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONTENT]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS], user_path,
         "downloads", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PLAYLIST], user_path,
         "playlists", sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_REMAP], g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG],
         "remaps", sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SRAM], user_path,
         "saves", sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT], user_path,
         "screenshots", sizeof(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SAVESTATE], user_path,
         "states", sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SYSTEM], user_path,
         "system", sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS], user_path,
         "thumbnails", sizeof(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_LOGS], user_path,
         "logs", sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));

   /* cache dir */
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CACHE], "/tmp/",
         "retroarch", sizeof(g_defaults.dirs[DEFAULT_DIR_CACHE]));

   /* history */
   strlcpy(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY],
         user_path, sizeof(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY]));

#ifndef IS_SALAMANDER
   dir_check_defaults("custom.ini");
#endif
}

static enum frontend_powerstate frontend_emscripten_get_powerstate(int *seconds, int *percent)
{
   enum frontend_powerstate ret = FRONTEND_POWERSTATE_NONE;
   float level;

   if (!emscripten_platform_data || !emscripten_platform_data->power_state_supported)
      return ret;

   level = PLATFORM_GETVAL(f32, &emscripten_platform_data->power_state_level);

   if (!emscripten_platform_data->power_state_charging)
      ret = FRONTEND_POWERSTATE_ON_POWER_SOURCE;
   else if (level == 1)
      ret = FRONTEND_POWERSTATE_CHARGED;
   else
      ret = FRONTEND_POWERSTATE_CHARGING;

   *seconds = PLATFORM_GETVAL(u32, &emscripten_platform_data->power_state_discharge_time);
   *percent = (int)(level * 100);

   return ret;
}

static uint64_t frontend_emscripten_get_total_mem(void)
{
   if (!emscripten_platform_data)
      return 0;
   return PLATFORM_GETVAL(u64, &emscripten_platform_data->memory_limit);
}

static uint64_t frontend_emscripten_get_free_mem(void)
{
   if (!emscripten_platform_data)
      return 0;
#ifndef PROXY_TO_PTHREAD
   uint64_t used = PLATFORM_GETVAL(u64, &emscripten_platform_data->memory_used);
#else
   uint64_t used = mallinfo().uordblks;
#endif
   return (PLATFORM_GETVAL(u64, &emscripten_platform_data->memory_limit) - used);
}

/* program entry and startup */

#ifdef HAVE_EXTRA_WASMFS
static void platform_emscripten_mount_filesystems(void)
{
   char *opfs_mount = getenv("OPFS_MOUNT");
   char *fetch_manifest = getenv("FETCH_MANIFEST");
   char *fetch_base_dir = getenv("FETCH_BASE_DIR");
   if (opfs_mount)
   {
      int res;
      printf("[OPFS] Mount OPFS at %s\n", opfs_mount);
      backend_t opfs = wasmfs_create_opfs_backend();
      {
         char *parent = strdup(opfs_mount);
         path_parent_dir(parent, strlen(parent));
         if (!path_mkdir(parent))
         {
            printf("mkdir error %d\n", errno);
            abort();
         }
         free(parent);
      }
      res = wasmfs_create_directory(opfs_mount, 0777, opfs);
      if (res)
      {
         printf("[OPFS] error result %d\n", res);
         if (errno)
         {
            printf("[OPFS] errno %d\n", errno);
            abort();
         }
         abort();
      }
   }
   if (fetch_manifest || fetch_base_dir)
   {
      /* fetch_manifest should be a path to a manifest file.
         manifest files have this format:

         BASEURL
         URL PATH
         URL PATH
         URL PATH
         ...

         Where URL may not contain spaces, but PATH may.
         URL segments are relative to BASEURL.
       */
      int max_line_len = 1024;
      if (!(fetch_manifest && fetch_base_dir))
      {
         printf("[FetchFS] must specify both FETCH_MANIFEST and FETCH_BASE_DIR\n");
         abort();
      }
      printf("[FetchFS] read fetch manifest from %s\n", fetch_manifest);
      FILE *file = fopen(fetch_manifest, "r");
      if (!file)
      {
        printf("[FetchFS] missing manifest file\n");
        abort();
      }
      char *line = calloc(sizeof(char), max_line_len);
      size_t len = max_line_len;
      if (getline(&line, &len, file) == -1 || len == 0)
         printf("[FetchFS] missing base URL suggest empty manifest, skipping fetch initialization\n");
      else
      {
         char *base_url = strdup(line);
         base_url[strcspn(base_url, "\r\n")] = '\0'; // drop newline
         base_url[len-1] = '\0'; // drop newline
         backend_t fetch = NULL;
         len = max_line_len;
         // Don't create fetch backend unless manifest actually has entries
         while (getline(&line, &len, file) != -1)
         {
            if (!fetch)
            {
               fetch = wasmfs_create_fetch_backend(base_url, 16*1024*1024);
               if(!fetch) {
                 printf("[FetchFS] couldn't create fetch backend for %s\n", base_url);
                 abort();
               }
               wasmfs_create_directory(fetch_base_dir, 0777, fetch);
            }
            char *realfs_path = strstr(line, " "), *url = line;
            int fd;
            if (len <= 2 || !realfs_path)
            {
               printf("[FetchFS] Manifest file has invalid line %s\n",line);
               continue;
            }
            *realfs_path = '\0';
            realfs_path += 1;
            realfs_path[strcspn(realfs_path, "\r\n")] = '\0';
            char fetchfs_path[PATH_MAX];
            fill_pathname_join(fetchfs_path, fetch_base_dir, url, sizeof(fetchfs_path));
            /* Make the directories for link path */
            {
               char *parent = strdup(realfs_path);
               path_parent_dir(parent, strlen(parent));
               if (!path_mkdir(parent))
               {
                  printf("[FetchFS] mkdir error %s %d\n", realfs_path, errno);
                  abort();
               }
               free(parent);
            }
            /* Make the directories for URL path */
            {
               char *parent = strdup(fetchfs_path);
               path_parent_dir(parent, strlen(parent));
               if (!path_mkdir(parent))
               {
                  printf("[FetchFS] mkdir error %s %d\n", fetchfs_path, errno);
                  abort();
               }
               free(parent);
            }
            fd = wasmfs_create_file(fetchfs_path, 0777, fetch);
            if (!fd)
            {
               printf("[FetchFS] couldn't create fetch file %s\n", fetchfs_path);
               abort();
            }
            close(fd);
            if (symlink(fetchfs_path, realfs_path) != 0)
            {
               printf("[FetchFS] couldn't create link %s to fetch file %s (errno %d)\n", realfs_path, fetchfs_path, errno);
               abort();
            }
            len = max_line_len;
         }
         free(base_url);
      }
      fclose(file);
      free(line);
   }
}
#endif /* HAVE_EXTRA_WASMFS */

static int thread_main(int argc, char *argv[])
{
#ifdef HAVE_EXTRA_WASMFS
   platform_emscripten_mount_filesystems();
#endif

   emscripten_set_main_loop(emscripten_mainloop, 0, 0);
#ifdef PROXY_TO_PTHREAD
   emscripten_set_main_loop_timing(EM_TIMING_SETIMMEDIATE, 0);
#else
   emscripten_set_main_loop_timing(EM_TIMING_RAF, 1);
#endif
   rarch_main(argc, argv, NULL);

   return 0;
}

#ifdef PROXY_TO_PTHREAD

static int _main_argc;
static char** _main_argv;

static void *main_pthread(void* arg)
{
   emscripten_set_thread_name(pthread_self(), "Application main thread");
   emscripten_platform_data->program_thread_id = pthread_self();
   thread_main(_main_argc, _main_argv);
   return NULL;
}

static void raf_signaler(void)
{
   emscripten_condvar_signal(&emscripten_platform_data->raf_cond, 1);
}
#endif

int main(int argc, char *argv[])
{
   int ret = 0;
#ifdef PROXY_TO_PTHREAD
   pthread_attr_t attr;
   pthread_t thread;
#endif
   /* this never gets freed */
   emscripten_platform_data = (emscripten_platform_data_t *)calloc(1, sizeof(emscripten_platform_data_t));

   emscripten_platform_data->has_async_atomics = EM_ASM_INT({
      return Atomics?.waitAsync?.toString().includes("[native code]");
   });

   PlatformEmscriptenWatchCanvasSizeAndDpr(malloc(sizeof(double)));
   PlatformEmscriptenWatchWindowVisibility();
   PlatformEmscriptenPowerStateInit();
   PlatformEmscriptenMemoryUsageInit();

   emscripten_platform_data->raf_interval = 1;
#ifdef PROXY_TO_PTHREAD
   /* run requestAnimationFrame on the browser thread, as some browsers (chrome on linux) */
   /* seem to have issues running at full speed with requestAnimationFrame in workers. */
   /* instead, we run the RetroArch main loop with setImmediate and just wait on a signal if we need RAF */
   emscripten_lock_init(&emscripten_platform_data->raf_lock);
   emscripten_condvar_init(&emscripten_platform_data->raf_cond);
   emscripten_set_main_loop(raf_signaler, 0, 0);
   emscripten_set_main_loop_timing(EM_TIMING_RAF, 1);

   _main_argc = argc;
   _main_argv = argv;
   pthread_attr_init(&attr);
   pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
   pthread_attr_setstacksize(&attr, EMSCRIPTEN_STACK_SIZE);
   emscripten_pthread_attr_settransferredcanvases(&attr, (const char*)-1);
   ret = pthread_create(&thread, &attr, main_pthread, NULL);
   pthread_attr_destroy(&attr);
#else
   ret = thread_main(argc, argv);
#endif
   return ret;
}

frontend_ctx_driver_t frontend_ctx_emscripten = {
   frontend_emscripten_get_env,         /* environment_get */
   NULL,                                /* init */
   NULL,                                /* deinit */
   NULL,                                /* exitspawn */
   NULL,                                /* process_args */
   NULL,                                /* exec */
   NULL,                                /* set_fork */
   NULL,                                /* shutdown */
   NULL,                                /* get_name */
   NULL,                                /* get_os */
   NULL,                                /* get_rating */
   NULL,                                /* load_content */
   NULL,                                /* get_architecture */
   frontend_emscripten_get_powerstate,  /* get_powerstate */
   NULL,                                /* parse_drive_list */
   frontend_emscripten_get_total_mem,   /* get_total_mem */
   frontend_emscripten_get_free_mem,    /* get_free_mem  */
   NULL,                                /* install_sighandlers */
   NULL,                                /* get_signal_handler_state */
   NULL,                                /* set_signal_handler_state */
   NULL,                                /* destroy_signal_handler_state */
   NULL,                                /* attach_console */
   NULL,                                /* detach_console */
   NULL,                                /* get_lakka_version */
   NULL,                                /* set_screen_brightness */
   NULL,                                /* watch_path_for_changes */
   NULL,                                /* check_for_path_changes */
   NULL,                                /* set_sustained_performance_mode */
   NULL,                                /* get_cpu_model_name */
   NULL,                                /* get_user_language */
   NULL,                                /* is_narrator_running */
   NULL,                                /* accessibility_speak */
   NULL,                                /* set_gamemode        */
   "emscripten",                        /* ident               */
   NULL                                 /* get_video_driver    */
};
