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

#ifndef __VIDEO_VIEWS_H
#define __VIDEO_VIEWS_H

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_inline.h>
#include <retro_atomic.h>
#include <retro_common_api.h>
#include <libretro.h>

RETRO_BEGIN_DECLS

enum video_stereo_mode
{
   VIDEO_STEREO_MODE_2D = 0,
   VIDEO_STEREO_MODE_SBS_HALF,
   VIDEO_STEREO_MODE_SBS_FULL,
   VIDEO_STEREO_MODE_TOP_BOTTOM,
   VIDEO_STEREO_MODE_ANAGLYPH,
   VIDEO_STEREO_MODE_INTERLACED,
   VIDEO_STEREO_MODE_LAST
};

enum video_screen_layout
{
   VIDEO_SCREEN_LAYOUT_VERTICAL = 0,
   VIDEO_SCREEN_LAYOUT_HORIZONTAL,
   VIDEO_SCREEN_LAYOUT_LAST
};

/* Headset input settings (video_openxr_controllers, video_openxr_laser,
 * video_openxr_laser_beam). */
enum video_openxr_controllers
{
   VIDEO_OPENXR_CONTROLLERS_COMBINED = 0,
   VIDEO_OPENXR_CONTROLLERS_SEPARATE,
   VIDEO_OPENXR_CONTROLLERS_LAST
};

enum video_openxr_laser
{
   VIDEO_OPENXR_LASER_AUTO = 0,
   VIDEO_OPENXR_LASER_ALWAYS,
   VIDEO_OPENXR_LASER_OFF,
   VIDEO_OPENXR_LASER_LAST
};

/* What a core is told the headset shows, cycled by the Headset View
 * hotkey and never saved: IMMERSIVE reports what the headset can do,
 * STEREOSCOPIC withholds RETRO_VIDEO_VIEWS_STATUS_HMD, FLAT also
 * RETRO_VIDEO_VIEWS_STATUS_STEREO. */
enum video_headset_view
{
   VIDEO_HEADSET_VIEW_IMMERSIVE = 0,
   VIDEO_HEADSET_VIEW_STEREOSCOPIC,
   VIDEO_HEADSET_VIEW_FLAT,
   VIDEO_HEADSET_VIEW_LAST
};

enum video_openxr_laser_beam
{
   VIDEO_OPENXR_LASER_BEAM_OFF = 0,
   VIDEO_OPENXR_LASER_BEAM_EDIT,
   VIDEO_OPENXR_LASER_BEAM_ALWAYS,
   VIDEO_OPENXR_LASER_BEAM_LAST
};

/* Snap to Angles: the step edit mode turns and tilts a held item in
 * (video_openxr_edit_snap_angles). */
enum video_openxr_edit_snap_angles
{
   VIDEO_OPENXR_EDIT_SNAP_ANGLES_OFF = 0,
   VIDEO_OPENXR_EDIT_SNAP_ANGLES_15,
   VIDEO_OPENXR_EDIT_SNAP_ANGLES_30,
   VIDEO_OPENXR_EDIT_SNAP_ANGLES_45,
   VIDEO_OPENXR_EDIT_SNAP_ANGLES_90,
   VIDEO_OPENXR_EDIT_SNAP_ANGLES_LAST
};

/* Hinge Gap: the gap edit mode leaves between screens whose edges
 * meet (video_openxr_edit_hinge_gap). */
enum video_openxr_edit_hinge_gap
{
   VIDEO_OPENXR_EDIT_HINGE_GAP_0 = 0,
   VIDEO_OPENXR_EDIT_HINGE_GAP_1,
   VIDEO_OPENXR_EDIT_HINGE_GAP_2,
   VIDEO_OPENXR_EDIT_HINGE_GAP_3,
   VIDEO_OPENXR_EDIT_HINGE_GAP_5,
   VIDEO_OPENXR_EDIT_HINGE_GAP_10,
   VIDEO_OPENXR_EDIT_HINGE_GAP_LAST
};

/* Edit Smoothing: how much edit mode steadies the controllers' aim
 * (video_openxr_edit_smoothing). */
enum video_openxr_edit_smoothing
{
   VIDEO_OPENXR_EDIT_SMOOTHING_OFF = 0,
   VIDEO_OPENXR_EDIT_SMOOTHING_LOW,
   VIDEO_OPENXR_EDIT_SMOOTHING_MEDIUM,
   VIDEO_OPENXR_EDIT_SMOOTHING_HIGH,
   VIDEO_OPENXR_EDIT_SMOOTHING_LAST
};

/* video_openxr_placement: where the headset keeps the screens. */
enum video_openxr_placement
{
   VIDEO_OPENXR_PLACEMENT_FIXED = 0,
   VIDEO_OPENXR_PLACEMENT_FOLLOW,
   VIDEO_OPENXR_PLACEMENT_LAZY,
   VIDEO_OPENXR_PLACEMENT_LAST
};

/* video_openxr_refresh_rate: one of these, or a rate in Hz. */
enum video_openxr_refresh
{
   VIDEO_OPENXR_REFRESH_AUTO = 0,
   VIDEO_OPENXR_REFRESH_HEADSET
};

/* The most refresh rates read from a headset's list. */
#define VIDEO_HEADSET_MAX_RATES 16

/* An origin in VIDEO_POS_PACK's layout and a size in
 * VIDEO_SCALE_PACK's, as video_viewport_t keeps them. */
typedef struct video_views_rect
{
   unsigned pos;
   unsigned dims;
} video_views_rect_t;

/* A validated copy of a core's view map. */
typedef struct video_views_map
{
   struct retro_video_view views[RETRO_VIDEO_VIEWS_MAX];
   unsigned num_views;
   unsigned num_screens;
} video_views_map_t;

typedef struct video_screen_xr
{
   float x;
   float y;
   float z;
   float yaw;
   float pitch;
   float roll;
   float scale;
   bool visible;
} video_screen_xr_t;

void video_views_xr_reset(video_screen_xr_t *screens, unsigned count);
void video_views_xr_normalize(video_screen_xr_t *screen);

bool video_views_validate(const struct retro_video_view *views,
      unsigned num_views, video_views_map_t *out);

bool video_views_fit_frame(const video_views_map_t *map,
      unsigned frame_dims);

/* Rescale every rectangle for a CPU filter's output size. Widths scale
 * alone so the two eyes of a screen stay the same size. A view keeps
 * its display aspect ratio. */
void video_views_scale(video_views_map_t *map,
      unsigned in_dims, unsigned out_dims);

/* A view's display aspect ratio: its own, or width / height. */
double video_views_aspect(const struct retro_video_view *v);

/* Index of the view showing @screen to @eye: its EYE_NONE view if it has
 * one, else the matching eye, and for EYE_NONE the left. -1 if none. */
int video_views_find(const video_views_map_t *map,
      unsigned screen, unsigned eye);

/* A tracked map's eyes: true when the map holds exactly one LEFT and
 * one RIGHT view, their indices in eyes[0] and eyes[1]; else -1. */
bool video_views_eye_pair(const video_views_map_t *map, int eyes[2]);

/* The views of screens [@first, @first + @count) of @map as a map of
 * their own in @out, their screens counted from 0 and in @map's order;
 * @index[i] is @out's view i's index in @map. For a driver that shows
 * some screens on one display and the rest on another. False, with
 * @out emptied, when none of those screens has a view. */
bool video_views_select_screens(const video_views_map_t *map,
      unsigned first, unsigned count, video_views_map_t *out,
      unsigned *index);

/* The map for one frame: a copy scaled from the core's frame size to the
 * size the driver receives. False, with out emptied, when there is no map
 * or it does not fit the frame. */
bool video_views_snapshot(const video_views_map_t *map,
      unsigned frame_dims, unsigned out_dims,
      video_views_map_t *out);

#define VIDEO_VIEWS_MAX_PLACEMENTS (RETRO_VIDEO_VIEWS_MAX * 2)

/* One view drawn into one eye's area. */
typedef struct video_views_placement
{
   video_views_rect_t dst;   /* canvas pixels, top-left origin */
   unsigned view;            /* index into the map's views */
   unsigned area;            /* 0: left eye or the only area, 1: right eye */
} video_views_placement_t;

typedef struct video_views_layout
{
   video_views_placement_t placements[VIDEO_VIEWS_MAX_PLACEMENTS];
   video_views_rect_t areas[2];
   unsigned num_placements;
   unsigned num_areas;
   /* What the placements are drawn into: the window, or for anaglyph and
    * interlaced an offscreen image holding both eyes side by side, which
    * a final pass blends into the window. */
   unsigned canvas_dims;
   /* The size the menu and widgets lay out at. */
   unsigned ui_dims;
   /* The params' stereo mode, so drivers pick the blend without reading
    * settings. */
   unsigned stereo_mode;
   bool offscreen;
   /* The UI is drawn once and placed in each eye's area. */
   bool ui_per_eye;
} video_views_layout_t;

typedef struct video_views_layout_params
{
   const video_views_map_t *map;
   /* One included screen only. custom_vp is the custom viewport in window
    * pixels, or NULL; single_aspect is the aspect ratio setting's value,
    * or 0 for the view's own; stretch fills the area instead. */
   const video_views_rect_t *custom_vp;
   float single_aspect;
   bool stretch;
   unsigned dims;
   unsigned stereo_mode;
   unsigned screen_layout;
   unsigned exclude_screens;
   unsigned rotation;
   bool swap_eyes;
   bool scale_integer;
} video_views_layout_params_t;

void video_views_layout(const video_views_layout_params_t *params,
      video_views_layout_t *out);

/* The size the menu and widgets lay out at: one eye's UI size when
 * @layout draws the UI per eye, else @dims. NULL is a frame drawn
 * whole. */
unsigned video_views_ui_dims(const video_views_layout_t *layout,
      unsigned dims);

/* The part of @r inside a target of size @dims, in @out; false when
 * nothing is. */
bool video_views_clip(const video_views_rect_t *r,
      unsigned dims, video_views_rect_t *out);

/* The part of placement @p drawn into a target of size @dims, in @out:
 * what is inside its area and the target. False when nothing is. A
 * custom viewport can be larger than the window, and under threaded
 * video a frame can carry placements made for the window's previous
 * size. */
bool video_views_visible(const video_views_layout_t *layout,
      const video_views_placement_t *p, unsigned dims,
      video_views_rect_t *out);

/* The part of view @v that @cut shows, @cut being part of the rectangle
 * @dst the whole view is scaled to: for a blitter that scales a source
 * rectangle into a destination one, as video_views_visible cut it. At
 * least one pixel each way. */
void video_views_cut_source(const struct retro_video_view *v,
      const video_views_rect_t *dst, const video_views_rect_t *cut,
      video_views_rect_t *out);

/* The rectangle of @view's first placement drawn into a target of
 * size @dims, which its shader passes are sized for; all_areas false
 * draws the first area only. False when no placement of the view
 * shows. */
bool video_views_first_drawn(const video_views_layout_t *layout,
      unsigned view, bool all_areas, unsigned dims,
      video_views_rect_t *out);

/* Window point to packed-frame pointer coordinates, through @layout,
 * VIDEO_POS_PACK'd into @res_pos. @map and @frame_dims are the core's
 * own. A point on any copy of a screen lands in that screen's EYE_NONE
 * or left-eye rectangle. */
bool video_views_map_point(const video_views_layout_t *layout,
      const video_views_map_t *map, unsigned frame_dims,
      int x, int y, bool report_oob, uint32_t *res_pos);

/* Window point to the coordinates the menu and widgets lay out in,
 * through @layout. A per-eye UI: through the area the point is in, or
 * the nearer one, so a point outside both stays outside the UI. NULL,
 * or a UI drawn once, leaves the point as it is. */
void video_views_ui_point(const video_views_layout_t *layout,
      int x, int y, int *res_x, int *res_y);

/* Where touches on a display of a console's own land in the core's
 * packed frame: the views it shows, written by the video driver and read
 * by the input driver without a lock, behind a sequence count that is
 * odd while they change. Zeroed, it holds none. */
typedef struct video_views_touch
{
   video_views_rect_t dst[RETRO_VIDEO_VIEWS_MAX];
   struct retro_video_view src[RETRO_VIDEO_VIEWS_MAX];
   unsigned count;
   unsigned dims;
   unsigned frame_dims;
   retro_atomic_int_t seq;
} video_views_touch_t;

/* @layout's placements, of @map's views through @index (the selection's
 * to the map's), on a display of size @dims; @frame_dims the packed
 * frame's. A NULL @layout holds none. */
void video_views_touch_publish(video_views_touch_t *t,
      const video_views_map_t *map, const video_views_layout_t *layout,
      const unsigned *index, unsigned dims, unsigned frame_dims);

/* A touch, -0x7fff to 0x7fff across the display, to the core's pointer
 * in its packed frame, -0x8000 off the views. False, with @res_x and
 * @res_y as they were, when the display shows no views. */
bool video_views_touch_point(video_views_touch_t *t, int x, int y,
      int16_t *res_x, int16_t *res_y);

/* A canvas's two halves, each of size @dims, into @out for the CPU
 * drivers, XRGB8888 with strides in pixels: anaglyph by Dubois'
 * least-squares red/cyan matrices, or interlaced, even rows from the
 * left half and odd from the right. */
void video_views_blend_xrgb8888(const uint32_t *canvas,
      size_t canvas_stride, uint32_t *out, size_t out_stride,
      unsigned dims, unsigned stereo_mode);

/* ---- The compositor: what drivers that draw views share ----
 * A driver keeps the GPU objects; the decisions are written once, in
 * gfx/video_views_draw.h, which each driver instantiates with its own
 * types and calls - no function pointers, nothing a driver did by hand
 * costs more. */

/* Per-view shader chains, the driver's, as video_views_draw.h's
 * pick_chains takes them: one set for the preset, one for the stock
 * shader. count is the views the driver last made room for. */
typedef struct video_views_chains
{
   void *preset[RETRO_VIDEO_VIEWS_MAX];
   void *stock[RETRO_VIDEO_VIEWS_MAX];
   unsigned count;
   bool stock_failed;
} video_views_chains_t;

/* A render target the views draw into: the size it was made at, kept
 * when making it failed so it isn't retried every frame. */
typedef struct video_views_target
{
   unsigned dims;
   bool made;
} video_views_target_t;

/* The dims the menu and widgets lay out at and the fonts size from,
 * while the UI layer is drawn. */
typedef struct video_views_ui_dims
{
   unsigned dims;
   unsigned video_dims;
} video_views_ui_dims_t;

/* The UI drawn once and placed in each eye's area: a map with a layout
 * that asks for it, on a driver that draws views (@allowed). */
static INLINE bool video_views_ui_per_eye(const video_views_map_t *map,
      const video_views_layout_t *layout, bool allowed)
{
   return map->num_views && layout->ui_per_eye && allowed;
}

/* The UI layer is drawn when there is UI to draw. A macro, so what is
 * asked last is asked only when nothing before it answered. */
#define VIDEO_VIEWS_UI_WANTED(layer, menu, msg, statistics, widgets) \
   ((layer) && ((menu) || ((msg) && *(msg)) || (statistics) || (widgets)))

/* @dims and @video_dims moved to the UI layer's size, and back. */
static INLINE void video_views_ui_enter(video_views_ui_dims_t *saved,
      unsigned *dims, unsigned *video_dims, unsigned ui_dims)
{
   saved->dims       = *dims;
   saved->video_dims = *video_dims;
   *dims             = ui_dims;
   *video_dims       = ui_dims;
}

static INLINE void video_views_ui_leave(const video_views_ui_dims_t *saved,
      unsigned *dims, unsigned *video_dims)
{
   *dims       = saved->dims;
   *video_dims = saved->video_dims;
}

RETRO_END_DECLS

#endif
