#include <string.h>
#include "gfx_media_controls.h"
#include "gfx_anim_preview.h"

static bool gfx_media_inside(const gfx_media_rect_t *r, int x, int y)
{
   return r->w > 0 && r->h > 0 && x >= r->x && y >= r->y
      && x - r->x < r->w && y - r->y < r->h;
}

void gfx_media_controls_cancel(gfx_media_controls_t *c)
{
   if (!c) return;
   c->held = c->dragging = c->row_focus = false;
   c->hover = c->press_target = -1;
   c->release_inhibit = true;
}

void gfx_media_controls_reset(gfx_media_controls_t *c)
{
   if (!c) return;
   memset(c, 0, sizeof(*c));
   c->last_x = c->last_y = c->hover = c->press_target = -1;
}

void gfx_media_controls_configure(gfx_media_controls_t *c,
      unsigned seek_seconds, unsigned hide_seconds)
{
   if (!c) return;
   c->seek_seconds = seek_seconds && seek_seconds <= 600 ? seek_seconds : 5;
   c->hide_seconds = hide_seconds <= 60 ? hide_seconds : 3;
   c->configured = true;
}

static int64_t gfx_media_drag_position(const gfx_media_controls_t *c, int x)
{
   int offset = x - c->layout.timeline.x;
   int width = c->layout.timeline.w;
   int64_t duration = c->playback.duration_us;
   if (width <= 0 || duration <= 0 || offset <= 0) return 0;
   if (offset >= width) return duration;
   return (duration / width) * offset + (duration % width) * offset / width;
}

bool gfx_media_controls_update(gfx_media_controls_t *c, const gfx_media_target_t *target,
      unsigned width, unsigned height, int64_t now)
{
   int unit, gap, bw, i, left, count;
   if (!c) return false;
   if (!target || !target->user || !target->state || !target->pause || !target->seek
         || width < 96 || height < 96 || width > 65535 || height > 65535)
   {
      gfx_media_controls_reset(c);
      return false;
   }
   if (c->target.user != target->user || c->target.identity != target->identity
         || c->target.state != target->state)
   {
      unsigned seek_seconds = c->configured ? c->seek_seconds : 5;
      unsigned hide_seconds = c->configured ? c->hide_seconds : 3;
      gfx_media_controls_reset(c);
      gfx_media_controls_configure(c, seek_seconds, hide_seconds);
      c->target = *target;
      c->release_inhibit = true;
      c->last_activity = now;
      c->visible = true;
   }
   c->target = *target;
   if (!target->state(target->user, now, &c->playback))
   {
      gfx_media_controls_reset(c);
      return false;
   }
   unit = (int)height / 24;
   if (unit < 16) unit = 16;
   if (unit > 72) unit = 72;
   gap = unit / 3;
   c->layout.unit = unit;
   c->layout.panel.x = gap;
   c->layout.panel.w = (int)width - 2 * gap;
   c->layout.panel.h = (target->subtitle_tracks ? 4 : 3) * unit + 3 * gap;
   c->layout.panel.y = (int)height - c->layout.panel.h - gap;
   c->layout.timeline.x = 2 * gap;
   c->layout.timeline.y = c->layout.panel.y + unit + gap;
   c->layout.timeline.w = (int)width - 4 * gap;
   c->layout.timeline.h = unit / 2;
   count = (target->settings ? 7 : target->step ? 6 : 4)
      + (target->subtitle_tracks ? 1 : 0);
   c->layout.button_count = (unsigned)count;
   if (c->focus >= (unsigned)count) c->focus = 0;
   bw = (c->layout.panel.w - (count + 1) * gap) / count;
   if (bw > 3 * unit) bw = 3 * unit;
   left = ((int)width - (count * bw + (count - 1) * gap)) / 2;
   for (i = 0; i < count; i++)
   {
      c->layout.buttons[i].x = left + i * (bw + gap);
      c->layout.buttons[i].y = c->layout.panel.y + 2 * unit + gap;
      c->layout.buttons[i].w = bw;
      c->layout.buttons[i].h = unit;
   }
   if (!c->hide_seconds) c->visible = true;
   if (c->hide_seconds && !c->playback.paused && !c->playback.seeking && !c->row_focus
         && !c->dragging && c->hover < 0 && now > c->last_activity
         && (uint64_t)now - (uint64_t)c->last_activity > (uint64_t)c->hide_seconds * 1000000)
      c->visible = false;
   if (c->playback.paused || c->playback.seeking || c->row_focus || c->dragging)
      c->visible = true;
   return true;
}

static bool gfx_media_button(gfx_media_controls_t *c, unsigned button, int64_t now)
{
   int64_t position = c->playback.seek_failed ? c->playback.seek_target_us
      : c->playback.position_us;
   const int64_t maximum = (int64_t)((~(uint64_t)0) >> 1);
   int64_t step = (int64_t)(c->seek_seconds ? c->seek_seconds : 5) * 1000000;
   if (button == 0)
   {
      if (c->playback.paused && c->playback.duration_us > 0
            && c->playback.position_us >= c->playback.duration_us
            && !c->target.seek(c->target.user, 0, now)) return false;
      return c->target.pause(c->target.user, !c->playback.paused, now);
   }
   if (c->target.subtitle_tracks && button + 1 == c->layout.button_count)
   {
      c->subtitle_requested = true;
      return true;
   }
   if (button == 6)
      return c->target.settings && c->target.settings(c->target.settings_user);
   if (button >= 4)
   {
      unsigned mask = button == 4 ? 1 : 2;
      return button < 6 && c->target.step && (c->target.directions & mask)
         && c->target.step(c->target.navigation_user, button == 4 ? -1 : 1);
   }
   if (!c->playback.seekable) return false;
   if (button == 3)
   {
      if (!c->target.pause(c->target.user, true, now)) return false;
      return c->target.seek(c->target.user, 0, now);
   }
   position = button == 1 ? (position > step ? position - step : 0)
      : position > maximum - step ? maximum : position + step;
   return c->target.seek(c->target.user, position, now);
}

bool gfx_media_controls_action(gfx_media_controls_t *c,
      enum gfx_media_action action, int64_t now)
{
   if (!c || !c->target.user) return false;
   if (!c->target.state(c->target.user, now, &c->playback))
   {
      gfx_media_controls_reset(c);
      return false;
   }
   c->visible = true;
   c->last_activity = now;
   switch (action)
   {
      case GFX_MEDIA_FOCUS: c->row_focus = true; return true;
      case GFX_MEDIA_UNFOCUS: c->row_focus = false; return true;
      case GFX_MEDIA_PREVIOUS:
      case GFX_MEDIA_NEXT:
         if (c->row_focus)
         {
            c->focus = (c->focus + (action == GFX_MEDIA_PREVIOUS
                  ? c->layout.button_count - 1 : 1)) % c->layout.button_count;
            return true;
         }
         return gfx_media_button(c, action == GFX_MEDIA_PREVIOUS ? 1 : 2, now);
      case GFX_MEDIA_ACTIVATE: return gfx_media_button(c, c->row_focus ? c->focus : 0, now);
      case GFX_MEDIA_PLAY:
         return !c->playback.paused || gfx_media_button(c, 0, now);
      case GFX_MEDIA_PAUSE:
         return c->playback.paused || c->target.pause(c->target.user, true, now);
      case GFX_MEDIA_PREVIOUS_FILE:
         return c->target.step && gfx_media_button(c, 4, now);
      case GFX_MEDIA_NEXT_FILE:
         return c->target.step && gfx_media_button(c, 5, now);
      case GFX_MEDIA_SETTINGS:
         return c->target.settings && gfx_media_button(c, 6, now);
      case GFX_MEDIA_STOP: return gfx_media_button(c, 3, now);
      case GFX_MEDIA_SEEK_BACK: return gfx_media_button(c, 1, now);
      case GFX_MEDIA_SEEK_FORWARD: return gfx_media_button(c, 2, now);
   }
   return false;
}

void gfx_media_controls_pointer(gfx_media_controls_t *c, int x, int y,
      bool pressed, bool valid, int64_t now)
{
   int i;
   bool lost_release;
   if (!c || !c->target.user) return;
   if (!valid)
   {
      c->held = c->dragging = false;
      c->hover = c->press_target = -1;
      c->release_inhibit = true;
      return;
   }
   if (x != c->last_x || y != c->last_y || pressed)
   {
      c->visible = true;
      c->last_activity = now;
   }
   c->last_x = x; c->last_y = y;
   c->hover = gfx_media_inside(&c->layout.panel, x, y) ? 8 : -1;
   for (i = 0; i < (int)c->layout.button_count; i++)
      if (gfx_media_inside(&c->layout.buttons[i], x, y)) c->hover = i;
   if (pressed && !c->held)
      c->press_target = c->hover >= 0 && c->hover < (int)c->layout.button_count ? c->hover : -1;
   if (pressed && !c->release_inhibit && !c->held && c->playback.seekable && c->playback.duration_us > 0
         && gfx_media_inside(&c->layout.timeline, x, y))
      c->dragging = true;
   if (pressed && c->dragging) c->drag_us = gfx_media_drag_position(c, x);
   /* Release commands come from the menu's pointer-up edge. A reset or
    * lost release cancels, rather than committing a stale drag later. */
   lost_release = !pressed && c->held;
   if (lost_release) gfx_media_controls_cancel(c);
   c->held = pressed;
   if (!pressed && !lost_release) c->release_inhibit = false;
}

void gfx_media_controls_release(gfx_media_controls_t *c, int x, int y,
      int64_t now)
{
   int i;
   if (!c || !c->target.user) return;
   if (c->release_inhibit) return;
   if (!c->target.state(c->target.user, now, &c->playback))
   {
      gfx_media_controls_reset(c);
      return;
   }
   c->visible = true; c->last_activity = now;
   if (c->dragging || (!c->held && gfx_media_inside(&c->layout.timeline, x, y)))
   {
      if (c->playback.seekable && c->playback.duration_us > 0)
         c->target.seek(c->target.user, gfx_media_drag_position(c, x), now);
   }
   else
      for (i = 0; i < (int)c->layout.button_count; i++)
         if (gfx_media_inside(&c->layout.buttons[i], x, y))
         {
            if (c->held && c->press_target != i) break;
            gfx_media_button(c, (unsigned)i, now);
            break;
         }
   c->held = c->dragging = false;
   c->press_target = -1;
}

static bool gfx_media_thumbnail_state(void *user, int64_t now,
      gfx_media_playback_state_t *out)
{ return gfx_thumbnail_get_playback_state((gfx_thumbnail_t*)user, now, out); }
static bool gfx_media_thumbnail_pause(void *user, bool paused, int64_t now)
{ return gfx_thumbnail_set_paused((gfx_thumbnail_t*)user, paused, now); }
static bool gfx_media_thumbnail_seek(void *user, int64_t target, int64_t now)
{ return gfx_thumbnail_seek((gfx_thumbnail_t*)user, target, now); }

void gfx_media_controls_thumbnail_target(gfx_thumbnail_t *thumbnail, gfx_media_target_t *out)
{
   if (!out) return;
   memset(out, 0, sizeof(*out));
   if (!thumbnail) return;
   out->user = thumbnail;
   out->identity = thumbnail->anim_sess;
   out->source_path = thumbnail->anim_sess
      ? ((const gfx_anim_preview_t*)thumbnail->anim_sess)->path : NULL;
   out->state = gfx_media_thumbnail_state;
   out->pause = gfx_media_thumbnail_pause;
   out->seek = gfx_media_thumbnail_seek;
}

void gfx_media_controls_publish(const gfx_media_controls_t *c, gfx_media_mailbox_t *m)
{
   unsigned slot, i;
   gfx_media_view_t *v;
   if (!m) return;
   for (slot = 0; slot < 3; slot++)
      if (retro_atomic_cas_int(&m->states[slot], 0, 1)) break;
   if (slot == 3) return;
   v = &m->views[slot];
   memset(v, 0, sizeof(*v));
   if (c) v->settings_preview = c->settings_preview;
   if (c && c->target.user)
   {
      v->layout = c->layout;
      v->playback = c->playback;
      v->drag_us = c->drag_us;
      v->focus = c->focus;
      v->seek_seconds = c->seek_seconds ? c->seek_seconds : 5;
      v->directions = c->target.directions;
      v->subtitle_tracks = c->target.subtitle_tracks;
      v->hover = c->hover;
      v->visible = c->visible;
      v->row_focus = c->row_focus;
      v->dragging = c->dragging;
   }
   for (i = 0; i < 3; i++)
      if (i != slot) retro_atomic_cas_int(&m->states[i], 2, 0);
   retro_atomic_store_release_int(&m->states[slot], 2);
}

bool gfx_media_controls_consume(gfx_media_mailbox_t *m, gfx_media_view_t *out)
{
   unsigned i;
   if (!m || !out) return false;
   for (i = 0; i < 3; i++)
      if (retro_atomic_cas_int(&m->states[i], 2, 3))
      {
         *out = m->views[i];
         retro_atomic_store_release_int(&m->states[i], 0);
         return true;
      }
   return false;
}
