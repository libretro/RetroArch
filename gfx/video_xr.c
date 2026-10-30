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

#include <math.h>
#include <string.h>

#include <retro_atomic.h>

#include "video_xr.h"
#include "video_defines.h"
#include "../runloop.h"

#define VIDEO_XR_PI 3.14159265358979323846

void video_xr_pose_identity(video_xr_pose_t *pose)
{
   memset(pose, 0, sizeof(*pose));
   pose->orientation.w = 1.0f;
}

void video_xr_rotate(const video_xr_quat_t *q, const video_xr_vec3_t *v,
      video_xr_vec3_t *out)
{
   /* v + 2w (q x v) + 2 q x (q x v) */
   float cx  = q->y * v->z - q->z * v->y;
   float cy  = q->z * v->x - q->x * v->z;
   float cz  = q->x * v->y - q->y * v->x;
   float ccx = q->y * cz - q->z * cy;
   float ccy = q->z * cx - q->x * cz;
   float ccz = q->x * cy - q->y * cx;
   float x   = v->x + 2.0f * (q->w * cx + ccx);
   float y   = v->y + 2.0f * (q->w * cy + ccy);
   float z   = v->z + 2.0f * (q->w * cz + ccz);
   out->x    = x;
   out->y    = y;
   out->z    = z;
}

bool video_xr_anchor_from_head(const video_xr_pose_t *head,
      video_xr_pose_t *anchor)
{
   double yaw;
   video_xr_vec3_t ahead, fwd;

   ahead.x = 0.0f;
   ahead.y = 0.0f;
   ahead.z = -1.0f;
   video_xr_rotate(&head->orientation, &ahead, &fwd);
   if (fabs(fwd.x) < 1e-4 && fabs(fwd.z) < 1e-4)
      return false;
   yaw                   = atan2(-fwd.x, -fwd.z);
   anchor->orientation.x = 0.0f;
   anchor->orientation.y = (float)sin(yaw / 2.0);
   anchor->orientation.z = 0.0f;
   anchor->orientation.w = (float)cos(yaw / 2.0);
   anchor->position      = head->position;
   return true;
}

void video_xr_compose(const video_xr_pose_t *a, const video_xr_pose_t *b,
      video_xr_pose_t *out)
{
   video_xr_vec3_t p;
   video_xr_quat_t q;
   const video_xr_quat_t *r = &a->orientation;
   const video_xr_quat_t *s = &b->orientation;
   video_xr_rotate(r, &b->position, &p);
   q.x = r->w * s->x + r->x * s->w + r->y * s->z - r->z * s->y;
   q.y = r->w * s->y - r->x * s->z + r->y * s->w + r->z * s->x;
   q.z = r->w * s->z + r->x * s->y - r->y * s->x + r->z * s->w;
   q.w = r->w * s->w - r->x * s->x - r->y * s->y - r->z * s->z;
   out->position.x  = a->position.x + p.x;
   out->position.y  = a->position.y + p.y;
   out->position.z  = a->position.z + p.z;
   out->orientation = q;
}

void video_xr_pose_invert(const video_xr_pose_t *a, video_xr_pose_t *out)
{
   video_xr_quat_t q;
   video_xr_vec3_t p;
   q.x = -a->orientation.x;
   q.y = -a->orientation.y;
   q.z = -a->orientation.z;
   q.w =  a->orientation.w;
   p.x = -a->position.x;
   p.y = -a->position.y;
   p.z = -a->position.z;
   video_xr_rotate(&q, &p, &out->position);
   out->orientation = q;
}

bool video_xr_lazy_follow(video_xr_pose_t *anchor,
      const video_xr_pose_t *target, double dt, bool gliding)
{
   double k;
   double yaw  = 2.0 * atan2(anchor->orientation.y, anchor->orientation.w);
   double turn = 2.0 * atan2(target->orientation.y, target->orientation.w)
      - yaw;
   double dx   = target->position.x - anchor->position.x;
   double dy   = target->position.y - anchor->position.y;
   double dz   = target->position.z - anchor->position.z;
   double dist = sqrt(dx * dx + dy * dy + dz * dz);

   /* The short way round. */
   while (turn > VIDEO_XR_PI)
      turn -= 2.0 * VIDEO_XR_PI;
   while (turn < -VIDEO_XR_PI)
      turn += 2.0 * VIDEO_XR_PI;
   if (     !gliding
         && fabs(turn) < VIDEO_XR_LAZY_START_DEG * VIDEO_XR_PI / 180.0
         && dist < VIDEO_XR_LAZY_START_M)
      return false;
   if (     fabs(turn) < VIDEO_XR_LAZY_STOP_DEG * VIDEO_XR_PI / 180.0
         && dist < VIDEO_XR_LAZY_STOP_M)
   {
      *anchor = *target;
      return false;
   }
   k    = (dt > 0.0) ? 1.0 - exp(-dt / VIDEO_XR_LAZY_TIME) : 0.0;
   yaw += turn * k;
   anchor->orientation.x = 0.0f;
   anchor->orientation.y = (float)sin(yaw / 2.0);
   anchor->orientation.z = 0.0f;
   anchor->orientation.w = (float)cos(yaw / 2.0);
   anchor->position.x   += (float)(dx * k);
   anchor->position.y   += (float)(dy * k);
   anchor->position.z   += (float)(dz * k);
   return true;
}

unsigned video_xr_image_dims(float width_m, float height_m, float distance,
      float px_per_rad, unsigned native_dims, unsigned max_dim)
{
   double w, h;
   if (width_m <= 0.0f || height_m <= 0.0f || distance <= 0.0f)
      return 0;
   w = ceil(2.0 * atan((double)width_m / (2.0 * distance)) * px_per_rad);
   if (w < (double)VIDEO_SCALE_W(native_dims))
      w = (double)VIDEO_SCALE_W(native_dims);
   if (w < 1.0)
      w = 1.0;
   h = floor(w * height_m / width_m + 0.5);
   if (h < 1.0)
      h = 1.0;
   if (max_dim && (w > max_dim || h > max_dim))
   {
      double s = (double)max_dim / (w > h ? w : h);
      w        = floor(w * s + 0.5);
      h        = floor(h * s + 0.5);
      if (w < 1.0)
         w = 1.0;
      if (h < 1.0)
         h = 1.0;
   }
   return VIDEO_SCALE_PACK((unsigned)w, (unsigned)h);
}

bool video_xr_shrinks(unsigned image_dims, unsigned source_dims,
      unsigned rotation)
{
   unsigned sw = VIDEO_SCALE_W(source_dims);
   unsigned sh = VIDEO_SCALE_H(source_dims);
   if (rotation & 1)
   {
      unsigned t = sw;
      sw         = sh;
      sh         = t;
   }
   if (!VIDEO_SCALE_W(image_dims) || !VIDEO_SCALE_H(image_dims))
      return false;
   return VIDEO_SCALE_W(image_dims) < sw || VIDEO_SCALE_H(image_dims) < sh;
}

unsigned video_xr_keep_dims(unsigned planned, unsigned current,
      bool editing)
{
   unsigned pw = VIDEO_SCALE_W(planned);
   unsigned ph = VIDEO_SCALE_H(planned);
   unsigned cw = VIDEO_SCALE_W(current);
   unsigned ch = VIDEO_SCALE_H(current);
   if (     editing
         && 5 * pw >= 4 * cw && 4 * pw <= 5 * cw
         && 5 * ph >= 4 * ch && 4 * ph <= 5 * ch)
      return current;
   return planned;
}

static void video_xr_screen_rotate(const video_screen_xr_t *screen,
      video_xr_pose_t *pose)
{
   video_xr_pose_t local;
   double yaw   = screen->yaw * VIDEO_XR_PI / 360.0;
   double pitch = screen->pitch * VIDEO_XR_PI / 360.0;
   double roll  = screen->roll * VIDEO_XR_PI / 360.0;
   double cy    = cos(yaw);
   double sy    = sin(yaw);
   double cp    = cos(pitch);
   double sp    = sin(pitch);
   double cr    = cos(roll);
   double sr    = sin(roll);

   video_xr_pose_identity(&local);
   local.orientation.x = (float)(cy * sp * cr + sy * cp * sr);
   local.orientation.y = (float)(sy * cp * cr - cy * sp * sr);
   local.orientation.z = (float)(cy * cp * sr - sy * sp * cr);
   local.orientation.w = (float)(cy * cp * cr + sy * sp * sr);
   video_xr_compose(pose, &local, pose);
}

/* Within this of straight up or down a turn has no roll of its own. */
#define VIDEO_XR_POLE 0.99999

/* The sine of q's pitch. */
static double video_xr_pitch_sin(const video_xr_quat_t *q)
{
   double sp = -2.0 * ((double)q->y * q->z - (double)q->w * q->x);
   if (sp > 1.0)
      return 1.0;
   if (sp < -1.0)
      return -1.0;
   return sp;
}

void video_xr_euler(const video_xr_quat_t *q, float *yaw, float *pitch,
      float *roll)
{
   double x   = q->x;
   double y   = q->y;
   double z   = q->z;
   double w   = q->w;
   double sp  = video_xr_pitch_sin(q);
   *pitch = (float)(asin(sp) * 180.0 / VIDEO_XR_PI);
   if (fabs(sp) > VIDEO_XR_POLE)
   {
      *yaw  = (float)(atan2(-2.0 * (x * z - w * y),
               1.0 - 2.0 * (y * y + z * z)) * 180.0 / VIDEO_XR_PI);
      *roll = 0.0f;
      return;
   }
   *yaw  = (float)(atan2(2.0 * (x * z + w * y),
            1.0 - 2.0 * (x * x + y * y)) * 180.0 / VIDEO_XR_PI);
   *roll = (float)(atan2(2.0 * (x * y + w * z),
            1.0 - 2.0 * (x * x + z * z)) * 180.0 / VIDEO_XR_PI);
}

void video_xr_item_pose(const video_xr_vec3_t *base,
      const video_screen_xr_t *screen, video_xr_pose_t *out)
{
   video_xr_pose_identity(out);
   out->position = *base;
   if (!screen)
      return;
   out->position.x += screen->x;
   out->position.y += screen->y;
   out->position.z += screen->z;
   if (screen->yaw != 0.0f || screen->pitch != 0.0f || screen->roll != 0.0f)
      video_xr_screen_rotate(screen, out);
}

void video_xr_item_settings(const video_xr_vec3_t *base,
      const video_xr_pose_t *pose, video_screen_xr_t *out)
{
   out->x = pose->position.x - base->x;
   out->y = pose->position.y - base->y;
   out->z = pose->position.z - base->z;
   video_xr_euler(&pose->orientation, &out->yaw, &out->pitch, &out->roll);
}

/* An item from its layout spot, adjusted by screen (NULL: as laid out):
 * its pose from p's anchor, its size scaled, and the distance its image
 * is sized for, which only an offset changes. */
static void video_xr_adjust(const video_xr_params_t *p,
      const video_xr_vec3_t *base, const video_screen_xr_t *screen,
      video_xr_pose_t *pose, double *w, double *h, float *distance)
{
   video_xr_pose_t local;
   video_xr_item_pose(base, screen, &local);
   if (screen)
   {
      *w *= screen->scale;
      *h *= screen->scale;
      if (screen->x != 0.0f || screen->y != 0.0f || screen->z != 0.0f)
      {
         double d  = sqrt((double)local.position.x * local.position.x
               + (double)local.position.y * local.position.y
               + (double)local.position.z * local.position.z);
         *distance = (float)(d < 0.05 ? 0.05 : d);
      }
   }
   video_xr_compose(&p->anchor, &local, pose);
}

static void video_xr_add(video_xr_quad_set_t *out, unsigned kind,
      unsigned eye, unsigned screen, unsigned slot, unsigned layer,
      const video_xr_pose_t *pose, float width, float height)
{
   video_xr_quad_t *q;
   if (out->num_quads >= VIDEO_XR_MAX_QUADS)
      return;
   q         = &out->quads[out->num_quads++];
   q->pose   = *pose;
   q->width  = width;
   q->height = height;
   q->kind   = kind;
   q->eye    = eye;
   q->screen = screen;
   q->slot   = slot;
   q->layer  = layer;
}

/* A screen's image: the shown size for the stock chain, which shrinks
 * smoothly; a preset's passes get at least the source's pixels. Without
 * a density, the source's size either way. */
static unsigned video_xr_screen_dims(const video_xr_params_t *p,
      float width, float height, float distance, unsigned native_dims)
{
   if (p->stock && p->px_per_rad > 0.0f)
      native_dims = 0;
   return video_xr_image_dims(width, height, distance, p->px_per_rad,
         native_dims, p->max_dim);
}

void video_xr_layout(const video_xr_params_t *p, video_xr_layout_t *out)
{
   unsigned s;
   double k;
   double disp_w[RETRO_VIDEO_VIEWS_MAX];
   double disp_h[RETRO_VIDEO_VIEWS_MAX];
   const video_views_map_t *map = p->map;
   bool horizontal              =
      (p->screen_layout == VIDEO_SCREEN_LAYOUT_HORIZONTAL);
   double gap                   = VIDEO_XR_GAP * p->width;
   double edge                  = 0.0;

   memset(out, 0, sizeof(*out));
   out->menu.z = -(p->distance - VIDEO_XR_MENU_LIFT);
   if (!map || !map->num_views)
   {
      float aspect = p->frame_aspect;
      if (aspect <= 0.0f && VIDEO_SCALE_H(p->frame_dims))
         aspect = (float)VIDEO_SCALE_W(p->frame_dims)
            / (float)VIDEO_SCALE_H(p->frame_dims);
      if (aspect <= 0.0f)
         aspect = 4.0f / 3.0f;
      out->pos[0].z  = -p->distance;
      out->width[0]  = p->width;
      out->height[0] = p->width / aspect;
      out->native[0] = p->frame_dims;
      out->count     = 1;
      return;
   }

   for (s = 0; s < map->num_screens; s++)
   {
      const struct retro_video_view *v = &map->views[
         video_views_find(map, s, RETRO_VIDEO_VIEW_EYE_NONE)];
      double h      = v->height;
      double aspect = video_views_aspect(v);
      if (p->rotation & 1)
      {
         h      = v->width;
         aspect = 1.0 / aspect;
      }
      disp_h[s] = h;
      disp_w[s] = h * aspect;
   }
   k = p->width / disp_w[0];

   for (s = 0; s < map->num_screens; s++)
   {
      double w = disp_w[s] * k;
      double h = disp_h[s] * k;
      double x = 0.0;
      double y = 0.0;
      if (s == 0)
         edge = horizontal ? w / 2.0 : -h / 2.0;
      else if (horizontal)
      {
         x    = edge + gap + w / 2.0;
         edge = x + w / 2.0;
      }
      else
      {
         y    = edge - gap - h / 2.0;
         edge = y - h / 2.0;
      }
      out->pos[s].x  = (float)x;
      out->pos[s].y  = (float)y;
      out->pos[s].z  = -p->distance;
      out->width[s]  = (float)w;
      out->height[s] = (float)h;
      out->native[s] = VIDEO_SCALE_PACK((unsigned)(disp_w[s] + 0.5),
            (unsigned)(disp_h[s] + 0.5));
   }
   out->count = map->num_screens;
}

/* Screens from their layout spots, each moved by its adjustments. */
static void video_xr_place_screens(const video_xr_params_t *p,
      const video_xr_layout_t *lay, video_xr_quad_set_t *out)
{
   unsigned s;
   const video_views_map_t *map = p->map;

   for (s = 0; s < lay->count; s++)
   {
      video_xr_pose_t pose;
      video_screen_xr_t screen;
      float distance        = p->distance;
      int left              = video_views_find(map, s,
            RETRO_VIDEO_VIEW_EYE_LEFT);
      int right             = video_views_find(map, s,
            RETRO_VIDEO_VIEW_EYE_RIGHT);
      double w              = lay->width[s];
      double h              = lay->height[s];
      video_xr_slot_t *slot = &out->slots[s];

      slot->base = lay->pos[s];
      if (p->skip_screens & (1u << s))
         continue;
      if (p->screens)
      {
         screen = p->screens[s];
         video_views_xr_normalize(&screen);
         if (!screen.visible)
            continue;
      }
      video_xr_adjust(p, &lay->pos[s], p->screens ? &screen : NULL,
            &pose, &w, &h, &distance);

      slot->dims    = video_xr_screen_dims(p, (float)w, (float)h, distance,
            lay->native[s]);
      slot->view[0] = left;
      if (left != right)
      {
         slot->layers  = 2;
         slot->view[1] = right;
         video_xr_add(out, VIDEO_XR_QUAD_SCREEN, VIDEO_XR_EYE_LEFT, s, s,
               p->swap_eyes ? 1 : 0, &pose, (float)w, (float)h);
         video_xr_add(out, VIDEO_XR_QUAD_SCREEN, VIDEO_XR_EYE_RIGHT, s, s,
               p->swap_eyes ? 0 : 1, &pose, (float)w, (float)h);
      }
      else
      {
         slot->layers = 1;
         video_xr_add(out, VIDEO_XR_QUAD_SCREEN, VIDEO_XR_EYE_BOTH, s, s,
               0, &pose, (float)w, (float)h);
      }
   }
}

/* The UI's image: its own size, but no more than VIDEO_XR_MENU_DENSITY
 * times the headset's pixels across the quad, and at most max_dim a
 * side, in its shape. */
static unsigned video_xr_menu_dims(const video_xr_params_t *p,
      float width, float height, float distance)
{
   unsigned w = VIDEO_SCALE_W(p->ui_dims);
   unsigned h = VIDEO_SCALE_H(p->ui_dims);
   if (p->px_per_rad > 0.0f)
   {
      double most = ceil(VIDEO_XR_MENU_DENSITY * 2.0
            * atan((double)width / (2.0 * distance)) * p->px_per_rad);
      if (most < (double)w)
         w = (unsigned)most;
   }
   if (     w == VIDEO_SCALE_W(p->ui_dims)
         && (!p->max_dim || (w <= p->max_dim && h <= p->max_dim)))
      return p->ui_dims;
   return video_xr_image_dims(width, height, distance, 0.0f,
         VIDEO_SCALE_PACK(w, h), p->max_dim);
}

/* At its own spot; or with menu_follow VIDEO_XR_MENU_LIFT in front of
 * screen 0, facing as it does, while screen 0 floats and shows. Following, it
 * comes no nearer than VIDEO_XR_MENU_NEAREST, and w and h scale with
 * its distance so it looks as large as at its spot; facing away from
 * the anchor, where no laser reaches it, it stays at its spot. */
static void video_xr_menu_spot(const video_xr_params_t *p,
      const video_xr_layout_t *lay, video_xr_pose_t *pose,
      double *w, double *h)
{
   video_xr_pose_t local, lift;
   video_screen_xr_t screen;
   bool follow = p->menu_follow && lay->count && !(p->skip_screens & 1u);
   if (follow && p->screens)
   {
      screen = p->screens[0];
      video_views_xr_normalize(&screen);
      follow = screen.visible;
   }
   if (!follow)
   {
      video_xr_item_pose(&lay->menu, NULL, &local);
      video_xr_compose(&p->anchor, &local, pose);
      return;
   }
   video_xr_item_pose(&lay->pos[0], p->screens ? &screen : NULL, &local);
   video_xr_pose_identity(&lift);
   lift.position.z = VIDEO_XR_MENU_LIFT;
   video_xr_compose(&local, &lift, &local);
   if (p->screens && (screen.x != 0.0f || screen.y != 0.0f
            || screen.z != 0.0f || screen.yaw != 0.0f
            || screen.pitch != 0.0f || screen.roll != 0.0f))
   {
      video_xr_vec3_t face;
      double spot = p->distance - VIDEO_XR_MENU_LIFT;
      double d    = sqrt((double)local.position.x * local.position.x
            + (double)local.position.y * local.position.y
            + (double)local.position.z * local.position.z);
      if (d < VIDEO_XR_MENU_NEAREST)
      {
         /* Out along the line from the anchor; at it, straight ahead. */
         if (d < 1e-6)
         {
            local.position.x = 0.0f;
            local.position.y = 0.0f;
            local.position.z = -1.0f;
            d                = 1.0;
         }
         local.position.x = (float)(local.position.x
               * VIDEO_XR_MENU_NEAREST / d);
         local.position.y = (float)(local.position.y
               * VIDEO_XR_MENU_NEAREST / d);
         local.position.z = (float)(local.position.z
               * VIDEO_XR_MENU_NEAREST / d);
         d                = VIDEO_XR_MENU_NEAREST;
      }
      face.x = 0.0f;
      face.y = 0.0f;
      face.z = 1.0f;
      video_xr_rotate(&local.orientation, &face, &face);
      if (     (double)face.x * local.position.x
             + (double)face.y * local.position.y
             + (double)face.z * local.position.z >= 0.0)
      {
         video_xr_item_pose(&lay->menu, NULL, &local);
         video_xr_compose(&p->anchor, &local, pose);
         return;
      }
      if (spot > 0.0)
      {
         *w *= d / spot;
         *h *= d / spot;
      }
   }
   video_xr_compose(&p->anchor, &local, pose);
}

void video_xr_place(const video_xr_params_t *p, video_xr_quad_set_t *out)
{
   unsigned s;
   video_xr_pose_t pose;
   video_xr_layout_t lay;

   memset(out, 0, sizeof(*out));
   for (s = 0; s < VIDEO_XR_MAX_SLOTS; s++)
   {
      out->slots[s].view[0]  = -1;
      out->slots[s].view[1]  = -1;
      out->slots[s].edit_seq = p->edit_seq;
   }
   if (p->width <= 0.0f || p->distance <= 0.0f)
      return;

   video_xr_layout(p, &lay);
   if (p->map && p->map->num_views)
      video_xr_place_screens(p, &lay, out);
   else
   {
      video_screen_xr_t screen;
      double w       = lay.width[0];
      double h       = lay.height[0];
      float distance = p->distance;
      out->slots[0].base = lay.pos[0];
      /* A whole frame is screen 0; without the screens' settings, slot 0
       * is no item. */
      if (p->screens)
      {
         screen = p->screens[0];
         video_views_xr_normalize(&screen);
      }
      else
         out->slots[0].fixed = true;
      if (!p->screens || screen.visible)
      {
         video_xr_adjust(p, &lay.pos[0], p->screens ? &screen : NULL,
               &pose, &w, &h, &distance);
         out->slots[0].dims   = video_xr_screen_dims(p, (float)w, (float)h,
               distance, p->frame_dims);
         out->slots[0].layers = 1;
         video_xr_add(out, p->window ? VIDEO_XR_QUAD_WINDOW
               : VIDEO_XR_QUAD_FRAME, VIDEO_XR_EYE_BOTH, 0, 0, 0,
               &pose, (float)w, (float)h);
      }
   }

   if (VIDEO_SCALE_W(p->ui_dims) && VIDEO_SCALE_H(p->ui_dims))
   {
      unsigned dims;
      double w, h;
      float aspect = (float)VIDEO_SCALE_W(p->ui_dims)
         / (float)VIDEO_SCALE_H(p->ui_dims);
      float mw     = p->width;
      float mh     = mw / aspect;
      if (mh > p->width * VIDEO_XR_MENU_MAX_H)
      {
         mh = p->width * VIDEO_XR_MENU_MAX_H;
         mw = mh * aspect;
      }
      w = mw;
      h = mh;
      if (p->menu)
      {
         video_screen_xr_t menu = *p->menu;
         float d                = p->distance - VIDEO_XR_MENU_LIFT;
         video_views_xr_normalize(&menu);
         video_xr_adjust(p, &lay.menu, &menu, &pose, &w, &h, &d);
         dims = video_xr_menu_dims(p, (float)w, (float)h, d);
      }
      else
      {
         /* Its angular size, so its image, is the one at its spot. */
         dims = video_xr_menu_dims(p, mw, mh,
               p->distance - VIDEO_XR_MENU_LIFT);
         video_xr_menu_spot(p, &lay, &pose, &w, &h);
      }
      out->slots[VIDEO_XR_MENU_SLOT].base   = lay.menu;
      out->slots[VIDEO_XR_MENU_SLOT].dims   = dims;
      out->slots[VIDEO_XR_MENU_SLOT].layers = 1;
      video_xr_add(out, VIDEO_XR_QUAD_MENU, VIDEO_XR_EYE_BOTH, 0,
            VIDEO_XR_MENU_SLOT, 0, &pose, (float)w, (float)h);
   }
}

/* video_xr_ray_hit, or with both either face. */
static bool video_xr_ray_hit_sides(const video_xr_quad_t *q,
      const video_xr_vec3_t *origin, const video_xr_vec3_t *dir, bool both,
      float *u, float *v, float *t)
{
   double denom, dist, hu, hv;
   video_xr_quat_t inv;
   video_xr_vec3_t z, n, rel, local;

   z.x = 0.0f;
   z.y = 0.0f;
   z.z = 1.0f;
   video_xr_rotate(&q->pose.orientation, &z, &n);
   denom = n.x * dir->x + n.y * dir->y + n.z * dir->z;
   if (both ? fabs(denom) < 1e-6 : denom > -1e-6)
      return false;
   dist = (n.x * (q->pose.position.x - origin->x)
         + n.y * (q->pose.position.y - origin->y)
         + n.z * (q->pose.position.z - origin->z)) / denom;
   if (dist < 0.0 || q->width <= 0.0f || q->height <= 0.0f)
      return false;

   rel.x = (float)(origin->x + dir->x * dist - q->pose.position.x);
   rel.y = (float)(origin->y + dir->y * dist - q->pose.position.y);
   rel.z = (float)(origin->z + dir->z * dist - q->pose.position.z);
   inv   = q->pose.orientation;
   inv.x = -inv.x;
   inv.y = -inv.y;
   inv.z = -inv.z;
   video_xr_rotate(&inv, &rel, &local);

   hu = local.x / q->width + 0.5;
   hv = 0.5 - local.y / q->height;
   if (hu < -1e-4 || hu > 1.0 + 1e-4 || hv < -1e-4 || hv > 1.0 + 1e-4)
      return false;
   *u = (float)(hu < 0.0 ? 0.0 : (hu > 1.0 ? 1.0 : hu));
   *v = (float)(hv < 0.0 ? 0.0 : (hv > 1.0 ? 1.0 : hv));
   *t = (float)dist;
   return true;
}

bool video_xr_ray_hit(const video_xr_quad_t *q, const video_xr_vec3_t *origin,
      const video_xr_vec3_t *dir, float *u, float *v, float *t)
{
   return video_xr_ray_hit_sides(q, origin, dir, false, u, v, t);
}

bool video_xr_quad_behind(const video_xr_quad_t *q,
      const video_xr_vec3_t *eye)
{
   video_xr_vec3_t z, n;
   z.x = 0.0f;
   z.y = 0.0f;
   z.z = 1.0f;
   video_xr_rotate(&q->pose.orientation, &z, &n);
   return n.x * (eye->x - q->pose.position.x)
      + n.y * (eye->y - q->pose.position.y)
      + n.z * (eye->z - q->pose.position.z) < 0.0f;
}

void video_xr_turn_back(video_xr_pose_t *pose)
{
   /* orientation times a half turn about Y, (0, 1, 0, 0). */
   video_xr_quat_t o    = pose->orientation;
   pose->orientation.x  = -o.z;
   pose->orientation.y  = o.w;
   pose->orientation.z  = o.x;
   pose->orientation.w  = -o.y;
}

static int16_t video_xr_norm(unsigned p, unsigned size)
{
   if (p == 0 || size < 2)
      return -0x7fff;
   if (p >= size - 1)
      return 0x7fff;
   return (int16_t)((int)(((unsigned long)p * 0xffffUL) / (size - 1))
         - 0x8000);
}

/* A fraction of a span of w pixels, as a pixel in it. */
static unsigned video_xr_pixel(float f, unsigned w)
{
   unsigned p;
   if (f <= 0.0f)
      return 0;
   p = (unsigned)(f * (float)w);
   return (p >= w) ? w - 1 : p;
}

bool video_xr_quad_to_frame(const video_xr_quad_t *q, float u, float v,
      const video_views_map_t *map, unsigned frame_dims,
      int16_t *res_x, int16_t *res_y)
{
   unsigned x0 = 0;
   unsigned y0 = 0;
   unsigned w  = VIDEO_SCALE_W(frame_dims);
   unsigned h  = VIDEO_SCALE_H(frame_dims);

   if (!w || !h || q->kind == VIDEO_XR_QUAD_MENU)
      return false;
   if (q->kind == VIDEO_XR_QUAD_SCREEN)
   {
      int r;
      if (!map || !map->num_views)
         return false;
      if ((r = video_views_find(map, q->screen,
                  RETRO_VIDEO_VIEW_EYE_NONE)) < 0)
         return false;
      x0 = map->views[r].x;
      y0 = map->views[r].y;
      w  = map->views[r].width;
      h  = map->views[r].height;
   }
   *res_x = video_xr_norm(x0 + video_xr_pixel(u, w), VIDEO_SCALE_W(frame_dims));
   *res_y = video_xr_norm(y0 + video_xr_pixel(v, h), VIDEO_SCALE_H(frame_dims));
   return true;
}

bool video_xr_window_point(const video_viewport_t *vp, float *u, float *v)
{
   double fu, fv;
   unsigned w = VIDEO_SCALE_W(vp->dims);
   unsigned h = VIDEO_SCALE_H(vp->dims);
   if (!w || !h)
      return false;
   fu = ((double)*u * VIDEO_SCALE_W(vp->full_dims)
         - VIDEO_POS_X(vp->pos)) / w;
   fv = ((double)*v * VIDEO_SCALE_H(vp->full_dims)
         - VIDEO_POS_Y(vp->pos)) / h;
   if (fu < -1e-4 || fu > 1.0 + 1e-4 || fv < -1e-4 || fv > 1.0 + 1e-4)
      return false;
   *u = (float)(fu < 0.0 ? 0.0 : (fu > 1.0 ? 1.0 : fu));
   *v = (float)(fv < 0.0 ? 0.0 : (fv > 1.0 ? 1.0 : fv));
   return true;
}

void video_xr_window_rect(const video_viewport_t *vp, unsigned frame_dims,
      unsigned rotation, const struct retro_video_view *v,
      video_views_rect_t *out)
{
   unsigned r;
   int x0, y0, x1, y1;
   double e[4];
   double fw = (double)VIDEO_SCALE_W(frame_dims);
   double fh = (double)VIDEO_SCALE_H(frame_dims);
   double vx = (double)VIDEO_POS_X(vp->pos);
   double vy = (double)VIDEO_POS_Y(vp->pos);
   double vw = (double)VIDEO_SCALE_W(vp->dims);
   double vh = (double)VIDEO_SCALE_H(vp->dims);

   out->pos  = 0;
   out->dims = 0;
   if (fw <= 0.0 || fh <= 0.0)
      return;
   /* Its left, top, right and bottom edges, as fractions of the frame. */
   e[0] = (double)v->x / fw;
   e[1] = (double)v->y / fh;
   e[2] = ((double)v->x + (double)v->width) / fw;
   e[3] = ((double)v->y + (double)v->height) / fh;
   /* A quarter turn anticlockwise takes u, v to v, 1 - u. */
   for (r = 0; r < (rotation & 3); r++)
   {
      double left = e[1];
      double top  = 1.0 - e[2];
      e[2]        = e[3];
      e[3]        = 1.0 - e[0];
      e[0]        = left;
      e[1]        = top;
   }
   /* Edges rounded, not sizes: neighbours meet without a gap. */
   x0        = (int)floor(vx + e[0] * vw + 0.5);
   y0        = (int)floor(vy + e[1] * vh + 0.5);
   x1        = (int)floor(vx + e[2] * vw + 0.5);
   y1        = (int)floor(vy + e[3] * vh + 0.5);
   out->pos  = VIDEO_POS_PACK(x0, y0);
   out->dims = VIDEO_SCALE_PACK(x1 - x0, y1 - y0);
}

void video_xr_place_window(const video_xr_params_t *p, bool menu,
      bool tracked, const video_viewport_t *vp, unsigned frame_dims,
      video_xr_quad_set_t *out,
      video_views_rect_t rects[VIDEO_XR_MAX_SLOTS][2])
{
   unsigned s, l;
   video_xr_params_t q = *p;
   /* The menu is drawn over the frame whole, and a tracked map's eyes
    * are the projection's: the window as it is. */
   if (menu || tracked)
   {
      q.map     = NULL;
      q.screens = NULL;
   }
   video_xr_place(&q, out);
   for (s = 0; s < VIDEO_XR_MAX_SLOTS; s++)
   {
      for (l = 0; l < 2; l++)
      {
         int view = out->slots[s].view[l];
         if (view >= 0)
            video_xr_window_rect(vp, frame_dims, q.rotation,
                  &q.map->views[view], &rects[s][l]);
         else
         {
            rects[s][l].pos  = 0;
            rects[s][l].dims = vp->full_dims;
         }
      }
   }
}

bool video_xr_quad_live(const video_xr_quad_t *q, unsigned laser,
      bool menu_open)
{
   if (laser == VIDEO_XR_LIVE_EDIT)
      return true;
   if (laser == VIDEO_OPENXR_LASER_OFF)
      return false;
   switch (q->kind)
   {
      case VIDEO_XR_QUAD_MENU:
         return menu_open;
      case VIDEO_XR_QUAD_WINDOW:
         if (menu_open)
            return true;
         break;
      case VIDEO_XR_QUAD_SCREEN:
         if (q->screen >= 1)
            return true;
         break;
      default:
         break;
   }
   return laser == VIDEO_OPENXR_LASER_ALWAYS;
}

/* How far below the menu's middle its handle's top edge is. */
static float video_xr_handle_top(const video_xr_quad_t *menu)
{
   video_xr_quad_t outline;
   video_xr_outline(menu, &outline);
   return outline.height * 0.5f + VIDEO_XR_HANDLE_GAP;
}

void video_xr_menu_handle(const video_xr_quad_t *menu, video_xr_quad_t *out)
{
   video_xr_vec3_t down, world;
   *out        = *menu;
   out->width  = menu->width * VIDEO_XR_HANDLE_WIDTH;
   out->height = VIDEO_XR_HANDLE_HEIGHT;
   down.x      = 0.0f;
   down.y      = -video_xr_handle_top(menu) - VIDEO_XR_HANDLE_HEIGHT * 0.5f;
   down.z      = 0.0f;
   video_xr_rotate(&menu->pose.orientation, &down, &world);
   out->pose.position.x += world.x;
   out->pose.position.y += world.y;
   out->pose.position.z += world.z;
}

/* Where a ray meets the menu q's handle, or comes within the margin of
 * it, as a point in q's own frame; *bar whether on the bar itself. */
static bool video_xr_handle_hit(const video_xr_quad_t *q,
      const video_xr_vec3_t *origin, const video_xr_vec3_t *dir, bool both,
      float *u, float *v, float *t, bool *bar)
{
   float hu, hv;
   video_xr_quad_t handle;
   if (q->width <= 0.0f || q->height <= 0.0f)
      return false;
   video_xr_menu_handle(q, &handle);
   handle.width  += 2.0f * VIDEO_XR_HANDLE_MARGIN;
   handle.height += 2.0f * VIDEO_XR_HANDLE_MARGIN;
   if (!video_xr_ray_hit_sides(&handle, origin, dir, both, &hu, &hv, t))
      return false;
   *bar = fabs(hu - 0.5f) * handle.width
         <= handle.width * 0.5f - VIDEO_XR_HANDLE_MARGIN
      && fabs(hv - 0.5f) * handle.height
         <= handle.height * 0.5f - VIDEO_XR_HANDLE_MARGIN;
   *u = 0.5f + (hu - 0.5f) * handle.width / q->width;
   *v = 0.5f + (video_xr_handle_top(q) - VIDEO_XR_HANDLE_MARGIN
         + hv * handle.height) / q->height;
   return true;
}

static int video_xr_pick_with(const video_xr_quad_set_t *set,
      unsigned laser, bool menu_open, bool stuck,
      const video_xr_vec3_t *origin, const video_xr_vec3_t *dir,
      float *u, float *v, float *t, bool *stick)
{
   unsigned i;
   int best   = -1;
   int handle = -1;
   bool bar   = false;
   bool both  = laser == VIDEO_XR_LIVE_EDIT;
   float bu   = 0.0f;
   float bv   = 0.0f;
   float bt   = 0.0f;
   for (i = 0; i < set->num_quads; i++)
   {
      float qu, qv, qt;
      const video_xr_quad_t *q = &set->quads[i];
      if (     !video_xr_quad_live(q, laser, menu_open)
            || (laser == VIDEO_XR_LIVE_EDIT && set->slots[q->slot].fixed))
         continue;
      if (laser == VIDEO_XR_LIVE_EDIT && q->kind == VIDEO_XR_QUAD_MENU)
      {
         if (video_xr_handle_hit(q, origin, dir, both, &qu, &qv, &qt,
                  &bar))
         {
            handle = (int)i;
            *u     = qu;
            *v     = qv;
            *t     = qt;
         }
         continue;
      }
      if (!video_xr_ray_hit_sides(q, origin, dir, both, &qu, &qv, &qt))
         continue;
      if (best < 0 || qt < bt)
      {
         best = (int)i;
         bu   = qu;
         bv   = qv;
         bt   = qt;
      }
   }
   /* The bar wins over anything the ray meets, its margin over
    * nothing, or over anything while stuck. */
   if (stick)
      *stick = handle >= 0 && (bar || stuck);
   if (handle >= 0 && (bar || stuck || best < 0))
      return handle;
   if (best >= 0)
   {
      *u = bu;
      *v = bv;
      *t = bt;
   }
   return best;
}

int video_xr_pick(const video_xr_quad_set_t *set, unsigned laser,
      bool menu_open, const video_xr_vec3_t *origin,
      const video_xr_vec3_t *dir, float *u, float *v, float *t)
{
   return video_xr_pick_with(set, laser, menu_open, false, origin, dir,
         u, v, t, NULL);
}

int video_xr_pick_edit(const video_xr_quad_set_t *set, bool stuck,
      const video_xr_vec3_t *origin, const video_xr_vec3_t *dir,
      float *u, float *v, float *t, bool *stick)
{
   return video_xr_pick_with(set, VIDEO_XR_LIVE_EDIT, false, stuck,
         origin, dir, u, v, t, stick);
}

float video_xr_cursor(const video_xr_quad_t *q, float u, float v,
      float dist, video_xr_pose_t *pose)
{
   video_xr_vec3_t local, world;
   float size          = dist * VIDEO_XR_CURSOR_SCALE;
   local.x             = (u - 0.5f) * q->width;
   local.y             = (0.5f - v) * q->height;
   local.z             = VIDEO_XR_CURSOR_LIFT;
   video_xr_rotate(&q->pose.orientation, &local, &world);
   pose->orientation   = q->pose.orientation;
   pose->position.x    = q->pose.position.x + world.x;
   pose->position.y    = q->pose.position.y + world.y;
   pose->position.z    = q->pose.position.z + world.z;
   return (size < VIDEO_XR_CURSOR_MIN) ? VIDEO_XR_CURSOR_MIN : size;
}

/* The rotation taking X, Y and Z to the orthonormal x, y and z. */
static void video_xr_quat_axes(const video_xr_vec3_t *x,
      const video_xr_vec3_t *y, const video_xr_vec3_t *z,
      video_xr_quat_t *q)
{
   double s;
   double tr = (double)x->x + y->y + z->z;
   if (tr > 0.0)
   {
      s    = sqrt(tr + 1.0) * 2.0;
      q->w = (float)(0.25 * s);
      q->x = (float)((y->z - z->y) / s);
      q->y = (float)((z->x - x->z) / s);
      q->z = (float)((x->y - y->x) / s);
   }
   else if (x->x > y->y && x->x > z->z)
   {
      s    = sqrt(1.0 + x->x - y->y - z->z) * 2.0;
      q->w = (float)((y->z - z->y) / s);
      q->x = (float)(0.25 * s);
      q->y = (float)((y->x + x->y) / s);
      q->z = (float)((z->x + x->z) / s);
   }
   else if (y->y > z->z)
   {
      s    = sqrt(1.0 + y->y - x->x - z->z) * 2.0;
      q->w = (float)((z->x - x->z) / s);
      q->x = (float)((y->x + x->y) / s);
      q->y = (float)(0.25 * s);
      q->z = (float)((z->y + y->z) / s);
   }
   else
   {
      s    = sqrt(1.0 + z->z - x->x - y->y) * 2.0;
      q->w = (float)((x->y - y->x) / s);
      q->x = (float)((z->x + x->z) / s);
      q->y = (float)((z->y + y->z) / s);
      q->z = (float)(0.25 * s);
   }
}

void video_xr_beam(const video_xr_vec3_t *o, const video_xr_vec3_t *d,
      float len, const video_xr_vec3_t *eye, video_xr_pose_t *pose)
{
   video_xr_vec3_t y, z;
   float k, n;
   pose->position.x = o->x + d->x * len * 0.5f;
   pose->position.y = o->y + d->y * len * 0.5f;
   pose->position.z = o->z + d->z * len * 0.5f;
   z.x              = eye->x - pose->position.x;
   z.y              = eye->y - pose->position.y;
   z.z              = eye->z - pose->position.z;
   k                = z.x * d->x + z.y * d->y + z.z * d->z;
   z.x             -= d->x * k;
   z.y             -= d->y * k;
   z.z             -= d->z * k;
   n                = (float)sqrt(z.x * z.x + z.y * z.y + z.z * z.z);
   if (n < 0.0001f)
   {
      /* Seen end on: any face square to the ray. */
      z.x  = 0.0f;
      z.y  = (fabs(d->y) < 0.9) ? 1.0f : 0.0f;
      z.z  = 1.0f - z.y;
      k    = z.y * d->y + z.z * d->z;
      z.x -= d->x * k;
      z.y -= d->y * k;
      z.z -= d->z * k;
      n    = (float)sqrt(z.x * z.x + z.y * z.y + z.z * z.z);
   }
   z.x /= n;
   z.y /= n;
   z.z /= n;
   y.x  = z.y * d->z - z.z * d->y;
   y.y  = z.z * d->x - z.x * d->z;
   y.z  = z.x * d->y - z.y * d->x;
   video_xr_quat_axes(d, &y, &z, &pose->orientation);
}

void video_xr_tag(const video_xr_vec3_t *hand, const video_xr_vec3_t *eye,
      video_xr_pose_t *pose)
{
   float n;
   video_xr_vec3_t x, y, z;
   pose->position.x = hand->x;
   pose->position.y = hand->y + VIDEO_XR_TAG_LIFT;
   pose->position.z = hand->z;
   z.x              = eye->x - pose->position.x;
   z.y              = eye->y - pose->position.y;
   z.z              = eye->z - pose->position.z;
   n                = (float)sqrt(z.x * z.x + z.y * z.y + z.z * z.z);
   if (n < 0.0001f)
   {
      pose->orientation.x = 0.0f;
      pose->orientation.y = 0.0f;
      pose->orientation.z = 0.0f;
      pose->orientation.w = 1.0f;
      return;
   }
   z.x /= n;
   z.y /= n;
   z.z /= n;
   /* Level: square to the vertical and to the way to the eye. */
   x.x  = z.z;
   x.y  = 0.0f;
   x.z  = -z.x;
   n    = (float)sqrt(x.x * x.x + x.z * x.z);
   if (n < 0.0001f)
   {
      x.x = 1.0f;
      x.z = 0.0f;
   }
   else
   {
      x.x /= n;
      x.z /= n;
   }
   y.x = z.y * x.z - z.z * x.y;
   y.y = z.z * x.x - z.x * x.z;
   y.z = z.x * x.y - z.y * x.x;
   video_xr_quat_axes(&x, &y, &z, &pose->orientation);
}

void video_xr_outline(const video_xr_quad_t *q, video_xr_quad_t *out)
{
   const float inner = 1.0f - 2.0f * (VIDEO_XR_OUTLINE_EDGE + 0.5f)
      / VIDEO_XR_OUTLINE_DIM;
   *out        = *q;
   out->width  = (q->width + 2.0f * VIDEO_XR_OUTLINE_GAP) / inner;
   out->height = (q->height + 2.0f * VIDEO_XR_OUTLINE_GAP) / inner;
}

void video_xr_outline_crop(video_xr_quad_t *o, unsigned hide, bool back,
      unsigned *x, unsigned *w)
{
   unsigned lo, hi;
   float texel;
   video_xr_vec3_t shift, world;
   *x = 0;
   *w = VIDEO_XR_OUTLINE_DIM;
   if (!hide)
      return;
   /* Turned to the head, its x runs the item's other way. */
   if (back)
      hide = (hide & 3u) | ((hide & 4u) << 1) | ((hide & 8u) >> 1);
   if (hide & 3u)
   {
      video_xr_pose_t turn;
      float t;
      video_xr_pose_identity(&turn);
      turn.orientation.z = 0.70710678f;
      turn.orientation.w = 0.70710678f;
      video_xr_compose(&o->pose, &turn, &o->pose);
      t         = o->width;
      o->width  = o->height;
      o->height = t;
      hi        = hide & 1u;
      lo        = (hide >> 1) & 1u;
   }
   else
   {
      hi = (hide >> 2) & 1u;
      lo = (hide >> 3) & 1u;
   }
   *x       = lo * VIDEO_XR_OUTLINE_CROP;
   *w       = VIDEO_XR_OUTLINE_DIM - (lo + hi) * VIDEO_XR_OUTLINE_CROP;
   texel    = o->width / VIDEO_XR_OUTLINE_DIM;
   shift.x  = 0.5f * texel * VIDEO_XR_OUTLINE_CROP
      * ((float)lo - (float)hi);
   shift.y  = 0.0f;
   shift.z  = 0.0f;
   video_xr_rotate(&o->pose.orientation, &shift, &world);
   o->pose.position.x += world.x;
   o->pose.position.y += world.y;
   o->pose.position.z += world.z;
   o->width  = texel * (float)*w;
}

/* The hide bits of each VIDEO_XR_CUT_X cut. */
static const uint8_t video_xr_cuts[VIDEO_XR_OUTLINE_CUTS] = {
   1u, 2u, 4u, 8u, 3u, 12u };

unsigned video_xr_outline_cell(unsigned hide, bool back, unsigned lit)
{
   unsigned c;
   if (back)
      hide = (hide & 3u) | ((hide & 4u) << 1) | ((hide & 8u) >> 1);
   hide &= (hide & 3u) ? 3u : 12u;
   for (c = 0; c < VIDEO_XR_OUTLINE_CUTS; c++)
      if (video_xr_cuts[c] == hide)
         return VIDEO_XR_CUT_X(c, lit);
   return VIDEO_XR_OUTLINE_X(lit);
}

/* A pull's share: whole within r, none from 2r, falling between. */
static double video_xr_snap_share(double d, double r)
{
   d = fabs(d);
   if (d <= r)
      return 1.0;
   if (d >= 2.0 * r)
      return 0.0;
   return (2.0 * r - d) / r;
}

/* How far a snap moves what is d off its line: all the way within r,
 * then less by as much as d grows, nothing from 2r, so the item
 * catches up with the hand at twice its speed. */
static double video_xr_snap_pull(double d, double r)
{
   double a = fabs(d);
   if (a <= r)
      return d;
   if (a >= 2.0 * r)
      return 0.0;
   return (d < 0.0) ? a - 2.0 * r : 2.0 * r - a;
}

static double video_xr_quat_dot(const video_xr_quat_t *a,
      const video_xr_quat_t *b)
{
   return (double)a->x * b->x + (double)a->y * b->y
      + (double)a->z * b->z + (double)a->w * b->w;
}

/* Degrees between two turns. */
static double video_xr_quat_angle(const video_xr_quat_t *a,
      const video_xr_quat_t *b)
{
   double d = fabs(video_xr_quat_dot(a, b));
   return (d >= 1.0) ? 0.0 : 2.0 * acos(d) * 180.0 / VIDEO_XR_PI;
}

/* q turned k of the way to to, the short way; exactly to from k 1. */
static void video_xr_quat_toward(video_xr_quat_t *q,
      const video_xr_quat_t *to, double k)
{
   double wq, wt, n;
   double d = video_xr_quat_dot(q, to);
   double s = (d < 0.0) ? -1.0 : 1.0;
   if (k >= 1.0)
   {
      *q = *to;
      return;
   }
   if (k <= 0.0)
      return;
   d = fabs(d);
   if (d > 0.9999)
   {
      wq = 1.0 - k;
      wt = k;
   }
   else
   {
      double a = acos(d);
      wq       = sin((1.0 - k) * a) / sin(a);
      wt       = sin(k * a) / sin(a);
   }
   wt  *= s;
   q->x = (float)(wq * q->x + wt * to->x);
   q->y = (float)(wq * q->y + wt * to->y);
   q->z = (float)(wq * q->z + wt * to->z);
   q->w = (float)(wq * q->w + wt * to->w);
   n    = sqrt(video_xr_quat_dot(q, q));
   if (n > 0.0)
   {
      q->x = (float)(q->x / n);
      q->y = (float)(q->y / n);
      q->z = (float)(q->z / n);
      q->w = (float)(q->w / n);
   }
}

float video_xr_snap_fade(const video_xr_pose_t *start,
      const video_xr_pose_t *pose)
{
   double dx   = (double)pose->position.x - start->position.x;
   double dy   = (double)pose->position.y - start->position.y;
   double dz   = (double)pose->position.z - start->position.z;
   double k    = sqrt(dx * dx + dy * dy + dz * dz)
      / VIDEO_XR_SNAP_FADE_DIST;
   double turn = video_xr_quat_angle(&start->orientation,
         &pose->orientation) / VIDEO_XR_SNAP_FADE_ANGLE;
   if (turn > k)
      k = turn;
   return (k < 1.0) ? (float)k : 1.0f;
}

/* Edit Smoothing's levels, Low to High: the turn's least cutoff in Hz
 * and its rise per radian a second, then the position's and its rise
 * per metre a second. */
static const float video_xr_smooth_levels[3][4] = {
   { 2.0f, 0.6f, 4.0f, 1.0f },
   { 1.0f, 0.4f, 2.0f, 0.7f },
   { 0.5f, 0.3f, 1.0f, 0.5f }
};

/* A smoother's share of the way for a step of dt at a cutoff of hz. */
static double video_xr_smooth_alpha(double dt, double hz)
{
   double tau = 1.0 / (2.0 * VIDEO_XR_PI * hz);
   return 1.0 / (1.0 + tau / dt);
}

static bool video_xr_smooth_jumped(const video_xr_pose_t *a,
      const video_xr_pose_t *b)
{
   double dx = (double)b->position.x - a->position.x;
   double dy = (double)b->position.y - a->position.y;
   double dz = (double)b->position.z - a->position.z;
   return sqrt(dx * dx + dy * dy + dz * dz) > VIDEO_XR_SMOOTH_JUMP_M
      || video_xr_quat_angle(&a->orientation, &b->orientation)
         > VIDEO_XR_SMOOTH_JUMP_DEG;
}

void video_xr_smooth_step(video_xr_smooth_t *s, unsigned level,
      const video_xr_pose_t *raw, double dt, video_xr_pose_t *out)
{
   const float *k;
   double dx, dy, dz, move, turn, d, a;
   if (     !level || level >= VIDEO_OPENXR_EDIT_SMOOTHING_LAST
         || !s->on || dt <= 0.0 || video_xr_smooth_jumped(&s->raw, raw))
   {
      s->raw   = *raw;
      s->pose  = *raw;
      s->speed = 0.0f;
      s->turn  = 0.0f;
      s->on    = level && level < VIDEO_OPENXR_EDIT_SMOOTHING_LAST;
      *out     = *raw;
      return;
   }
   k     = video_xr_smooth_levels[level - 1];
   /* The speeds from the last smoothed pose, as the filter's own. */
   dx    = (double)raw->position.x - s->pose.position.x;
   dy    = (double)raw->position.y - s->pose.position.y;
   dz    = (double)raw->position.z - s->pose.position.z;
   move  = sqrt(dx * dx + dy * dy + dz * dz) / dt;
   turn  = video_xr_quat_angle(&s->pose.orientation, &raw->orientation)
      * VIDEO_XR_PI / 180.0 / dt;
   d     = video_xr_smooth_alpha(dt, VIDEO_XR_SMOOTH_DCUTOFF);
   s->speed += (float)(d * (move - s->speed));
   s->turn  += (float)(d * (turn - s->turn));
   a     = video_xr_smooth_alpha(dt, k[2] + k[3] * s->speed);
   s->pose.position.x += (float)(a * dx);
   s->pose.position.y += (float)(a * dy);
   s->pose.position.z += (float)(a * dz);
   a     = video_xr_smooth_alpha(dt, k[0] + k[1] * s->turn);
   video_xr_quat_toward(&s->pose.orientation, &raw->orientation, a);
   s->raw = *raw;
   *out   = s->pose;
}

/* A screen a held one may snap to: the turn it is nearest, parallel or
 * square, how many degrees off that it is, how near its edges are, and
 * the share of its pulls: the gap's, times the grab's fade-in. */
typedef struct video_xr_snap_target
{
   const video_xr_quad_t *q;
   video_xr_quat_t goal;
   double angle;
   double dist;
   double share;
   bool square;
} video_xr_snap_target_t;

/* The best line yet on one of the held screen's axes: how far off it
 * is, the share of the pull it gets, and how far that pull moves it. */
typedef struct video_xr_snap_line
{
   double d;
   double k;
   double pull;
   bool any;
} video_xr_snap_line_t;

/* Nearer by less than this, in metres or degrees, is rounding: a tie. */
#define VIDEO_XR_SNAP_TIE 1e-5
/* Past square by less than this, in degrees, is square: a step parks
 * a swing exactly there, where rounding would straddle the limit. */
#define VIDEO_XR_SNAP_FOLD_TIE 0.01

/* -1 when a is below b by more than rounding, 1 above, else 0. */
static int video_xr_snap_cmp(double a, double b)
{
   if (a < b - VIDEO_XR_SNAP_TIE)
      return -1;
   return (a > b + VIDEO_XR_SNAP_TIE) ? 1 : 0;
}

/* The line kept is the one left nearest after its pull (a whole pull
 * leaves exactly 0), then the stronger pull, then the nearest, then
 * the first. */
static void video_xr_snap_offer_pull(video_xr_snap_line_t *l, double d,
      double k, double pull)
{
   if (k <= 0.0 || fabs(d) >= 2.0 * VIDEO_XR_SNAP_DIST)
      return;
   if (l->any)
   {
      double e = fabs(d) - fabs(pull);
      double f = fabs(l->d) - fabs(l->pull);
      int c    = (e < f) ? -1 : (e > f) ? 1 : 0;
      if (!c)
         c = (k > l->k) ? -1 : (k < l->k) ? 1 : 0;
      if (!c)
         c = video_xr_snap_cmp(fabs(d), fabs(l->d));
      if (c >= 0)
         return;
   }
   l->d    = d;
   l->k    = k;
   l->pull = pull;
   l->any  = true;
}

static void video_xr_snap_offer(video_xr_snap_line_t *l, double d,
      double k)
{
   video_xr_snap_offer_pull(l, d, k,
         video_xr_snap_pull(d, VIDEO_XR_SNAP_DIST) * k);
}

/* A line at the set gap: whole within VIDEO_XR_SNAP_CLICK, fading out
 * over as far again, none past. */
static void video_xr_snap_offer_click(video_xr_snap_line_t *l, double d,
      double k)
{
   if (fabs(d) < 2.0 * VIDEO_XR_SNAP_CLICK)
      video_xr_snap_offer_pull(l, d, k,
            video_xr_snap_pull(d, VIDEO_XR_SNAP_CLICK) * k);
}

/* How much of the way to t's goal its pull turns the screen. */
static double video_xr_snap_turn(const video_xr_snap_target_t *t)
{
   return (t->angle > 0.0) ? t->share
      * video_xr_snap_pull(t->angle, VIDEO_XR_SNAP_ANGLE) / t->angle
      : t->share;
}

static double video_xr_snap_nearer(double a, double b)
{
   return (fabs(a) <= fabs(b)) ? a : b;
}

/* Goal i of a screen snapping to q: 0 parallel, q's own turn; 1 and 2
 * square, a quarter turn either way about its up. */
static void video_xr_snap_goal(const video_xr_quad_t *q, unsigned i,
      video_xr_quat_t *goal)
{
   video_xr_pose_t turn, g;
   if (!i)
   {
      *goal = q->pose.orientation;
      return;
   }
   video_xr_pose_identity(&turn);
   turn.orientation.y = (i == 1) ? 0.70710678f : -0.70710678f;
   turn.orientation.w = 0.70710678f;
   video_xr_compose(&q->pose, &turn, &g);
   *goal = g.orientation;
}

/* q as a target for a screen at pose, a and b its half width and
 * height; false when too far off in turn or place to pull at all. */
static bool video_xr_snap_target(const video_xr_quad_t *q,
      const video_xr_pose_t *pose, double a, double b,
      video_xr_snap_target_t *t)
{
   unsigned i;
   double gx, gy, gz;
   video_xr_pose_t inv, rel;
   t->q      = q;
   video_xr_snap_goal(q, 0, &t->goal);
   t->angle  = video_xr_quat_angle(&pose->orientation, &t->goal);
   t->square = false;
   for (i = 1; i < 3; i++)
   {
      double e;
      video_xr_quat_t goal;
      video_xr_snap_goal(q, i, &goal);
      e = video_xr_quat_angle(&pose->orientation, &goal);
      if (e < t->angle)
      {
         t->goal   = goal;
         t->angle  = e;
         t->square = true;
      }
   }
   if (t->angle >= 2.0 * VIDEO_XR_SNAP_ANGLE)
      return false;
   /* The gap as if lined up: square, the held screen's width runs along
    * the target's facing. */
   video_xr_pose_invert(&q->pose, &inv);
   video_xr_compose(&inv, pose, &rel);
   gx = fabs(rel.position.x) - q->width * 0.5 - (t->square ? 0.0 : a);
   gy = fabs(rel.position.y) - q->height * 0.5 - b;
   gz = fabs(rel.position.z) - (t->square ? a : 0.0);
   if (gx < 0.0)
      gx = 0.0;
   if (gy < 0.0)
      gy = 0.0;
   if (gz < 0.0)
      gz = 0.0;
   t->dist  = sqrt(gx * gx + gy * gy + gz * gz);
   t->share = video_xr_snap_share(t->dist, VIDEO_XR_SNAP_NEAR);
   return t->share > 0.0;
}

/* Edge e of a screen a by b metres from its middle to its sides, in its
 * own frame: 0 top, 1 bottom, 2 right, 3 left. Writes its middle, the
 * way out of the screen across it and half its length; returns the
 * axis it runs along, 0 X or 1 Y. */
static unsigned video_xr_snap_edge(unsigned e, double a, double b,
      video_xr_vec3_t *mid, video_xr_vec3_t *out, double *half)
{
   float s = (e & 1u) ? -1.0f : 1.0f;
   mid->x  = 0.0f;
   mid->y  = 0.0f;
   mid->z  = 0.0f;
   *out    = *mid;
   if (e < 2)
   {
      mid->y = (float)(s * b);
      out->y = s;
      *half  = a;
      return 0;
   }
   mid->x = (float)(s * a);
   out->x = s;
   *half  = b;
   return 1;
}

/* One held edge against one target edge, the target at rel in the held
 * screen's frame: the held axis and degrees that turn them parallel,
 * where the hinge would hold the held edge and the moves, along the
 * held axes, that put it there. */
typedef struct video_xr_snap_pair
{
   video_xr_vec3_t axis;
   double angle;
   double across;      /* past the target's edge, in its plane */
   double plane;       /* out of that plane */
   double hold;        /* how whole the hinge holds it there */
   double gap;         /* metres from its place at the set gap */
   double overlap;     /* of the edges, along the line */
   double fold;        /* from flat, 180 folded shut */
   double off[3];      /* to there: the gap if it clicks, else touching */
   double pull[3];     /* the hinge's whole pulls toward there */
   double to_plane[3]; /* off, into the plane alone */
   double plane_pull[3]; /* pull, into the plane alone */
   double level[3];    /* along the edge (run), levelling centres or ends */
   double away[3];     /* out across the target's edge */
   double stop;        /* how far the stop alone pushes it back out */
   unsigned run;
} video_xr_snap_pair_t;

/* How far past touching the stop's whole range starts, gap the set
 * one: where a click pulling from below touching lets go of it, so
 * the stop holds touching over VIDEO_XR_SNAP_STOP at any gap. */
static double video_xr_snap_stop_from(double gap)
{
   return (gap > 0.0 && gap < 2.0 * VIDEO_XR_SNAP_CLICK)
      ? VIDEO_XR_SNAP_CLICK - 0.5 * gap : 0.0;
}

/* The stop's share for an edge x metres past touching: whole within
 * VIDEO_XR_SNAP_STOP past where it starts, to rounding, none from
 * VIDEO_XR_SNAP_STOP_FADE past there. */
static double video_xr_snap_stop_share(double x, double gap)
{
   double s = video_xr_snap_stop_from(gap);
   double w = s + VIDEO_XR_SNAP_STOP + VIDEO_XR_SNAP_TIE;
   double f = s + VIDEO_XR_SNAP_STOP_FADE;
   if (x <= w)
      return 1.0;
   if (x >= f)
      return 0.0;
   return (f - x) / (f - w);
}

/* Whether the stop holds that edge whole, VIDEO_XR_SNAP_STOP_ARM short
 * once a push has let its hinge go (through). */
static bool video_xr_snap_stop_held(double x, double gap, bool through)
{
   return x <= video_xr_snap_stop_from(gap) + VIDEO_XR_SNAP_STOP
      + VIDEO_XR_SNAP_TIE - (through ? VIDEO_XR_SNAP_STOP_ARM : 0.0);
}

/* How far the stop moves that edge back to touching: all the way
 * while whole, then less, so nothing hops. Through, past its short
 * hold it lets the edge in by twice the snap's tie and
 * VIDEO_XR_SNAP_STOP_SLIP of the hand's way, never less than the fade
 * does: out of the hinge's tie from just where it stops holding, so a
 * hinge takes hold only where it holds, but by too little to see. */
#define VIDEO_XR_SNAP_STOP_SLIP 0.02
static double video_xr_snap_stop(double x, double gap, bool through)
{
   double w = video_xr_snap_stop_from(gap) + VIDEO_XR_SNAP_STOP
      + VIDEO_XR_SNAP_TIE;
   double s = (x <= w) ? x : w * video_xr_snap_stop_share(x, gap);
   if (through && !video_xr_snap_stop_held(x, gap, true))
   {
      double t = x - 2.0 * VIDEO_XR_SNAP_TIE - VIDEO_XR_SNAP_STOP_SLIP
         * (x - (w - VIDEO_XR_SNAP_STOP_ARM));
      if (t < s)
         s = t;
   }
   return s;
}

/* How whole a hinge holds an edge g metres out past the other's, gap
 * the set one: as its stop past touching, whole from there to
 * VIDEO_XR_SNAP_FREE, fading out over VIDEO_XR_SNAP_DIST past that,
 * and within VIDEO_XR_SNAP_CLICK of gap, fading out over as far
 * again. */
static double video_xr_snap_gap_share(double g, double gap)
{
   double click = video_xr_snap_share(g - gap, VIDEO_XR_SNAP_CLICK);
   double x     = (g > VIDEO_XR_SNAP_FREE) ? g - VIDEO_XR_SNAP_FREE : 0.0;
   double held  = (g < 0.0) ? video_xr_snap_stop_share(-g, gap)
      : (x <= VIDEO_XR_SNAP_TIE) ? 1.0
      : video_xr_snap_share(VIDEO_XR_SNAP_DIST + x, VIDEO_XR_SNAP_DIST);
   return (click > held) ? click : held;
}

/* v moved along d until its run part is 0, d[run] above 0. */
static void video_xr_snap_slide(double *v, const double *d, unsigned run)
{
   double k = v[run] / d[run];
   v[0]    -= k * d[0];
   v[1]    -= k * d[1];
   v[2]    -= k * d[2];
}

static double video_xr_snap_dot(const double *a, const double *b)
{
   return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

/* Degrees from g to f about d, all three at right angles to d. */
static double video_xr_snap_about(const double *g, const double *f,
      const double *d)
{
   double c[3];
   c[0] = g[1] * f[2] - g[2] * f[1];
   c[1] = g[2] * f[0] - g[0] * f[2];
   c[2] = g[0] * f[1] - g[1] * f[0];
   return atan2(video_xr_snap_dot(c, d), video_xr_snap_dot(g, f))
      * 180.0 / VIDEO_XR_PI;
}

/* v less its part along d. */
static void video_xr_snap_across(double *v, const double *d)
{
   double k = video_xr_snap_dot(v, d);
   v[0]    -= k * d[0];
   v[1]    -= k * d[1];
   v[2]    -= k * d[2];
}

/* Held edge he (a, b the held screen's half size) against target edge te
 * (A, B the target's). */
static void video_xr_snap_pair(const video_xr_pose_t *rel, double A,
      double B, double a, double b, unsigned he, unsigned te, double gap,
      bool through, video_xr_snap_pair_t *p)
{
   unsigned i;
   double lt, lh, s, n, k, e, q;
   double c[3], d[3], u[3], m[3], f[3], t[3];
   video_xr_vec3_t mid, out, v;
   unsigned run = video_xr_snap_edge(te, A, B, &mid, &out, &lt);
   video_xr_rotate(&rel->orientation, &mid, &v);
   c[0]  = (double)rel->position.x + v.x;
   c[1]  = (double)rel->position.y + v.y;
   c[2]  = (double)rel->position.z + v.z;
   video_xr_rotate(&rel->orientation, &out, &v);
   u[0]  = v.x;
   u[1]  = v.y;
   u[2]  = v.z;
   mid.x = 0.0f;
   mid.y = 0.0f;
   mid.z = 1.0f;
   video_xr_rotate(&rel->orientation, &mid, &v);
   t[0]  = v.x;
   t[1]  = v.y;
   t[2]  = v.z;
   mid.x = run ? 0.0f : 1.0f;
   mid.y = run ? 1.0f : 0.0f;
   mid.z = 0.0f;
   video_xr_rotate(&rel->orientation, &mid, &v);
   d[0]  = v.x;
   d[1]  = v.y;
   d[2]  = v.z;
   run    = video_xr_snap_edge(he, a, b, &mid, &out, &lh);
   p->run = run;
   /* The target's line the held edge's way. */
   s = (d[run] < 0.0) ? -1.0 : 1.0;
   for (i = 0; i < 3; i++)
      d[i] *= s;
   /* The held edge's way crossed with it. */
   m[0] = run ? d[2] : 0.0;
   m[1] = run ? 0.0 : -d[2];
   m[2] = run ? -d[0] : d[1];
   n    = sqrt(video_xr_snap_dot(m, m));
   p->angle = atan2(n, d[run]) * 180.0 / VIDEO_XR_PI;
   p->axis.x = (float)(n > 0.0 ? m[0] / n : 1.0);
   p->axis.y = (float)(n > 0.0 ? m[1] / n : 0.0);
   p->axis.z = (float)(n > 0.0 ? m[2] / n : 0.0);
   /* The line where it passes the held edge's middle. */
   s    = (d[run] > 0.0) ? -c[run] / d[run] : 0.0;
   m[0] = mid.x;
   m[1] = mid.y;
   m[2] = 0.0;
   for (i = 0; i < 3; i++)
      p->off[i] = m[i] - c[i] - s * d[i];
   p->level[0] = c[run];
   p->level[1] = c[run] + lt - lh;
   p->level[2] = c[run] - lt + lh;
   /* Past the target's edge and out of its plane; the gap clicks in
    * VIDEO_XR_SNAP_CLICK, the plane pulls in VIDEO_XR_SNAP_DIST. Both
    * ways are square to the line, so sliding along it to the held
    * edge's middle leaves them be. */
   p->across = video_xr_snap_dot(p->off, u);
   p->plane  = video_xr_snap_dot(p->off, t);
   k = gap - p->across;
   if (fabs(k) >= 2.0 * VIDEO_XR_SNAP_CLICK)
      k = 0.0;
   e = video_xr_snap_pull(k, VIDEO_XR_SNAP_CLICK);
   /* Past touching, both push it back out: the farther wins, so the
    * click's fade hands over to the stop smoothly. */
   p->stop = (p->across < 0.0)
      ? video_xr_snap_stop(-p->across, gap, through) : 0.0;
   if (p->across < 0.0 && p->stop > e)
   {
      k = -p->across;
      e = p->stop;
   }
   q = video_xr_snap_pull(-p->plane, VIDEO_XR_SNAP_DIST);
   for (i = 0; i < 3; i++)
   {
      p->to_plane[i]   = -p->plane * t[i];
      p->plane_pull[i] = q * t[i];
      p->off[i]        = k * u[i];
      p->pull[i]       = e * u[i];
      p->away[i]       = u[i];
   }
   if (d[run] > 0.0)
   {
      video_xr_snap_slide(p->to_plane, d, run);
      video_xr_snap_slide(p->plane_pull, d, run);
      video_xr_snap_slide(p->off, d, run);
      video_xr_snap_slide(p->pull, d, run);
      video_xr_snap_slide(p->away, d, run);
   }
   for (i = 0; i < 3; i++)
   {
      p->off[i]  += p->to_plane[i];
      p->pull[i] += p->plane_pull[i];
   }
   p->gap     = sqrt(p->plane * p->plane
         + (p->across - gap) * (p->across - gap));
   p->hold    = video_xr_snap_share(p->plane, VIDEO_XR_SNAP_DIST)
      * video_xr_snap_gap_share(p->across, gap);
   p->overlap = lt * d[run] + lh - fabs(c[run]);
   /* Into the held screen from its edge, against flat: out of the
    * target across its edge. */
   f[0] = -out.x;
   f[1] = -out.y;
   f[2] = 0.0;
   video_xr_snap_across(f, d);
   video_xr_snap_across(u, d);
   p->fold = video_xr_snap_about(u, f, d);
}

/* Whether the held edge, half long, lies in the target's plane and
 * parallel to its edge to within rounding at both its ends. */
static bool video_xr_snap_on_line(const video_xr_snap_pair_t *p,
      double half)
{
   double tip = half * sin(p->angle * VIDEO_XR_PI / 180.0);
   return fabs(p->plane) + tip <= VIDEO_XR_SNAP_TIE;
}

/* Whether a held edge half long lies where its hinge engages: in the
 * target's plane on its line, free or at the set gap (the click's fade
 * leaves it between), overlapping the target's by VIDEO_XR_SNAP_DIST,
 * flat to square. */
static bool video_xr_snap_engaged(const video_xr_snap_pair_t *p,
      double half, double gap)
{
   return p->angle < 2.0 * VIDEO_XR_SNAP_ANGLE
      && video_xr_snap_on_line(p, half)
      && p->hold >= 1.0
      && (     fabs(p->across - gap) <= VIDEO_XR_SNAP_TIE
         || (     p->across >= -VIDEO_XR_SNAP_TIE
               && p->across <= VIDEO_XR_SNAP_FREE + VIDEO_XR_SNAP_TIE))
      && p->overlap >= VIDEO_XR_SNAP_DIST
      && fabs(p->fold) <= 90.0 + VIDEO_XR_SNAP_FOLD_TIE;
}

/* The edges' overlap as a share: whole from VIDEO_XR_SNAP_DIST, none
 * without, so a hinge slides off an end smoothly. */
static double video_xr_snap_overlap(const video_xr_snap_pair_t *p)
{
   double x = 2.0 * VIDEO_XR_SNAP_DIST - p->overlap;
   return video_xr_snap_share((x > 0.0) ? x : 0.0, VIDEO_XR_SNAP_DIST);
}

/* A hinge's share of its pulls: its place's, the overlap's, the angle's
 * between the screens (whole to square, none folded shut) and the
 * grab's fade. */
static double video_xr_snap_hinge_share(const video_xr_snap_pair_t *p,
      double fade)
{
   return fade * p->hold * video_xr_snap_overlap(p)
      * video_xr_snap_share(p->fold, 90.0);
}

/* The held axis, other than the edge's, the way out of the target
 * across its edge runs most along. */
static unsigned video_xr_snap_away_axis(const video_xr_snap_pair_t *p)
{
   unsigned i;
   unsigned a = p->run ? 0 : 1;
   for (i = 0; i < 3; i++)
      if (i != p->run && fabs(p->away[i]) > fabs(p->away[a]))
         a = i;
   return a;
}

/* Across the hinge's edge, the screen's own lines blended into the
 * hinge's (share k, holding w of the pull), never past it, so nothing
 * hops; a blend short of the pull ranks no better than a whole line. */
static void video_xr_snap_hinge_join(video_xr_snap_line_t *line,
      const video_xr_snap_line_t *own, const video_xr_snap_pair_t *p,
      double k, double w, double stop, bool loose)
{
   unsigned i;
   for (i = 0; i < 3; i++)
   {
      double d, pull, kk;
      if (i == p->run)
         continue;
      /* Whole, its own lines win a tie: the turn and the hinge chose
       * its screen, the nearer, in whichever order the quads come. */
      if (!loose)
      {
         if (own[i].any)
         {
            video_xr_snap_line_t other = line[i];
            line[i] = own[i];
            if (other.any)
               video_xr_snap_offer_pull(&line[i], other.d, other.k,
                     other.pull);
         }
         continue;
      }
      d    = p->off[i];
      kk   = k;
      pull = p->pull[i] * k;
      if (own[i].any)
      {
         double to    = p->to_plane[i];
         double carry = w * ((fabs(own[i].pull) < fabs(to))
               ? fabs(own[i].pull) : fabs(to));
         if (carry > fabs(p->plane_pull[i] * k))
         {
            pull = (p->pull[i] - p->plane_pull[i]) * k
               + ((to < 0.0) ? -carry : carry);
            if (own[i].k > kk)
               kk = own[i].k;
         }
         pull += (1.0 - w) * own[i].pull;
         d     = w * d + (1.0 - w) * own[i].d;
         if (fabs(pull) > fabs(d))
            d = pull;
      }
      video_xr_snap_offer_pull(&line[i], d, kk, pull);
   }
   /* Whole, its stop joins its own lines along the axis the way out
    * runs, as strong as the hinge, where it pushes farther than they
    * do: so the click hands over to it smoothly at any strength. */
   if (!loose && p->stop > 0.0)
   {
      unsigned a  = video_xr_snap_away_axis(p);
      double pull = p->stop * p->away[a] * stop;
      if (!own[a].any || fabs(pull) > fabs(own[a].pull))
         video_xr_snap_offer_pull(&line[a], -p->across * p->away[a],
               stop, pull);
   }
}

/* How near the held screen's edges (inv, its pose inverted) come to
 * q's, one measure for every target, so one stacked behind never
 * outranks a hinged one; dist past that if no edge is near parallel. */
static double video_xr_snap_edge_dist(const video_xr_quad_t *q,
      const video_xr_pose_t *inv, double a, double b, double gap,
      double dist)
{
   unsigned he, te;
   video_xr_pose_t rel;
   video_xr_snap_pair_t p;
   double reach = gap + VIDEO_XR_SNAP_FREE + VIDEO_XR_SNAP_DIST;
   dist        += reach;
   video_xr_compose(inv, &q->pose, &rel);
   for (he = 0; he < 4; he++)
   {
      for (te = 0; te < 4; te++)
      {
         double d, f, w;
         video_xr_snap_pair(&rel, q->width * 0.5, q->height * 0.5, a, b,
               he, te, gap, false, &p);
         if (p.angle >= 2.0 * VIDEO_XR_SNAP_ANGLE)
            continue;
         f = fabs(p.fold);
         w = (f <= 90.0 + VIDEO_XR_SNAP_ANGLE) ? 1.0
            : (f >= 180.0) ? 0.0
            : (180.0 - f) / (90.0 - VIDEO_XR_SNAP_ANGLE);
         d = p.gap + reach * (1.0 - video_xr_snap_overlap(&p) * w);
         if (d < dist)
            dist = d;
      }
   }
   return dist;
}

float video_xr_hinge_angle(const video_xr_hinge_t *h,
      const video_xr_quat_t *q)
{
   double f[3], g[3], d[3];
   video_xr_vec3_t mid, out, v;
   double half;
   video_xr_snap_edge(h->edge, 1.0, 1.0, &mid, &out, &half);
   out.x = -out.x;
   out.y = -out.y;
   video_xr_rotate(q, &out, &v);
   f[0] = v.x;
   f[1] = v.y;
   f[2] = v.z;
   g[0] = h->flat.x;
   g[1] = h->flat.y;
   g[2] = h->flat.z;
   d[0] = h->dir.x;
   d[1] = h->dir.y;
   d[2] = h->dir.z;
   video_xr_snap_across(f, d);
   video_xr_snap_across(g, d);
   return (float)video_xr_snap_about(g, f, d);
}

bool video_xr_hinge_line(const video_xr_hinge_t *h, float width,
      float height, const video_xr_quad_t *target, video_xr_vec3_t *a,
      video_xr_vec3_t *b)
{
   unsigned run;
   double lh, lt, s, k, lo, hi, g;
   video_xr_vec3_t mid, out, e;
   video_xr_snap_edge(h->edge, width * 0.5, height * 0.5, &mid, &out, &lh);
   run = video_xr_snap_edge(h->on, target->width * 0.5,
         target->height * 0.5, &mid, &out, &lt);
   video_xr_rotate(&target->pose.orientation, &mid, &e);
   /* Along the line from the held edge's middle: the target edge's
    * middle, and half its length seen along the line. */
   s = ((double)target->pose.position.x + e.x - h->point.x) * h->dir.x
      + ((double)target->pose.position.y + e.y - h->point.y) * h->dir.y
      + ((double)target->pose.position.z + e.z - h->point.z) * h->dir.z;
   mid.x = run ? 0.0f : 1.0f;
   mid.y = run ? 1.0f : 0.0f;
   mid.z = 0.0f;
   video_xr_rotate(&target->pose.orientation, &mid, &e);
   k  = lt * fabs((double)e.x * h->dir.x + (double)e.y * h->dir.y
         + (double)e.z * h->dir.z);
   lo = (s - k > -lh) ? s - k : -lh;
   hi = (s + k < lh) ? s + k : lh;
   if (hi - lo <= VIDEO_XR_SNAP_TIE)
      return false;
   g    = h->gap * 0.5;
   a->x = (float)(h->point.x + h->dir.x * lo - h->flat.x * g);
   a->y = (float)(h->point.y + h->dir.y * lo - h->flat.y * g);
   a->z = (float)(h->point.z + h->dir.z * lo - h->flat.z * g);
   b->x = (float)(h->point.x + h->dir.x * hi - h->flat.x * g);
   b->y = (float)(h->point.y + h->dir.y * hi - h->flat.y * g);
   b->z = (float)(h->point.z + h->dir.z * hi - h->flat.z * g);
   return true;
}

void video_xr_hinge_place(video_xr_hinge_t *h, const video_xr_pose_t *pose,
      float width, float height, const video_xr_quad_t *target, bool over)
{
   unsigned i, run;
   double half, n, x;
   double m[3], d[3], e[3], f[3], g[3];
   video_xr_vec3_t mid, out, v;
   run  = video_xr_snap_edge(h->edge, width * 0.5, height * 0.5, &mid,
         &out, &half);
   video_xr_rotate(&pose->orientation, &mid, &v);
   m[0] = (double)pose->position.x + v.x;
   m[1] = (double)pose->position.y + v.y;
   m[2] = (double)pose->position.z + v.z;
   mid.x = run ? 0.0f : 1.0f;
   mid.y = run ? 1.0f : 0.0f;
   mid.z = 0.0f;
   video_xr_rotate(&pose->orientation, &mid, &v);
   d[0] = v.x;
   d[1] = v.y;
   d[2] = v.z;
   run  = video_xr_snap_edge(h->on, target->width * 0.5,
         target->height * 0.5, &mid, &out, &half);
   video_xr_rotate(&target->pose.orientation, &mid, &v);
   g[0] = m[0] - target->pose.position.x - v.x;
   g[1] = m[1] - target->pose.position.y - v.y;
   g[2] = m[2] - target->pose.position.z - v.z;
   video_xr_rotate(&target->pose.orientation, &out, &v);
   f[0] = v.x;
   f[1] = v.y;
   f[2] = v.z;
   mid.x = run ? 0.0f : 1.0f;
   mid.y = run ? 1.0f : 0.0f;
   mid.z = 0.0f;
   video_xr_rotate(&target->pose.orientation, &mid, &v);
   e[0] = v.x;
   e[1] = v.y;
   e[2] = v.z;
   /* The line's way midway between the edges'. */
   x    = (video_xr_snap_dot(d, e) < 0.0) ? -1.0 : 1.0;
   for (i = 0; i < 3; i++)
   {
      e[i] *= x;
      d[i] += e[i];
   }
   n = sqrt(video_xr_snap_dot(d, d));
   for (i = 0; i < 3; i++)
      d[i] /= n;
   /* From the target's edge to the held edge's middle, square to both
    * edges, so the line's ends stay where the held edge's are. */
   video_xr_snap_across(g, e);
   video_xr_snap_across(g, d);
   x = video_xr_snap_dot(g, f);
   if (x < 0.0)
   {
      for (i = 0; i < 3; i++)
      {
         g[i] -= x * f[i];
         if (!over)
            m[i] -= x * f[i];
      }
   }
   n          = sqrt(video_xr_snap_dot(g, g));
   h->point.x = (float)m[0];
   h->point.y = (float)m[1];
   h->point.z = (float)m[2];
   h->dir.x   = (float)d[0];
   h->dir.y   = (float)d[1];
   h->dir.z   = (float)d[2];
   h->flat.x  = (float)((n > VIDEO_XR_SNAP_TIE) ? g[0] / n : f[0]);
   h->flat.y  = (float)((n > VIDEO_XR_SNAP_TIE) ? g[1] / n : f[1]);
   h->flat.z  = (float)((n > VIDEO_XR_SNAP_TIE) ? g[2] / n : f[2]);
   h->gap     = (float)n;
}

/* video_xr_snap_hinge, but the screens in whole (slots, as bits) pull
 * whole whatever fade: a hinge's target as a grab taken in it, or
 * taking a screen where it rests, holds it. */
static unsigned video_xr_snap_hinge_at(const video_xr_quad_set_t *set,
      unsigned slot, unsigned skip, unsigned whole, float width,
      float height, float gap, float fade, bool through,
      video_xr_pose_t *pose, video_xr_hinge_t *hinge)
{
   unsigned i, he, te;
   double e;
   double turn = 0.0;
   video_xr_snap_target_t t[RETRO_VIDEO_VIEWS_MAX];
   const video_xr_quad_t *h[RETRO_VIDEO_VIEWS_MAX];
   video_xr_snap_line_t line[3], own[3];
   video_xr_snap_pair_t p;
   video_xr_pose_t inv, rel, turned;
   video_xr_vec3_t move, world;
   double pull[3];
   unsigned n       = 0;
   unsigned nh      = 0;
   unsigned done    = 0;
   unsigned snapped = 0;
   int ref          = -1;
   int hq           = -1;
   unsigned hhe     = 0;
   unsigned hte     = 0;
   double hk        = 0.0;
   double ha        = 0.0;
   double hg        = 0.0;
   double hw        = 0.0;
   double hn        = 0.0;
   double hs        = 0.0;
   double hin       = 0.0;
   double hk_line   = 0.0;
   double hk_stop   = 0.0;
   double hf        = 0.0;
   bool joined      = false;
   bool pressed     = false;
   double a         = width * 0.5;
   double b         = height * 0.5;

   if (!set || slot >= VIDEO_XR_MENU_SLOT || (fade <= 0.0f && !whole))
      return 0;
   video_xr_pose_invert(pose, &inv);
   for (i = 0; i < set->num_quads && nh < RETRO_VIDEO_VIEWS_MAX; i++)
   {
      const video_xr_quad_t *q = &set->quads[i];
      if (     q->slot >= VIDEO_XR_MENU_SLOT || q->slot == slot
            || q->kind == VIDEO_XR_QUAD_MENU
            || (((skip | done) >> q->slot) & 1u)
            || set->slots[q->slot].fixed)
         continue;
      done   |= 1u << q->slot;
      h[nh++] = q;
      if (video_xr_snap_target(q, pose, a, b, &t[n]))
      {
         t[n].share *= ((whole >> q->slot) & 1u) ? 1.0 : fade;
         t[n].dist   = video_xr_snap_edge_dist(q, &inv, a, b, gap,
               t[n].dist);
         n++;
      }
   }
   if (!nh)
      return 0;

   /* The turn: while the rest fade in, to a screen pulling whole, not a
    * nearer one still fading; of two in its whole snap, to the nearer,
    * so it turns to the screen it hinges to; else to the target whose
    * pull leaves it nearest parallel or square, then the stronger pull,
    * the smaller turn, the nearer, the first. */
   for (i = 0; i < n; i++)
   {
      double k = video_xr_snap_turn(&t[i]);
      int c    = -1;
      int lead = (ref >= 0 && fade < 1.0f)
         ? (int)((whole >> t[ref].q->slot) & 1u)
            - (int)((whole >> t[i].q->slot) & 1u) : 0;
      if (ref >= 0)
      {
         bool both = t[i].angle <= VIDEO_XR_SNAP_ANGLE
            && t[ref].angle <= VIDEO_XR_SNAP_ANGLE;
         c = lead;
         if (!c && both)
            c = video_xr_snap_cmp(t[i].dist, t[ref].dist);
         if (!c)
            c = video_xr_snap_cmp(t[i].angle * (1.0 - k),
                  t[ref].angle * (1.0 - turn));
         if (!c)
            c = (k > turn) ? -1 : (k < turn) ? 1 : 0;
         if (!c && !both)
            c = (t[i].angle < t[ref].angle) ? -1
               : (t[i].angle > t[ref].angle) ? 1 : 0;
         if (!c && t[i].dist < t[ref].dist)
            c = -1;
      }
      if (c < 0)
      {
         ref  = (int)i;
         turn = k;
      }
   }

   /* A hinge: one of the held screen's edges turned on parallel to a
    * target's, at any angle between the screens, by the same rule, the
    * smaller turn only past the whole snap, then the nearer; to a screen
    * the turn does not go to, only as far as that turn leaves it free. */
   for (i = 0; i < nh; i++)
   {
      double f = ((whole >> h[i]->slot) & 1u) ? 1.0 : fade;
      video_xr_compose(&inv, &h[i]->pose, &rel);
      for (he = 0; he < 4; he++)
      {
         for (te = 0; te < 4; te++)
         {
            int c;
            double k;
            video_xr_snap_pair(&rel, h[i]->width * 0.5,
                  h[i]->height * 0.5, a, b, he, te, gap, false, &p);
            if (p.angle >= 2.0 * VIDEO_XR_SNAP_ANGLE)
               continue;
            k = video_xr_snap_hinge_share(&p, f);
            if (k <= 0.0)
               continue;
            if (ref >= 0 && t[ref].q != h[i])
               k *= 1.0 - turn;
            if (p.angle > 0.0)
               k *= video_xr_snap_pull(p.angle, VIDEO_XR_SNAP_ANGLE)
                  / p.angle;
            c = (hq < 0) ? -1 : video_xr_snap_cmp(p.angle * (1.0 - k),
                  ha * (1.0 - hk));
            if (!c)
               c = (k > hk) ? -1 : (k < hk) ? 1 : 0;
            if (     !c && (p.angle > VIDEO_XR_SNAP_ANGLE
                  || ha > VIDEO_XR_SNAP_ANGLE))
               c = video_xr_snap_cmp(p.angle, ha);
            if (!c)
               c = video_xr_snap_cmp(p.gap, hg);
            if (c < 0)
            {
               hq  = (int)i;
               hhe = he;
               hte = te;
               hk  = k;
               ha  = p.angle;
               hg  = p.gap;
               hn  = p.hold;
               hw  = p.hold * video_xr_snap_overlap(&p);
               hs  = video_xr_snap_hinge_share(&p, f);
               hf  = f;
               hin = p.angle;
            }
         }
      }
   }

   if (ref >= 0)
   {
      video_xr_vec3_t at, from, to;
      double half;
      e = t[ref].angle;
      /* Hinged to the same screen, it turns about its own edge, so a
       * swing out of flat or square keeps the edge's gap. */
      video_xr_snap_edge(hhe, a, b, &at, &from, &half);
      half   = (hq >= 0 && h[hq] == t[ref].q) ? hn * hw : 0.0;
      at.x   = (float)(at.x * half);
      at.y   = (float)(at.y * half);
      video_xr_rotate(&pose->orientation, &at, &from);
      video_xr_quat_toward(&pose->orientation, &t[ref].goal, turn);
      video_xr_rotate(&pose->orientation, &at, &to);
      pose->position.x += from.x - to.x;
      pose->position.y += from.y - to.y;
      pose->position.z += from.z - to.z;
      if (e <= VIDEO_XR_SNAP_ANGLE && t[ref].share >= 1.0)
         snapped |= VIDEO_XR_SNAP_TURN;
      if (turn > 0.0)
         snapped |= VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_TURN);
   }
   video_xr_pose_invert(pose, &turned);
   if (hq >= 0)
   {
      /* The rest of the way parallel, as the turn left it. */
      video_xr_compose(&turned, &h[hq]->pose, &rel);
      video_xr_snap_pair(&rel, h[hq]->width * 0.5, h[hq]->height * 0.5,
            a, b, hhe, hte, gap, false, &p);
      ha = p.angle;
      hk = (ha < 2.0 * VIDEO_XR_SNAP_ANGLE) ? hs : 0.0;
      if (ref >= 0 && t[ref].q != h[hq])
         hk *= 1.0 - turn;
      if (ha > 0.0)
         hk *= video_xr_snap_pull(ha, VIDEO_XR_SNAP_ANGLE) / ha;
      if (ha * hk > VIDEO_XR_SNAP_TIE)
      {
         video_xr_pose_t held, by;
         video_xr_vec3_t at, out, from, to;
         double half;
         double s = sin(ha * hk * VIDEO_XR_PI / 360.0);
         video_xr_pose_identity(&held);
         video_xr_pose_identity(&by);
         held.orientation = pose->orientation;
         by.orientation.x = (float)(p.axis.x * s);
         by.orientation.y = (float)(p.axis.y * s);
         by.orientation.z = (float)(p.axis.z * s);
         by.orientation.w = (float)cos(ha * hk * VIDEO_XR_PI / 360.0);
         /* About its edge's middle, so its place along the line
          * keeps through any roll. */
         video_xr_snap_edge(hhe, a, b, &at, &out, &half);
         video_xr_rotate(&pose->orientation, &at, &from);
         video_xr_compose(&held, &by, &held);
         pose->orientation = held.orientation;
         video_xr_rotate(&pose->orientation, &at, &to);
         pose->position.x += from.x - to.x;
         pose->position.y += from.y - to.y;
         pose->position.z += from.z - to.z;
      }
   }

   /* Lines along the held screen's own axes, as now turned; they fade
    * with each target's turn. The hinge's screen's lines across its
    * edge are kept apart, to join the hinge's own. */
   memset(line, 0, sizeof(line));
   memset(own, 0, sizeof(own));
   video_xr_pose_invert(pose, &inv);
   hk_line = hs * video_xr_snap_share(hin, VIDEO_XR_SNAP_ANGLE);
   if (hq >= 0)
   {
      double fold;
      video_xr_compose(&inv, &h[hq]->pose, &rel);
      video_xr_snap_pair(&rel, h[hq]->width * 0.5, h[hq]->height * 0.5,
            a, b, hhe, hte, gap, through, &p);
      /* As strong as the hinge where the hand holds it, the share
       * that chose it and set the turn's pivot, so the stop lets go
       * with it; folded as the turn put it, square to rounding while
       * that is whole. */
      fold    = fabs(p.fold) - VIDEO_XR_SNAP_FOLD_TIE;
      hk_stop = hf * hw
         * video_xr_snap_share((fold > 0.0) ? fold : 0.0, 90.0);
   }
   for (i = 0; i < n; i++)
   {
      unsigned j;
      double x, y, z;
      video_xr_snap_line_t *l[3];
      double A = t[i].q->width * 0.5;
      double B = t[i].q->height * 0.5;
      double k = t[i].share
         * video_xr_snap_share(t[i].angle, VIDEO_XR_SNAP_ANGLE);
      for (j = 0; j < 3; j++)
         l[j] = (hq >= 0 && t[i].q == h[hq] && j != p.run)
            ? &own[j] : &line[j];
      video_xr_compose(&inv, &t[i].q->pose, &rel);
      x = rel.position.x;
      y = rel.position.y;
      z = rel.position.z;
      if (!t[i].square)
      {
         video_xr_snap_offer(l[0], x - A + a, k);
         video_xr_snap_offer(l[0], x + A - a, k);
         video_xr_snap_offer(l[0], x,         k);
         video_xr_snap_offer_click(l[0], x + A + a + gap, k);
         video_xr_snap_offer_click(l[0], x - A - a - gap, k);
         video_xr_snap_offer(l[1], y + B - b, k);
         video_xr_snap_offer(l[1], y - B + b, k);
         video_xr_snap_offer(l[1], y,         k);
         video_xr_snap_offer_click(l[1], y + B + b + gap, k);
         video_xr_snap_offer_click(l[1], y - B - b - gap, k);
         video_xr_snap_offer(l[2], z,         k);
      }
      else
      {
         /* A vertical edge into its plane, its own plane the gap out
          * past one of its vertical edges: a corner, held while the gap
          * is, clicking to the set one. */
         double g1 = z - A;
         double g2 = -z - A;
         double dx = video_xr_snap_nearer(x - a, x + a);
         double dz = (g1 >= g2) ? g1 - gap : gap - g2;
         video_xr_snap_offer(l[0], dx,
               k * video_xr_snap_gap_share((g1 >= g2) ? g1 : g2, gap));
         video_xr_snap_offer_click(l[2], dz,
               k * video_xr_snap_share(dx, VIDEO_XR_SNAP_DIST));
         video_xr_snap_offer(l[1], y + B - b, k);
         video_xr_snap_offer(l[1], y - B + b, k);
         video_xr_snap_offer(l[1], y,         k);
      }
      if (l[0] == &own[0] || l[1] == &own[1])
      {
         video_xr_snap_hinge_join(line, own, &p, hk_line, hw, hk_stop,
               t[i].angle > VIDEO_XR_SNAP_ANGLE);
         joined = true;
      }
   }
   /* The hinge: its edge onto the line, joined across it with its
    * screen's own lines there, each by how hard it pulls, so neither
    * hops to the other; along it centres or ends level. */
   if (hq >= 0)
   {
      if (!joined)
         video_xr_snap_hinge_join(line, own, &p, hk_line, hw, hk_stop,
               true);
      video_xr_snap_offer(&line[p.run], p.level[0], hk_line);
      video_xr_snap_offer(&line[p.run], p.level[1], hk_line);
      video_xr_snap_offer(&line[p.run], p.level[2], hk_line);
   }
   if (     hq >= 0 && hk_line > 0.0
         && fabs(p.across - gap) < 2.0 * VIDEO_XR_SNAP_CLICK)
      snapped |= VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_GAP);
   /* Near touching the stop counts as pulling, on its axis and on the
    * gap's bit, so it ticks once per push. */
   if (hq >= 0 && hk_stop > 0.0 && p.across < VIDEO_XR_SNAP_STOP_ARM)
   {
      snapped |= VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_GAP
            | (VIDEO_XR_SNAP_X << video_xr_snap_away_axis(&p)));
      pressed  = p.across < 0.0;
   }
   for (i = 0; i < 3; i++)
   {
      pull[i] = 0.0;
      if (!line[i].any)
         continue;
      snapped |= VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_X << i);
      pull[i] = line[i].pull;
      if (     line[i].k >= 1.0
            && fabs(line[i].d) - fabs(line[i].pull) <= VIDEO_XR_SNAP_TIE)
         snapped |= VIDEO_XR_SNAP_X << i;
   }
   move.x = (float)pull[0];
   move.y = (float)pull[1];
   move.z = (float)pull[2];
   video_xr_rotate(&pose->orientation, &move, &world);
   pose->position.x += world.x;
   pose->position.y += world.y;
   pose->position.z += world.z;

   /* Engaged, by whatever pull put it there, its fade whole. */
   if (hq >= 0 && hf >= 1.0)
   {
      video_xr_pose_invert(pose, &inv);
      video_xr_compose(&inv, &h[hq]->pose, &rel);
      video_xr_snap_pair(&rel, h[hq]->width * 0.5, h[hq]->height * 0.5,
            a, b, hhe, hte, gap, false, &p);
      if (video_xr_snap_engaged(&p, hhe < 2 ? a : b, gap))
      {
         snapped |= VIDEO_XR_SNAP_HINGE;
         if (     fabs(p.across - gap) <= VIDEO_XR_SNAP_TIE
               || (pressed && fabs(p.across) <= VIDEO_XR_SNAP_TIE))
            snapped |= VIDEO_XR_SNAP_GAP;
         if (hinge)
         {
            video_xr_vec3_t mid, out, x;
            double half, tx, ty;
            video_xr_snap_edge(hte, 1.0, 1.0, &mid, &out, &half);
            video_xr_rotate(&h[hq]->pose.orientation, &out, &hinge->flat);
            p.run = video_xr_snap_edge(hhe, a, b, &mid, &out, &half);
            video_xr_rotate(&pose->orientation, &mid, &x);
            hinge->point.x = pose->position.x + x.x;
            hinge->point.y = pose->position.y + x.y;
            hinge->point.z = pose->position.z + x.z;
            mid.x = p.run ? 0.0f : 1.0f;
            mid.y = p.run ? 1.0f : 0.0f;
            mid.z = 0.0f;
            video_xr_rotate(&pose->orientation, &mid, &hinge->dir);
            hinge->edge  = hhe;
            hinge->slot  = h[hq]->slot;
            hinge->on    = hte;
            hinge->gap   = (float)p.across;
            hinge->angle = video_xr_hinge_angle(hinge, &pose->orientation);
            /* The tilt turns about the screen's own X, the turn about
             * the vertical: whichever lies nearer the line. */
            mid.x = 1.0f;
            mid.y = 0.0f;
            video_xr_rotate(&pose->orientation, &mid, &x);
            tx = fabs((double)x.x * hinge->dir.x + (double)x.y * hinge->dir.y
                  + (double)x.z * hinge->dir.z);
            ty = fabs(hinge->dir.y);
            hinge->axis  = (tx >= ty) ? VIDEO_XR_STEP_TILT
               : VIDEO_XR_STEP_TURN;
         }
      }
   }
   return snapped;
}

unsigned video_xr_snap_hinge(const video_xr_quad_set_t *set, unsigned slot,
      unsigned skip, float width, float height, float gap, float fade,
      video_xr_pose_t *pose, video_xr_hinge_t *hinge)
{
   return video_xr_snap_hinge_at(set, slot, skip, 0, width, height, gap,
         fade, false, pose, hinge);
}

unsigned video_xr_snap(const video_xr_quad_set_t *set, unsigned slot,
      unsigned skip, float width, float height, float fade,
      video_xr_pose_t *pose)
{
   return video_xr_snap_hinge(set, slot, skip, width, height, 0.0f, fade,
         pose, NULL);
}

/* The screens of set a hinge can take, but those in skip (slots, as
 * bits), as video_xr_snap_hinge's targets: a stereo pair once, at most
 * RETRO_VIDEO_VIEWS_MAX into s. Returns how many. */
static unsigned video_xr_rest_screens(const video_xr_quad_set_t *set,
      unsigned skip, const video_xr_quad_t **s)
{
   unsigned i;
   unsigned n    = 0;
   unsigned done = 0;
   for (i = 0; i < set->num_quads && n < RETRO_VIDEO_VIEWS_MAX; i++)
   {
      const video_xr_quad_t *q = &set->quads[i];
      if (     q->slot >= VIDEO_XR_MENU_SLOT
            || q->kind == VIDEO_XR_QUAD_MENU
            || (((skip | done) >> q->slot) & 1u)
            || set->slots[q->slot].fixed)
         continue;
      done  |= 1u << q->slot;
      s[n++] = q;
   }
   return n;
}

/* Whether a screen a by b metres from its middle to its sides, inv its
 * pose's inverse, rests with its edge he where a hinge whole at gap
 * metres engages it on q's edge te; the pair into p. Unmoved, its edge
 * is the hinge's own, so whole is the engaged test's hold. */
static bool video_xr_rest_pair(const video_xr_quad_t *q,
      const video_xr_pose_t *inv, double a, double b, unsigned he,
      unsigned te, double gap, video_xr_snap_pair_t *p)
{
   video_xr_pose_t rel;
   video_xr_compose(inv, &q->pose, &rel);
   video_xr_snap_pair(&rel, q->width * 0.5, q->height * 0.5, a, b, he, te,
         gap, false, p);
   return video_xr_snap_engaged(p, (he < 2) ? a : b, gap);
}

/* Whether screen c, taken where it rests with nothing else pulling yet
 * (a grab's fade of none), stays unmoved held whole by the hinge of
 * its edge edge on target's edge on, the screens in skip held; so a
 * hinge shown at rest stays hinged when its screen is taken. */
static bool video_xr_rest_holds(const video_xr_quad_set_t *set,
      const video_xr_quad_t *c, const video_xr_pose_t *hand, unsigned skip,
      unsigned target, unsigned edge, unsigned on, float gap)
{
   video_xr_hinge_t k;
   video_xr_pose_t moved    = *hand;
   const video_xr_quat_t *q = &c->pose.orientation;
   const video_xr_quat_t *m = &moved.orientation;
   /* The turn by its dot over both lengths: video_xr_quat_angle reads a
    * float turn, unit only to rounding, hundredths of a degree off
    * itself. */
   return (video_xr_snap_hinge_at(set, c->slot, skip, 1u << target,
            c->width, c->height, gap, 0.0f, false, &moved, &k)
         & VIDEO_XR_SNAP_HINGE)
      && k.slot == target && k.edge == edge && k.on == on
      && fabs((double)moved.position.x - c->pose.position.x)
         <= VIDEO_XR_SNAP_TIE
      && fabs((double)moved.position.y - c->pose.position.y)
         <= VIDEO_XR_SNAP_TIE
      && fabs((double)moved.position.z - c->pose.position.z)
         <= VIDEO_XR_SNAP_TIE
      && fabs(video_xr_quat_dot(m, q))
         >= sqrt(video_xr_quat_dot(m, m) * video_xr_quat_dot(q, q))
         * cos(VIDEO_XR_SNAP_FOLD_TIE * VIDEO_XR_PI / 360.0);
}

/* hand turned on about the middle of c's edge he until the snap's turn
 * to target, parallel or square about its up as video_xr_snap_target,
 * shows c from it: c turned y degrees off short of twice
 * VIDEO_XR_SNAP_ANGLE shows from x, where x less the turn's pull
 * (2r - x) is y. */
static void video_xr_rest_unturn(const video_xr_quad_t *c,
      const video_xr_quad_t *target, unsigned he, video_xr_pose_t *hand)
{
   unsigned i;
   double half, x, k;
   video_xr_pose_t goal, at, by;
   video_xr_vec3_t mid, out, e, f;
   double r[3]               = { 0.0, 0.0, 0.0 };
   double y                  = 2.0 * VIDEO_XR_SNAP_ANGLE;
   double n                  = 0.0;
   double s                  = 1.0;
   const video_xr_quat_t *qc = &c->pose.orientation;
   video_xr_pose_identity(&goal);
   for (i = 0; i < 3; i++)
   {
      /* The turn off each goal as an axis and angle: acos would lose a
       * turn of hundredths of a degree to float rounding. */
      double v[3], m, a, d;
      const video_xr_quat_t *g = &goal.orientation;
      video_xr_snap_goal(target, i, &goal.orientation);
      v[0] = (double)g->w * qc->x - (double)g->x * qc->w
         - (double)g->y * qc->z + (double)g->z * qc->y;
      v[1] = (double)g->w * qc->y + (double)g->x * qc->z
         - (double)g->y * qc->w - (double)g->z * qc->x;
      v[2] = (double)g->w * qc->z - (double)g->x * qc->y
         + (double)g->y * qc->x - (double)g->z * qc->w;
      d    = video_xr_quat_dot(g, qc);
      m    = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
      a    = 2.0 * atan2(m, fabs(d)) * 180.0 / VIDEO_XR_PI;
      if (a < y)
      {
         at   = goal;
         y    = a;
         n    = m;
         s    = (d < 0.0) ? -1.0 : 1.0;
         r[0] = v[0];
         r[1] = v[1];
         r[2] = v[2];
      }
   }
   video_xr_snap_edge(he, c->width * 0.5, c->height * 0.5, &mid, &out,
         &half);
   /* A turn to the goal that moves c less than video_xr_rest_exact sees
    * is rounding, which a flat rest on a turned target carries. */
   if (     n <= 0.0
         || (y <= VIDEO_XR_SNAP_FOLD_TIE
            && y * VIDEO_XR_PI / 180.0
               * sqrt((double)mid.x * mid.x + (double)mid.y * mid.y)
               <= VIDEO_XR_SNAP_TIE))
      return;
   x = (y + 2.0 * VIDEO_XR_SNAP_ANGLE) * 0.5;
   k = s * sin(x * VIDEO_XR_PI / 360.0) / n;
   video_xr_pose_identity(&by);
   by.orientation.x = (float)(r[0] * k);
   by.orientation.y = (float)(r[1] * k);
   by.orientation.z = (float)(r[2] * k);
   by.orientation.w = (float)cos(x * VIDEO_XR_PI / 360.0);
   video_xr_compose(&at, &by, &at);
   hand->orientation = at.orientation;
   video_xr_rotate(qc, &mid, &e);
   video_xr_rotate(&hand->orientation, &mid, &f);
   hand->position.x += e.x - f.x;
   hand->position.y += e.y - f.y;
   hand->position.z += e.z - f.z;
}

/* Where a hand taking screen c, resting in the hinge of its edge he on
 * target's edge te, holds it (video_xr_rest_take). */
static void video_xr_rest_hand(const video_xr_quad_t *c,
      const video_xr_quad_t *target, unsigned he, unsigned te, double gap,
      video_xr_pose_t *hand)
{
   video_xr_pose_t inv, rel;
   video_xr_snap_pair_t p;
   video_xr_vec3_t m, w;
   double s = video_xr_snap_stop_from(gap) + VIDEO_XR_SNAP_STOP_ARM;
   *hand    = c->pose;
   video_xr_rest_unturn(c, target, he, hand);
   video_xr_pose_invert(&c->pose, &inv);
   video_xr_compose(&inv, &target->pose, &rel);
   video_xr_snap_pair(&rel, target->width * 0.5, target->height * 0.5,
         c->width * 0.5, c->height * 0.5, he, te, gap, false, &p);
   if (fabs(p.across) > VIDEO_XR_SNAP_TIE)
      return;
   m.x = (float)(-s * p.away[0]);
   m.y = (float)(-s * p.away[1]);
   m.z = (float)(-s * p.away[2]);
   video_xr_rotate(&c->pose.orientation, &m, &w);
   hand->position.x += w.x;
   hand->position.y += w.y;
   hand->position.z += w.z;
}

/* Whether pair p, gap metres the set gap, lies where a let-go leaves
 * it: not apart within VIDEO_XR_HINGE_SETTLE of the gap but off it. */
static bool video_xr_rest_settled(const video_xr_snap_pair_t *p,
      double gap)
{
   double d = fabs(p->across - gap);
   return p->across <= VIDEO_XR_SNAP_TIE || d <= VIDEO_XR_SNAP_TIE
      || d > VIDEO_XR_HINGE_SETTLE;
}

/* Whether screen c, carried, rests exactly in the hinge of its edge he
 * on target's edge te: engaged as the poses are, where a let-go leaves
 * it, and held there unmoved from where a hand taking it holds it. The
 * one test for both a hinge shown at rest and one taken up. */
static bool video_xr_rest_exact(const video_xr_quad_set_t *set,
      const video_xr_quad_t *c, const video_xr_quad_t *target,
      unsigned skip, unsigned he, unsigned te, float gap)
{
   video_xr_pose_t inv, hand;
   video_xr_snap_pair_t p;
   video_xr_pose_invert(&c->pose, &inv);
   if (     !video_xr_rest_pair(target, &inv, c->width * 0.5,
               c->height * 0.5, he, te, gap, &p)
         || !video_xr_rest_settled(&p, gap))
      return false;
   video_xr_rest_hand(c, target, he, te, gap, &hand);
   return video_xr_rest_holds(set, c, &hand, skip, target->slot, he, te,
         gap);
}

/* The first edge of screen c resting exactly hinged on an edge of
 * target, c carried: into r; false with none. */
static bool video_xr_rest_on(const video_xr_quad_set_t *set,
      const video_xr_quad_t *c, const video_xr_quad_t *target,
      unsigned skip, float gap, video_xr_rest_t *r)
{
   unsigned he, te;
   for (he = 0; he < 4; he++)
      for (te = 0; te < 4; te++)
         if (video_xr_rest_exact(set, c, target, skip, he, te, gap))
         {
            r->slot   = (unsigned char)c->slot;
            r->edge   = (unsigned char)he;
            r->target = (unsigned char)target->slot;
            r->on     = (unsigned char)te;
            return true;
         }
   return false;
}

void video_xr_rest_take(const video_xr_quad_set_t *set, unsigned slot,
      float width, float height, float gap, const video_xr_hinge_t *h,
      video_xr_pose_t *pose)
{
   unsigned i;
   video_xr_quad_t c;
   if (!set)
      return;
   memset(&c, 0, sizeof(c));
   c.pose   = *pose;
   c.width  = width;
   c.height = height;
   c.slot   = slot;
   for (i = 0; i < set->num_quads; i++)
      if (     set->quads[i].slot == h->slot
            && set->quads[i].kind != VIDEO_XR_QUAD_MENU)
      {
         video_xr_rest_hand(&c, &set->quads[i], h->edge, h->on, gap, pose);
         return;
      }
}

unsigned video_xr_rest_hinges(const video_xr_quad_set_t *set,
      unsigned skip, float gap, video_xr_rest_t *rest, unsigned max)
{
   unsigned i, j;
   const video_xr_quad_t *s[RETRO_VIDEO_VIEWS_MAX];
   unsigned n  = 0;
   unsigned ns = set ? video_xr_rest_screens(set, skip, s) : 0;
   for (i = 0; i < ns; i++)
      for (j = i + 1; j < ns && n < max; j++)
         if (     video_xr_rest_on(set, s[i], s[j], skip, gap, &rest[n])
               || video_xr_rest_on(set, s[j], s[i], skip, gap, &rest[n]))
            n++;
   return n;
}

bool video_xr_rest_hinge(const video_xr_quad_set_t *set, unsigned slot,
      unsigned skip, float width, float height, float gap,
      const video_xr_pose_t *pose, video_xr_hinge_t *hinge)
{
   unsigned i, n;
   video_xr_quad_t c;
   video_xr_rest_t r;
   video_xr_pose_t moved;
   const video_xr_quad_t *s[RETRO_VIDEO_VIEWS_MAX];
   unsigned on = 0;
   if (!set || slot >= VIDEO_XR_MENU_SLOT)
      return false;
   memset(&c, 0, sizeof(c));
   c.pose   = *pose;
   c.width  = width;
   c.height = height;
   c.slot   = slot;
   n        = video_xr_rest_screens(set, skip | (1u << slot), s);
   for (i = 0; i < n; i++)
      if (video_xr_rest_on(set, &c, s[i], skip, gap, &r))
         on |= 1u << s[i]->slot;
   if (!on)
      return false;
   /* Of those, the one the snap ranks best; it may rank another edge of
    * that screen, which must rest exactly too. */
   moved = *pose;
   if (!(video_xr_snap_hinge(set, slot, ~on, width, height, gap, 1.0f,
            &moved, hinge) & VIDEO_XR_SNAP_HINGE))
      return false;
   for (i = 0; i < n; i++)
      if (s[i]->slot == hinge->slot)
         break;
   return i < n && video_xr_rest_exact(set, &c, s[i], skip, hinge->edge,
         hinge->on, gap);
}

/* Whether a screen turned q has its edge along its axis run (0 x, 1 y)
 * within VIDEO_XR_SNAP_ANGLE degrees of h's line, where a grab's hinge
 * holds it whole. */
static bool video_xr_hinge_along(const video_xr_hinge_t *h,
      const video_xr_quat_t *q, unsigned run)
{
   video_xr_vec3_t mid, e;
   mid.x = run ? 0.0f : 1.0f;
   mid.y = run ? 1.0f : 0.0f;
   mid.z = 0.0f;
   video_xr_rotate(q, &mid, &e);
   return fabs((double)e.x * h->dir.x + (double)e.y * h->dir.y
         + (double)e.z * h->dir.z)
      >= cos(VIDEO_XR_SNAP_ANGLE * VIDEO_XR_PI / 180.0);
}

bool video_xr_hinge_settle(const video_xr_quad_set_t *set, unsigned slot,
      unsigned skip, float width, float height, float gap,
      const video_xr_hinge_t *h, const video_xr_pose_t *hand,
      video_xr_pose_t *pose)
{
   unsigned i;
   video_xr_quad_t c;
   video_xr_hinge_t k;
   video_xr_snap_pair_t p;
   video_xr_pose_t inv;
   video_xr_vec3_t m, w;
   const video_xr_quad_t *target = NULL;
   double across                 = 0.0;
   double along                  = 0.0;
   double nearest                = 2.0 * VIDEO_XR_SNAP_DIST;
   if (!set || h->slot >= VIDEO_XR_MENU_SLOT || ((skip >> h->slot) & 1u))
      return false;
   for (i = 0; i < set->num_quads && !target; i++)
      if (     set->quads[i].slot == h->slot
            && set->quads[i].kind != VIDEO_XR_QUAD_MENU)
         target = &set->quads[i];
   if (!target)
      return false;
   memset(&c, 0, sizeof(c));
   c.pose   = *hand;
   c.width  = width;
   c.height = height;
   c.slot   = slot;
   if (     !(video_xr_snap_hinge_at(set, slot, skip, 1u << h->slot, width,
               height, gap, 0.0f, false, &c.pose, &k)
            & VIDEO_XR_SNAP_HINGE)
         || k.slot != h->slot || k.edge != h->edge || k.on != h->on)
      return false;
   video_xr_pose_invert(&c.pose, &inv);
   video_xr_rest_pair(target, &inv, width * 0.5, height * 0.5, h->edge,
         h->on, gap, &p);
   /* The snap turns it parallel from farther than the hinge engages. */
   if (!video_xr_hinge_along(&k, &hand->orientation, p.run))
      return false;
   /* Pushed or touching, the stop has put it where it stays. */
   if (     p.across > VIDEO_XR_SNAP_TIE
         && fabs(p.across - gap) <= VIDEO_XR_HINGE_SETTLE)
      across = gap - p.across;
   for (i = 0; i < 3; i++)
      if (fabs(p.level[i]) < nearest)
      {
         nearest = fabs(p.level[i]);
         along   = p.level[i];
      }
   m.x = (float)(across * p.away[0] + (p.run ? 0.0 : along));
   m.y = (float)(across * p.away[1] + (p.run ? along : 0.0));
   m.z = (float)(across * p.away[2]);
   video_xr_rotate(&c.pose.orientation, &m, &w);
   c.pose.position.x += w.x;
   c.pose.position.y += w.y;
   c.pose.position.z += w.z;
   *pose = c.pose;
   return video_xr_rest_exact(set, &c, target, skip, h->edge, h->on, gap);
}

bool video_xr_grab_settle(const video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_quad_set_t *targets,
      unsigned skip, const video_xr_hinge_t *h, video_xr_pose_t *pose)
{
   video_xr_pose_t hand;
   if (!targets)
      return false;
   video_xr_grab_pose(g, aim, NULL, 0, NULL, &hand, NULL, NULL);
   return video_xr_hinge_settle(targets, g->slot, skip, g->width,
         g->height, g->gap * 0.01f, h, &hand, pose);
}

/* Past this pitch of the ray, in degrees, its heading blends evenly
 * into the one the hand's up gives, which straight up or down is the
 * only one: so it never jumps there, but a twist there turns it. */
#define VIDEO_XR_HEADING_BLEND 80.0

/* The heading of aim's ray, as a turn about the vertical. */
static void video_xr_heading(const video_xr_quat_t *aim,
      video_xr_pose_t *out)
{
   double x, z, t, half;
   video_xr_vec3_t v, ray, up;
   v.x = 0.0f;
   v.y = 0.0f;
   v.z = -1.0f;
   video_xr_rotate(aim, &v, &ray);
   v.y = 1.0f;
   v.z = 0.0f;
   video_xr_rotate(aim, &v, &up);
   x = ray.x;
   z = ray.z;
   t = fabs(ray.y);
   t = (asin(t < 1.0 ? t : 1.0) * 180.0 / VIDEO_XR_PI
         - VIDEO_XR_HEADING_BLEND) / (90.0 - VIDEO_XR_HEADING_BLEND);
   if (t > 0.0)
   {
      /* Tipped up, the hand's up points back from the heading, tipped
       * down along it; this far tipped, it is never upright. */
      double ux = -ray.y * up.x;
      double uz = -ray.y * up.z;
      double n  = sqrt(x * x + z * z);
      double m  = sqrt(ux * ux + uz * uz);
      if (t > 1.0)
         t = 1.0;
      x = (n > 0.0 ? (1.0 - t) * x / n : 0.0) + t * ux / m;
      z = (n > 0.0 ? (1.0 - t) * z / n : 0.0) + t * uz / m;
   }
   half = atan2(-x, -z) / 2.0;
   video_xr_pose_identity(out);
   out->orientation.y = (float)sin(half);
   out->orientation.w = (float)cos(half);
}

void video_xr_grab_hold(video_xr_grab_t *g, const video_xr_pose_t *aim,
      const video_xr_quat_t *q)
{
   video_xr_pose_t head, turn;
   video_xr_heading(&aim->orientation, &head);
   video_xr_pose_invert(&head, &head);
   video_xr_pose_identity(&turn);
   turn.orientation = *q;
   video_xr_compose(&head, &turn, &turn);
   g->turn = turn.orientation;
}

/* Where g puts its item for an aim pose, with no snap or clamp. */
static void video_xr_grab_aimed(const video_xr_grab_t *g,
      const video_xr_pose_t *aim, video_xr_pose_t *pose)
{
   video_xr_compose(aim, &g->rel, pose);
   if (!g->tilt)
   {
      video_xr_pose_t head, turn;
      video_xr_heading(&aim->orientation, &head);
      video_xr_pose_identity(&turn);
      turn.orientation = g->turn;
      video_xr_compose(&head, &turn, &turn);
      pose->orientation = turn.orientation;
   }
}

/* video_xr_grab_pose, and the hinge the snap holds unless NULL. */
static bool video_xr_grab_snapped(const video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_quad_set_t *targets,
      unsigned skip, const video_xr_pose_t *start, video_xr_pose_t *pose,
      video_screen_xr_t *out, unsigned *snapped, video_xr_hinge_t *hinge)
{
   video_screen_xr_t s, c;
   bool clamped;
   unsigned snaps = 0;
   video_xr_grab_aimed(g, aim, pose);
   if (targets)
   {
      video_xr_hinge_t kept;
      unsigned whole = video_xr_grab_kept(g, aim, targets, start, &kept)
         ? 1u << kept.slot : 0u;
      snaps = video_xr_snap_hinge_at(targets, g->slot, skip, whole,
            g->width, g->height, g->gap * 0.01f,
            video_xr_snap_fade(start, pose), g->through != 0, pose, hinge);
   }
   if (snapped)
      *snapped = snaps;
   video_views_xr_reset(&s, 1);
   video_xr_item_settings(&g->base, pose, &s);
   c = s;
   video_views_xr_normalize(&c);
   clamped = c.x != s.x || c.y != s.y || c.z != s.z;
   if (clamped)
   {
      pose->position.x = g->base.x + c.x;
      pose->position.y = g->base.y + c.y;
      pose->position.z = g->base.z + c.z;
   }
   if (out)
   {
      out->x     = c.x;
      out->y     = c.y;
      out->z     = c.z;
      out->yaw   = c.yaw;
      out->pitch = c.pitch;
      out->roll  = c.roll;
   }
   return clamped;
}

bool video_xr_grab_pose(const video_xr_grab_t *g, const video_xr_pose_t *aim,
      const video_xr_quad_set_t *targets, unsigned skip,
      const video_xr_pose_t *start, video_xr_pose_t *pose,
      video_screen_xr_t *out, unsigned *snapped)
{
   return video_xr_grab_snapped(g, aim, targets, skip, start, pose, out,
         snapped, NULL);
}

/* A stick axis past the deadzone, as a fraction of the travel left. */
static double video_xr_deflection(float v)
{
   double a = fabs(v);
   if (a <= VIDEO_XR_EDIT_DEADZONE)
      return 0.0;
   if (a > 1.0)
      a = 1.0;
   a = (a - VIDEO_XR_EDIT_DEADZONE) / (1.0 - VIDEO_XR_EDIT_DEADZONE);
   return (v < 0.0f) ? -a : a;
}

void video_xr_grab_stick(video_xr_grab_t *g, float x, float y, double dt,
      float *scale)
{
   double push = video_xr_deflection(y) * VIDEO_XR_EDIT_PUSH_RATE * dt;
   double grow = video_xr_deflection(x) * VIDEO_XR_EDIT_SCALE_RATE * dt;
   if (push != 0.0)
   {
      /* The aim's ray is its -Z. */
      double z = g->rel.position.z - push;
      if (push < 0.0 && z > -VIDEO_XR_EDIT_REACH)
         z = (g->rel.position.z > -VIDEO_XR_EDIT_REACH)
            ? g->rel.position.z : -VIDEO_XR_EDIT_REACH;
      g->rel.position.z = (float)z;
   }
   if (grow != 0.0)
   {
      video_screen_xr_t s;
      video_views_xr_reset(&s, 1);
      s.scale = (float)(*scale * pow(2.0, grow));
      video_views_xr_normalize(&s);
      *scale = s.scale;
   }
}

/* g's item turned about the vertical through its middle and tipped
 * about its own X, half and tip being half the angles, as a quaternion
 * takes them. */
static void video_xr_grab_rotate(video_xr_grab_t *g,
      const video_xr_pose_t *aim, double half, double tip)
{
   double n, s;
   video_xr_pose_t turn, item;
   video_xr_vec3_t axis;
   video_xr_quat_t *q;
   if (half == 0.0 && tip == 0.0)
      return;
   axis.x = 0.0f;
   axis.y = 1.0f;
   axis.z = 0.0f;
   video_xr_pose_identity(&item);
   if (g->tilt)
   {
      video_xr_pose_t inv;
      /* About the anchor's up in the aim's frame, before g->rel. */
      video_xr_pose_invert(aim, &inv);
      video_xr_rotate(&inv.orientation, &axis, &axis);
      item.orientation = g->rel.orientation;
      q                = &g->rel.orientation;
   }
   else
   {
      item.orientation = g->turn;
      q                = &g->turn;
   }
   s = sin(half);
   video_xr_pose_identity(&turn);
   turn.orientation.x = (float)(axis.x * s);
   turn.orientation.y = (float)(axis.y * s);
   turn.orientation.z = (float)(axis.z * s);
   turn.orientation.w = (float)cos(half);
   video_xr_compose(&turn, &item, &item);
   /* Composed after it: about its own X, carried or tilting. */
   video_xr_pose_identity(&turn);
   turn.orientation.x = (float)sin(tip);
   turn.orientation.w = (float)cos(tip);
   video_xr_compose(&item, &turn, &item);
   n = sqrt((double)item.orientation.x * item.orientation.x
         + (double)item.orientation.y * item.orientation.y
         + (double)item.orientation.z * item.orientation.z
         + (double)item.orientation.w * item.orientation.w);
   if (n > 0.0)
   {
      q->x = (float)(item.orientation.x / n);
      q->y = (float)(item.orientation.y / n);
      q->z = (float)(item.orientation.z / n);
      q->w = (float)(item.orientation.w / n);
   }
}

/* video_xr_grab_rotate, but lid's axis (lid unless NULL) about the
 * middle of g's item's edge lid->edge. */
static void video_xr_grab_rotate_lid(video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_hinge_t *lid,
      double half, double tip)
{
   double l;
   video_xr_pose_t pose;
   video_xr_vec3_t mid, out, e, f;
   if (!lid)
   {
      video_xr_grab_rotate(g, aim, half, tip);
      return;
   }
   if (lid->axis == VIDEO_XR_STEP_TURN)
   {
      video_xr_grab_rotate(g, aim, 0.0, tip);
      tip = 0.0;
   }
   else
   {
      video_xr_grab_rotate(g, aim, half, 0.0);
      half = 0.0;
   }
   if (half == 0.0 && tip == 0.0)
      return;
   video_xr_snap_edge(lid->edge, g->width * 0.5, g->height * 0.5, &mid,
         &out, &l);
   video_xr_grab_pose(g, aim, NULL, 0, NULL, &pose, NULL, NULL);
   video_xr_rotate(&pose.orientation, &mid, &e);
   video_xr_grab_rotate(g, aim, half, tip);
   video_xr_grab_pose(g, aim, NULL, 0, NULL, &pose, NULL, NULL);
   video_xr_rotate(&pose.orientation, &mid, &f);
   pose.position.x += e.x - f.x;
   pose.position.y += e.y - f.y;
   pose.position.z += e.z - f.z;
   video_xr_grab_put(g, aim, &pose);
}

unsigned video_xr_grab_turn_lid(video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_hinge_t *lid, float x,
      float y, double dt)
{
   double half, tip;
   double lock = tan(VIDEO_XR_EDIT_TURN_LOCK * VIDEO_XR_PI / 180.0);
   /* A push a little off one axis is meant along it: a turn takes no
    * tilt, a tilt no turn. */
   if (fabs(y) < fabs(x) * lock)
      y = 0.0f;
   else if (fabs(x) < fabs(y) * lock)
      x = 0.0f;
   half = video_xr_deflection(x) * VIDEO_XR_EDIT_TURN_RATE
      * dt * VIDEO_XR_PI / 360.0;
   /* Up tips its top back: about its own -X. */
   tip  = -video_xr_deflection(y) * VIDEO_XR_EDIT_TURN_RATE
      * dt * VIDEO_XR_PI / 360.0;
   video_xr_grab_rotate_lid(g, aim, lid, half, tip);
   return (half != 0.0 ? VIDEO_XR_STEP_TURN : 0u)
      | (tip != 0.0 ? VIDEO_XR_STEP_TILT : 0u);
}

unsigned video_xr_grab_turn(video_xr_grab_t *g, const video_xr_pose_t *aim,
      float x, float y, double dt)
{
   return video_xr_grab_turn_lid(g, aim, NULL, x, y, dt);
}

/* Degrees within half a turn either way. */
static double video_xr_wrap(double deg)
{
   deg = fmod(deg, 360.0);
   if (deg > 180.0)
      deg -= 360.0;
   else if (deg <= -180.0)
      deg += 360.0;
   return deg;
}

/* An item's turn from facing head across the floor, in degrees; false
 * with head right above or below its middle. */
static bool video_xr_facing(const video_xr_pose_t *item,
      const video_xr_vec3_t *head, double *deg)
{
   video_xr_pose_t back, h;
   double dx = (double)head->x - item->position.x;
   double dz = (double)head->z - item->position.z;
   if (dx * dx + dz * dz < 1e-8)
      return false;
   /* It faces its +Z, the heading reads a ray's -Z: so the heading of
    * it turned half round, which lying flat blends as a ray's does. */
   video_xr_pose_identity(&back);
   back.orientation.y = 1.0f;
   back.orientation.w = 0.0f;
   video_xr_compose(item, &back, &back);
   video_xr_heading(&back.orientation, &h);
   *deg = video_xr_wrap((2.0 * atan2(h.orientation.y, h.orientation.w)
            - atan2(-dx, -dz)) * 180.0 / VIDEO_XR_PI);
   return true;
}

/* An item's tilt from upright about its own X, in degrees; false on its
 * side, with no upright about that line. */
static bool video_xr_upright(const video_xr_quat_t *q, double *deg)
{
   video_xr_vec3_t v, front, up;
   v.x = 0.0f;
   v.y = 0.0f;
   v.z = 1.0f;
   video_xr_rotate(q, &v, &front);
   v.y = 1.0f;
   v.z = 0.0f;
   video_xr_rotate(q, &v, &up);
   if ((double)front.y * front.y + (double)up.y * up.y < 1e-8)
      return false;
   *deg = atan2(-(double)front.y, (double)up.y) * 180.0 / VIDEO_XR_PI;
   return true;
}

/* The step, every size degrees (0: none), within
 * VIDEO_XR_EDIT_TURN_HOLD degrees of deg, if one is. */
static bool video_xr_step_near(double deg, double size, double *step)
{
   double s;
   if (size <= 0.0)
      return false;
   s = floor(deg / size + 0.5) * size;
   if (fabs(deg - s) > VIDEO_XR_EDIT_TURN_HOLD)
      return false;
   *step = s;
   return true;
}

unsigned video_xr_grab_step(video_xr_grab_t *g, const video_xr_pose_t *aim,
      const video_xr_vec3_t *head, float size, unsigned axes, float *rest,
      const video_xr_quad_set_t *targets, unsigned skip,
      const video_xr_pose_t *start, const video_xr_hinge_t *lid)
{
   double deg, step;
   video_xr_pose_t pose;
   unsigned held = 0;
   /* Where the stick alone has taken it. */
   video_xr_grab_rotate_lid(g, aim, lid,
         (axes & VIDEO_XR_STEP_TURN) ? rest[0] * VIDEO_XR_PI / 360.0 : 0.0,
         (axes & VIDEO_XR_STEP_TILT) ? rest[1] * VIDEO_XR_PI / 360.0 : 0.0);
   if (axes & VIDEO_XR_STEP_TURN)
      rest[0] = 0.0f;
   if (axes & VIDEO_XR_STEP_TILT)
      rest[1] = 0.0f;
   if (!head)
      axes &= ~VIDEO_XR_STEP_TURN;
   if (!axes)
      return 0;
   if (targets)
   {
      unsigned snapped = 0;
      video_xr_grab_pose(g, aim, targets, skip, start, &pose, NULL,
            &snapped);
      if (snapped & VIDEO_XR_SNAP_ALL)
         return 0;
   }
   /* The tilt first: a turn about the vertical keeps it. */
   video_xr_grab_pose(g, aim, NULL, 0, NULL, &pose, NULL, NULL);
   if (     (axes & VIDEO_XR_STEP_TILT)
         && video_xr_upright(&pose.orientation, &deg)
         && video_xr_step_near(deg, size, &step))
   {
      video_xr_grab_rotate_lid(g, aim, lid, 0.0,
            (step - deg) * VIDEO_XR_PI / 360.0);
      rest[1] = (float)(deg - step);
      held   |= VIDEO_XR_STEP_TILT;
      video_xr_grab_pose(g, aim, NULL, 0, NULL, &pose, NULL, NULL);
   }
   if (     (axes & VIDEO_XR_STEP_TURN)
         && video_xr_facing(&pose, head, &deg)
         && video_xr_step_near(deg, size, &step))
   {
      video_xr_grab_rotate_lid(g, aim, lid,
            (step - deg) * VIDEO_XR_PI / 360.0, 0.0);
      rest[0] = (float)(deg - step);
      held   |= VIDEO_XR_STEP_TURN;
   }
   return held;
}

unsigned video_xr_grab_off_step(const video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_vec3_t *head, float size,
      const float *rest)
{
   double deg, step;
   video_xr_pose_t pose;
   unsigned off = 0;
   video_xr_grab_pose(g, aim, NULL, 0, NULL, &pose, NULL, NULL);
   /* rest adds to the angles exactly: it turns about the vertical and
    * tips about the item's own X. */
   if (     video_xr_upright(&pose.orientation, &deg)
         && !video_xr_step_near(deg + rest[1], size, &step))
      off |= VIDEO_XR_STEP_TILT;
   if (     head
         && video_xr_facing(&pose, head, &deg)
         && !video_xr_step_near(deg + rest[0], size, &step))
      off |= VIDEO_XR_STEP_TURN;
   return off;
}

/* g's item where it is with no snap, in pose, and in m the middle of
 * its edge h->edge from h's line, straight across it. Returns the
 * edge's way, as video_xr_snap_edge. */
static unsigned video_xr_hinge_across(const video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_hinge_t *h,
      video_xr_pose_t *pose, double *m)
{
   double half;
   double d[3];
   video_xr_vec3_t mid, out, v;
   unsigned run = video_xr_snap_edge(h->edge, g->width * 0.5,
         g->height * 0.5, &mid, &out, &half);
   video_xr_grab_pose(g, aim, NULL, 0, NULL, pose, NULL, NULL);
   video_xr_rotate(&pose->orientation, &mid, &v);
   m[0] = (double)pose->position.x + v.x - h->point.x;
   m[1] = (double)pose->position.y + v.y - h->point.y;
   m[2] = (double)pose->position.z + v.z - h->point.z;
   d[0] = h->dir.x;
   d[1] = h->dir.y;
   d[2] = h->dir.z;
   video_xr_snap_across(m, d);
   return run;
}

/* h's plane's facing: square to its line and to flat. */
static void video_xr_hinge_normal(const video_xr_hinge_t *h, double *n)
{
   n[0] = (double)h->dir.y * h->flat.z - (double)h->dir.z * h->flat.y;
   n[1] = (double)h->dir.z * h->flat.x - (double)h->dir.x * h->flat.z;
   n[2] = (double)h->dir.x * h->flat.y - (double)h->dir.y * h->flat.x;
}

/* Whether g's item, where it is with no snap, has its edge h->edge
 * within VIDEO_XR_SNAP_DIST metres of h's plane, VIDEO_XR_SNAP_ANGLE
 * degrees of its line's way and at a gap the hinge holds whole. */
static bool video_xr_hinge_whole(const video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_hinge_t *h)
{
   double m[3], n[3], f[3];
   video_xr_pose_t pose;
   unsigned run = video_xr_hinge_across(g, aim, h, &pose, m);
   video_xr_hinge_normal(h, n);
   f[0]  = h->flat.x;
   f[1]  = h->flat.y;
   f[2]  = h->flat.z;
   return fabs(video_xr_snap_dot(m, n)) <= VIDEO_XR_SNAP_DIST
      && video_xr_snap_gap_share(h->gap + video_xr_snap_dot(m, f),
            g->gap * 0.01f) >= 1.0
      && video_xr_hinge_along(h, &pose.orientation, run);
}

/* g's item where it is with no snap, its edge h->edge paired with h's
 * target's in targets. False if targets has no such target. */
static bool video_xr_grab_hinge_pair(const video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_quad_set_t *targets,
      const video_xr_hinge_t *h, video_xr_snap_pair_t *p)
{
   unsigned i;
   video_xr_pose_t pose, inv, rel;
   if (!targets)
      return false;
   for (i = 0; i < targets->num_quads; i++)
   {
      const video_xr_quad_t *q = &targets->quads[i];
      if (q->slot != h->slot || q->kind == VIDEO_XR_QUAD_MENU)
         continue;
      video_xr_grab_pose(g, aim, NULL, 0, NULL, &pose, NULL, NULL);
      video_xr_pose_invert(&pose, &inv);
      video_xr_compose(&inv, &q->pose, &rel);
      video_xr_snap_pair(&rel, q->width * 0.5, q->height * 0.5,
            g->width * 0.5, g->height * 0.5, h->edge, h->on,
            g->gap * 0.01f, false, p);
      return true;
   }
   return false;
}

bool video_xr_grab_near_hinge(const video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_quad_set_t *targets,
      const video_xr_hinge_t *h, float reach)
{
   video_xr_snap_pair_t p;
   double gap = g->gap * 0.01f;
   if (!video_xr_grab_hinge_pair(g, aim, targets, h, &p))
      return false;
   return fabs(p.plane) <= reach
      && ((p.across < 0.0)
         ? video_xr_snap_stop_held(-p.across, gap, g->through != 0)
         : video_xr_snap_gap_share(p.across, gap) > 0.0)
      && p.overlap > 0.0;
}

bool video_xr_grab_through(const video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_quad_set_t *targets,
      const video_xr_hinge_t *h)
{
   video_xr_snap_pair_t p;
   return video_xr_grab_hinge_pair(g, aim, targets, h, &p)
      && p.across < 0.0
      && !video_xr_snap_stop_held(-p.across, g->gap * 0.01f,
            g->through != 0);
}

bool video_xr_grab_kept(const video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_quad_set_t *targets,
      const video_xr_pose_t *start, video_xr_hinge_t *kept)
{
   video_xr_pose_t pose;
   if (!g->rest_slot || !targets)
      return false;
   memset(kept, 0, sizeof(*kept));
   kept->slot = g->rest_slot - 1u;
   kept->on   = g->rest_on;
   kept->edge = g->rest_edge;
   if (video_xr_grab_near_hinge(g, aim, targets, kept,
            VIDEO_XR_HINGE_REACH))
      return true;
   video_xr_grab_aimed(g, aim, &pose);
   return video_xr_snap_fade(start, &pose) < 1.0f;
}

bool video_xr_grab_marked(const video_xr_grab_t *g,
      const video_xr_pose_t *pose, const video_xr_quad_set_t *set,
      unsigned holding, video_xr_hinge_t *h)
{
   unsigned i;
   unsigned ti = set->num_quads;
   unsigned hi = set->num_quads;
   if (!g->mark_slot)
      return false;
   memset(h, 0, sizeof(*h));
   h->slot = g->mark_slot - 1u;
   h->on   = g->mark_on;
   h->edge = g->mark_edge;
   /* The other hand can take its target later in the poll that
    * publishes it. */
   if (holding & (1u << h->slot))
      return false;
   /* On the edges as they are; crossed, on the one drawn later, as the
    * quads draw in order. */
   for (i = 0; i < set->num_quads; i++)
   {
      const video_xr_quad_t *q = &set->quads[i];
      if (q->kind == VIDEO_XR_QUAD_MENU)
         continue;
      if (ti == set->num_quads && q->slot == h->slot)
         ti = i;
      if (hi == set->num_quads && q->slot == g->slot)
         hi = i;
   }
   /* Its target not in this frame's quads: no line, no rim cut. */
   if (ti == set->num_quads)
      return false;
   video_xr_hinge_place(h, pose, g->width, g->height, &set->quads[ti],
         hi > ti);
   return true;
}

unsigned video_xr_grab_hinge(const video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_quad_set_t *targets,
      unsigned skip, const video_xr_pose_t *start, video_xr_pose_t *pose,
      video_xr_hinge_t *hinge)
{
   unsigned snaps = 0;
   video_xr_hinge_t own;
   if (!hinge)
      hinge = &own;
   /* Not a hinge only other pulls put it on: a swing about a line its
    * own edge is far from would carry it round. */
   if (     video_xr_grab_snapped(g, aim, targets, skip, start, pose, NULL,
               &snaps, hinge)
         || (     (snaps & VIDEO_XR_SNAP_HINGE)
               && !video_xr_hinge_whole(g, aim, hinge)))
      snaps &= ~VIDEO_XR_SNAP_HINGE;
   return snaps;
}

void video_xr_grab_put(video_xr_grab_t *g, const video_xr_pose_t *aim,
      const video_xr_pose_t *pose)
{
   video_xr_pose_t inv, rel;
   video_xr_pose_invert(aim, &inv);
   video_xr_compose(&inv, pose, &rel);
   g->rel.position = rel.position;
   if (g->tilt)
      g->rel.orientation = rel.orientation;
   else
      video_xr_grab_hold(g, aim, &pose->orientation);
}

void video_xr_grab_swing(video_xr_grab_t *g, const video_xr_pose_t *aim,
      const video_xr_hinge_t *h, double deg)
{
   double s, half;
   video_xr_pose_t pose, by, at;
   video_xr_vec3_t mid, out, e;
   if (deg == 0.0)
      return;
   video_xr_grab_pose(g, aim, NULL, 0, NULL, &pose, NULL, NULL);
   video_xr_snap_edge(h->edge, g->width * 0.5, g->height * 0.5, &mid,
         &out, &half);
   video_xr_rotate(&pose.orientation, &mid, &e);
   e.x += pose.position.x;
   e.y += pose.position.y;
   e.z += pose.position.z;
   s = sin(deg * VIDEO_XR_PI / 360.0);
   video_xr_pose_identity(&by);
   by.orientation.x = (float)(h->dir.x * s);
   by.orientation.y = (float)(h->dir.y * s);
   by.orientation.z = (float)(h->dir.z * s);
   by.orientation.w = (float)cos(deg * VIDEO_XR_PI / 360.0);
   /* About the line through its edge's middle: there, turned, and
    * back, so the gap keeps. */
   video_xr_pose_identity(&at);
   at.position.x = -e.x;
   at.position.y = -e.y;
   at.position.z = -e.z;
   video_xr_compose(&at, &pose, &pose);
   video_xr_compose(&by, &pose, &pose);
   pose.position.x += e.x;
   pose.position.y += e.y;
   pose.position.z += e.z;
   video_xr_grab_put(g, aim, &pose);
}

void video_xr_grab_put_hinge(video_xr_grab_t *g, const video_xr_pose_t *aim,
      const video_xr_hinge_t *h, const video_xr_pose_t *pose, bool turned)
{
   if (turned)
   {
      double half;
      video_xr_pose_t under, at;
      video_xr_vec3_t mid, out, e, f;
      video_xr_snap_edge(h->edge, g->width * 0.5, g->height * 0.5, &mid,
            &out, &half);
      video_xr_grab_pose(g, aim, NULL, 0, NULL, &under, NULL, NULL);
      video_xr_rotate(&under.orientation, &mid, &e);
      video_xr_rotate(&pose->orientation, &mid, &f);
      at             = under;
      at.orientation = pose->orientation;
      at.position.x += e.x - f.x;
      at.position.y += e.y - f.y;
      at.position.z += e.z - f.z;
      video_xr_grab_put(g, aim, &at);
   }
   video_xr_grab_put_line(g, aim, h);
}

void video_xr_grab_put_line(video_xr_grab_t *g, const video_xr_pose_t *aim,
      const video_xr_hinge_t *h)
{
   double m[3], n[3], f[3], o, k;
   video_xr_pose_t pose;
   double set = g->gap * 0.01;
   video_xr_hinge_across(g, aim, h, &pose, m);
   video_xr_hinge_normal(h, n);
   f[0] = h->flat.x;
   f[1] = h->flat.y;
   f[2] = h->flat.z;
   o    = video_xr_snap_dot(m, n);
   /* Clicked, to the set gap it shows already: a drift while unhinged,
    * as past the hinge's limit and back, starts again from there. */
   k    = set - h->gap - video_xr_snap_dot(m, f);
   if (     fabs(h->gap - set) > VIDEO_XR_SNAP_TIE
         || fabs(k) > VIDEO_XR_SNAP_CLICK)
      k = 0.0;
   pose.position.x += (float)(k * f[0] - o * n[0]);
   pose.position.y += (float)(k * f[1] - o * n[1]);
   pose.position.z += (float)(k * f[2] - o * n[2]);
   video_xr_grab_put(g, aim, &pose);
}

unsigned video_xr_grab_turn_hinge(video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_hinge_t *h, float x,
      float y, double dt)
{
   double deg, s;
   unsigned acted;
   video_xr_vec3_t v, w;
   double lock = tan(VIDEO_XR_EDIT_TURN_LOCK * VIDEO_XR_PI / 180.0);
   if (!h)
      return video_xr_grab_turn(g, aim, x, y, dt);
   if (fabs(y) < fabs(x) * lock)
      y = 0.0f;
   else if (fabs(x) < fabs(y) * lock)
      x = 0.0f;
   /* The turn's own way, about the vertical, or the tilt's, about the
    * item's own X with up tipping its top back: along the line. */
   if (h->axis == VIDEO_XR_STEP_TILT)
   {
      video_xr_pose_t pose;
      video_xr_grab_pose(g, aim, NULL, 0, NULL, &pose, NULL, NULL);
      v.x = 1.0f;
      v.y = 0.0f;
      v.z = 0.0f;
      video_xr_rotate(&pose.orientation, &v, &w);
      deg = -video_xr_deflection(y);
      y   = 0.0f;
   }
   else
   {
      w.x = 0.0f;
      w.y = 1.0f;
      w.z = 0.0f;
      deg = video_xr_deflection(x);
      x   = 0.0f;
   }
   s      = (double)w.x * h->dir.x + (double)w.y * h->dir.y
      + (double)w.z * h->dir.z;
   deg   *= VIDEO_XR_EDIT_TURN_RATE * dt * ((s < 0.0) ? -1.0 : 1.0);
   acted  = video_xr_grab_turn(g, aim, x, y, dt);
   video_xr_grab_swing(g, aim, h, deg);
   return acted | ((deg != 0.0) ? h->axis : 0u);
}

/* Which of rest the hinge's axis keeps. */
static unsigned video_xr_hinge_rest(const video_xr_hinge_t *h)
{
   return (h->axis == VIDEO_XR_STEP_TURN) ? 0 : 1;
}

unsigned video_xr_grab_step_hinge(video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_hinge_t *h, float size,
      unsigned axes, float *rest, const video_xr_quad_set_t *targets,
      unsigned skip, const video_xr_pose_t *start)
{
   double deg, step;
   video_xr_pose_t pose;
   unsigned k     = video_xr_hinge_rest(h);
   unsigned other = h->axis ^ (VIDEO_XR_STEP_TURN | VIDEO_XR_STEP_TILT);
   /* Where the stick alone has taken it. */
   if (axes & other)
   {
      video_xr_grab_rotate(g, aim,
            (other & VIDEO_XR_STEP_TURN) ? rest[0] * VIDEO_XR_PI / 360.0
            : 0.0,
            (other & VIDEO_XR_STEP_TILT) ? rest[1] * VIDEO_XR_PI / 360.0
            : 0.0);
      rest[k ^ 1] = 0.0f;
   }
   if (!(axes & h->axis))
      return 0;
   video_xr_grab_swing(g, aim, h, rest[k]);
   rest[k] = 0.0f;
   if (     targets
         && (video_xr_grab_hinge(g, aim, targets, skip, start, &pose, NULL)
            & VIDEO_XR_SNAP_TURN))
      return 0;
   video_xr_grab_pose(g, aim, NULL, 0, NULL, &pose, NULL, NULL);
   deg = video_xr_hinge_angle(h, &pose.orientation);
   if (!video_xr_step_near(deg, size, &step))
      return 0;
   video_xr_grab_swing(g, aim, h, step - deg);
   rest[k] = (float)(deg - step);
   return h->axis;
}

unsigned video_xr_grab_off_hinge(const video_xr_grab_t *g,
      const video_xr_pose_t *aim, const video_xr_hinge_t *h, float size,
      const float *rest)
{
   double step;
   video_xr_pose_t pose;
   video_xr_grab_pose(g, aim, NULL, 0, NULL, &pose, NULL, NULL);
   return video_xr_step_near(video_xr_hinge_angle(h, &pose.orientation)
         + rest[video_xr_hinge_rest(h)], size, &step) ? 0u : h->axis;
}

/* A grey premultiplied texel of the laser's image. */
static void video_xr_cursor_texel(uint8_t *px, unsigned x, unsigned y,
      uint8_t level, uint8_t alpha)
{
   uint8_t *p = px + ((size_t)y * VIDEO_XR_CURSOR_W + x) * 4;
   p[0]       = level;
   p[1]       = level;
   p[2]       = level;
   p[3]       = alpha;
}

/* How many texels x, y is in from the nearest edge of a w by h part. */
static unsigned video_xr_rim_depth(unsigned x, unsigned y, unsigned w,
      unsigned h)
{
   unsigned d = x;
   if (y < d)
      d = y;
   if (w - 1 - x < d)
      d = w - 1 - x;
   if (h - 1 - y < d)
      d = h - 1 - y;
   return d;
}

/* An outline's square at x0, at alpha, without the rims of the edges in
 * cut (bit e for edge e as video_xr_outline_crop takes them, rows top
 * down) VIDEO_XR_OUTLINE_CROP texels deep. */
static void video_xr_outline_pixels(uint8_t *px, unsigned x0,
      uint8_t alpha, unsigned cut)
{
   unsigned x, y;
   const unsigned end = VIDEO_XR_OUTLINE_DIM - VIDEO_XR_OUTLINE_CROP;
   for (y = 0; y < VIDEO_XR_OUTLINE_DIM; y++)
      for (x = 0; x < VIDEO_XR_OUTLINE_DIM; x++)
      {
         unsigned band = video_xr_rim_depth(x, y, VIDEO_XR_OUTLINE_DIM,
               VIDEO_XR_OUTLINE_DIM) / VIDEO_XR_OUTLINE_BAND;
         if (     band < 3
               && !((cut & 1u) && y < VIDEO_XR_OUTLINE_CROP)
               && !((cut & 2u) && y >= end)
               && !((cut & 4u) && x >= end)
               && !((cut & 8u) && x < VIDEO_XR_OUTLINE_CROP))
            video_xr_cursor_texel(px, x0 + x, y, (band == 1) ? alpha : 0,
                  alpha);
      }
}

void video_xr_cursor_pixels(uint8_t *px)
{
   unsigned x, y, k;
   const float c = (VIDEO_XR_CURSOR_DIM - 1) * 0.5f;
   const float r = VIDEO_XR_CURSOR_DIM * 0.5f;
   memset(px, 0, (size_t)VIDEO_XR_CURSOR_W * VIDEO_XR_CURSOR_H * 4);
   for (y = 0; y < VIDEO_XR_CURSOR_DIM; y++)
   {
      for (x = 0; x < VIDEO_XR_CURSOR_DIM; x++)
      {
         float dx = (float)x - c;
         float dy = (float)y - c;
         float d  = (float)sqrt(dx * dx + dy * dy) / r;
         float a  = (d < 1.0f - 1.0f / r) ? 1.0f
            : ((d < 1.0f) ? (1.0f - d) * r : 0.0f);
         float l  = (d < 0.6f) ? 1.0f : 0.15f;
         video_xr_cursor_texel(px, x, y + VIDEO_XR_CURSOR_Y,
               (uint8_t)(l * a * 255.0f + 0.5f),
               (uint8_t)(a * 255.0f + 0.5f));
      }
   }
   for (k = 0; k < 2; k++)
   {
      uint8_t alpha = k ? 255 : VIDEO_XR_OUTLINE_FAINT;
      video_xr_outline_pixels(px, VIDEO_XR_OUTLINE_X(k), alpha, 0);
      for (y = 0; y < VIDEO_XR_HANDLE_ROWS; y++)
         for (x = 0; x < VIDEO_XR_HANDLE_COLS; x++)
            video_xr_cursor_texel(px, VIDEO_XR_HANDLE_X(k) + x,
                  VIDEO_XR_HANDLE_Y + y,
                  (video_xr_rim_depth(x, y, VIDEO_XR_HANDLE_COLS,
                     VIDEO_XR_HANDLE_ROWS) < VIDEO_XR_OUTLINE_BAND)
                  ? 0 : alpha, alpha);
   }
   for (k = 0; k < 2; k++)
      for (x = 0; x < VIDEO_XR_BEAM_LEN; x++)
      {
         float in  = ((float)x + 0.5f) / VIDEO_XR_BEAM_FADE_IN;
         float out = ((float)(VIDEO_XR_BEAM_LEN - x) - 0.5f)
            / VIDEO_XR_BEAM_FADE_OUT;
         uint8_t level;
         if (in > 1.0f)
            in = 1.0f;
         if (!k || out > 1.0f)
            out = 1.0f;
         level = (uint8_t)(in * out * VIDEO_XR_BEAM_LEVEL + 0.5f);
         for (y = 0; y < VIDEO_XR_BEAM_ROWS; y++)
            video_xr_cursor_texel(px, VIDEO_XR_BEAM_X(k) + x,
                  VIDEO_XR_BEAM_Y + y, level, level);
      }
   for (y = 0; y < VIDEO_XR_BACK_CELL; y++)
      for (x = 0; x < VIDEO_XR_BACK_CELL; x++)
         video_xr_cursor_texel(px, VIDEO_XR_BACK_X + x, VIDEO_XR_BACK_Y + y,
               VIDEO_XR_BACK_LEVEL, 255);
   for (y = 0; y < VIDEO_XR_HINGE_ROWS; y++)
      for (x = 0; x < VIDEO_XR_HINGE_COLS; x++)
      {
         uint8_t *p = px + ((size_t)(VIDEO_XR_HINGE_Y + y)
               * VIDEO_XR_CURSOR_W + VIDEO_XR_HINGE_X + x) * 4;
         p[0] = (uint8_t)((VIDEO_XR_HINGE_RB * VIDEO_XR_HINGE_LEVEL + 127)
               / 255);
         p[1] = (uint8_t)((VIDEO_XR_HINGE_G * VIDEO_XR_HINGE_LEVEL + 127)
               / 255);
         p[2] = p[0];
         p[3] = VIDEO_XR_HINGE_LEVEL;
      }
}

void video_xr_cut_pixels(uint8_t *px, bool bottom_up)
{
   unsigned k, c;
   for (k = 0; k < 2; k++)
      for (c = 0; c < VIDEO_XR_OUTLINE_CUTS; c++)
      {
         unsigned cut = video_xr_cuts[c];
         if (bottom_up)
            cut = (cut & 12u) | ((cut & 1u) << 1) | ((cut >> 1) & 1u);
         video_xr_outline_pixels(px, VIDEO_XR_CUT_X(c, k),
               k ? 255 : VIDEO_XR_OUTLINE_FAINT, cut);
      }
}

void video_xr_tag_pixels(uint8_t *px, unsigned tag, const uint8_t *cov,
      bool bottom_up)
{
   unsigned x, y;
   for (y = 0; y < VIDEO_XR_TAG_ROWS; y++)
      for (x = 0; x < VIDEO_XR_TAG_COLS; x++)
      {
         unsigned i, j, a;
         unsigned c    = cov[y * VIDEO_XR_TAG_COLS + x];
         unsigned halo = 0;
         unsigned row  = bottom_up ? VIDEO_XR_TAG_ROWS - 1 - y : y;
         for (j = (y > VIDEO_XR_TAG_HALO) ? y - VIDEO_XR_TAG_HALO : 0;
               j <= y + VIDEO_XR_TAG_HALO && j < VIDEO_XR_TAG_ROWS; j++)
            for (i = (x > VIDEO_XR_TAG_HALO) ? x - VIDEO_XR_TAG_HALO : 0;
                  i <= x + VIDEO_XR_TAG_HALO && i < VIDEO_XR_TAG_COLS;
                  i++)
               if (cov[j * VIDEO_XR_TAG_COLS + i] > halo)
                  halo = cov[j * VIDEO_XR_TAG_COLS + i];
         /* Light text over its dark halo, premultiplied. */
         a = c + halo * (255 - c) / 255;
         video_xr_cursor_texel(px, VIDEO_XR_TAG_X(tag) + x,
               VIDEO_XR_TAG_Y + row,
               (uint8_t)(c * VIDEO_XR_TAG_FAINT / 255),
               (uint8_t)(a * VIDEO_XR_TAG_FAINT / 255));
      }
}

/* Two rates this close in skew are a tie. */
#define VIDEO_XR_RATE_TIE 0.0001f

void video_xr_period_init(video_xr_period_t *f)
{
   memset(f, 0, sizeof(*f));
}

bool video_xr_period_add(video_xr_period_t *f, int64_t period_ns)
{
   int64_t s[VIDEO_XR_PERIODS];
   int64_t median, d;
   unsigned i, j;

   if (period_ns <= 0 || period_ns > VIDEO_XR_PERIOD_MAX_NS)
      return false;
   f->samples[f->next] = period_ns;
   f->next             = (f->next + 1) % VIDEO_XR_PERIODS;
   if (f->count < VIDEO_XR_PERIODS)
      f->count++;
   if (f->count < VIDEO_XR_PERIODS)
      return false;

   for (i = 0; i < VIDEO_XR_PERIODS; i++)
   {
      int64_t v = f->samples[i];
      for (j = i; j > 0 && s[j - 1] > v; j--)
         s[j] = s[j - 1];
      s[j] = v;
   }
   median = (s[VIDEO_XR_PERIODS / 2 - 1] + s[VIDEO_XR_PERIODS / 2]) / 2;
   if (     (median - s[0]) * 100 > median
         || (s[VIDEO_XR_PERIODS - 1] - median) * 100 > median)
      return false;
   if (f->published)
   {
      d = median - f->published;
      if (d < 0)
         d = -d;
      if (d * 100 <= f->published)
         return false;
   }
   f->published = median;
   return true;
}

unsigned video_xr_pace_interval(float hz, float fps, float skew,
      unsigned ceiling)
{
   unsigned n;
   if (hz <= 0.0f || fps <= 0.0f)
      return 0;
   n = runloop_video_swap_interval_for(hz, fps, skew, ceiling);
   if (!(runloop_sync_plan_for(hz, fps, (float)n, skew, false)
            & RUNLOOP_SYNC_WITHIN_SKEW))
      return 0;
   return n;
}

/* How far fps is from hz over the whole multiple nearest hz / fps, as
 * the auto swap interval rounds it. */
static float video_xr_rate_skew(float hz, float fps, unsigned ceiling)
{
   float skew;
   unsigned n = (unsigned)(hz / fps + 0.5f);
   if (n < 1)
      n = 1;
   if (n > ceiling)
      n = ceiling;
   skew = 1.0f - fps * (float)n / hz;
   return (skew < 0.0f) ? -skew : skew;
}

float video_xr_pick_rate(const float *rates, unsigned count, float fps,
      unsigned ceiling)
{
   unsigned i;
   float best      = 0.0f;
   float best_skew = 0.0f;
   if (fps <= 0.0f)
      return 0.0f;
   for (i = 0; i < count; i++)
   {
      float skew;
      if (rates[i] <= 0.0f)
         continue;
      skew = video_xr_rate_skew(rates[i], fps, ceiling);
      if (     best <= 0.0f
            || skew < best_skew - VIDEO_XR_RATE_TIE
            || (skew <= best_skew + VIDEO_XR_RATE_TIE && rates[i] > best))
      {
         best      = rates[i];
         best_skew = skew;
      }
   }
   return best;
}

float video_xr_request_rate(unsigned setting, const float *rates,
      unsigned count, float fps, unsigned ceiling)
{
   unsigned i;
   if (setting == VIDEO_OPENXR_REFRESH_AUTO)
      return video_xr_pick_rate(rates, count, fps, ceiling);
   if (setting == VIDEO_OPENXR_REFRESH_HEADSET)
      return 0.0f;
   for (i = 0; i < count; i++)
      if (fabs(rates[i] - (float)setting) < 0.5)
         return rates[i];
   return 0.0f;
}

/* Zeroed is the setting's default: off. */
static retro_atomic_int_t video_xr_perf_boost_on;

void video_xr_set_perf_boost(bool boost)
{
   retro_atomic_store_release_int(&video_xr_perf_boost_on, boost ? 1 : 0);
}

bool video_xr_perf_boost(void)
{
   return retro_atomic_load_acquire_int(&video_xr_perf_boost_on) != 0;
}
