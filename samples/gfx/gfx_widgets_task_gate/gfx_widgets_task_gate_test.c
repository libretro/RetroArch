/* The task queue's retirement push against the widgets' owner.
 *
 * Under the threaded video wrapper the widgets are freed on the video
 * thread, and a widget bound to a task that has not finished unlinks
 * itself there: it clears task->frontend_userdata, under the widgets'
 * state lock. The task queue's gather, on the main thread, decides
 * whether a finished task with nothing to say still needs a push to
 * retire its widget - and must not decide that by reading the same
 * field without that lock.
 *
 * Each round binds a widget to a muted task that finishes, then frees
 * the widget on a second thread, as the video thread does, while the
 * main thread's gather retires the task through the widgets' own push.
 * Built under ThreadSanitizer: an unguarded read of the link is a
 * reported race; under ASan, a widget writing into a task already
 * retired is a use after free. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../gfx/gfx_widgets.c"

#include <rthreads/rthreads.h>

#define ROUNDS 300

static video_driver_state_t fake_video_st;
static retro_atomic_int_t   go;
static unsigned             pushes;

static void finish_handler(retro_task_t *task)
{
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
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

static void owner_frees(void *data)
{
   disp_widget_msg_t *w = (disp_widget_msg_t*)data;
   while (!retro_atomic_load_acquire_int(&go)) { }
   gfx_widgets_state_lock();
   gfx_widgets_msg_queue_free(&dispwidget_st, w);
   gfx_widgets_state_unlock();
}

int main(void)
{
   unsigned i;

   dispwidget_st.state_lock          = slock_new();
   fake_video_st.thread_wrapper_active = true;
   dispwidget_st.video_st            = &fake_video_st;
   task_queue_init(false, msg_push);

   for (i = 0; i < ROUNDS; i++)
   {
      sthread_t *t;
      retro_task_t *task     = task_init();
      disp_widget_msg_t *w   = (disp_widget_msg_t*)calloc(1, sizeof(*w));
      if (!task || !w)
         return 1;
      task->handler          = finish_handler;
      task->flags           |= RETRO_TASK_FLG_MUTE;
      w->flags               = DISPWIDG_FLAG_TASK;
      w->task_ptr            = task;
      task->frontend_userdata = w;

      retro_atomic_store_release_int(&go, 0);
      t = sthread_create(owner_frees, w);
      task_queue_push(task);
      retro_atomic_store_release_int(&go, 1);
      task_queue_check();
      sthread_join(t);
      free(w);
   }

   /* A muted task with a title and nothing on screen: its retirement
    * push carries no text and must not put a widget up. */
   {
      retro_task_t *task = task_init();
      int before         = retro_atomic_load_acquire_int(
            &dispwidget_st.msg_queue_tail);
      if (!task)
         return 1;
      task->handler      = finish_handler;
      task->title        = strdup("muted");
      task->flags       |= RETRO_TASK_FLG_MUTE;
      task_queue_push(task);
      task_queue_check();
      if (retro_atomic_load_acquire_int(&dispwidget_st.msg_queue_tail)
            != before)
      {
         printf("FAIL  a muted task's retirement put up a widget\n");
         return 1;
      }
   }

   task_queue_deinit();
   slock_free(dispwidget_st.state_lock);
   printf("ok    %u tasks retired while their widgets were freed on another thread (%u pushes)\n",
         ROUNDS, pushes);
   return 0;
}
