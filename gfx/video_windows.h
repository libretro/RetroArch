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

#ifndef __VIDEO_WINDOWS_H
#define __VIDEO_WINDOWS_H

#include "video_views.h"

RETRO_BEGIN_DECLS

enum video_window_mode
{
   VIDEO_WINDOW_ATTACHED = 0,
   VIDEO_WINDOW_SEPARATE,
   VIDEO_WINDOW_HIDDEN
};

typedef struct video_window_settings
{
   unsigned pos;
   unsigned dims;
   unsigned monitor;
   unsigned mode;
   bool fullscreen;
} video_window_settings_t;

enum video_window_native_kind
{
   VIDEO_WINDOW_NATIVE_X11 = 1,
   VIDEO_WINDOW_NATIVE_WAYLAND,
   VIDEO_WINDOW_NATIVE_WIN32,
   VIDEO_WINDOW_NATIVE_SDL2,
   VIDEO_WINDOW_NATIVE_SDL3
};

enum video_window_event_kind
{
   VIDEO_WINDOW_EVENT_CLOSE = 1,
   VIDEO_WINDOW_EVENT_GEOMETRY,
   VIDEO_WINDOW_EVENT_FOCUS_GAIN,
   VIDEO_WINDOW_EVENT_FOCUS_LOSS,
   VIDEO_WINDOW_EVENT_SUSPEND,
   VIDEO_WINDOW_EVENT_RESUME
};

typedef struct video_window_request
{
   unsigned screen;
   unsigned generation;
   video_window_settings_t settings;
} video_window_request_t;

typedef struct video_window_native
{
   void *state;
   void *display;
   void *surface;
   unsigned kind;
   unsigned generation;
   unsigned dims;
   bool configured;
   bool focused;
   bool presentable;
} video_window_native_t;

typedef struct video_window_event
{
   unsigned screen;
   unsigned generation;
   unsigned kind;
   unsigned pos;
   unsigned dims;
} video_window_event_t;

/* A context's optional operations on secondary windows, on the thread
 * that runs the context. The record passed to create is zeroed, and
 * stays in place until destroy, which comes only after the renderer has
 * retired the window's GPU resources. present_gl follows bind_gl of the
 * same window: EGL needs the surface current, and the contract is the
 * stricter API's. present_gl false means not presentable or closed, so
 * the frame skips the window; a failure arrives as a close event.
 * bind_gl(NULL) makes the main window's drawable current again, with its
 * own swap interval; secondaries swap at interval 0. Events poll() has
 * no room for wait for the next call. */
typedef struct gfx_ctx_window_driver
{
   bool (*create)(void *owner, const video_window_request_t *request,
         video_window_native_t *window);
   void (*update)(void *owner, video_window_native_t *window,
         const video_window_request_t *request);
   unsigned (*poll)(void *owner, video_window_event_t *events,
         unsigned capacity);
   bool (*bind_gl)(void *owner, video_window_native_t *window);
   bool (*present_gl)(void *owner, video_window_native_t *window);
   void (*destroy)(void *owner, video_window_native_t *window);
} gfx_ctx_window_driver_t;

#define VIDEO_WINDOW_EVENT_BIT(kind) (1u << VIDEO_WINDOW_EVENT_##kind)

/* What a context's native layer keeps of each window it makes, first
 * in its own window->state, on the thread that runs the context */
typedef struct video_window_common
{
   void *owner;
   /* The caller's record, which stays put until destroy */
   video_window_native_t *native;
   video_window_request_t request;
   /* The position GEOMETRY reports; 0 where the compositor places */
   unsigned pos;
   /* What waits for poll(), one VIDEO_WINDOW_EVENT_BIT per kind */
   unsigned pending;
   bool closed;
} video_window_common_t;

/* A native layer's windows by screen, NULL for none */
typedef video_window_common_t *(*video_window_at_t)(unsigned screen);

/* Closed by the window manager or by a failure: CLOSE, and FOCUS_LOSS
 * where it had the focus; a gain or resume still waiting goes. */
void video_window_mark_closed(video_window_common_t *window);

/* poll() over 'owner''s windows: each one's transitions in the order
 * focus loss, close, geometry, suspend, resume, focus gain, and what
 * has no room waits for the next call, so close and focus loss always
 * get through. */
unsigned video_windows_drain(void *owner, video_window_at_t at,
      video_window_event_t *events, unsigned capacity);

/* Hands each window 'owner' still has to 'destroy'. */
void video_windows_sweep(void *owner, video_window_at_t at,
      void (*destroy)(void *owner, video_window_native_t *window));

typedef struct video_window_rect
{
   int x;
   int y;
   int w;
   int h;
} video_window_rect_t;

/* The area two rects share, 0 for none */
unsigned video_window_rect_overlap(const video_window_rect_t *a,
      const video_window_rect_t *b);

/* Saved bounds on 'area', a monitor or its work area: bounds partly on
 * it are moved onto it, automatic ones and those not on it at all are
 * centred on it, and the size is clamped to it. */
void video_window_place_rect(video_window_rect_t *rect,
      const video_window_rect_t *area, bool automatic);

typedef struct video_windows_plan
{
   unsigned main_mask;
   unsigned separate_mask;
   unsigned hidden_mask;
} video_windows_plan_t;

/* What a driver needs to make, place and lay out the screens' own
 * windows for one frame, without reading settings */
typedef struct video_windows_frame
{
   /* The core's own map and frame size, as input maps back into */
   video_views_map_t core;
   video_window_settings_t settings[RETRO_VIDEO_VIEWS_MAX];
   video_windows_plan_t plan;
   unsigned core_dims;
   /* Counts video_driver_restore_windows() */
   unsigned restore;
   /* The main window's layout parameters, for a window's own */
   float single_aspect;
   unsigned stereo_mode;
   unsigned screen_layout;
   unsigned rotation;
   bool stretch;
   bool scale_integer;
   bool swap_eyes;
} video_windows_frame_t;

/* Any thread; zero means the lifetime counter is exhausted. */
unsigned video_driver_window_generation(void);

/* @attached stays in the main window whatever its mode, and screen 0
 * takes its mode; one that @map does not have counts as 0. */
void video_windows_plan(const video_views_map_t *map,
      const video_window_settings_t *settings, bool enabled,
      bool supported, unsigned forced_attached, unsigned attached,
      video_windows_plan_t *out);

/* Screen @screen's own window at @dims, from a frame's snapshot: its
 * map with every other screen left out. */
void video_driver_window_layout(const video_windows_frame_t *frame,
      unsigned screen, unsigned dims, video_views_layout_t *out);

/* Main thread: screens a close attached go back to their windows, and
 * the frame's restore count asks drivers to place them again. */
void video_driver_restore_windows(void);

/* Window @window's layout as drawn, from the thread that runs the
 * context, one at a time per window; latest wins. @window_dims are
 * client pixels, which input points arrive in. NULL clears it. */
void video_driver_publish_window_layout(unsigned window,
      unsigned generation, const video_views_layout_t *layout,
      const video_views_map_t *core_map, unsigned core_dims,
      unsigned window_dims);

/* Main thread: a point in window @window to libretro coordinates in
 * the core's frame. Window 0 is the main window, through its viewport;
 * another is a screen's own, through its last published layout, and
 * false unless that is of @generation. */
bool video_driver_translate_window_point(unsigned window,
      unsigned generation, int x, int y, bool report_oob,
      uint32_t *result);

RETRO_END_DECLS

#endif
