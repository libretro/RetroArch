/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <compat/strcasestr.h>
#include <compat/strl.h>
#include <retro_endianness.h>
#include <string/stdstring.h>

#include <queues/message_queue.h>

#include "tasks.h"

#ifdef HAVE_LIBRETRODB
#include "../database_info.h"
#endif

#include "../dir_list_special.h"
#include "../file_ops.h"
#include "../msg_hash.h"
#include "../general.h"
#include "../verbosity.h"

#define CB_DB_SCAN_FILE                0x70ce56d2U
#define CB_DB_SCAN_FOLDER              0xde2bef8eU

#define HASH_EXTENSION_ZIP             0x0b88c7d8U
#define HASH_EXTENSION_CUE             0x0b886782U
#define HASH_EXTENSION_CUE_UPPERCASE   0x0b87db22U
#define HASH_EXTENSION_ISO             0x0b8880d0U
#define HASH_EXTENSION_ISO_UPPERCASE   0x0b87f470U

#ifndef COLLECTION_SIZE
#define COLLECTION_SIZE                99999
#endif

typedef struct database_state_handle
{
   database_info_list_t *info;
   struct string_list *list;
   size_t list_index;
   size_t entry_index;
   uint32_t crc;
   uint8_t *buf;
   char zip_name[PATH_MAX_LENGTH];
   char serial[4096];
} database_state_handle_t;

typedef struct db_handle
{
   database_state_handle_t state;
   database_info_handle_t *handle;
   msg_queue_t *msg_queue;
   unsigned status;
} db_handle_t;

#ifdef HAVE_LIBRETRODB

#ifdef HAVE_ZLIB
static int zlib_compare_crc32(const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t crc32, void *userdata)
{
   database_state_handle_t *db_state = (database_state_handle_t*)userdata;
    
   db_state->crc = crc32;

   strlcpy(db_state->zip_name, name, sizeof(db_state->zip_name));

#if 0
   RARCH_LOG("Going to compare CRC 0x%x for %s\n", crc32, name);
#endif

   return 1;
}
#endif

static int database_info_iterate_start(database_info_handle_t *db,
      const char *name)
{
   char msg[128] = {0};

#ifdef _WIN32
   snprintf(msg, sizeof(msg),
         "%Iu/%Iu: %s %s...\n",
         db->list_ptr,
         db->list->size,
         msg_hash_to_str(MSG_SCANNING),
         name);
#else
   snprintf(msg, sizeof(msg),
         "%zu/%zu: %s %s...\n",
         db->list_ptr,
         db->list->size,
         msg_hash_to_str(MSG_SCANNING),
         name);
#endif

   if (!string_is_empty(msg))
      runloop_msg_queue_push(msg, 1, 180, true);

#if 0
   RARCH_LOG("msg: %s\n", msg);
#endif


   db->status = DATABASE_STATUS_ITERATE;

   return 0;
}

static int iso_get_serial(database_state_handle_t *db_state,
      database_info_handle_t *db, const char *name, char* serial)
{
   int rv;
   int32_t offset = 0;
   const char* system_name = NULL;

   if ((rv = detect_system(name, offset, &system_name)) < 0)
      return rv;

   if (string_is_equal(system_name, "psp"))
   {
      if (detect_psp_game(name, serial) == 0)
         return 0;
      RARCH_LOG("Found disk label '%s'\n", serial);
   }
   else if (string_is_equal(system_name, "ps1"))
   {
      if (detect_ps1_game(name, serial) == 0)
         return 0;
      RARCH_LOG("Found disk label '%s'\n", serial);
   }

   return 0;
}

static int cue_get_serial(database_state_handle_t *db_state,
      database_info_handle_t *db, const char *name, char* serial)
{
   char track_path[PATH_MAX_LENGTH];
   int32_t offset = 0;
   int rv = find_first_data_track(name, &offset, track_path, PATH_MAX_LENGTH);
    
   if (rv < 0)
   {
      RARCH_LOG("Could not find valid data track: %s\n", strerror(-rv));
      return rv;
   }

   RARCH_LOG("Reading 1st data track...\n");

   return iso_get_serial(db_state, db, track_path, serial);
}

static int database_info_iterate_playlist(
      database_state_handle_t *db_state,
      database_info_handle_t *db, const char *name)
{
   uint32_t extension_hash          = 0;
   char parent_dir[PATH_MAX_LENGTH] = {0};

   path_parent_dir(parent_dir);

   extension_hash = msg_hash_calculate(path_get_extension(name));

   switch (extension_hash)
   {
      case HASH_EXTENSION_ZIP:
#ifdef HAVE_ZLIB
         db->type = DATABASE_TYPE_ITERATE_ZIP;
         memset(&db->state, 0, sizeof(zlib_transfer_t));
         db_state->zip_name[0] = '\0';
         db->state.type = ZLIB_TRANSFER_INIT;

         return 1;
#endif
      case HASH_EXTENSION_CUE:
      case HASH_EXTENSION_CUE_UPPERCASE:
         db_state->serial[0] = '\0';
         cue_get_serial(db_state, db, name, db_state->serial);
         db->type = DATABASE_TYPE_SERIAL_LOOKUP;
         return 1;
      case HASH_EXTENSION_ISO:
      case HASH_EXTENSION_ISO_UPPERCASE:
         db_state->serial[0] = '\0';
         iso_get_serial(db_state, db, name, db_state->serial);
         db->type = DATABASE_TYPE_SERIAL_LOOKUP;
         return 1;
      default:
         {
            ssize_t ret;
            int read_from            = read_file(name, (void**)&db_state->buf, &ret);
#ifdef HAVE_ZLIB
            const struct zlib_file_backend *stream_backend = 
               file_archive_get_default_file_backend();
#endif

            if (read_from != 1 || ret <= 0)
               return 0;


#ifdef HAVE_ZLIB
            db_state->crc = stream_backend->stream_crc_calculate(0, db_state->buf, ret);
#endif
            db->type = DATABASE_TYPE_CRC_LOOKUP;
         }
         break;
   }

   return 1;
}

static int database_info_list_iterate_end_no_match(database_state_handle_t *db_state)
{
   /* Reached end of database list, CRC match probably didn't succeed. */
   db_state->list_index  = 0;
   db_state->entry_index = 0;

   if (db_state->crc != 0)
      db_state->crc = 0;
   return 0;
}

static int database_info_iterate_next(database_info_handle_t *db)
{
   db->list_ptr++;

   if (db->list_ptr < db->list->size)
      return 0;
   return -1;
}

static int database_info_list_iterate_new(database_state_handle_t *db_state, const char *query)
{
   const char *new_database = db_state->list->elems[db_state->list_index].data;
#if 0
   RARCH_LOG("Check database [%d/%d] : %s\n", (unsigned)db_state->list_index, 
         (unsigned)db_state->list->size, new_database);
#endif
   if (db_state->info)
      database_info_list_free(db_state->info);
   db_state->info = database_info_list_new(new_database, query);
   return 0;
}

static int database_info_list_iterate_found_match(
      database_state_handle_t *db_state,
      database_info_handle_t *db,
      const char *zip_name
      )
{
   char db_crc[PATH_MAX_LENGTH]                = {0};
   char db_playlist_path[PATH_MAX_LENGTH]      = {0};
   char  db_playlist_base_str[PATH_MAX_LENGTH] = {0};
   char entry_path_str[PATH_MAX_LENGTH]        = {0};
   content_playlist_t   *playlist = NULL;
   settings_t           *settings = config_get_ptr();
   const char            *db_path = db_state->list->elems[db_state->list_index].data;
   const char         *entry_path = db ? db->list->elems[db->list_ptr].data : NULL;
   database_info_t *db_info_entry = &db_state->info->list[db_state->entry_index];

   fill_short_pathname_representation(db_playlist_base_str,
         db_path, sizeof(db_playlist_base_str));

   path_remove_extension(db_playlist_base_str);

   strlcat(db_playlist_base_str, ".lpl", sizeof(db_playlist_base_str));
   fill_pathname_join(db_playlist_path, settings->playlist_directory,
         db_playlist_base_str, sizeof(db_playlist_path));

   playlist = content_playlist_init(db_playlist_path, COLLECTION_SIZE);


   snprintf(db_crc, sizeof(db_crc), "%08X|crc", db_info_entry->crc32);

   strlcpy(entry_path_str, entry_path, sizeof(entry_path_str));

   if (!string_is_empty(zip_name))
      fill_pathname_join_delim(entry_path_str, entry_path_str, zip_name,
            '#', sizeof(entry_path_str));

#if 0
   RARCH_LOG("Found match in database !\n");

   RARCH_LOG("Path: %s\n", db_path);
   RARCH_LOG("CRC : %s\n", db_crc);
   RARCH_LOG("Playlist Path: %s\n", db_playlist_path);
   RARCH_LOG("Entry Path: %s\n", entry_path);
   RARCH_LOG("Playlist not NULL: %d\n", playlist != NULL);
   RARCH_LOG("ZIP entry: %s\n", zip_name);
   RARCH_LOG("entry path str: %s\n", entry_path_str);
#endif

   content_playlist_push(playlist, entry_path_str,
         db_info_entry->name, "DETECT", "DETECT", db_crc, db_playlist_base_str);

   content_playlist_write_file(playlist);
   content_playlist_free(playlist);

   database_info_list_free(db_state->info);
   db_state->info = NULL;
   db_state->crc  = 0;

   return 0;
}

/* End of entries in database info list and didn't find a 
 * match, go to the next database. */
static int database_info_list_iterate_next(
      database_state_handle_t *db_state
      )
{
   db_state->list_index++;
   db_state->entry_index = 0;

   database_info_list_free(db_state->info);
   db_state->info        = NULL;

   return 1;
}

static int database_info_iterate_crc_lookup(
      database_state_handle_t *db_state,
      database_info_handle_t *db,
      const char *zip_entry)
{

   if (!db_state->list || (unsigned)db_state->list_index == (unsigned)db_state->list->size)
      return database_info_list_iterate_end_no_match(db_state);

   if (db_state->entry_index == 0)
   {
      char query[50] = {0};
      snprintf(query, sizeof(query), "{crc: b\"%08X\"}", swap_if_big32(db_state->crc));

      database_info_list_iterate_new(db_state, query);
   }

   if (db_state->info)
   {
      database_info_t *db_info_entry = &db_state->info->list[db_state->entry_index];

      if (db_info_entry && db_info_entry->crc32)
      {
#if 0
         RARCH_LOG("CRC32: 0x%08X , entry CRC32: 0x%08X (%s).\n",
                   db_state->crc, db_info_entry->crc32, db_info_entry->name);
#endif
         if (db_state->crc == db_info_entry->crc32)
            return database_info_list_iterate_found_match(db_state, db, zip_entry);
      }
   }

   db_state->entry_index++;

   if (db_state->entry_index >= db_state->info->count)
      return database_info_list_iterate_next(db_state);

   if (db_state->list_index < db_state->list->size)
   {
      /* Didn't reach the end of the database list yet,
       * continue iterating. */
      return 1;
   }

   if (db_state->info)
      database_info_list_free(db_state->info);
   return 0;
}

static int database_info_iterate_playlist_zip(
      database_state_handle_t *db_state,
      database_info_handle_t *db, const char *name)
{
   bool returnerr = true;
#ifdef HAVE_ZLIB
   if (db_state->crc != 0)
      return database_info_iterate_crc_lookup(db_state, db, db_state->zip_name);
   else
   {
      if (file_archive_parse_file_iterate(&db->state,
               &returnerr, name, NULL, zlib_compare_crc32,
               (void*)db_state) != 0)
         return 0;

      if (db_state->crc)
         file_archive_parse_file_iterate_stop(&db->state);
   }
#endif

   return 1;
}

static int database_info_iterate_serial_lookup(
      database_state_handle_t *db_state,
      database_info_handle_t *db, const char *name)
{
   if (!db_state->list || (unsigned)db_state->list_index == (unsigned)db_state->list->size)
      return database_info_list_iterate_end_no_match(db_state);

   if (db_state->entry_index == 0)
   {
      char query[50];
      char *serial_buf = bin_to_hex_alloc((uint8_t*)db_state->serial, 10 * sizeof(uint8_t));

      if (!serial_buf)
         return 1;

      snprintf(query, sizeof(query), "{'serial': b'%s'}", serial_buf);
      database_info_list_iterate_new(db_state, query);

      free(serial_buf);
   }

   if (db_state->info)
   {
      database_info_t *db_info_entry = &db_state->info->list[db_state->entry_index];

      if (db_info_entry && db_info_entry->serial)
      {
#if 0
         RARCH_LOG("serial: %s , entry serial: %s (%s).\n",
                   db_state->serial, db_info_entry->serial, db_info_entry->name);
#endif
         if (string_is_equal(db_state->serial, db_info_entry->serial))
            return database_info_list_iterate_found_match(db_state, db, NULL);
      }
   }

   db_state->entry_index++;

   if (db_state->entry_index >= db_state->info->count)
      return database_info_list_iterate_next(db_state);

   if (db_state->list_index < db_state->list->size)
   {
      /* Didn't reach the end of the database list yet,
       * continue iterating. */
      return 1;
   }

   if (db_state->info)
      database_info_list_free(db_state->info);
   return 0;
}

static int database_info_iterate(database_state_handle_t *db_state, database_info_handle_t *db)
{
   const char *name = db ? db->list->elems[db->list_ptr].data : NULL;

   if (!db || !db->list)
      return -1;

   if (!name)
      return 0;

   switch (db->type)
   {
      case DATABASE_TYPE_NONE:
         break;
      case DATABASE_TYPE_ITERATE:
         return database_info_iterate_playlist(db_state, db, name);
      case DATABASE_TYPE_ITERATE_ZIP:
         return database_info_iterate_playlist_zip(db_state, db, name);
      case DATABASE_TYPE_SERIAL_LOOKUP:
         return database_info_iterate_serial_lookup(db_state, db, name);
      case DATABASE_TYPE_CRC_LOOKUP:
         return database_info_iterate_crc_lookup(db_state, db, NULL);
   }

   return 0;
}

static void rarch_main_data_db_cleanup_state(database_state_handle_t *db_state)
{
   if (!db_state)
      return;

   if (db_state->buf)
      free(db_state->buf);
   db_state->buf = NULL;
}

static void rarch_dbscan_task_handler(rarch_task_t *task)
{
   db_handle_t *db = (db_handle_t*)task->state;
   database_info_handle_t  *dbinfo  = db->handle;
   database_state_handle_t *dbstate = &db->state;
   const char *name = dbinfo ? dbinfo->list->elems[dbinfo->list_ptr].data    : NULL;

   if (!dbinfo || task->cancelled)
      goto task_finished;

   switch (dbinfo->status)
   {
      case DATABASE_STATUS_ITERATE_BEGIN:
         if (dbstate && !dbstate->list)
            dbstate->list = dir_list_new_special(NULL, DIR_LIST_DATABASES, NULL);
         dbinfo->status = DATABASE_STATUS_ITERATE_START;
         break;
      case DATABASE_STATUS_ITERATE_START:
         rarch_main_data_db_cleanup_state(dbstate);
         dbstate->list_index  = 0;
         dbstate->entry_index = 0;
         database_info_iterate_start(dbinfo, name);
         break;
      case DATABASE_STATUS_ITERATE:
         if (database_info_iterate(&db->state, dbinfo) == 0)
         {
            dbinfo->status = DATABASE_STATUS_ITERATE_NEXT;
            dbinfo->type   = DATABASE_TYPE_ITERATE;
         }
         break;
      case DATABASE_STATUS_ITERATE_NEXT:
         if (database_info_iterate_next(dbinfo) == 0)
         {
            dbinfo->status = DATABASE_STATUS_ITERATE_START;
            dbinfo->type   = DATABASE_TYPE_ITERATE;
         }
         else
         {
            runloop_msg_queue_push_new(MSG_SCANNING_OF_DIRECTORY_FINISHED, 0, 180, true);
            goto task_finished;
         }
         break;
      default:
      case DATABASE_STATUS_FREE:
      case DATABASE_STATUS_NONE:
         goto task_finished;
   }

   return;
task_finished:
   task->finished = true;

   if (db->state.list)
      dir_list_free(db->state.list);

   if (db->state.buf)
      free(db->state.buf);

   if (db->handle)
      database_info_free(db->handle);

   free(dbinfo);
   free(db);
}

bool rarch_task_push_dbscan(const char *fullpath, bool directory, rarch_task_callback_t cb)
{
   rarch_task_t *t   = (rarch_task_t*)calloc(1, sizeof(*t));
   db_handle_t *db   = (db_handle_t*)calloc(1, sizeof(db_handle_t));

   if (!t || !db)
      goto error;

   t->handler        = rarch_dbscan_task_handler;
   t->state          = db;
   t->callback       = cb;

   if (directory)
      db->handle = database_info_dir_init(fullpath, DATABASE_TYPE_ITERATE);
   else
      db->handle = database_info_file_init(fullpath, DATABASE_TYPE_ITERATE);

   if (db->handle)
      db->handle->status = DATABASE_STATUS_ITERATE_BEGIN;

   rarch_task_push(t);

   return true;

error:
   if (t)
      free(t);
   if (db)
      free(db);
   return false;
}

#endif

