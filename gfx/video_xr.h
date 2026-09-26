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

#ifndef __VIDEO_XR_H
#define __VIDEO_XR_H

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "video_views.h"

RETRO_BEGIN_DECLS

/* Where a headset shows the core's screens: quads in OpenXR's LOCAL
 * space (metres, +Y up, the viewer starts looking down -Z), what each
 * quad's swapchain image holds, and how a point on a quad maps back to
 * the core's frame. A quad faces +Z of its pose. */

/* Slot s < num_screens holds screen s; the UI has its own slot; a
 * whole frame uses slot 0. */
#define VIDEO_XR_MENU_SLOT RETRO_VIDEO_VIEWS_MAX
#define VIDEO_XR_MAX_SLOTS (RETRO_VIDEO_VIEWS_MAX + 1)
#define VIDEO_XR_MAX_QUADS (RETRO_VIDEO_VIEWS_MAX * 2 + 1)
/* Gap between screens, as a fraction of screen 0's width. */
#define VIDEO_XR_GAP       0.02f
/* How far the UI floats in front of screen 0, in metres. */
#define VIDEO_XR_MENU_LIFT 0.10f
/* The UI's quad fits in screen 0's width by this much of it. */
#define VIDEO_XR_MENU_MAX_H 0.75f
/* At most this many UI pixels per headset pixel across its quad. */
#define VIDEO_XR_MENU_DENSITY 2.0f

enum video_xr_eye
{
   VIDEO_XR_EYE_BOTH = 0,
   VIDEO_XR_EYE_LEFT,
   VIDEO_XR_EYE_RIGHT
};

enum video_xr_kind
{
   VIDEO_XR_QUAD_SCREEN = 0,
   VIDEO_XR_QUAD_FRAME,
   VIDEO_XR_QUAD_MENU
};

typedef struct video_xr_vec3
{
   float x;
   float y;
   float z;
} video_xr_vec3_t;

typedef struct video_xr_quat
{
   float x;
   float y;
   float z;
   float w;
} video_xr_quat_t;

typedef struct video_xr_pose
{
   video_xr_quat_t orientation;
   video_xr_vec3_t position;
} video_xr_pose_t;

typedef struct video_xr_quad
{
   video_xr_pose_t pose;   /* centre */
   float width;            /* metres */
   float height;
   unsigned kind;          /* enum video_xr_kind */
   unsigned eye;           /* enum video_xr_eye */
   unsigned screen;        /* the core's screen, for a SCREEN quad */
   unsigned slot;
   unsigned layer;         /* the slot's array layer it shows */
} video_xr_quad_t;

typedef struct video_xr_slot
{
   unsigned dims;          /* image size, VIDEO_SCALE_PACK; 0 unused */
   unsigned layers;        /* 2 for a stereo screen */
   int view[2];            /* map view drawn per layer; -1 frame or UI */
} video_xr_slot_t;

typedef struct video_xr_quad_set
{
   video_xr_quad_t quads[VIDEO_XR_MAX_QUADS];
   video_xr_slot_t slots[VIDEO_XR_MAX_SLOTS];
   unsigned num_quads;
} video_xr_quad_set_t;

typedef struct video_xr_params
{
   /* The frame's views; NULL or empty for one quad of the whole
    * frame, shown at frame_aspect (0: frame_dims' own). */
   const video_views_map_t *map;
   float frame_aspect;
   unsigned frame_dims;
   /* Screen 0 is centred straight ahead of this position and yaw. */
   video_xr_pose_t anchor;
   float distance;
   float width;
   /* Headset pixels per radian; 0 sizes images at the source's own. */
   float px_per_rad;
   unsigned max_dim;
   unsigned screen_layout;
   unsigned rotation;
   /* The UI layer's size, or 0 for no menu quad. */
   unsigned ui_dims;
   bool swap_eyes;
   /* The screens are drawn by the stock chain, which shrinks smoothly:
    * their images are the shown size, even below the source's. */
   bool stock;
} video_xr_params_t;

void video_xr_pose_identity(video_xr_pose_t *pose);

/* out = q applied to v; out may be v. */
void video_xr_rotate(const video_xr_quat_t *q, const video_xr_vec3_t *v,
      video_xr_vec3_t *out);

/* The head's position and heading, level. False when it looks straight
 * up or down and has no heading. */
bool video_xr_anchor_from_head(const video_xr_pose_t *head,
      video_xr_pose_t *anchor);

/* A quad's image size: its angular width at px_per_rad, never below
 * native_dims' width (the source's; 0 for no floor), the quad's shape,
 * at most max_dim a side. 0 for an empty quad. */
unsigned video_xr_image_dims(float width_m, float height_m, float distance,
      float px_per_rad, unsigned native_dims, unsigned max_dim);

/* Whether an image of image_dims shows a source of source_dims smaller
 * in either dimension, the source turned rotation quarter turns. */
bool video_xr_shrinks(unsigned image_dims, unsigned source_dims,
      unsigned rotation);

void video_xr_place(const video_xr_params_t *p, video_xr_quad_set_t *out);

/* Where a ray (origin + t * dir, t >= 0) meets a quad's front face:
 * u left to right, v top to bottom, both in [0, 1]. */
bool video_xr_ray_hit(const video_xr_quad_t *q, const video_xr_vec3_t *origin,
      const video_xr_vec3_t *dir, float *u, float *v, float *t);

/* A point on a quad as packed-frame pointer coordinates: any copy of a
 * screen lands in its EYE_NONE or left view at the same fraction, a
 * whole frame spreads over the frame. map and frame_dims are the
 * core's own. False for the menu. Rotated views map unrotated, as the
 * window's touch does. */
bool video_xr_quad_to_frame(const video_xr_quad_t *q, float u, float v,
      const video_views_map_t *map, unsigned frame_dims,
      int16_t *res_x, int16_t *res_y);

/* The laser's dot: this wide per metre along the ray, never narrower
 * than VIDEO_XR_CURSOR_MIN, just in front of the quad it is on. */
#define VIDEO_XR_CURSOR_SCALE 0.012f
#define VIDEO_XR_CURSOR_MIN   0.005f
#define VIDEO_XR_CURSOR_LIFT  0.002f

/* Whether a laser (enum video_openxr_laser) can point at q. Auto: the
 * menu while it is open, and screens 1 and up. Always: every screen
 * and a whole frame too. Off: nothing. */
bool video_xr_quad_live(const video_xr_quad_t *q, unsigned laser,
      bool menu_open);

/* The nearest live quad the ray meets, as an index into set->quads, or
 * -1; of a stereo screen's two quads, the first. t is in units of
 * dir's length. */
int video_xr_pick(const video_xr_quad_set_t *set, unsigned laser,
      bool menu_open, const video_xr_vec3_t *origin,
      const video_xr_vec3_t *dir, float *u, float *v, float *t);

/* The dot where a ray dist metres long meets q at (u, v): its pose,
 * facing as q does. Returns its width in metres. */
float video_xr_cursor(const video_xr_quad_t *q, float u, float v,
      float dist, video_xr_pose_t *pose);

/* Headset pacing. The XR thread's filter over each frame's predicted
 * display period: a period is published once VIDEO_XR_PERIODS frames
 * agree within 1% of their median, and again when a new median is more
 * than 1% from it. Periods outside (0, 1 s] are ignored. */
#define VIDEO_XR_PERIODS       16
#define VIDEO_XR_PERIOD_MAX_NS 1000000000

typedef struct video_xr_period
{
   int64_t samples[VIDEO_XR_PERIODS];
   int64_t published;   /* ns; 0 until settled */
   unsigned count;
   unsigned next;
} video_xr_period_t;

void video_xr_period_init(video_xr_period_t *f);

/* True when the published period changed. */
bool video_xr_period_add(video_xr_period_t *f, int64_t period_ns);

/* How many headset frames at hz each core frame at fps shows for: the
 * auto swap interval's multiple, up to ceiling, when the sync plan puts
 * the core within skew of hz / N; 0 when it doesn't fit. */
unsigned video_xr_pace_interval(float hz, float fps, float skew,
      unsigned ceiling);

/* Auto: of the count rates offered, the one whose nearest whole
 * multiple of fps, up to ceiling, is closest; the higher on a tie; 0
 * with none. */
float video_xr_pick_rate(const float *rates, unsigned count, float fps,
      unsigned ceiling);

/* The rate to ask for under a Headset Refresh Rate setting (enum
 * video_openxr_refresh, or Hz): Auto's pick, nothing for Headset's
 * Choice, or the offered rate within half a hertz of the setting; 0
 * for none. */
float video_xr_request_rate(unsigned setting, const float *rates,
      unsigned count, float fps, unsigned ceiling);

RETRO_END_DECLS

#endif
