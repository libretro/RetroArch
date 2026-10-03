/* Use the production renderer and cleanup with display/animation stubs. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../gfx/gfx_widgets.c"

static unsigned failures;

#define CHECK(test, message) \
   do { if (!(test)) { puts(message); failures++; } } while (0)

static void draw_and_free(int8_t progress, bool alternative,
      uint32_t flags)
{
   gfx_display_t display;
   gfx_display_ctx_driver_t context;
   disp_widget_msg_t widget;
   retro_task_t task;
   bool finished = (flags & DISPWIDG_FLAG_TASK_FINISHED) != 0;

   memset(&display, 0, sizeof(display));
   memset(&context, 0, sizeof(context));
   memset(&widget, 0, sizeof(widget));
   memset(&task, 0, sizeof(task));
   widget.msg              = strdup("task");
   if (!widget.msg)
      abort();
   widget.msg_len          = 4;
   widget.width            = 40;
   widget.alpha            = 1.0f;
   widget.task_progress    = progress;
   widget.alternative_look = alternative;
   widget.flags            = flags | DISPWIDG_FLAG_TASK | DISPWIDG_FLAG_SMALL;
   widget.task_ptr         = &task;

   /* Model memory no longer belonging to the retired task. Cleanup
    * must not write through task_ptr after the finished publication. */
   task.frontend_userdata  = &task;
   dispwidget_st.msg_queue_height = 32;
   dispwidget_st.gfx_widget_fonts.msg_queue.glyph_width = 10;
   gfx_widgets_draw_task_msg(&dispwidget_st, &display, &context,
         &widget, NULL, VIDEO_SCALE_PACK(640, 480), 0);

   CHECK(((widget.flags & DISPWIDG_FLAG_TASK_FINISHED) != 0) == finished,
         "renderer changed task lifetime state");
   CHECK(widget.task_progress == progress, "renderer changed progress");
   gfx_widgets_msg_queue_free(&dispwidget_st, &widget);
   CHECK(task.frontend_userdata == (finished ? (void*)&task : NULL),
         "cleanup accessed a retired task or failed to unlink a live task");
}

int main(void)
{
   static const int8_t progress[] = {-1, 0, 1, 37, 99, 100};
   static const uint32_t states[] = {
      DISPWIDG_FLAG_TASK_FINISHED,
      DISPWIDG_FLAG_TASK_FINISHED | DISPWIDG_FLAG_TASK_CANCELLED,
      DISPWIDG_FLAG_TASK_FINISHED | DISPWIDG_FLAG_TASK_ERROR
   };
   unsigned i, j, alternative;

   for (alternative = 0; alternative < 2; alternative++)
   {
      for (i = 0; i < sizeof(progress) / sizeof(progress[0]); i++)
         for (j = 0; j < sizeof(states) / sizeof(states[0]); j++)
            draw_and_free(progress[i], alternative != 0, states[j]);
      draw_and_free(37, alternative != 0, 0);
   }
   if (failures)
   {
      printf("%u widget lifetime checks failed\n", failures);
      return 1;
   }
   puts("widget task lifetime tests passed (38 cases)");
   return 0;
}
