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

#include <stdint.h>

#include <boolean.h>
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

/* Headset input settings (video_openxr_controllers, video_openxr_laser). */
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
   /* One-screen maps only. custom_vp is the custom viewport in window
    * pixels, or NULL; single_aspect is the aspect ratio setting's value,
    * or 0 for the view's own; stretch fills the area instead. */
   const video_views_rect_t *custom_vp;
   float single_aspect;
   bool stretch;
   unsigned dims;
   unsigned stereo_mode;
   unsigned screen_layout;
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

RETRO_END_DECLS

#endif
