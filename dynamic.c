/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include <boolean.h>
#include <file/file_path.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <dynamic/dylib.h>
#include <string/stdstring.h>
#include <retro_assert.h>

#include <features/features_cpu.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_CHEEVOS
#include "cheevos/cheevos.h"
#endif

#ifdef HAVE_NETWORKING
#include "network/netplay/netplay.h"
#endif

#include "dynamic.h"
#include "command.h"

#include "audio/audio_driver.h"
#include "camera/camera_driver.h"
#include "location/location_driver.h"
#include "record/record_driver.h"
#include "core.h"
#include "driver.h"
#include "performance_counters.h"
#include "gfx/video_driver.h"

#include "cores/internal_cores.h"
#include "frontend/frontend_driver.h"
#include "content.h"
#include "dirs.h"
#include "paths.h"
#include "retroarch.h"
#include "configuration.h"
#include "msg_hash.h"
#include "verbosity.h"

#ifdef HAVE_DYNAMIC
#define SYMBOL(x) do { \
   function_t func = dylib_proc(lib_handle, #x); \
   memcpy(&current_core->x, &func, sizeof(func)); \
   if (current_core->x == NULL) { RARCH_ERR("Failed to load symbol: \"%s\"\n", #x); retroarch_fail(1, "init_libretro_sym()"); } \
} while (0)

static dylib_t lib_handle;
#else
#define SYMBOL(x) current_core->x = x
#endif

#define SYMBOL_DUMMY(x) current_core->x = libretro_dummy_##x

#ifdef HAVE_FFMPEG
#define SYMBOL_FFMPEG(x) current_core->x = libretro_ffmpeg_##x
#endif

#ifdef HAVE_IMAGEVIEWER
#define SYMBOL_IMAGEVIEWER(x) current_core->x = libretro_imageviewer_##x
#endif

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
#define SYMBOL_NETRETROPAD(x) current_core->x = libretro_netretropad_##x
#endif

#if defined(HAVE_VIDEO_PROCESSOR)
#define SYMBOL_VIDEOPROCESSOR(x) current_core->x = libretro_videoprocessor_##x
#endif

static bool ignore_environment_cb   = false;
static bool core_set_shared_context = false;
static bool *load_no_content_hook   = NULL;

const struct retro_subsystem_info *libretro_find_subsystem_info(
      const struct retro_subsystem_info *info, unsigned num_info,
      const char *ident)
{
   unsigned i;
   for (i = 0; i < num_info; i++)
   {
      if (string_is_equal(info[i].ident, ident))
         return &info[i];
      else if (string_is_equal(info[i].desc, ident))
         return &info[i];
   }

   return NULL;
}

/**
 * libretro_find_controller_description:
 * @info                         : Pointer to controller info handle.
 * @id                           : Identifier of controller to search
 *                                 for.
 *
 * Search for a controller of type @id in @info.
 *
 * Returns: controller description of found controller on success,
 * otherwise NULL.
 **/
const struct retro_controller_description *
libretro_find_controller_description(
      const struct retro_controller_info *info, unsigned id)
{
   unsigned i;

   for (i = 0; i < info->num_types; i++)
   {
      if (info->types[i].id != id)
         continue;

      return &info->types[i];
   }

   return NULL;
}

/**
 * libretro_free_system_info:
 * @info                         : Pointer to system info information.
 *
 * Frees system information.
 **/
void libretro_free_system_info(struct retro_system_info *info)
{
   if (!info)
      return;

   free((void*)info->library_name);
   free((void*)info->library_version);
   free((void*)info->valid_extensions);
   memset(info, 0, sizeof(*info));
}


static bool environ_cb_get_system_info(unsigned cmd, void *data)
{
   switch (cmd)
   {
      case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
         *load_no_content_hook = *(const bool*)data;
         break;

      default:
         return false;
   }

   return true;
}

#ifdef HAVE_DYNAMIC
/**
 * libretro_get_environment_info:
 * @func                         : Function pointer for get_environment_info.
 * @load_no_content              : If true, core should be able to auto-start
 *                                 without any content loaded.
 *
 * Sets environment callback in order to get statically known
 * information from it.
 *
 * Fetched via environment callbacks instead of
 * retro_get_system_info(), as this info is part of extensions.
 *
 * Should only be called once right after core load to
 * avoid overwriting the "real" environ callback.
 *
 * For statically linked cores, pass retro_set_environment as argument.
 */
static void libretro_get_environment_info(void (*func)(retro_environment_t),
      bool *load_no_content)
{
   load_no_content_hook = load_no_content;

   /* load_no_content gets set in this callback. */
   func(environ_cb_get_system_info);

   /* It's possible that we just set get_system_info callback 
    * to the currently running core.
    *
    * Make sure we reset it to the actual environment callback.
    * Ignore any environment callbacks here in case we're running 
    * on the non-current core. */
   ignore_environment_cb = true;
   func(rarch_environment_cb);
   ignore_environment_cb = false;
}

static bool load_dynamic_core(void)
{
   function_t sym       = dylib_proc(NULL, "retro_init");

   if (sym)
   {
      /* Try to verify that -lretro was not linked in from other modules
       * since loading it dynamically and with -l will fail hard. */
      RARCH_ERR("Serious problem. RetroArch wants to load libretro cores"
            " dynamically, but it is already linked.\n");
      RARCH_ERR("This could happen if other modules RetroArch depends on "
            "link against libretro directly.\n");
      RARCH_ERR("Proceeding could cause a crash. Aborting ...\n");
      retroarch_fail(1, "init_libretro_sym()");
   }

   if (string_is_empty(path_get(RARCH_PATH_CORE)))
   {
      RARCH_ERR("RetroArch is built for dynamic libretro cores, but "
            "libretro_path is not set. Cannot continue.\n");
      retroarch_fail(1, "init_libretro_sym()");
   }

   /* Need to use absolute path for this setting. It can be
    * saved to content history, and a relative path would
    * break in that scenario. */
   path_resolve_realpath(
         path_get_ptr(RARCH_PATH_CORE),
         path_get_realsize(RARCH_PATH_CORE));

   RARCH_LOG("Loading dynamic libretro core from: \"%s\"\n",
         path_get(RARCH_PATH_CORE));
   lib_handle = dylib_load(path_get(RARCH_PATH_CORE));

   if (lib_handle)
      return true;

   RARCH_ERR("Failed to open libretro core: \"%s\"\n",
         path_get(RARCH_PATH_CORE));
   RARCH_ERR("Error(s): %s\n", dylib_error());

   runloop_msg_queue_push(msg_hash_to_str(MSG_FAILED_TO_OPEN_LIBRETRO_CORE), 1, 180, true);

   return false;
}

static dylib_t libretro_get_system_info_lib(const char *path,
      struct retro_system_info *info, bool *load_no_content)
{
   dylib_t lib = dylib_load(path);
   void (*proc)(struct retro_system_info*);

   if (!lib)
   {
      RARCH_ERR("%s: \"%s\"\n",
            msg_hash_to_str(MSG_FAILED_TO_OPEN_LIBRETRO_CORE),
            path);
      RARCH_ERR("Error(s): %s\n", dylib_error());
      return NULL;
   }

   proc = (void (*)(struct retro_system_info*))
      dylib_proc(lib, "retro_get_system_info");

   if (!proc)
   {
      dylib_close(lib);
      return NULL;
   }

   proc(info);

   if (load_no_content)
   {
      void (*set_environ)(retro_environment_t);
      *load_no_content = false;
      set_environ = (void (*)(retro_environment_t))
         dylib_proc(lib, "retro_set_environment");

      if (!set_environ)
         return lib;

      libretro_get_environment_info(set_environ, load_no_content);
   }

   return lib;
}
#endif

/**
 * libretro_get_system_info:
 * @path                         : Path to libretro library.
 * @info                         : Pointer to system info information.
 * @load_no_content              : If true, core should be able to auto-start
 *                                 without any content loaded.
 *
 * Gets system info from an arbitrary lib.
 * The struct returned must be freed as strings are allocated dynamically.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool libretro_get_system_info(const char *path,
      struct retro_system_info *info, bool *load_no_content)
{
   struct retro_system_info dummy_info;
#ifdef HAVE_DYNAMIC
   dylib_t lib;
#endif

   dummy_info.library_name     = NULL;
   dummy_info.library_version  = NULL;
   dummy_info.valid_extensions = NULL;
   dummy_info.need_fullpath    = false;
   dummy_info.block_extract    = false;

#ifdef HAVE_DYNAMIC
   lib                         = libretro_get_system_info_lib(
         path, &dummy_info, load_no_content);

   if (!lib)
      return false;
#else
   if (load_no_content)
   {
      load_no_content_hook = load_no_content;

      /* load_no_content gets set in this callback. */
      retro_set_environment(environ_cb_get_system_info);

      /* It's possible that we just set get_system_info callback 
       * to the currently running core.
       *
       * Make sure we reset it to the actual environment callback.
       * Ignore any environment callbacks here in case we're running 
       * on the non-current core. */
      ignore_environment_cb = true;
      retro_set_environment(rarch_environment_cb);
      ignore_environment_cb = false;
   }

   retro_get_system_info(&dummy_info);
#endif

   memcpy(info, &dummy_info, sizeof(*info));

   if (!string_is_empty(dummy_info.library_name))
      info->library_name    = strdup(dummy_info.library_name);
   if (!string_is_empty(dummy_info.library_version))
      info->library_version    = strdup(dummy_info.library_version);

   if (dummy_info.valid_extensions)
      info->valid_extensions = strdup(dummy_info.valid_extensions);

#ifdef HAVE_DYNAMIC
   dylib_close(lib);
#endif

   return true;
}

/**
 * load_symbols:
 * @type                        : Type of core to be loaded.
 *                                If CORE_TYPE_DUMMY, will
 *                                load dummy symbols.
 *
 * Setup libretro callback symbols. Returns true on success,
 * or false if symbols could not be loaded.
 **/
static bool load_symbols(enum rarch_core_type type, struct retro_core_t *current_core)
{
   switch (type)
   {
      case CORE_TYPE_PLAIN:
#ifdef HAVE_DYNAMIC
         if (!load_dynamic_core())
            return false;
#endif

         SYMBOL(retro_init);
         SYMBOL(retro_deinit);

         SYMBOL(retro_api_version);
         SYMBOL(retro_get_system_info);
         SYMBOL(retro_get_system_av_info);

         SYMBOL(retro_set_environment);
         SYMBOL(retro_set_video_refresh);
         SYMBOL(retro_set_audio_sample);
         SYMBOL(retro_set_audio_sample_batch);
         SYMBOL(retro_set_input_poll);
         SYMBOL(retro_set_input_state);

         SYMBOL(retro_set_controller_port_device);

         SYMBOL(retro_reset);
         SYMBOL(retro_run);

         SYMBOL(retro_serialize_size);
         SYMBOL(retro_serialize);
         SYMBOL(retro_unserialize);

         SYMBOL(retro_cheat_reset);
         SYMBOL(retro_cheat_set);

         SYMBOL(retro_load_game);
         SYMBOL(retro_load_game_special);

         SYMBOL(retro_unload_game);
         SYMBOL(retro_get_region);
         SYMBOL(retro_get_memory_data);
         SYMBOL(retro_get_memory_size);
         break;
      case CORE_TYPE_DUMMY:
         SYMBOL_DUMMY(retro_init);
         SYMBOL_DUMMY(retro_deinit);

         SYMBOL_DUMMY(retro_api_version);
         SYMBOL_DUMMY(retro_get_system_info);
         SYMBOL_DUMMY(retro_get_system_av_info);

         SYMBOL_DUMMY(retro_set_environment);
         SYMBOL_DUMMY(retro_set_video_refresh);
         SYMBOL_DUMMY(retro_set_audio_sample);
         SYMBOL_DUMMY(retro_set_audio_sample_batch);
         SYMBOL_DUMMY(retro_set_input_poll);
         SYMBOL_DUMMY(retro_set_input_state);

         SYMBOL_DUMMY(retro_set_controller_port_device);

         SYMBOL_DUMMY(retro_reset);
         SYMBOL_DUMMY(retro_run);

         SYMBOL_DUMMY(retro_serialize_size);
         SYMBOL_DUMMY(retro_serialize);
         SYMBOL_DUMMY(retro_unserialize);

         SYMBOL_DUMMY(retro_cheat_reset);
         SYMBOL_DUMMY(retro_cheat_set);

         SYMBOL_DUMMY(retro_load_game);
         SYMBOL_DUMMY(retro_load_game_special);

         SYMBOL_DUMMY(retro_unload_game);
         SYMBOL_DUMMY(retro_get_region);
         SYMBOL_DUMMY(retro_get_memory_data);
         SYMBOL_DUMMY(retro_get_memory_size);
         break;
      case CORE_TYPE_FFMPEG:
#ifdef HAVE_FFMPEG
         SYMBOL_FFMPEG(retro_init);
         SYMBOL_FFMPEG(retro_deinit);

         SYMBOL_FFMPEG(retro_api_version);
         SYMBOL_FFMPEG(retro_get_system_info);
         SYMBOL_FFMPEG(retro_get_system_av_info);

         SYMBOL_FFMPEG(retro_set_environment);
         SYMBOL_FFMPEG(retro_set_video_refresh);
         SYMBOL_FFMPEG(retro_set_audio_sample);
         SYMBOL_FFMPEG(retro_set_audio_sample_batch);
         SYMBOL_FFMPEG(retro_set_input_poll);
         SYMBOL_FFMPEG(retro_set_input_state);

         SYMBOL_FFMPEG(retro_set_controller_port_device);

         SYMBOL_FFMPEG(retro_reset);
         SYMBOL_FFMPEG(retro_run);

         SYMBOL_FFMPEG(retro_serialize_size);
         SYMBOL_FFMPEG(retro_serialize);
         SYMBOL_FFMPEG(retro_unserialize);

         SYMBOL_FFMPEG(retro_cheat_reset);
         SYMBOL_FFMPEG(retro_cheat_set);

         SYMBOL_FFMPEG(retro_load_game);
         SYMBOL_FFMPEG(retro_load_game_special);

         SYMBOL_FFMPEG(retro_unload_game);
         SYMBOL_FFMPEG(retro_get_region);
         SYMBOL_FFMPEG(retro_get_memory_data);
         SYMBOL_FFMPEG(retro_get_memory_size);
#endif
         break;
      case CORE_TYPE_IMAGEVIEWER:
#ifdef HAVE_IMAGEVIEWER
         SYMBOL_IMAGEVIEWER(retro_init);
         SYMBOL_IMAGEVIEWER(retro_deinit);

         SYMBOL_IMAGEVIEWER(retro_api_version);
         SYMBOL_IMAGEVIEWER(retro_get_system_info);
         SYMBOL_IMAGEVIEWER(retro_get_system_av_info);

         SYMBOL_IMAGEVIEWER(retro_set_environment);
         SYMBOL_IMAGEVIEWER(retro_set_video_refresh);
         SYMBOL_IMAGEVIEWER(retro_set_audio_sample);
         SYMBOL_IMAGEVIEWER(retro_set_audio_sample_batch);
         SYMBOL_IMAGEVIEWER(retro_set_input_poll);
         SYMBOL_IMAGEVIEWER(retro_set_input_state);

         SYMBOL_IMAGEVIEWER(retro_set_controller_port_device);

         SYMBOL_IMAGEVIEWER(retro_reset);
         SYMBOL_IMAGEVIEWER(retro_run);

         SYMBOL_IMAGEVIEWER(retro_serialize_size);
         SYMBOL_IMAGEVIEWER(retro_serialize);
         SYMBOL_IMAGEVIEWER(retro_unserialize);

         SYMBOL_IMAGEVIEWER(retro_cheat_reset);
         SYMBOL_IMAGEVIEWER(retro_cheat_set);

         SYMBOL_IMAGEVIEWER(retro_load_game);
         SYMBOL_IMAGEVIEWER(retro_load_game_special);

         SYMBOL_IMAGEVIEWER(retro_unload_game);
         SYMBOL_IMAGEVIEWER(retro_get_region);
         SYMBOL_IMAGEVIEWER(retro_get_memory_data);
         SYMBOL_IMAGEVIEWER(retro_get_memory_size);
#endif
         break;
      case CORE_TYPE_NETRETROPAD:
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
         SYMBOL_NETRETROPAD(retro_init);
         SYMBOL_NETRETROPAD(retro_deinit);

         SYMBOL_NETRETROPAD(retro_api_version);
         SYMBOL_NETRETROPAD(retro_get_system_info);
         SYMBOL_NETRETROPAD(retro_get_system_av_info);

         SYMBOL_NETRETROPAD(retro_set_environment);
         SYMBOL_NETRETROPAD(retro_set_video_refresh);
         SYMBOL_NETRETROPAD(retro_set_audio_sample);
         SYMBOL_NETRETROPAD(retro_set_audio_sample_batch);
         SYMBOL_NETRETROPAD(retro_set_input_poll);
         SYMBOL_NETRETROPAD(retro_set_input_state);

         SYMBOL_NETRETROPAD(retro_set_controller_port_device);

         SYMBOL_NETRETROPAD(retro_reset);
         SYMBOL_NETRETROPAD(retro_run);

         SYMBOL_NETRETROPAD(retro_serialize_size);
         SYMBOL_NETRETROPAD(retro_serialize);
         SYMBOL_NETRETROPAD(retro_unserialize);

         SYMBOL_NETRETROPAD(retro_cheat_reset);
         SYMBOL_NETRETROPAD(retro_cheat_set);

         SYMBOL_NETRETROPAD(retro_load_game);
         SYMBOL_NETRETROPAD(retro_load_game_special);

         SYMBOL_NETRETROPAD(retro_unload_game);
         SYMBOL_NETRETROPAD(retro_get_region);
         SYMBOL_NETRETROPAD(retro_get_memory_data);
         SYMBOL_NETRETROPAD(retro_get_memory_size);
#endif
         break;
      case CORE_TYPE_VIDEO_PROCESSOR:
#if defined(HAVE_VIDEO_PROCESSOR)
         SYMBOL_VIDEOPROCESSOR(retro_init);
         SYMBOL_VIDEOPROCESSOR(retro_deinit);

         SYMBOL_VIDEOPROCESSOR(retro_api_version);
         SYMBOL_VIDEOPROCESSOR(retro_get_system_info);
         SYMBOL_VIDEOPROCESSOR(retro_get_system_av_info);

         SYMBOL_VIDEOPROCESSOR(retro_set_environment);
         SYMBOL_VIDEOPROCESSOR(retro_set_video_refresh);
         SYMBOL_VIDEOPROCESSOR(retro_set_audio_sample);
         SYMBOL_VIDEOPROCESSOR(retro_set_audio_sample_batch);
         SYMBOL_VIDEOPROCESSOR(retro_set_input_poll);
         SYMBOL_VIDEOPROCESSOR(retro_set_input_state);

         SYMBOL_VIDEOPROCESSOR(retro_set_controller_port_device);

         SYMBOL_VIDEOPROCESSOR(retro_reset);
         SYMBOL_VIDEOPROCESSOR(retro_run);

         SYMBOL_VIDEOPROCESSOR(retro_serialize_size);
         SYMBOL_VIDEOPROCESSOR(retro_serialize);
         SYMBOL_VIDEOPROCESSOR(retro_unserialize);

         SYMBOL_VIDEOPROCESSOR(retro_cheat_reset);
         SYMBOL_VIDEOPROCESSOR(retro_cheat_set);

         SYMBOL_VIDEOPROCESSOR(retro_load_game);
         SYMBOL_VIDEOPROCESSOR(retro_load_game_special);

         SYMBOL_VIDEOPROCESSOR(retro_unload_game);
         SYMBOL_VIDEOPROCESSOR(retro_get_region);
         SYMBOL_VIDEOPROCESSOR(retro_get_memory_data);
         SYMBOL_VIDEOPROCESSOR(retro_get_memory_size);
#endif
         break;
   }

   return true;
}

/**
 * init_libretro_sym:
 * @type                        : Type of core to be loaded.
 *                                If CORE_TYPE_DUMMY, will
 *                                load dummy symbols.
 *
 * Initializes libretro symbols and
 * setups environment callback functions. Returns true on success,
 * or false if symbols could not be loaded.
 **/
bool init_libretro_sym(enum rarch_core_type type, struct retro_core_t *current_core)
{
   /* Guarantee that we can do "dirty" casting.
    * Every OS that this program supports should pass this. */
   retro_assert(sizeof(void*) == sizeof(void (*)(void)));

   if (!load_symbols(type, current_core))
      return false;

   return true;
}

bool libretro_get_shared_context(void)
{
   return core_set_shared_context;
}

/**
 * uninit_libretro_sym:
 *
 * Frees libretro core.
 *
 * Frees all core options,
 * associated state, and
 * unbind all libretro callback symbols.
 **/
void uninit_libretro_sym(struct retro_core_t *current_core)
{
#ifdef HAVE_DYNAMIC
   if (lib_handle)
      dylib_close(lib_handle);
   lib_handle = NULL;
#endif

   memset(current_core, 0, sizeof(struct retro_core_t));

   core_set_shared_context = false;

   rarch_ctl(RARCH_CTL_CORE_OPTIONS_DEINIT, NULL);
   rarch_ctl(RARCH_CTL_SYSTEM_INFO_FREE, NULL);
   rarch_ctl(RARCH_CTL_FRAME_TIME_FREE, NULL);
   camera_driver_ctl(RARCH_CAMERA_CTL_UNSET_ACTIVE, NULL);
   location_driver_ctl(RARCH_LOCATION_CTL_UNSET_ACTIVE, NULL);

   /* Performance counters no longer valid. */
   performance_counters_clear();
}

static void rarch_log_libretro(enum retro_log_level level,
      const char *fmt, ...)
{
   va_list vp;
   settings_t *settings = config_get_ptr();

   if ((unsigned)level < settings->uints.libretro_log_level)
      return;

   va_start(vp, fmt);

   switch (level)
   {
      case RETRO_LOG_DEBUG:
         RARCH_LOG_V("[libretro DEBUG]", fmt, vp);
         break;

      case RETRO_LOG_INFO:
         RARCH_LOG_OUTPUT_V("[libretro INFO]", fmt, vp);
         break;

      case RETRO_LOG_WARN:
         RARCH_WARN_V("[libretro WARN]", fmt, vp);
         break;

      case RETRO_LOG_ERROR:
         RARCH_ERR_V("[libretro ERROR]", fmt, vp);
         break;

      default:
         break;
   }

   va_end(vp);
}

static size_t mmap_add_bits_down(size_t n)
{
   n |= n >>  1;
   n |= n >>  2;
   n |= n >>  4;
   n |= n >>  8;
   n |= n >> 16;
   
   /* double shift to avoid warnings on 32bit (it's dead code, but compilers suck) */
   if (sizeof(size_t) > 4)
      n |= n >> 16 >> 16;
   
   return n;
}

static size_t mmap_inflate(size_t addr, size_t mask)
{
    while (mask)
   {
      size_t tmp = (mask - 1) & ~mask;

      /* to put in an 1 bit instead, OR in tmp+1 */
      addr       = ((addr & ~tmp) << 1) | (addr & tmp);
      mask       = mask & (mask - 1);
   }
   
   return addr;
}

static size_t mmap_reduce(size_t addr, size_t mask)
{
   while (mask)
   {
      size_t tmp = (mask - 1) & ~mask;
      addr       = (addr & tmp) | ((addr >> 1) & ~tmp);
      mask       = (mask & (mask - 1)) >> 1;
   }
   
   return addr;
}

static size_t mmap_highest_bit(size_t n)
{
   n = mmap_add_bits_down(n);
   return n ^ (n >> 1);
}

static bool mmap_preprocess_descriptors(rarch_memory_descriptor_t *first, unsigned count)
{
   size_t                      top_addr = 1;
   rarch_memory_descriptor_t *desc      = NULL;
   const rarch_memory_descriptor_t *end = first + count;
   
   for (desc = first; desc < end; desc++)
   {
      if (desc->core.select != 0)
         top_addr |= desc->core.select;
      else
         top_addr |= desc->core.start + desc->core.len - 1;
   }
   
   top_addr = mmap_add_bits_down(top_addr);
   
   for (desc = first; desc < end; desc++)
   {
      if (desc->core.select == 0)
      {
         if (desc->core.len == 0)
            return false;
         
         if ((desc->core.len & (desc->core.len - 1)) != 0)
            return false;
         
         desc->core.select = top_addr & ~mmap_inflate(mmap_add_bits_down(desc->core.len - 1),
               desc->core.disconnect);
      }
      
      if (desc->core.len == 0)
         desc->core.len = mmap_add_bits_down(mmap_reduce(top_addr & ~desc->core.select,
                  desc->core.disconnect)) + 1;
      
      if (desc->core.start & ~desc->core.select)
         return false;
       
      while (mmap_reduce(top_addr & ~desc->core.select, desc->core.disconnect) >> 1 > desc->core.len - 1)
         desc->core.disconnect |= mmap_highest_bit(top_addr & ~desc->core.select & ~desc->core.disconnect);
      
      desc->disconnect_mask = mmap_add_bits_down(desc->core.len - 1);
      desc->core.disconnect &= desc->disconnect_mask;
      
      while ((~desc->disconnect_mask) >> 1 & desc->core.disconnect)
      {
         desc->disconnect_mask >>= 1;
         desc->core.disconnect &= desc->disconnect_mask;
      }
   }
   
   return true;
}

static bool dynamic_request_hw_context(enum retro_hw_context_type type,
      unsigned minor, unsigned major)
{
   switch (type)
   {
      case RETRO_HW_CONTEXT_NONE:
         RARCH_LOG("Requesting no HW context.\n");
         break;

      case RETRO_HW_CONTEXT_VULKAN:
#ifdef HAVE_VULKAN
         RARCH_LOG("Requesting Vulkan context.\n");
         break;
#else
         RARCH_ERR("Requesting Vulkan context, but RetroArch is not compiled against Vulkan. Cannot use HW context.\n");
         return false;
#endif

#if defined(HAVE_OPENGLES)

#if (defined(HAVE_OPENGLES2) || defined(HAVE_OPENGLES3))
      case RETRO_HW_CONTEXT_OPENGLES2:
      case RETRO_HW_CONTEXT_OPENGLES3:
         RARCH_LOG("Requesting OpenGLES%u context.\n",
               type == RETRO_HW_CONTEXT_OPENGLES2 ? 2 : 3);
         break;

#if defined(HAVE_OPENGLES3)
      case RETRO_HW_CONTEXT_OPENGLES_VERSION:
         RARCH_LOG("Requesting OpenGLES%u.%u context.\n",
               major, minor);
         break;
#endif

#endif
      case RETRO_HW_CONTEXT_OPENGL:
      case RETRO_HW_CONTEXT_OPENGL_CORE:
         RARCH_ERR("Requesting OpenGL context, but RetroArch "
               "is compiled against OpenGLES. Cannot use HW context.\n");
         return false;

#elif defined(HAVE_OPENGL)
      case RETRO_HW_CONTEXT_OPENGLES2:
      case RETRO_HW_CONTEXT_OPENGLES3:
         RARCH_ERR("Requesting OpenGLES%u context, but RetroArch "
               "is compiled against OpenGL. Cannot use HW context.\n",
               type == RETRO_HW_CONTEXT_OPENGLES2 ? 2 : 3);
         return false;

      case RETRO_HW_CONTEXT_OPENGLES_VERSION:
         RARCH_ERR("Requesting OpenGLES%u.%u context, but RetroArch "
               "is compiled against OpenGL. Cannot use HW context.\n",
               major, minor);
         return false;

      case RETRO_HW_CONTEXT_OPENGL:
         RARCH_LOG("Requesting OpenGL context.\n");
         break;

      case RETRO_HW_CONTEXT_OPENGL_CORE:
         {
            gfx_ctx_flags_t flags;
            flags.flags = 0;
            BIT32_SET(flags.flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT);

            video_context_driver_set_flags(&flags);

            RARCH_LOG("Requesting core OpenGL context (%u.%u).\n",
                  major, minor);
         }
         break;
#endif

      default:
         RARCH_LOG("Requesting unknown context.\n");
         return false;
   }

   return true;
}

static bool dynamic_verify_hw_context(enum retro_hw_context_type type,
      unsigned minor, unsigned major)
{
   const char *video_ident = video_driver_get_ident();

   switch (type)
   {
      case RETRO_HW_CONTEXT_VULKAN:
         if (!string_is_equal(video_ident, "vulkan"))
            return false;
         break;
      case RETRO_HW_CONTEXT_OPENGLES2:
      case RETRO_HW_CONTEXT_OPENGLES3:
      case RETRO_HW_CONTEXT_OPENGLES_VERSION:
      case RETRO_HW_CONTEXT_OPENGL:
      case RETRO_HW_CONTEXT_OPENGL_CORE:
         if (!string_is_equal(video_ident, "gl"))
            return false;
         break;
      default:
         break;
   }

   return true;
}

static void core_performance_counter_start(struct retro_perf_counter *perf)
{
   if (rarch_ctl(RARCH_CTL_IS_PERFCNT_ENABLE, NULL))
   {
      perf->call_cnt++;
      perf->start      = cpu_features_get_perf_counter();
   }
}

static void core_performance_counter_stop(struct retro_perf_counter *perf)
{
   if (rarch_ctl(RARCH_CTL_IS_PERFCNT_ENABLE, NULL))
      perf->total += cpu_features_get_perf_counter() - perf->start;
}

/**
 * rarch_environment_cb:
 * @cmd                          : Identifier of command.
 * @data                         : Pointer to data.
 *
 * Environment callback function implementation.
 *
 * Returns: true (1) if environment callback command could
 * be performed, otherwise false (0).
 **/
bool rarch_environment_cb(unsigned cmd, void *data)
{
   unsigned p;
   settings_t         *settings = config_get_ptr();
   rarch_system_info_t *system  = runloop_get_system_info();
   
   if (ignore_environment_cb)
      return false;

   switch (cmd)
   {
      case RETRO_ENVIRONMENT_GET_OVERSCAN:
         *(bool*)data = !settings->bools.video_crop_overscan;
         RARCH_LOG("Environ GET_OVERSCAN: %u\n",
               (unsigned)!settings->bools.video_crop_overscan);
         break;

      case RETRO_ENVIRONMENT_GET_CAN_DUPE:
         *(bool*)data = true;
         RARCH_LOG("Environ GET_CAN_DUPE: true\n");
         break;

      case RETRO_ENVIRONMENT_GET_VARIABLE:
         if (!rarch_ctl(RARCH_CTL_CORE_OPTIONS_GET, data))
         {
            struct retro_variable *var = (struct retro_variable*)data;

            if (var)
            {
               RARCH_LOG("Environ GET_VARIABLE %s: not implemented.\n", var->key);
               var->value = NULL;
            }
         }

         break;

      case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
         *(bool*)data = rarch_ctl(RARCH_CTL_IS_CORE_OPTION_UPDATED, NULL);
         break;

      case RETRO_ENVIRONMENT_SET_VARIABLES:
         RARCH_LOG("Environ SET_VARIABLES.\n");

         rarch_ctl(RARCH_CTL_CORE_OPTIONS_DEINIT, NULL);
         rarch_ctl(RARCH_CTL_CORE_OPTIONS_INIT,   data);

         break;

      case RETRO_ENVIRONMENT_SET_MESSAGE:
      {
         const struct retro_message *msg = (const struct retro_message*)data;
         RARCH_LOG("Environ SET_MESSAGE: %s\n", msg->msg);
         runloop_msg_queue_push(msg->msg, 3, msg->frames, true);
         break;
      }

      case RETRO_ENVIRONMENT_SET_ROTATION:
      {
         unsigned rotation = *(const unsigned*)data;
         RARCH_LOG("Environ SET_ROTATION: %u\n", rotation);
         if (!settings->bools.video_allow_rotate)
            break;

         if (system)
            system->rotation = rotation;

         if (!video_driver_set_rotation(rotation))
            return false;
         break;
      }

      case RETRO_ENVIRONMENT_SHUTDOWN:
         RARCH_LOG("Environ SHUTDOWN.\n");
         rarch_ctl(RARCH_CTL_SET_SHUTDOWN,      NULL);
         rarch_ctl(RARCH_CTL_SET_CORE_SHUTDOWN, NULL);
         break;

      case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
         if (system)
         {
            system->performance_level = *(const unsigned*)data;
            RARCH_LOG("Environ PERFORMANCE_LEVEL: %u.\n",
                  system->performance_level);
         }
         break;

      case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
         if (string_is_empty(settings->paths.directory_system) || settings->bools.systemfiles_in_content_dir)
         {
            const char *fullpath = path_get(RARCH_PATH_CONTENT);
            if (!string_is_empty(fullpath))
            {
               char temp_path[PATH_MAX_LENGTH];

               temp_path[0] = '\0';

               if (string_is_empty(settings->paths.directory_system))
                  RARCH_WARN("SYSTEM DIR is empty, assume CONTENT DIR %s\n",
                        fullpath);
               fill_pathname_basedir(temp_path, fullpath, sizeof(temp_path));
               dir_set(RARCH_DIR_SYSTEM, temp_path);
            }

            *(const char**)data = dir_get_ptr(RARCH_DIR_SYSTEM);
            RARCH_LOG("Environ SYSTEM_DIRECTORY: \"%s\".\n",
                  dir_get(RARCH_DIR_SYSTEM));
         }
         else
         {
            *(const char**)data = settings->paths.directory_system;
            RARCH_LOG("Environ SYSTEM_DIRECTORY: \"%s\".\n",
               settings->paths.directory_system);
         }

         break;

      case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
         *(const char**)data = dir_get(RARCH_DIR_CURRENT_SAVEFILE);
         break;

      case RETRO_ENVIRONMENT_GET_USERNAME:
         *(const char**)data = *settings->paths.username ?
            settings->paths.username : NULL;
         RARCH_LOG("Environ GET_USERNAME: \"%s\".\n",
               settings->paths.username);
         break;

      case RETRO_ENVIRONMENT_GET_LANGUAGE:
#ifdef HAVE_LANGEXTRA
         {
            unsigned user_lang = *msg_hash_get_uint(MSG_HASH_USER_LANGUAGE);
            *(unsigned *)data  = user_lang;
            RARCH_LOG("Environ GET_LANGUAGE: \"%u\".\n", user_lang);
         }
#endif
         break;

      case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
      {
         enum retro_pixel_format pix_fmt =
            *(const enum retro_pixel_format*)data;

         switch (pix_fmt)
         {
            case RETRO_PIXEL_FORMAT_0RGB1555:
               RARCH_LOG("Environ SET_PIXEL_FORMAT: 0RGB1555.\n");
               break;

            case RETRO_PIXEL_FORMAT_RGB565:
               RARCH_LOG("Environ SET_PIXEL_FORMAT: RGB565.\n");
               break;
            case RETRO_PIXEL_FORMAT_XRGB8888:
               RARCH_LOG("Environ SET_PIXEL_FORMAT: XRGB8888.\n");
               break;
            default:
               return false;
         }

         video_driver_set_pixel_format(pix_fmt);
         break;
      }

      case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
      {
         static const char *libretro_btn_desc[]    = {
            "B (bottom)", "Y (left)", "Select", "Start",
            "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right",
            "A (right)", "X (up)",
            "L", "R", "L2", "R2", "L3", "R3",
         };

         if (system)
         {
            unsigned retro_id;
            const struct retro_input_descriptor *desc = NULL;
            memset(&system->input_desc_btn, 0,
                  sizeof(system->input_desc_btn));

            desc = (const struct retro_input_descriptor*)data;

            for (; desc->description; desc++)
            {
               unsigned retro_port = desc->port;

               retro_id            = desc->id;

               if (desc->port >= MAX_USERS)
                  continue;

               /* Ignore all others for now. */
               if (desc->device != RETRO_DEVICE_JOYPAD  &&
                     desc->device != RETRO_DEVICE_ANALOG)
                  continue;

               if (desc->id >= RARCH_FIRST_CUSTOM_BIND)
                  continue;

               if (desc->device == RETRO_DEVICE_ANALOG)
               {
                  switch (retro_id)
                  {
                     case RETRO_DEVICE_ID_ANALOG_X:
                        switch (desc->index)
                        {
                           case RETRO_DEVICE_INDEX_ANALOG_LEFT:
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_LEFT_X_PLUS]  = desc->description;
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_LEFT_X_MINUS] = desc->description;
                              break;
                           case RETRO_DEVICE_INDEX_ANALOG_RIGHT:
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_RIGHT_X_PLUS] = desc->description;
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_RIGHT_X_MINUS] = desc->description;
                              break;
                        }
                        break;
                     case RETRO_DEVICE_ID_ANALOG_Y:
                        switch (desc->index)
                        {
                           case RETRO_DEVICE_INDEX_ANALOG_LEFT:
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_LEFT_Y_PLUS] = desc->description;
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_LEFT_Y_MINUS] = desc->description;
                              break;
                           case RETRO_DEVICE_INDEX_ANALOG_RIGHT:
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_RIGHT_Y_PLUS] = desc->description;
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_RIGHT_Y_MINUS] = desc->description;
                              break;
                        }
                        break;
                  }
               }
               else
                  system->input_desc_btn[retro_port]
                     [retro_id] = desc->description;
            }

            RARCH_LOG("Environ SET_INPUT_DESCRIPTORS:\n");

            {
               unsigned max_users = *(input_driver_get_uint(INPUT_ACTION_MAX_USERS));

               for (p = 0; p < max_users; p++)
               {
                  for (retro_id = 0; retro_id < RARCH_FIRST_CUSTOM_BIND; retro_id++)
                  {
                     const char *description = system->input_desc_btn[p][retro_id];

                     if (!description)
                        continue;

                     RARCH_LOG("\tRetroPad, User %u, Button \"%s\" => \"%s\"\n",
                           p + 1, libretro_btn_desc[retro_id], description);
                  }
               }
            }

            core_set_input_descriptors();
         }

         break;
      }

      case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
      {
         retro_keyboard_event_t *frontend_key_event = NULL;
         retro_keyboard_event_t *key_event          = NULL;
         const struct retro_keyboard_callback *info =
            (const struct retro_keyboard_callback*)data;

         rarch_ctl(RARCH_CTL_FRONTEND_KEY_EVENT_GET, &frontend_key_event);
         rarch_ctl(RARCH_CTL_KEY_EVENT_GET, &key_event);

         RARCH_LOG("Environ SET_KEYBOARD_CALLBACK.\n");
         if (key_event)
            *key_event                  = info->callback;

         if (frontend_key_event && key_event)
            *frontend_key_event         = *key_event;
         break;
      }

      case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
         RARCH_LOG("Environ SET_DISK_CONTROL_INTERFACE.\n");
         if (system)
            system->disk_control_cb =
               *(const struct retro_disk_control_callback*)data;
         break;

      case RETRO_ENVIRONMENT_SET_HW_RENDER:
      case RETRO_ENVIRONMENT_SET_HW_RENDER | RETRO_ENVIRONMENT_EXPERIMENTAL:
      {
         struct retro_hw_render_callback *cb =
            (struct retro_hw_render_callback*)data;
         struct retro_hw_render_callback *hwr = 
            video_driver_get_hw_context();

         RARCH_LOG("Environ SET_HW_RENDER.\n");

         if (!dynamic_request_hw_context(cb->context_type, cb->version_minor, cb->version_major))
            return false;

         if (!dynamic_verify_hw_context(cb->context_type, cb->version_minor, cb->version_major))
            return false;

         cb->get_current_framebuffer = video_driver_get_current_framebuffer;
         cb->get_proc_address        = video_driver_get_proc_address;

         /* Old ABI. Don't copy garbage. */
         if (cmd & RETRO_ENVIRONMENT_EXPERIMENTAL) 
            memcpy(hwr,
                  cb, offsetof(struct retro_hw_render_callback, stencil));
         else
            memcpy(hwr, cb, sizeof(*cb));
         break;
      }

      case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
      {
         bool state = *(const bool*)data;
         RARCH_LOG("Environ SET_SUPPORT_NO_GAME: %s.\n", state ? "yes" : "no");

         if (state)
            content_set_does_not_need_content();
         else
            content_unset_does_not_need_content();
         break;
      }

      case RETRO_ENVIRONMENT_GET_LIBRETRO_PATH:
      {
         const char **path = (const char**)data;
#ifdef HAVE_DYNAMIC
         *path = path_get(RARCH_PATH_CORE);
#else
         *path = NULL;
#endif
         break;
      }

      case RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK:
#ifdef HAVE_THREADS
      {
         RARCH_LOG("Environ SET_AUDIO_CALLBACK.\n");
#ifdef HAVE_NETWORKING
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
            return false;
#endif
         if (recording_driver_get_data_ptr()) /* A/V sync is a must. */
            return false;
         audio_driver_set_callback(data);
      }
#endif
      break;

      case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK:
      {
         RARCH_LOG("Environ SET_FRAME_TIME_CALLBACK.\n");
         rarch_ctl(RARCH_CTL_SET_FRAME_TIME, data);
         break;
      }

      case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
      {
         struct retro_rumble_interface *iface =
            (struct retro_rumble_interface*)data;

         RARCH_LOG("Environ GET_RUMBLE_INTERFACE.\n");
         iface->set_rumble_state = input_driver_set_rumble_state;
         break;
      }

      case RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES:
      {
         uint64_t *mask = (uint64_t*)data;

         RARCH_LOG("Environ GET_INPUT_DEVICE_CAPABILITIES.\n");
         if (input_driver_has_capabilities())
            *mask = input_driver_get_capabilities();
         else
            return false;
         break;
      }

      case RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE:
      {
         struct retro_sensor_interface *iface =
            (struct retro_sensor_interface*)data;

         RARCH_LOG("Environ GET_SENSOR_INTERFACE.\n");
         iface->set_sensor_state = input_sensor_set_state;
         iface->get_sensor_input = input_sensor_get_input;
         break;
      }
      case RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE:
      {
         struct retro_camera_callback *cb =
            (struct retro_camera_callback*)data;

         RARCH_LOG("Environ GET_CAMERA_INTERFACE.\n");
         cb->start                        = driver_camera_start;
         cb->stop                         = driver_camera_stop;

         camera_driver_ctl(RARCH_CAMERA_CTL_SET_CB, cb);

         if (cb->caps != 0)
            camera_driver_ctl(RARCH_CAMERA_CTL_SET_ACTIVE, NULL);
         else
            camera_driver_ctl(RARCH_CAMERA_CTL_UNSET_ACTIVE, NULL);
         break;
      }

      case RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE:
      {
         struct retro_location_callback *cb =
            (struct retro_location_callback*)data;

         RARCH_LOG("Environ GET_LOCATION_INTERFACE.\n");
         cb->start                 = driver_location_start;
         cb->stop                  = driver_location_stop;
         cb->get_position          = driver_location_get_position;
         cb->set_interval          = driver_location_set_interval;

         if (system)
            system->location_cb    = *cb;

         location_driver_ctl(RARCH_LOCATION_CTL_UNSET_ACTIVE, NULL);
         break;
      }

      case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
      {
         struct retro_log_callback *cb = (struct retro_log_callback*)data;

         RARCH_LOG("Environ GET_LOG_INTERFACE.\n");
         cb->log = rarch_log_libretro;
         break;
      }

      case RETRO_ENVIRONMENT_GET_PERF_INTERFACE:
      {
         struct retro_perf_callback *cb = (struct retro_perf_callback*)data;

         RARCH_LOG("Environ GET_PERF_INTERFACE.\n");
         cb->get_time_usec    = cpu_features_get_time_usec;
         cb->get_cpu_features = cpu_features_get;
         cb->get_perf_counter = cpu_features_get_perf_counter;

         cb->perf_register    = performance_counter_register; 
         cb->perf_start       = core_performance_counter_start;
         cb->perf_stop        = core_performance_counter_stop;
         cb->perf_log         = retro_perf_log;
         break;
      }

      case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
      {
         const char **dir = (const char**)data;

         *dir = *settings->paths.directory_core_assets ?
            settings->paths.directory_core_assets : NULL;
         RARCH_LOG("Environ CORE_ASSETS_DIRECTORY: \"%s\".\n",
               settings->paths.directory_core_assets);
         break;
      }

      case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
      {
         RARCH_LOG("Environ SET_SYSTEM_AV_INFO.\n");
         return driver_ctl(RARCH_DRIVER_CTL_UPDATE_SYSTEM_AV_INFO,
               (void**)&data);
      }

      case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
      {
         unsigned i, j;
         const struct retro_subsystem_info *info =
            (const struct retro_subsystem_info*)data;

         RARCH_LOG("Environ SET_SUBSYSTEM_INFO.\n");

         for (i = 0; info[i].ident; i++)
         {
            RARCH_LOG("Special game type: %s\n", info[i].desc);
            RARCH_LOG("  Ident: %s\n", info[i].ident);
            RARCH_LOG("  ID: %u\n", info[i].id);
            RARCH_LOG("  Content:\n");
            for (j = 0; j < info[i].num_roms; j++)
            {
               RARCH_LOG("    %s (%s)\n",
                     info[i].roms[j].desc, info[i].roms[j].required ?
                     "required" : "optional");
            }
         }

         if (system)
         {
            struct retro_subsystem_info *info_ptr = NULL;
            free(system->subsystem.data);
            system->subsystem.data = NULL;

            info_ptr = (struct retro_subsystem_info*)
               calloc(i, sizeof(*info_ptr));

            if (!info_ptr)
               return false;

            system->subsystem.data = info_ptr;

            memcpy(system->subsystem.data, info,
                  i * sizeof(*system->subsystem.data));
            system->subsystem.size = i;
         }
         break;
      }

      case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
      {
         unsigned i, j;
         const struct retro_controller_info *info =
            (const struct retro_controller_info*)data;

         RARCH_LOG("Environ SET_CONTROLLER_INFO.\n");

         for (i = 0; info[i].types; i++)
         {
            RARCH_LOG("Controller port: %u\n", i + 1);
            for (j = 0; j < info[i].num_types; j++)
               RARCH_LOG("   %s (ID: %u)\n", info[i].types[j].desc,
                     info[i].types[j].id);
         }

         if (system)
         {
            struct retro_controller_info *info_ptr = NULL;

            free(system->ports.data);
            system->ports.data = NULL;

            info_ptr = (struct retro_controller_info*)calloc(i, sizeof(*info_ptr));
            if (!info_ptr)
               return false;

            system->ports.data = info_ptr;
            memcpy(system->ports.data, info,
                  i * sizeof(*system->ports.data));
            system->ports.size = i;
         }
         break;
      }
      
      case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
      {
         if (system)
         {
            unsigned i;
            const struct retro_memory_map *mmaps        =
               (const struct retro_memory_map*)data;
            rarch_memory_descriptor_t *descriptors = NULL;

            RARCH_LOG("Environ SET_MEMORY_MAPS.\n");
            free((void*)system->mmaps.descriptors);
            system->mmaps.num_descriptors = 0;
            descriptors = (rarch_memory_descriptor_t*)
               calloc(mmaps->num_descriptors,
                     sizeof(*descriptors));

            if (!descriptors)
               return false;

            system->mmaps.descriptors     = descriptors;
            system->mmaps.num_descriptors = mmaps->num_descriptors;

            for (i = 0; i < mmaps->num_descriptors; i++)
               system->mmaps.descriptors[i].core = mmaps->descriptors[i];

            mmap_preprocess_descriptors(descriptors, mmaps->num_descriptors);

            if (sizeof(void *) == 8)
               RARCH_LOG("   ndx flags  ptr              offset   start    select   disconn  len      addrspace\n");
            else
               RARCH_LOG("   ndx flags  ptr          offset   start    select   disconn  len      addrspace\n");

            for (i = 0; i < system->mmaps.num_descriptors; i++)
            {
               const rarch_memory_descriptor_t *desc =
                  &system->mmaps.descriptors[i];
               char flags[7];

               flags[0] = 'M';
               if ((desc->core.flags & RETRO_MEMDESC_MINSIZE_8) == RETRO_MEMDESC_MINSIZE_8)
                  flags[1] = '8';
               else if ((desc->core.flags & RETRO_MEMDESC_MINSIZE_4) == RETRO_MEMDESC_MINSIZE_4)
                  flags[1] = '4';
               else if ((desc->core.flags & RETRO_MEMDESC_MINSIZE_2) == RETRO_MEMDESC_MINSIZE_2)
                  flags[1] = '2';
               else
                  flags[1] = '1';

               flags[2] = 'A';
               if ((desc->core.flags & RETRO_MEMDESC_ALIGN_8) == RETRO_MEMDESC_ALIGN_8)
                  flags[3] = '8';
               else if ((desc->core.flags & RETRO_MEMDESC_ALIGN_4) == RETRO_MEMDESC_ALIGN_4)
                  flags[3] = '4';
               else if ((desc->core.flags & RETRO_MEMDESC_ALIGN_2) == RETRO_MEMDESC_ALIGN_2)
                  flags[3] = '2';
               else
                  flags[3] = '1';

               flags[4] = (desc->core.flags & RETRO_MEMDESC_BIGENDIAN) ? 'B' : 'b';
               flags[5] = (desc->core.flags & RETRO_MEMDESC_CONST) ? 'C' : 'c';
               flags[6] = 0;

               RARCH_LOG("   %03u %s %p %08X %08X %08X %08X %08X %s\n",
                     i + 1, flags, desc->core.ptr, desc->core.offset, desc->core.start,
                     desc->core.select, desc->core.disconnect, desc->core.len,
                     desc->core.addrspace ? desc->core.addrspace : "");
            }
         }
         else
         {
            RARCH_WARN("Environ SET_MEMORY_MAPS, but system pointer not initialized..\n");
         }

         break;
      }

      case RETRO_ENVIRONMENT_SET_GEOMETRY:
      {
         const struct retro_game_geometry *in_geom = NULL;
         struct retro_game_geometry *geom = NULL;
         struct retro_system_av_info *av_info = 
            video_viewport_get_system_av_info();
         
         if (av_info)
            geom = (struct retro_game_geometry*)&av_info->geometry;

         if (!geom)
            return false;

         in_geom = (const struct retro_game_geometry*)data;

         RARCH_LOG("Environ SET_GEOMETRY.\n");

         /* Can potentially be called every frame,
          * don't do anything unless required. */
         if (  (geom->base_width   != in_geom->base_width)  ||
               (geom->base_height  != in_geom->base_height) ||
               (geom->aspect_ratio != in_geom->aspect_ratio))
         {
            geom->base_width   = in_geom->base_width;
            geom->base_height  = in_geom->base_height;
            geom->aspect_ratio = in_geom->aspect_ratio;

            RARCH_LOG("SET_GEOMETRY: %ux%u, aspect: %.3f.\n",
                  geom->base_width, geom->base_height, geom->aspect_ratio);

            /* Forces recomputation of aspect ratios if
             * using core-dependent aspect ratios. */
            command_event(CMD_EVENT_VIDEO_SET_ASPECT_RATIO, NULL);

            /* TODO: Figure out what to do, if anything, with recording. */
         }
         break;
      }

      case RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER:
         return video_driver_get_current_software_framebuffer(
               (struct retro_framebuffer*)data);

      case RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE:
         return video_driver_get_hw_render_interface(
               (const struct retro_hw_render_interface**)data);
      
      case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
#ifdef HAVE_CHEEVOS
         {
            bool state = *(const bool*)data;
            RARCH_LOG("Environ SET_SUPPORT_ACHIEVEMENTS: %s.\n", state ? "yes" : "no");
            cheevos_set_support_cheevos(state);
         }
#endif
         break;

      case RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE:
      {
         const struct retro_hw_render_context_negotiation_interface *iface =
            (const struct retro_hw_render_context_negotiation_interface*)data;
         RARCH_LOG("Environ SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE.\n");
         video_driver_set_context_negotiation_interface(iface);
         break;
      }

      case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS:
      {
         uint64_t *quirks = (uint64_t *) data;
         core_set_serialization_quirks(*quirks);
         break;
      }

      case RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT:
      {
         core_set_shared_context = true;
         break;
      }

      /* Default */
      default:
         RARCH_LOG("Environ UNSUPPORTED (#%u).\n", cmd);
         return false;
   }

   return true;
}
