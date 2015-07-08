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

#ifndef __RETROARCH_DATA_RUNLOOP_H
#define __RETROARCH_DATA_RUNLOOP_H

#include <queues/message_queue.h>
#include <retro_miscellaneous.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif
#ifdef HAVE_LIBRETRODB
#include "database_info.h"
#endif
#include "tasks/tasks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*transfer_cb_t)(void *data, size_t len);

enum runloop_data_type
{
   DATA_TYPE_NONE = 0,
   DATA_TYPE_FILE,
   DATA_TYPE_IMAGE,
   DATA_TYPE_HTTP,
   DATA_TYPE_OVERLAY,
   DATA_TYPE_DB
};

#ifdef HAVE_LIBRETRODB
typedef struct database_state_handle
{
   database_info_list_t *info;
   struct string_list *list;
   size_t list_index;
   size_t entry_index;
   uint32_t crc;
   uint8_t *buf;
   char zip_name[PATH_MAX_LENGTH];
} database_state_handle_t;

typedef struct db_handle
{
   database_state_handle_t state;
   database_info_handle_t *handle;
   msg_queue_t *msg_queue;
   unsigned status;
} db_handle_t;
#endif

typedef struct data_runloop
{
#ifdef HAVE_LIBRETRODB
   db_handle_t db;
#endif

   bool inited;

#ifdef HAVE_THREADS
   bool thread_inited;
   unsigned thread_code;
   bool alive;

   slock_t *lock;
   slock_t *cond_lock;
   slock_t *overlay_lock;
   scond_t *cond;
   sthread_t *thread;
#endif
} data_runloop_t;

void rarch_main_data_msg_queue_push(unsigned type,
      const char *msg, const char *msg2,
      unsigned prio, unsigned duration, bool flush);

void rarch_main_data_clear_state(void);

void rarch_main_data_iterate(void);

void rarch_main_data_deinit(void);

void rarch_main_data_free(void);

void rarch_main_data_init_queues(void);

void rarch_main_data_init(void);

bool rarch_main_data_active(data_runloop_t *runloop);

data_runloop_t *rarch_main_data_get_ptr(void);

#ifdef __cplusplus
}
#endif

#endif
