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

#include "menu_media_controls.h"
#include "../gfx/video_defines.h"

void menu_media_controls_update(menu_media_controls_t *media,
      const gfx_media_target_t *target, unsigned width, unsigned height,
      const menu_input_pointer_t *pointer, int64_t now)
{
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
   gfx_media_controls_publish(&media->controls, &media->mailbox);
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

void menu_media_controls_draw(menu_media_controls_t *media,
      gfx_display_t *display, void *userdata, unsigned dims,
      const font_data_t *font)
{
   const gfx_media_view_t *view;
   gfx_media_rect_t bar;
   char elapsed[24], duration[24], caption[56];
   const char *labels[4];
   unsigned i;
   int64_t position;
   float scale;
   gfx_media_controls_consume(&media->mailbox, &media->view);
   view = &media->view;
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
   labels[1] = "-5"; labels[2] = "+5"; labels[3] = "[]";
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
