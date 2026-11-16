/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2019 - Daniel De Matteis
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

#ifndef SDL2_COMMON_H__
#define SDL2_COMMON_H__

#include <stdint.h>
#include <boolean.h>

#include "SDL.h"

#include "../video_defines.h"
#include "../video_windows.h"
#include "../font_driver.h"
#include "../../retroarch.h"

/* Views need SDL_SetTextureScaleMode and a custom blend mode */
#if SDL_VERSION_ATLEAST(2, 0, 18)
#define SDL2_VIEWS
#include <retro_atomic.h>
#include "../video_views.h"
#endif

enum sd2l_flags
{
   SDL2_FLAG_QUITTING      = (1 << 0),
   SDL2_FLAG_SHOULD_RESIZE = (1 << 1)
};

typedef struct sdl2_tex
{
   SDL_Texture *tex;

   unsigned dims;
   size_t pitch;
   bool active;
   bool rgb32;
} sdl2_tex_t;

#ifdef HAVE_OVERLAY
/* On-screen input overlay entry. Defined at file scope (not inside
 * _sdl2_video) so the tag resolves to the same type in both C and C++;
 * see the note at the 'overlays' member below. */
struct sdl2_overlay
{
   SDL_Texture *tex;
   unsigned  tex_w;
   unsigned  tex_h;
   float     tex_coords[4];
   float     vert_coords[4];
   float     alpha_mod;
   bool      fullscreen;
};
#endif

typedef struct _sdl2_video
{
   double rotation;

   struct video_viewport vp;
   video_info_t video;

   sdl2_tex_t frame; /* ptr alignment */
   sdl2_tex_t menu;  /* ptr alignment */
   sdl2_tex_t font;  /* ptr alignment */

   /* The frame texture while it is locked and lent to the core.
    * scratch holds a window of it pushed at another size or pitch. */
   struct
   {
      void    *pixels;
      uint8_t *scratch;
      size_t   scratch_size;
      size_t   pitch;
      unsigned dims;
      unsigned declined_dims;
   } sw_lend;

   SDL_Window *window;
   SDL_Renderer *renderer;

   void *font_data;
   const font_renderer_driver_t *font_driver;
   /* The OSD font's atlas as ARGB, its texture's source */
   uint32_t *font_staging;

   uint8_t font_r;
   uint8_t font_g;
   uint8_t font_b;

   uint8_t flags;

#ifdef HAVE_OVERLAY
   /* On-screen input overlay state.  Each entry holds an SDL_Texture
    * uploaded as ARGB8888 (matching the supports_rgba=false byte
    * layout RetroArch hands us in load()) and rendered via
    * SDL_RenderCopy with per-texture alpha modulation each frame.
    *
    * vert_coords / tex_coords are 4-float (x, y, w, h) tuples in
    * 0..1 normalised space.  vertex_geom flips y to (1.0f - y) and
    * negates h, matching d3d8 / gl / d3d9_common; vertex_geom
    * comments document the rationale - we undo the flip at render
    * time in sdl2_overlays_render.
    *
    * Note: struct sdl2_overlay is defined at file scope (above this
    * typedef) rather than inline here. Defining a struct inside
    * another struct's body scopes the tag to the enclosing type in
    * C++ (sdl2_video::sdl2_overlay), so under CXX_BUILD a file-scope
    * 'struct sdl2_overlay *' in sdl2_gfx.c would refer to a different,
    * incomplete type. A top-level definition keeps a single shared
    * type in both C and C++. */
   struct sdl2_overlay *overlays;
   unsigned overlays_size;
   bool overlays_enabled;
   /* overlays[].tex are the overlay pack's (load_textures) */
   bool overlays_borrowed;
#endif

   /* What a display draw hands SDL_RenderGeometry. Grown as needed and
    * kept, rather than alloca'd per draw: the count comes from the
    * caller, so the stack cost was whatever geometry someone asked to
    * draw - a nine-slice is 52 vertices and a pipeline is thousands. */
   void   *display_verts;
   size_t  display_verts_cap;
   int    *display_indices;
   size_t  display_indices_cap;

#ifdef SDL2_VIEWS
   struct
   {
      video_views_chains_t  chains;
      video_views_target_t  canvas_target;
      video_views_target_t  ui_target;
      video_views_ui_dims_t saved;
      /* What the last frame drew, kept for the viewport readback to
       * draw again: its map, layout, chains and the window's size */
      video_views_map_t     map;
      video_views_layout_t  layout;
      void                 *drawn[RETRO_VIDEO_VIEWS_MAX];
      unsigned              dims;
      /* Each view of the frame, written straight into its own texture */
      SDL_Texture          *tex[RETRO_VIDEO_VIEWS_MAX];
      unsigned              tex_dims[RETRO_VIDEO_VIEWS_MAX];
      bool                  tex_rgb32[RETRO_VIDEO_VIEWS_MAX];
      SDL_Texture          *canvas;
      SDL_Texture          *ui;
      SDL_BlendMode         premultiplied;
      unsigned              frame_dims;
      /* The game viewport's size while the UI layer is drawn */
      unsigned              vp_dims;
      /* For get_flags on the main thread: whether views may be drawn,
       * and whether the last frame with a map drew it whole */
      retro_atomic_int_t    allowed;
      retro_atomic_int_t    fallback;
      bool                  tried;
      /* Render targets, for the canvas, and the premultiplied blend the
       * UI layer is placed with */
      bool                  targets;
      bool                  layer;
      bool                  active;
      bool                  blend;
      bool                  direct_seen;
   } views;
#endif
} sdl2_video_t;

/* Keeps the pointer in the SDL2 video driver's window, or lets it go.
 * Nothing happens when that is not the video driver in use. */
void sdl2_video_grab_window(bool state);

void sdl2_set_handles(void *data, enum rarch_display_type 
      display_type);

/* Gives 'win' RetroArch's icon */
void sdl2_window_set_icon(SDL_Window *win);

#ifdef HAVE_VIDEO_WINDOWS
/* A secondary screen's OpenGL window, window->state from create to
 * destroy, on the thread that runs the context. The record's surface
 * is the SDL_Window, its display NULL; dims are its size. Only on
 * SDL's x11, wayland and windows backends. SDL quits only when its
 * last window closes, so the main window's own close request quits as
 * that would (SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE), and a secondary's
 * close is a CLOSE event alone. */
typedef struct sdl2_video_window
{
   video_window_common_t common;
   /* When the check of judge_pos and judge_dims ran */
   retro_time_t judge_at;
   /* When an update stops waiting for SDL: for a MAXIMIZED 'waiting',
    * then for the frame of a 'framed' one or the display of 'via' */
   retro_time_t wait_by;
   SDL_Window *win;
   /* The context's main window, for placement */
   SDL_Window *primary;
   /* Where it stood before it last went fullscreen */
   SDL_Rect windowed;
   /* On its way to a fullscreen request's display as a window: the
    * display it was moved to, which SDL is to have it on next; -1 when
    * not. 'windowed' stays. */
   int via;
   Uint32 id;
   /* The thread that made it, whose it is on Windows */
   SDL_threadID thread;
   /* What SDL is still to finish of what it was asked, which an
    * update waits for: MAXIMIZED until wait_by, FULLSCREEN as long as
    * SDL has it so, as for a minimised window */
   Uint32 waiting;
   /* SDL's flags at the last check */
   Uint32 flags;
   /* Bounds the next pump judges, where 'judge' */
   unsigned judge_pos;
   unsigned judge_dims;
   bool fullscreen;
   bool windowed_set;
   /* It left fullscreen on its way to a fullscreen request's display:
    * 'windowed' stays, and SDL puts it back where it stood, which is
    * no display to wait for */
   bool left_fullscreen;
   /* It had a frame going fullscreen, or may have one: made fullscreen,
    * or fullscreen again before the frame came back. Placing it as a
    * window waits for the frame, until wait_by */
   bool framed;
   /* An update that waits for the window */
   bool apply;
   /* Whether they were normal at that check */
   bool judge_normal;
   bool judge;
} sdl2_video_window_t;

/* Context thread, at init once SDL video is up: what
 * sdl2_video_windows_supported() answers from then on */
void sdl2_video_windows_probe(void);
/* Any thread: whether SDL's video driver can make them, the context's
 * window table's usable() */
bool sdl2_video_windows_supported(void);
bool sdl2_video_window_create(void *owner, SDL_Window *primary,
      const video_window_request_t *request, video_window_native_t *window);
void sdl2_video_window_update(void *owner, video_window_native_t *window,
      const video_window_request_t *request);
unsigned sdl2_video_window_poll(void *owner, video_window_event_t *events,
      unsigned capacity);
void sdl2_video_window_destroy(void *owner, video_window_native_t *window);
/* While kept for the next driver a window holds the video subsystem,
 * whose last quit would take it down */
void sdl2_video_window_keep(void *owner, video_window_native_t *window);
bool sdl2_video_window_take(void *owner, unsigned screen,
      video_window_native_t *window, unsigned *display);
/* Kept windows nobody took go; on the thread of the context */
void sdl2_video_windows_release_kept(void);
/* Any thread: whether the window, or the window SDL gives this ID,
 * is a secondary screen's; NULL and 0 are none */
bool sdl2_video_window_is_screen(const SDL_Window *win, Uint32 id);
/* Hands each window 'owner' still has to 'destroy'. */
void sdl2_video_windows_free(void *owner,
      void (*destroy)(void *owner, video_window_native_t *window));
/* True for a secondary's window event, which then goes nowhere else.
 * The main window's close request sets 'quit', as SDL would for its
 * last window. */
bool sdl2_video_window_event(const SDL_Event *event, bool *quit);
/* After each SDL_PumpEvents(), before its window events are taken */
void sdl2_video_windows_judge(void);
/* The window flags of the screens' open windows, together */
Uint32 sdl2_video_windows_flags(void);
/* The displays for the screens' Display setting, on the context's
 * thread: 'changed' after a display event (@removed: one went),
 * 'moved' after a main window move. Published once the main window is
 * made (@init), then after each pump, and only when the list changed. */
void sdl2_video_monitors_changed(bool removed);
void sdl2_video_monitors_moved(void);
void sdl2_video_monitors_publish(SDL_Window *primary, bool init);
#endif

#endif
