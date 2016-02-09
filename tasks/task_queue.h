/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Higor Euripedes
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

#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include <stdint.h>
#include <boolean.h>

#ifdef __cplusplus
extern "C" {
#endif

enum task_queue_ctl_state
{
   TASK_CTL_NONE = 0,

   /* Deinitializes the task system.
    * This deinitializes the task system. 
    * The tasks that are running at
    * the moment will stay on hold 
    * until TASK_CTL_INIT is called again. */
   TASK_CTL_DEINIT,

   /* Initializes the task system.
    * This initializes the task system 
    * and chooses an appropriate
    * implementation according to the settings.
    *
    * This must only be called from the main thread. */
   TASK_CTL_INIT,

   /**
    * Calls func for every running task 
    * until it returns true.
    * Returns a task or NULL if not found.
    */
   TASK_CTL_FIND,

   /* Blocks until all tasks have finished.
    * This must only be called from the main thread. */
   TASK_CTL_WAIT,

   /* Checks for finished tasks
    * Takes the finished tasks, if any, 
    * and runs their callbacks.
    * This must only be called from the main thread. */
   TASK_CTL_CHECK,

   /* Pushes a task
    * The task will start as soon as possible. */
   TASK_CTL_PUSH,

   /* Sends a signal to terminate all the tasks.
    *
    * This won't terminate the tasks immediately.
    * They will finish as soon as possible.
    *
    * This must only be called from the main thread. */
   TASK_CTL_RESET,

   TASK_CTL_SET_THREADED,

   TASK_CTL_UNSET_THREADED,

   TASK_CTL_IS_THREADED
};

typedef struct retro_task retro_task_t;
typedef void (*retro_task_callback_t)(void *task_data,
      void *user_data, const char *error);

typedef void (*retro_task_handler_t)(retro_task_t *task);

typedef bool (*retro_task_finder_t)(retro_task_t *task,
      void *userdata);

typedef struct
{
   char *source_file;
} decompress_task_data_t;

struct retro_task
{
   retro_task_handler_t  handler;

   /* always called from the main loop */
   retro_task_callback_t callback;

   /* set to true by the handler to signal 
    * the task has finished executing. */
   bool finished;

   /* set to true by the task system 
    * to signal the task *must* end. */
   bool cancelled;

   /* created by the handler, destroyed by the user */
   void *task_data;

   /* owned by the user */
   void *user_data;

   /* created and destroyed by the code related to the handler */
   void *state;

   /* created by task handler; destroyed by main loop 
    * (after calling the callback) */
   char *error;

   /* -1 = unmettered, 0-100 progress value */
   int8_t progress;

   /* handler can modify but will be 
    * free()d automatically if non-NULL. */
   char *title;

   /* don't touch this. */
   retro_task_t *next;
};

typedef struct task_finder_data
{
   retro_task_finder_t func;
   void *userdata;
} task_finder_data_t;

void task_queue_push_progress(retro_task_t *task);

bool task_queue_ctl(enum task_queue_ctl_state state, void *data);

#ifdef __cplusplus
}
#endif

#endif
