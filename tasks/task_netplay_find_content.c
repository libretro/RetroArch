/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2017 - Jean-André Santoni
 *  Copyright (C) 2017-2019 - Andrés Suárez
 *  Copyright (C) 2021-2022 - Roberto V. Rampim
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

#include <stdlib.h>
#include <stdio.h>

#include <retro_miscellaneous.h>

#include <string/stdstring.h>
#include <lists/string_list.h>
#include <file/file_path.h>
#include <lists/dir_list.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "../verbosity.h"
#include "../configuration.h"
#include "../paths.h"
#include "../command.h"
#include "../playlist.h"
#include "../core_info.h"
#include "../content.h"
#include "../runloop.h"

#ifndef HAVE_DYNAMIC
#include "../retroarch.h"
#include "../frontend/frontend_driver.h"
#endif

#include "task_content.h"
#include "tasks_internal.h"

#ifdef HAVE_NETWORKING

#include "../network/netplay/netplay.h"

enum
{
   /* values */
   STATE_NONE             = 0,
   STATE_LOAD,
   STATE_LOAD_SUBSYSTEM,
   STATE_LOAD_CONTENTLESS,
   /* values mask */
   STATE_MASK             = 0xFF,
   /* flags */
   STATE_RELOAD           = 0x100
};

struct netplay_crc_scan_state
{
   int  state;
   bool running;
};

struct netplay_crc_scan_data
{
   struct
   {
      struct string_list *subsystem_content;
      uint32_t crc;
      char content[NETPLAY_HOST_LONGSTR_LEN];
      char content_path[PATH_MAX_LENGTH];
      char subsystem[NETPLAY_HOST_LONGSTR_LEN];
      char extension[32];
      bool core_loaded;
   } current;
   struct string_list content_paths;
   playlist_config_t playlist_config;
   struct string_list *playlists;
   struct string_list *extensions;
   uint32_t crc;
   char content[NETPLAY_HOST_LONGSTR_LEN];
   char subsystem[NETPLAY_HOST_LONGSTR_LEN];
   char core[PATH_MAX_LENGTH];
   char hostname[512];
};

static struct netplay_crc_scan_state scan_state = {0};

static bool find_content_by_crc(playlist_config_t *playlist_config,
      const struct string_list *playlists, uint32_t crc,
      struct string_list *paths, bool first_only)
{
   size_t i, j;
   char crc_ident[16];
   playlist_t *playlist;
   union string_list_elem_attr attr;
   bool ret = false;

   snprintf(crc_ident, sizeof(crc_ident), "%08lX|crc", (unsigned long)crc);
   attr.i = 0;

   for (i = 0; i < playlists->size; i++)
   {
      playlist_config_set_path(playlist_config, playlists->elems[i].data);

      if (!(playlist = playlist_init(playlist_config)))
         continue;

      for (j = 0; j < playlist_get_size(playlist); j++)
      {
         const struct playlist_entry *entry = NULL;

         playlist_get_index(playlist, j, &entry);
         if (!entry)
            continue;

         if (string_is_equal(entry->crc32, crc_ident) &&
               !string_is_empty(entry->path))
         {
            if (!string_list_append(paths, entry->path, attr))
            {
               playlist_free(playlist);
               return false;
            }

            if (first_only)
            {
               playlist_free(playlist);
               return true;
            }

            ret = true;
         }
      }

      playlist_free(playlist);
   }

   return ret;
}

static bool find_content_by_name(playlist_config_t *playlist_config,
      const struct string_list *playlists, const struct string_list *names,
      const struct string_list *extensions, struct string_list *paths,
      bool with_extension)
{
   size_t i, j, k;
   char buf[PATH_MAX_LENGTH];
   bool has_extensions;
   union string_list_elem_attr attr;
   bool err                   = false;
   playlist_t *playlist       = NULL;
   playlist_t **playlist_ptrs =
      (playlist_t**)calloc(playlists->size, sizeof(*playlist_ptrs));

   if (!playlist_ptrs)
      return false;

   has_extensions = extensions && extensions->size > 0;
   attr.i         = 0;

   for (i = 0; i < names->size && !err; i++)
   {
      bool found       = false;
      const char *name = names->elems[i].data;

      for (j = 0; j < playlists->size; j++)
      {
         /* We do it like this
            because we want names and paths to have the same order;
            we also want to read and parse a playlist only the first time. */
         if (!(playlist = playlist_ptrs[j]))
         {
            playlist_config_set_path(playlist_config,
               playlists->elems[j].data);

            if (!(playlist = playlist_init(playlist_config)))
               continue;
            playlist_ptrs[j] = playlist;
         }

         for (k = 0; k < playlist_get_size(playlist); k++)
         {
            const struct playlist_entry *entry = NULL;

            playlist_get_index(playlist, k, &entry);
            if (!entry)
               continue;

            /* If we don't have an extensions list, accept anything,
               as long as the name matches. */
            if (has_extensions)
            {
               const char *extension = path_get_extension(entry->path);

               if (string_is_empty(extension) || !string_list_find_elem(
                     extensions, extension))
                  continue;
            }

            strlcpy(buf, path_basename(entry->path), sizeof(buf));
            if (!with_extension)
               path_remove_extension(buf);

            if (string_is_equal_case_insensitive(buf, name))
            {
               found = true;

               if (!string_list_append(paths, entry->path, attr))
                  err = true;

               break;
            }
         }

         if (found)
            break;
      }

      if (!found)
         break;
   }

   for (j = 0; j < playlists->size; j++)
      playlist_free(playlist_ptrs[j]);
   free(playlist_ptrs);

   return !err && i == names->size;
}

/**
 * Execute a search for compatible content for netplay.
 * We prioritize a CRC match, if we have a CRC to match against.
 * If we don't have a CRC, or if there's no CRC match found,
 * fall back to a filename match and hope for the best.
 */
static void task_netplay_crc_scan_handler(retro_task_t *task)
{
   struct netplay_crc_scan_state *state =
      (struct netplay_crc_scan_state*)task->state;
   struct netplay_crc_scan_data  *data  =
      (struct netplay_crc_scan_data*)task->task_data;
   const char                    *title = NULL;
   struct string_list content_list      = {0};

   if (state->state != STATE_NONE)
      goto finished; /* We already have what we need. */

   /* We really can't do much without the core's path. */
   if (string_is_empty(data->core))
   {
      title =
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_CORE);
      goto finished;
   }

   if (string_is_empty(data->content) ||
         string_is_equal_case_insensitive(data->content, "N/A"))
   {
      title        =
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND);
      state->state = STATE_LOAD_CONTENTLESS;

      if (data->current.core_loaded)
         state->state |= STATE_RELOAD;

      goto finished;
   }

   if (data->current.core_loaded && data->crc > 0 && data->current.crc > 0)
   {
      RARCH_LOG("[Lobby] Testing CRC matching for: %08lX\n",
         (unsigned long)data->crc);
      RARCH_LOG("[Lobby] Current content CRC: %08lX\n",
         (unsigned long)data->current.crc);

      if (data->current.crc == data->crc)
      {
         RARCH_LOG("[Lobby] CRC match with currently loaded content.\n");

         title        = msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND);
         state->state = STATE_LOAD | STATE_RELOAD;
         goto finished;
      }
   }

   if (string_is_empty(data->subsystem) ||
         string_is_equal_case_insensitive(data->subsystem, "N/A"))
   {
      if (data->current.core_loaded && data->extensions &&
            !string_is_empty(data->current.content) &&
            !string_is_empty(data->current.extension))
      {
         if (!data->current.subsystem_content ||
               !data->current.subsystem_content->size)
         {
            if (string_is_equal_case_insensitive(
                     data->current.content, data->content) &&
                  string_list_find_elem(
                     data->extensions, data->current.extension))
            {
               RARCH_LOG("[Lobby] Filename match with currently loaded content.\n");

               title        = msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND);
               state->state = STATE_LOAD | STATE_RELOAD;
               goto finished;
            }
         }
      }

      if (!data->playlists || !data->playlists->size)
      {
         title = msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_PLAYLISTS);
         goto finished;
      }

      if (!string_list_initialize(&data->content_paths))
         goto finished;

      /* We try a CRC match first. */
      if (data->crc > 0 && find_content_by_crc(&data->playlist_config,
            data->playlists, data->crc, &data->content_paths, true))
      {
         RARCH_LOG("[Lobby] Playlist CRC match.\n");

         title        = msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND);
         state->state = STATE_LOAD;
         goto finished;
      }
      else
      {
         union string_list_elem_attr attr;

         if (!string_list_initialize(&content_list))
            goto finished;

         attr.i = 0;
         if (!string_list_append(&content_list, data->content, attr))
            goto finished;

         /* Now we try a filename match as a last resort. */
         if (find_content_by_name(&data->playlist_config, data->playlists,
               &content_list, data->extensions, &data->content_paths, false))
         {
            RARCH_LOG("[Lobby] Playlist filename match.\n");

            title        = msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND);
            state->state = STATE_LOAD;
            goto finished;
         }
      }
   }
   else
   {
      if (!string_list_initialize(&content_list))
         goto finished;
      if (!string_split_noalloc(&content_list, data->content, "|"))
         goto finished;

      if (data->current.core_loaded && data->current.subsystem_content &&
            data->current.subsystem_content->size > 0 &&
            string_is_equal_case_insensitive(data->current.subsystem,
               data->subsystem))
      {
         if (content_list.size == data->current.subsystem_content->size)
         {
            size_t i;
            bool loaded = true;

            for (i = 0; i < content_list.size; i++)
            {
               const char *local_content  = path_basename(
                  data->current.subsystem_content->elems[i].data);
               const char *remote_content = content_list.elems[i].data;

               if (!string_is_equal_case_insensitive(local_content,
                     remote_content))
               {
                  loaded = false;
                  break;
               }
            }

            if (loaded)
            {
               RARCH_LOG("[Lobby] Subsystem match with currently loaded content.\n");

               title        = msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND);
               state->state = STATE_LOAD_SUBSYSTEM | STATE_RELOAD;
               goto finished;
            }
         }
      }

      if (!data->playlists || !data->playlists->size)
      {
         title = msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_PLAYLISTS);
         goto finished;
      }

      if (!string_list_initialize(&data->content_paths))
         goto finished;

      /* Subsystems won't have a CRC.
         Must always match by filename(s). */
      if (find_content_by_name(&data->playlist_config, data->playlists,
            &content_list, data->extensions, &data->content_paths, true))
      {
         RARCH_LOG("[Lobby] Playlist subsystem match.\n");

         title        = msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND);
         state->state = STATE_LOAD_SUBSYSTEM;
         goto finished;
      }
   }

   title =
      msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NOT_FOUND);

finished:
   string_list_deinitialize(&content_list);

   task_set_progress(task, 100);

   if (title)
   {
      task_free_title(task);
      task_set_title(task, strdup(title));
   }

   task_set_finished(task, true);
}

#ifndef HAVE_DYNAMIC
static bool static_load(const char *core, const char *subsystem,
      const void *content, const char *hostname)
{
#define ARG(arg) (void*)(arg)
   netplay_driver_ctl(RARCH_NETPLAY_CTL_CLEAR_FORK_ARGS, NULL);

   if (string_is_empty(hostname))
   {
      if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_ADD_FORK_ARG, ARG("-H")))
         goto failure;
   }
   else
   {
      if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_ADD_FORK_ARG, ARG("-C")) ||
            !netplay_driver_ctl(RARCH_NETPLAY_CTL_ADD_FORK_ARG, ARG(hostname)))
         goto failure;
   }

   if (!string_is_empty(subsystem))
   {
      const struct string_list *subsystem_content =
         (const struct string_list*)content;

      if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_ADD_FORK_ARG, ARG("--subsystem")) ||
            !netplay_driver_ctl(RARCH_NETPLAY_CTL_ADD_FORK_ARG, ARG(subsystem)))
         goto failure;

      if (subsystem_content && subsystem_content->size > 0)
      {
         size_t i;

         for (i = 0; i < subsystem_content->size; i++)
         {
            if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_ADD_FORK_ARG,
                  ARG(subsystem_content->elems[i].data)))
               goto failure;
         }
      }
#ifdef HAVE_MENU
      else
      {
         if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_ADD_FORK_ARG, ARG("--menu")))
            goto failure;
      }
#endif
   }
   else
   {
      if (content)
      {
         if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_ADD_FORK_ARG, ARG(content)))
            goto failure;
      }
#ifdef HAVE_MENU
      else
      {
         if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_ADD_FORK_ARG, ARG("--menu")))
            goto failure;
      }
#endif
   }

   if (!frontend_driver_set_fork(FRONTEND_FORK_CORE_WITH_ARGS))
      goto failure;

   path_set(RARCH_PATH_CORE, core);

   retroarch_ctl(RARCH_CTL_SET_SHUTDOWN, NULL);

   return true;

failure:
   RARCH_ERR("[Lobby] Failed to fork RetroArch for netplay.\n");

   netplay_driver_ctl(RARCH_NETPLAY_CTL_CLEAR_FORK_ARGS, NULL);

   return false;

#undef ARG
}
#endif

static void task_netplay_crc_scan_callback(retro_task_t *task,
      void *task_data, void *user_data, const char *error)
{
   struct netplay_crc_scan_state *state =
      (struct netplay_crc_scan_state*)task->state;
   struct netplay_crc_scan_data  *data  =
      (struct netplay_crc_scan_data*)task_data;

   switch (state->state & STATE_MASK)
   {
      case STATE_LOAD:
         {
            const char *content_path        = (state->state & STATE_RELOAD) ?
               data->current.content_path : data->content_paths.elems[0].data;
#ifdef HAVE_DYNAMIC
            content_ctx_info_t content_info = {0};

            if (data->current.core_loaded)
               command_event(CMD_EVENT_UNLOAD_CORE, NULL);

            RARCH_LOG("[Lobby] Loading core '%s' with content file '%s'.\n",
               data->core, content_path);

            command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);

            if (string_is_empty(data->hostname))
            {
               netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_SERVER, NULL);
            }
            else
            {
               netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_CLIENT, NULL);
               command_event(CMD_EVENT_NETPLAY_INIT_DIRECT_DEFERRED,
                  data->hostname);
            }

            task_push_load_new_core(data->core,
               NULL, NULL, CORE_TYPE_PLAIN, NULL, NULL);
            task_push_load_content_with_core(content_path,
               &content_info, CORE_TYPE_PLAIN, NULL, NULL);
#else

            static_load(data->core, NULL, content_path, data->hostname);
#endif
         }
         break;

      case STATE_LOAD_SUBSYSTEM:
         {
            const char *subsystem;
            struct string_list *subsystem_content;

            if (state->state & STATE_RELOAD)
            {
               subsystem         = data->current.subsystem;
               subsystem_content = data->current.subsystem_content;
            }
            else
            {
               subsystem         = data->subsystem;
               subsystem_content = &data->content_paths;
            }

#ifdef HAVE_DYNAMIC
            if (data->current.core_loaded)
               command_event(CMD_EVENT_UNLOAD_CORE, NULL);

            RARCH_LOG("[Lobby] Loading core '%s' with subsystem '%s'.\n",
               data->core, subsystem);

            command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);

            if (string_is_empty(data->hostname))
               netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_SERVER, NULL);
            else
               netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_CLIENT, NULL);

            task_push_load_new_core(data->core,
               NULL, NULL, CORE_TYPE_PLAIN, NULL, NULL);

            content_clear_subsystem();

            if (content_set_subsystem_by_name(subsystem))
            {
               size_t i;
               content_ctx_info_t content_info = {0};

               for (i = 0; i < subsystem_content->size; i++)
                  content_add_subsystem(subsystem_content->elems[i].data);

               if (!string_is_empty(data->hostname))
                  command_event(CMD_EVENT_NETPLAY_INIT_DIRECT_DEFERRED,
                     data->hostname);

               task_push_load_subsystem_with_core(NULL,
                  &content_info, CORE_TYPE_PLAIN, NULL, NULL);
            }
            else
            {
               RARCH_ERR("[Lobby] Subsystem not found.\n");

               /* Disable netplay if we don't have the subsystem. */
               netplay_driver_ctl(RARCH_NETPLAY_CTL_DISABLE, NULL);

               command_event(CMD_EVENT_UNLOAD_CORE, NULL);
            }
#else
            static_load(data->core, subsystem, subsystem_content,
               data->hostname);
#endif
         }
         break;

      case STATE_LOAD_CONTENTLESS:
         {
#ifdef HAVE_DYNAMIC
            content_ctx_info_t content_info = {0};

            if (data->current.core_loaded)
               command_event(CMD_EVENT_UNLOAD_CORE, NULL);

            RARCH_LOG("[Lobby] Loading contentless core '%s'.\n", data->core);

            command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);

            if (string_is_empty(data->hostname))
            {
               netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_SERVER, NULL);
            }
            else
            {
               netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_CLIENT, NULL);
               command_event(CMD_EVENT_NETPLAY_INIT_DIRECT_DEFERRED,
                  data->hostname);
            }

            task_push_load_new_core(data->core,
               NULL, NULL, CORE_TYPE_PLAIN, NULL, NULL);
            task_push_start_current_core(&content_info);
#else
            static_load(data->core, NULL, NULL, data->hostname);
#endif
         }
         break;

      case STATE_NONE:
         {
            if (data->current.core_loaded && (state->state & STATE_RELOAD))
            {
#ifdef HAVE_DYNAMIC
               command_event(CMD_EVENT_UNLOAD_CORE, NULL);

               command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);

               if (string_is_empty(data->hostname))
               {
                  netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_SERVER, NULL);
               }
               else
               {
                  netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_CLIENT, NULL);
                  command_event(CMD_EVENT_NETPLAY_INIT_DIRECT_DEFERRED,
                     data->hostname);
               }

               task_push_load_new_core(data->core,
                  NULL, NULL, CORE_TYPE_PLAIN, NULL, NULL);

               if (!string_is_empty(data->current.subsystem))
               {
                  content_clear_subsystem();
                  content_set_subsystem_by_name(data->current.subsystem);
               }

               runloop_msg_queue_push(
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED),
                  1, 480, true, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
#else
               runloop_msg_queue_push(
                  msg_hash_to_str(MSG_NETPLAY_NEED_CONTENT_LOADED),
                  1, 480, true, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
#endif
            }
            else
               RARCH_WARN("[Lobby] Nothing to load.\n");
         }
         break;

      default:
         break;
   }
}

static void task_netplay_crc_scan_cleanup(retro_task_t *task)
{
   struct netplay_crc_scan_state *state =
      (struct netplay_crc_scan_state*)task->state;
   struct netplay_crc_scan_data  *data  =
      (struct netplay_crc_scan_data*)task->task_data;

   string_list_deinitialize(&data->content_paths);

   string_list_free(data->current.subsystem_content);
   string_list_free(data->playlists);
   string_list_free(data->extensions);

   free(data);

   state->running = false;
}

bool task_push_netplay_crc_scan(uint32_t crc, const char *content,
      const char *subsystem, const char *core, const char *hostname)
{
   size_t i;
   struct netplay_crc_scan_data *data;
   retro_task_t *task;
   const char *pbasename, *pcontent, *psubsystem;
   core_info_list_t         *coreinfos = NULL;
   settings_t               *settings  = config_get_ptr();
   struct retro_system_info *system    = &runloop_state_get_ptr()->system.info;

   /* Do not run more than one CRC scan task at a time. */
   if (scan_state.running)
      return false;

   data = (struct netplay_crc_scan_data*)calloc(1, sizeof(*data));
   task = task_init();

   if (!data || !task)
   {
      free(data);
      free(task);

      return false;
   }

   data->crc = crc;

   strlcpy(data->content, content, sizeof(data->content));
   strlcpy(data->subsystem, subsystem, sizeof(data->subsystem));
   strlcpy(data->hostname, hostname, sizeof(data->hostname));

   core_info_get_list(&coreinfos);
   for (i = 0; i < coreinfos->count; i++)
   {
      core_info_t *coreinfo = &coreinfos->list[i];

      if (string_is_equal_case_insensitive(coreinfo->core_name, core))
      {
         strlcpy(data->core, coreinfo->path, sizeof(data->core));

         if (coreinfo->supported_extensions_list)
            data->extensions =
               string_list_clone(coreinfo->supported_extensions_list);

         break;
      }
   }

   data->playlist_config.capacity            = COLLECTION_SIZE;
   data->playlist_config.old_format          =
      settings->bools.playlist_use_old_format;
   data->playlist_config.compress            =
      settings->bools.playlist_compression;
   data->playlist_config.fuzzy_archive_match =
      settings->bools.playlist_fuzzy_archive_match;
   playlist_config_set_base_content_directory(&data->playlist_config,
      settings->bools.playlist_portable_paths ?
         settings->paths.directory_menu_content : NULL);

   data->playlists = dir_list_new(settings->paths.directory_playlist, "lpl",
      false, true, false, false);
   if (!data->playlists)
      data->playlists = string_list_new();
   if (data->playlists)
   {
      union string_list_elem_attr attr;

      attr.i = RARCH_PLAIN_FILE;
      string_list_append(data->playlists,
         settings->paths.path_content_history, attr);
   }

   data->current.crc = content_get_crc();

   pbasename  = path_get(RARCH_PATH_BASENAME);
   if (!string_is_empty(pbasename))
      strlcpy(data->current.content, path_basename(pbasename),
         sizeof(data->current.content));

   pcontent   = path_get(RARCH_PATH_CONTENT);
   if (!string_is_empty(pcontent))
   {
      strlcpy(data->current.content_path, pcontent,
         sizeof(data->current.content_path));
      strlcpy(data->current.extension, path_get_extension(pcontent),
         sizeof(data->current.extension));
   }

   psubsystem = path_get(RARCH_PATH_SUBSYSTEM);
   if (!string_is_empty(psubsystem))
      strlcpy(data->current.subsystem, psubsystem,
         sizeof(data->current.subsystem));

   if (path_get_subsystem_list())
      data->current.subsystem_content =
         string_list_clone(path_get_subsystem_list());

   data->current.core_loaded =
      string_is_equal_case_insensitive(system->library_name, core);

   scan_state.state   = STATE_NONE;
   scan_state.running = true;

   task->handler   = task_netplay_crc_scan_handler;
   task->callback  = task_netplay_crc_scan_callback;
   task->cleanup   = task_netplay_crc_scan_cleanup;
   task->task_data = data;
   task->state     = &scan_state;
   task->title     = strdup(
      msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_LOOK));

   task_queue_push(task);

   return true;
}

bool task_push_netplay_content_reload(const char *hostname)
{
   struct netplay_crc_scan_data *data;
   retro_task_t *task;
   const char *pcore;
   bool contentless, is_inited;

   /* Do not run more than one CRC scan task at a time. */
   if (scan_state.running)
      return false;

   pcore = path_get(RARCH_PATH_CORE);
   if (string_is_empty(pcore) || string_is_equal(pcore, "builtin"))
      return false; /* Nothing to reload. */

   data = (struct netplay_crc_scan_data*)calloc(1, sizeof(*data));
   task = task_init();

   if (!data || !task)
   {
      free(data);
      free(task);

      return false;
   }

   scan_state.state = STATE_RELOAD;

   strlcpy(data->core, pcore, sizeof(data->core));

   /* Hostname being NULL indicates this is a host. */
   if (hostname)
      strlcpy(data->hostname, hostname, sizeof(data->hostname));

   content_get_status(&contentless, &is_inited);
   if (contentless)
   {
      scan_state.state |= STATE_LOAD_CONTENTLESS;
   }
   else if (is_inited)
   {
      const char *psubsystem = path_get(RARCH_PATH_SUBSYSTEM);

      if (!string_is_empty(psubsystem))
      {
         strlcpy(data->current.subsystem, psubsystem,
            sizeof(data->current.subsystem));

         if (path_get_subsystem_list())
         {
            data->current.subsystem_content =
               string_list_clone(path_get_subsystem_list());

            if (data->current.subsystem_content &&
                  data->current.subsystem_content->size > 0)
               scan_state.state |= STATE_LOAD_SUBSYSTEM;
         }
      }
      else if (!path_is_empty(RARCH_PATH_BASENAME))
      {
         const char *pcontent = path_get(RARCH_PATH_CONTENT);

         if (!string_is_empty(pcontent))
         {
            strlcpy(data->current.content_path, pcontent,
               sizeof(data->current.content_path));

            scan_state.state |= STATE_LOAD;
         }
      }
   }

   data->current.core_loaded = true;

   scan_state.running = true;

   task->handler   = task_netplay_crc_scan_handler;
   task->callback  = task_netplay_crc_scan_callback;
   task->cleanup   = task_netplay_crc_scan_cleanup;
   task->task_data = data;
   task->state     = &scan_state;

   task_queue_push(task);

   return true;
}
#else
bool task_push_netplay_crc_scan(uint32_t crc, const char *content,
      const char *subsystem, const char *core, const char *hostname)
{
   return false;
}

bool task_push_netplay_content_reload(const char *hostname)
{
   return false;
}
#endif
