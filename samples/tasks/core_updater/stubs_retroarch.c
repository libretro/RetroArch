/* Stubs for get_list_refresh_flags_test: the shipping objects are
 * linked; only the frontend singletons the task reaches are replaced.
 * menu_state_get_ptr() hands out one static menu_state whose flags
 * word the test inspects; config_get_ptr() hands out one static
 * settings_t whose buildbot URL the test aims at its loopback
 * server. */

#include <string.h>
#include <boolean.h>

#include "../../../configuration.h"
#include "../../../msg_hash.h"
#include "../../../menu/menu_driver.h"
#include "../../../verbosity.h"

static settings_t stub_settings;

settings_t *config_get_ptr(void)
{
   return &stub_settings;
}

void get_list_test_set_buildbot_url(const char *url)
{
   strlcpy(stub_settings.paths.network_buildbot_url, url,
         sizeof(stub_settings.paths.network_buildbot_url));
   strlcpy(stub_settings.paths.directory_libretro, "/tmp",
         sizeof(stub_settings.paths.directory_libretro));
   strlcpy(stub_settings.paths.path_libretro_info, "/tmp",
         sizeof(stub_settings.paths.path_libretro_info));
}

static struct menu_state stub_menu_state;

struct menu_state *menu_state_get_ptr(void)
{
   return &stub_menu_state;
}

const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   (void)msg;
   return "stub";
}

void task_window_progress_cb(retro_task_t *task)
{
   (void)task;
}

/* Logging, message queue and the neighbouring subsystems the other
 * tasks in task_core_updater.c reach; the get-list lanes never call
 * them, they only have to link. */

#include <stdarg.h>
#include <stdio.h>

#include "../../../command.h"
#include "../../../core_info.h"
#include "../../../runloop.h"
#include "../../../tasks/tasks_internal.h"

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }

void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}

void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }

bool command_event(enum event_command cmd, void *data)
{
   (void)cmd;
   (void)data;
   return true;
}

void runloop_msg_queue_push(const char *msg, size_t len,
      unsigned prio, unsigned duration,
      bool flush, char *title,
      enum message_queue_icon icon,
      enum message_queue_category category)
{
   (void)msg; (void)len; (void)prio; (void)duration;
   (void)flush; (void)title; (void)icon; (void)category;
}

core_updater_info_t *core_info_get_core_updater_info(
      const char *info_path)
{
   (void)info_path;
   return NULL;
}

void core_info_free_core_updater_info(core_updater_info_t *info)
{
   (void)info;
}

bool core_info_get_core_lock(const char *core_path, bool validate_path)
{
   (void)core_path;
   (void)validate_path;
   return false;
}

void *task_push_core_backup(
      const char *core_path, const char *core_display_name,
      uint32_t crc, enum core_backup_mode backup_mode,
      size_t auto_backup_history_size,
      const char *dir_core_assets, bool mute)
{
   (void)core_path; (void)core_display_name; (void)crc;
   (void)backup_mode; (void)auto_backup_history_size;
   (void)dir_core_assets; (void)mute;
   return NULL;
}

void menu_contentless_cores_free(void)
{
}
