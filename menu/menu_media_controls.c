/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

#include <stdio.h>
#include <string.h>
#include <string/stdstring.h>

#include "menu_media_controls.h"
#include "../gfx/video_defines.h"
#include "../configuration.h"

bool menu_media_controls_update(menu_media_controls_t *media,
      const gfx_media_target_t *target, unsigned width, unsigned height,
      const menu_input_pointer_t *pointer, int64_t now)
{
   settings_t *settings = config_get_ptr();
   bool subtitle_changed = false;
   gfx_media_controls_configure(&media->controls,
         settings->uints.media_player_seek_seconds,
         settings->uints.media_player_controls_timeout);
   if (gfx_media_controls_update(&media->controls, target, width, height, now))
   {
      bool valid = pointer && pointer->type != MENU_POINTER_DISABLED
         && !(pointer->flags & MENU_INP_PTR_FLG_RESET);
      if (pointer && (pointer->flags & MENU_INP_PTR_FLG_RESET))
         gfx_media_controls_cancel(&media->controls);
      gfx_media_controls_pointer(&media->controls,
            pointer ? VIDEO_POS_X(pointer->pos) : 0,
            pointer ? VIDEO_POS_Y(pointer->pos) : 0,
            pointer && (pointer->flags & MENU_INP_PTR_FLG_PRESSED), valid, now);
   }
   {
      bool changed = false;
      bool seeking = media->controls.playback.seeking;
      const char *path = media->controls.target.source_path;
      enum image_type_enum type = path ? image_texture_get_type(path) : IMAGE_TYPE_NONE;
      bool enabled = settings->bools.media_player_subtitles
         && (type == IMAGE_TYPE_MP4 || type == IMAGE_TYPE_WEBM);
      if (enabled && !media->subtitle)
         media->subtitle = gfx_media_subtitle_new();
      if (media->subtitle)
         changed = gfx_media_subtitle_update(media->subtitle, media->controls.target.identity,
               path, media->controls.playback.position_us,
               (int64_t)settings->ints.media_player_subtitle_delay * 1000, enabled);
      subtitle_changed = changed || seeking != media->subtitle_seeking;
      if (subtitle_changed)
         gfx_media_subtitle_publish(&media->subtitle_mailbox,
               seeking ? NULL : gfx_media_subtitle_view(media->subtitle));
      media->subtitle_seeking = seeking;
   }
   gfx_media_controls_publish(&media->controls, &media->mailbox);
   return subtitle_changed;
}

void menu_media_controls_thumbnail(menu_media_controls_t *media,
      gfx_thumbnail_t *primary, gfx_thumbnail_t *secondary, bool shown,
      unsigned width, unsigned height, const menu_input_pointer_t *pointer,
      int64_t now)
{
   gfx_media_target_t target;
   gfx_media_playback_state_t state;
   memset(&target, 0, sizeof(target));
   if (shown)
   {
      if (gfx_thumbnail_get_playback_state(primary, now, &state))
         gfx_media_controls_thumbnail_target(primary, &target);
      else if (gfx_thumbnail_get_playback_state(secondary, now, &state))
         gfx_media_controls_thumbnail_target(secondary, &target);
   }
   menu_media_controls_update(media, &target, width, height, pointer, now);
}

bool menu_media_controls_action(menu_media_controls_t *media,
      enum menu_action action, int64_t now)
{
   enum gfx_media_action command;
   if (!media->controls.target.user || menu_input_dialog_get_display_kb())
      return false;
   switch (action)
   {
      case MENU_ACTION_OK: command = GFX_MEDIA_ACTIVATE; break;
      case MENU_ACTION_LEFT: command = GFX_MEDIA_PREVIOUS; break;
      case MENU_ACTION_RIGHT: command = GFX_MEDIA_NEXT; break;
      case MENU_ACTION_DOWN: command = GFX_MEDIA_FOCUS; break;
      case MENU_ACTION_UP: command = GFX_MEDIA_UNFOCUS; break;
      case MENU_ACTION_START: command = GFX_MEDIA_STOP; break;
      case MENU_ACTION_SCROLL_UP: command = GFX_MEDIA_SEEK_BACK; break;
      case MENU_ACTION_SCROLL_DOWN: command = GFX_MEDIA_SEEK_FORWARD; break;
      case MENU_ACTION_CANCEL:
      case MENU_ACTION_SELECT:
         gfx_media_controls_cancel(&media->controls);
         return false;
      default: return true;
   }
   gfx_media_controls_action(&media->controls, command, now);
   gfx_media_controls_publish(&media->controls, &media->mailbox);
   return true;
}

bool menu_media_controls_release(menu_media_controls_t *media,
      unsigned x, unsigned y, int64_t now)
{
   if (!media->controls.target.user) return false;
   gfx_media_controls_release(&media->controls, (int)x, (int)y, now);
   gfx_media_controls_publish(&media->controls, &media->mailbox);
   return true;
}

void menu_media_controls_cancel(menu_media_controls_t *media)
{
   gfx_media_subtitle_free(media->subtitle);
   media->subtitle = NULL;
   media->subtitle_seeking = false;
   gfx_media_subtitle_publish(&media->subtitle_mailbox, NULL);
   gfx_media_controls_reset(&media->controls);
   gfx_media_controls_publish(NULL, &media->mailbox);
}

static void menu_media_quad(gfx_display_t *display, void *userdata,
      unsigned dims, const gfx_media_rect_t *rect, float r, float g,
      float b, float a)
{
   float color[16];
   unsigned i;
   if (rect->w <= 0 || rect->h <= 0) return;
   for (i = 0; i < 4; i++)
   {
      color[4*i] = r; color[4*i+1] = g;
      color[4*i+2] = b; color[4*i+3] = a;
   }
   gfx_display_draw_quad(display, userdata, dims, rect->x, rect->y,
         VIDEO_SCALE_PACK(rect->w, rect->h), dims, color, NULL);
}

void menu_media_controls_time(char *out, size_t size, int64_t us)
{
   uint64_t seconds = us > 0 ? (uint64_t)us / 1000000 : 0;
   snprintf(out, size, "%u:%02u:%02u", (unsigned)(seconds / 3600),
         (unsigned)((seconds / 60) % 60), (unsigned)(seconds % 60));
}

static void menu_media_subtitle_draw(menu_media_controls_t *media,
      gfx_display_t *display, void *userdata, unsigned dims,
      const font_data_t *font)
{
   char wrapped[GFX_MEDIA_SUBTITLE_TEXT_SIZE];
   char *lines[8];
   char *line;
   unsigned count = 0, i;
   int width = VIDEO_SCALE_W(dims);
   int height = VIDEO_SCALE_H(dims);
   int line_height, glyph_width, max_width, bottom;
   float scale;
   gfx_media_subtitle_consume(&media->subtitle_mailbox, &media->subtitle_view);
   if (!media->subtitle_view.text[0] || !font || font->size <= 0.0f
         || width < 96 || height < 96)
      return;
   line_height = height / 28;
   if (line_height < 12)
      line_height = 12;
   scale = line_height * 0.8f / font->size;
   glyph_width = font_driver_get_message_width((void*)font, "M", 1, scale);
   if (glyph_width <= 0)
      return;
   max_width = width * 4 / 5;
   wrapped[0] = '\0';
   word_wrap_wideglyph(wrapped, sizeof(wrapped), media->subtitle_view.text,
         strlen(media->subtitle_view.text), max_width / glyph_width, 200, 8);
   line = wrapped;
   while (*line && count < ARRAY_SIZE(lines))
   {
      char *next = strchr(line, '\n');
      lines[count++] = line;
      if (!next)
         break;
      *next = '\0';
      line = next + 1;
   }
   bottom = media->view.visible ? media->view.layout.panel.y - line_height / 2
      : height - line_height;
   for (i = 0; i < count; i++)
   {
      gfx_media_rect_t background;
      int text_width = font_driver_get_message_width((void*)font, lines[i],
            strlen(lines[i]), scale);
      int y = bottom - ((int)count - (int)i) * line_height;
      float line_scale = scale;
      if (y < line_height)
         continue;
      if (text_width > max_width)
      {
         line_scale *= (float)max_width / text_width;
         text_width = max_width;
      }
      background.x = (width - text_width) / 2 - line_height / 4;
      background.y = y;
      background.w = text_width + line_height / 2;
      background.h = line_height;
      menu_media_quad(display, userdata, dims, &background,
            0.0f, 0.0f, 0.0f, 0.75f);
      gfx_display_draw_text(font, lines[i], width * 0.5f,
            y + line_height * 0.8f, dims, 0xffffffffU,
            TEXT_ALIGN_CENTER, line_scale, false, 0.0f, false);
   }
}

void menu_media_controls_draw(menu_media_controls_t *media,
      gfx_display_t *display, void *userdata, unsigned dims,
      const font_data_t *font)
{
   const gfx_media_view_t *view;
   gfx_media_rect_t bar;
   char elapsed[24], duration[24], caption[56];
   char seek_back[16], seek_forward[16];
   const char *labels[4];
   unsigned i;
   int64_t position;
   float scale;
   gfx_media_controls_consume(&media->mailbox, &media->view);
   view = &media->view;
   menu_media_subtitle_draw(media, display, userdata, dims, font);
   if (!view->visible || !view->layout.unit) return;
   menu_media_quad(display, userdata, dims, &view->layout.panel, 0.025f, 0.025f, 0.025f, 0.92f);
   bar = view->layout.timeline;
   menu_media_quad(display, userdata, dims, &bar, 0.3f, 0.3f, 0.3f, 1.0f);
   position = view->dragging ? view->drag_us : view->playback.position_us;
   if (view->playback.duration_us > 0)
   {
      double fraction = (double)position / (double)view->playback.duration_us;
      if (fraction < 0.0) fraction = 0.0;
      if (fraction > 1.0) fraction = 1.0;
      bar.w = (int)(bar.w * fraction);
      menu_media_quad(display, userdata, dims, &bar, 0.1f, 0.8f, 0.8f, 1.0f);
   }
   if (!font || font->size <= 0.0f) return;
   scale = view->layout.unit * 0.75f / font->size;
   menu_media_controls_time(elapsed, sizeof(elapsed), position);
   menu_media_controls_time(duration, sizeof(duration), view->playback.duration_us);
   snprintf(caption, sizeof(caption), "%s / %s", elapsed, duration);
   gfx_display_draw_text(font, caption, (float)view->layout.timeline.x,
         (float)(view->layout.panel.y + view->layout.unit), dims,
         0xffffffffU, TEXT_ALIGN_LEFT, scale, false, 0.0f, false);
   labels[0] = view->playback.paused ? ">" : "||";
   snprintf(seek_back, sizeof(seek_back), "-%u", view->seek_seconds);
   snprintf(seek_forward, sizeof(seek_forward), "+%u", view->seek_seconds);
   labels[1] = seek_back; labels[2] = seek_forward; labels[3] = "[]";
   for (i = 0; i < 4; i++)
   {
      const gfx_media_rect_t *button = &view->layout.buttons[i];
      bool enabled = !i || view->playback.seekable;
      bool focused = (view->row_focus && view->focus == i) || view->hover == (int)i;
      menu_media_quad(display, userdata, dims, button,
            focused ? 0.05f : 0.16f, focused ? 0.45f : 0.16f,
            focused ? 0.45f : 0.16f, 1.0f);
      gfx_display_draw_text(font, labels[i], button->x + button->w * 0.5f,
            button->y + button->h * 0.8f, dims,
            enabled ? 0xffffffffU : 0x808080ffU, TEXT_ALIGN_CENTER,
            scale, false, 0.0f, false);
   }
}
