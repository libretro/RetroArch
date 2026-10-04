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

static void *checked_calloc(size_t count, size_t size)
{
   void *ptr = checked_malloc(count * size);
   if (ptr)
      memset(ptr, 0, count * size);
   return ptr;
}

#define malloc checked_malloc
#define calloc checked_calloc
#define free checked_free
#define strdup checked_strdup
#include "../gfx/gfx_widgets.c"
#undef malloc
#undef calloc
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

static void push(gfx_widgets_task_cmd_t *cmd)
{
   gfx_widgets_task_cmd_apply(&dispwidget_st, cmd);
}

static void release_cmd(gfx_widgets_task_cmd_t *cmd)
{
   checked_free(cmd->title);
   checked_free(cmd->error);
   cmd->title = NULL;
   cmd->error = NULL;
}

/* The waiting task widget goes on screen, where updates find it. */
static disp_widget_msg_t *take_widget(void)
{
   disp_widget_msg_t *widget;
   if (!dispwidget_st.task_pending_size)
      return NULL;
   widget = dispwidget_st.task_pending[0];
   dispwidget_st.task_pending_size = 0;
   dispwidget_st.current_msgs[0]   = widget;
   dispwidget_st.current_msgs_size = 1;
   return widget;
}

static void release_widget(disp_widget_msg_t *widget)
{
   gfx_widgets_msg_queue_free(&dispwidget_st, widget);
   assert(pending_count == 0);
   dispwidget_st.current_msgs[0]   = NULL;
   dispwidget_st.current_msgs_size = 0;
   checked_free(widget);
}

int main(void)
{
   gfx_widgets_task_cmd_t cmd;
   disp_widget_msg_t *widget;
   char *text;
   unsigned before;
   unsigned i;
   memset(&cmd, 0, sizeof(cmd));
   cmd.key      = 2;
   cmd.ident    = 1;
   cmd.duration = 60;
   cmd.category = MESSAGE_QUEUE_CATEGORY_INFO;

   /* Neither widget allocation failure nor duplicate failure makes a
    * widget or consumes the update's text. */
   for (i = 1; i <= 2; i++)
   {
      cmd.title = checked_strdup("initial");
      text = cmd.title;
      fail_after = i;
      push(&cmd);
      assert(cmd.title == text);
      assert(!take_widget());
      release_cmd(&cmd);
      assert(live_allocations == 0);
   }

   cmd.title = checked_strdup("initial");
   text = cmd.title;
   before = duplicate_calls;
   push(&cmd);
   assert(duplicate_calls == before + 1);
   assert(!cmd.title);
   widget = take_widget();
   assert(widget && widget->task_key == cmd.key);
   assert(widget->msg == text);
   assert(strcmp(widget->msg_new, "initial") == 0);

   cmd.title = checked_strdup("initial");
   text = cmd.title;
   before = duplicate_calls;
   push(&cmd);
   assert(cmd.title == text && duplicate_calls == before);
   release_cmd(&cmd);

   /* A movement tween must survive replacing the title tween. */
   memset(&pending[0], 0, sizeof(pending[0]));
   pending[0].tag = (uintptr_t)widget;
   pending_count = 1;

   /* Changed titles and animation completion need no new allocation. */
   for (i = 0; i < 2; i++)
   {
      cmd.title = checked_strdup(i ? "latest" : "replacement");
      text = cmd.title;
      before = duplicate_calls;
      fail_after = 1;
      push(&cmd);
      assert(!cmd.title && widget->msg_new == text);
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
   cmd.title = checked_strdup("latest");
   before = animation_calls;
   push(&cmd);
   assert(cmd.title && !widget->msg_new);
   assert(widget->msg == text && animation_calls == before);
   release_cmd(&cmd);

   /* A missing title must still publish the final task state. */
   cmd.task_flags = RETRO_TASK_FLG_FINISHED;
   cmd.progress = 100;
   push(&cmd);
   assert(widget->flags & DISPWIDG_FLAG_TASK_FINISHED);
   assert(widget->task_progress == 100);
   assert(strcmp(widget->msg, "latest") == 0);

   /* Another task taking the widget over starts it afresh. */
   cmd.task_flags = 0;
   cmd.ident      = 7;
   cmd.rebind     = true;
   push(&cmd);
   assert(!(widget->flags & DISPWIDG_FLAG_TASK_FINISHED));
   assert(widget->task_ident == 7 && widget->task_progress == 100);
   cmd.rebind     = false;
   cmd.ident      = 1;
   release_widget(widget);
   assert(live_allocations == 0);

   /* Error text is transferred instead of the unused title. */
   cmd.title = checked_strdup("title");
   cmd.error = checked_strdup("error");
   text = cmd.error;
   push(&cmd);
   widget = take_widget();
   assert(widget && widget->msg == text);
   assert(!cmd.error && cmd.title);
   assert(widget->flags & DISPWIDG_FLAG_TASK_ERROR);
   release_cmd(&cmd);
   release_widget(widget);
   assert(live_allocations == 0);

   /* Alternative-look widgets complete the transition synchronously. */
   cmd.task_flags = RETRO_TASK_FLG_ALTERNATIVE_LOOK;
   cmd.title = checked_strdup("alternative");
   push(&cmd);
   widget = take_widget();
   assert(widget && widget->alternative_look);
   cmd.title = checked_strdup("immediate");
   text = cmd.title;
   before = duplicate_calls;
   push(&cmd);
   assert(!cmd.title && !widget->msg_new && widget->msg == text);
   assert(duplicate_calls == before);
   cmd.title = checked_strdup("immediate");
   push(&cmd);
   assert(cmd.title && !widget->msg_new && widget->msg == text);
   release_cmd(&cmd);
   cmd.title = checked_strdup("changed again");
   text = cmd.title;
   push(&cmd);
   assert(!cmd.title && !widget->msg_new && widget->msg == text);
   release_widget(widget);
   assert(live_allocations == 0);

   /* Teardown also cancels an unfinished title transition. */
   cmd.task_flags = 0;
   cmd.title = checked_strdup("before teardown");
   push(&cmd);
   widget = take_widget();
   assert(widget);
   cmd.title = checked_strdup("pending teardown");
   push(&cmd);
   assert(pending_count == 1);
   release_widget(widget);
   assert(live_allocations == 0);

   /* A muted task, or one with no title, makes no widget. */
   cmd.task_flags = RETRO_TASK_FLG_MUTE;
   cmd.title = checked_strdup("muted");
   push(&cmd);
   assert(cmd.title && !take_widget());
   release_cmd(&cmd);
   cmd.task_flags = RETRO_TASK_FLG_FINISHED;
   push(&cmd);
   assert(!take_widget() && live_allocations == 0);

   /* With no room to wait in, nothing is made and the text is left. */
   cmd.task_flags = 0;
   {
      static disp_widget_msg_t filler;
      for (i = 0; i < MSG_QUEUE_PENDING_MAX; i++)
         dispwidget_st.task_pending[i] = &filler;
   }
   dispwidget_st.task_pending_size = MSG_QUEUE_PENDING_MAX;
   cmd.title = checked_strdup("full");
   text = cmd.title;
   push(&cmd);
   assert(cmd.title == text);
   release_cmd(&cmd);
   assert(live_allocations == 0);
   dispwidget_st.task_pending_size = 0;
   puts("widget snapshot ownership tests passed");
   return 0;
}
