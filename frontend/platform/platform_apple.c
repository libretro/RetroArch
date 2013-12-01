/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2013 - Daniel De Matteis
 * Copyright (C) 2013      - Jason Fetters
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 * * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <dispatch/dispatch.h>
#include <pthread.h>
#include "../../apple/common/rarch_wrapper.h"
#include "../../apple/common/apple_export.h"
#include "../../apple/common/setting_data.h"

#include "../frontend_context.h"
#include "platform_ios.h"

#include <stdint.h>
#include "../../boolean.h"
#include <stddef.h>
#include <string.h>

static pthread_mutex_t apple_event_queue_lock = PTHREAD_MUTEX_INITIALIZER;

static struct
{
   void (*function)(void*);
   void* userdata;
} apple_event_queue[16];

static uint32_t apple_event_queue_size;

void apple_frontend_post_event(void (*fn)(void*), void* userdata)
{
   pthread_mutex_lock(&apple_event_queue_lock);

   if (apple_event_queue_size < 16)
   {
      apple_event_queue[apple_event_queue_size].function = fn;
      apple_event_queue[apple_event_queue_size].userdata = userdata;
      apple_event_queue_size ++;
   }

   pthread_mutex_unlock(&apple_event_queue_lock);
}

void apple_event_basic_command(void* userdata)
{
   int action = (int)userdata;
   switch (action)
   {
      case RESET:
         rarch_game_reset();
         return;
      case LOAD_STATE:
         rarch_load_state();
         return;
      case SAVE_STATE:
         rarch_save_state();
         return;
      case QUIT:
         g_extern.system.shutdown = true;
         return;
   }
}

void apple_event_set_state_slot(void* userdata)
{
   g_extern.state_slot = (uint32_t)userdata;
}

void apple_event_show_rgui(void* userdata)
{
   const bool in_menu = g_extern.lifecycle_state & (1 << MODE_MENU);
   g_extern.lifecycle_state &= ~(1ULL << (in_menu ? MODE_MENU : MODE_GAME));
   g_extern.lifecycle_state |=  (1ULL << (in_menu ? MODE_GAME : MODE_MENU));
}

// Little nudge to prevent stale values when reloading the confg file
void objc_clear_config_hack(void)
{
   g_extern.block_config_read = false;
   memset(g_settings.input.overlay, 0, sizeof(g_settings.input.overlay));
   memset(g_settings.video.shader_path, 0, sizeof(g_settings.video.shader_path));
}

static void event_reload_config(void* userdata)
{
   objc_clear_config_hack();

   uninit_drivers();
   config_load();
   init_drivers();
}

void apple_refresh_config()
{
   if (apple_is_running)
      apple_frontend_post_event(&event_reload_config, 0);
   else
      objc_clear_config_hack();
}

pthread_mutex_t stasis_mutex = PTHREAD_MUTEX_INITIALIZER;

static void event_stasis(void* userdata)
{
   uninit_drivers();
   pthread_mutex_lock(&stasis_mutex);
   pthread_mutex_unlock(&stasis_mutex);
   init_drivers();
}

void apple_enter_stasis()
{
   if (apple_is_running)
   {
      pthread_mutex_lock(&stasis_mutex);
      apple_frontend_post_event(event_stasis, 0);
   }
}

void apple_exit_stasis(bool reload_config)
{
   if (reload_config)
   {
      objc_clear_config_hack();
      config_load();
   }

   if (apple_is_running)
      pthread_mutex_unlock(&stasis_mutex);
}

static int process_events(void *data)
{
   (void)data;
   pthread_mutex_lock(&apple_event_queue_lock);

   for (int i = 0; i < apple_event_queue_size; i ++)
      apple_event_queue[i].function(apple_event_queue[i].userdata);

   apple_event_queue_size = 0;

   pthread_mutex_unlock(&apple_event_queue_lock);
   return 0;
}

static void system_shutdown(bool force)
{
   /* force set to true makes it display the 'Failed to load game' message. */
   if (force)
      dispatch_async_f(dispatch_get_main_queue(), (void*)1, apple_rarch_exited);
   else
      dispatch_async_f(dispatch_get_main_queue(), 0, apple_rarch_exited);
}

void *rarch_main_spring(void* args)
{
   char** argv = args;

   uint32_t argc = 0;
   while (argv && argv[argc])
      argc++;
   
   if (rarch_main(argc, argv))
   {
      rarch_main_clear_state();
      dispatch_async_f(dispatch_get_main_queue(), (void*)1, apple_rarch_exited);
   }
   
   return 0;
}

#ifdef IOS
const void* apple_get_frontend_settings(void)
{
    static rarch_setting_t settings[16];
    
    settings[0]  = setting_data_group_setting(ST_GROUP, "Frontend Settings");
    settings[1]  = setting_data_group_setting(ST_SUB_GROUP, "Frontend");
    settings[2]  = setting_data_bool_setting("ios_use_file_log", "Enable File Logging",
                                             &apple_frontend_settings.logging_enabled, false);
    settings[3]  = setting_data_bool_setting("ios_tv_mode", "TV Mode", &apple_use_tv_mode, false);
    settings[4]  = setting_data_group_setting(ST_END_SUB_GROUP, 0);
    
    settings[5]  = setting_data_group_setting(ST_SUB_GROUP, "Bluetooth");
    settings[6]  = setting_data_string_setting(ST_STRING, "ios_btmode", "Mode", apple_frontend_settings.bluetooth_mode,
                                               sizeof(apple_frontend_settings.bluetooth_mode), "keyboard");
    settings[7]  = setting_data_group_setting(ST_END_SUB_GROUP, 0);
    
    settings[8]  = setting_data_group_setting(ST_SUB_GROUP, "Orientations");
    settings[9]  = setting_data_bool_setting("ios_allow_portrait", "Portrait",
                                             &apple_frontend_settings.portrait, true);
    settings[10]  = setting_data_bool_setting("ios_allow_portrait_upside_down", "Portrait Upside Down",
                                              &apple_frontend_settings.portrait_upside_down, true);
    settings[11]  = setting_data_bool_setting("ios_allow_landscape_left", "Landscape Left",
                                              &apple_frontend_settings.landscape_left, true);
    settings[12] = setting_data_bool_setting("ios_allow_landscape_right", "Landscape Right",
                                             &apple_frontend_settings.landscape_right, true);
    settings[13] = setting_data_group_setting(ST_END_SUB_GROUP, 0);
    settings[14] = setting_data_group_setting(ST_END_GROUP, 0);
    
    return settings;
}

void ios_set_logging_state(const char *log_path, bool on)
{
   fflush(stdout);
   fflush(stderr);

   if (on && !apple_frontend_settings.logging.file)
   {
      apple_frontend_settings.logging.file = fopen(log_path, "a");
      apple_frontend_settings.logging.stdout = dup(1);
      apple_frontend_settings.logging.stderr = dup(2);
      dup2(fileno(apple_frontend_settings.logging.file), 1);
      dup2(fileno(apple_frontend_settings.logging.file), 2);
   }
   else if (!on && apple_frontend_settings.logging.file)
   {
      dup2(apple_frontend_settings.logging.stdout, 1);
      dup2(apple_frontend_settings.logging.stderr, 2);
      
      fclose(apple_frontend_settings.logging.file);
      apple_frontend_settings.logging.file = 0;
   }
}
#endif

const frontend_ctx_driver_t frontend_ctx_apple = {
   NULL,                         /* environment_get */
   NULL,                         /* init */
   NULL,                         /* deinit */
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   process_events,               /* process_events */
   NULL,                         /* exec */
   system_shutdown,              /* shutdown */
   "apple",
};
