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

#include "video_defines.h"
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
/* Following screen 0, the UI comes no nearer the anchor than this. */
#define VIDEO_XR_MENU_NEAREST 0.40f
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
   VIDEO_XR_QUAD_MENU,
   /* A whole frame that is the window: the menu is drawn over it. */
   VIDEO_XR_QUAD_WINDOW
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
   video_xr_vec3_t base;   /* its item's layout spot, the anchor's frame */
   unsigned edit_seq;      /* edit mode's count when it was planned */
   /* A frame shown whole without the screens' settings (a views core's
    * fallback): their edits would not move it, so it is no item. */
   bool fixed;
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
   /* Floating-screen adjustments by screen ID; NULL uses defaults. */
   const video_screen_xr_t *screens;
   /* The menu's own place from its spot; NULL: in front of screen 0. */
   const video_screen_xr_t *menu;
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
   /* Edit mode's count of let-go grabs, carried by every slot. */
   unsigned edit_seq;

   /* Screens the headset's projection shows, a bit each: no quad, and
    * the others keep their places. */
   unsigned skip_screens;
   bool swap_eyes;
   /* The whole frame is the window, with the menu drawn in it. */
   bool window;
   /* The screens are drawn by the stock chain, which shrinks smoothly:
    * their images are the shown size, even below the source's. */
   bool stock;
} video_xr_params_t;

/* Where video_xr_place starts each floating item, in the anchor's
 * frame, before its adjustments: screen s of the map (a whole frame is
 * screen 0) centred at pos[s], width[s] by height[s] metres at scale 1,
 * from a source native[s] in size (VIDEO_SCALE_PACK); the menu's own
 * spot at menu. count screens. */
typedef struct video_xr_layout
{
   video_xr_vec3_t pos[RETRO_VIDEO_VIEWS_MAX];
   video_xr_vec3_t menu;
   float width[RETRO_VIDEO_VIEWS_MAX];
   float height[RETRO_VIDEO_VIEWS_MAX];
   unsigned native[RETRO_VIDEO_VIEWS_MAX];
   unsigned count;
} video_xr_layout_t;

void video_xr_layout(const video_xr_params_t *p, video_xr_layout_t *out);

/* An item's pose in the anchor's frame: at base moved by screen's
 * offsets, turned by its yaw, pitch and roll; NULL leaves it at base. */
void video_xr_item_pose(const video_xr_vec3_t *base,
      const video_screen_xr_t *screen, video_xr_pose_t *out);

/* The offsets and turn that put an item from base at pose, both in the
 * anchor's frame; out's scale and visible are left as they are. */
void video_xr_item_settings(const video_xr_vec3_t *base,
      const video_xr_pose_t *pose, video_screen_xr_t *out);

/* Edit mode's stick: its deadzone as a fraction of its travel; at full
 * travel past it, the push along the aim's ray in metres a second and
 * the resize in doublings a second. A pulled item comes no nearer the
 * hand than VIDEO_XR_EDIT_REACH metres. */
#define VIDEO_XR_EDIT_DEADZONE   0.2f
#define VIDEO_XR_EDIT_PUSH_RATE  1.5f
#define VIDEO_XR_EDIT_SCALE_RATE 1.0f
#define VIDEO_XR_EDIT_REACH      0.1f

enum video_xr_grab_state
{
   VIDEO_XR_GRAB_NONE = 0,
   VIDEO_XR_GRAB_HELD,
   /* Let go: the item stays where the hand left it until a quad set
    * planned at release_seq or later shows it. */
   VIDEO_XR_GRAB_RELEASED
};

/* A hand's hold on a floating item, in the anchor's frame. */
typedef struct video_xr_grab
{
   video_xr_pose_t rel;    /* the item in the aim pose's frame */
   video_xr_pose_t last;   /* where the hand last put it */
   video_xr_vec3_t base;   /* its layout spot */
   float roll;             /* degrees, kept while it does not tilt */
   float width;            /* metres, at its scale now */
   float height;
   unsigned slot;          /* its quads' slot */
   unsigned release_seq;
   unsigned state;         /* enum video_xr_grab_state */
   bool tilt;              /* its roll follows the hand too */
} video_xr_grab_t;

/* Where g puts its item for an aim pose: the aim composed with g->rel,
 * levelled to g->roll unless g->tilt, its offsets from g->base clamped
 * to the settings' range. Writes the pose and, unless out is NULL, the
 * offsets and turn. Returns whether a clamp held it back; the caller
 * then holds g->rel again from pose, so the item resumes from the
 * limit. */
bool video_xr_grab_pose(const video_xr_grab_t *g, const video_xr_pose_t *aim,
      video_xr_pose_t *pose, video_screen_xr_t *out);

/* The stick for dt seconds: past the deadzone y (up, +1) pushes g's
 * item along the aim's ray and x (right, +1) enlarges it, each by how
 * far past it is; *scale stays within the setting's range. */
void video_xr_grab_stick(video_xr_grab_t *g, float x, float y, double dt,
      float *scale);

void video_xr_pose_identity(video_xr_pose_t *pose);

/* out = q applied to v; out may be v. */
void video_xr_rotate(const video_xr_quat_t *q, const video_xr_vec3_t *v,
      video_xr_vec3_t *out);

/* The head's position and heading, level. False when it looks straight
 * up or down and has no heading. */
bool video_xr_anchor_from_head(const video_xr_pose_t *head,
      video_xr_pose_t *anchor);

/* out = b, a pose in a's frame, in a's space; out may be a or b. */
void video_xr_compose(const video_xr_pose_t *a, const video_xr_pose_t *b,
      video_xr_pose_t *out);

/* out = a's inverse: video_xr_compose(a, out) is the identity; out may
 * be a. */
void video_xr_pose_invert(const video_xr_pose_t *a, video_xr_pose_t *out);

/* q's turn in degrees, in the order an item's adjustments turn it: yaw
 * about +Y, pitch about +X, roll about +Z. Pitch is within [-90, 90];
 * straight up or down, roll is 0 and yaw takes the whole turn about the
 * vertical. */
void video_xr_euler(const video_xr_quat_t *q, float *yaw, float *pitch,
      float *roll);

/* q with its roll set to roll degrees, its yaw and pitch kept; straight
 * up or down, where it has no roll of its own, q as it is. */
void video_xr_level(video_xr_quat_t *q, float roll);

/* Lazy Follow: the screens stay until the head's heading is this far
 * from theirs, or the head this far from where they were placed from;
 * then they glide back, closing about two thirds of the way each
 * VIDEO_XR_LAZY_TIME seconds, until this close. */
#define VIDEO_XR_LAZY_START_DEG 30.0
#define VIDEO_XR_LAZY_START_M   0.5
#define VIDEO_XR_LAZY_STOP_DEG  1.0
#define VIDEO_XR_LAZY_STOP_M    0.01
#define VIDEO_XR_LAZY_TIME      0.3

/* One frame, dt seconds, of Lazy Follow: anchor (level, as
 * video_xr_anchor_from_head makes them) moves towards target while it
 * glides. Returns whether it still glides. */
bool video_xr_lazy_follow(video_xr_pose_t *anchor,
      const video_xr_pose_t *target, double dt, bool gliding);

/* A quad's image size: its angular width at px_per_rad, never below
 * native_dims' width (the source's; 0 for no floor), the quad's shape,
 * at most max_dim a side. 0 for an empty quad. */
unsigned video_xr_image_dims(float width_m, float height_m, float distance,
      float px_per_rad, unsigned native_dims, unsigned max_dim);

/* Whether an image of image_dims shows a source of source_dims smaller
 * in either dimension, the source turned rotation quarter turns. */
bool video_xr_shrinks(unsigned image_dims, unsigned source_dims,
      unsigned rotation);

/* A slot's image size: while editing, current (the image it has, 0 for
 * none) if planned is within 0.8 to 1.25 times it on both axes, so a
 * screen being moved is not given a new swapchain at each distance;
 * planned otherwise. */
unsigned video_xr_keep_dims(unsigned planned, unsigned current,
      bool editing);

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

/* A point on a window's quad as one on the core's frame, which the
 * window draws in vp (the video driver's viewport, from the window's
 * top left). False on the bars around it. */
bool video_xr_window_point(const video_viewport_t *vp, float *u, float *v);

/* Where view v of a frame of frame_dims shows in a window that draws
 * the frame in vp, as video_xr_window_point maps it back, the frame
 * turned rotation quarter turns anticlockwise: in window pixels from
 * its top left, reaching past the window where vp does. */
void video_xr_window_rect(const video_viewport_t *vp, unsigned frame_dims,
      unsigned rotation, const struct retro_video_view *v,
      video_views_rect_t *out);

/* The quads of a window that holds the frame, the menu and all, as
 * the Quest's GL host shows it: p's map as video_xr_place puts its screens
 * while it is untracked and the menu is closed, each slot's layers cut
 * from the window at rects; else the window whole, its rect the window.
 * vp and frame_dims as video_xr_window_rect's, turned p->rotation. */
void video_xr_place_window(const video_xr_params_t *p, bool menu,
      bool tracked, const video_viewport_t *vp, unsigned frame_dims,
      video_xr_quad_set_t *out,
      video_views_rect_t rects[VIDEO_XR_MAX_SLOTS][2]);

/* The laser's image, its parts side by side with a clear column
 * between each, so filtering at one's edge takes in none of the next,
 * each centred on the image's middle row so GL's bottom-up rows find
 * the same texels: its dot, VIDEO_XR_CURSOR_DIM square at the left;
 * edit mode's faint and bright outlines (lit 0 and 1), each
 * VIDEO_XR_OUTLINE_DIM square with a rim of three VIDEO_XR_OUTLINE_BAND
 * texel bands, dark, light, dark, so a light band always has a dark
 * neighbour; the menu's faint and bright handle bars,
 * VIDEO_XR_HANDLE_COLS by VIDEO_XR_HANDLE_ROWS, light inside a dark
 * band; the beam's strips to the dot and into nothing (miss 0 and 1),
 * VIDEO_XR_BEAM_LEN long from the hand along x. */
#define VIDEO_XR_CURSOR_DIM     32
#define VIDEO_XR_OUTLINE_DIM    256
#define VIDEO_XR_OUTLINE_BAND   2
#define VIDEO_XR_OUTLINE_EDGE   (3 * VIDEO_XR_OUTLINE_BAND)
#define VIDEO_XR_OUTLINE_FAINT  64
#define VIDEO_XR_HANDLE_COLS    128
#define VIDEO_XR_HANDLE_ROWS    8
#define VIDEO_XR_BEAM_LEN       128
#define VIDEO_XR_BEAM_ROWS      4
#define VIDEO_XR_OUTLINE_X(lit) \
   (VIDEO_XR_CURSOR_DIM + 1 + (lit) * (VIDEO_XR_OUTLINE_DIM + 1))
#define VIDEO_XR_HANDLE_X(lit) \
   (VIDEO_XR_OUTLINE_X(2) + (lit) * (VIDEO_XR_HANDLE_COLS + 1))
#define VIDEO_XR_BEAM_X(miss) \
   (VIDEO_XR_HANDLE_X(2) + (miss) * (VIDEO_XR_BEAM_LEN + 1))
#define VIDEO_XR_CURSOR_W       (VIDEO_XR_BEAM_X(1) + VIDEO_XR_BEAM_LEN)
#define VIDEO_XR_CURSOR_H       VIDEO_XR_OUTLINE_DIM
#define VIDEO_XR_CURSOR_Y       ((VIDEO_XR_CURSOR_H - VIDEO_XR_CURSOR_DIM) / 2)
#define VIDEO_XR_HANDLE_Y       ((VIDEO_XR_CURSOR_H - VIDEO_XR_HANDLE_ROWS) / 2)
#define VIDEO_XR_BEAM_Y         ((VIDEO_XR_CURSOR_H - VIDEO_XR_BEAM_ROWS) / 2)
/* The beam's strips: white at this level, fading in over their first
 * FADE_IN texels; the one into nothing out over its last FADE_OUT. */
#define VIDEO_XR_BEAM_LEVEL     160
#define VIDEO_XR_BEAM_FADE_IN   8
#define VIDEO_XR_BEAM_FADE_OUT  32

/* Its pixels, RGBA8 premultiplied, grey so RGBA and BGRA hold the same
 * bytes: a white dot with a dark rim and a one-pixel soft edge; outline
 * rims and handle bars in black and white, the faint ones at
 * VIDEO_XR_OUTLINE_FAINT alpha; white beam strips. px holds
 * VIDEO_XR_CURSOR_W by VIDEO_XR_CURSOR_H times four bytes. */
void video_xr_cursor_pixels(uint8_t *px);

/* The laser's dot: this wide per metre along the ray, never narrower
 * than VIDEO_XR_CURSOR_MIN, just in front of the quad it is on. */
#define VIDEO_XR_CURSOR_SCALE 0.012f
#define VIDEO_XR_CURSOR_MIN   0.005f
#define VIDEO_XR_CURSOR_LIFT  0.002f

/* Whether a laser (enum video_openxr_laser, or VIDEO_XR_LIVE_EDIT) can
 * point at q. Auto: the menu or a window while the menu is open, and
 * screens 1 and up. Always: every screen and a whole frame too. Off:
 * nothing. Edit mode's: every item. */
#define VIDEO_XR_LIVE_EDIT VIDEO_OPENXR_LASER_LAST
bool video_xr_quad_live(const video_xr_quad_t *q, unsigned laser,
      bool menu_open);

/* Edit mode takes the menu by a handle bar centred under its bottom
 * edge, in its plane: this fraction of its width, this many metres
 * tall and this many below its outline. */
#define VIDEO_XR_HANDLE_WIDTH  0.4f
#define VIDEO_XR_HANDLE_HEIGHT 0.03f
#define VIDEO_XR_HANDLE_GAP    0.005f

/* The menu quad's handle: the menu's quad, moved and sized to it. */
void video_xr_menu_handle(const video_xr_quad_t *menu, video_xr_quad_t *out);

/* The nearest live quad the ray meets, as an index into set->quads, or
 * -1; of a stereo screen's two quads, the first. t is in units of
 * dir's length. Edit mode's laser passes fixed slots' quads and meets
 * the menu only at its handle, at (u, v) in the menu's own frame, v past
 * 1; through the menu, what is behind. */
int video_xr_pick(const video_xr_quad_set_t *set, unsigned laser,
      bool menu_open, const video_xr_vec3_t *origin,
      const video_xr_vec3_t *dir, float *u, float *v, float *t);

/* The dot where a ray dist metres long meets q at (u, v): its pose,
 * facing as q does. Returns its width in metres. */
float video_xr_cursor(const video_xr_quad_t *q, float u, float v,
      float dist, video_xr_pose_t *pose);

/* The laser's beam: this wide, and this long with nothing under the
 * laser. */
#define VIDEO_XR_BEAM_WIDTH 0.004f
#define VIDEO_XR_BEAM_REACH 1.5f

/* A beam len metres long from o along the unit ray d: its pose at its
 * middle, its x down the ray, turned about the ray so its face is
 * towards eye. */
void video_xr_beam(const video_xr_vec3_t *o, const video_xr_vec3_t *d,
      float len, const video_xr_vec3_t *eye, video_xr_pose_t *pose);

/* Edit mode's outline round q: q, grown so the rim's inner edge, with
 * the half texel filtering spreads it by, is VIDEO_XR_OUTLINE_GAP
 * metres outside q's on every side. */
#define VIDEO_XR_OUTLINE_GAP 0.005f
void video_xr_outline(const video_xr_quad_t *q, video_xr_quad_t *out);

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

/* Headset Performance Boost: whether a session asks the headset for
 * its higher clocks. Set from the setting each poll, read by the XR
 * thread; any thread. */
void video_xr_set_perf_boost(bool boost);
bool video_xr_perf_boost(void);

RETRO_END_DECLS

#endif
