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

#ifndef COMMON_TASKS_H
#define COMMON_TASKS_H

#include <stdint.h>
#include <boolean.h>

#include "../runloop.h"

#ifdef __cplusplus
extern "C" {
#endif

enum task_ctl_state
{
   TASK_CTL_NONE = 0,

   /* Blocks until all tasks have finished.
    * This must only be called from the main thread. */
   TASK_CTL_WAIT,

   /* Sends a signal to terminate all the tasks.
    *
    * This won't terminate the tasks immediately.
    * They will finish as soon as possible.
    *
    * This must only be called from the main thread. */
   TASK_CTL_RESET,
};

typedef struct rarch_task rarch_task_t;
typedef void (*rarch_task_callback_t)(void *task_data, void *user_data, const char *error);
typedef void (*rarch_task_handler_t)(rarch_task_t *task);
typedef bool (*rarch_task_finder_t)(rarch_task_t *task, void *user_data);

typedef struct
{
   char *source_file;
} decompress_task_data_t;

struct rarch_task
{
   rarch_task_handler_t  handler;
   rarch_task_callback_t callback; /* always called from the main loop */

   /* set to true by the handler to signal the task has finished executing. */
   bool finished;

   /* set to true by the task system to signal the task *must* end. */
   bool cancelled;

   /* created by the handler, destroyed by the user */
   void *task_data;

   /* owned by the user */
   void *user_data;

   /* created and destroyed by the code related to the handler */
   void *state;

   /* created by task handler; destroyed by main loop (after calling the callback) */
   char *error;

   /* -1 = unmettered, 0-100 progress value */
   int8_t progress;
   char *title; /* handler can modify but will be free()d automatically if non-null */

   /* don't touch this. */
   rarch_task_t *next;
};

/**
 * @brief Initializes the task system
 *
 * This function initializes the task system and chooses an appropriate
 * implementation according to the settings.
 *
 * This function must only be called from the main thread.
 */
void rarch_task_init(void);

/**
 * @brief Deinitializes the task system
 *
 * This function deinitializes the task system. The tasks that are running at
 * the moment will stay on hold until rarch_task_init() is called again.
 */
void rarch_task_deinit(void);

/**
 * @brief Checks for finished tasks
 *
 * Takes the finished tasks, if any, and runs their callbacks.
 *
 * This function must only be called from the main thread.
 */
void rarch_task_check(void);

/**
 * @brief Calls func for every running task until it returns true.
 *
 * @param func
 * @param user_data
 * @return a task or NULL if not found.
 */
bool rarch_task_find(rarch_task_finder_t func, void *user_data);

/**
 * @brief Pushes a task
 *
 * The task will start as soon as possible.
 *
 * @param task
 */
void rarch_task_push(rarch_task_t *task);

#ifdef HAVE_NETWORKING
typedef struct {
    char *data;
    size_t len;
} http_transfer_data_t;

bool rarch_task_push_http_transfer(const char *url, const char *type, rarch_task_callback_t cb, void *user_data);
#endif

bool rarch_task_push_image_load(const char *fullpath, const char *type, rarch_task_callback_t cb, void *user_data);

#ifdef HAVE_LIBRETRODB
bool rarch_task_push_dbscan(const char *fullpath, bool directory, rarch_task_callback_t cb);
#endif

#ifdef HAVE_OVERLAY
bool rarch_task_push_overlay_load_default(
        rarch_task_callback_t cb, void *user_data);
#endif
    
int find_first_data_track(const char* cue_path,
      int32_t* offset, char* track_path, size_t max_len);

int detect_system(const char* track_path, int32_t offset,
        const char** system_name);

int detect_ps1_game(const char *track_path, char *game_id);

int detect_psp_game(const char *track_path, char *game_id);

bool rarch_task_push_decompress(
      const char *source_file,
      const char *target_dir,
      const char *target_file,
      const char *subdir,
      const char *valid_ext,
      rarch_task_callback_t cb, void *user_data);

bool rarch_task_push_content_load_default(
      const char *core_path, const char *fullpath,
      bool persist, enum rarch_core_type type,
      rarch_task_callback_t cb, void *user_data);

bool task_ctl(enum task_ctl_state state, void *data);

#ifdef __cplusplus
}
#endif

#endif
