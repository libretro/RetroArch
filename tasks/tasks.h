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

#ifndef COMMON_TASKS_H
#define COMMON_TASKS_H

#include <stdint.h>
#include <boolean.h>

#include <queues/message_queue.h>

#include "../runloop_data.h"

#define MAX_TOKEN_LEN 255

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rarch_task rarch_task_t;
typedef void (*rarch_task_callback_t)(void *task_data, void *user_data, const char *error);
typedef void (*rarch_task_handler_t)(rarch_task_t *task);

struct rarch_task {
    rarch_task_handler_t  handler;
    rarch_task_callback_t callback; /* always called from the main loop */

    /* set by the handler */
    bool finished;

    /* created by the handler, destroyed by the user */
    void *task_data;

    /* owned by the user */
    void *user_data;

    /* created and destroyed by the code related to the handler */
    void *state;

    /* created by task handler; destroyed by main loop (after calling the callback) */
    char *error;

    /* don't touch this. */
    rarch_task_t *next;
};

/* MAIN THREAD ONLY */
void rarch_task_init(void);
void rarch_task_deinit(void);
void rarch_task_check(void);

/* MAIN AND TASK THREADS */
void rarch_task_push(rarch_task_t *task);

#ifdef HAVE_NETWORKING
typedef struct {
    char *data;
    size_t len;
} http_transfer_data_t;

bool rarch_task_push_http_transfer(const char *url, const char *type, rarch_task_callback_t cb, void *user_data);
#endif

#ifdef HAVE_RPNG
bool rarch_task_push_image_load(const char *fullpath, const char *type, rarch_task_callback_t cb);
#endif

#ifdef HAVE_LIBRETRODB
bool rarch_task_push_dbscan(const char *fullpath, bool directory, rarch_task_callback_t cb);
#endif

#ifdef HAVE_OVERLAY
void rarch_main_data_overlay_iterate(void);
#endif

void rarch_main_data_nbio_iterate(bool is_thread);
    
void data_runloop_osd_msg(const char *s, size_t len);

int find_first_data_track(const char* cue_path,
      int32_t* offset, char* track_path, size_t max_len);

int detect_system(const char* track_path, int32_t offset,
        const char** system_name);

int detect_ps1_game(const char *track_path, char *game_id);

int detect_psp_game(const char *track_path, char *game_id);

#ifdef __cplusplus
}
#endif

#endif
