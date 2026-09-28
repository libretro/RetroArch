/* Exercise the production message path with controlled allocation failures. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *allocations[64];
static unsigned live_allocations;
static unsigned duplicate_calls;
static unsigned animation_calls;
static unsigned fail_after;

static void *checked_malloc(size_t size)
{
   void *ptr;
   unsigned i;
   if (fail_after && --fail_after == 0)
      return NULL;
   ptr = malloc(size);
   assert(ptr);
   for (i = 0; i < 64; i++)
      if (!allocations[i])
      {
         allocations[i] = ptr;
         live_allocations++;
         return ptr;
      }
   abort();
}

static void checked_free(void *ptr)
{
   unsigned i;
   if (!ptr)
      return;
   for (i = 0; i < 64; i++)
      if (allocations[i] == ptr)
      {
         allocations[i] = NULL;
         live_allocations--;
         free(ptr);
         return;
      }
   assert(!"double free or untracked allocation");
}

static char *checked_strdup(const char *text)
{
   char *copy;
   size_t len;
   assert(text);
   duplicate_calls++;
   len = strlen(text) + 1;
   copy = (char*)checked_malloc(len);
   if (copy)
      memcpy(copy, text, len);
   return copy;
}

#define malloc checked_malloc
#define free checked_free
#define strdup checked_strdup
#include "../gfx/gfx_widgets.c"
#undef malloc
#undef free
#undef strdup

int font_driver_get_message_width(void *font, const char *text,
      size_t len, float scale)
{
   (void)font;
   (void)scale;
   assert(text && strlen(text) == len);
   return (int)len;
}

static gfx_animation_ctx_entry_t pending[8];
static unsigned pending_count;

bool gfx_animation_kill_widget_by_tag(uintptr_t *tag)
{
   unsigned i = 0;
   while (i < pending_count)
   {
      if (pending[i].tag == *tag)
         pending[i] = pending[--pending_count];
      else
         i++;
   }
   return true;
}

bool gfx_animation_push_widget(gfx_animation_ctx_entry_t *entry)
{
   assert(pending_count < 8);
   pending[pending_count++] = *entry;
   animation_calls++;
   return true;
}

static void push(retro_task_t *task, task_progress_snapshot_t *snapshot)
{
   gfx_widgets_msg_queue_push_state(task, snapshot, "message", 7, 60,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO,
         1, false, false);
}

static void release_snapshot(task_progress_snapshot_t *snapshot)
{
   checked_free(snapshot->title);
   checked_free(snapshot->error);
   snapshot->title = NULL;
   snapshot->error = NULL;
}

static void release_widget(disp_widget_msg_t *widget)
{
   gfx_widgets_msg_queue_free(&dispwidget_st, widget);
   assert(pending_count == 0);
   checked_free(widget);
}

int main(void)
{
   retro_task_t task;
   task_progress_snapshot_t snapshot;
   disp_widget_msg_t *widget;
   char *text;
   unsigned before;
   unsigned i;
   memset(&task, 0, sizeof(task));
   memset(&snapshot, 0, sizeof(snapshot));
   task.ident = 1;
   retro_atomic_int_init(&dispwidget_st.msg_queue_count, 0);

   /* Neither widget allocation failure nor duplicate failure publishes
    * a task association or consumes the caller's snapshot. */
   for (i = 1; i <= 2; i++)
   {
      snapshot.title = checked_strdup("initial");
      text = snapshot.title;
      fail_after = i;
      push(&task, &snapshot);
      assert(!task.frontend_userdata);
      assert(snapshot.title == text);
      assert(!gfx_widgets_pending_pop(&dispwidget_st));
      release_snapshot(&snapshot);
      assert(live_allocations == 0);
   }

   snapshot.title = checked_strdup("initial");
   text = snapshot.title;
   before = duplicate_calls;
   push(&task, &snapshot);
   assert(duplicate_calls == before + 1);
   assert(!snapshot.title);
   widget = gfx_widgets_pending_pop(&dispwidget_st);
   assert(widget && widget == task.frontend_userdata);
   assert(widget->msg == text);
   assert(strcmp(widget->msg_new, "initial") == 0);

   snapshot.title = checked_strdup("initial");
   text = snapshot.title;
   before = duplicate_calls;
   push(&task, &snapshot);
   assert(snapshot.title == text && duplicate_calls == before);
   release_snapshot(&snapshot);

   /* A movement tween must survive replacing the title tween. */
   memset(&pending[0], 0, sizeof(pending[0]));
   pending[0].tag = (uintptr_t)widget;
   pending_count = 1;

   /* Changed titles and animation completion need no new allocation. */
   for (i = 0; i < 2; i++)
   {
      snapshot.title = checked_strdup(i ? "latest" : "replacement");
      text = snapshot.title;
      before = duplicate_calls;
      fail_after = 1;
      push(&task, &snapshot);
      assert(!snapshot.title && widget->msg_new == text);
      assert(pending_count == 2);
      assert(pending[0].tag == (uintptr_t)widget);
      assert(duplicate_calls == before && fail_after == 1);
      fail_after = 0;
   }
   before = duplicate_calls;
   fail_after = 1;
   while (pending_count)
   {
      gfx_animation_ctx_entry_t entry = pending[--pending_count];
      if (entry.cb)
         entry.cb(entry.userdata);
   }
   assert(widget->msg == text && !widget->msg_new);
   assert(duplicate_calls == before && fail_after == 1);
   fail_after = 0;

   /* Completed transitions compare against the displayed title. */
   snapshot.title = checked_strdup("latest");
   before = animation_calls;
   push(&task, &snapshot);
   assert(snapshot.title && !widget->msg_new);
   assert(widget->msg == text && animation_calls == before);
   release_snapshot(&snapshot);

   /* A missing title must still publish the final task state. */
   snapshot.flags = RETRO_TASK_FLG_FINISHED;
   snapshot.progress = 100;
   push(&task, &snapshot);
   assert(widget->flags & DISPWIDG_FLAG_TASK_FINISHED);
   assert(widget->task_progress == 100);
   assert(strcmp(widget->msg, "latest") == 0);
   release_widget(widget);
   task.frontend_userdata = NULL;
   assert(live_allocations == 0);

   /* Error text is transferred instead of the unused title. */
   snapshot.flags = 0;
   snapshot.title = checked_strdup("title");
   snapshot.error = checked_strdup("error");
   text = snapshot.error;
   push(&task, &snapshot);
   widget = gfx_widgets_pending_pop(&dispwidget_st);
   assert(widget && widget->msg == text);
   assert(!snapshot.error && snapshot.title);
   assert(widget->flags & DISPWIDG_FLAG_TASK_ERROR);
   release_snapshot(&snapshot);
   release_widget(widget);
   assert(!task.frontend_userdata && live_allocations == 0);

   /* Alternative-look widgets complete the transition synchronously. */
   snapshot.flags = RETRO_TASK_FLG_ALTERNATIVE_LOOK;
   snapshot.title = checked_strdup("alternative");
   push(&task, &snapshot);
   widget = gfx_widgets_pending_pop(&dispwidget_st);
   assert(widget && widget->alternative_look);
   snapshot.title = checked_strdup("immediate");
   text = snapshot.title;
   before = duplicate_calls;
   push(&task, &snapshot);
   assert(!snapshot.title && !widget->msg_new && widget->msg == text);
   assert(duplicate_calls == before);
   snapshot.title = checked_strdup("immediate");
   push(&task, &snapshot);
   assert(snapshot.title && !widget->msg_new && widget->msg == text);
   release_snapshot(&snapshot);
   snapshot.title = checked_strdup("changed again");
   text = snapshot.title;
   push(&task, &snapshot);
   assert(!snapshot.title && !widget->msg_new && widget->msg == text);
   release_widget(widget);
   assert(!task.frontend_userdata && live_allocations == 0);

   /* Teardown also cancels an unfinished title transition. */
   snapshot.flags = 0;
   snapshot.title = checked_strdup("before teardown");
   push(&task, &snapshot);
   widget = gfx_widgets_pending_pop(&dispwidget_st);
   assert(widget);
   snapshot.title = checked_strdup("pending teardown");
   push(&task, &snapshot);
   assert(pending_count == 1);
   release_widget(widget);
   assert(!task.frontend_userdata && live_allocations == 0);

   /* A full pending ring rolls back the transferred text and task link. */
   retro_atomic_store_release_int(&dispwidget_st.msg_queue_count,
         MSG_QUEUE_PENDING_MAX);
   snapshot.title = checked_strdup("full");
   push(&task, &snapshot);
   assert(!snapshot.title && !task.frontend_userdata);
   release_snapshot(&snapshot);
   assert(live_allocations == 0);
   retro_atomic_store_release_int(&dispwidget_st.msg_queue_count, 0);
   puts("widget snapshot ownership tests passed");
   return 0;
}
