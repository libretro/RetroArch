#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <boolean.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>

#include <file/nbio.h>
#include <formats/image.h>

#ifdef HAVE_LIBNX
#include <switch.h>
#include "../../switch_performance_profiles.h"
#include "../../configuration.h"
#include <unistd.h>
#include <malloc.h>
#else
#include <libtransistor/nx.h>
#include <libtransistor/ipc_helpers.h>
#endif

#include <file/file_path.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifndef IS_SALAMANDER
#include <lists/file_list.h>
#endif

#include <string/stdstring.h>

#include "../frontend_driver.h"
#include "../../verbosity.h"
#include "../../defaults.h"
#include "../../paths.h"
#include "../../retroarch.h"
#include "../../file_path_special.h"

#ifndef IS_SALAMANDER
#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#if defined(HAVE_LIBNX) && defined(HAVE_NETWORKING)
#include "../../network/netplay/netplay.h"
#endif
#endif

#ifdef HAVE_LIBNX
#define SD_PREFIX
#include "../../gfx/common/switch_common.h"
#else
#define SD_PREFIX "/sd"
#endif

static enum frontend_fork switch_fork_mode = FRONTEND_FORK_NONE;
bool platform_switch_has_focus = true;

#ifdef HAVE_LIBNX
static bool psmInitialized  = false;

static AppletHookCookie applet_hook_cookie;

#ifdef NXLINK
extern bool nxlink_connected;
#endif

void libnx_apply_overclock(void)
{
   const size_t profiles_count = sizeof(SWITCH_CPU_PROFILES) 
      / sizeof(SWITCH_CPU_PROFILES[1]);
   settings_t *settings        = config_get_ptr();
   unsigned libnx_overclock    = settings->uints.libnx_overclock;

   if (libnx_overclock >= 0 && libnx_overclock <= profiles_count)
   {
      if (hosversionBefore(8, 0, 0))
      {
         pcvSetClockRate(PcvModule_CpuBus, SWITCH_CPU_SPEEDS_VALUES[
               libnx_overclock]);
      }
      else
      {
         ClkrstSession session = {0};
         clkrstOpenSession(&session, PcvModuleId_CpuBus, 3);
         clkrstSetClockRate(&session, SWITCH_CPU_SPEEDS_VALUES[libnx_overclock]);
         clkrstCloseSession(&session);
      }
   }
}

static void on_applet_hook(AppletHookType hook, void *param)
{
   AppletFocusState focus_state;

   /* Exit request */
   switch (hook)
   {
      case AppletHookType_OnExitRequest:
         retroarch_main_quit();
         break;

         /* Focus state*/
      case AppletHookType_OnFocusState:
         focus_state = appletGetFocusState();
         platform_switch_has_focus = focus_state == AppletFocusState_InFocus;

         if (!platform_switch_has_focus)
         {
            if (hosversionBefore(8, 0, 0))
            {
               pcvSetClockRate(PcvModule_CpuBus, 1020000000);
            }
            else
            {
               ClkrstSession session = {0};
               clkrstOpenSession(&session, PcvModuleId_CpuBus, 3);
               clkrstSetClockRate(&session, 1020000000);
               clkrstCloseSession(&session);
            }
         }
         else
            libnx_apply_overclock();
         break;

         /* Performance mode */
      case AppletHookType_OnPerformanceMode:
         libnx_apply_overclock();
         break;

      default:
         break;
   }
}

#endif /* HAVE_LIBNX */

#ifdef IS_SALAMANDER
static void get_first_valid_core(char *path_return, size_t len)
{
   DIR *dir;
   struct dirent *ent;
   const char *extension = ".nro";

   path_return[0] = '\0';

   dir = opendir(SD_PREFIX "/retroarch/cores");
   if (dir)
   {
      while ((ent = readdir(dir)))
      {
         if (!ent)
            break;
         if (strlen(ent->d_name) > strlen(extension) && !strcmp(ent->d_name + strlen(ent->d_name) - strlen(extension), extension))
         {
            strcpy_literal(path_return, SD_PREFIX "/retroarch/cores");
            strlcat(path_return, "/", len);
            strlcat(path_return, ent->d_name, len);
            break;
         }
      }
      closedir(dir);
   }
}
#endif

static void frontend_switch_get_env(
      int *argc, char *argv[], void *args, void *params_data)
{
   unsigned i;
#ifndef IS_SALAMANDER
#if defined(HAVE_LOGGER)
   logger_init();
#elif defined(HAVE_FILE_LOGGER)
   retro_main_log_file_init(SD_PREFIX "/retroarch-log.txt");
#endif
#endif

   fill_pathname_basedir(g_defaults.dirs[DEFAULT_DIR_PORT], SD_PREFIX "/retroarch/retroarch_switch.nro", sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "cores", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "info", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "autoconfig", sizeof(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "assets", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SAVESTATE], g_defaults.dirs[DEFAULT_DIR_CORE],
                      "savestates", sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SRAM], g_defaults.dirs[DEFAULT_DIR_CORE],
                      "savefiles", sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SYSTEM], g_defaults.dirs[DEFAULT_DIR_CORE],
                      "system", sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "config", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_REMAP], g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG],
                      "remaps", sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PLAYLIST], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "playlists", sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_RECORD_CONFIG], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "records_config", sizeof(g_defaults.dirs[DEFAULT_DIR_RECORD_CONFIG]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_RECORD_OUTPUT], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "records", sizeof(g_defaults.dirs[DEFAULT_DIR_RECORD_OUTPUT]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CURSOR], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "database/cursors", sizeof(g_defaults.dirs[DEFAULT_DIR_CURSOR]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_DATABASE], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "database/rdb", sizeof(g_defaults.dirs[DEFAULT_DIR_DATABASE]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "filters", sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SHADER], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "shaders", sizeof(g_defaults.dirs[DEFAULT_DIR_SHADER]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CHEATS], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "cheats", sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OVERLAY], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "overlay", sizeof(g_defaults.dirs[DEFAULT_DIR_OVERLAY]));

#ifdef HAVE_VIDEO_LAYOUT
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "layouts", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT]));
#endif

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "downloads", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "screenshots", sizeof(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "thumbnails", sizeof(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_LOGS], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "logs", sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));

   for (i = 0; i < DEFAULT_DIR_LAST; i++)
   {
      const char *dir_path = g_defaults.dirs[i];
      if (!string_is_empty(dir_path))
         path_mkdir(dir_path);
   }

   fill_pathname_join(g_defaults.path_config,
         g_defaults.dirs[DEFAULT_DIR_PORT],
         FILE_PATH_MAIN_CONFIG,
         sizeof(g_defaults.path_config));

#ifndef IS_SALAMANDER
   dir_check_defaults("custom.ini");
#endif
}

static void frontend_switch_deinit(void *data)
{
   (void)data;

#ifdef HAVE_LIBNX
   nifmExit();

   if (hosversionBefore(8, 0, 0))
   {
      pcvSetClockRate(PcvModule_CpuBus, 1020000000);
      pcvExit();
   }
   else
   {
      ClkrstSession session = {0};
      clkrstOpenSession(&session, PcvModuleId_CpuBus, 3);
      clkrstSetClockRate(&session, 1020000000);
      clkrstCloseSession(&session);
      clkrstExit();
   }

#if defined(SWITCH) && defined(NXLINK)
   socketExit();
#endif

   if (psmInitialized)
       psmExit();

   appletUnlockExit();
#endif
}

#ifdef HAVE_LIBNX
static void frontend_switch_exec(const char *path, bool should_load_game)
{
   if (!string_is_empty(path))
   {
      char args[PATH_MAX];

      strlcpy(args, path, sizeof(args));

#ifndef IS_SALAMANDER
      if (should_load_game)
      {
         const char *content = path_get(RARCH_PATH_CONTENT);
#ifdef HAVE_NETWORKING
         char *arg_data[NETPLAY_FORK_MAX_ARGS];

         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_GET_FORK_ARGS,
               (void*)arg_data))
         {
            char buf[PATH_MAX];
            char **arg = arg_data;

            do
            {
               snprintf(buf, sizeof(buf), " \"%s\"", *arg);
               strlcat(args, buf, sizeof(args));
            }
            while (*(++arg));
         }
         else
#endif
         if (!string_is_empty(content))
            snprintf(args, sizeof(args), "%s \"%s\"", path, content);
      }
#else
      {
         struct stat sbuff;

         if (stat(path, &sbuff))
         {
            char core_path[PATH_MAX];

            get_first_valid_core(core_path, sizeof(core_path));

            if (string_is_empty(core_path))
               svcExitProcess();
         }
      }
#endif

      envSetNextLoad(path, args);
   }
}

#ifndef IS_SALAMANDER
static bool frontend_switch_set_fork(enum frontend_fork fork_mode)
{
   switch (fork_mode)
   {
   case FRONTEND_FORK_CORE:
      switch_fork_mode = fork_mode;
      break;
   case FRONTEND_FORK_CORE_WITH_ARGS:
      switch_fork_mode = fork_mode;
      break;
   case FRONTEND_FORK_RESTART:
      /*  NOTE: We don't implement Salamander, so just turn
             this into FRONTEND_FORK_CORE. */
      switch_fork_mode = FRONTEND_FORK_CORE;
      break;
   case FRONTEND_FORK_NONE:
   default:
      return false;
   }

   return true;
}
#endif

static void frontend_switch_exitspawn(char *s, size_t len, char *args)
{
   bool should_load_content = false;
#ifndef IS_SALAMANDER
   if (switch_fork_mode == FRONTEND_FORK_NONE)
      return;

   switch (switch_fork_mode)
   {
   case FRONTEND_FORK_CORE_WITH_ARGS:
      should_load_content = true;
      break;
   default:
      break;
   }
#endif
   frontend_switch_exec(s, should_load_content);
}

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
   svcSleepThread(rqtp->tv_nsec + (rqtp->tv_sec * 1000000000));
   return 0;
}

long sysconf(int name)
{
   if (name == 8)
      return 0x1000;
   return -1;
}

ssize_t readlink(const char *restrict path, char *restrict buf, size_t bufsize)
{
   return -1;
}

/* Taken from glibc */
char *realpath(const char *name, char *resolved)
{
   char *rpath, *dest = NULL;
   const char *start, *end, *rpath_limit;
   long int path_max;

   /* As per Single Unix Specification V2 we must return an error if
      either parameter is a null pointer.  We extend this to allow
      the RESOLVED parameter to be NULL in case the we are expected to
      allocate the room for the return value.  */
   if (!name)
      return NULL;

   /* As per Single Unix Specification V2 we must return an error if
      the name argument points to an empty string.  */
   if (name[0] == '\0')
      return NULL;

#ifdef PATH_MAX
   path_max = PATH_MAX;
#else
   path_max = pathconf(name, _PC_PATH_MAX);
   if (path_max <= 0)
      path_max = 1024;
#endif

   if (!resolved)
   {
      rpath = malloc(path_max);
      if (!rpath)
         return NULL;
   }
   else
      rpath = resolved;
   rpath_limit = rpath + path_max;

   if (name[0] != '/')
   {
      if (!getcwd(rpath, path_max))
      {
         rpath[0] = '\0';
         goto error;
      }
      dest = memchr(rpath, '\0', path_max);
   }
   else
   {
      rpath[0] = '/';
      dest = rpath + 1;
   }

   for (start = end = name; *start; start = end)
   {
      /* Skip sequence of multiple path-separators.  */
      while (*start == '/')
         ++start;

      /* Find end of path component.  */
      for (end = start; *end && *end != '/'; ++end)
         /* Nothing.  */;

      if (end - start == 0)
         break;
      else if (end - start == 1 && start[0] == '.')
         /* nothing */;
      else if (end - start == 2 && start[0] == '.' && start[1] == '.')
      {
         /* Back up to previous component, ignore if at root already.  */
         if (dest > rpath + 1)
            while ((--dest)[-1] != '/')
               ;
      }
      else
      {
         size_t new_size;

         if (dest[-1] != '/')
            *dest++ = '/';

         if (dest + (end - start) >= rpath_limit)
         {
            ptrdiff_t dest_offset = dest - rpath;
            char *new_rpath;

            if (resolved)
            {
               if (dest > rpath + 1)
                  dest--;
               *dest = '\0';
               goto error;
            }
            new_size = rpath_limit - rpath;
            if (end - start + 1 > path_max)
               new_size += end - start + 1;
            else
               new_size += path_max;
            new_rpath = (char *)realloc(rpath, new_size);
            if (!new_rpath)
               goto error;
            rpath = new_rpath;
            rpath_limit = rpath + new_size;

            dest = rpath + dest_offset;
         }

         dest = memcpy(dest, start, end - start);
         *dest = '\0';
      }
   }
   if (dest > rpath + 1 && dest[-1] == '/')
      --dest;
   *dest = '\0';

   return rpath;

error:
   if (!resolved)
      free(rpath);
   return NULL;
}

#endif /* HAVE_LIBNX */

static void frontend_switch_shutdown(bool unused)
{
   (void)unused;
}

/* runloop_get_system_info isnt initialized that early.. */
extern void retro_get_system_info(struct retro_system_info *info);

static void frontend_switch_init(void *data)
{
#ifdef HAVE_LIBNX
   Result rc;
   bool recording_supported      = false;

   nifmInitialize(NifmServiceType_User);
   
   if (hosversionBefore(8, 0, 0))
      pcvInitialize();
   else
      clkrstInitialize();

   appletLockExit();
   appletHook(&applet_hook_cookie, on_applet_hook, NULL);
   appletSetFocusHandlingMode(AppletFocusHandlingMode_NoSuspend);

   appletIsGamePlayRecordingSupported(&recording_supported);
   if (recording_supported)
      appletInitializeGamePlayRecording();

#ifdef NXLINK
   socketInitializeDefault();
   nxlink_connected = nxlinkStdio() != -1;
#ifndef IS_SALAMANDER
   verbosity_enable();
#endif /* IS_SALAMANDER */
#endif /* NXLINK */

   rc = psmInitialize();
   if (R_SUCCEEDED(rc))
       psmInitialized = true;
   else
       RARCH_WARN("Error initializing psm\n");
#endif /* HAVE_LIBNX (splash) */
}

static int frontend_switch_get_rating(void)
{
   return 11;
}

enum frontend_architecture frontend_switch_get_arch(void)
{
   return FRONTEND_ARCH_ARMV8;
}

static int frontend_switch_parse_drive_list(void *data, bool load_content)
{
#ifndef IS_SALAMANDER
   file_list_t *list = (file_list_t *)data;
   enum msg_hash_enums enum_idx = load_content 
      ? MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR 
      : MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;

   if (!list)
      return -1;

   menu_entries_append_enum(list,
         "/", msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
#endif

   return 0;
}

static uint64_t frontend_switch_get_free_mem(void)
{
   struct mallinfo mem_info = mallinfo();
   return mem_info.fordblks;
}

static uint64_t frontend_switch_get_total_mem(void)
{
   struct mallinfo mem_info = mallinfo();
   return mem_info.usmblks;
}

static enum frontend_powerstate 
frontend_switch_get_powerstate(int *seconds, int *percent)
{
   uint32_t pct;
   PsmChargerType ct;
   Result rc;
   if (!psmInitialized)
      return FRONTEND_POWERSTATE_NONE;

   rc = psmGetBatteryChargePercentage(&pct);
   if (!R_SUCCEEDED(rc))
      return FRONTEND_POWERSTATE_NONE;

   rc = psmGetChargerType(&ct);
   if (!R_SUCCEEDED(rc))
      return FRONTEND_POWERSTATE_NONE;

   *percent = (int)pct;

   if (*percent >= 100)
      return FRONTEND_POWERSTATE_CHARGED;

   switch (ct)
   {
      case PsmChargerType_EnoughPower:
      case PsmChargerType_LowPower:
         return FRONTEND_POWERSTATE_CHARGING;
      default:
         break;
   }

   return FRONTEND_POWERSTATE_NO_SOURCE;
}

static void frontend_switch_get_os(
      char *s, size_t len, int *major, int *minor)
{
#ifdef HAVE_LIBNX
   u32 hosVersion;
#else
   int patch;
   char firmware_version[0x100];
   result_t r; /* used by LIB_ASSERT_OK macros */
   ipc_object_t set_sys;
   ipc_request_t rq;
#endif

   strcpy_literal(s, "Horizon OS");

#ifdef HAVE_LIBNX
   *major     = 0;
   *minor     = 0;

   hosVersion = hosversionGet();
   *major     = HOSVER_MAJOR(hosVersion);
   *minor     = HOSVER_MINOR(hosVersion);
#else
   /* defaults in case we error out */
   *major     = 0;
   *minor     = 0;

   LIB_ASSERT_OK(fail, sm_init());
   LIB_ASSERT_OK(fail_sm, sm_get_service(&set_sys, "set:sys"));

   rq                     = ipc_make_request(3);
   ipc_buffer_t buffers[] = {
      ipc_make_buffer(firmware_version, 0x100, 0x1a),
   };
   ipc_msg_set_buffers(rq, buffers, buffer_ptrs);

   LIB_ASSERT_OK(fail_object, ipc_send(set_sys, &rq, &ipc_default_response_fmt));

   sscanf(firmware_version + 0x68, "%d.%d.%d", major, minor, &patch);

fail_object:
   ipc_close(set_sys);
fail_sm:
   sm_finalize();
fail:
   return;
#endif
}

static void frontend_switch_get_name(char *s, size_t len)
{
   /* TODO: Add Mariko at some point */
   strcpy_literal(s, "Nintendo Switch");
}

void frontend_switch_process_args(int *argc, char *argv[])
{
#ifdef HAVE_STATIC_DUMMY
   if (*argc >= 1)
   {
      /* Ensure current Path is set, only works for the static dummy, likely a hbloader args Issue (?) */
      path_set(RARCH_PATH_CORE, argv[0]);
   }
#endif
}

frontend_ctx_driver_t frontend_ctx_switch =
{
   frontend_switch_get_env,
   frontend_switch_init,
   frontend_switch_deinit,
#ifdef HAVE_LIBNX
   frontend_switch_exitspawn,
   frontend_switch_process_args,
   frontend_switch_exec,
#ifdef IS_SALAMANDER
   NULL,
#else
   frontend_switch_set_fork,
#endif
#else /* HAVE_LIBNX */
   NULL,
   NULL,
   NULL,
   NULL,
#endif /* HAVE_LIBNX */
   frontend_switch_shutdown,
   frontend_switch_get_name,
   frontend_switch_get_os,
   frontend_switch_get_rating,
   NULL,                               /* content_loaded */
   frontend_switch_get_arch,           /* get_architecture       */
   frontend_switch_get_powerstate,     /* get_powerstate         */
   frontend_switch_parse_drive_list,   /* parse_drive_list       */
   frontend_switch_get_total_mem,      /* get_total_mem          */
   frontend_switch_get_free_mem,       /* get_free_mem           */
   NULL,                               /* install_signal_handler */
   NULL,                               /* get_signal_handler_state */
   NULL,                               /* set_signal_handler_state */
   NULL,                               /* destroy_signal_handler_state */
   NULL,                               /* attach_console */
   NULL,                               /* detach_console */
   NULL,                               /* get_lakka_version */
   NULL,                               /* set_screen_brightness */
   NULL,                               /* watch_path_for_changes */
   NULL,                               /* check_for_path_changes */
   NULL,                               /* set_sustained_performance_mode */
   NULL,                               /* get_cpu_model_name */
   NULL,                               /* get_user_language */
   NULL,                               /* is_narrator_running */
   NULL,                               /* accessibility_speak */
   NULL,                               /* set_gamemode */
   "switch",                           /* ident */
   NULL                                /* get_video_driver */
};
