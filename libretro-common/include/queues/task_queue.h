/* Copyright  (C) 2010-2018 The RetroArch team
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
#include <stddef.h>
#include <boolean.h>

#include <retro_common.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum task_type
{
   TASK_TYPE_NONE,
   /* Only one blocking task can exist in the queue at a time.
    * Attempts to add a new one while another is running is
    * ignored.
    */
   TASK_TYPE_BLOCKING
};

typedef struct retro_task retro_task_t;
typedef void (*retro_task_callback_t)(retro_task_t *task,
      void *task_data,
      void *user_data, const char *error);

typedef void (*retro_task_handler_t)(retro_task_t *task);

typedef bool (*retro_task_finder_t)(retro_task_t *task,
      void *userdata);

typedef void (*retro_task_queue_msg_t)(retro_task_t *task,
      const char *msg,
      unsigned prio, unsigned duration, bool flush);

typedef bool (*retro_task_retriever_t)(retro_task_t *task, void *data);

typedef bool (*retro_task_condition_fn_t)(void *data);

typedef struct
{
   char *source_file;
} decompress_task_data_t;

struct retro_task
{
   retro_task_handler_t  handler;

   /* always called from the main loop */
   retro_task_callback_t callback;

   /* task cleanup handler to free allocated resources, will
    * be called immediately after running the main callback */
   retro_task_handler_t cleanup;

   /* set to true by the handler to signal
    * the task has finished executing. */
   bool finished;

   /* set to true by the task system
    * to signal the task *must* end. */
   bool cancelled;

   /* if true no OSD messages will be displayed. */
   bool mute;

   /* created by the handler, destroyed by the user */
   void *task_data;

   /* owned by the user */
   void *user_data;

   /* created and destroyed by the code related to the handler */
   void *state;

   /* created by task handler; destroyed by main loop
    * (after calling the callback) */
   char *error;

   /* -1 = unmetered/indeterminate, 0-100 = current progress percentage */
   int8_t progress;

   void (*progress_cb)(retro_task_t*);

   /* handler can modify but will be
    * free()d automatically if non-NULL. */
   char *title;

   enum task_type type;

   /* task identifier */
   uint32_t ident;

   /* frontend userdata
    * (e.g. associate a sticky notification to a task) */
   void *frontend_userdata;

   /* if set to true, frontend will
   use an alternative look for the
   task progress display */
   bool alternative_look;

   /* don't touch this. */
   retro_task_t *next;
};

typedef struct task_finder_data
{
   retro_task_finder_t func;
   void *userdata;
} task_finder_data_t;

typedef struct task_retriever_info
{
   struct task_retriever_info *next;
   void *data;
} task_retriever_info_t;

typedef struct task_retriever_data
{
   retro_task_handler_t handler;
   size_t element_size;
   retro_task_retriever_t func;
   task_retriever_info_t *list;
} task_retriever_data_t;

void *task_queue_retriever_info_next(task_retriever_info_t **link);

void task_queue_retriever_info_free(task_retriever_info_t *list);

/**
 * Signals a task to end without waiting for
 * it to complete. */
void task_queue_cancel_task(void *task);

void task_set_finished(retro_task_t *task, bool finished);

void task_set_mute(retro_task_t *task, bool mute);

void task_set_error(retro_task_t *task, char *error);

void task_set_progress(retro_task_t *task, int8_t progress);

void task_set_title(retro_task_t *task, char *title);

void task_set_data(retro_task_t *task, void *data);

void task_set_cancelled(retro_task_t *task, bool cancelled);

void task_free_title(retro_task_t *task);

bool task_get_cancelled(retro_task_t *task);

bool task_get_finished(retro_task_t *task);

bool task_get_mute(retro_task_t *task);

char* task_get_error(retro_task_t *task);

int8_t task_get_progress(retro_task_t *task);

char* task_get_title(retro_task_t *task);

void* task_get_data(retro_task_t *task);

void task_queue_set_threaded(void);

void task_queue_unset_threaded(void);

bool task_queue_is_threaded(void);

/**
 * Calls func for every running task
 * until it returns true.
 * Returns a task or NULL if not found.
 */
bool task_queue_find(task_finder_data_t *find_data);

/**
 * Calls func for every running task when handler
 * parameter matches task handler, allowing the
 * list parameter to be filled with user-defined
 * data.
 */
void task_queue_retrieve(task_retriever_data_t *data);

 /* Checks for finished tasks
  * Takes the finished tasks, if any,
  * and runs their callbacks.
  * This must only be called from the main thread. */
void task_queue_check(void);

/* Pushes a task
 * The task will start as soon as possible.
 * If a second blocking task is attempted, false will be returned
 * and the task will be ignored. */
bool task_queue_push(retro_task_t *task);

/* Blocks until all tasks have finished
 * will return early if cond is not NULL
 * and cond(data) returns false.
 * This must only be called from the main thread. */
void task_queue_wait(retro_task_condition_fn_t cond, void* data);

/* Sends a signal to terminate all the tasks.
 *
 * This won't terminate the tasks immediately.
 * They will finish as soon as possible.
 *
 * This must only be called from the main thread. */
void task_queue_reset(void);

/* Deinitializes the task system.
 * This deinitializes the task system.
 * The tasks that are running at
 * the moment will stay on hold */
void task_queue_deinit(void);

/* Initializes the task system.
 * This initializes the task system
 * and chooses an appropriate
 * implementation according to the settings.
 *
 * This must only be called from the main thread. */
void task_queue_init(bool threaded, retro_task_queue_msg_t msg_push);

/* Allocates and inits a new retro_task_t */
retro_task_t *task_init(void);

RETRO_END_DECLS

#endif
