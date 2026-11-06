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

/* video_screen%u_display's value for a screen that is not shown */
#define VIDEO_WINDOW_DISPLAY_OFF "off"

/* video_screen%u_display's value for a screen shown in the main window,
 * on the desktop */
#define VIDEO_WINDOW_DISPLAY_MAIN "main"

/* video_menu_follows below this: the screen the menu goes with, 0 for
 * Game Screen 1; this value: a display of the menu's own */
#define VIDEO_MENU_FOLLOWS_SEPARATE RETRO_VIDEO_VIEWS_MAX

enum video_notifications_window
{
   VIDEO_NOTIFICATIONS_WITH_GAME = 0,
   VIDEO_NOTIFICATIONS_WITH_MENU
};

typedef struct video_window_settings
{
   unsigned pos;
   unsigned dims;
   unsigned monitor;
   unsigned mode;
   /* Counts edits of the screen's window rows; not saved */
   unsigned edit;
   bool fullscreen;
   /* @pos was set, by the user or a window's move; (0,0) is a place */
   bool pos_set;
} video_window_settings_t;

enum video_window_native_kind
{
   VIDEO_WINDOW_NATIVE_X11 = 1,
   VIDEO_WINDOW_NATIVE_WAYLAND,
   VIDEO_WINDOW_NATIVE_WIN32,
   VIDEO_WINDOW_NATIVE_SDL2,
   VIDEO_WINDOW_NATIVE_SDL3,
   VIDEO_WINDOW_NATIVE_ANDROID
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
   /* The screens it shows, a bit each; 0 for screen @screen alone */
   unsigned screens;
   unsigned generation;
   video_window_settings_t settings;
   /* The display it goes on: an ordinal of the published list, 0 where
    * the native layer places it */
   unsigned display;
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
   /* GEOMETRY: the bounds the user left it at, in create()'s units: a
    * size, 0 for none to save, and @save_pos where @save_moved */
   unsigned save_pos;
   unsigned save_dims;
   bool save_moved;
   /* CLOSE: from its title bar, not a failure */
   bool user;
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
   /* Optional. keep is destroy at the renderer's teardown, with the
    * window left up as last drawn. take, at the next renderer's init,
    * adopts screen @screen's kept window into @window, and the display
    * it is on into *@display. One nobody takes goes once the next
    * driver is up. */
   void (*keep)(void *owner, video_window_native_t *window);
   bool (*take)(void *owner, unsigned screen,
         video_window_native_t *window, unsigned *display);
   /* Optional, any thread: false where no window can be made (SDL
    * under cocoa or offscreen). NULL is true. */
   bool (*usable)(void);
} gfx_ctx_window_driver_t;

#define VIDEO_WINDOW_EVENT_BIT(kind) (1u << VIDEO_WINDOW_EVENT_##kind)

/* The screen a window's title names: its first, else its id's */
unsigned video_window_title_screen(const video_window_request_t *request);

/* What a context's native layer keeps of each window it makes, first
 * in its own window->state, on the thread that runs the context */
typedef struct video_window_common
{
   /* Moves before then are the window manager's or the OS's; 0 until
    * it is first placed */
   retro_time_t settle;
   void *owner;
   /* The caller's record, which stays put until destroy */
   video_window_native_t *native;
   video_window_request_t request;
   /* The position GEOMETRY reports; 0 where the compositor places */
   unsigned pos;
   /* What waits for poll(), one VIDEO_WINDOW_EVENT_BIT per kind */
   unsigned pending;
   /* What the next GEOMETRY saves (video_window_moved()) */
   unsigned save_pos;
   unsigned save_dims;
   /* Where it was last put, left by the window manager, or saved */
   unsigned asked_pos;
   unsigned asked_dims;
   /* Where RetroArch last put it, until a save; 0 dims for none */
   unsigned placed_pos;
   unsigned placed_dims;
   bool closed;
   bool user_closed;
   bool save_moved;
} video_window_common_t;

/* A native layer's windows by screen, NULL for none */
typedef video_window_common_t *(*video_window_at_t)(unsigned screen);

/* Closed by a failure or by another client: CLOSE, and FOCUS_LOSS
 * where it had the focus; a gain or resume still waiting goes. */
void video_window_mark_closed(video_window_common_t *window);

/* Closed from its title bar: CLOSE with user set */
void video_window_mark_closed_by_user(video_window_common_t *window);

#define VIDEO_WINDOW_SETTLE_USEC 500000

/* The native layer put it at @pos and @dims (create()'s units), or is
 * about to: a move there is not the user's, nor any move until it
 * settles */
void video_window_placed(video_window_common_t *window, unsigned pos,
      unsigned dims, retro_time_t now);

/* Its monitors changed or it was mapped: moves for a while after @now
 * are not the user's */
void video_window_unsettled(video_window_common_t *window, retro_time_t now);

/* Now at @pos (known where @has_pos) and @dims, in create()'s units, as
 * of @now; @normal: not fullscreen, maximised or minimised. For a
 * settled normal window not where it was last put or saved, keeps the
 * bounds for GEOMETRY to save and writes them into its request, so
 * update() takes them as no change. True when there is something to
 * save. */
bool video_window_moved(video_window_common_t *window, unsigned pos,
      bool has_pos, unsigned dims, bool normal, retro_time_t now);

/* poll() over 'owner''s windows: each one's transitions in the order
 * focus loss, close, geometry, suspend, resume, focus gain, and what
 * has no room waits for the next call, so close and focus loss always
 * get through. */
unsigned video_windows_drain(void *owner, video_window_at_t at,
      video_window_event_t *events, unsigned capacity);

/* Hands each window 'owner' still has to 'destroy'. */
void video_windows_sweep(void *owner, video_window_at_t at,
      void (*destroy)(void *owner, video_window_native_t *window));

/* Whether a context with table @windows can make the screens' windows:
 * only then do renderers advertise them. Any thread. */
bool video_windows_usable(const gfx_ctx_window_driver_t *windows);

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

/* @rect lies on none of @count @areas; false with no areas */
bool video_window_rect_lost(const video_window_rect_t *rect,
      const video_window_rect_t *areas, unsigned count);

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

/* A display a platform lists for the screens' windows. connection
 * takes Android's DeviceProductInfo values. */
enum video_window_connection
{
   VIDEO_WINDOW_CONNECTION_UNKNOWN = 0,
   VIDEO_WINDOW_CONNECTION_BUILT_IN,
   VIDEO_WINDOW_CONNECTION_DIRECT,
   VIDEO_WINDOW_CONNECTION_TRANSITIVE
};

/* What a platform says a display is, where it can tell */
enum video_window_display_type
{
   VIDEO_WINDOW_TYPE_UNKNOWN = 0,
   VIDEO_WINDOW_TYPE_INTERNAL,
   VIDEO_WINDOW_TYPE_EXTERNAL
};

#define VIDEO_WINDOW_DISPLAYS_MAX      8
#define VIDEO_WINDOW_DISPLAY_NAME_SIZE 64
#define VIDEO_WINDOW_DISPLAY_MODEL_SIZE 32

typedef struct video_window_display
{
   /* 1-based place in the platform's list */
   unsigned ordinal;
   /* The platform's own id for it: Android's display id */
   int id;
   unsigned connection;
   /* The main window is on it */
   bool own;
   bool valid;
   bool on;
   bool is_private;
   /* A real panel or monitor: false for casts, and where unknown */
   bool physical;
   /* A Presentation failed on it since the list last changed */
   bool refused;
   /* The platform's default display: Android's DEFAULT_DISPLAY */
   bool is_default;
   /* A video_window_display_type */
   unsigned char type;
   /* A desktop monitor: a screen sent there gets a window of its own,
    * and nothing is placed (video_windows_resolve()) */
   bool monitor;
   char name[VIDEO_WINDOW_DISPLAY_NAME_SIZE];
   /* The product's name; empty where unknown */
   char model[VIDEO_WINDOW_DISPLAY_MODEL_SIZE];
   /* VIDEO_SCALE_PACK of its size in the platform's logical pixels,
    * 0 where unknown */
   unsigned dims;
} video_window_display_t;

/* The UI drawn over the screens, as placement sends it to a window */
enum video_window_ui_layer
{
   VIDEO_WINDOW_UI_MENU = 0,
   VIDEO_WINDOW_UI_NOTIFICATIONS,
   VIDEO_WINDOW_UI_LAYERS
};

#define VIDEO_WINDOW_UI_BIT(layer) (1u << (layer))

/* Of @layers, VIDEO_WINDOW_UI_BIT()s, those with something to draw: the
 * menu while open, the notifications as VIDEO_VIEWS_UI_WANTED() asks.
 * A macro, so @widgets is asked last. */
#define VIDEO_WINDOW_UI_DRAWN(layers, menu, msg, statistics, widgets) \
   (  ((((layers) & VIDEO_WINDOW_UI_BIT(VIDEO_WINDOW_UI_MENU)) && (menu)) \
         ? VIDEO_WINDOW_UI_BIT(VIDEO_WINDOW_UI_MENU) : 0u) \
    | (VIDEO_VIEWS_UI_WANTED((layers) \
            & VIDEO_WINDOW_UI_BIT(VIDEO_WINDOW_UI_NOTIFICATIONS), \
            0, msg, statistics, widgets) \
         ? VIDEO_WINDOW_UI_BIT(VIDEO_WINDOW_UI_NOTIFICATIONS) : 0u))

/* The window placed on a display of its own: the Presentation */
#define VIDEO_WINDOWS_PRESENTATION 1

/* Placement calls the display RetroArch moved onto keeps its
 * Presentation, so the main window has drawn under it */
#define VIDEO_WINDOWS_HOLD_FRAMES 3

/* How long a UI layer waits for its window to show */
#define VIDEO_WINDOWS_UI_WAIT_USEC 1000000

/* What placement reads, one frame's worth */
typedef struct video_windows_place_in
{
   const video_window_display_t *displays;
   unsigned count;
   /* Each screen's video_screen%u_display */
   const char (*screen_display)[VIDEO_WINDOW_DISPLAY_NAME_SIZE];
   /* video_menu_display */
   const char *menu_display;
   unsigned menu_follows;
   unsigned notifications;
   /* The running content's screens, 0 with none; a core with no views
    * map has 1, and mapless */
   unsigned num_screens;
   /* The display the Presentation's window has drawn on since it was
    * made, 0 for none (video_driver_window_shown()) */
   unsigned shown;
   /* The number of a move the platform could not start (a route's
    * move_seq), 0 for none */
   unsigned failed_move;
   retro_time_t now;
   bool mapless;
   bool menu_open;
   /* A staged content load is under way (content_switching) */
   bool switching;
} video_windows_place_in_t;

/* Where the screens, the menu and the notifications go, by display
 * ordinal */
typedef struct video_windows_route
{
   /* Each screen's display; 0 for an Off one or one past the content */
   unsigned target[RETRO_VIDEO_VIEWS_MAX];
   /* Each screen's edit count: the edits it was worked out with */
   unsigned edit[RETRO_VIDEO_VIEWS_MAX];
   /* The menu's display, open or not */
   unsigned menu;
   /* RetroArch's display, the list's own; 0 when nothing is routed */
   unsigned own;
   /* The Presentation's display, 0 for none */
   unsigned display;
   /* A bit a screen: those on the Presentation */
   unsigned screens;
   /* A bit a screen: those shown in the main window */
   unsigned main_screens;
   /* VIDEO_WINDOW_UI_BIT()s: the layers drawn in the Presentation's
    * window; the menu's counted as if open */
   unsigned ui;
   /* A display whose Presentation stays up with its last image, 0 for
    * none */
   unsigned keep;
   /* The display RetroArch is to move onto, this frame only; 0 for
    * none */
   unsigned move;
   /* That move's number, which a failure of it names */
   unsigned move_seq;
   /* A bit per listed ordinal (bit 0 = ordinal 1) */
   unsigned present;
   /* Screens whose Display names a listed monitor */
   unsigned named;
   /* The main window has nothing to show: the idle logo */
   bool logo;
   /* From video_windows_resolve(): @screens are those with a window of
    * their own, @target their monitors */
   bool monitors;
} video_windows_route_t;

#define VIDEO_WINDOWS_ORDINAL_BIT(o) \
   (((o) && (o) <= 32) ? 1u << ((o) - 1) : 0u)

/* How long a move may take before its display is given up on */
#define VIDEO_WINDOWS_MOVE_USEC 2000000

/* Placement from one frame to the next */
typedef struct video_windows_host
{
   /* A map change before this time is still the start's; 0 for none */
   retro_time_t start_ends;
   /* When the pending move was asked for */
   retro_time_t asked;
   /* The list's names and on/valid flags, hashed */
   unsigned names;
   /* The screens' and the menu's display settings, hashed */
   unsigned settings;
   /* A bit an ordinal, bit 0 for 1: refusals since the names last
    * changed */
   unsigned refused;
   /* A bit an ordinal: displays that could not take RetroArch, until
    * the names change */
   unsigned unable;
   /* The main window's display as last decided, 0 undecided */
   unsigned host;
   /* The display a move was asked onto, 0 for none */
   unsigned moving;
   /* The last move's number, 1 to INT_MAX: Android carries it as an
    * int */
   unsigned move_seq;
   /* RetroArch's display, whose Presentation must show before the
    * move to host is asked; 0 for none */
   unsigned make;
   /* The display whose Presentation is kept, and the calls left */
   unsigned keep;
   unsigned keep_frames;
   /* The content as last seen, and whether it was being replaced */
   unsigned num_screens;
   bool switching;
   bool mapless;
} video_windows_host_t;

/* Where a UI layer is drawn this frame */
typedef struct video_window_ui_target
{
   /* Window id: 0 the main window */
   unsigned window;
   /* VIDEO_SCALE_PACK: the size the app thread laid it out at */
   unsigned dims;
   /* Its window is still being made: drawn nowhere until it shows */
   bool wait;
} video_window_ui_target_t;

/* What a driver needs to make, place and lay out the screens' own
 * windows for one frame, without reading settings */
typedef struct video_windows_frame
{
   /* The core's own map and frame size, as input maps back into */
   video_views_map_t core;
   video_window_settings_t settings[RETRO_VIDEO_VIEWS_MAX];
   video_windows_plan_t plan;
   unsigned core_dims;
   /* Count video_driver_restore_windows(), and the retries
    * (video_driver_retry_windows() and each restore) */
   unsigned restore;
   unsigned retry;
   /* Counts the main thread's snapshots; a redraw's is the last drawn */
   unsigned serial;
   /* Where the platform puts the windows: a display's ordinal, 0 where
    * the native layer places each one itself */
   unsigned display;
   /* The screens each window shows, by window id; [0] is the main
    * window's and stays 0 */
   unsigned window_screens[RETRO_VIDEO_VIEWS_MAX];
   /* Each UI layer's window and size */
   video_window_ui_target_t ui[VIDEO_WINDOW_UI_LAYERS];
   /* By window id, VIDEO_WINDOW_UI_BIT()s: the layers it shows */
   unsigned window_ui[RETRO_VIDEO_VIEWS_MAX];
   /* A bit a window id: kept with its last image, not drawn */
   unsigned window_keep;
   /* With @monitors: the route's present */
   unsigned monitors_present;
   /* By window id: shows a screen whose Display names a listed monitor */
   unsigned window_named;
   /* The idle logo's texture, 0 for black only */
   uintptr_t logo_texture;
   /* The main window shows the idle logo */
   bool logo;
   /* Desktop: each screen's window from its Display setting
    * (video_windows_resolve()); a live window is made again for an
    * edit, or when its monitor goes or its named one comes back */
   bool monitors;
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

/* Without @route, screen 0 stays in the main window whatever its mode
 * and the others go by theirs; with one, a screen goes where @route
 * puts it. */
void video_windows_plan(const video_views_map_t *map,
      const video_window_settings_t *settings, bool enabled,
      bool supported, unsigned forced_attached,
      const video_windows_route_t *route, video_windows_plan_t *out);

/* Display @index (< VIDEO_WINDOW_DISPLAYS_MAX) as a screen's display
 * setting names it: its name, or "Monitor N" by its ordinal without
 * one, " 2" or " 3" added for the second or third listed with that
 * name. */
void video_windows_display_key(const video_window_display_t *displays,
      unsigned index, char *out, size_t size);

/* App thread, each frame windows are on: where the screens, the menu
 * and notifications go, and RetroArch's moves. Pure. */
void video_windows_place(video_windows_host_t *host,
      const video_windows_place_in_t *in, video_windows_route_t *out);

/* Desktop, a frame's worth: Game Screen N's own window and its monitor
 * by its display setting. Off: no window; Main Window, and Auto or a
 * monitor not listed for Game Screen 1: the main window; a monitor
 * listed: a window there; Auto or a monitor not listed past Game
 * Screen 1: a window on the first monitor RetroArch is not on, else on
 * its own, or where saved bounds put it (a size and a position set:
 * target 0, the platform picks). Pure. */
void video_windows_resolve(const video_window_display_t *displays,
      unsigned count,
      const char (*screen_display)[VIDEO_WINDOW_DISPLAY_NAME_SIZE],
      const video_window_settings_t *settings,
      video_windows_route_t *out);

/* A window id for Game Screen 1's own window, whose id 0 is the main
 * window's: the first past the content's screens, else one no screen
 * in @separate_mask has; 0 for none */
unsigned video_windows_spare_window(unsigned separate_mask,
      unsigned num_screens);

/* The Display values offer Main Window: on the desktop, for its own
 * monitors or before any are listed */
bool video_windows_offer_main(const video_window_display_t *displays,
      unsigned count);

/* Game Screen @screen gets a window of its own by display setting @key,
 * as video_windows_resolve() decides with @displays listed */
bool video_windows_display_own(const video_window_display_t *displays,
      unsigned count, const char *key, unsigned screen);

/* A host that decides afresh on its next call */
void video_windows_host_reset(video_windows_host_t *host);

/* Screen @screen's own window at @dims, from a frame's snapshot: its
 * map with every other screen left out. */
void video_driver_window_layout(const video_windows_frame_t *frame,
      unsigned screen, unsigned dims, video_views_layout_t *out);

/* App thread: closed and failed screens get their windows again, each
 * at its saved settings (a restore-windows action, a content change) */
void video_driver_restore_windows(void);

/* App thread: failed screens get another try (a new display list) */
void video_driver_retry_windows(void);

/* App thread: screen @screen's window rows changed; its window is made
 * again for them, after a close too */
void video_driver_edit_window(unsigned screen);

/* UI thread, the only publisher: the platform's displays, latest wins,
 * at most VIDEO_WINDOW_DISPLAYS_MAX kept. Once a list is published,
 * every frame places the screens and the menu through
 * video_windows_place(); count 0 stops it. */
void video_driver_publish_window_displays(
      const video_window_display_t *displays, unsigned count);

/* As above, but the list to place them by is still coming: until it
 * or another publish, the frame keeps the screens in the main window,
 * so their windows are made once, where that list puts them. */
void video_driver_publish_window_displays_pending(
      const video_window_display_t *displays, unsigned count);

/* App thread: the newest published list, at most @capacity entries;
 * returns how many. For the menu's picker. */
unsigned video_driver_get_window_displays(video_window_display_t *out,
      unsigned capacity);

/* A window showing @screens, from @frame's snapshot at @dims: the map
 * with every screen outside @screens left out. Any thread. */
void video_driver_window_layout_screens(const video_windows_frame_t *frame,
      unsigned screens, unsigned dims, video_views_layout_t *out);

/* Any thread: the screens in @screens go back to the main window until
 * the next retry. For a window the renderer could not make or keep, in
 * a frame of @retry (video_windows_frame_t's): one laid out before the
 * last retry is ignored. */
void video_driver_attach_windows(unsigned screens, unsigned retry);

/* Any thread: @screens' window was closed by the user in @frame;
 * attached until the next restore or an edit of their rows, those
 * since @frame included */
void video_driver_close_windows(const video_windows_frame_t *frame,
      unsigned screens);

/* Window @window's layout as drawn on display @display (an ordinal, 0
 * where the native layer placed it), from the thread that runs the
 * context, one at a time per window; latest wins. @window_dims are
 * client pixels, which input points arrive in. NULL clears it. It
 * carries the serial of the snapshot being drawn. */
void video_driver_publish_window_layout(unsigned window,
      unsigned generation, unsigned display,
      const video_views_layout_t *layout,
      const video_views_map_t *core_map, unsigned core_dims,
      unsigned window_dims);

/* The context's thread: window @window, showing @screens, was left at
 * @dims, and at @pos where @moved; latest wins */
void video_driver_publish_window_bounds(unsigned window, unsigned screens,
      unsigned pos, unsigned dims, bool moved);

/* Main thread: those bounds into their screens' settings now, as a
 * frame does, before a config save that runs no frame first */
void video_driver_take_window_bounds(void);

/* Main thread: a point in window @window to libretro coordinates in
 * the core's frame. Window 0 is the main window, through its viewport;
 * another is a screen's own, through its last published layout, and
 * false unless that is of @generation. */
bool video_driver_translate_window_point(unsigned window,
      unsigned generation, int x, int y, bool report_oob,
      uint32_t *result);

/* App thread: the display window @window has drawn on since it was
 * made, 0 while it has not or is gone, or has not drawn since the
 * snapshots last asked for it differently; its client size in *@dims
 * (may be NULL) */
unsigned video_driver_window_shown(unsigned window, unsigned *dims);

/* A renderer's screen windows from one frame to the next, a bit a
 * window id; id 0, the main window, is never set */
typedef struct video_windows_held
{
   /* Ids that have a window */
   unsigned live;
   /* Ids whose window could not be made or drawn, or closed: not made
    * again while the frame's retry count stays at retry, or for a
    * user's close its restore count at restore and the id's
    * settings[].edit at closed_edit[] */
   unsigned failed;
   unsigned closed;
   unsigned retry;
   unsigned restore;
   unsigned closed_edit[RETRO_VIDEO_VIEWS_MAX];
   /* Each live id's video_window_request_t::display */
   unsigned display[RETRO_VIDEO_VIEWS_MAX];
   /* With video_windows_frame_t::monitors: what each id was made for,
    * and the ids that have it */
   video_window_settings_t made[RETRO_VIDEO_VIEWS_MAX];
   unsigned made_screens[RETRO_VIDEO_VIEWS_MAX];
   unsigned made_known;
   /* Of made_known: those made on a named Display (window_named) */
   unsigned made_named;
} video_windows_held_t;

/* @frame's ids with screens, UI layers or a keep, a bit an id: those
 * it wants a window for, or holds one for (a keep). Pure, any thread. */
unsigned video_windows_wanted(const video_windows_frame_t *frame);

/* For @frame, NULL while windows are off: in *@retire the live ids to
 * let go of, those with nothing to show now or made for another
 * display, or on a monitors frame made for another mode or other
 * screens, for other settings after an edit, or on a monitor gone or
 * other than its named one;
 * in *@create the ids with screens or UI layers, no keep and no window,
 * those that failed since the retry count last moved, or were closed
 * since the restore count or their edit count did, left out. An id in
 * both is made again. The caller keeps live, failed, closed,
 * closed_edit and display as it makes and lets go of windows; made is
 * kept here. Pure, any thread. */
void video_windows_steps(video_windows_held_t *held,
      const video_windows_frame_t *frame, unsigned *retire,
      unsigned *create);

/* Window @w's bounds, @dims and @pos where @moved, went to its screen's
 * settings (video_driver_publish_window_bounds()): what it was made for
 * becomes what the frames carry from then, so their echo is no edit.
 * Pure, any thread. */
void video_windows_held_saved(video_windows_held_t *held, unsigned w,
      unsigned pos, unsigned dims, bool moved);

RETRO_END_DECLS

#endif
