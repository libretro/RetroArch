/* The task queue's pushes against the widgets' owner.
 *
 * Under the threaded video wrapper the widgets are made, updated and
 * freed on the video thread. The main thread's gather never touches a
 * widget: each push sends what the task looks like to the owner, which
 * finds the widget by the key the task carries. The gather retires and
 * frees a task while the owner may still be applying its updates, so
 * an update holds nothing of the task's.
 *
 * Each round runs a titled task from its first push to its retirement
 * on the main thread while a second thread applies the updates and
 * frees the widgets, as the video thread does. Built under
 * ThreadSanitizer: anything the two sides share without ordering is a
 * reported race; under ASan, an update reaching into a task already
 * retired is a use after free. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../gfx/gfx_widgets.c"

#include <rthreads/rthreads.h>

#define ROUNDS 300

static video_driver_state_t fake_video_st;
static retro_atomic_int_t   stop;
static unsigned             pushes;
static unsigned             spawned;
static unsigned             passes;
static bool                 retired;

static void progress_handler(retro_task_t *task)
{
   task_set_progress(task, (int8_t)(passes * 30));
   if (++passes == 3)
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void retired_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   (void)task; (void)task_data; (void)user_data; (void)err;
   retired = true;
}

/* What runloop_task_msg_queue_push() does with widgets active */
static void msg_push(retro_task_t *task, const char *msg,
      unsigned prio, unsigned duration, bool flush)
{
   pushes++;
   gfx_widgets_msg_queue_push(task, msg, strlen(msg), duration, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO,
         prio, flush, false);
}

/* The owner's frame: apply what was sent; the widgets made go
 * straight back, as if shown and expired. */
static void owner_frame(void)
{
   gfx_widgets_state_lock();
   gfx_widgets_task_cmds_apply(&dispwidget_st);
   spawned += dispwidget_st.task_pending_size;
   gfx_widgets_task_pending_discard(&dispwidget_st);
   gfx_widgets_state_unlock();
}

static void owner_runs(void *unused)
{
   (void)unused;
   while (!retro_atomic_load_acquire_int(&stop))
      owner_frame();
}

int main(void)
{
   unsigned i;
   sthread_t *owner;

   dispwidget_st.state_lock            = slock_new();
   fake_video_st.thread_wrapper_active = true;
   dispwidget_st.video_st              = &fake_video_st;
   task_queue_init(false, msg_push);

   owner = sthread_create(owner_runs, NULL);
   if (!owner)
      return 1;

   for (i = 0; i < ROUNDS; i++)
   {
      retro_task_t *task = task_init();
      if (!task)
         return 1;
      task->handler      = progress_handler;
      task->callback     = retired_cb;
      task->title        = strdup("a task");
      passes             = 0;
      retired            = false;
      task_queue_push(task);
      while (!retired)
         task_queue_check();
   }

   retro_atomic_store_release_int(&stop, 1);
   sthread_join(owner);
   owner_frame();

   if (     !spawned
         || retro_atomic_load_acquire_int(&dispwidget_st.task_cmds_count))
   {
      printf("FAIL  %u widgets made, %d updates left unapplied\n", spawned,
            (int)retro_atomic_load_acquire_int(&dispwidget_st.task_cmds_count));
      return 1;
   }

   /* A muted task with a title and nothing on screen: its retirement
    * push carries no text and must not send for a widget. */
   {
      retro_task_t *task = task_init();
      if (!task)
         return 1;
      task->handler      = progress_handler;
      task->callback     = retired_cb;
      task->title        = strdup("muted");
      task->flags       |= RETRO_TASK_FLG_MUTE;
      passes             = 2;
      retired            = false;
      task_queue_push(task);
      while (!retired)
         task_queue_check();
      if (retro_atomic_load_acquire_int(&dispwidget_st.task_cmds_count))
      {
         printf("FAIL  a muted task's retirement sent for a widget\n");
         return 1;
      }
   }

   task_queue_deinit();
   slock_free(dispwidget_st.state_lock);
   printf("ok    %u tasks pushed and retired while their widgets were "
         "made and freed on another thread (%u pushes, %u widgets)\n",
         ROUNDS, pushes, spawned);
   return 0;
}
