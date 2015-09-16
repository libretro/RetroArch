/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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
#include <errno.h>
#include <fcntl.h>

#include "tasks.h"

#ifdef HAVE_LIBRETRODB
#include "../database_info.h"
#endif

#include "../dir_list_special.h"
#include "../file_ops.h"
#include "../msg_hash.h"
#include "../general.h"

#define CB_DB_SCAN_FILE    0x70ce56d2U
#define CB_DB_SCAN_FOLDER  0xde2bef8eU

#define HASH_EXTENSION_ZIP 0x0b88c7d8U
#define HASH_EXTENSION_CUE 0x0b886782U

#ifndef COLLECTION_SIZE
#define COLLECTION_SIZE 99999
#endif

#define MAX_TOKEN_LEN 255

#define MAGIC_LEN 16

struct MagicEntry
{
    const char* system_name;
    const char* magic;
};

static struct MagicEntry MAGIC_NUMBERS[] = {
    {"ps1", "\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x02\x00\x02\x00"},
    {"pcecd", "\x82\xb1\x82\xcc\x83\x76\x83\x8d\x83\x4f\x83\x89\x83\x80\x82\xcc\x92"},
    {"scd", "\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x02\x00\x01\x53"},
    {NULL, NULL}
};

typedef struct database_state_handle
{
   database_info_list_t *info;
   struct string_list *list;
   size_t list_index;
   size_t entry_index;
   uint32_t crc;
   uint8_t *buf;
   char zip_name[PATH_MAX_LENGTH];
   char *ps1_id;
} database_state_handle_t;

typedef struct db_handle
{
   database_state_handle_t state;
   database_info_handle_t *handle;
   msg_queue_t *msg_queue;
   unsigned status;
} db_handle_t;

static db_handle_t *db_ptr;

static bool pending_scan_finished;

bool rarch_main_data_db_pending_scan_finished(void)
{
   if (!pending_scan_finished)
      return false;

   pending_scan_finished = false;
   return true;
}

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

static int database_info_iterate_start
(database_info_handle_t *db, const char *name)
{
   char msg[PATH_MAX_LENGTH] = {0};

   snprintf(msg, sizeof(msg),
#ifdef _WIN32
         "%Iu/%Iu: %s %s...\n",
#else
         "%zu/%zu: %s %s...\n",
#endif
         db->list_ptr,
         db->list->size,
         msg_hash_to_str(MSG_SCANNING),
         name);

   if (msg[0] != '\0')
      rarch_main_msg_queue_push(msg, 1, 180, true);

#if 0
   RARCH_LOG("msg: %s\n", msg);
#endif


   db->status = DATABASE_STATUS_ITERATE;

   return 0;
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
         db->type = DATABASE_TYPE_ITERATE_CUE;
         return 1;
      default:
         {
            ssize_t ret;
            int read_from            = read_file(name, (void**)&db_state->buf, &ret);

            if (read_from != 1 || ret <= 0)
               return 0;


#ifdef HAVE_ZLIB
            db_state->crc = zlib_crc32_calculate(db_state->buf, ret);
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

   if (zip_name && zip_name[0] != '\0')
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
            database_info_list_iterate_found_match(db_state, db, zip_entry);
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
      if (zlib_parse_file_iterate(&db->state,
               &returnerr, name, NULL, zlib_compare_crc32,
               (void*)db_state) != 0)
         return 0;

      if (db_state->crc)
         zlib_parse_file_iterate_stop(&db->state);
   }
#endif

   return 1;
}

ssize_t get_token(int fd, char *token, size_t max_len)
{
   char *c = token;
   int rv;
   ssize_t len = 0;
   int in_string = 0;

   while (1)
   {
      rv = read(fd, c, 1);
      if (rv == 0)
         return 0;
      else if (rv < 1)
      {
         switch (errno)
         {
            case EINTR:
            case EAGAIN:
               continue;
            default:
               return -errno;
         }
      }

      switch (*c)
      {
         case ' ':
         case '\t':
         case '\r':
         case '\n':
            if (c == token)
               continue;

            if (!in_string)
            {
               *c = '\0';
               return len;
            }
            break;
         case '\"':
            if (c == token)
            {
               in_string = 1;
               continue;
            }

            *c = '\0';
            return len;
      }

      len++;
      c++;
      if (len == (ssize_t)max_len)
      {
         *c = '\0';
         return len;
      }
   }
}

int find_token(int fd, const char *token)
{
   int tmp_len = strlen(token);
   char *tmp_token = (char*)calloc(tmp_len, 1);
   if (!tmp_token)
      return -1;
   while (strncmp(tmp_token, token, tmp_len) != 0)
   {
      if (get_token(fd, tmp_token, tmp_len) <= 0)
         return -1;
   }

   return 0;
}

static int detect_ps1_game(const char* track_path, char* game_id)
{
   const char* pattern = "cdrom:";
   const char* pat_c;
   char* c;
   char* id_start;
   int i;

   int fd = open(track_path, O_RDONLY);
   if (fd < 0)
   {
      RARCH_LOG("Could not open data track: %s\n", strerror(errno));
      return -errno;
   }

   lseek(fd, 0x9340, SEEK_SET);
   if (read(fd, game_id, 10) > 0)
   {
      game_id[10] = '\0';
      game_id[4] = '-';
   }

   close(fd);
   return 1;
}

static int detect_system(const char* track_path, int32_t offset,
        const char** system_name)
{
   int rv;
   char magic[MAGIC_LEN];
   int fd;
   int i;

   fd = open(track_path, O_RDONLY);
   if (fd < 0)
   {
      RARCH_LOG("Could not open data track of file '%s': %s\n",
            track_path, strerror(errno));
      rv = -errno;
      goto clean;
   }

   lseek(fd, offset, SEEK_SET);
   if (read(fd, magic, MAGIC_LEN) < MAGIC_LEN)
   {
      RARCH_LOG("Could not read data from file '%s' at offset %d: %s\n",
            track_path, offset, strerror(errno));
      rv = -errno;
      goto clean;
   }

   RARCH_LOG("Comparing with known magic numbers...\n");
   for (i = 0; MAGIC_NUMBERS[i].system_name != NULL; i++)
   {
      if (memcmp(MAGIC_NUMBERS[i].magic, magic, MAGIC_LEN) == 0)
      {
         *system_name = MAGIC_NUMBERS[i].system_name;
         rv = 0;
         goto clean;
      }
   }

   RARCH_LOG("Could not find compatible system\n");
   rv = -EINVAL;
clean:
   close(fd);
   return rv;
}

static int find_first_data_track(const char* cue_path, int32_t* offset,
                                 char* track_path, size_t max_len)
{
   int rv;
   int fd = -1;
   char tmp_token[MAX_TOKEN_LEN];
   int m, s, f;
   char cue_dir[PATH_MAX];
   strlcpy(cue_dir, cue_path, PATH_MAX);
   path_basedir(cue_dir);

   fd = open(cue_path, O_RDONLY);
   if (fd < 0)
   {
      RARCH_LOG("Could not open CUE file '%s': %s\n", cue_path,
            strerror(errno));
      return -errno;
   }

   RARCH_LOG("Parsing CUE file '%s'...\n", cue_path);

   while (get_token(fd, tmp_token, MAX_TOKEN_LEN) > 0)
   {
      if (strcmp(tmp_token, "FILE") == 0)
      {
         get_token(fd, tmp_token, MAX_TOKEN_LEN);
         fill_pathname_join(track_path, cue_dir, tmp_token, max_len);

      }
      else if (strcasecmp(tmp_token, "TRACK") == 0)
      {
         get_token(fd, tmp_token, MAX_TOKEN_LEN);
         get_token(fd, tmp_token, MAX_TOKEN_LEN);
         if (strcasecmp(tmp_token, "AUDIO") == 0)
            continue;

         find_token(fd, "INDEX");
         get_token(fd, tmp_token, MAX_TOKEN_LEN);
         get_token(fd, tmp_token, MAX_TOKEN_LEN);
         if (sscanf(tmp_token, "%02d:%02d:%02d", &m, &s, &f) < 3)
         {
            RARCH_LOG("Error parsing time stamp '%s'\n", tmp_token);
            return -errno;
         }
         *offset = ((m * 60) * (s * 75) * f) * 25;

         RARCH_LOG("Found 1st data track on file '%s+%d'\n",
               track_path, *offset);

         rv = 0;
         goto clean;
      }
   }

   rv = -EINVAL;

clean:
   close(fd);
   return rv;
}

static int database_info_iterate_playlist_cue(
      database_state_handle_t *db_state,
      database_info_handle_t *db, const char *name)
{
   int rv;
   char track_path[PATH_MAX];
   int32_t offset = 0;
   const char* system_name = NULL;
   size_t max_len = MAX_TOKEN_LEN;
   char game_id[4096];

   rv = find_first_data_track(name, &offset, track_path, PATH_MAX);
   if (rv < 0)
   {
      RARCH_LOG("Could not find valid data track: %s\n", strerror(-rv));
      return rv;
   }

   RARCH_LOG("Reading 1st data track...\n");

   if ((rv = detect_system(track_path, offset, &system_name)) < 0)
      return rv;

   RARCH_LOG("Detected %s media\n", system_name);

   if (strcmp(system_name, "ps1") == 0)
   {
      if (detect_ps1_game(track_path, game_id) == 0)
         return 0;
      RARCH_LOG("Found disk label '%s'\n", game_id);
   }

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
      case DATABASE_TYPE_ITERATE_CUE:
         return database_info_iterate_playlist_cue(db_state, db, name);
      case DATABASE_TYPE_CRC_LOOKUP:
         return database_info_iterate_crc_lookup(db_state, db, NULL);
   }

   return 0;
}

static int database_info_poll(db_handle_t *db)
{
   char elem0[PATH_MAX_LENGTH];
   uint32_t cb_type_hash        = 0;
   struct string_list *str_list = NULL;
   const char *path = msg_queue_pull(db->msg_queue);

   if (!path)
      return -1;

   str_list                     = string_split(path, "|"); 

   if (!str_list)
      goto error;
   if (str_list->size < 1)
      goto error;

   strlcpy(elem0, str_list->elems[0].data, sizeof(elem0));
   if (str_list->size > 1)
      cb_type_hash = msg_hash_calculate(str_list->elems[1].data);

   switch (cb_type_hash)
   {
      case CB_DB_SCAN_FILE:
         db->handle = database_info_file_init(elem0, DATABASE_TYPE_ITERATE);
         break;
      case CB_DB_SCAN_FOLDER:
         db->handle = database_info_dir_init(elem0, DATABASE_TYPE_ITERATE);
         break;
   }

   string_list_free(str_list);

   return 0;

error:
   if (str_list)
      string_list_free(str_list);

   return -1;
}

static void rarch_main_data_db_cleanup_state(database_state_handle_t *db_state)
{
   if (!db_state)
      return;

   if (db_state->buf)
      free(db_state->buf);
   db_state->buf = NULL;
}

void rarch_main_data_db_iterate(bool is_thread)
{
   database_info_handle_t      *db   = (db_ptr) ? db_ptr->handle : NULL;
   database_state_handle_t *db_state = (db_ptr) ? &db_ptr->state : NULL;
   const char *name = db ? db->list->elems[db->list_ptr].data    : NULL;

   if (!db)
      goto do_poll;

   switch (db->status)
   {
      case DATABASE_STATUS_ITERATE_BEGIN:
         if (db_state && !db_state->list)
            db_state->list = dir_list_new_special(NULL, DIR_LIST_DATABASES);
         db->status = DATABASE_STATUS_ITERATE_START;
         break;
      case DATABASE_STATUS_ITERATE_START:
         rarch_main_data_db_cleanup_state(db_state);
         db_state->list_index  = 0;
         db_state->entry_index = 0;
         database_info_iterate_start(db, name);
         break;
      case DATABASE_STATUS_ITERATE:
         if (database_info_iterate(&db_ptr->state, db) == 0)
         {
            db->status = DATABASE_STATUS_ITERATE_NEXT;
            db->type   = DATABASE_TYPE_ITERATE;
         }
         break;
      case DATABASE_STATUS_ITERATE_NEXT:
         if (database_info_iterate_next(db) == 0)
         {
            db->status = DATABASE_STATUS_ITERATE_START;
            db->type   = DATABASE_TYPE_ITERATE;
         }
         else
         {
            rarch_main_msg_queue_push_new(MSG_SCANNING_OF_DIRECTORY_FINISHED, 0, 180, true);
            pending_scan_finished = true;
            db->status = DATABASE_STATUS_FREE;
         }
         break;
      case DATABASE_STATUS_FREE:
         if (db_state->list)
            dir_list_free(db_state->list);
         db_state->list = NULL;
         rarch_main_data_db_cleanup_state(db_state);
         database_info_free(db);
         if (db_ptr->handle)
            free(db_ptr->handle);
         db_ptr->handle = NULL;
         break;
      default:
      case DATABASE_STATUS_NONE:
         goto do_poll;
   }

   return;

do_poll:
   if (database_info_poll(db_ptr) != -1)
   {
      if (db_ptr->handle)
         db_ptr->handle->status = DATABASE_STATUS_ITERATE_BEGIN;
   }
}


void rarch_main_data_db_init_msg_queue(void)
{
   db_handle_t      *db   = (db_handle_t*)db_ptr;
   
   if (!db || !db->msg_queue)
      rarch_assert(db->msg_queue         = msg_queue_new(8));
}

msg_queue_t *rarch_main_data_db_get_msg_queue_ptr(void)
{
   db_handle_t      *db   = (db_handle_t*)db_ptr;
   if (!db)
      return NULL;
   return db->msg_queue;
}

void rarch_main_data_db_uninit(void)
{
   if (db_ptr)
      free(db_ptr);
   db_ptr = NULL;
   pending_scan_finished = false;
}

void rarch_main_data_db_init(void)
{
   db_ptr                = (db_handle_t*)calloc(1, sizeof(*db_ptr));
   pending_scan_finished = false;
}

bool rarch_main_data_db_is_active(void)
{
   db_handle_t              *db   = (db_handle_t*)db_ptr;
   database_info_handle_t   *dbi  = db ? db->handle : NULL;
   if (dbi && dbi->status != DATABASE_STATUS_NONE)
      return true;

   return false;
}
#endif

