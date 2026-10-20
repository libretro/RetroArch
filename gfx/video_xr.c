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

/* The menu VIDEO_XR_MENU_LIFT in front of screen 0, facing as it does,
 * while screen 0 floats and shows, else at its own spot. Following, it
 * comes no nearer than VIDEO_XR_MENU_NEAREST, and w and h scale with
 * its distance so it looks as large as at its spot; facing away from
 * the anchor, where no laser reaches it, it stays at its spot. */
static void video_xr_menu_spot(const video_xr_params_t *p,
      const video_xr_layout_t *lay, video_xr_pose_t *pose,
      double *w, double *h)
{
   video_xr_pose_t local, lift;
   video_screen_xr_t screen;
   bool follow = lay->count && !(p->skip_screens & 1u);
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

bool video_xr_ray_hit(const video_xr_quad_t *q, const video_xr_vec3_t *origin,
      const video_xr_vec3_t *dir, float *u, float *v, float *t)
{
   double denom, dist, hu, hv;
   video_xr_quat_t inv;
   video_xr_vec3_t z, n, rel, local;

   z.x = 0.0f;
   z.y = 0.0f;
   z.z = 1.0f;
   video_xr_rotate(&q->pose.orientation, &z, &n);
   denom = n.x * dir->x + n.y * dir->y + n.z * dir->z;
   /* Only the front face. */
   if (denom > -1e-6)
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
      const video_xr_vec3_t *origin, const video_xr_vec3_t *dir,
      float *u, float *v, float *t, bool *bar)
{
   float hu, hv;
   video_xr_quad_t handle;
   if (q->width <= 0.0f || q->height <= 0.0f)
      return false;
   video_xr_menu_handle(q, &handle);
   handle.width  += 2.0f * VIDEO_XR_HANDLE_MARGIN;
   handle.height += 2.0f * VIDEO_XR_HANDLE_MARGIN;
   if (!video_xr_ray_hit(&handle, origin, dir, &hu, &hv, t))
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
         if (video_xr_handle_hit(q, origin, dir, &qu, &qv, &qt, &bar))
         {
            handle = (int)i;
            *u     = qu;
            *v     = qv;
            *t     = qt;
         }
         continue;
      }
      if (!video_xr_ray_hit(q, origin, dir, &qu, &qv, &qt))
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

void video_xr_outline(const video_xr_quad_t *q, video_xr_quad_t *out)
{
   const float inner = 1.0f - 2.0f * (VIDEO_XR_OUTLINE_EDGE + 0.5f)
      / VIDEO_XR_OUTLINE_DIM;
   *out        = *q;
   out->width  = (q->width + 2.0f * VIDEO_XR_OUTLINE_GAP) / inner;
   out->height = (q->height + 2.0f * VIDEO_XR_OUTLINE_GAP) / inner;
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

/* A screen a held one may snap to: the turn it is nearest, parallel or
 * square, how many degrees off that it is, and the share of its pulls:
 * the gap's, times the grab's fade-in. */
typedef struct video_xr_snap_target
{
   const video_xr_quad_t *q;
   video_xr_quat_t goal;
   double angle;
   double share;
   bool square;
} video_xr_snap_target_t;

/* The best line yet on one of the held screen's axes: how far off it
 * is, and the share of the pull it gets. */
typedef struct video_xr_snap_line
{
   double d;
   double k;
   bool any;
} video_xr_snap_line_t;

/* Nearer by less than this, in metres or degrees, is rounding: a tie. */
#define VIDEO_XR_SNAP_TIE 1e-5

/* -1 when a is below b by more than rounding, 1 above, else 0. */
static int video_xr_snap_cmp(double a, double b)
{
   if (a < b - VIDEO_XR_SNAP_TIE)
      return -1;
   return (a > b + VIDEO_XR_SNAP_TIE) ? 1 : 0;
}

/* How far off its line d stays after a pull of share k. */
static double video_xr_snap_left(double d, double k)
{
   return fabs(d) - fabs(video_xr_snap_pull(d, VIDEO_XR_SNAP_DIST)) * k;
}

/* The line kept is the one left nearest after its pull (a whole pull
 * leaves exactly 0), then the stronger pull, then the nearest, then
 * the first. */
static void video_xr_snap_offer(video_xr_snap_line_t *l, double d,
      double k)
{
   if (k <= 0.0 || fabs(d) >= 2.0 * VIDEO_XR_SNAP_DIST)
      return;
   if (l->any)
   {
      double e = video_xr_snap_left(d, k);
      double f = video_xr_snap_left(l->d, l->k);
      int c    = (e < f) ? -1 : (e > f) ? 1 : 0;
      if (!c)
         c = (k > l->k) ? -1 : (k < l->k) ? 1 : 0;
      if (!c)
         c = video_xr_snap_cmp(fabs(d), fabs(l->d));
      if (c >= 0)
         return;
   }
   l->d   = d;
   l->k   = k;
   l->any = true;
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

/* q as a target for a screen at pose, a and b its half width and
 * height; false when too far off in turn or place to pull at all. */
static bool video_xr_snap_target(const video_xr_quad_t *q,
      const video_xr_pose_t *pose, double a, double b,
      video_xr_snap_target_t *t)
{
   unsigned i;
   double gx, gy, gz;
   video_xr_pose_t inv, rel, turn, goal;
   t->q      = q;
   t->goal   = q->pose.orientation;
   t->angle  = video_xr_quat_angle(&pose->orientation, &t->goal);
   t->square = false;
   video_xr_pose_identity(&turn);
   for (i = 0; i < 2; i++)
   {
      double e;
      /* A quarter turn either way about its up. */
      turn.orientation.y = i ? -0.70710678f : 0.70710678f;
      turn.orientation.w = 0.70710678f;
      video_xr_compose(&q->pose, &turn, &goal);
      e = video_xr_quat_angle(&pose->orientation, &goal.orientation);
      if (e < t->angle)
      {
         t->goal   = goal.orientation;
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
   t->share = video_xr_snap_share(sqrt(gx * gx + gy * gy + gz * gz),
         VIDEO_XR_SNAP_NEAR);
   return t->share > 0.0;
}

unsigned video_xr_snap(const video_xr_quad_set_t *set, unsigned slot,
      unsigned skip, float width, float height, float fade,
      video_xr_pose_t *pose)
{
   unsigned i;
   double e;
   double turn = 0.0;
   video_xr_snap_target_t t[RETRO_VIDEO_VIEWS_MAX];
   video_xr_snap_line_t line[3];
   video_xr_pose_t inv;
   video_xr_vec3_t move, world;
   double pull[3];
   unsigned n       = 0;
   unsigned done    = 0;
   unsigned snapped = 0;
   int ref          = -1;
   double a         = width * 0.5;
   double b         = height * 0.5;

   if (!set || slot >= VIDEO_XR_MENU_SLOT || fade <= 0.0f)
      return 0;
   for (i = 0; i < set->num_quads && n < RETRO_VIDEO_VIEWS_MAX; i++)
   {
      const video_xr_quad_t *q = &set->quads[i];
      if (     q->slot >= VIDEO_XR_MENU_SLOT || q->slot == slot
            || q->kind == VIDEO_XR_QUAD_MENU
            || (((skip | done) >> q->slot) & 1u)
            || set->slots[q->slot].fixed)
         continue;
      done |= 1u << q->slot;
      if (video_xr_snap_target(q, pose, a, b, &t[n]))
      {
         t[n].share *= fade;
         n++;
      }
   }
   if (!n)
      return 0;

   /* The turn, to the target whose pull leaves it nearest parallel or
    * square; then the stronger pull, the smaller turn, the first. */
   for (i = 0; i < n; i++)
   {
      double k = video_xr_snap_turn(&t[i]);
      int c    = (ref < 0) ? -1 : video_xr_snap_cmp(
            t[i].angle * (1.0 - k), t[ref].angle * (1.0 - turn));
      if (!c)
         c = (k > turn) ? -1 : (k < turn) ? 1 : 0;
      if (!c && t[i].angle < t[ref].angle)
         c = -1;
      if (c < 0)
      {
         ref  = (int)i;
         turn = k;
      }
   }
   e = t[ref].angle;
   video_xr_quat_toward(&pose->orientation, &t[ref].goal, turn);
   if (e <= VIDEO_XR_SNAP_ANGLE && t[ref].share >= 1.0)
      snapped |= VIDEO_XR_SNAP_TURN;
   if (turn > 0.0)
      snapped |= VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_TURN);

   /* Lines along the held screen's own axes, as now turned; they fade
    * with each target's turn. */
   memset(line, 0, sizeof(line));
   video_xr_pose_invert(pose, &inv);
   for (i = 0; i < n; i++)
   {
      video_xr_pose_t rel;
      double x, y, z;
      double A = t[i].q->width * 0.5;
      double B = t[i].q->height * 0.5;
      double k = t[i].share
         * video_xr_snap_share(t[i].angle, VIDEO_XR_SNAP_ANGLE);
      video_xr_compose(&inv, &t[i].q->pose, &rel);
      x = rel.position.x;
      y = rel.position.y;
      z = rel.position.z;
      if (!t[i].square)
      {
         video_xr_snap_offer(&line[0], x - A + a, k);
         video_xr_snap_offer(&line[0], x + A - a, k);
         video_xr_snap_offer(&line[0], x,         k);
         video_xr_snap_offer(&line[0], x + A + a, k);
         video_xr_snap_offer(&line[0], x - A - a, k);
         video_xr_snap_offer(&line[1], y + B - b, k);
         video_xr_snap_offer(&line[1], y - B + b, k);
         video_xr_snap_offer(&line[1], y,         k);
         video_xr_snap_offer(&line[1], y + B + b, k);
         video_xr_snap_offer(&line[1], y - B - b, k);
         video_xr_snap_offer(&line[2], z,         k);
      }
      else
      {
         /* A vertical edge into its plane at one of its vertical edges:
          * a corner, both together or neither. */
         double dx = video_xr_snap_nearer(x - a, x + a);
         double dz = video_xr_snap_nearer(z - A, z + A);
         video_xr_snap_offer(&line[0], dx,
               k * video_xr_snap_share(dz, VIDEO_XR_SNAP_DIST));
         video_xr_snap_offer(&line[2], dz,
               k * video_xr_snap_share(dx, VIDEO_XR_SNAP_DIST));
         video_xr_snap_offer(&line[1], y + B - b, k);
         video_xr_snap_offer(&line[1], y - B + b, k);
         video_xr_snap_offer(&line[1], y,         k);
      }
   }
   for (i = 0; i < 3; i++)
   {
      pull[i] = 0.0;
      if (!line[i].any)
         continue;
      snapped |= VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_X << i);
      pull[i] = video_xr_snap_pull(line[i].d, VIDEO_XR_SNAP_DIST)
         * line[i].k;
      if (fabs(line[i].d) <= VIDEO_XR_SNAP_DIST && line[i].k >= 1.0)
         snapped |= VIDEO_XR_SNAP_X << i;
   }
   move.x = (float)pull[0];
   move.y = (float)pull[1];
   move.z = (float)pull[2];
   video_xr_rotate(&pose->orientation, &move, &world);
   pose->position.x += world.x;
   pose->position.y += world.y;
   pose->position.z += world.z;
   return snapped;
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

bool video_xr_grab_pose(const video_xr_grab_t *g, const video_xr_pose_t *aim,
      const video_xr_quad_set_t *targets, unsigned skip,
      const video_xr_pose_t *start, video_xr_pose_t *pose,
      video_screen_xr_t *out, unsigned *snapped)
{
   video_screen_xr_t s, c;
   bool clamped;
   unsigned snaps = 0;
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
   if (targets)
      snaps = video_xr_snap(targets, g->slot, skip, g->width, g->height,
            video_xr_snap_fade(start, pose), pose);
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

void video_xr_grab_turn(video_xr_grab_t *g, const video_xr_pose_t *aim,
      float x, double dt)
{
   double n, s;
   video_xr_pose_t turn, item;
   video_xr_vec3_t axis;
   video_xr_quat_t *q;
   double half = video_xr_deflection(x) * VIDEO_XR_EDIT_TURN_RATE
      * dt * VIDEO_XR_PI / 360.0;
   if (half == 0.0)
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
      for (y = 0; y < VIDEO_XR_OUTLINE_DIM; y++)
         for (x = 0; x < VIDEO_XR_OUTLINE_DIM; x++)
         {
            unsigned band = video_xr_rim_depth(x, y, VIDEO_XR_OUTLINE_DIM,
                  VIDEO_XR_OUTLINE_DIM) / VIDEO_XR_OUTLINE_BAND;
            if (band < 3)
               video_xr_cursor_texel(px, VIDEO_XR_OUTLINE_X(k) + x, y,
                     (band == 1) ? alpha : 0, alpha);
         }
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
