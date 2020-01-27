/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018      - natinusala
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

#ifndef _MENU_WIDGETS_H
#define _MENU_WIDGETS_H

#include <formats/image.h>
#include <queues/task_queue.h>
#include <queues/message_queue.h>

#define DEFAULT_BACKDROP               0.75f

#define MSG_QUEUE_PENDING_MAX          32
#define MSG_QUEUE_ONSCREEN_MAX         4

#define MSG_QUEUE_ANIMATION_DURATION      330
#define VOLUME_DURATION                   3000
#define SCREENSHOT_DURATION_IN            66
#define SCREENSHOT_DURATION_OUT           SCREENSHOT_DURATION_IN*10
#define SCREENSHOT_NOTIFICATION_DURATION  6000
#define CHEEVO_NOTIFICATION_DURATION      4000
#define TASK_FINISHED_DURATION            3000
#define HOURGLASS_INTERVAL                5000
#define HOURGLASS_DURATION                1000
#define GENERIC_MESSAGE_DURATION          3000

bool menu_widgets_init(bool video_is_threaded);

void menu_widgets_free(void);

void menu_widgets_msg_queue_push(
      retro_task_t *task, const char *msg,
      unsigned duration,
      char *title,
      enum message_queue_icon icon,
      enum message_queue_category category,
      unsigned prio, bool flush);

void menu_widgets_volume_update_and_show(void);

void menu_widgets_iterate(unsigned width, unsigned height);

void menu_widgets_screenshot_taken(const char *shotname, const char *filename);

/* AI Service functions */
int menu_widgets_ai_service_overlay_get_state(void);
bool menu_widgets_ai_service_overlay_set_state(int state);

bool menu_widgets_ai_service_overlay_load(
        char* buffer, unsigned buffer_len,
        enum image_type_enum image_type);

void menu_widgets_ai_service_overlay_unload(void);

void menu_widgets_start_load_content_animation(
      const char *content_name, bool remove_extension);

void menu_widgets_cleanup_load_content_animation(void);

void menu_widgets_context_reset(bool is_threaded,
      unsigned width, unsigned height);

void menu_widgets_context_destroy(void);

void menu_widgets_push_achievement(const char *title, const char *badge);

/* Warning: not thread safe! */
void menu_widgets_set_message(char *message);

/* Warning: not thread safe! */
void menu_widgets_set_libretro_message(const char *message, unsigned duration);

/* All the functions below should be called in
 * the video driver - once they are all added, set
 * enable_menu_widgets to true for that driver */
void menu_widgets_frame(void *data);

bool menu_widgets_set_fps_text(const char *new_fps_text);

#endif
