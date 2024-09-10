/* Copyright  (C) 2010-2020 The RetroArch team
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

#include <libretro.h>

RETRO_BEGIN_DECLS

enum task_type
{
   /** A regular task. The vast majority of tasks will use this type. */
   TASK_TYPE_NONE,

   /**
    * Only one blocking task can exist in the queue at a time.
    * Attempts to add a new one while another is running will fail.
    */
   TASK_TYPE_BLOCKING
};

enum task_style
{
   TASK_STYLE_NONE,
   TASK_STYLE_POSITIVE,
   TASK_STYLE_NEGATIVE
};

typedef struct retro_task retro_task_t;

/** @copydoc retro_task::callback */
typedef void (*retro_task_callback_t)(retro_task_t *task,
      void *task_data,
      void *user_data, const char *error);

/** @copydoc retro_task::handler */
typedef void (*retro_task_handler_t)(retro_task_t *task);

/** @copydoc task_finder_data::func */
typedef bool (*retro_task_finder_t)(retro_task_t *task,
      void *userdata);

/**
 * Displays a message output by a task.
 */
typedef void (*retro_task_queue_msg_t)(retro_task_t *task,
      const char *msg,
      unsigned prio, unsigned duration, bool flush);

/** @copydoc task_retriever_data::func */
typedef bool (*retro_task_retriever_t)(retro_task_t *task, void *data);

/**
 * Called by \c task_queue_wait after each task executes
 * (i.e. once per pass over the queue).
 * @param data Arbitrary data.
 * The function should cast this to a known type.
 * @return \c true if \c task_queue_wait should continue waiting,
 * \c false if it should return early.
 * @see task_queue_wait
 */
typedef bool (*retro_task_condition_fn_t)(void *data);

typedef struct
{
   char *source_file;
} decompress_task_data_t;

enum retro_task_flags
{
   /**
    * If \c true, the frontend should use some alternative means
    * of displaying this task's progress or messages.
    * Not used within cores.
    */
   RETRO_TASK_FLG_ALTERNATIVE_LOOK = (1 << 0),
   /**
    * Set to \c true by \c handler to indicate that this task has finished.
    * At this point the task queue will call \c callback and \c cleanup,
    * then deallocate the task.
    */
   RETRO_TASK_FLG_FINISHED         = (1 << 1),
   /**
    * Set to true by the task queue to signal that this task \em must end.
    * \c handler should check to see if this is set,
    * aborting or completing its work as soon as possible afterward.
    * \c callback and \c cleanup will still be called as normal.
    *
    * @see task_queue_reset
    */
   RETRO_TASK_FLG_CANCELLED        = (1 << 2),
   /**
    * If set, the task queue will not call \c progress_cb
    * and will not display any messages from this task.
    */
   RETRO_TASK_FLG_MUTE             = (1 << 3)
};

/**
 * A unit of work executed by the task system,
 * spread across one or more frames.
 *
 * Some fields are set by the caller,
 * others are set by the task system.
 */
struct retro_task
{
   /**
    * The time (in microseconds) when the task should start running,
    * or 0 if it should start as soon as possible.
    * The exact time this task starts will depend on when the queue is next updated.
    * Set by the caller.
    * @note This is a point in time, not a duration.
    * It is not affected by a frontend's time manipulation (pausing, fast-forward, etc.).
    * @see cpu_features_get_time_usec
    */
   retro_time_t when;

   /**
    * The main body of work for a task.
    * Should be as fast as possible,
    * as it will be called with each task queue update
    * (usually once per frame).
    * Must not be \c NULL.
    *
    * @param task The task that this handler is associated with.
    * Can be used to configure or query the task.
    * Will never be \c NULL.
    * @see task_queue_check
    */
   retro_task_handler_t  handler;

   /**
    * Called when this task finishes;
    * executed during the next task queue update
    * after \c finished is set to \c true.
    * May be \c NULL, in which case this function is skipped.
    *
    * @param task The task that is being cleaned up.
    * Will never be \c NULL.
    * @param task_data \c task's \c task_data field.
    * @param user_data \c task's \c user_data field.
    * @param error \c task's \c error field.
    * @see task_queue_check
    * @see retro_task_callback_t
    */
   retro_task_callback_t callback;

   /**
    * Called when this task finishes immediately after \c callback is run.
    * Used to clean up any resources or state owned by the task.
    * May be \c NULL, in which case this function is skipped.
    *
    * @param task The task that is being cleaned up.
    * Will never be \c NULL.
    */
   retro_task_handler_t cleanup;

   /**
    * Pointer to arbitrary data, intended for "returning" an object from the task
    * (although it can be used for any purpose).
    * If owned by the task, it should be cleaned up within \c cleanup.
    * Not modified or freed by the task queue.
    */
   void *task_data;

   /**
    * Pointer to arbitrary data, intended for passing parameters to the task.
    * If owned by the task, it should be cleaned up within \c cleanup.
    * Not modified or freed by the task queue.
    */
   void *user_data;

   /**
    * Pointer to arbitrary data, intended for state that exists for the task's lifetime.
    * If owned by the task, it should be cleaned up within \c cleanup.
    * Not modified or freed by the task queue.
    */
   void *state;

   /**
    * Human-readable details about an error that occurred while running the task.
    * Should be created and assigned within \c handler if there was an error.
    * Will be cleaned up by the task queue with \c free() upon this task's completion.
    * @see task_set_error
    */
   char *error;

   /**
    * Called to update a task's \c progress,
    * or to update some view layer (e.g. an on-screen display)
    * about the task's progress.
    *
    * Skipped if \c NULL or if \c mute is set.
    *
    * @param task The task whose progress is being updated or reported.
    * @see progress
    */
   void (*progress_cb)(retro_task_t*);

   /**
    * Human-readable description of this task.
    * May be \c NULL,
    * but if not then it will be disposed of by the task system with \c free()
    * upon this task's completion.
    * Can be modified or replaced at any time.
    * @see strdup
    */
   char *title;

   /**
    * Pointer to arbitrary data, intended for use by a frontend
    * (for example, to associate a sticky notification with a task).
    *
    * This should be cleaned up within \c cleanup if necessary.
    * Cores may use this for any purpose.
    */
   void *frontend_userdata;

   /**
    * @private Pointer to the next task in the queue.
    * Do not touch this; it is managed by the task system.
    */
   retro_task_t *next;

   /**
    * Indicates the current progress of the task.
    *
    * -1 means the task is indefinite or not measured,
    * 0-100 is a percentage of the task's completion.
    *
    * Set by the caller.
    *
    * @see progress_cb
    */
   int8_t progress;

   /**
    * A unique identifier assigned to a task when it's created.
    * Set by the task system.
    */
   uint32_t ident;

   /**
    * The type of task this is.
    * Set by the caller.
    */
   enum task_type type;
   enum task_style style;

   uint8_t flags;
};

/**
 * Parameters for \c task_queue_find.
 *
 * @see task_queue_find
 */
typedef struct task_finder_data
{
   /**
    * Predicate to call for each task.
    * Must not be \c NULL.
    *
    * @param task The task to query.
    * @param userdata \c userdata from this struct.
    * @return \c true if this task matches the search criteria,
    * at which point \c task_queue_find will stop searching and return \c true.
    */
   retro_task_finder_t func;

   /**
    * Pointer to arbitrary data.
    * Passed directly to \c func.
    * May be \c NULL.
    */
   void *userdata;
} task_finder_data_t;

/**
 * Contains the result of a call to \c task_retriever_data::func.
 * Implemented as an intrusive singly-linked list.
 */
typedef struct task_retriever_info
{
   /**
    * The next item in the result list,
    * or \c NULL if this is the last one.
    */
   struct task_retriever_info *next;

   /**
    * Arbitrary data returned by \c func.
    * Can be anything, but should be a simple \c struct
    * so that it can be freed by \c task_queue_retriever_info_free.
    */
   void *data;
} task_retriever_info_t;

/**
 * Parameters for \c task_queue_retrieve.
 *
 * @see task_queue_retrieve
 */
typedef struct task_retriever_data
{
   /**
    * Contains the result of each call to \c func that returned \c true.
    * Should be initialized to \c NULL.
    * Will remain \c NULL after \c task_queue_retrieve if no tasks matched.
    * Must be freed by \c task_queue_retriever_info_free.
    * @see task_queue_retriever_info_free
    */
   task_retriever_info_t *list;

   /**
    * The handler to compare against.
    * Only tasks with this handler will be considered for retrieval.
    * Must not be \c NULL.
    */
   retro_task_handler_t handler;

   /**
    * The predicate that determines if the given task will be retrieved.
    * Must not be \c NULL.
    *
    * @param task[in] The task to query.
    * @param data[out] Arbitrary data that the retriever should return.
    * Allocated by the task queue based on \c element_size.
    * @return \c true if \c data should be appended to \c list.
    */
   retro_task_retriever_t func;

   /**
    * The size of the output that \c func may write to.
    * Must not be zero.
    */
   size_t element_size;
} task_retriever_data_t;

/**
 * Returns the next item in the result list.
 * Here's a usage example, assuming that \c results is used to store strings:
 *
 * @code{.c}
 * void print_results(task_retriever_info_t *results)
 * {
 *    char* text = NULL;
 *    task_retriever_info_t *current = results;
 *    while (text = task_queue_retriever_info_next(&current))
 *    {
 *       printf("%s\n", text);
 *    }
 * }
 * @endcode
 *
 * @param link Pointer to the first element in the result list.
 * Must not be \c NULL.
 * @return The next item in the result list.
 */
void *task_queue_retriever_info_next(task_retriever_info_t **link);

/**
 * Frees the result of a call to \c task_queue_retrieve.
 * @param list The result list to free.
 * May be \c NULL, in which case this function does nothing.
 * The list, its nodes, and its elements will all be freed.
 *
 * If the list's elements must be cleaned up with anything besides \c free,
 * then the caller must do that itself before invoking this function.
 */
void task_queue_retriever_info_free(task_retriever_info_t *list);

/**
 * Cancels the provided task.
 * The task should finish its work as soon as possible afterward.
 *
 * @param task The task to cancel.
 * @see task_set_cancelled
 */
void task_queue_cancel_task(void *task);

void task_set_flags(retro_task_t *task, uint8_t flags, bool set);

/**
 * Sets \c task::error to the given value.
 * Thread-safe if the task queue is threaded.
 *
 * @param task The task to modify.
 * Behavior is undefined if \c NULL.
 * @param error The error message to set.
 * @see retro_task::error
 * @warning This does not free the existing error message.
 * The caller must do that itself.
 */
void task_set_error(retro_task_t *task, char *error);

/**
 * Sets \c task::progress to the given value.
 * Thread-safe if the task queue is threaded.
 *
 * @param task The task to modify.
 * Behavior is undefined if \c NULL.
 * @param progress The progress value to set.
 * @see retro_task::progress
 */
void task_set_progress(retro_task_t *task, int8_t progress);

/**
 * Sets \c task::title to the given value.
 * Thread-safe if the task queue is threaded.
 *
 * @param task The task to modify.
 * Behavior is undefined if \c NULL.
 * @param title The title to set.
 * @see retro_task::title
 * @see task_free_title
 * @warning This does not free the existing title.
 * The caller must do that itself.
 */
void task_set_title(retro_task_t *task, char *title);

/**
 * Sets \c task::data to the given value.
 * Thread-safe if the task queue is threaded.
 *
 * @param task The task to modify.
 * Behavior is undefined if \c NULL.
 * @param data The data to set.
 * @see retro_task::data
 */
void task_set_data(retro_task_t *task, void *data);

/**
 * Frees the \c task's title, if any.
 * Thread-safe if the task queue is threaded.
 *
 * @param task The task to modify.
 * @see task_set_title
 */
void task_free_title(retro_task_t *task);

/**
 * Returns \c task::error.
 * Thread-safe if the task queue is threaded.
 *
 * @param task The task to query.
 * Behavior is undefined if \c NULL.
 * @return The value of \c task::error.
 * @see retro_task::error
 */
char* task_get_error(retro_task_t *task);

/**
 * Returns \c task::progress.
 * Thread-safe if the task queue is threaded.
 *
 * @param task The task to query.
 * Behavior is undefined if \c NULL.
 * @return The value of \c task::progress.
 * @see retro_task::progress
 */
int8_t task_get_progress(retro_task_t *task);

/**
 * Returns \c task::title.
 * Thread-safe if the task queue is threaded.
 *
 * @param task The task to query.
 * Behavior is undefined if \c NULL.
 * @return The value of \c task::title.
 * @see retro_task::title
 */
char* task_get_title(retro_task_t *task);

/**
 * Returns \c task::data.
 * Thread-safe if the task queue is threaded.
 *
 * @param task The task to query.
 * Behavior is undefined if \c NULL.
 * @return The value of \c task::data.
 * @see retro_task::data
 */
void* task_get_data(retro_task_t *task);

/**
 * Returns whether the task queue is running
 * on the same thread that called \c task_queue_init.
 *
 * @return \c true if the caller is running
 * on the same thread that called \c task_queue_init.
 */
bool task_is_on_main_thread(void);

/**
 * Ensures that the task queue is in threaded mode.
 *
 * Next time \c retro_task_queue_check is called,
 * the task queue will be recreated with threading enabled.
 * Existing tasks will continue to run on the new thread.
 */
void task_queue_set_threaded(void);

/**
 * Ensures that the task queue is not in threaded mode.
 *
 * Next time \c retro_task_queue_check is called,
 * the task queue will be recreated with threading disabled.
 * Existing tasks will continue to run on whichever thread updates the queue.
 *
 * @see task_queue_set_threaded
 * @see task_queue_is_threaded
 */
void task_queue_unset_threaded(void);

/**
 * Returns whether the task queue is running in threaded mode.
 *
 * @return \c true if the task queue is running its tasks on a separate thread.
 */
bool task_queue_is_threaded(void);

/**
 * Calls the function given in \c find_data for each task
 * until it returns \c true for one of them,
 * or until all tasks have been searched.
 *
 * @param find_data Parameters for the search.
 * Behavior is undefined if \c NULL.
 * @return \c true if \c find_data::func returned \c true for any task.
 * @see task_finder_data_t
 */
bool task_queue_find(task_finder_data_t *find_data);

/**
 * Retrieves arbitrary data from every task
 * whose handler matches \c data::handler.
 *
 * @param data[in, out] Parameters for retrieving data from the task queue,
 * including the results themselves.
 * Behavior is undefined if \c NULL.
 * @see task_retriever_data_t
 */
void task_queue_retrieve(task_retriever_data_t *data);

 /**
  * Runs each task.
  * If a task is finished or cancelled,
  * its callback and cleanup handler will be called.
  * Afterwards, the task will be deallocated.
  * If used in a core, generally called in \c retro_run
  * and just before \c task_queue_deinit.
  * @warning This must only be called from the main thread.
  */
void task_queue_check(void);

uint8_t task_get_flags(retro_task_t *task);

/**
 * Schedules a task to start running.
 * If \c task::when is 0, it will start as soon as possible.
 *
 * Tasks with the same \c task::when value
 * will be executed in the order they were scheduled.
 *
 * @param task The task to schedule.
 * @return \c true unless \c task's type is \c TASK_TYPE_BLOCKING
 * and there's already a blocking task in the queue.
 */
bool task_queue_push(retro_task_t *task);

/**
 * Block until all active (i.e. current time > \c task::when) tasks have finished,
 * or until the given function returns \c false.
 * If a scheduled task's \c when is passed while within this function,
 * it will start executing.
 *
 * Must only be called from the main thread.
 *
 * @param cond Function to call after all tasks in the queue have executed
 * (i.e. once per pass over the task queue).
 * May be \c NULL, in which case the task queue will wait unconditionally.
 * @param data Pointer to arbitrary data, passed directly into \c cond.
 * May be \c NULL.
 * @see retro_task_condition_fn_t
 * @see task_queue_deinit
 * @see task_queue_reset
 * @warning Passing \c NULL to \c cond is strongly discouraged.
 * If you use tasks that run indefinitely
 * (e.g. for the lifetime of the core),
 * you will need a way to stop these tasks externally;
 * otherwise, you risk the frontend and core freezing.
 */
void task_queue_wait(retro_task_condition_fn_t cond, void* data);

/**
 * Marks all tasks in the queue as cancelled.
 *
 * The tasks won't immediately be terminated;
 * each task may finish its work,
 * but must do so as quickly as possible.
 *
 * Must only be called from the main thread.
 *
 * @see task_queue_wait
 * @see task_queue_deinit
 * @see task_set_finished
 * @see task_get_cancelled
 */
void task_queue_reset(void);

/**
 * Deinitializes the task system.
 *
 * Outstanding tasks will not be cleaned up;
 * if the intent is to finish the core or frontend's work,
 * then all tasks must be finished before calling this function.
 * May be safely called multiple times in a row,
 * but only from the main thread.
 * @see task_queue_wait
 * @see task_queue_reset
 */
void task_queue_deinit(void);

/**
 * Initializes the task system with the provided parameters.
 * Must be called before any other task_queue_* function,
 * and must only be called from the main thread.
 *
 * @param threaded \c true if tasks should run on a separate thread,
 * \c false if they should remain on the calling thread.
 * All tasks run in sequence on a single thread.
 * If you want to scale a task to multiple threads,
 * you must do so within the task itself.
 * @param msg_push The task system will call this function to output messages.
 * If \c NULL, no messages will be output.
 * @note Calling this function while the task system is already initialized
 * will reinitialize it with the new parameters,
 * but it will not reset the task queue;
 * all existing tasks will continue to run
 * when the queue is updated.
 * @see task_queue_deinit
 * @see retro_task_queue_msg_t
 */
void task_queue_init(bool threaded, retro_task_queue_msg_t msg_push);

/**
 * Allocates and initializes a new task.
 * Deallocated by the task queue after it finishes executing.
 *
 * @returns Pointer to a newly allocated task,
 * or \c NULL if allocation fails.
 */
retro_task_t *task_init(void);

RETRO_END_DECLS

#endif
