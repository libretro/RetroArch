/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2021 - Daniel De Matteis
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
#include <string.h>

#include <string/stdstring.h>

#include "tasks_internal.h"

#include "../database_info.h"
#include "../menu/menu_driver.h"

/* Loading a database view means walking every record of the .rdb
 * sequentially (libretrodb queries scan the whole file), which for
 * large databases is many megabytes of IO - far too much to run on
 * the menu thread during displaylist building. The displaylist
 * parsers instead consult a one-slot result cache; on a miss they
 * push this task, display a 'initialising list' placeholder, and
 * get refreshed by the task callback once the scan (performed on
 * the task queue) has finished and populated the cache.
 *
 * The cache retains ownership of the list. It holds the most
 * recently requested {path, query} result, so refresh-driven
 * rebuilds of the same view (and re-entering it) are free, and is
 * replaced when a different view is requested. */

typedef struct db_info_handle
{
   database_info_list_t *db_list;
   char *path;
   char *query;   /* NULL = unfiltered */
} db_info_handle_t;

static database_info_list_t *db_info_cache_list = NULL;
static char *db_info_cache_path                 = NULL;
static char *db_info_cache_query                = NULL;

static bool db_info_key_equal(const char *a, const char *b)
{
   return string_is_equal(a ? a : "", b ? b : "");
}

/* True while the cache slot holds a result - including a NULL
 * result from a failed scan - for this exact {path, query}. Lets
 * the displaylist parsers distinguish 'scan failed' (show the
 * empty fallback, do NOT re-push) from 'never scanned' (push). */
static bool db_info_cache_occupied = false;

bool menu_dbinfo_cache_has(const char *path, const char *query)
{
   if (!db_info_cache_occupied)
      return false;
   if (!db_info_key_equal(path,  db_info_cache_path))
      return false;
   if (!db_info_key_equal(query, db_info_cache_query))
      return false;
   return true;
}

database_info_list_t *menu_dbinfo_cache_get(const char *path,
      const char *query)
{
   if (!menu_dbinfo_cache_has(path, query))
      return NULL;
   return db_info_cache_list;
}

void menu_dbinfo_cache_free(void)
{
   if (db_info_cache_list)
   {
      database_info_list_free(db_info_cache_list);
      free(db_info_cache_list);
      db_info_cache_list = NULL;
   }
   if (db_info_cache_path)
   {
      free(db_info_cache_path);
      db_info_cache_path = NULL;
   }
   if (db_info_cache_query)
   {
      free(db_info_cache_query);
      db_info_cache_query = NULL;
   }
   db_info_cache_occupied = false;
}

static void free_db_info_handle(db_info_handle_t *db)
{
   if (!db)
      return;
   if (db->path)
   {
      free(db->path);
      db->path = NULL;
   }
   if (db->query)
   {
      free(db->query);
      db->query = NULL;
   }
   if (db->db_list)
   {
      database_info_list_free(db->db_list);
      free(db->db_list);
      db->db_list = NULL;
   }
   free(db);
}

static void task_database_info_free(retro_task_t *task)
{
   if (!task)
      return;
   free_db_info_handle((db_info_handle_t*)task->state);
}

static void task_database_info_handler(retro_task_t *task)
{
   if (task)
   {
      db_info_handle_t *db = NULL;
      if ((db = (db_info_handle_t*)task->state))
      {
         uint8_t flg = task_get_flags(task);

         if (!((flg & RETRO_TASK_FLG_CANCELLED) > 0))
         {
            db->db_list = database_info_list_new(db->path,
                  (db->query && *db->query) ? db->query : NULL);
            task_set_progress(task, 100);
         }
      }

      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   }
}

static void cb_task_database_info(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   const char *stack_path     = NULL;
   db_info_handle_t *db       = NULL;
   struct menu_state *menu_st = menu_state_get_ptr();

   if (!task)
      return;
   if (!(db = (db_info_handle_t*)task->state))
      return;

   /* Hand the result (which may be NULL on scan failure - cached
    * anyway, so the displaylist shows its empty-list fallback
    * instead of re-pushing the task forever) over to the cache. */
   menu_dbinfo_cache_free();
   db_info_cache_list     = db->db_list;
   db_info_cache_path     = db->path;
   db_info_cache_query    = db->query;
   db_info_cache_occupied = true;
   db->db_list            = NULL;
   db->path               = NULL;
   db->query              = NULL;

   /* If a database view for this path is currently displayed, it
    * must be refreshed so it rebuilds and consumes the cache. If
    * the user has already navigated elsewhere, leave the result
    * cached for when they return. */
   menu_entries_get_last_stack(&stack_path, NULL, NULL, NULL, NULL);

   if (   stack_path && db_info_cache_path
       && string_is_equal(stack_path, db_info_cache_path))
      menu_st->flags |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                      |  MENU_ST_FLAG_PREVENT_POPULATE;
}

static bool task_database_info_finder(retro_task_t *task, void *user_data)
{
   return (task && task->handler == task_database_info_handler);
}

bool menu_dbinfo_load_in_progress(void *data)
{
   task_finder_data_t find_data;

   find_data.func     = task_database_info_finder;
   find_data.userdata = NULL;

   if (task_queue_find(&find_data))
      return true;

   return false;
}

void menu_dbinfo_wait_for_task(void)
{
   task_queue_wait(menu_dbinfo_load_in_progress, NULL);
}

bool task_push_dbinfo_load(const char *path, const char *query)
{
   task_finder_data_t find_data;
   retro_task_t *task   = NULL;
   db_info_handle_t *db = NULL;

   if (!path || !*path)
      goto error;

   task = task_init();
   db   = (db_info_handle_t*)calloc(1, sizeof(db_info_handle_t));

   if (!task || !db)
      goto error;

   /* One database scan at a time - the cache has a single slot,
    * and a second concurrent scan would just fight over it. */
   find_data.func     = task_database_info_finder;
   find_data.userdata = NULL;

   if (task_queue_find(&find_data))
      goto error;

   db->db_list = NULL;
   db->path    = strdup(path);
   db->query   = (query && *query) ? strdup(query) : NULL;

   /* Silent task: no title, no notifications */
   task->handler  = task_database_info_handler;
   task->state    = db;
   task->title    = NULL;
   task->progress = 0;
   task->callback = cb_task_database_info;
   task->cleanup  = task_database_info_free;
   task->flags   |= RETRO_TASK_FLG_MUTE;

   task_queue_push(task);

   return true;

error:
   if (task)
   {
      free(task);
      task = NULL;
   }
   free_db_info_handle(db);
   return false;
}
