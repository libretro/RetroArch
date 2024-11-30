/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2023 - Daniel De Matteis
 *  Copyright (C) 2018-2023 - Dan Weiss
 *  Copyright (C) 2022-2023 - Neil Fore
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

#include <stdint.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <encodings/utf.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <time/rtime.h>

#include "content.h"
#include "core.h"
#include "dynamic.h"
#include "driver.h"
#include "audio/audio_driver.h"
#include "gfx/video_driver.h"
#include "paths.h"
#include "runloop.h"
#include "verbosity.h"

static int16_t input_state_get_last(unsigned port,
      unsigned device, unsigned index, unsigned id)
{
   runloop_state_t      *runloop_st = runloop_state_get_ptr();

   if (runloop_st->input_state_list)
   {
      int i;
      /* find list item */
      for (i = 0; i < runloop_st->input_state_list->size; i++)
      {
         input_list_element *element =
            (input_list_element*)runloop_st->input_state_list->data[i];

         if (     (element->port   == port)
               && (element->device == device)
               && (element->index  == index))
         {
            if (id < element->state_size)
               return element->state[id];
            break;
         }
      }
   }

   return 0;
}

static void free_retro_ctx_load_content_info(struct
      retro_ctx_load_content_info *dest)
{
   if (!dest)
      return;

   string_list_free((struct string_list*)dest->content);
   if (dest->info)
      free(dest->info);

   dest->info    = NULL;
   dest->content = NULL;
}

static struct retro_game_info* clone_retro_game_info(const
      struct retro_game_info *src)
{
   struct retro_game_info *dest = (struct retro_game_info*)malloc(
         sizeof(struct retro_game_info));

   if (!dest)
      return NULL;

   /* content_file_init() guarantees that all
    * elements of the source retro_game_info
    * struct will persist for the lifetime of
    * the core. This means we do not have to
    * copy any data; pointer assignment is
    * sufficient */
   dest->path = src->path;
   dest->data = src->data;
   dest->size = src->size;
   dest->meta = src->meta;

   return dest;
}

static struct retro_ctx_load_content_info
*clone_retro_ctx_load_content_info(
      const struct retro_ctx_load_content_info *src)
{
   struct retro_ctx_load_content_info *dest = NULL;
   if (!src || src->special)
      return NULL;   /* refuse to deal with the Special field */

   dest          = (struct retro_ctx_load_content_info*)
      malloc(sizeof(*dest));

   if (!dest)
      return NULL;

   dest->info       = NULL;
   dest->content    = NULL;
   dest->special    = NULL;

   if (src->info)
      dest->info    = clone_retro_game_info(src->info);
   if (src->content)
      dest->content = string_list_clone(src->content);

   return dest;
}

void runahead_set_load_content_info(void *data,
      const retro_ctx_load_content_info_t *ctx)
{
   runloop_state_t *runloop_st = (runloop_state_t*)data;
   free_retro_ctx_load_content_info(runloop_st->load_content_info);
   free(runloop_st->load_content_info);
   runloop_st->load_content_info = clone_retro_ctx_load_content_info(ctx);
}

/* RUNAHEAD - SECONDARY CORE  */
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
static void strcat_alloc(char **dst, const char *s)
{
   size_t len1;
   char *src          = *dst;

   if (!src)
   {
      if (s)
      {
         size_t   len = strlen(s);
         if (len != 0)
         {
            char *_dst= (char*)malloc(len + 1);
            strcpy_literal(_dst, s);
            src       = _dst;
         }
         else
            src       = NULL;
      }
      else
         src          = (char*)calloc(1,1);

      *dst            = src;
      return;
   }

   if (!s)
      return;

   len1               = strlen(src);

   if (!(src = (char*)realloc(src, len1 + strlen(s) + 1)))
      return;

   *dst               = src;
   strcpy_literal(src + len1, s);
}

void runahead_secondary_core_destroy(void *data)
{
   runloop_state_t *runloop_st      = (runloop_state_t*)data;
   if (!runloop_st->secondary_lib_handle)
      return;

   /* unload game from core */
   if (runloop_st->secondary_core.retro_unload_game)
      runloop_st->secondary_core.retro_unload_game();
   runloop_st->core_poll_type_override = POLL_TYPE_OVERRIDE_DONTCARE;

   /* deinit */
   if (runloop_st->secondary_core.retro_deinit)
      runloop_st->secondary_core.retro_deinit();
   memset(&runloop_st->secondary_core, 0, sizeof(struct retro_core_t));

   dylib_close(runloop_st->secondary_lib_handle);
   runloop_st->secondary_lib_handle = NULL;
   filestream_delete(runloop_st->secondary_library_path);
   if (runloop_st->secondary_library_path)
      free(runloop_st->secondary_library_path);
   runloop_st->secondary_library_path = NULL;
}

static char *get_tmpdir_alloc(const char *override_dir)
{
   const char *src    = NULL;
   char *path         = NULL;
#ifdef _WIN32
#ifdef LEGACY_WIN32
   DWORD plen         = GetTempPath(0, NULL) + 1;

   if (!(path = (char*)malloc(plen * sizeof(char))))
      return NULL;

   path[plen - 1]     = 0;
   GetTempPath(plen, path);
#else
   DWORD plen         = GetTempPathW(0, NULL) + 1;
   wchar_t *wide_str  = (wchar_t*)malloc(plen * sizeof(wchar_t));

   if (!wide_str)
      return NULL;

   wide_str[plen - 1] = 0;
   GetTempPathW(plen, wide_str);

   path               = utf16_to_utf8_string_alloc(wide_str);
   free(wide_str);
#endif
#else
#if defined ANDROID
   src                = override_dir;
#else
   {
      char *tmpdir    = getenv("TMPDIR");
      if (tmpdir)
         src          = tmpdir;
      else
         src          = "/tmp";
   }
#endif
   if (src)
   {
      size_t   len    = strlen(src);
      if (len != 0)
      {
         char *dst    = (char*)malloc(len + 1);
         strcpy_literal(dst, src);
         path         = dst;
      }
   }
   else
      path            = (char*)calloc(1,1);
#endif
   return path;
}

static bool write_file_with_random_name(char **temp_dll_path,
      const char *tmp_path, const void* data, ssize_t dataSize)
{
   int i;
   size_t ext_len;
   char number_buf[32];
   bool okay                = false;
   const char *prefix       = "tmp";
   char *ext                = NULL;
   time_t time_value        = time(NULL);
   unsigned _number_value   = (unsigned)time_value;
   const char *src          = path_get_extension(*temp_dll_path);

   if (src)
   {
      size_t   len          = strlen(src);
      if (len != 0)
      {
         char *dst          = (char*)malloc(len + 1);
         strcpy_literal(dst, src);
         ext                = dst;
      }
   }
   else
      ext                   = (char*)calloc(1,1);

   ext_len                  = strlen(ext);

   if (ext_len > 0)
   {
      strcat_alloc(&ext, ".");
      memmove(ext + 1, ext, ext_len);
      ext[0] = '.';
      ext_len++;
   }

   /* Try up to 30 'random' filenames before giving up */
   for (i = 0; i < 30; i++)
   {
      int number_value = _number_value * 214013 + 2531011;
      int number       = (number_value >> 14) % 100000;

      snprintf(number_buf, sizeof(number_buf), "%05d", number);

      if (*temp_dll_path)
         free(*temp_dll_path);
      *temp_dll_path = NULL;

      strcat_alloc(temp_dll_path, tmp_path);
      strcat_alloc(temp_dll_path, PATH_DEFAULT_SLASH());
      strcat_alloc(temp_dll_path, prefix);
      strcat_alloc(temp_dll_path, number_buf);
      strcat_alloc(temp_dll_path, ext);

      if (filestream_write_file(*temp_dll_path, data, dataSize))
      {
         okay = true;
         break;
      }
   }

   if (ext)
      free(ext);
   ext = NULL;
   return okay;
}


static char *copy_core_to_temp_file(
      const char *core_path,
      const char *dir_libretro)
{
   char tmp_path[PATH_MAX_LENGTH];
   bool  failed                = false;
   char  *tmpdir               = NULL;
   char  *tmp_dll_path         = NULL;
   void  *dll_file_data        = NULL;
   int64_t  dll_file_size      = 0;
   const char  *core_base_name = path_basename_nocompression(core_path);

   if (string_is_empty(core_base_name))
      return NULL;

   if (!(tmpdir = get_tmpdir_alloc(dir_libretro)))
      return NULL;

   fill_pathname_join_special(tmp_path,
         tmpdir, "retroarch_temp",
         sizeof(tmp_path));

   if (!path_mkdir(tmp_path))
   {
      failed = true;
      goto end;
   }

   if (!filestream_read_file(core_path, &dll_file_data, &dll_file_size))
   {
      failed = true;
      goto end;
   }

   strcat_alloc(&tmp_dll_path, tmp_path);
   strcat_alloc(&tmp_dll_path, PATH_DEFAULT_SLASH());
   strcat_alloc(&tmp_dll_path, core_base_name);

   if (!filestream_write_file(tmp_dll_path, dll_file_data, dll_file_size))
   {
      /* try other file names */
      if (!write_file_with_random_name(&tmp_dll_path,
               tmp_path, dll_file_data, dll_file_size))
         failed = true;
   }

end:
   if (tmpdir)
      free(tmpdir);
   if (dll_file_data)
      free(dll_file_data);

   tmpdir              = NULL;
   dll_file_data       = NULL;

   if (!failed)
      return tmp_dll_path;

   if (tmp_dll_path)
      free(tmp_dll_path);

   tmp_dll_path     = NULL;

   return NULL;
}

static bool runloop_environment_secondary_core_hook(
      unsigned cmd, void *data)
{
   runloop_state_t *runloop_st    = runloop_state_get_ptr();
   bool result                    = runloop_environment_cb(cmd, data);

   if (runloop_st->flags & RUNLOOP_FLAG_HAS_VARIABLE_UPDATE)
   {
      if (cmd == RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE)
      {
         bool *bool_p                      = (bool*)data;
         *bool_p                           = true;
         runloop_st->flags                &= ~RUNLOOP_FLAG_HAS_VARIABLE_UPDATE;
         return true;
      }
      else if (cmd == RETRO_ENVIRONMENT_GET_VARIABLE)
         runloop_st->flags &= ~RUNLOOP_FLAG_HAS_VARIABLE_UPDATE;
   }
   return result;
}

void runahead_clear_controller_port_map(void *data)
{
   int port;
   runloop_state_t *runloop_st = (runloop_state_t*)data;
   for (port = 0; port < MAX_USERS; port++)
      runloop_st->port_map[port] = -1;
}

static bool secondary_core_create(runloop_state_t *runloop_st,
      settings_t *settings)
{
   const enum rarch_core_type
      last_core_type             = runloop_st->last_core_type;
   rarch_system_info_t *sys_info = &runloop_st->system;
   unsigned num_active_users     = settings->uints.input_max_users;
   uint8_t flags                 = content_get_flags();

   if (     (last_core_type != CORE_TYPE_PLAIN)
         || (!runloop_st->load_content_info)
         || ( runloop_st->load_content_info->special))
      return false;

   if (runloop_st->secondary_library_path)
      free(runloop_st->secondary_library_path);
   runloop_st->secondary_library_path = NULL;
   runloop_st->secondary_library_path = copy_core_to_temp_file(
		   path_get(RARCH_PATH_CORE),
		   settings->paths.directory_libretro);

   if (!runloop_st->secondary_library_path)
      return false;

   /* Load Core */
   if (!runloop_init_libretro_symbols(runloop_st,
            CORE_TYPE_PLAIN, &runloop_st->secondary_core,
            runloop_st->secondary_library_path,
            &runloop_st->secondary_lib_handle))
      return false;

   runloop_st->secondary_core.flags |= RETRO_CORE_FLAG_SYMBOLS_INITED;
   runloop_st->secondary_core.retro_set_environment(
         runloop_environment_secondary_core_hook);
   runloop_st->flags                |= RUNLOOP_FLAG_HAS_VARIABLE_UPDATE;

   runloop_st->secondary_core.retro_init();

   if (flags & CONTENT_ST_FLAG_IS_INITED)
      runloop_st->secondary_core.flags |=  RETRO_CORE_FLAG_INITED;
   else
      runloop_st->secondary_core.flags &= ~RETRO_CORE_FLAG_INITED;

   /* Load Content */
   /* disabled due to crashes */
   if (    (!runloop_st->load_content_info)
         || (runloop_st->load_content_info->special))
      return false;

   if ( (   runloop_st->load_content_info->content->size > 0)
         && runloop_st->load_content_info->content->elems[0].data)
   {
      if (!runloop_st->secondary_core.retro_load_game(
               runloop_st->load_content_info->info))
      {
         runloop_st->secondary_core.flags &= ~RETRO_CORE_FLAG_GAME_LOADED;
         goto error;
      }
      runloop_st->secondary_core.flags    |=  RETRO_CORE_FLAG_GAME_LOADED;
   }
   else if (flags & CONTENT_ST_FLAG_CORE_DOES_NOT_NEED_CONTENT)
   {
      if (!runloop_st->secondary_core.retro_load_game(NULL))
      {
         runloop_st->secondary_core.flags &= ~RETRO_CORE_FLAG_GAME_LOADED;
         goto error;
      }
      runloop_st->secondary_core.flags    |=  RETRO_CORE_FLAG_GAME_LOADED;
   }
   else
      runloop_st->secondary_core.flags    &= ~RETRO_CORE_FLAG_GAME_LOADED;

   if (!(runloop_st->secondary_core.flags & RETRO_CORE_FLAG_INITED))
      goto error;

   core_set_default_callbacks(&runloop_st->secondary_callbacks);
   runloop_st->secondary_core.retro_set_video_refresh(
         runloop_st->secondary_callbacks.frame_cb);
   runloop_st->secondary_core.retro_set_audio_sample(
         runloop_st->secondary_callbacks.sample_cb);
   runloop_st->secondary_core.retro_set_audio_sample_batch(
         runloop_st->secondary_callbacks.sample_batch_cb);
   runloop_st->secondary_core.retro_set_input_state(
         runloop_st->secondary_callbacks.state_cb);
   runloop_st->secondary_core.retro_set_input_poll(
         runloop_st->secondary_callbacks.poll_cb);

   if (sys_info)
   {
      ssize_t port;
      for (port = 0; port < MAX_USERS; port++)
      {
         if (port < (ssize_t)sys_info->ports.size)
         {
            unsigned device = (port < (ssize_t)num_active_users)
                  ? runloop_st->port_map[port]
                  : RETRO_DEVICE_NONE;
            runloop_st->secondary_core.retro_set_controller_port_device(
                  (unsigned)port, device);
         }
      }
   }

#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
   runahead_clear_controller_port_map(runloop_st);
#endif

   return true;

error:
   runahead_secondary_core_destroy(runloop_st);
   return false;
}

#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
bool secondary_core_ensure_exists(void *data, settings_t *settings)
{
   runloop_state_t *runloop_st   = (runloop_state_t*)data;
   if (!runloop_st->secondary_lib_handle)
      if (!secondary_core_create(runloop_st, settings))
         return false;
   return true;
}
#endif

#if defined(HAVE_DYNAMIC)
static bool secondary_core_deserialize(runloop_state_t *runloop_st,
      settings_t *settings,
      const void *data, size_t size)
{
   bool ret = false;

   if (secondary_core_ensure_exists(runloop_st, settings))
   {
      runloop_st->flags |=  RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE;
      ret                = runloop_st->secondary_core.retro_unserialize(data, size);
      runloop_st->flags &= ~RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE;
   }
   else
      runahead_secondary_core_destroy(runloop_st);

   return ret;
}
#endif

static void secondary_core_input_poll_null(void) { }

static bool secondary_core_run_use_last_input(runloop_state_t *runloop_st)
{
   retro_input_poll_t old_poll_function;
   retro_input_state_t old_input_function;

   if (!secondary_core_ensure_exists(runloop_st, config_get_ptr()))
   {
      runahead_secondary_core_destroy(runloop_st);
      return false;
   }

   old_poll_function                        = runloop_st->secondary_callbacks.poll_cb;
   old_input_function                       = runloop_st->secondary_callbacks.state_cb;

   runloop_st->secondary_callbacks.poll_cb  = secondary_core_input_poll_null;
   runloop_st->secondary_callbacks.state_cb = input_state_get_last;

   runloop_st->secondary_core.retro_set_input_poll(
         runloop_st->secondary_callbacks.poll_cb);
   runloop_st->secondary_core.retro_set_input_state(
         runloop_st->secondary_callbacks.state_cb);

   runloop_st->secondary_core.retro_run();
   runloop_st->secondary_callbacks.poll_cb  = old_poll_function;
   runloop_st->secondary_callbacks.state_cb = old_input_function;

   runloop_st->secondary_core.retro_set_input_poll(
         runloop_st->secondary_callbacks.poll_cb);
   runloop_st->secondary_core.retro_set_input_state(
         runloop_st->secondary_callbacks.state_cb);

   return true;
}

void runahead_remember_controller_port_device(void *data,
		long port, long device)
{
   runloop_state_t *runloop_st   = (runloop_state_t*)data;
   if (port >= 0 && port < MAX_USERS)
      runloop_st->port_map[port] = (int)device;
   if (     runloop_st->secondary_lib_handle
         && runloop_st->secondary_core.retro_set_controller_port_device)
      runloop_st->secondary_core.retro_set_controller_port_device((unsigned)port, (unsigned)device);
}

#else
void runahead_secondary_core_destroy(void *data) { }
#endif

static void mylist_resize(my_list *list,
      int new_size, bool run_constructor)
{
   int i;
   int new_capacity;
   int old_size;
   void *element    = NULL;
   if (new_size < 0)
      new_size      = 0;
   new_capacity     = new_size;
   old_size         = list->size;

   if (new_size == old_size)
      return;

   if (new_size > list->capacity)
   {
      if (new_capacity < list->capacity * 2)
         new_capacity = list->capacity * 2;

      /* try to realloc */
      list->data      = (void**)realloc(
            (void*)list->data, new_capacity * sizeof(void*));

      for (i = list->capacity; i < new_capacity; i++)
         list->data[i] = NULL;

      list->capacity = new_capacity;
   }

   if (new_size <= list->size)
   {
      for (i = new_size; i < list->size; i++)
      {
         element = list->data[i];

         if (element)
         {
            list->destructor(element);
            list->data[i] = NULL;
         }
      }
   }
   else
   {
      for (i = list->size; i < new_size; i++)
      {
         list->data[i] = NULL;
         if (run_constructor)
            list->data[i] = list->constructor();
      }
   }

   list->size = new_size;
}

static void *mylist_add_element(my_list *list)
{
   int old_size = list->size;
   if (list)
      mylist_resize(list, old_size + 1, true);
   return list->data[old_size];
}

static void mylist_destroy(my_list **list_p)
{
   my_list *list = NULL;
   if (!list_p)
      return;

   list = *list_p;

   if (list)
   {
      mylist_resize(list, 0, false);
      free(list->data);
      free(list);
      *list_p = NULL;
   }
}

static void mylist_create(my_list **list_p, int initial_capacity,
      constructor_t constructor, destructor_t destructor)
{
   my_list *list        = NULL;

   if (!list_p)
      return;

   list                = *list_p;
   if (list)
      mylist_destroy(list_p);

   list               = (my_list*)malloc(sizeof(my_list));
   *list_p            = list;
   list->size         = 0;
   list->constructor  = constructor;
   list->destructor   = destructor;
   list->data         = (void**)calloc(initial_capacity, sizeof(void*));
   list->capacity     = initial_capacity;
}

static void *input_list_element_constructor(void)
{
   void *ptr                   = malloc(sizeof(input_list_element));
   input_list_element *element = (input_list_element*)ptr;

   element->port               = 0;
   element->device             = 0;
   element->index              = 0;
   element->state              = (int16_t*)calloc(NAME_MAX_LENGTH,
         sizeof(int16_t));
   element->state_size         = NAME_MAX_LENGTH;

   return ptr;
}

static void input_list_element_realloc(
      input_list_element *element,
      unsigned int new_size)
{
   if (new_size > element->state_size)
   {
      element->state = (int16_t*)realloc(element->state,
            new_size * sizeof(int16_t));
      memset(&element->state[element->state_size], 0,
            (new_size - element->state_size) * sizeof(int16_t));
      element->state_size = new_size;
   }
}

static void input_list_element_expand(
      input_list_element *element, unsigned int new_index)
{
   unsigned int new_size = element->state_size;
   if (new_size == 0)
      new_size = 32;
   while (new_index >= new_size)
      new_size *= 2;
   input_list_element_realloc(element, new_size);
}

static void input_list_element_destructor(void* element_ptr)
{
   input_list_element *element = (input_list_element*)element_ptr;
   if (!element)
      return;

   free(element->state);
   free(element_ptr);
}

static void runahead_input_state_set_last(
      runloop_state_t *runloop_st,
      unsigned port, unsigned device,
      unsigned index, unsigned id, int16_t value)
{
   unsigned i;
   input_list_element *element = NULL;

   if (!runloop_st->input_state_list)
      mylist_create(&runloop_st->input_state_list, 16,
            input_list_element_constructor,
            input_list_element_destructor);

   /* Find list item */
   for (i = 0; i < (unsigned)runloop_st->input_state_list->size; i++)
   {
      element = (input_list_element*)runloop_st->input_state_list->data[i];
      if (  (element->port   == port)   &&
            (element->device == device) &&
            (element->index  == index)
         )
      {
         if (id >= element->state_size)
            input_list_element_expand(element, id);
         element->state[id] = value;
         return;
      }
   }

   element               = NULL;
   if (runloop_st->input_state_list)
      element            = (input_list_element*)
         mylist_add_element(runloop_st->input_state_list);
   if (element)
   {
      element->port         = port;
      element->device       = device;
      element->index        = index;
      if (id >= element->state_size)
         input_list_element_expand(element, id);
      element->state[id]    = value;
   }
}

static int16_t runahead_input_state_with_logging(unsigned port,
      unsigned device, unsigned index, unsigned id)
{
   runloop_state_t     *runloop_st  = runloop_state_get_ptr();

   if (runloop_st->input_state_callback_original)
   {
      int16_t result                =
         runloop_st->input_state_callback_original(
            port, device, index, id);
      int16_t last_input            =
         input_state_get_last(port, device, index, id);
      if (result != last_input)
         runloop_st->flags         |= RUNLOOP_FLAG_INPUT_IS_DIRTY;
      /*arbitrary limit of up to 65536 elements in state array*/
      if (id < 65536)
         runahead_input_state_set_last(runloop_st, port, device, index, id, result);
      return result;
   }
   return 0;
}

static void runahead_reset_hook(void)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   runloop_st->flags          |= RUNLOOP_FLAG_INPUT_IS_DIRTY;
   if (runloop_st->retro_reset_callback_original)
      runloop_st->retro_reset_callback_original();
}

static bool runahead_unserialize_hook(const void *buf, size_t size)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   runloop_st->flags          |= RUNLOOP_FLAG_INPUT_IS_DIRTY;
   if (runloop_st->retro_unserialize_callback_original)
      return runloop_st->retro_unserialize_callback_original(buf, size);
   return false;
}

static void runahead_add_input_state_hook(runloop_state_t *runloop_st)
{
   struct retro_callbacks *cbs      = &runloop_st->retro_ctx;

   if (!runloop_st->input_state_callback_original)
   {
      runloop_st->input_state_callback_original = cbs->state_cb;
      cbs->state_cb                             = runahead_input_state_with_logging;
      runloop_st->current_core.retro_set_input_state(cbs->state_cb);
   }

   if (!runloop_st->retro_reset_callback_original)
   {
      runloop_st->retro_reset_callback_original
         = runloop_st->current_core.retro_reset;
      runloop_st->current_core.retro_reset   = runahead_reset_hook;
   }

   if (!runloop_st->retro_unserialize_callback_original)
   {
      runloop_st->retro_unserialize_callback_original = runloop_st->current_core.retro_unserialize;
      runloop_st->current_core.retro_unserialize      = runahead_unserialize_hook;
   }
}

static void runahead_remove_input_state_hook(runloop_state_t *runloop_st)
{
   struct retro_callbacks *cbs      = &runloop_st->retro_ctx;

   if (runloop_st->input_state_callback_original)
   {
      cbs->state_cb                             =
         runloop_st->input_state_callback_original;
      runloop_st->current_core.retro_set_input_state(cbs->state_cb);
      runloop_st->input_state_callback_original = NULL;
      mylist_destroy(&runloop_st->input_state_list);
   }

   if (runloop_st->retro_reset_callback_original)
   {
      runloop_st->current_core.retro_reset               =
         runloop_st->retro_reset_callback_original;
      runloop_st->retro_reset_callback_original          = NULL;
   }

   if (runloop_st->retro_unserialize_callback_original)
   {
      runloop_st->current_core.retro_unserialize                =
         runloop_st->retro_unserialize_callback_original;
      runloop_st->retro_unserialize_callback_original           = NULL;
   }
}

static void *runahead_save_state_alloc(void)
{
   runloop_state_t     *runloop_st       = runloop_state_get_ptr();
   retro_ctx_serialize_info_t *savestate = (retro_ctx_serialize_info_t*)
      malloc(sizeof(retro_ctx_serialize_info_t));

   if (!savestate)
      return NULL;

   savestate->data          = NULL;
   savestate->data_const    = NULL;
   savestate->size          = 0;

   if (     (runloop_st->runahead_save_state_size > 0)
         && (runloop_st->flags & RUNLOOP_FLAG_RUNAHEAD_SAVE_STATE_SIZE_KNOWN))
   {
      savestate->data       = malloc(runloop_st->runahead_save_state_size);
      savestate->data_const = savestate->data;
      savestate->size       = runloop_st->runahead_save_state_size;
   }

   return savestate;
}

static void runahead_save_state_free(void *data)
{
   retro_ctx_serialize_info_t *savestate = (retro_ctx_serialize_info_t*)data;
   if (!savestate)
      return;
   free(savestate->data);
   free(savestate);
}

static void runahead_save_state_list_init(
      runloop_state_t *runloop_st,
      size_t save_state_size)
{
   runloop_st->runahead_save_state_size  = save_state_size;
   runloop_st->flags                    |= RUNLOOP_FLAG_RUNAHEAD_SAVE_STATE_SIZE_KNOWN;

   mylist_create(&runloop_st->runahead_save_state_list, 16,
         runahead_save_state_alloc, runahead_save_state_free);
}

/* Hooks - Hooks to cleanup, and add dirty input hooks */
static void runahead_remove_hooks(runloop_state_t *runloop_st)
{
   if (runloop_st->original_retro_deinit)
   {
      runloop_st->current_core.retro_deinit =
         runloop_st->original_retro_deinit;
      runloop_st->original_retro_deinit     = NULL;
   }

   if (runloop_st->original_retro_unload)
   {
      runloop_st->current_core.retro_unload_game =
         runloop_st->original_retro_unload;
      runloop_st->original_retro_unload          = NULL;
   }
   runahead_remove_input_state_hook(runloop_st);
}

static void runahead_destroy(runloop_state_t *runloop_st)
{
   mylist_destroy(&runloop_st->runahead_save_state_list);
   runahead_remove_hooks(runloop_st);
   runahead_clear_variables(runloop_st);
}

static void runahead_unload_hook(void)
{
   runloop_state_t     *runloop_st  = runloop_state_get_ptr();

   runahead_remove_hooks(runloop_st);
   runahead_destroy(runloop_st);
   runahead_secondary_core_destroy(runloop_st);
   if (runloop_st->current_core.retro_unload_game)
      runloop_st->current_core.retro_unload_game();
   runloop_st->core_poll_type_override = POLL_TYPE_OVERRIDE_DONTCARE;
}

static void runahead_deinit_hook(void)
{
   runloop_state_t     *runloop_st = runloop_state_get_ptr();

   runahead_remove_hooks(runloop_st);
   runahead_destroy(runloop_st);
   runahead_secondary_core_destroy(runloop_st);
   if (runloop_st->current_core.retro_deinit)
      runloop_st->current_core.retro_deinit();
}

static void runahead_add_hooks(runloop_state_t *runloop_st)
{
   if (!runloop_st->original_retro_deinit)
   {
      runloop_st->original_retro_deinit     =
         runloop_st->current_core.retro_deinit;
      runloop_st->current_core.retro_deinit = runahead_deinit_hook;
   }

   if (!runloop_st->original_retro_unload)
   {
      runloop_st->original_retro_unload          = runloop_st->current_core.retro_unload_game;
      runloop_st->current_core.retro_unload_game = runahead_unload_hook;
   }
   runahead_add_input_state_hook(runloop_st);
}

/* Runahead Code */

static void runahead_error(runloop_state_t *runloop_st)
{
   runloop_st->flags &= ~RUNLOOP_FLAG_RUNAHEAD_AVAILABLE;
   mylist_destroy(&runloop_st->runahead_save_state_list);
   runahead_remove_hooks(runloop_st);
   runloop_st->runahead_save_state_size       = 0;
   runloop_st->flags                         |= RUNLOOP_FLAG_RUNAHEAD_SAVE_STATE_SIZE_KNOWN;
}

static bool runahead_create(runloop_state_t *runloop_st)
{
   /* get savestate size and allocate buffer */
   video_driver_state_t *video_st = video_state_get_ptr();
   size_t info_size               = core_serialize_size_special();

   runahead_save_state_list_init(runloop_st, info_size);
   if (video_st->flags & VIDEO_FLAG_ACTIVE)
      video_st->flags |=  VIDEO_FLAG_RUNAHEAD_IS_ACTIVE;
   else
      video_st->flags &= ~VIDEO_FLAG_RUNAHEAD_IS_ACTIVE;

   if (      (runloop_st->runahead_save_state_size == 0)
         || !(runloop_st->flags & RUNLOOP_FLAG_RUNAHEAD_SAVE_STATE_SIZE_KNOWN))
   {
      runahead_error(runloop_st);
      return false;
   }

   runahead_add_hooks(runloop_st);
   runloop_st->flags |= RUNLOOP_FLAG_RUNAHEAD_FORCE_INPUT_DIRTY;
   if (runloop_st->runahead_save_state_list)
      mylist_resize(runloop_st->runahead_save_state_list, 1, true);
   return true;
}

static bool runahead_save_state(runloop_state_t *runloop_st)
{
   retro_ctx_serialize_info_t *serialize_info;

   if (!runloop_st->runahead_save_state_list)
      return false;

   serialize_info                  =
      (retro_ctx_serialize_info_t*)runloop_st->runahead_save_state_list->data[0];

   if (core_serialize_special(serialize_info))
      return true;

   runahead_error(runloop_st);
   return false;
}

static bool runahead_load_state(runloop_state_t *runloop_st)
{
   retro_ctx_serialize_info_t *serialize_info =
      (retro_ctx_serialize_info_t*)
      runloop_st->runahead_save_state_list->data[0];
   bool last_dirty                            = (runloop_st->flags & RUNLOOP_FLAG_INPUT_IS_DIRTY) ? true : false;
   bool ret                                   = core_unserialize_special(serialize_info);
   if (last_dirty)
      runloop_st->flags                      |=  RUNLOOP_FLAG_INPUT_IS_DIRTY;
   else
      runloop_st->flags                      &= ~RUNLOOP_FLAG_INPUT_IS_DIRTY;

   if (!ret)
      runahead_error(runloop_st);

   return ret;
}

#if HAVE_DYNAMIC
static bool runahead_load_state_secondary(runloop_state_t *runloop_st, settings_t *settings)
{
   retro_ctx_serialize_info_t *serialize_info =
      (retro_ctx_serialize_info_t*)runloop_st->runahead_save_state_list->data[0];

   if (!secondary_core_deserialize(runloop_st,
            settings, serialize_info->data_const,
            serialize_info->size))
   {
      runloop_st->flags &= ~RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE;
      runahead_error(runloop_st);

      return false;
   }

   return true;
}
#endif

static void runahead_core_run_use_last_input(runloop_state_t *runloop_st)
{
   struct retro_callbacks *cbs            = &runloop_st->retro_ctx;
   retro_input_poll_t old_poll_function   = cbs->poll_cb;
   retro_input_state_t old_input_function = cbs->state_cb;

   cbs->poll_cb                           = retro_input_poll_null;
   cbs->state_cb                          = input_state_get_last;

   runloop_st->current_core.retro_set_input_poll(cbs->poll_cb);
   runloop_st->current_core.retro_set_input_state(cbs->state_cb);

   runloop_st->current_core.retro_run();

   cbs->poll_cb                           = old_poll_function;
   cbs->state_cb                          = old_input_function;

   runloop_st->current_core.retro_set_input_poll(cbs->poll_cb);
   runloop_st->current_core.retro_set_input_state(cbs->state_cb);
}

void runahead_run(void *data,
      int runahead_count,
      bool runahead_hide_warnings,
      bool use_secondary)
{
   runloop_state_t *runloop_st = (runloop_state_t*)data;
   int frame_number        = 0;
   bool last_frame         = false;
   bool suspended_frame    = false;
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
   const bool have_dynamic = true;
   settings_t *settings    = config_get_ptr();
#else
   const bool have_dynamic = false;
#endif
   video_driver_state_t
      *video_st            = video_state_get_ptr();
   uint64_t frame_count    = video_st->frame_count;
   audio_driver_state_t
      *audio_st            = audio_state_get_ptr();

   if (      runahead_count <= 0
         || !(runloop_st->flags & RUNLOOP_FLAG_RUNAHEAD_AVAILABLE))
      goto force_input_dirty;

   if (!(runloop_st->flags & RUNLOOP_FLAG_RUNAHEAD_SAVE_STATE_SIZE_KNOWN))
   {
      /* Disable runahead if current core reports
       * that it has an insufficient savestate
       * support level */
      if (!core_info_current_supports_runahead())
      {
         runahead_error(runloop_st);
         /* If core is incompatible with runahead,
          * log a warning but do not spam OSD messages.
          * Runahead menu entries are hidden when using
          * incompatible cores, so there is no mechanism
          * for users to respond to notifications. In
          * addition, auto-disabling runahead is a feature,
          * not a cause for 'concern'; OSD warnings should
          * be reserved for when a core reports that it is
          * runahead-compatible but subsequently fails in
          * execution */
         RARCH_WARN("[Run-Ahead]: %s\n", msg_hash_to_str(MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_RUNAHEAD));
         goto force_input_dirty;
      }

      if (!runahead_create(runloop_st))
      {
         const char *runahead_failed_str =
            msg_hash_to_str(MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_SAVESTATES);
         if (!runahead_hide_warnings)
            runloop_msg_queue_push(runahead_failed_str, 0, 2 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         RARCH_WARN("[Run-Ahead]: %s\n", runahead_failed_str);
         goto force_input_dirty;
      }
   }

   /* Check for GUI */
   /* Hack: If we were in the GUI, force a resync. */
   if (frame_count != runloop_st->runahead_last_frame_count + 1)
      runloop_st->flags                  |= RUNLOOP_FLAG_RUNAHEAD_FORCE_INPUT_DIRTY;

   runloop_st->runahead_last_frame_count  = frame_count;

   if (     !use_secondary
         || !have_dynamic
         || !(runloop_st->flags & RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE))
   {
      /* TODO: multiple savestates for higher performance
       * when not using secondary core */
      for (frame_number = 0; frame_number <= runahead_count; frame_number++)
      {
         last_frame      = frame_number == runahead_count;
         suspended_frame = !last_frame;

         if (suspended_frame)
         {
            audio_st->flags     |=  AUDIO_FLAG_SUSPENDED;
            video_st->flags     &= ~VIDEO_FLAG_ACTIVE;
         }

         if (frame_number == 0)
            core_run();
         else
            runahead_core_run_use_last_input(runloop_st);

         if (suspended_frame)
         {
            if (video_st->flags & VIDEO_FLAG_RUNAHEAD_IS_ACTIVE)
               video_st->flags |=  VIDEO_FLAG_ACTIVE;
            else
               video_st->flags &= ~VIDEO_FLAG_ACTIVE;

            audio_st->flags    &= ~AUDIO_FLAG_SUSPENDED;
         }

         if (frame_number == 0)
         {
            if (!runahead_save_state(runloop_st))
            {
               const char *runahead_failed_str =
                  msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_SAVE_STATE);
               runloop_msg_queue_push(runahead_failed_str, 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               RARCH_WARN("[Run-Ahead]: %s\n", runahead_failed_str);
               return;
            }
         }

         if (last_frame)
         {
            if (!runahead_load_state(runloop_st))
            {
               const char *runahead_failed_str =
                  msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_LOAD_STATE);
               runloop_msg_queue_push(runahead_failed_str, 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               RARCH_WARN("[Run-Ahead]: %s\n", runahead_failed_str);
               return;
            }
         }
      }
   }
   else
   {
#if HAVE_DYNAMIC
      if (!secondary_core_ensure_exists(runloop_st, config_get_ptr()))
      {
         const char *runahead_failed_str =
            msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_CREATE_SECONDARY_INSTANCE);
         runahead_secondary_core_destroy(runloop_st);
         runloop_st->flags &= ~RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE;
         runloop_msg_queue_push(runahead_failed_str, 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         RARCH_WARN("[Run-Ahead]: %s\n", runahead_failed_str);
         goto force_input_dirty;
      }

      /* run main core with video suspended */
      video_st->flags &= ~VIDEO_FLAG_ACTIVE;
      core_run();
      if (video_st->flags & VIDEO_FLAG_RUNAHEAD_IS_ACTIVE)
         video_st->flags |=  VIDEO_FLAG_ACTIVE;
      else
         video_st->flags &= ~VIDEO_FLAG_ACTIVE;

      if (     (runloop_st->flags & RUNLOOP_FLAG_INPUT_IS_DIRTY)
            || (runloop_st->flags & RUNLOOP_FLAG_RUNAHEAD_FORCE_INPUT_DIRTY))
      {
         runloop_st->flags &= ~RUNLOOP_FLAG_INPUT_IS_DIRTY;

         if (!runahead_save_state(runloop_st))
         {
            const char *runahead_failed_str =
               msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_SAVE_STATE);
            runloop_msg_queue_push(runahead_failed_str, 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            RARCH_WARN("[Run-Ahead]: %s\n", runahead_failed_str);
            return;
         }

         if (!runahead_load_state_secondary(runloop_st, settings))
         {
            const char *runahead_failed_str =
               msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_LOAD_STATE);
            runloop_msg_queue_push(runahead_failed_str, 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            RARCH_WARN("[Run-Ahead]: %s\n", runahead_failed_str);
            return;
         }

         for (frame_number = 0; frame_number < runahead_count - 1; frame_number++)
         {
            video_st->flags             &= ~VIDEO_FLAG_ACTIVE;
            audio_st->flags             |= AUDIO_FLAG_SUSPENDED
                                         | AUDIO_FLAG_HARD_DISABLE;
            if (secondary_core_run_use_last_input(runloop_st))
               runloop_st->flags        |=  RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE;
            else
               runloop_st->flags        &= ~RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE;
            audio_st->flags             &= ~(AUDIO_FLAG_SUSPENDED
                                         | AUDIO_FLAG_HARD_DISABLE);
            if (video_st->flags & VIDEO_FLAG_RUNAHEAD_IS_ACTIVE)
               video_st->flags          |=  VIDEO_FLAG_ACTIVE;
            else
               video_st->flags          &= ~VIDEO_FLAG_ACTIVE;
         }
      }
      audio_st->flags                   |= AUDIO_FLAG_SUSPENDED
                                         | AUDIO_FLAG_HARD_DISABLE;
      if (secondary_core_run_use_last_input(runloop_st))
         runloop_st->flags              |=  RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE;
      else
         runloop_st->flags              &= ~RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE;
      audio_st->flags                   &= ~(AUDIO_FLAG_SUSPENDED
                                         | AUDIO_FLAG_HARD_DISABLE);
#endif
   }
   runloop_st->flags &= ~RUNLOOP_FLAG_RUNAHEAD_FORCE_INPUT_DIRTY;
   return;

force_input_dirty:
   core_run();
   runloop_st->flags |=  RUNLOOP_FLAG_RUNAHEAD_FORCE_INPUT_DIRTY;
}

/* Preemptive Frames */

static int16_t preempt_input_state(unsigned port,
      unsigned device, unsigned index, unsigned id)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   preempt_t *preempt          = runloop_st->preempt_data;
   unsigned device_class       = device & RETRO_DEVICE_MASK;

   switch (device_class)
   {
      case RETRO_DEVICE_ANALOG:
         /* Add requested inputs to mask */
         preempt->analog_mask[port] |= (1 << (id + index * 2));
         break;
      case RETRO_DEVICE_LIGHTGUN:
      case RETRO_DEVICE_POINTER:
         /* Set pointing device for this port */
         preempt->ptr_dev_needed[port] = device_class;
         break;
      case RETRO_DEVICE_MOUSE:
         /* Set pointing device and return stored x,y */
         if (id <= RETRO_DEVICE_ID_MOUSE_Y)
         {
            preempt->ptr_dev_needed[port] = device_class;
            if (preempt->ptr_dev_polled[port] == device_class)
               return preempt->ptrdev_state[port][id];
         }
         break;
      default:
         break;
   }

   return input_driver_state_wrapper(port, device, index, id);
}

static const char* preempt_allocate(runloop_state_t *runloop_st,
      const uint8_t frames)
{
   uint8_t i;
   size_t info_size;
   preempt_t *preempt          = (preempt_t*)calloc(1, sizeof(preempt_t));

   if (!(runloop_st->preempt_data = preempt))
      return msg_hash_to_str(MSG_PREEMPT_FAILED_TO_ALLOCATE);

   info_size = core_serialize_size_special();
   if (!info_size)
      return msg_hash_to_str(MSG_PREEMPT_CORE_DOES_NOT_SUPPORT_SAVESTATES);

   preempt->state_size = info_size;
   preempt->frames     = frames;

   for (i = 0; i < frames; i++)
   {
      preempt->buffer[i] = malloc(preempt->state_size);
      if (!preempt->buffer[i])
         return msg_hash_to_str(MSG_PREEMPT_FAILED_TO_ALLOCATE);
   }

   return NULL;
}

/**
 * preempt_deinit:
 *
 * Frees preempt object and unsets overrides.
 **/
void preempt_deinit(void *data)
{
   runloop_state_t *runloop_st       = (runloop_state_t*)data;
   preempt_t *preempt                = runloop_st->preempt_data;
   struct retro_core_t *current_core = &runloop_st->current_core;
   size_t i;

   if (!preempt)
      return;

   /* Free memory */
   for (i = 0; i < preempt->frames; i++)
      free(preempt->buffer[i]);

   free(preempt);
   runloop_st->preempt_data = NULL;

   /* Undo overrides */
   runloop_st->flags |= (RUNLOOP_FLAG_RUNAHEAD_AVAILABLE
         | RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE);

   if (current_core->retro_set_input_poll)
      current_core->retro_set_input_poll(runloop_st->input_poll_callback_original);
   if (current_core->retro_set_input_state)
      current_core->retro_set_input_state(runloop_st->retro_ctx.state_cb);
}


/**
 * preempt_init:
 *
 * @return true on success, false on failure
 *
 * Allocates savestate buffer and sets overrides for preemptive frames.
 **/
bool preempt_init(void *data)
{
   runloop_state_t *runloop_st = (runloop_state_t*)data;
   settings_t *settings        = config_get_ptr();
   const char *failed_str      = NULL;

   if (     runloop_st->preempt_data
         || !settings->bools.preemptive_frames_enable
         || !settings->uints.run_ahead_frames
         || !(runloop_st->current_core.flags & RETRO_CORE_FLAG_GAME_LOADED))
      return false;

   /* Check if supported - same requirements as runahead */
   if (!core_info_current_supports_runahead())
   {
      failed_str = msg_hash_to_str(MSG_PREEMPT_CORE_DOES_NOT_SUPPORT_PREEMPT);
      goto error;
   }

   /* Set flags to block runahead and request 'same instance' states */
   runloop_st->flags &= ~(RUNLOOP_FLAG_RUNAHEAD_AVAILABLE
         | RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE);

   /* Run at least one frame before attempting
    * retro_serialize_size or retro_serialize */
   if (video_state_get_ptr()->frame_count == 0)
      runloop_st->current_core.retro_run();

   /* Allocate - same 'frames' setting as runahead */
   if ((failed_str = preempt_allocate(runloop_st,
               settings->uints.run_ahead_frames)))
      goto error;

   /* Only poll in preempt_run() */
   runloop_st->current_core.retro_set_input_poll(retro_input_poll_null);
   /* Track requested analog states and pointing device types */
   runloop_st->current_core.retro_set_input_state(preempt_input_state);

   return true;

error:
   preempt_deinit(runloop_st);

   if (!settings->bools.run_ahead_hide_warnings)
      runloop_msg_queue_push(
            failed_str, 0, 2 * 60, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_WARN("[Preemptive Frames]: %s\n", failed_str);

   return false;
}

static INLINE bool preempt_analog_input_dirty(preempt_t *preempt,
      retro_input_state_t state_cb, unsigned port)
{
   int16_t state[20] = {0};
   uint8_t base, i;

   /* axes */
   for (i = 0; i < 2; i++)
   {
      base = i * 2;
      if (preempt->analog_mask[port] & (1 << (base    )))
         state[base    ] = state_cb(port, RETRO_DEVICE_ANALOG, i, 0);
      if (preempt->analog_mask[port] & (1 << (base + 1)))
         state[base + 1] = state_cb(port, RETRO_DEVICE_ANALOG, i, 1);
   }

   /* buttons */
   if (preempt->analog_mask[port] & 0xfff0)
   {
      for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
      {
         if (preempt->analog_mask[port] & (1 << (i + 4)))
            state[i + 4] = state_cb(port, RETRO_DEVICE_ANALOG,
                  RETRO_DEVICE_INDEX_ANALOG_BUTTON, i);
      }
   }

   if (memcmp(preempt->analog_state[port], state, sizeof(state)) == 0)
      return false;

   memcpy(preempt->analog_state[port], state, sizeof(state));
   return true;
}

static INLINE bool preempt_ptr_input_dirty(preempt_t *preempt,
      retro_input_state_t state_cb, unsigned device, unsigned port)
{
   int16_t state[4]  = {0};
   unsigned count_id = 0;
   unsigned x_id     = 0;
   unsigned id, max_id;

   switch (device)
   {
      case RETRO_DEVICE_MOUSE:
         max_id = RETRO_DEVICE_ID_MOUSE_BUTTON_5;
         break;
      case RETRO_DEVICE_LIGHTGUN:
         x_id   = RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X;
         max_id = RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT;
         break;
      case RETRO_DEVICE_POINTER:
         max_id   = RETRO_DEVICE_ID_POINTER_PRESSED;
         count_id = RETRO_DEVICE_ID_POINTER_COUNT;
         break;
      default:
         return false;
   }

   /* x, y */
   state[0] = state_cb(port, device, 0, x_id    );
   state[1] = state_cb(port, device, 0, x_id + 1);

   /* buttons */
   for (id = 2; id <= max_id; id++)
      state[2] |= state_cb(port, device, 0, id) ? 1 << id : 0;

   /* ptr count */
   if (count_id)
      state[3] = state_cb(port, device, 0, count_id);

   if (memcmp(preempt->ptrdev_state[port], state, sizeof(state)) == 0)
      return false;

   memcpy(preempt->ptrdev_state[port], state, sizeof(state));
   return true;
}

static INLINE void preempt_input_poll(preempt_t *preempt,
      runloop_state_t *runloop_st, settings_t *settings)
{
   size_t p;
   int16_t joypad_state;
   retro_input_state_t state_cb = input_driver_state_wrapper;
   unsigned max_users           = settings->uints.input_max_users;

   input_driver_poll();

   /* Check for input state changes */
   for (p = 0; p < max_users; p++)
   {
      /* Check full digital joypad */
      joypad_state = state_cb(p, RETRO_DEVICE_JOYPAD,
            0, RETRO_DEVICE_ID_JOYPAD_MASK);
      if (joypad_state != preempt->joypad_state[p])
      {
         preempt->joypad_state[p] = joypad_state;
         runloop_st->flags |= RUNLOOP_FLAG_INPUT_IS_DIRTY;
      }

      /* Check requested analogs */
      if (     preempt->analog_mask[p]
            && preempt_analog_input_dirty(preempt, state_cb, (unsigned)p))
      {
         runloop_st->flags |= RUNLOOP_FLAG_INPUT_IS_DIRTY;
         preempt->analog_mask[p] = 0;
      }

      /* Check requested pointing device */
      if (preempt->ptr_dev_needed[p])
      {
         if (preempt_ptr_input_dirty(
               preempt, state_cb, preempt->ptr_dev_needed[p], (unsigned)p))
            runloop_st->flags |= RUNLOOP_FLAG_INPUT_IS_DIRTY;

         preempt->ptr_dev_polled[p] = preempt->ptr_dev_needed[p];
         preempt->ptr_dev_needed[p] = RETRO_DEVICE_NONE;
      }
   }
}

/* macro for preempt_run */
#define PREEMPT_NEXT_PTR(x) ((x + 1) % preempt->frames)

/**
 * preempt_run:
 * @preempt : pointer to preemptive frames object
 *
 * Call in place of core_run() for preemptive frames.
 **/
void preempt_run(preempt_t *preempt, void *data)
{
   runloop_state_t     *runloop_st   = (runloop_state_t*)data;
   struct retro_core_t *current_core = &runloop_st->current_core;
   const char *failed_str            = NULL;
   settings_t *settings              = config_get_ptr();
   audio_driver_state_t *audio_st    = audio_state_get_ptr();
   video_driver_state_t *video_st    = video_state_get_ptr();

   /* Poll and check for dirty input */
   preempt_input_poll(preempt, runloop_st, settings);

   runloop_st->flags                |= RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE;

   if ((runloop_st->flags & RUNLOOP_FLAG_INPUT_IS_DIRTY)
         && preempt->frame_count >= preempt->frames)
   {
      /* Suspend A/V and run preemptive frames */
      audio_st->flags |=  AUDIO_FLAG_SUSPENDED;
      video_st->flags &= ~VIDEO_FLAG_ACTIVE;

      if (!current_core->retro_unserialize(
            preempt->buffer[preempt->start_ptr], preempt->state_size))
      {
         failed_str = msg_hash_to_str(MSG_PREEMPT_FAILED_TO_LOAD_STATE);
         goto error;
      }

      current_core->retro_run();
      preempt->replay_ptr = PREEMPT_NEXT_PTR(preempt->start_ptr);

      while (preempt->replay_ptr != preempt->start_ptr)
      {
         if (!current_core->retro_serialize(
               preempt->buffer[preempt->replay_ptr], preempt->state_size))
         {
            failed_str = msg_hash_to_str(MSG_PREEMPT_FAILED_TO_SAVE_STATE);
            goto error;
         }

         current_core->retro_run();
         preempt->replay_ptr = PREEMPT_NEXT_PTR(preempt->replay_ptr);
      }

      audio_st->flags &= ~AUDIO_FLAG_SUSPENDED;
      video_st->flags |=  VIDEO_FLAG_ACTIVE;
   }

   /* Save current state and set start_ptr to oldest state */
   if (!current_core->retro_serialize(
         preempt->buffer[preempt->start_ptr], preempt->state_size))
   {
      failed_str = msg_hash_to_str(MSG_PREEMPT_FAILED_TO_SAVE_STATE);
      goto error;
   }
   preempt->start_ptr = PREEMPT_NEXT_PTR(preempt->start_ptr);
   runloop_st->flags &= ~(RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE
         | RUNLOOP_FLAG_INPUT_IS_DIRTY);

   /* Run normal frame */
   current_core->retro_run();
   preempt->frame_count++;
   return;

error:
   runloop_st->flags &= ~(RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE
         | RUNLOOP_FLAG_INPUT_IS_DIRTY);
   audio_st->flags   &= ~AUDIO_FLAG_SUSPENDED;
   video_st->flags   |=  VIDEO_FLAG_ACTIVE;
   preempt_deinit(runloop_st);

   if (!settings->bools.run_ahead_hide_warnings)
      runloop_msg_queue_push(
            failed_str, 0, 2 * 60, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_ERR("[Preemptive Frames]: %s\n", failed_str);
}

void runahead_clear_variables(void *data)
{
   runloop_state_t *runloop_st            = (runloop_state_t*)data;
   video_driver_state_t *video_st         = video_state_get_ptr();
   runloop_st->runahead_save_state_size   = 0;
   runloop_st->flags                     &= ~RUNLOOP_FLAG_RUNAHEAD_SAVE_STATE_SIZE_KNOWN;
   video_st->flags                       |= VIDEO_FLAG_RUNAHEAD_IS_ACTIVE;
   runloop_st->flags                     |= RUNLOOP_FLAG_RUNAHEAD_AVAILABLE
                                          | RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE
                                          | RUNLOOP_FLAG_RUNAHEAD_FORCE_INPUT_DIRTY;
   runloop_st->runahead_last_frame_count  = 0;
}
