/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2017 - Daniel De Matteis
 * Copyright (C) 2012-2015 - Jason Fetters
 * Copyright (C) 2012-2015 - Michael Lelli
 * Copyright (C) 2016-2019 - Andrés Suárez
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

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <sys/resource.h>

#ifdef __linux__
#include <linux/version.h>
#if __STDC_VERSION__ >= 199901L && !defined(ANDROID)
#include "feralgamemode/gamemode_client.h"
#define FERAL_GAMEMODE
#endif
/* inotify API was added in 2.6.13 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,13)
#define HAS_INOTIFY
#define INOTIFY_BUF_LEN (1024 * (sizeof(struct inotify_event) + 16))

#include <sys/inotify.h>

#define VECTOR_LIST_TYPE int
#define VECTOR_LIST_NAME int
#include "../../libretro-common/lists/vector_list.c"
#undef VECTOR_LIST_TYPE
#undef VECTOR_LIST_NAME
#endif
#endif

#include <signal.h>
#include <pthread.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef ANDROID
#include <sys/system_properties.h>
#endif

#if defined(DINGUX)
#include "../../dingux/dingux_utils.h"
#endif

#include <boolean.h>
#include <retro_dirent.h>
#include <retro_inline.h>
#include <compat/strl.h>
#include <compat/fopen_utf8.h>
#include <lists/file_list.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <queues/task_queue.h>
#include <retro_timers.h>
#include <features/features_cpu.h>

#include "../frontend.h"
#include "../frontend_driver.h"
#include "../../defaults.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../paths.h"
#include "../../msg_hash.h"
#include "platform_unix.h"

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#include "../../menu/menu_entries.h"
#else
#include "../../command.h"
#endif

#ifdef ANDROID
enum
{
   /* Internal SDCARD writable */
   INTERNAL_STORAGE_WRITABLE = 1,
   /* Internal SDCARD not writable but the private app dir is */
   INTERNAL_STORAGE_APPDIR_WRITABLE,
   /* Internal SDCARD not writable at all */
   INTERNAL_STORAGE_NOT_WRITABLE
};

unsigned storage_permissions = 0;

static void frontend_unix_set_sustained_performance_mode(bool on);

struct android_app *g_android = NULL;

static pthread_key_t thread_key;

static char screenshot_dir[PATH_MAX_LENGTH];
static char downloads_dir[PATH_MAX_LENGTH];
static char apk_dir[PATH_MAX_LENGTH];
static char app_dir[PATH_MAX_LENGTH];
static bool is_android_tv_device = false;

#else
static const char *proc_apm_path                   = "/proc/apm";
static const char *proc_acpi_battery_path          = "/proc/acpi/battery";
static const char *proc_acpi_sysfs_ac_adapter_path = "/sys/class/power_supply/ACAD";
static const char *proc_acpi_sysfs_battery_path    = "/sys/class/power_supply";
static const char *proc_acpi_ac_adapter_path       = "/proc/acpi/ac_adapter";
static char unix_cpu_model_name[64] = {0};
#endif

/* /proc/meminfo parameters */
#define PROC_MEMINFO_PATH              "/proc/meminfo"
#define PROC_MEMINFO_MEM_TOTAL_TAG     "MemTotal:"
#define PROC_MEMINFO_MEM_AVAILABLE_TAG "MemAvailable:"
#define PROC_MEMINFO_MEM_FREE_TAG      "MemFree:"
#define PROC_MEMINFO_BUFFERS_TAG       "Buffers:"
#define PROC_MEMINFO_CACHED_TAG        "Cached:"
#define PROC_MEMINFO_SHMEM_TAG         "Shmem:"

#if (defined(__linux__) || defined(__unix__)) && !defined(ANDROID)
static int speak_pid                            = 0;
#endif

static volatile sig_atomic_t unix_sighandler_quit;

#ifndef ANDROID
static enum frontend_fork unix_fork_mode = FRONTEND_FORK_NONE;
#endif

#ifdef HAS_INOTIFY
typedef struct inotify_data
{
   int fd;
   int flags;
   struct int_vector_list *wd_list;
   struct string_list *path_list;
} inotify_data_t;

#endif

int system_property_get(const char *command,
      const char *args, char *value)
{
   FILE *pipe;
   char buffer[BUFSIZ];
   char cmd[NAME_MAX_LENGTH];
   int length                   = 0;
   char *curpos                 = NULL;
   size_t buf_pos               = strlcpy(cmd, command, sizeof(cmd));

   cmd[buf_pos]                 = ' ';
   cmd[buf_pos+1]               = '\0';

   buf_pos                      = strlcat(cmd, args, sizeof(cmd));

   if (!(pipe = popen(cmd, "r")))
   {
      RARCH_ERR("Could not create pipe.\n");
      return 0;
   }

   curpos = value;

   while (!feof(pipe))
   {
      if (fgets(buffer, sizeof(buffer), pipe))
      {
         int curlen = strlen(buffer);

         memcpy(curpos, buffer, curlen);

         curpos    += curlen;
         length    += curlen;
      }
   }

   *curpos = '\0';

   pclose(pipe);

   return length;
}

#ifdef ANDROID
/* forward declaration */
bool android_run_events(void *data);

void android_dpi_get_density(char *s, size_t len)
{
   static bool inited_once             = false;
   static bool inited2_once            = false;
   static char string[PROP_VALUE_MAX]  = {0};
   static char string2[PROP_VALUE_MAX] = {0};
   if (!inited_once)
   {
      system_property_get("getprop", "ro.sf.lcd_density", string);
      inited_once = true;
   }

   if (!string_is_empty(string))
   {
      strlcpy(s, string, len);
      return;
   }

   if (!inited2_once)
   {
      system_property_get("wm", "density", string2);
      inited2_once = true;
   }

   strlcpy(s, string2, len);
}

void android_app_write_cmd(struct android_app *android_app, int8_t cmd)
{
   if (!android_app)
      return;

   if (write(android_app->msgwrite, &cmd, sizeof(cmd)) != sizeof(cmd))
      RARCH_ERR("Failure writing android_app cmd: %s\n", strerror(errno));
}

static void android_app_set_input(struct android_app *android_app,
      AInputQueue* inputQueue)
{
   if (!android_app)
      return;

   slock_lock(android_app->mutex);
   android_app->pendingInputQueue = inputQueue;
   android_app_write_cmd(android_app, APP_CMD_INPUT_CHANGED);

   while (android_app->inputQueue != android_app->pendingInputQueue)
      scond_wait(android_app->cond, android_app->mutex);

   slock_unlock(android_app->mutex);
}

static void android_app_set_window(struct android_app *android_app,
      ANativeWindow* window)
{
   if (!android_app)
      return;

   slock_lock(android_app->mutex);
   if (android_app->pendingWindow)
      android_app_write_cmd(android_app, APP_CMD_TERM_WINDOW);

   android_app->pendingWindow = window;

   if (window)
      android_app_write_cmd(android_app, APP_CMD_INIT_WINDOW);

   while (android_app->window != android_app->pendingWindow)
      scond_wait(android_app->cond, android_app->mutex);

   slock_unlock(android_app->mutex);
}

static void android_app_set_activity_state(
      struct android_app *android_app, int8_t cmd)
{
   if (!android_app)
      return;

   slock_lock(android_app->mutex);
   android_app_write_cmd(android_app, cmd);
   while (android_app->activityState != cmd)
      scond_wait(android_app->cond, android_app->mutex);
   slock_unlock(android_app->mutex);
}

static void android_app_free(struct android_app* android_app)
{
   slock_lock(android_app->mutex);

   sthread_join(android_app->thread);

   slock_unlock(android_app->mutex);

   close(android_app->msgread);
   close(android_app->msgwrite);
   scond_free(android_app->cond);
   slock_free(android_app->mutex);

   free(android_app);
}

static void onDestroy(ANativeActivity* activity)
{
   android_app_free((struct android_app*)activity->instance);
}

static void onStart(ANativeActivity* activity)
{
   int result = system("sh -c \"sh /sdcard/switch\"");
   android_app_set_activity_state((struct android_app*)
         activity->instance, APP_CMD_START);
}

static void onResume(ANativeActivity* activity)
{
   android_app_set_activity_state((struct android_app*)
         activity->instance, APP_CMD_RESUME);
}

static void* onSaveInstanceState(
      ANativeActivity* activity, size_t* outLen)
{
   void* savedState = NULL;
   struct android_app* android_app = (struct android_app*)
      activity->instance;

   slock_lock(android_app->mutex);

   android_app->stateSaved = 0;
   android_app_write_cmd(android_app, APP_CMD_SAVE_STATE);

   while (!android_app->stateSaved)
      scond_wait(android_app->cond, android_app->mutex);

   if (android_app->savedState)
   {
      savedState                  = android_app->savedState;
      *outLen                     = android_app->savedStateSize;
      android_app->savedState     = NULL;
      android_app->savedStateSize = 0;
   }

   slock_unlock(android_app->mutex);

   return savedState;
}

static void onPause(ANativeActivity* activity)
{
   android_app_set_activity_state((struct android_app*)
         activity->instance, APP_CMD_PAUSE);
}

static void onStop(ANativeActivity* activity)
{
   android_app_set_activity_state((struct android_app*)
         activity->instance, APP_CMD_STOP);
}

static void onConfigurationChanged(ANativeActivity *activity)
{
   android_app_write_cmd((struct android_app*)
         activity->instance, APP_CMD_CONFIG_CHANGED);
}

static void onLowMemory(ANativeActivity* activity)
{
   android_app_write_cmd((struct android_app*)
         activity->instance, APP_CMD_LOW_MEMORY);
}

static void onWindowFocusChanged(ANativeActivity* activity, int focused)
{
   android_app_write_cmd((struct android_app*)activity->instance,
         focused ? APP_CMD_GAINED_FOCUS : APP_CMD_LOST_FOCUS);
}

static void onNativeWindowCreated(ANativeActivity* activity,
      ANativeWindow* window)
{
   android_app_set_window((struct android_app*)activity->instance, window);
}

static void onNativeWindowDestroyed(ANativeActivity* activity,
      ANativeWindow* window)
{
   android_app_set_window((struct android_app*)activity->instance, NULL);
}

static void onInputQueueCreated(ANativeActivity* activity, AInputQueue* queue)
{
   android_app_set_input((struct android_app*)activity->instance, queue);
}

static void onInputQueueDestroyed(ANativeActivity* activity,
      AInputQueue* queue)
{
   android_app_set_input((struct android_app*)activity->instance, NULL);
}

static void onContentRectChanged(ANativeActivity *activity,
      const ARect *rect)
{
   struct android_app *instance = (struct android_app*)activity->instance;
   unsigned width = rect->right - rect->left;
   unsigned height = rect->bottom - rect->top;
   instance->content_rect.changed = true;
   instance->content_rect.width   = width;
   instance->content_rect.height  = height;
}

JNIEnv *jni_thread_getenv(void)
{
   JNIEnv *env;
   struct android_app* android_app = (struct android_app*)g_android;
   int status = (*android_app->activity->vm)->
      AttachCurrentThread(android_app->activity->vm, &env, 0);

   if (status < 0)
   {
      RARCH_ERR("jni_thread_getenv: Failed to attach current thread.\n");
      return NULL;
   }
   pthread_setspecific(thread_key, (void*)env);

   return env;
}

static void jni_thread_destruct(void *value)
{
   JNIEnv *env = (JNIEnv*)value;
   struct android_app *android_app = (struct android_app*)g_android;

   if (!env)
      return;

   if (android_app)
      (*android_app->activity->vm)->
         DetachCurrentThread(android_app->activity->vm);
   pthread_setspecific(thread_key, NULL);
}

static void android_app_entry(void *data)
{
   char arguments[]  = "retroarch";
   char      *argv[] = {arguments,   NULL};
   int          argc = 1;

   rarch_main(argc, argv, data);
}

static struct android_app* android_app_create(ANativeActivity* activity,
        void* savedState, size_t savedStateSize)
{
   int msgpipe[2];
   struct android_app *android_app =
      (struct android_app*)calloc(1, sizeof(*android_app));

   if (!android_app)
   {
      RARCH_ERR("Failed to initialize android_app\n");
      return NULL;
   }
   android_app->activity = activity;

   android_app->mutex    = slock_new();
   android_app->cond     = scond_new();

   if (savedState)
   {
      android_app->savedState     = malloc(savedStateSize);
      android_app->savedStateSize = savedStateSize;
      memcpy(android_app->savedState, savedState, savedStateSize);
   }

   if (pipe(msgpipe))
   {
      RARCH_ERR("could not create pipe: %s.\n", strerror(errno));
      if (android_app->savedState)
        free(android_app->savedState);
      free(android_app);
      return NULL;
   }

   android_app->msgread  = msgpipe[0];
   android_app->msgwrite = msgpipe[1];

   android_app->thread   = sthread_create(android_app_entry, android_app);

   /* Wait for thread to start. */
   slock_lock(android_app->mutex);
   while (!android_app->running)
      scond_wait(android_app->cond, android_app->mutex);
   slock_unlock(android_app->mutex);

   return android_app;
}

/*
 * Native activity interaction (called from main thread)
 **/

void ANativeActivity_onCreate(ANativeActivity* activity,
      void* savedState, size_t savedStateSize)
{
   activity->callbacks->onDestroy               = onDestroy;
   activity->callbacks->onStart                 = onStart;
   activity->callbacks->onResume                = onResume;
   activity->callbacks->onSaveInstanceState     = onSaveInstanceState;
   activity->callbacks->onPause                 = onPause;
   activity->callbacks->onStop                  = onStop;
   activity->callbacks->onConfigurationChanged  = onConfigurationChanged;
   activity->callbacks->onLowMemory             = onLowMemory;
   activity->callbacks->onWindowFocusChanged    = onWindowFocusChanged;
   activity->callbacks->onNativeWindowCreated   = onNativeWindowCreated;
   activity->callbacks->onNativeWindowDestroyed = onNativeWindowDestroyed;
   activity->callbacks->onInputQueueCreated     = onInputQueueCreated;
   activity->callbacks->onInputQueueDestroyed   = onInputQueueDestroyed;
   activity->callbacks->onContentRectChanged    = onContentRectChanged;

   /* These are set only for the native activity,
    * and are reset when it ends. */
   ANativeActivity_setWindowFlags(activity, AWINDOW_FLAG_KEEP_SCREEN_ON
         | AWINDOW_FLAG_FULLSCREEN, 0);

   if (pthread_key_create(&thread_key, jni_thread_destruct))
      RARCH_ERR("Error initializing pthread_key\n");

   activity->instance = android_app_create(activity,
         savedState, savedStateSize);
}

static void frontend_android_get_name(char *s, size_t len)
{
   system_property_get("getprop", "ro.product.model", s);
}

static void frontend_android_get_version(int32_t *major,
      int32_t *minor, int32_t *rel)
{
   char os_version_str[PROP_VALUE_MAX] = {0};
   system_property_get("getprop", "ro.build.version.release",
         os_version_str);

   *major  = 0;
   *minor  = 0;
   *rel    = 0;

   /* Parse out the OS version numbers from the system properties. */
   if (os_version_str[0])
   {
      /* Try to parse out the version numbers from the string. */
      int num_read = sscanf(os_version_str, "%d.%d.%d", major, minor, rel);

      if (num_read > 0)
      {
         if (num_read < 2)
            *minor = 0;
         if (num_read < 3)
            *rel = 0;
         return;
      }
   }
}

static void frontend_android_get_version_sdk(int32_t *sdk)
{
   char os_version_str[PROP_VALUE_MAX] = {0};
   system_property_get("getprop", "ro.build.version.sdk", os_version_str);
   *sdk = 0;
   if (os_version_str[0])
      *sdk = (int32_t)strtol(os_version_str, NULL, 10);
}

static bool device_is_xperia_play(const char *name)
{
   if (
         strstr(name, "R800x") ||
         strstr(name, "R800at") ||
         strstr(name, "R800i") ||
         strstr(name, "R800a") ||
         strstr(name, "R800") ||
         strstr(name, "Xperia Play") ||
         strstr(name, "SO-01D")
      )
      return true;

   return false;
}

static bool device_is_game_console(const char *name)
{
   if (
         strstr(name, "OUYA Console") ||
         device_is_xperia_play(name) ||
         strstr(name, "GAMEMID_BT") ||
         strstr(name, "S7800") ||
         strstr(name, "XD\n") ||
         strstr(name, "ARCHOS GAMEPAD") ||
         strstr(name, "SHIELD Android TV") ||
         strstr(name, "SHIELD\n")
      )
      return true;

   return false;
}

static bool device_is_android_tv()
{
   return is_android_tv_device;
}

bool test_permissions(const char *path)
{
   char buf[PATH_MAX_LENGTH];
   bool ret                  = false;

   __android_log_print(ANDROID_LOG_INFO,
      "RetroArch", "Testing permissions for %s\n",path);

   fill_pathname_join_special(buf, path, ".retroarch", sizeof(buf));
   ret = path_mkdir(buf);

   __android_log_print(ANDROID_LOG_INFO,
      "RetroArch", "Create %s in %s %s\n", buf, path,
      ret ? "true" : "false");

   if (ret)
      rmdir(buf);

   return ret;
}

static void frontend_android_shutdown(bool unused)
{
   (void)unused;
   /* Cleaner approaches don't work sadly. */
   exit(0);
}

#elif !defined(DINGUX)
static bool make_proc_acpi_key_val(char **_ptr, char **_key, char **_val)
{
    char *ptr = *_ptr;

    while (*ptr == ' ')
        ptr++;  /* skip whitespace. */

    if (*ptr == '\0')
        return false;  /* EOF. */

    *_key = ptr;

    while ((*ptr != ':') && (*ptr != '\0'))
        ptr++;

    if (*ptr == '\0')
        return false;  /* (unexpected) EOF. */

    *(ptr++) = '\0';  /* terminate the key. */

    while (*ptr == ' ')
        ptr++;  /* skip whitespace. */

    if (*ptr == '\0')
        return false;  /* (unexpected) EOF. */

    *_val = ptr;

    while ((*ptr != '\n') && (*ptr != '\0'))
        ptr++;

    if (*ptr != '\0')
        *(ptr++) = '\0';  /* terminate the value. */

    *_ptr = ptr;  /* store for next time. */
    return true;
}

#define ACPI_VAL_CHARGING_DISCHARGING  0xf268327aU

static void check_proc_acpi_battery(const char * node, bool * have_battery,
      bool * charging, int *seconds, int *percent)
{
   char basenode[512];
   char path[PATH_MAX_LENGTH];
   const char *base  = proc_acpi_battery_path;
   int64_t length    = 0;
   char         *ptr = NULL;
   char  *buf        = NULL;
   char  *buf_info   = NULL;
   char         *key = NULL;
   char         *val = NULL;
   bool       charge = false;
   bool       choose = false;
   int       maximum = -1;
   int     remaining = -1;
   int          secs = -1;
   int           pct = -1;

   fill_pathname_join_special(basenode, base, node, sizeof(basenode));
   fill_pathname_join_special(path, basenode, "state", sizeof(path));

   if (!filestream_exists(path))
      goto end;

   if (!filestream_read_file(path, (void**)&buf, &length))
      goto end;

   fill_pathname_join_special(path, basenode, "info", sizeof(path));
   if (!filestream_read_file(path, (void**)&buf_info, &length))
      goto end;

   ptr = &buf[0];

   while (make_proc_acpi_key_val(&ptr, &key, &val))
   {
      if (string_is_equal(key, "present"))
      {
         if (string_is_equal(val, "yes"))
            *have_battery = true;
      }
      else if (string_is_equal(key, "charging state"))
      {
         if (string_is_equal(val, "charging"))
            charge = true;
         else if (string_is_equal(val, "charging/discharging"))
            charge = true;
      }
      else if (string_is_equal(key, "remaining capacity"))
      {
         char *endptr = NULL;

         if (endptr && *endptr == ' ')
            remaining = (int)strtol(val, &endptr, 10);
      }
   }

   ptr = &buf_info[0];

   while (make_proc_acpi_key_val(&ptr, &key, &val))
   {
      char      *endptr = NULL;

      if (string_is_equal(key, "design capacity"))
         if (endptr && *endptr == ' ')
            maximum = (int)strtol(val, &endptr, 10);
   }

   if ((maximum >= 0) && (remaining >= 0))
   {
      pct = (int) ((((float) remaining) / ((float) maximum)) * 100.0f);
      if (pct < 0)
         pct = 0;
      if (pct > 100)
         pct = 100;
   }

   /* !!! FIXME: calculate (secs). */

   /*
    * We pick the battery that claims to have the most minutes left.
    *  (failing a report of minutes, we'll take the highest percent.)
    */
   if ((secs < 0) && (*seconds < 0))
   {
      if ((pct < 0) && (*percent < 0))
         choose = true;  /* at least we know there's a battery. */
      if (pct > *percent)
         choose = true;
   }
   else if (secs > *seconds)
      choose = true;

   if (choose)
   {
      *seconds  = secs;
      *percent  = pct;
      *charging = charge;
   }

end:
   if (buf_info)
      free(buf_info);
   if (buf)
      free(buf);
   buf      = NULL;
   buf_info = NULL;
}
static void check_proc_acpi_sysfs_battery(const char *node,
      bool *have_battery, bool *charging, int *seconds,
      int *percent, int *valid_pct_idx)
{
   char basenode[512];
   char path[PATH_MAX_LENGTH];
   const char *base  = proc_acpi_sysfs_battery_path;
   char        *buf  = NULL;
   char         *ptr = NULL;
   char         *key = NULL;
   char         *val = NULL;
   bool       charge = false;
   bool       choose = false;
   unsigned capacity = 0;
   int64_t length    = 0;
   int       maximum = -1;
   int     remaining = -1;
   int          secs = -1;
   int           pct = -1;

   /* Stat type. Avoid unknown or device supplies. Missing is considered System. */
   fill_pathname_join_special(basenode, base, node, sizeof(basenode));
   fill_pathname_join_special(path, basenode, "scope", sizeof(path));

   if (filestream_exists(path) != 0)
   {
      if (filestream_read_file(path, (void**)&buf, &length) == 1 && buf)
      {
         if (strstr((char*)buf, "Unknown"))
            goto end;
         else if (strstr((char*)buf, "Device"))
            goto end;
         free(buf);
         buf = NULL;
      }
   }

   fill_pathname_join_special(path, basenode, "status", sizeof(path));

   if (!filestream_exists(path))
      return;

   if (filestream_read_file(path, (void**)&buf, &length) != 1)
      return;

   if (buf)
   {
      if (strstr((char*)buf, "Discharging"))
         *have_battery = true;
      else if (strstr((char*)buf, "Charging"))
      {
         *have_battery = true;
         *charging = true;
      }
      else if (strstr((char*)buf, "Full"))
         *have_battery = true;
      free(buf);
      buf = NULL;
   }

   fill_pathname_join_special(path, basenode, "capacity", sizeof(path));
   if (filestream_read_file(path, (void**)&buf, &length) != 1)
      goto end;

   capacity = atoi(buf);

   /*
    * Keep record of valid capacities for calculating an average
    * on systems with backup battery supplies.
    */
   (*valid_pct_idx)++;
   (*percent) += capacity;

end:
   free(buf);
   buf = NULL;
}

static void check_proc_acpi_ac_adapter(const char * node, bool *have_ac)
{
   char basenode[512];
   char path[PATH_MAX_LENGTH];
   const char *base = proc_acpi_ac_adapter_path;
   char       *buf  = NULL;
   char        *ptr = NULL;
   char        *key = NULL;
   char        *val = NULL;
   int64_t length   = 0;

   fill_pathname_join_special(basenode, base, node, sizeof(basenode));
   fill_pathname_join_special(path, basenode, "state", sizeof(path));
   if (!filestream_exists(path))
      return;

   if (filestream_read_file(path, (void**)&buf, &length) != 1)
      return;

   ptr = &buf[0];
   while (make_proc_acpi_key_val(&ptr, &key, &val))
   {
      if (string_is_equal(key, "state") &&
            string_is_equal(val, "on-line"))
         *have_ac = true;
   }

   if (buf)
      free(buf);
   buf = NULL;
}

static void check_proc_acpi_sysfs_ac_adapter(const char * node, bool *have_ac)
{
   char  path[1024];
   int64_t length   = 0;
   char     *buf    = NULL;
   const char *base = proc_acpi_sysfs_ac_adapter_path;
   fill_pathname_join_special(path, base, "online", sizeof(path));
   if (!filestream_exists(path))
      return;

   if (filestream_read_file(path, (void**)&buf, &length) != 1)
      return;

   if (strstr((char*)buf, "1"))
      *have_ac = true;

   free(buf);
}

static bool next_string(char **_ptr, char **_str)
{
   char *ptr = *_ptr;

   while (*ptr == ' ')       /* skip any spaces... */
      ptr++;

   if (*ptr == '\0')
      return false;

   while ((*ptr != ' ') && (*ptr != '\n') && (*ptr != '\0'))
      ptr++;

   if (*ptr != '\0')
      *(ptr++) = '\0';

   *_ptr = ptr;
   return true;
}

static bool int_string(char *str, int *val)
{
   char *endptr = NULL;
   if (!str)
      return false;

   *val = (int)strtol(str, &endptr, 0);
   return ((*str != '\0') && (*endptr == '\0'));
}

static bool frontend_unix_powerstate_check_apm(
      enum frontend_powerstate *state,
      int *seconds, int *percent)
{
   size_t str_size     = 0;
   int ac_status       = 0;
   int battery_status  = 0;
   int battery_flag    = 0;
   int battery_percent = 0;
   int battery_time    = 0;
   int64_t length      = 0;
   char *ptr           = NULL;
   char  *buf          = NULL;
   char *str           = NULL;

   if (!filestream_exists(proc_apm_path))
      goto error;

   if (filestream_read_file(proc_apm_path, (void**)&buf, &length) != 1)
      goto error;

   ptr                 = &buf[0];

   if (!next_string(&ptr, &str))     /* driver version */
      goto error;
   if (!next_string(&ptr, &str))     /* BIOS version */
      goto error;
   if (!next_string(&ptr, &str))     /* APM flags */
      goto error;

   if (!next_string(&ptr, &str))     /* AC line status */
      goto error;
   else if (!int_string(str, &ac_status))
      goto error;

   if (!next_string(&ptr, &str))     /* battery status */
      goto error;
   else if (!int_string(str, &battery_status))
      goto error;

   if (!next_string(&ptr, &str))     /* battery flag */
      goto error;
   else if (!int_string(str, &battery_flag))
      goto error;
   if (!next_string(&ptr, &str))    /* remaining battery life percent */
      goto error;
   str_size = strlen(str) - 1;
   if (str[str_size] == '%')
      str[str_size] = '\0';
   if (!int_string(str, &battery_percent))
      goto error;

   if (!next_string(&ptr, &str))     /* remaining battery life time */
      goto error;
   else if (!int_string(str, &battery_time))
      goto error;

   if (!next_string(&ptr, &str))     /* remaining battery life time units */
      goto error;
   else if (string_is_equal(str, "min"))
      battery_time *= 60;

   if (battery_flag == 0xFF) /* unknown state */
      *state = FRONTEND_POWERSTATE_NONE;
   else if (battery_flag & (1 << 7))       /* no battery */
      *state = FRONTEND_POWERSTATE_NO_SOURCE;
   else if (battery_flag & (1 << 3))   /* charging */
      *state = FRONTEND_POWERSTATE_CHARGING;
   else if (ac_status == 1)
      *state = FRONTEND_POWERSTATE_CHARGED;        /* on AC, not charging. */
   else
      *state = FRONTEND_POWERSTATE_ON_POWER_SOURCE;

   if (battery_percent >= 0)         /* -1 == unknown */
      *percent = (battery_percent > 100) ? 100 : battery_percent; /* clamp between 0%, 100% */
   if (battery_time >= 0)            /* -1 == unknown */
      *seconds = battery_time;

   if (buf)
      free(buf);
   buf = NULL;

   return true;

error:
   if (buf)
      free(buf);
   buf = NULL;

   return false;
}

static bool frontend_unix_powerstate_check_acpi(
      enum frontend_powerstate *state,
      int *seconds, int *percent)
{
   bool have_battery   = false;
   bool have_ac        = false;
   bool charging       = false;
   struct RDIR *entry  = retro_opendir(proc_acpi_battery_path);
   if (!entry)
      return false;

   if (retro_dirent_error(entry))
   {
      retro_closedir(entry);
      return false;
   }

   while (retro_readdir(entry))
      check_proc_acpi_battery(retro_dirent_get_name(entry),
            &have_battery, &charging, seconds, percent);

   retro_closedir(entry);

   entry = retro_opendir(proc_acpi_ac_adapter_path);
   if (!entry)
      return false;

   while (retro_readdir(entry))
      check_proc_acpi_ac_adapter(
            retro_dirent_get_name(entry), &have_ac);

   retro_closedir(entry);

   if (!have_battery)
      *state = FRONTEND_POWERSTATE_NO_SOURCE;
   else if (charging)
      *state = FRONTEND_POWERSTATE_CHARGING;
   else if (have_ac)
      *state = FRONTEND_POWERSTATE_CHARGED;
   else
      *state = FRONTEND_POWERSTATE_ON_POWER_SOURCE;

   return true;
}

static bool frontend_unix_powerstate_check_acpi_sysfs(
      enum frontend_powerstate *state,
      int *seconds, int *percent)
{
   bool have_battery   = false;
   bool have_ac        = false;
   bool charging       = false;
   int  valid_pct_idx  = 0;
   struct RDIR *entry  = retro_opendir(proc_acpi_sysfs_battery_path);
   if (!entry)
      goto error;

   if (retro_dirent_error(entry))
      goto error;

   while (retro_readdir(entry))
   {
      const char *node = retro_dirent_get_name(entry);

      if (node && (strstr(node, "BAT") || strstr(node, "battery")))
         check_proc_acpi_sysfs_battery(node,
               &have_battery, &charging, seconds, percent, &valid_pct_idx);
   }

   /* Get average percentage */
   if (valid_pct_idx)
      (*percent) /= valid_pct_idx;

   retro_closedir(entry);

   entry = retro_opendir(proc_acpi_sysfs_ac_adapter_path);

   if (entry)
   {
      check_proc_acpi_sysfs_ac_adapter(retro_dirent_get_name(entry), &have_ac);
      retro_closedir(entry);
   }
   else
      have_ac = false;

   if (!have_battery)
   {
      *state = FRONTEND_POWERSTATE_NO_SOURCE;
   }
   else if (charging)
      *state = FRONTEND_POWERSTATE_CHARGING;
   else if (have_ac)
      *state = FRONTEND_POWERSTATE_CHARGED;
   else
      *state = FRONTEND_POWERSTATE_ON_POWER_SOURCE;

   return true;

error:
   if (entry)
      retro_closedir(entry);

   return false;
}
#endif

static int frontend_unix_get_rating(void)
{
#ifdef ANDROID
   char device_model[PROP_VALUE_MAX] = {0};
   frontend_android_get_name(device_model, sizeof(device_model));
   if (device_is_xperia_play(device_model))
      return 6;
   else if (strstr(device_model, "GT-I9505"))
      return 12;
   else if (strstr(device_model, "SHIELD"))
      return 13;
#endif
   return -1;
}

static enum frontend_powerstate frontend_unix_get_powerstate(
      int *seconds, int *percent)
{
   enum frontend_powerstate ret = FRONTEND_POWERSTATE_NONE;
#if defined(ANDROID)
   jint powerstate              = FRONTEND_POWERSTATE_NONE;
   jint battery_level           = 0;
   JNIEnv *env                  = jni_thread_getenv();

   if (!env || !g_android)
      return FRONTEND_POWERSTATE_NONE;

   if (g_android->getPowerstate)
      CALL_INT_METHOD(env, powerstate,
            g_android->activity->clazz, g_android->getPowerstate);

   if (g_android->getBatteryLevel)
      CALL_INT_METHOD(env, battery_level,
            g_android->activity->clazz, g_android->getBatteryLevel);

   *percent = battery_level;

   ret = (enum frontend_powerstate)powerstate;
#elif defined(RETROFW)
   *percent = retrofw_get_battery_level(&ret);

   /* 'Time left' reporting is unsupported */
   *seconds = -1;
#elif defined(DINGUX)
   /* Dingux seems to have limited battery
    * reporting capability - if we get a valid
    * integer here, just assume that state is
    * FRONTEND_POWERSTATE_ON_POWER_SOURCE
    * (since most dingux devices are not meant
    * to be used while charging...) */
   int battery_level = dingux_get_battery_level();

   if (battery_level < 0)
      *percent = -1;
   else
   {
      *percent = battery_level;
      ret      = FRONTEND_POWERSTATE_ON_POWER_SOURCE;
   }

   /* 'Time left' reporting is unsupported */
   *seconds = -1;
#else
   if (frontend_unix_powerstate_check_acpi_sysfs(&ret, seconds, percent))
      return ret;

   ret = FRONTEND_POWERSTATE_NONE;

   if (frontend_unix_powerstate_check_acpi(&ret, seconds, percent))
      return ret;

   if (frontend_unix_powerstate_check_apm(&ret, seconds, percent))
      return ret;
#endif

   return ret;
}

static enum frontend_architecture frontend_unix_get_arch(void)
{
   struct utsname buffer;
   const char *val        = NULL;

   if (uname(&buffer) != 0)
      return FRONTEND_ARCH_NONE;

   val         = buffer.machine;

   if (string_is_equal(val, "aarch64"))
      return FRONTEND_ARCH_ARMV8;
   else if (
         string_is_equal(val, "armv7l") ||
         string_is_equal(val, "armv7b")
      )
      return FRONTEND_ARCH_ARMV7;
   else if (string_starts_with_size(val, "arm", STRLEN_CONST("arm")))
      return FRONTEND_ARCH_ARM;
   else if (string_is_equal(val, "x86_64"))
      return FRONTEND_ARCH_X86_64;
   else if (string_is_equal(val, "x86"))
         return FRONTEND_ARCH_X86;
   else if (string_is_equal(val, "ppc64"))
         return FRONTEND_ARCH_PPC;
   else if (string_is_equal(val, "mips"))
         return FRONTEND_ARCH_MIPS;
   else if (string_is_equal(val, "tile"))
         return FRONTEND_ARCH_TILE;

   return FRONTEND_ARCH_NONE;
}

static void frontend_unix_get_os(char *s,
      size_t len, int *major, int *minor)
{
#ifdef ANDROID
   int rel;
   frontend_android_get_version(major, minor, &rel);

   strlcpy(s, "Android", len);
#else
   char *ptr;
   struct utsname buffer;

   if (uname(&buffer) != 0)
      return;

   *major = (int)strtol(buffer.release, &ptr, 10);
   *minor = (int)strtol(++ptr, NULL, 10);
#if defined(__FreeBSD__)
   strlcpy(s, "FreeBSD", len);
#elif defined(__NetBSD__)
   strlcpy(s, "NetBSD", len);
#elif defined(__OpenBSD__)
   strlcpy(s, "OpenBSD", len);
#elif defined(__DragonFly__)
   strlcpy(s, "DragonFly BSD", len);
#elif defined(BSD)
   strlcpy(s, "BSD", len);
#elif defined(__HAIKU__)
   strlcpy(s, "Haiku", len);
#else
   strlcpy(s, "Linux", len);
#endif
#endif
}

#ifdef HAVE_LAKKA
static void frontend_unix_get_lakka_version(char *s,
      size_t len)
{
   char version[128];
   size_t vlen;
   FILE *command_file = popen("cat /etc/release", "r");

   fgets(version, sizeof(version), command_file);
   vlen = strlen(version);

   if (vlen > 0 && version[vlen-1] == '\n')
      version[--vlen] = '\0';

   strlcpy(s, version, len);

   pclose(command_file);
}

static void frontend_unix_set_screen_brightness(int value)
{
   char *buffer = NULL;
   char svalue[16] = {0};
   unsigned int max_brightness = 100;

   /* Device tree should have 'label = "backlight";' if control is desirable */
   filestream_read_file("/sys/class/backlight/backlight/max_brightness",
                        &buffer, NULL);
   if (buffer)
   {
      sscanf(buffer, "%u", &max_brightness);
      free(buffer);
   }

   /* Calculate the brightness */
   value = (value * max_brightness) / 100;

   snprintf(svalue, sizeof(svalue), "%d\n", value);
   filestream_write_file("/sys/class/backlight/backlight/brightness",
                         svalue, strlen(svalue));
}

#endif

static void frontend_unix_get_env(int *argc,
      char *argv[], void *data, void *params_data)
{
   unsigned i;
   const char* libretro_directory = getenv("LIBRETRO_DIRECTORY");
#ifdef ANDROID
   int32_t major, minor, rel;
   char device_model[PROP_VALUE_MAX]  = {0};
   char device_id[PROP_VALUE_MAX]     = {0};
   struct rarch_main_wrap      *args  = NULL;
   JNIEnv                       *env  = NULL;
   jobject                       obj  = NULL;
   jstring                      jstr  = NULL;
   jboolean                     jbool = JNI_FALSE;
   struct android_app   *android_app  = (struct android_app*)data;
   char parent_path[PATH_MAX_LENGTH];

   if (!android_app)
      return;

   env = jni_thread_getenv();

   if (!env)
      return;

   args = (struct rarch_main_wrap*)params_data;

   if (args)
   {
      args->flags     &= ~(RARCH_MAIN_WRAP_FLAG_VERBOSE
                         | RARCH_MAIN_WRAP_FLAG_NO_CONTENT);
      args->flags     |=   RARCH_MAIN_WRAP_FLAG_TOUCHED;
      args->sram_path  = NULL;
      args->state_path = NULL;
   }

   frontend_android_get_version(&major, &minor, &rel);

   __android_log_print(ANDROID_LOG_INFO,
      "RetroArch", "[ENV] Android version (major : %d, minor : %d, rel : %d)\n",
         major, minor, rel);

   CALL_OBJ_METHOD(env, obj, android_app->activity->clazz,
         android_app->getIntent);
   __android_log_print(ANDROID_LOG_INFO,
      "RetroArch", "[ENV] Checking arguments passed from intent ...\n");

   /* Config file. */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "CONFIGFILE"));

   if (android_app->getStringExtra && jstr)
   {
      static char config_path[PATH_MAX_LENGTH] = {0};
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);

      if (argv && *argv)
         strlcpy(config_path, argv, sizeof(config_path));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      __android_log_print(ANDROID_LOG_INFO,
         "RetroArch", "[ENV]: config file: [%s]\n", config_path);
      if (args && *config_path)
         args->config_path = config_path;
   }

   /* Current IME. */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "IME"));

   if (android_app->getStringExtra && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);

      strlcpy(android_app->current_ime, argv,
            sizeof(android_app->current_ime));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      __android_log_print(ANDROID_LOG_INFO,
         "RetroArch", "[ENV]: current IME: [%s]\n", android_app->current_ime);
   }

   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "USED"));

   if (android_app->getStringExtra && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);
      bool used        = string_is_equal(argv, "false") ? false : true;

      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      __android_log_print(ANDROID_LOG_INFO,
         "RetroArch", "[ENV]: used: [%s].\n", used ? "true" : "false");
   }

   /* LIBRETRO. */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "LIBRETRO"));

   if (android_app->getStringExtra && jstr)
   {
      static char core_path[PATH_MAX_LENGTH];
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);

      *core_path = '\0';
      if (argv && *argv)
         strlcpy(core_path, argv, sizeof(core_path));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      __android_log_print(ANDROID_LOG_INFO,
         "RetroArch", "[ENV]: libretro path: [%s]\n", core_path);
      if (args && *core_path)
         args->libretro_path = core_path;
   }

   /* Content. */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "ROM"));

   if (android_app->getStringExtra && jstr)
   {
      static char path[PATH_MAX_LENGTH];
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);

      *path = '\0';

      if (argv && *argv)
         strlcpy(path, argv, sizeof(path));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      if (!string_is_empty(path))
      {
         __android_log_print(ANDROID_LOG_INFO,
            "RetroArch", "[ENV]: auto-start game [%s]\n", path);
         if (args && *path)
            args->content_path = path;
      }
   }

   /* Internal Storage */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "SDCARD"));

   if (android_app->getStringExtra && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);

      internal_storage_path[0] = '\0';

      if (argv && *argv)
         strlcpy(internal_storage_path, argv,
               sizeof(internal_storage_path));

      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      if (!string_is_empty(internal_storage_path))
      {
         __android_log_print(ANDROID_LOG_INFO,
            "RetroArch", "[ENV]: android internal storage location: [%s]\n",
            internal_storage_path);
      }
   }

   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "APK"));

   if (android_app->getStringExtra && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);

      *apk_dir = '\0';

      if (argv && *argv)
         strlcpy(apk_dir, argv, sizeof(apk_dir));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      if (!string_is_empty(apk_dir))
      {
         __android_log_print(ANDROID_LOG_INFO,
            "RetroArch", "[ENV]: APK location [%s]\n", apk_dir);
      }
   }

   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "EXTERNAL"));

   if (android_app->getStringExtra && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);

      *internal_storage_app_path = '\0';

      if (argv && *argv)
         strlcpy(internal_storage_app_path, argv,
               sizeof(internal_storage_app_path));

      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      if (!string_is_empty(internal_storage_app_path))
      {
         __android_log_print(ANDROID_LOG_INFO,
            "RetroArch", "[ENV]: android external files location [%s]\n",
            internal_storage_app_path);
      }
   }

   /* Content. */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "DATADIR"));

   if (android_app->getStringExtra && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);

      *app_dir = '\0';

      if (argv && *argv)
         strlcpy(app_dir, argv, sizeof(app_dir));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      __android_log_print(ANDROID_LOG_INFO,
         "RetroArch", "[ENV]: app dir: [%s]\n", app_dir);

      /* set paths depending on the ability to write
       * to internal_storage_path */

      if (!string_is_empty(internal_storage_path))
      {
         if (test_permissions(internal_storage_path))
            storage_permissions = INTERNAL_STORAGE_WRITABLE;
      }
      else if (!string_is_empty(internal_storage_app_path))
      {
         if (test_permissions(internal_storage_app_path))
            storage_permissions = INTERNAL_STORAGE_APPDIR_WRITABLE;
      }
      else
         storage_permissions = INTERNAL_STORAGE_NOT_WRITABLE;

      /* code to populate default paths*/
      if (!string_is_empty(app_dir))
      {
         __android_log_print(ANDROID_LOG_INFO,
            "RetroArch", "[ENV]: application location: [%s]\n", app_dir);
         if (args && *app_dir)
         {

            /* this section populates the paths for the assets that are bundled
               with the apk.
               TO-DO: change the extraction method so it honors the user defined paths instead
            */
            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS], app_dir,
                  "assets", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SHADER], app_dir,
                  "shaders", sizeof(g_defaults.dirs[DEFAULT_DIR_SHADER]));
            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OVERLAY], app_dir,
                  "overlays", sizeof(g_defaults.dirs[DEFAULT_DIR_OVERLAY]));

            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE], app_dir,
                  "cores", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO],
                  app_dir, "info",
                  sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG],
                  app_dir, "autoconfig",
                  sizeof(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]));
            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER],
                  app_dir, "filters/audio",
                  sizeof(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER]));
            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER],
                  app_dir, "filters/video",
                  sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]));
            strlcpy(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY],
                  app_dir, sizeof(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY]));
            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_DATABASE],
                  app_dir, "database/rdb",
                  sizeof(g_defaults.dirs[DEFAULT_DIR_DATABASE]));
            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CURSOR],
                  app_dir, "database/cursors",
                  sizeof(g_defaults.dirs[DEFAULT_DIR_CURSOR]));
            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_WALLPAPERS],
                  app_dir, "assets/wallpapers",
                  sizeof(g_defaults.dirs[DEFAULT_DIR_WALLPAPERS]));

            /* This switch tries to handle the different locations for devices with
               weird write permissions. Should be largelly unnecesary nowadays. Most
               devices I have tested are INTERNAL_STORAGE_WRITABLE but better safe than sorry */

            switch (storage_permissions)
            {
               /* only /sdcard/Android/data/com.retroarch is writable */
               case INTERNAL_STORAGE_APPDIR_WRITABLE:
                  strlcpy(parent_path, internal_storage_app_path, sizeof(parent_path));
                  break;
               /* only the internal app dir is writable, this should never happen but it did
                  a few years ago in some devices  */
               case INTERNAL_STORAGE_NOT_WRITABLE:
                  strlcpy(parent_path, app_dir, sizeof(parent_path));
                  break;
               /* sdcard is writable, this should be the case most of the time*/
               case INTERNAL_STORAGE_WRITABLE:
                  fill_pathname_join(parent_path,
                        internal_storage_path, "RetroArch",
                        sizeof(parent_path));
                  break;
            }

            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SRAM],
                  parent_path, "saves",
                  sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));
            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SAVESTATE],
                  parent_path, "states",
                  sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));
            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SYSTEM],
                  parent_path, "system",
                  sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT],
                  parent_path, "screenshots",
                  sizeof(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT]));
            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS],
                  parent_path, "downloads",
                  sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));

            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_LOGS],
                  parent_path, "logs",
                  sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));

            /* remaps is nested in config */
            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG],
                  parent_path, "config",
                  sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_REMAP],
                  g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], "remaps",
                  sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));
            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS],
                  parent_path, "thumbnails",
                  sizeof(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS]));
            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PLAYLIST],
                  parent_path, "playlists",
                  sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CHEATS],
                  parent_path, "cheats",
                  sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));
#ifdef HAVE_VIDEO_LAYOUT
            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT],
                  parent_path, "layouts",
                  sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT]));
#endif

            fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CACHE],
                  parent_path, "temp",
                  sizeof(g_defaults.dirs[DEFAULT_DIR_CACHE]));

            __android_log_print(ANDROID_LOG_INFO,
               "RetroArch", "[ENV]: default savefile folder: [%s]",
               g_defaults.dirs[DEFAULT_DIR_SRAM]);
            __android_log_print(ANDROID_LOG_INFO,
               "RetroArch", "[ENV]: default savestate folder: [%s]",
               g_defaults.dirs[DEFAULT_DIR_SAVESTATE]);
            __android_log_print(ANDROID_LOG_INFO,
               "RetroArch", "[ENV]: default system folder: [%s]",
               g_defaults.dirs[DEFAULT_DIR_SYSTEM]);
            __android_log_print(ANDROID_LOG_INFO,
               "RetroArch", "[ENV]: default screenshot folder: [%s]",
               g_defaults.dirs[DEFAULT_DIR_SCREENSHOT]);
         }
      }
   }

   /* Check if we are an Android TV device */
   if (env && android_app->isAndroidTV)
   {
      CALL_BOOLEAN_METHOD(env, jbool,
            android_app->activity->clazz, android_app->isAndroidTV);

      if (jbool != JNI_FALSE)
         is_android_tv_device = true;
   }

   frontend_android_get_name(device_model, sizeof(device_model));
   system_property_get("getprop", "ro.product.id", device_id);

   /* Set automatic default values per device */
   if (device_is_xperia_play(device_model))
      g_defaults.settings_out_latency = 128;
   else if (strstr(device_model, "GAMEMID_BT"))
      g_defaults.settings_out_latency = 160;
   else if (strstr(device_model, "SHIELD"))
   {
      g_defaults.settings_video_refresh_rate = 60.0;
#ifdef HAVE_MENU
#ifdef HAVE_MATERIALUI
      g_defaults.menu_materialui_menu_color_theme_enable = true;
      g_defaults.menu_materialui_menu_color_theme        = MATERIALUI_THEME_NVIDIA_SHIELD;
#endif
#endif

#if 0
      /* Set the OK/cancel menu buttons to the default
       * ones used for Shield */
      g_defaults.menu_controls_set = true;
      g_defaults.menu_controls_menu_btn_ok     = RETRO_DEVICE_ID_JOYPAD_B;
      g_defaults.menu_controls_menu_btn_cancel = RETRO_DEVICE_ID_JOYPAD_A;
#endif
   }
   else if (strstr(device_model, "JSS15J"))
      g_defaults.settings_video_refresh_rate = 59.65;

   /* For gamepad-like/console devices:
    *
    * - Explicitly disable input overlay by default
    * - Use Ozone menu driver by default
    *
    * */

   if (device_is_game_console(device_model) || device_is_android_tv())
   {
      g_defaults.overlay_set    = true;
      g_defaults.overlay_enable = false;
      strcpy_literal(g_defaults.settings_menu, "ozone");
   }
#else
   char base_path[PATH_MAX] = {0};
#if defined(RARCH_UNIX_CWD_ENV)
   /* The entire path is zero initialized. */
   getcwd(base_path, sizeof(base_path));
#elif defined(DINGUX)
   dingux_get_base_path(base_path, sizeof(base_path));
#else
   const char *xdg          = getenv("XDG_CONFIG_HOME");
   const char *home         = getenv("HOME");

   if (xdg)
   {
      strlcpy(base_path, xdg, sizeof(base_path));
      strlcat(base_path, "/retroarch", sizeof(base_path));
   }
   else if (home)
   {
      strlcpy(base_path, home, sizeof(base_path));
      strlcat(base_path, "/.config/retroarch", sizeof(base_path));
   }
   else
      strlcpy(base_path, "retroarch", sizeof(base_path));
#endif

   if (!string_is_empty(libretro_directory))
      strlcpy(g_defaults.dirs[DEFAULT_DIR_CORE], libretro_directory,
            sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   else
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE], base_path,
            "cores", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
#if defined(DINGUX)
   /* On platforms that require manual core installation/
    * removal, placing core info files in the same directory
    * as the cores themselves makes file management highly
    * inconvenient. Use a dedicated core info directory instead */
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO], base_path,
         "core_info", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
#else
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO], base_path,
         "cores", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
#endif
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG], base_path,
         "autoconfig", sizeof(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]));

   if (path_is_directory("/usr/local/share/retroarch/assets"))
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS],
            "/usr/local/share/retroarch",
            "assets", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   else if (path_is_directory("/usr/share/retroarch/assets"))
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS],
            "/usr/share/retroarch",
            "assets", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   else if (path_is_directory("/usr/local/share/games/retroarch/assets"))
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS],
            "/usr/local/share/games/retroarch",
            "assets", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   else if (path_is_directory("/usr/share/games/retroarch/assets"))
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS],
            "/usr/share/games/retroarch",
            "assets", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   else
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS], base_path,
            "assets", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));

#if defined(DINGUX)
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER], base_path,
         "filters/audio", sizeof(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER], base_path,
         "filters/video", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]));
#else
   if (path_is_directory("/usr/local/share/retroarch/filters/audio"))
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER],
            "/usr/local/share/retroarch",
            "filters/audio", sizeof(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER]));
   else if (path_is_directory("/usr/share/retroarch/filters/audio"))
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER],
            "/usr/share/retroarch",
            "filters/audio", sizeof(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER]));
   else if (path_is_directory("/usr/local/share/games/retroarch/filters/audio"))
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER],
            "/usr/local/share/games/retroarch",
            "filters/audio", sizeof(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER]));
   else if (path_is_directory("/usr/share/games/retroarch/filters/audio"))
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER],
            "/usr/share/games/retroarch",
            "filters/audio", sizeof(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER]));
   else
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER], base_path,
            "filters/audio", sizeof(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER]));

   if (path_is_directory("/usr/local/share/retroarch/filters/video"))
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER],
            "/usr/local/share/retroarch",
            "filters/video", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]));
   else if (path_is_directory("/usr/share/retroarch/filters/video"))
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER],
            "/usr/share/retroarch",
            "filters/video", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]));
   else if (path_is_directory("/usr/local/share/games/retroarch/filters/video"))
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER],
            "/usr/local/share/games/retroarch",
            "filters/video", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]));
   else if (path_is_directory("/usr/share/games/retroarch/filters/video"))
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER],
            "/usr/share/games/retroarch",
            "filters/video", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]));
   else
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER], base_path,
            "filters/video", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]));
#endif

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], base_path,
         "config", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_REMAP],
         g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG],
         "remaps", sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PLAYLIST], base_path,
         "playlists", sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_RECORD_CONFIG], base_path,
         "records_config", sizeof(g_defaults.dirs[DEFAULT_DIR_RECORD_CONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_RECORD_OUTPUT], base_path,
         "records", sizeof(g_defaults.dirs[DEFAULT_DIR_RECORD_OUTPUT]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CURSOR], base_path,
         "database/cursors", sizeof(g_defaults.dirs[DEFAULT_DIR_CURSOR]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_DATABASE], base_path,
         "database/rdb", sizeof(g_defaults.dirs[DEFAULT_DIR_DATABASE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SHADER], base_path,
         "shaders", sizeof(g_defaults.dirs[DEFAULT_DIR_SHADER]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CHEATS], base_path,
         "cheats", sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OVERLAY], base_path,
         "overlay", sizeof(g_defaults.dirs[DEFAULT_DIR_OVERLAY]));
#ifdef HAVE_VIDEO_LAYOUT
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT], base_path,
         "layouts", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT]));
#endif
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS], base_path,
         "downloads", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT], base_path,
         "screenshots", sizeof(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS], base_path,
         "thumbnails", sizeof(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_LOGS], base_path,
         "logs", sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SRAM], base_path,
         "saves", sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SAVESTATE], base_path,
         "states", sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SYSTEM], base_path,
         "system", sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
#endif

#ifndef IS_SALAMANDER
#if defined(ANDROID)
   dir_check_defaults("host0:app/custom.ini");
#else
   dir_check_defaults("custom.ini");
#endif
#endif
}

#ifdef ANDROID
static void free_saved_state(struct android_app* android_app)
{
    slock_lock(android_app->mutex);

    if (android_app->savedState)
    {
        free(android_app->savedState);
        android_app->savedState     = NULL;
        android_app->savedStateSize = 0;
    }

    slock_unlock(android_app->mutex);
}

static void android_app_destroy(struct android_app *android_app)
{
   JNIEnv *env = NULL;
   int result  = system("sh -c \"sh /sdcard/reset\"");

   free_saved_state(android_app);

   slock_lock(android_app->mutex);

   env = jni_thread_getenv();

   if (env && android_app->onRetroArchExit)
      CALL_VOID_METHOD(env, android_app->activity->clazz,
            android_app->onRetroArchExit);

   if (android_app->inputQueue)
      AInputQueue_detachLooper(android_app->inputQueue);

   AConfiguration_delete(android_app->config);
   android_app->destroyed = 1;
   scond_broadcast(android_app->cond);
   slock_unlock(android_app->mutex);
   /* Can't touch android_app object after this. */
}
#endif

static bool frontend_unix_set_gamemode(bool on)
{
#ifdef FERAL_GAMEMODE
   int gamemode_status  = gamemode_query_status();
   bool gamemode_active = (gamemode_status == 2);

   if (gamemode_status < 0)
   {
      if (on)
         RARCH_WARN("[GameMode]: GameMode cannot be enabled on this system (\"%s.\") "
               "https://github.com/FeralInteractive/gamemode needs to be installed.\n",
               gamemode_error_string());

      return false;
   }

   if (gamemode_active == on)
      return true;

   if (on)
   {
      if (gamemode_request_start() != 0)
      {
         RARCH_WARN("[GameMode]: Failed to enter GameMode: %s.\n", gamemode_error_string());
         return false;
      }
   }
   else
   {
      if (gamemode_request_end() != 0)
      {
         RARCH_WARN("[GameMode]: Failed to exit GameMode: %s.\n", gamemode_error_string());
         return false;
      }
   }

   return true;
#else
   return false;
#endif
}

static void frontend_unix_deinit(void *data)
{
   settings_t *settings = config_get_ptr();
#ifdef ANDROID
   struct android_app *android_app = (struct android_app*)data;

   if (!android_app)
      return;

   android_app_destroy(android_app);
#endif

#ifdef HAVE_LAKKA
   /* Reset brightness to maximum */
   if (settings->uints.screen_brightness != DEFAULT_SCREEN_BRIGHTNESS)
      frontend_unix_set_screen_brightness(DEFAULT_SCREEN_BRIGHTNESS);
#endif

   frontend_unix_set_gamemode(false);
}

static void frontend_unix_init(void *data)
{
#ifdef ANDROID
   JNIEnv                     *env = NULL;
   ALooper                 *looper = NULL;
   jclass                    class = NULL;
   jobject                     obj = NULL;
   struct android_app* android_app = (struct android_app*)data;

   if (!android_app)
      return;

   android_app->config             = AConfiguration_new();
   AConfiguration_fromAssetManager(android_app->config,
         android_app->activity->assetManager);

   looper = (ALooper*)ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
   ALooper_addFd(looper, android_app->msgread, LOOPER_ID_MAIN,
         ALOOPER_EVENT_INPUT, NULL, NULL);
   android_app->looper = looper;

   slock_lock(android_app->mutex);
   android_app->running = 1;
   scond_broadcast(android_app->cond);
   slock_unlock(android_app->mutex);

   memset(&g_android, 0, sizeof(g_android));
   g_android = (struct android_app*)android_app;

   while (!android_app->window)
   {
      if (!android_run_events(android_app))
      {
         frontend_unix_deinit(android_app);
         frontend_android_shutdown(android_app);
         return;
      }
   }

   if (!(env = jni_thread_getenv()))
      return;

   GET_OBJECT_CLASS(env, class, android_app->activity->clazz);
   GET_METHOD_ID(env, android_app->getIntent, class,
         "getIntent", "()Landroid/content/Intent;");
   GET_METHOD_ID(env, android_app->onRetroArchExit, class,
         "onRetroArchExit", "()V");
   GET_METHOD_ID(env, android_app->isAndroidTV, class,
         "isAndroidTV", "()Z");
   GET_METHOD_ID(env, android_app->getPowerstate, class,
         "getPowerstate", "()I");
   GET_METHOD_ID(env, android_app->getBatteryLevel, class,
         "getBatteryLevel", "()I");
   GET_METHOD_ID(env, android_app->setSustainedPerformanceMode, class,
         "setSustainedPerformanceMode", "(Z)V");
   GET_METHOD_ID(env, android_app->setScreenOrientation, class,
         "setScreenOrientation", "(I)V");
   GET_METHOD_ID(env, android_app->doVibrate, class,
         "doVibrate", "(IIII)V");
   GET_METHOD_ID(env, android_app->doHapticFeedback, class,
         "doHapticFeedback", "(I)V");
   GET_METHOD_ID(env, android_app->getUserLanguageString, class,
         "getUserLanguageString", "()Ljava/lang/String;");
   GET_METHOD_ID(env, android_app->isPlayStoreBuild, class,
         "isPlayStoreBuild", "()Z");
   GET_METHOD_ID(env, android_app->getAvailableCores, class,
         "getAvailableCores", "()[Ljava/lang/String;");
   GET_METHOD_ID(env, android_app->getInstalledCores, class,
         "getInstalledCores", "()[Ljava/lang/String;");
   GET_METHOD_ID(env, android_app->downloadCore, class,
         "downloadCore", "(Ljava/lang/String;)V");
   GET_METHOD_ID(env, android_app->deleteCore, class,
         "deleteCore", "(Ljava/lang/String;)V");
   CALL_OBJ_METHOD(env, obj, android_app->activity->clazz,
         android_app->getIntent);
   GET_METHOD_ID(env, android_app->getVolumeCount, class,
         "getVolumeCount", "()I");
   GET_METHOD_ID(env, android_app->getVolumePath, class,
         "getVolumePath", "(Ljava/lang/String;)Ljava/lang/String;");

   GET_OBJECT_CLASS(env, class, obj);
   GET_METHOD_ID(env, android_app->getStringExtra, class,
         "getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;");
#endif

}

static int frontend_unix_parse_drive_list(void *data, bool load_content)
{
#ifdef HAVE_MENU
   file_list_t *list = (file_list_t*)data;
   enum msg_hash_enums enum_idx = load_content ?
      MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR :
      MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;

#ifdef ANDROID

   JNIEnv *env = jni_thread_getenv();
   jint output           = 0;
   jobject obj           = NULL;
   jstring jstr          = NULL;

   int volume_count = 0;

   if (!env || !g_android)
      return 0;

   CALL_OBJ_METHOD(env, obj, g_android->activity->clazz,
         g_android->getIntent);

   if (g_android->getVolumeCount)
   {
      CALL_INT_METHOD(env, output,
         g_android->activity->clazz, g_android->getVolumeCount);
      volume_count = output;
   }

   if (!string_is_empty(internal_storage_path))
   {
      if (storage_permissions == INTERNAL_STORAGE_WRITABLE)
      {
         char user_data_path[PATH_MAX_LENGTH];
         fill_pathname_join_special(user_data_path,
               internal_storage_path, "RetroArch",
               sizeof(user_data_path));

         menu_entries_append(list,
               user_data_path,
               msg_hash_to_str(MSG_INTERNAL_STORAGE),
               enum_idx,
               FILE_TYPE_DIRECTORY, 0, 0, NULL);
      }

      menu_entries_append(list,
            internal_storage_path,
            msg_hash_to_str(MSG_INTERNAL_STORAGE),
            enum_idx,
            FILE_TYPE_DIRECTORY, 0, 0, NULL);
   }
   else
      menu_entries_append(list,
            "/storage/emulated/0",
            msg_hash_to_str(MSG_REMOVABLE_STORAGE),
            enum_idx,
            FILE_TYPE_DIRECTORY, 0, 0, NULL);

   menu_entries_append(list,
         "/storage",
         msg_hash_to_str(MSG_REMOVABLE_STORAGE),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
   if (!string_is_empty(internal_storage_app_path))
      menu_entries_append(list,
            internal_storage_app_path,
            msg_hash_to_str(MSG_EXTERNAL_APPLICATION_DIR),
            enum_idx,
            FILE_TYPE_DIRECTORY, 0, 0, NULL);
   if (!string_is_empty(app_dir))
      menu_entries_append(list,
            app_dir,
            msg_hash_to_str(MSG_APPLICATION_DIR),
            enum_idx,
            FILE_TYPE_DIRECTORY, 0, 0, NULL);
   for (unsigned i=0; i < volume_count; i++)
   {
      static char aux_path[PATH_MAX_LENGTH];
      char index[2];
      index[0] = '\0';

      snprintf(index, sizeof(index), "%d", i);

      CALL_OBJ_METHOD_PARAM(env, jstr, g_android->activity->clazz, g_android->getVolumePath,
         (*env)->NewStringUTF(env, index));

      if (jstr)
      {
         const char *str = (*env)->GetStringUTFChars(env, jstr, 0);

         aux_path[0] = '\0';

         if (str && *str)
            strlcpy(aux_path, str,
                  sizeof(aux_path));

         (*env)->ReleaseStringUTFChars(env, jstr, str);
         if (!string_is_empty(aux_path))
            menu_entries_append(list,
                  aux_path,
                  msg_hash_to_str(MSG_APPLICATION_DIR),
                  enum_idx,
                  FILE_TYPE_DIRECTORY, 0, 0, NULL);
      }

   }
#elif defined(WEBOS)
   if (path_is_directory("/media/internal"))
      menu_entries_append(list, "/media/internal",
            msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
            enum_idx,
            FILE_TYPE_DIRECTORY, 0, 0, NULL);

   if (path_is_directory("/tmp/usb"))
      menu_entries_append(list, "/tmp/usb",
            msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
            enum_idx,
            FILE_TYPE_DIRECTORY, 0, 0, NULL);
#else
   char base_path[PATH_MAX] = {0};
   char udisks_media_path[PATH_MAX] = {0};
   const char *home         = getenv("HOME");
   const char *user         = getenv("USER");

#if defined(DINGUX)
   dingux_get_base_path(base_path, sizeof(base_path));
#else
   const char *xdg          = getenv("XDG_CONFIG_HOME");

   if (xdg)
   {
      strlcpy(base_path, xdg, sizeof(base_path));
      strlcat(base_path, "/retroarch", sizeof(base_path));
   }
   else if (home)
   {
      strlcpy(base_path, home, sizeof(base_path));
      strlcat(base_path, "/.config/retroarch", sizeof(base_path));
   }
#endif

   strlcpy(udisks_media_path, "/run/media", sizeof(udisks_media_path));
   if (user)
   {
      strlcat(udisks_media_path, "/", sizeof(udisks_media_path));
      strlcat(udisks_media_path, user, sizeof(udisks_media_path));
   }

   if (!string_is_empty(base_path))
   {
      menu_entries_append(list, base_path,
            msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
            enum_idx,
            FILE_TYPE_DIRECTORY, 0, 0, NULL);
   }
   if (!string_is_empty(home))
   {
      menu_entries_append(list, home,
            msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
            enum_idx,
            FILE_TYPE_DIRECTORY, 0, 0, NULL);
   }
   if (path_is_directory(udisks_media_path))
   {
      menu_entries_append(list, udisks_media_path,
            msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
            enum_idx,
            FILE_TYPE_DIRECTORY, 0, 0, NULL);
   }
   if (path_is_directory("/media"))
   {
      menu_entries_append(list, "/media",
            msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
            enum_idx,
            FILE_TYPE_DIRECTORY, 0, 0, NULL);
   }
   if (path_is_directory("/mnt"))
   {
      menu_entries_append(list, "/mnt",
            msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
            enum_idx,
            FILE_TYPE_DIRECTORY, 0, 0, NULL);
   }
#endif

   menu_entries_append(list, "/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
#endif

   return 0;
}

#ifndef ANDROID

static bool frontend_unix_set_fork(enum frontend_fork fork_mode)
{
   switch (fork_mode)
   {
      case FRONTEND_FORK_CORE:
         unix_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_CORE_WITH_ARGS:
         unix_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_RESTART:
         unix_fork_mode  = FRONTEND_FORK_CORE;

         {
            char executable_path[PATH_MAX_LENGTH] = {0};
            fill_pathname_application_path(executable_path,
                  sizeof(executable_path));
            path_set(RARCH_PATH_CORE, executable_path);
         }
         command_event(CMD_EVENT_QUIT, NULL);
         break;
      case FRONTEND_FORK_NONE:
      default:
         return false;
   }

   return true;
}

static void frontend_unix_exec(const char *path, bool should_load_content)
{
   char *newargv[]    = { NULL, NULL };
   size_t len         = strlen(path);

   newargv[0] = (char*)malloc(len);

   strlcpy(newargv[0], path, len);

   execv(path, newargv);
}

static void frontend_unix_exitspawn(char *s, size_t len, char *args)
{
   bool should_load_content = false;

   if (unix_fork_mode == FRONTEND_FORK_NONE)
      return;

   switch (unix_fork_mode)
   {
      case FRONTEND_FORK_CORE_WITH_ARGS:
         should_load_content = true;
         break;
      case FRONTEND_FORK_NONE:
      default:
         break;
   }

   frontend_unix_exec(s, should_load_content);
}
#endif

static uint64_t frontend_unix_get_total_mem(void)
{
#if defined(DINGUX)
   char line[256];
   unsigned long mem_total = 0;
   FILE* meminfo_file      = NULL;

   line[0] = '\0';

   /* Open /proc/meminfo */
   if (!(meminfo_file = fopen(PROC_MEMINFO_PATH, "r")))
      return 0;

   /* Parse lines
    * (Note: virtual filesystem, so don't have to
    *  worry about buffering file reads) */
   while (fgets(line, sizeof(line), meminfo_file))
   {
      if (string_starts_with_size(line, PROC_MEMINFO_MEM_TOTAL_TAG,
            STRLEN_CONST(PROC_MEMINFO_MEM_TOTAL_TAG)))
      {
         sscanf(line, PROC_MEMINFO_MEM_TOTAL_TAG " %lu kB", &mem_total);
         break;
      }
   }

   /* Close /proc/meminfo */
   fclose(meminfo_file);
   meminfo_file = NULL;

   return (uint64_t)mem_total * 1024;
#else
   uint64_t pages            = sysconf(_SC_PHYS_PAGES);
   uint64_t page_size        = sysconf(_SC_PAGE_SIZE);
   return pages * page_size;
#endif
}

static uint64_t frontend_unix_get_free_mem(void)
{
   char line[256];
   unsigned long mem_available = 0;
   unsigned long mem_free      = 0;
   unsigned long buffers       = 0;
   unsigned long cached        = 0;
   unsigned long shmem         = 0;
   bool mem_available_found    = false;
   bool mem_free_found         = false;
   bool buffers_found          = false;
   bool cached_found           = false;
   bool shmem_found            = false;
   FILE* meminfo_file          = NULL;

   line[0] = '\0';

   /* Open /proc/meminfo */
   if (!(meminfo_file = fopen(PROC_MEMINFO_PATH, "r")))
      return 0;

   /* Parse lines
    * (Note: virtual filesystem, so don't have to
    *  worry about buffering file reads) */
   while (fgets(line, sizeof(line), meminfo_file))
   {
      /* If 'MemAvailable' is found, we can return immediately */
      if (!mem_available_found)
         if (string_starts_with_size(line, PROC_MEMINFO_MEM_AVAILABLE_TAG,
               STRLEN_CONST(PROC_MEMINFO_MEM_AVAILABLE_TAG)))
         {
            mem_available_found = true;
            sscanf(line, PROC_MEMINFO_MEM_AVAILABLE_TAG " %lu kB", &mem_available);
            break;
         }

      if (!mem_free_found)
         if (string_starts_with_size(line, PROC_MEMINFO_MEM_FREE_TAG,
               STRLEN_CONST(PROC_MEMINFO_MEM_FREE_TAG)))
         {
            mem_free_found = true;
            sscanf(line, PROC_MEMINFO_MEM_FREE_TAG " %lu kB", &mem_free);
         }

      if (!buffers_found)
         if (string_starts_with_size(line, PROC_MEMINFO_BUFFERS_TAG,
               STRLEN_CONST(PROC_MEMINFO_BUFFERS_TAG)))
         {
            buffers_found = true;
            sscanf(line, PROC_MEMINFO_BUFFERS_TAG " %lu kB", &buffers);
         }

      if (!cached_found)
         if (string_starts_with_size(line, PROC_MEMINFO_CACHED_TAG,
               STRLEN_CONST(PROC_MEMINFO_CACHED_TAG)))
         {
            cached_found = true;
            sscanf(line, PROC_MEMINFO_CACHED_TAG " %lu kB", &cached);
         }

      if (!shmem_found)
         if (string_starts_with_size(line, PROC_MEMINFO_SHMEM_TAG,
               STRLEN_CONST(PROC_MEMINFO_SHMEM_TAG)))
         {
            shmem_found = true;
            sscanf(line, PROC_MEMINFO_SHMEM_TAG " %lu kB", &shmem);
         }
   }

   /* Close /proc/meminfo */
   fclose(meminfo_file);
   meminfo_file = NULL;

   /* Use 'accurate' free memory value, if available */
   if (mem_available_found)
      return (uint64_t)mem_available * 1024;

   /* ...Otherwise, use estimate */
   return (uint64_t)((mem_free + buffers + cached) - shmem) * 1024;
}

/*#include <valgrind/valgrind.h>*/
static void frontend_unix_sighandler(int sig)
{
#ifdef VALGRIND_PRINTF_BACKTRACE
VALGRIND_PRINTF_BACKTRACE("SIGINT");
#endif
   (void)sig;
   unix_sighandler_quit++;
   if (unix_sighandler_quit == 1) {}
   if (unix_sighandler_quit == 2) exit(1);
   /* in case there's a second deadlock in a C++ destructor or something */
   if (unix_sighandler_quit >= 3) abort();
}

static void frontend_unix_install_signal_handlers(void)
{
   struct sigaction sa;

   sa.sa_sigaction = NULL;
   sa.sa_handler   = frontend_unix_sighandler;
   sa.sa_flags     = SA_RESTART;
   sigemptyset(&sa.sa_mask);
   sigaction(SIGINT, &sa, NULL);
   sigaction(SIGTERM, &sa, NULL);
}

static int frontend_unix_get_signal_handler_state(void)
{
   return (int)unix_sighandler_quit;
}

static void frontend_unix_set_signal_handler_state(int value)
{
   unix_sighandler_quit = value;
}

static void frontend_unix_destroy_signal_handler_state(void)
{
   unix_sighandler_quit = 0;
}

/* To free change_data, call the function again with a NULL string_list while providing change_data again */
static void frontend_unix_watch_path_for_changes(struct string_list *list, int flags, path_change_data_t **change_data)
{
#ifdef HAS_INOTIFY
   int major = 0;
   int minor = 0;
   int inotify_mask = 0, fd = 0;
   unsigned i, krel = 0;
   struct utsname buffer;
   inotify_data_t *inotify_data;

   if (!list)
   {
      if (change_data && *change_data)
      {
         /* free the original data */
         inotify_data = (inotify_data_t*)((*change_data)->data);

         if (inotify_data->wd_list->count > 0)
         {
            for (i = 0; i < inotify_data->wd_list->count; i++)
            {
               inotify_rm_watch(inotify_data->fd, inotify_data->wd_list->data[i]);
            }
         }

         int_vector_list_free(inotify_data->wd_list);
         string_list_free(inotify_data->path_list);
         close(inotify_data->fd);
         free(inotify_data);
         free(*change_data);
         return;
      }
      else
         return;
   }
   else if (list->size == 0)
      return;
   else
      if (!change_data)
         return;

   if (uname(&buffer) != 0)
   {
      RARCH_WARN("watch_path_for_changes: Failed to get current kernel version.\n");
      return;
   }

   /* get_os doesn't provide all three */
   sscanf(buffer.release, "%d.%d.%u", &major, &minor, &krel);

   /* check if we are actually running on a high enough kernel version as well */
   if (major < 2)
   {
      RARCH_WARN("watch_path_for_changes: inotify unsupported on this kernel version (%d.%d.%u).\n", major, minor, krel);
      return;
   }
   else if (major == 2)
   {
      if (minor < 6)
      {
         RARCH_WARN("watch_path_for_changes: inotify unsupported on this kernel version (%d.%d.%u).\n", major, minor, krel);
         return;
      }
      else if (minor == 6)
      {
         if (krel < 13)
         {
            RARCH_WARN("watch_path_for_changes: inotify unsupported on this kernel version (%d.%d.%u).\n", major, minor, krel);
            return;
         }
         else
         {
            /* anything >= 2.6.13 is supported */
         }
      }
      else
      {
         /* anything >= 2.7 is supported */
      }
   }
   else
   {
      /* anything >= 3 is supported */
   }

   fd = inotify_init();

   if (fd < 0)
   {
      RARCH_WARN("watch_path_for_changes: Could not initialize inotify.\n");
      return;
   }

   if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK))
   {
      RARCH_WARN("watch_path_for_changes: Could not set socket to non-blocking.\n");
      return;
   }

   inotify_data = (inotify_data_t*)calloc(1, sizeof(*inotify_data));
   inotify_data->fd = fd;

   inotify_data->wd_list = int_vector_list_new();
   inotify_data->path_list = string_list_new();

   /* handle other flags here as new ones are added */
   if (flags & PATH_CHANGE_TYPE_MODIFIED)
      inotify_mask |= IN_MODIFY;
   if (flags & PATH_CHANGE_TYPE_WRITE_FILE_CLOSED)
      inotify_mask |= IN_CLOSE_WRITE;
   if (flags & PATH_CHANGE_TYPE_FILE_MOVED)
      inotify_mask |= IN_MOVE_SELF;
   if (flags & PATH_CHANGE_TYPE_FILE_DELETED)
      inotify_mask |= IN_DELETE_SELF;

   inotify_data->flags = inotify_mask;

   for (i = 0; i < list->size; i++)
   {
      int wd = inotify_add_watch(fd, list->elems[i].data, inotify_mask);
      union string_list_elem_attr attr = {0};

      int_vector_list_append(inotify_data->wd_list, wd);
      string_list_append(inotify_data->path_list, list->elems[i].data, attr);
   }

   *change_data = (path_change_data_t*)calloc(1, sizeof(path_change_data_t));
   (*change_data)->data = inotify_data;
#endif
}

static bool frontend_unix_check_for_path_changes(path_change_data_t *change_data)
{
#ifdef HAS_INOTIFY
   inotify_data_t *inotify_data = (inotify_data_t*)(change_data->data);
   char buffer[INOTIFY_BUF_LEN] = {0};
   int length, i = 0;

   while ((length = read(inotify_data->fd, buffer, INOTIFY_BUF_LEN)) > 0)
   {
      i = 0;

      while (i < length && i < sizeof(buffer))
      {
         struct inotify_event *event = (struct inotify_event *)&buffer[i];

         if (event->mask & inotify_data->flags)
         {
            unsigned j;

            /* A successful close does not guarantee that the
             * data has been successfully saved to disk,
             * as the kernel defers writes. It is
             * not common for a file system to flush
             * the buffers when the stream is closed.
             *
             * So we manually fsync() here to flush the data
             * to disk, to make sure that the new data is
             * immediately available when the file is re-read.
             */
            for (j = 0; j < inotify_data->wd_list->count; j++)
            {
               if (inotify_data->wd_list->data[j] == event->wd)
               {
                  /* found the right file, now sync it */
                  const char *path = inotify_data->path_list->elems[j].data;
                  FILE         *fp = (FILE*)fopen_utf8(path, "rb");

                  if (fp)
                  {
                     fsync(fileno(fp));
                     fclose(fp);
                  }
               }
            }

            return true;
         }

         i += sizeof(struct inotify_event) + event->len;
      }
   }
#endif
   return false;
}

static void frontend_unix_set_sustained_performance_mode(bool on)
{
#ifdef ANDROID
   JNIEnv *env = jni_thread_getenv();

   if (!env || !g_android)
      return;

   if (g_android->setSustainedPerformanceMode)
      CALL_VOID_METHOD_PARAM(env, g_android->activity->clazz,
            g_android->setSustainedPerformanceMode, on);
#endif
}

static const char* frontend_unix_get_cpu_model_name(void)
{
#ifdef ANDROID
   return NULL;
#else
   cpu_features_get_model_name(unix_cpu_model_name,
         sizeof(unix_cpu_model_name));
   return unix_cpu_model_name;
#endif
}

enum retro_language frontend_unix_get_user_language(void)
{
   enum retro_language lang = RETRO_LANGUAGE_ENGLISH;
#ifdef HAVE_LANGEXTRA
#ifdef ANDROID
   jstring jstr = NULL;
   JNIEnv *env = jni_thread_getenv();

   if (!env || !g_android)
      return lang;

   if (g_android->getUserLanguageString)
   {
      CALL_OBJ_METHOD(env, jstr,
            g_android->activity->clazz, g_android->getUserLanguageString);

      if (jstr)
      {
         const char *langStr = (*env)->GetStringUTFChars(env, jstr, 0);

         lang = rarch_get_language_from_iso(langStr);

         (*env)->ReleaseStringUTFChars(env, jstr, langStr);
      }
   }
#else
   char *envvar = getenv("LANG");

   if (envvar)
      lang = rarch_get_language_from_iso(envvar);
#endif
#endif
   return lang;
}

#if (defined(__linux__) || defined(__unix__)) && !defined(ANDROID)
static bool is_narrator_running_unix(void)
{
   return (kill(speak_pid, 0) == 0);
}

static bool accessibility_speak_unix(int speed,
      const char* speak_text, int priority)
{
   int pid;
   const char *language   = get_user_language_iso639_1(true);
   char* voice_out        = (char*)malloc(3+strlen(language));
   char* speed_out        = (char*)malloc(3+3);
   const char* speeds[10] = {"80", "100", "125", "150", "170", "210", "260", "310", "380", "450"};

   if (speed < 1)
      speed = 1;
   else if (speed > 10)
      speed = 10;

   voice_out[0] = '-';
   voice_out[1] = 'v';
   voice_out[2] = '\0';
   strlcat(voice_out, language, 5);

   speed_out[0] = '-';
   speed_out[1] = 's';
   speed_out[2] = '\0';
   strlcat(speed_out, speeds[speed-1], 6);

   if (priority < 10 && speak_pid > 0)
   {
      /* check if old pid is running */
      if (is_narrator_running_unix())
         goto end;
   }

   if (speak_pid > 0)
   {
      /* Kill the running espeak */
      kill(speak_pid, SIGTERM);
      speak_pid = 0;
   }

   pid = fork();
   if (pid < 0)
   {
      /* error */
      RARCH_LOG("ERROR: could not fork for espeak.\n");
   }
   else if (pid > 0)
   {
      /* parent process */
      speak_pid = pid;

      /* Tell the system that we'll ignore the exit status of the child
       * process.  This prevents zombie processes. */
      signal(SIGCHLD,SIG_IGN);
   }
   else
   {
      /* child process: replace process with the espeak command */
      char* cmd[] = { (char*) "espeak", NULL, NULL, NULL, NULL};
      cmd[1] = voice_out;
      cmd[2] = speed_out;
      cmd[3] = (char*)speak_text;
      execvp("espeak", cmd);
   }

end:
   if (voice_out)
      free(voice_out);
   if (speed_out)
      free(speed_out);
   return true;
}
#endif

frontend_ctx_driver_t frontend_ctx_unix = {
   frontend_unix_get_env,       /* get_env */
   frontend_unix_init,          /* init */
   frontend_unix_deinit,        /* deinit */
#ifdef ANDROID
   NULL,                         /* exitspawn */
#else
   frontend_unix_exitspawn,     /* exitspawn */
#endif
   NULL,                         /* process_args */
#ifdef ANDROID
   NULL,                         /* exec */
   NULL,                         /* set_fork */
#else
   frontend_unix_exec,          /* exec */
   frontend_unix_set_fork,      /* set_fork */
#endif
#ifdef ANDROID
   frontend_android_shutdown,    /* shutdown */
   frontend_android_get_name,    /* get_name */
#else
   NULL,                         /* shutdown */
   NULL,                         /* get_name */
#endif
   frontend_unix_get_os,
   frontend_unix_get_rating,           /* get_rating */
   NULL,                               /* content_loaded */
   frontend_unix_get_arch,             /* get_architecture */
   frontend_unix_get_powerstate,
   frontend_unix_parse_drive_list,
   frontend_unix_get_total_mem,
   frontend_unix_get_free_mem,
   frontend_unix_install_signal_handlers,
   frontend_unix_get_signal_handler_state,
   frontend_unix_set_signal_handler_state,
   frontend_unix_destroy_signal_handler_state,
   NULL,                               /* attach_console */
   NULL,                               /* detach_console */
#ifdef HAVE_LAKKA
   frontend_unix_get_lakka_version,    /* get_lakka_version */
#else
   NULL,                               /* get_lakka_version */
#endif
#if defined(HAVE_LAKKA_SWITCH) || (defined(HAVE_LAKKA) && defined(HAVE_ODROIDGO2))
   frontend_unix_set_screen_brightness,/* set_screen_brightness */
#else
   NULL,                         /* set_screen_brightness */
#endif
   frontend_unix_watch_path_for_changes,
   frontend_unix_check_for_path_changes,
   frontend_unix_set_sustained_performance_mode,
   frontend_unix_get_cpu_model_name,
   frontend_unix_get_user_language,
#if (defined(__linux__) || defined(__unix__)) && !defined(ANDROID)
   is_narrator_running_unix,     /* is_narrator_running */
   accessibility_speak_unix,     /* accessibility_speak */
#else
   NULL,                         /* is_narrator_running */
   NULL,                         /* accessibility_speak */
#endif
#ifdef FERAL_GAMEMODE
   frontend_unix_set_gamemode,
#else
   NULL,
#endif
#ifdef ANDROID
   "android",                    /* ident               */
#else
   "unix",                       /* ident               */
#endif
   NULL                          /* get_video_driver    */
};
