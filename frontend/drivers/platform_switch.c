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
#endif

#ifdef HAVE_LIBNX
#define SD_PREFIX
#include "../../gfx/common/switch_common.h"
#else
#define SD_PREFIX "/sd"
#endif

static enum frontend_fork switch_fork_mode = FRONTEND_FORK_NONE;
static const char *elf_path_cst = "/switch/retroarch_switch.nro";

bool platform_switch_has_focus = true;

#ifdef HAVE_LIBNX

/* Splash */
static uint32_t *splashData = NULL;

static bool psmInitialized = false;

static AppletHookCookie applet_hook_cookie;

#ifdef NXLINK
extern bool nxlink_connected;
#endif

void libnx_apply_overclock() {
   const size_t profiles_count = sizeof(SWITCH_CPU_PROFILES) / sizeof(SWITCH_CPU_PROFILES[1]);
   if (config_get_ptr()->uints.libnx_overclock >= 0 && config_get_ptr()->uints.libnx_overclock <= profiles_count){
      if(hosversionBefore(8, 0, 0)) {
         pcvSetClockRate(PcvModule_CpuBus, SWITCH_CPU_SPEEDS_VALUES[config_get_ptr()->uints.libnx_overclock]);
      } else {
         ClkrstSession session = {0};
         clkrstOpenSession(&session, PcvModuleId_CpuBus, 3);
         clkrstSetClockRate(&session, SWITCH_CPU_SPEEDS_VALUES[config_get_ptr()->uints.libnx_overclock]);
         clkrstCloseSession(&session);
      }
   }
}

static void on_applet_hook(AppletHookType hook, void *param) {
   u32 performance_mode;
   AppletFocusState focus_state;

   /* Exit request */
   switch (hook)
   {
   case AppletHookType_OnExitRequest:
      RARCH_LOG("Got AppletHook OnExitRequest, exiting.\n");
      retroarch_main_quit();
      break;

   /* Focus state*/
   case AppletHookType_OnFocusState:
      focus_state = appletGetFocusState();
      RARCH_LOG("Got AppletHook OnFocusState - new focus state is %d\n", focus_state);
      platform_switch_has_focus = focus_state == AppletFocusState_Focused;
      if(!platform_switch_has_focus) {
         if(hosversionBefore(8, 0, 0)) {
            pcvSetClockRate(PcvModule_CpuBus, 1020000000);
         } else {
            ClkrstSession session = {0};
            clkrstOpenSession(&session, PcvModuleId_CpuBus, 3);
            clkrstSetClockRate(&session, 1020000000);
            clkrstCloseSession(&session);
         }
      } else {
         libnx_apply_overclock();
      }
      break;

   /* Performance mode */
   case AppletHookType_OnPerformanceMode:
      // 0 == Handheld, 1 == Docked
      // Since CPU doesn't change we just re-apply
      performance_mode = appletGetPerformanceMode();
      libnx_apply_overclock();
      break;

   default:
      break;
   }
}

#endif /* HAVE_LIBNX */

static void get_first_valid_core(char *path_return)
{
   DIR *dir;
   struct dirent *ent;
   const char *extension = ".nro";

   path_return[0] = '\0';

   dir = opendir(SD_PREFIX "/retroarch/cores");
   if (dir != NULL)
   {
      while ((ent = readdir(dir)) != NULL)
      {
         if (ent == NULL)
            break;
         if (strlen(ent->d_name) > strlen(extension) && !strcmp(ent->d_name + strlen(ent->d_name) - strlen(extension), extension))
         {
            strcpy(path_return, SD_PREFIX "/retroarch/cores");
            strcat(path_return, "/");
            strcat(path_return, ent->d_name);
            break;
         }
      }
      closedir(dir);
   }
}

static void frontend_switch_get_environment_settings(int *argc, char *argv[], void *args, void *params_data)
{
   unsigned i;
   (void)args;

#ifndef IS_SALAMANDER
#if defined(HAVE_LOGGER)
   logger_init();
#elif defined(HAVE_FILE_LOGGER)
   retro_main_log_file_init(SD_PREFIX "/retroarch-log.txt");
#endif
#endif

   fill_pathname_basedir(g_defaults.dirs[DEFAULT_DIR_PORT], SD_PREFIX "/retroarch/retroarch_switch.nro", sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));
   RARCH_LOG("port dir: [%s]\n", g_defaults.dirs[DEFAULT_DIR_PORT]);

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

   fill_pathname_join(g_defaults.path.config,
         g_defaults.dirs[DEFAULT_DIR_PORT],
         file_path_str(FILE_PATH_MAIN_CONFIG),
         sizeof(g_defaults.path.config));
}

extern switch_ctx_data_t *nx_ctx_ptr;
static void frontend_switch_deinit(void *data)
{
   (void)data;

#ifdef HAVE_LIBNX
   nifmExit();

   if(hosversionBefore(8, 0, 0)) {
      pcvSetClockRate(PcvModule_CpuBus, 1020000000);
      pcvExit();
   } else {
      ClkrstSession session = {0};
      clkrstOpenSession(&session, PcvModuleId_CpuBus, 3);
      clkrstSetClockRate(&session, 1020000000);
      clkrstCloseSession(&session);
      clkrstExit();
   }

#if defined(SWITCH) && defined(NXLINK)
   socketExit();
#endif

   /* Splash */
   if (splashData)
   {
      free(splashData);
      splashData = NULL;
   }

   if (psmInitialized)
       psmExit();

   appletUnlockExit();
#endif
}

#ifdef HAVE_LIBNX
static void frontend_switch_exec(const char *path, bool should_load_game)
{
   char game_path[PATH_MAX-4];
   const char *arg_data[3];
   int args           = 0;

   game_path[0]       = NULL;
   arg_data[0]        = NULL;

   arg_data[args]     = elf_path_cst;
   arg_data[args + 1] = NULL;
   args++;

   RARCH_LOG("Attempt to load core: [%s].\n", path);
#ifndef IS_SALAMANDER
   if (should_load_game && !path_is_empty(RARCH_PATH_CONTENT))
   {
      strcpy(game_path, path_get(RARCH_PATH_CONTENT));
      arg_data[args] = game_path;
      arg_data[args + 1] = NULL;
      args++;
      RARCH_LOG("content path: [%s].\n", path_get(RARCH_PATH_CONTENT));
   }
#endif

   if (path && path[0])
   {
#ifdef IS_SALAMANDER
      struct stat sbuff;
      bool file_exists = stat(path, &sbuff) == 0;

      if (!file_exists)
      {
         char core_path[PATH_MAX];

         /* find first valid core and load it if the target core doesnt exist */
         get_first_valid_core(&core_path[0]);

         if (core_path[0] == '\0')
            svcExitProcess();
      }
#endif
      char *argBuffer = (char *)malloc(PATH_MAX);
      if (should_load_game)
         snprintf(argBuffer, PATH_MAX, "%s \"%s\"", path, game_path);
      else
         snprintf(argBuffer, PATH_MAX, "%s", path);

      envSetNextLoad(path, argBuffer);
   }
}

#ifndef IS_SALAMANDER
static bool frontend_switch_set_fork(enum frontend_fork fork_mode)
{
   switch (fork_mode)
   {
   case FRONTEND_FORK_CORE:
      RARCH_LOG("FRONTEND_FORK_CORE\n");
      switch_fork_mode = fork_mode;
      break;
   case FRONTEND_FORK_CORE_WITH_ARGS:
      RARCH_LOG("FRONTEND_FORK_CORE_WITH_ARGS\n");
      switch_fork_mode = fork_mode;
      break;
   case FRONTEND_FORK_RESTART:
      RARCH_LOG("FRONTEND_FORK_RESTART\n");
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

static void frontend_switch_exitspawn(char *s, size_t len)
{
   bool should_load_game = false;
#ifndef IS_SALAMANDER
   if (switch_fork_mode == FRONTEND_FORK_NONE)
      return;

   switch (switch_fork_mode)
   {
   case FRONTEND_FORK_CORE_WITH_ARGS:
      should_load_game = true;
      break;
   default:
      break;
   }
#endif
   frontend_switch_exec(s, should_load_game);
}

#if 0
/* TODO/FIXME - should be refactored into something that can be used for all
 * RetroArch versions, and not just Switch */
static void argb_to_rgba8(uint32_t *buff, uint32_t height, uint32_t width)
{
   uint32_t h, w;
   /* Convert */
   for (h = 0; h < height; h++)
   {
      for (w = 0; w < width; w++)
      {
         uint32_t offset = (h * width) + w;
         uint32_t c      = buff[offset];

         uint32_t a      = (uint32_t)((c & 0xff000000) >> 24);
         uint32_t r      = (uint32_t)((c & 0x00ff0000) >> 16);
         uint32_t g      = (uint32_t)((c & 0x0000ff00) >> 8);
         uint32_t b      = (uint32_t)(c & 0x000000ff);

         buff[offset]    = RGBA8(r, g, b, a);
      }
   }
}

static void frontend_switch_showsplash(void)
{
   printf("[Splash] Showing splashScreen\n");

   NWindow *win = nwindowGetDefault();
   Framebuffer fb;
   framebufferCreate(&fb, win, 1280, 720, PIXEL_FORMAT_RGBA_8888, 2);
   framebufferMakeLinear(&fb);

   if (splashData)
   {
      uint32_t width       = 0;
      uint32_t height      = 0;
      uint32_t stride;
      uint32_t *frambuffer = (uint32_t *)framebufferBegin(&fb, &stride);

      gfx_cpy_dsp_buf(frambuffer, splashData, width, height, stride, false);

      framebufferEnd(&fb);
   }

   framebufferClose(&fb);
}

/* From rpng_test.c */
static bool rpng_load_image_argb(const char *path,
      uint32_t **data, unsigned *width, unsigned *height)
{
   int retval;
   size_t file_len;
   bool ret              = true;
   rpng_t *rpng          = NULL;
   void *ptr             = NULL;
   struct nbio_t *handle = (struct nbio_t *)nbio_open(path, NBIO_READ);

   if (!handle)
      goto end;

   nbio_begin_read(handle);

   while (!nbio_iterate(handle))
      svcSleepThread(3);

   ptr = nbio_get_ptr(handle, &file_len);

   if (!ptr)
   {
      ret = false;
      goto end;
   }

   rpng = rpng_alloc();

   if (!rpng)
   {
      ret = false;
      goto end;
   }

   if (!rpng_set_buf_ptr(rpng, (uint8_t *)ptr, file_len))
   {
      ret = false;
      goto end;
   }

   if (!rpng_start(rpng))
   {
      ret = false;
      goto end;
   }

   while (rpng_iterate_image(rpng))
      svcSleepThread(3);

   if (!rpng_is_valid(rpng))
   {
      ret = false;
      goto end;
   }

   do
   {
      retval = rpng_process_image(rpng, (void **)data, file_len, width, height);
      svcSleepThread(3);
   } while (retval == IMAGE_PROCESS_NEXT);

   if (retval == IMAGE_PROCESS_ERROR || retval == IMAGE_PROCESS_ERROR_END)
      ret = false;

end:
   if (handle)
      nbio_free(handle);

   if (rpng)
      rpng_free(rpng);

   rpng = NULL;

   if (!ret)
      free(*data);

   return ret;
}
#endif

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
   svcSleepThread(rqtp->tv_nsec + (rqtp->tv_sec * 1000000000));
   return 0;
}

long sysconf(int name)
{
   switch (name)
   {
   case 8:
      return 0x1000;
   }
   return -1;
}

ssize_t readlink(const char *restrict path, char *restrict buf, size_t bufsize)
{
   return -1;
}

/* Taken from glibc */
char *realpath(const char *name, char *resolved)
{
   char *rpath, *dest, *extra_buf = NULL;
   const char *start, *end, *rpath_limit;
   long int path_max;
   int num_links = 0;

   if (name == NULL)
   {
      /* As per Single Unix Specification V2 we must return an error if
       either parameter is a null pointer.  We extend this to allow
       the RESOLVED parameter to be NULL in case the we are expected to
       allocate the room for the return value.  */
      return NULL;
   }

   if (name[0] == '\0')
   {
      /* As per Single Unix Specification V2 we must return an error if
       the name argument points to an empty string.  */
      return NULL;
   }

#ifdef PATH_MAX
   path_max = PATH_MAX;
#else
   path_max = pathconf(name, _PC_PATH_MAX);
   if (path_max <= 0)
      path_max = 1024;
#endif

   if (resolved == NULL)
   {
      rpath = malloc(path_max);
      if (rpath == NULL)
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
      int n;

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
            if (new_rpath == NULL)
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
   if (resolved == NULL)
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
   bool recording_supported      = false;
   uint32_t width                = 0;
   uint32_t height               = 0;

   nifmInitialize(NifmServiceType_User);
   
   if(hosversionBefore(8, 0, 0))
      pcvInitialize();
   else
      clkrstInitialize();

   appletLockExit();
   appletHook(&applet_hook_cookie, on_applet_hook, NULL);
   appletSetFocusHandlingMode(AppletFocusHandlingMode_NoSuspend);

   appletIsGamePlayRecordingSupported(&recording_supported);
   if(recording_supported)
      appletInitializeGamePlayRecording();

#ifdef NXLINK
   socketInitializeDefault();
   nxlink_connected = nxlinkStdio() != -1;
#ifndef IS_SALAMANDER
   verbosity_enable();
#endif /* IS_SALAMANDER */
#endif /* NXLINK */

   Result rc;
   rc = psmInitialize();
   if (R_SUCCEEDED(rc))
       psmInitialized = true;
   else
   {
       RARCH_WARN("Error initializing psm\n");
   }

#if 0
#ifndef HAVE_OPENGL
   /* Load splash */
   if (!splashData)
   {
      rarch_system_info_t *sys_info = runloop_get_system_info();
      retro_get_system_info(sys_info);

      if (sys_info)
      {
         const char *core_name       = sys_info->info.library_name;
         char *full_core_splash_path = (char*)malloc(PATH_MAX);

         snprintf(full_core_splash_path,
               PATH_MAX, "/retroarch/rgui/splash/%s.png", core_name);

         rpng_load_image_argb((const char *)
               full_core_splash_path, &splashData, &width, &height);

         if (splashData)
         {
            argb_to_rgba8(splashData, height, width);
            frontend_switch_showsplash();
         }
         else
         {
            rpng_load_image_argb(
                  "/retroarch/rgui/splash/RetroArch.png",
                  &splashData, &width, &height);

            if (splashData)
            {
               argb_to_rgba8(splashData, height, width);
               frontend_switch_showsplash();
            }
         }

         free(full_core_splash_path);
      }
      else
      {
         rpng_load_image_argb(
               "/retroarch/rgui/splash/RetroArch.png",
               &splashData, &width, &height);

         if (splashData)
         {
            argb_to_rgba8(splashData, height, width);
            frontend_switch_showsplash();
         }
      }
   }
   else
   {
      frontend_switch_showsplash();
   }
#endif
#endif

#endif /* HAVE_LIBNX (splash) */
}

static int frontend_switch_get_rating(void)
{
   return 11;
}

enum frontend_architecture frontend_switch_get_architecture(void)
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

static uint64_t frontend_switch_get_mem_free(void)
{
   struct mallinfo mem_info = mallinfo();
   return mem_info.fordblks;
}

static uint64_t frontend_switch_get_mem_total(void)
{
   struct mallinfo mem_info = mallinfo();
   return mem_info.usmblks;
}

static enum frontend_powerstate 
frontend_switch_get_powerstate(int *seconds, int *percent)
{
   uint32_t pct;
   ChargerType ct;
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
      case ChargerType_Charger:
      case ChargerType_Usb:
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

   strlcpy(s, "Horizon OS", len);

#ifdef HAVE_LIBNX
   *major = 0;
   *minor = 0;

   hosVersion = hosversionGet();
   *major     = HOSVER_MAJOR(hosVersion);
   *minor     = HOSVER_MINOR(hosVersion);
#else
   /* defaults in case we error out */
   *major     = 0;
   *minor     = 0;

   LIB_ASSERT_OK(fail, sm_init());
   LIB_ASSERT_OK(fail_sm, sm_get_service(&set_sys, "set:sys"));

   rq = ipc_make_request(3);
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
   strlcpy(s, "Nintendo Switch", len);
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
        frontend_switch_get_environment_settings,
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
        NULL, /* load_content */
        frontend_switch_get_architecture,
        frontend_switch_get_powerstate,
        frontend_switch_parse_drive_list,
        frontend_switch_get_mem_total,
        frontend_switch_get_mem_free,
        NULL, /* install_signal_handler */
        NULL, /* get_signal_handler_state */
        NULL, /* set_signal_handler_state */
        NULL, /* destroy_signal_handler_state */
        NULL, /* attach_console */
        NULL, /* detach_console */
        NULL, /* watch_path_for_changes */
        NULL, /* check_for_path_changes */
        NULL, /* set_sustained_performance_mode */
        NULL, /* get_cpu_model_name */
        NULL, /* get_user_language */
        "switch",
};
