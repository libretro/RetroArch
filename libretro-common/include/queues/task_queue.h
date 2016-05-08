/* Copyright  (C) 2010-2016 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (task_queue.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_TASK_QUEUE_H__
#define __LIBRETRO_SDK_TASK_QUEUE_H__

#include <stdint.h>
#include <boolean.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum task_queue_ctl_state
{
   TASK_QUEUE_CTL_NONE = 0,

   /* Deinitializes the task system.
    * This deinitializes the task system. 
    * The tasks that are running at
    * the moment will stay on hold 
    * until TASK_QUEUE_CTL_INIT is called again. */
   TASK_QUEUE_CTL_DEINIT,

   /* Initializes the task system.
    * This initializes the task system 
    * and chooses an appropriate
    * implementation according to the settings.
    *
    * This must only be called from the main thread. */
   TASK_QUEUE_CTL_INIT,

   /**
    * Calls func for every running task 
    * until it returns true.
    * Returns a task or NULL if not found.
    */
   TASK_QUEUE_CTL_FIND,

   /* Blocks until all tasks have finished.
    * This must only be called from the main thread. */
   TASK_QUEUE_CTL_WAIT,

   /* Checks for finished tasks
    * Takes the finished tasks, if any, 
    * and runs their callbacks.
    * This must only be called from the main thread. */
   TASK_QUEUE_CTL_CHECK,

   /* Pushes a task
    * The task will start as soon as possible. */
   TASK_QUEUE_CTL_PUSH,

   /* Sends a signal to terminate all the tasks.
    *
    * This won't terminate the tasks immediately.
    * They will finish as soon as possible.
    *
    * This must only be called from the main thread. */
   TASK_QUEUE_CTL_RESET,

   TASK_QUEUE_CTL_SET_THREADED,

   TASK_QUEUE_CTL_UNSET_THREADED,

   TASK_QUEUE_CTL_IS_THREADED,
   
   /**
    * Signals a task to end without waiting for
    * it to complete. */
   TASK_QUEUE_CTL_CANCEL,
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
   
   /* 1 = don't push task progress to the osd. */
   int mute;

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

void task_queue_cancel_task(void *task);

RETRO_END_DECLS

#endif
