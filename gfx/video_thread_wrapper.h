/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef RARCH_VIDEO_THREAD_H__
#define RARCH_VIDEO_THREAD_H__

#include <limits.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
#include <retro_miscellaneous.h>

#include "font_driver.h"

/* Everything here belongs to the wrapper, which is only built with
 * HAVE_THREADS. Without it, the callers that must still compile -
 * gl2/gl3 for the swap count, video_driver.c for the wrapper-active
 * test - get macro stand-ins from video_driver.h, and a prototype here
 * would only collide with those. */
#ifdef HAVE_THREADS

RETRO_BEGIN_DECLS

enum video_thread_win_flags
{
   VIDEO_THREAD_WIN_ALIVE        = (1 << 0),
   VIDEO_THREAD_WIN_FOCUS        = (1 << 1),
   VIDEO_THREAD_WIN_PRESENTABLE  = (1 << 2),
   VIDEO_THREAD_WIN_HAS_WINDOWED = (1 << 3)
};

enum thread_cmd
{
   CMD_VIDEO_NONE = 0,
   CMD_INIT,
   CMD_SET_SHADER,
   CMD_FREE,
   CMD_ALIVE, /* Blocking alive check. Used when paused. */
   CMD_SET_VIEWPORT,
   CMD_SET_ROTATION,
   CMD_READ_VIEWPORT,

   CMD_OVERLAY_ENABLE,
   CMD_OVERLAY_LOAD,
   CMD_OVERLAY_LOAD_TEXTURES,
   CMD_OVERLAY_TEX_GEOM,
   CMD_OVERLAY_VERTEX_GEOM,
   CMD_OVERLAY_FULL_SCREEN,

   CMD_POKE_SET_VIDEO_MODE,
   CMD_POKE_SET_FILTERING,

   CMD_POKE_SET_FBO_STATE,
   CMD_POKE_GET_FBO_STATE,

   CMD_POKE_SET_ASPECT_RATIO,
   CMD_FONT_INIT,
   CMD_CUSTOM_COMMAND,

   CMD_POKE_SHOW_MOUSE,
   CMD_POKE_GRAB_MOUSE_TOGGLE,

   CMD_POKE_SET_HDR_MENU_NITS,
   CMD_POKE_SET_HDR_PAPER_WHITE_NITS,
   CMD_POKE_SET_HDR_EXPAND_GAMUT,
   CMD_POKE_SET_HDR_SCANLINES,
   CMD_POKE_SET_HDR_SUBPIXEL_LAYOUT,
   CMD_SET_NONBLOCK,
   CMD_SUPPRESS_SCREENSAVER,

   CMD_DUMMY = INT_MAX
};

typedef uintptr_t (*custom_command_method_t)(void*);

typedef bool (*custom_font_command_method_t)(const void **font_driver,
      void **font_handle, void *video_data, const char *font_path,
      float font_size, const font_renderer_t *backend,
      bool is_threaded);

typedef struct thread_packet
{
   union
   {
      const char *str;
      void *v;
      int i;
      float f;
      bool b;

      struct
      {
         enum rarch_shader_type type;
         const char *path;
      } set_shader;

      struct
      {
         unsigned dims;
         bool force_full;
         bool allow_rotate;
      } set_viewport;

      struct
      {
         unsigned swap_interval;
         bool nonblock;
         bool adaptive_vsync;
      } nonblock;

      struct
      {
         unsigned index;
         float x, y, w, h;
      } rect;

      struct
      {
         const struct texture_image *data;
         const uintptr_t *textures;
         unsigned num;
      } image;

      struct
      {
         unsigned dims;
         bool fullscreen;
      } new_mode;

      struct
      {
         unsigned index;
         bool smooth;
         bool ctx_scaling;
      } filtering;

      struct
      {
         char msg[128];
         struct font_params params;
      } osd_message;

      struct
      {
         custom_command_method_t method;
         void* data;
         uintptr_t return_value;
      } custom_command;

      struct
      {
         custom_font_command_method_t method;
         const void **font_driver;
         void **font_handle;
         void *video_data;
         const char *font_path;
         float font_size;
         bool return_value;
         bool is_threaded;
         const font_renderer_t *backend;
      } font_init;

      struct
      {
         float menu_nits;
         float paper_white_nits;
         unsigned expand_gamut;
         bool scanlines;
         unsigned subpixel_layout;
      } hdr;
   } data;
   enum thread_cmd type;
} thread_packet_t;

/* A texture upload the main thread does not wait for. Queued nodes
 * are owned by the video thread from post to completion; completed
 * nodes wait in the out list until the main thread delivers them from
 * video_thread_async_poll(). Both lists are guarded by thr->lock. */
typedef void (*video_thread_async_done_t)(void *user, uintptr_t handle);
typedef void (*video_thread_async_release_t)(void *img);

/* What the video thread does with a node. LOAD creates a texture from
 * img and reports the handle; UPDATE writes img into the texture whose
 * handle the node carries, in place, and reports that handle back (0
 * when the driver could not take the update). */
enum video_thread_async_kind
{
   VIDEO_THREAD_ASYNC_LOAD = 0,
   VIDEO_THREAD_ASYNC_UPDATE
};

typedef struct video_thread_async_load
{
   struct video_thread_async_load *next;
   void *img;                      /* struct texture_image*, ours until released */
   void *user;
   video_thread_async_done_t    done;
   video_thread_async_release_t release;
   uintptr_t handle;
   enum texture_filter_type filter;
   uint8_t kind;                   /* enum video_thread_async_kind */
   /* The node belongs to the poster, who embeds it in a resource that
    * outlives the post: the wrapper never frees it, and delivers
    * done() exactly once for every accepted post, so the poster can
    * count on getting it back. Nodes the wrapper allocates itself
    * (video_thread_texture_load_async) have this clear. */
   uint8_t caller_owned;
} video_thread_async_load_t;

/* Deep enough for the burst an overlay issues between two frames - one
 * geometry call per descriptor, and a pad's worth of those is dozens -
 * as well as the settings a menu page changes. Past it the sender
 * waits, as it always did. At 192 bytes a packet this is 24 KiB of the
 * wrapper's own state; the video thread runs the packets where they
 * lie, so none of it reaches a stack frame. A power of two: the index
 * wraps with a mask. */
#define VIDEO_THREAD_DEFERRED_MAX 128

/* The main thread's cost of handing a frame to the video thread,
 * counted in CPU cycles in video_thread_frame() while the statistics
 * overlay is shown and shown as microseconds, over windows of 120
 * pushed frames. Time deliberately spent waiting for a free slot is
 * kept apart from the handoff itself. */
typedef struct video_thread_handoff_stats
{
   uint64_t handoff_avg_x100; /* entry to signal, wait excluded, us */
   uint64_t handoff_worst;
   uint64_t copy_avg_x100;    /* the frame memcpy, us */
   uint64_t copy_worst;
   uint64_t wait_avg_x100;    /* slot wait, us */
   uint64_t wait_worst;
   uint64_t bytes_per_frame;  /* copied */
   unsigned frames_copied;    /* of the window */
   unsigned frames_zero_copy;
   unsigned frames_hw;
   unsigned waits;            /* pushes that waited for a slot */
   unsigned dropped;          /* pushes that replaced a queued frame */
   unsigned drains;           /* holds run a period long to drain */
   /* The core's software-framebuffer asks in the window: granted a
    * slot, granted but pushed from elsewhere, declined because both
    * slots were taken, declined because the frame would not fit */
   unsigned asked;
   unsigned lent;
   unsigned lapsed;
   unsigned declined_ring;
   unsigned declined_size;
} video_thread_handoff_stats_t;

/* Slots of the statistics snapshot thread_video_t::stats publishes.
 * Each 64-bit value takes two, low half first, because the atomics are
 * int-wide on every backend. */
enum video_thread_stat_slot
{
   VIDEO_THREAD_STAT_FLAGS = 0,
   VIDEO_THREAD_STAT_REPEATS_LO,
   VIDEO_THREAD_STAT_REPEATS_HI,
   VIDEO_THREAD_STAT_LAT_AVG_LO,
   VIDEO_THREAD_STAT_LAT_AVG_HI,
   VIDEO_THREAD_STAT_LAT_MAX_LO,
   VIDEO_THREAD_STAT_LAT_MAX_HI,
   VIDEO_THREAD_STAT_CORE_LO,
   VIDEO_THREAD_STAT_CORE_HI,
   VIDEO_THREAD_STAT_RENDER_LO,
   VIDEO_THREAD_STAT_RENDER_HI,
   VIDEO_THREAD_STAT_SWAPS_LO,
   VIDEO_THREAD_STAT_SWAPS_HI,
   VIDEO_THREAD_STAT_SLOTS
};

/* Slots of the viewport thread_video_t::vp_pub publishes. Each size
 * pair travels as one word in VIDEO_SCALE_PACK's layout, so a pair
 * cannot be read half updated even before the sequence is checked. */
enum video_thread_vp_slot
{
   VIDEO_THREAD_VP_POS = 0,
   VIDEO_THREAD_VP_WH,
   VIDEO_THREAD_VP_FULL_WH,
   VIDEO_THREAD_VP_SLOTS
};

/* The bools of that snapshot, in VIDEO_THREAD_STAT_FLAGS. */
#define VIDEO_THREAD_STAT_F_PRESENT_REPEAT  (1 << 0)
#define VIDEO_THREAD_STAT_F_PHASE_DISPLAY   (1 << 1)
#define VIDEO_THREAD_STAT_F_LAT_DISPLAY     (1 << 2)
#define VIDEO_THREAD_STAT_F_DISPLAY_PACING  (1 << 3)

/* Cache-line padding, where two threads write fields that would
 * otherwise share a line. A line carrying a write from each is pulled
 * back and forth between them on every write to either, which costs
 * more than the writes do; the article's phrase for it is write
 * sharing, and it is the reason the values below are published rather
 * than locked in the first place. A full line each way, so the
 * separation holds wherever the fields land rather than depending on
 * an offset. */
#define VIDEO_THREAD_LINE 64

typedef struct thread_video
{
   retro_time_t last_time;
   /* Asynchronous texture uploads, see video_thread_texture_load_async(). */
   struct
   {
      video_thread_async_load_t *in_head,  *in_tail;   /* to upload */
      video_thread_async_load_t *out_head, *out_tail;  /* to deliver */
      /* Set with an entry on out_head, so the main thread looks for
       * uploads to deliver without taking 'lock' when there are none */
      retro_atomic_int_t out_ready;
   } async;
   /* Presenter state, all owned by the video thread. present_period
    * is one display period in usec, taken from the refresh rate of the
    * last frame rendered; next_present is when a repeat of it falls
    * due. present_repeat is set once a frame
    * has been rendered with retain_output and the driver can present
    * it again. */
   retro_time_t present_period;
   /* When the next repeat is due: one period after the last present,
    * measured from the display's own timestamp when the driver has one
    * and from the clock otherwise, and always after that present's
    * completion so a late display report cannot pile repeats up. */
   retro_time_t next_present;
   uint64_t frames_repeated;
   /* Whether the last repeat deadline came from a display timestamp
    * the driver reported (true) or from the clock (false). Stats. */
   bool phase_from_display;
   /* The wrapped driver's answer to get_refresh_rate, polled on the
    * video thread after each frame; 0 when it has none. The presenter
    * paces on it in preference to the setting. Video thread only -
    * every other thread takes it from refresh_rate_bits. */
   float driver_refresh_rate;
   /* Swaps one repeat makes: the group the retained frame made. */
   unsigned present_group;
   /* Whether to ask the driver when its last present reached the
    * display; the setting, carried across with the frame. */
   bool present_timing_ask;
   /* Latency, push to the vblank the frame goes out on, for the
    * statistics overlay: a moving average and the session's worst, in
    * microseconds, and whether that vblank is on the display's own
    * grid or one estimated from the clock. Written by the video thread
    * under 'lock'. */
   retro_time_t last_present_end;
   retro_time_t latency_avg;
   retro_time_t latency_max;
   retro_time_t latency_max_at;
   bool         latency_from_display;

   /* Display pacing. render_time is the video thread's moving average
    * of driver->frame() and is read under 'lock'; core_time is the main
    * thread's moving average of the time between one frame handoff's
    * return and the next handoff's arrival, and run_start is when the
    * last handoff returned. Both main-thread only. */
   retro_time_t render_time;
   retro_time_t core_time;
   /* The last frame presented had queued behind another: the next
    * hold runs a period longer to drain it. Video thread sets it,
    * the hold takes it, both under 'lock'. */
   unsigned drain_cooldown;
   bool drain_pending;
   /* Fast-forward, from the frame info at the push: the hold stands
    * down for it. Distinct from nonblock, which vsync-off also sets. */
   bool fast_forward;
   /* Whether the core ran this iteration, from the frame info at the
    * push: the hold's period is the content's only while it does. */
   bool core_running;
   /* Display pacing's schedule: when the next content frame is due,
    * accumulated in the content's own period. Main thread. */
   retro_time_t content_due;
   retro_time_t run_start;
   /* Handoff cost, this window and the last full one. Main thread. */
   struct
   {
      uint64_t handoff_sum, handoff_max;   /* cycles */
      uint64_t copy_sum, copy_max;
      uint64_t wait_sum, wait_max;
      uint64_t span_ticks, span_us;        /* the window's tick rate */
      uint64_t bytes;
      unsigned copied, zero_copy, hw, waits, dropped, drains;
      unsigned asked, lent, lapsed, declined_ring, declined_size;
      unsigned frames;
      bool counting;                       /* overlay was up last push */
      video_thread_handoff_stats_t last;
   } handoff;
   bool display_pacing;
   bool present_repeat;
   /* A main-thread present_last() asks for one repeat at the next
    * opportunity rather than waiting for the deadline. */
   bool repeat_request;
   /* Log the geometry clamp once per session, not per frame. */
   bool clamp_logged;

   slock_t *lock;
   /* cond_reply: the command reply (pkt->type == reply_cmd). One
    * command is outstanding at a time (cmd_data is a single slot), so
    * one waiter, woken with scond_signal(). Same-thread nesting is
    * counted rather than rejected because the cocoa trampoline drained
    * by video_thread_pump_wait() can re-enter the wrapper on the waiting
    * thread; a second distinct thread is the case that breaks, and
    * cond_reply_waiters checks for it in debug builds. cond_user below
    * is what keeps a second thread out. */
   scond_t *cond_reply;
   /* cond_user: the poster slot. User-side commands come from more than
    * one thread -- the main thread uploads an achievement badge while a
    * task thread takes the screenshot the same unlock asked for -- and
    * two of them in the single slot at once means one reply satisfies
    * both waiters, and the video thread then runs a packet whose payload
    * points into a stack frame that has already returned. So a poster
    * holds the slot from send to reply; the wait pumps the cocoa
    * trampoline like the reply wait does, because the holder's command
    * may be blocked on the main thread. user_owner/user_depth only keep
    * an owner from deadlocking on its own slot; a nested post is not
    * serviceable (see video_thread_user_acquire()). The video thread
    * never takes the slot: a wrapper entry reached from driver->frame()
    * runs inline via inline_reply before the acquire. */
   scond_t *cond_user;
   uintptr_t user_owner;
   unsigned user_depth;
   /* Widget state lock depth user_acquire() released on the owner's
    * behalf, retaken when the slot is released */
   unsigned user_widgets_depth;
#ifdef HAVE_GFX_WIDGETS
   /* Main thread: the panels' text for the next frame pushed,
    * staged by video_thread_status_text() */
   char status_text[NAME_MAX_LENGTH];
   size_t status_text_len;
#endif
#ifdef HAVE_VIDEO_FILTER
   /* Main thread: the next frame pushed is raw, for this thread to
    * filter; its bytes per pixel. Staged by video_thread_defer_filter() */
   unsigned filter_next;
#endif
   /* Main thread: the next frame pushed is in a source pixel format the
    * driver does not take, for this thread to convert. One of
    * enum video_thread_convert. Staged by video_thread_defer_convert() */
   unsigned convert_next;
   /* cond_ring: ring progress (frame.pending / frame.busy changing),
    * broadcast by the video thread when it claims or completes a slot.
    * Any number of waiters, each re-testing its own predicate. */
   scond_t *cond_ring;
   scond_t *cond_thread;
   sthread_t *thread;
   /* The video singleton's (stable) address, captured on the main
    * thread at init: the loop and its helpers reach ra-video state
    * through this, never through the getter, so no thread entry in
    * this file calls into a singleton getter at all. */
   video_driver_state_t *video_st;

   video_info_t info;
   const video_driver_t *driver;

#ifdef HAVE_OVERLAY
   const video_overlay_interface_t *overlay;
#endif
   const video_poke_interface_t *poke;

   void *driver_data;
   input_driver_t **input;
   void **input_data;

   /* Overlay alpha modulation, lock-free. The values are float bits
    * in atomic ints: the main thread's set_alpha (fire-and-forget by
    * design) stores a value relaxed and then store-releases
    * alpha_update; the video thread's per-frame apply clears the
    * flag with an acquire exchange BEFORE reading the values, so a
    * set that lands mid-apply re-raises the flag and is applied
    * whole next frame - the exchange-first order is what makes an
    * update impossible to lose. The array pointer and alpha_mods
    * are written only inside the CMD_OVERLAY_LOAD handler, with the
    * main thread blocked in that command's reply wait and the video
    * thread out of its frame call, so plain reads of both are safe
    * everywhere. */
   retro_atomic_int_t *alpha_mod;
   /* Video thread only: the float bits last handed to the driver for
    * each image, alpha_mods of them, so an apply passes on only what
    * changed. NULL when it could not be had, and every image is set
    * at every apply. */
   int *alpha_applied;

   struct
   {
      void *frame;
      size_t frame_cap;
      unsigned dims;
      float alpha;
      bool frame_updated;
      bool rgb32;
      bool enable;
      bool full_screen;
   } texture;

   unsigned hit_count;
   unsigned miss_count;
   unsigned alpha_mods;

   struct video_viewport read_vp; /* Last viewport reported to caller. */

   /* Content scale, published at the end of each frame. The viewport
    * maths that produces it runs on the video thread, so
    * video_driver_build_info() reads it from here rather than from
    * video_driver_st. Width in the high 16 bits, height in the low, one
    * value so the pair is read whole and without 'lock'. Statistics
    * only. */
   retro_atomic_int_t scale_packed;

   /* The statistics overlay's numbers, published as a seqlock so that
    * reading them takes no lock: 'lock' stays the frame handoff's and
    * the ring's, and a reader only ever loads. The video thread writes
    * the slots inside a region it already holds 'lock' for, which is
    * what serialises publishers; a reader retries while a publish is
    * in flight (odd) or lands across its copy. Statistics only, so the
    * two the main thread feeds - core_time and display_pacing - are
    * carried at one frame's lag. */
   retro_atomic_int_t stats_seq;
   retro_atomic_int_t stats[VIDEO_THREAD_STAT_SLOTS];

   /* The driver's viewport and the rate that came back with it,
    * published by the video thread - the only writer of either - so
    * that an input driver asking for the viewport every poll, and the
    * runloop asking for the rate every iteration, take no lock. The
    * viewport is a seqlock over six slots; the rate is one word, whole
    * at int width, carried as float bits. */
   retro_atomic_int_t vp_seq;
   retro_atomic_int_t vp_pub[VIDEO_THREAD_VP_SLOTS];
   retro_atomic_int_t refresh_rate_bits;

   /* The snapshots above are written by the video thread every frame;
    * cmd_data below is rewritten by a sender for every synchronous
    * command, and a burst of them rewrites it in place. */
   unsigned char pad_published[VIDEO_THREAD_LINE];

   thread_packet_t cmd_data;
   /* Set by the video thread while it runs a command inline on itself:
    * the reply goes here instead of into cmd_data, so a command the
    * main thread sent meanwhile is neither answered nor overwritten.
    * Video thread only. */
   thread_packet_t *inline_reply;

   /* Commands that want nothing back: queued here and run by the video
    * thread on its next pass, so the caller does not wait for a round
    * trip. Under thr->lock, as send_cmd is. A full queue falls back to
    * the synchronous send, so nothing is ever dropped. */
   video_driver_t video_thread;

   enum thread_cmd send_cmd;
   enum thread_cmd reply_cmd;

   retro_atomic_int_t alpha_update;

   /* Core frames cross to the video thread through a two-slot ring so
    * the main thread's copy of frame N+1 overlaps the worker's upload
    * and render of frame N. All ring bookkeeping (tail, pending, busy)
    * is guarded by 'lock'; slot contents are owned by whichever side
    * holds the slot - the main thread between claim and publish, the
    * video thread between claim and completion - and need no lock. */
   struct
   {
      /* Protects the menu texture / apply_state_changes handoff and
       * nothing beyond it: the video thread takes it only for the
       * thread_update_driver_state() that applies them, and the render
       * that follows runs without it. The driver takes the texture's
       * pixels inside its own set_texture_frame(), uploading or copying
       * them there, so the staging buffer is free again as soon as the
       * update returns.
       *
       * It is close to uncontended on the path that uses it most:
       * video_thread_frame() drains the ring whenever the menu texture
       * is enabled, so the worker is idle before the next menu frame's
       * push. The callers it does keep off a render are the ones with
       * no push behind them - set_texture_enable() and
       * apply_state_changes(). Not the ring: 'lock' guards that. */
      slock_t *lock;
      /* Bytes allocated for each slot buffer at thread_init, from the
       * core's declared maximum geometry. A core that hands over a
       * larger frame than it declared is clamped to this. */
      size_t   buffer_size;
      struct video_thread_frame_slot
      {
         uint64_t count;
         /* When the core handed this frame over, on the main thread's
          * clock; the latency readout measures from here. */
         retro_time_t pushed_at;
         /* Hardware-rendered frame: the HW ring slot it lives in, -1
          * for a software frame. See hw_ring below. */
         int hw_slot;
         /* The push carried no pixels (the core duped): the driver is
          * given NULL and repeats what it has, never this buffer -
          * which holds whatever frame was last put in it, an old one. */
         bool dupe;
         uint8_t *buffer;
         unsigned dims;
         unsigned pitch;
         char msg[NAME_MAX_LENGTH];
#ifdef HAVE_OZONE
         char menu_ozone_color_theme[32];
#endif
#ifdef HAVE_GFX_WIDGETS
         /* The on-screen panels' text for the widgets, which this
          * thread draws; zero length leaves what they show */
         char status_text[NAME_MAX_LENGTH];
         /* The widget paths this frame was handed, copied for the same
          * reason the status text is: they are the main thread's, and
          * it may write them again while this frame is drawn. */
         char widget_dir_assets[PATH_MAX_LENGTH];
         char widget_path_font[PATH_MAX_LENGTH];
         /* And the two the menu's frame() watches for a change */
         char menu_rgui_theme_preset[PATH_MAX_LENGTH];
         char menu_dynamic_wallpapers_dir[PATH_MAX_LENGTH];
         size_t status_text_len;
#endif
#ifdef HAVE_VIDEO_FILTER
         /* A raw core frame for this thread to run the software filter
          * on: its bytes per pixel, 0 for a frame ready to draw */
         unsigned filter_bpp;
#endif
         /* A frame still in the core's source pixel format, for this
          * thread to convert before the filter and the driver */
         unsigned convert;
         /* Built by the main thread in video_thread_frame() and handed
          * to the driver's frame call by pointer on the video thread.
          * video_driver_build_info() reads video_driver_st and
          * runloop_state, both of which the main thread mutates, so it
          * must not be called from the worker. */
         video_frame_info_t video_info;
         /* Textures the frontend released before this frame was handed
          * over: every frame that could still name one is drawn by the
          * time this one is, so the video thread frees them once it
          * has drawn it (video_thread_tex_retire_t). */
         void *tex_retire;
         /* The statistics overlay's text for this frame, which the main
          * thread's buffer will not hold by the time this thread draws:
          * video_info.stat_text points here. Only copied when there is
          * text, so a frame without the overlay carries none. */
         char stat_text[VIDEO_STAT_TEXT_SIZE];
      } slot[2];
      /* Slot the video thread claims next. Claiming flips it. */
      unsigned tail;
      /* Filled slots not yet claimed by the video thread, 0..2. */
      unsigned pending;
      /* The video thread has claimed a slot and is rendering it. While
       * set, the slot being rendered is tail ^ 1. */
      bool busy;
      /* Zero-copy: the slot handed to the core through
       * get_current_software_framebuffer, -1 for none. Held free until
       * the core pushes a frame: a push whose data is that slot's
       * buffer publishes it without a copy; any other push clears the
       * reservation first. Guarded by 'lock'. */
      int lent;
      /* Hardware-rendered cores. The core's sync index space is this
       * ring, not the swapchain: VIDEO_THREAD_HW_RING slots each hold a
       * copy of what the core handed over for one frame and a fence
       * the video thread signals after driving the driver with it.
       * Opaque here so this header needs no API types; see
       * video_thread_hw.c. */
      void *hw_ring;
      uint64_t zero_copy_count;
   } frame;

   bool apply_state_changes;
   /* Video thread only: the driver was handed a page since the last
    * apply, so it holds none of alpha_applied - the next apply sets
    * every image. */
   bool alpha_reset;

   /* Textures the frontend has released since the last frame was handed
    * over, waiting for one to carry them to the video thread. Held
    * under thr->lock, as the frame handoff is. */
   void *tex_retire;

   /* Which thread is currently blocked on cond_reply, and how deep,
    * both guarded by lock; see the note on cond_reply. Maintained
    * unconditionally so the struct layout does not depend on the build
    * type; only asserted on in debug builds. */
   uintptr_t cond_reply_waiter;
   unsigned cond_reply_waiters;
   /* A call the video thread needs run on the thread that is waiting
    * for its reply - the core's thread, which holds the core's GL
    * context. Posted from the video thread while a synchronous command
    * is in flight, run by the waiter inside its wait loop, and the
    * video thread waits for done. See video_thread_call_on_waiter(). */
   struct
   {
      void (*fn)(void *data);
      void *data;
      scond_t *cond;
      /* The number of commands sent and not yet answered, each with a
       * thread that will wait for the reply and can service a call
       * there; without one, the caller runs it itself. */
      unsigned waiters;
      bool pending;
      bool done;
   } waiter_call;

   /* Published by the video thread after each frame and read by the
    * main thread, every frame, without 'lock': the window's four
    * answers in one word of VIDEO_THREAD_WIN_* bits, one store for all
    * of them. PRESENTABLE is the context's answer to "have you anything
    * to present to"; the context data belongs to the video thread, and
    * asking it directly from the runloop would read a swapchain handle
    * while this thread rebuilds it. */
   retro_atomic_int_t win_flags;
   /* The worker still takes commands: set before it starts, cleared as
    * it handles CMD_FREE. Not 'alive', which is the driver's answer for
    * the window and goes false - on a close or a quit signal - while the
    * worker still runs and holds the context */
   retro_atomic_int_t worker_running;

   /* The flags above are published by the video thread every frame;
    * the two below, and the ring's head after them, are the main
    * thread's. */
   unsigned char pad_flags[VIDEO_THREAD_LINE];

   bool nonblock;
   bool is_idle;

   /* Commands that want nothing back, for the video thread to run on
    * its next pass. One producer (whichever thread calls the setters)
    * and one consumer (the video thread), so the ring needs no lock:
    * the producer moves head after writing a slot, the consumer moves
    * tail after running one, and each reads the other's index with an
    * acquire. The indices only grow; the slot is the index modulo the
    * size. Allocated once, so the ring's size is not this struct's.
    *
    * Last in the struct, so a new field goes after these rather than
    * between anything above. The padding around them is deliberate and
    * is described where VIDEO_THREAD_LINE is defined; a field added
    * inside one of those pads puts back the sharing they exist to
    * remove. */
   thread_packet_t *deferred;
   retro_atomic_int_t deferred_head;

   /* head is the producer's, tail is the video thread's, and each
    * reads the other every pass: on one line the pass costs a
    * transfer in each direction. */
   unsigned char pad_ring[VIDEO_THREAD_LINE];

   retro_atomic_int_t deferred_tail;
} thread_video_t;

/**
 * video_init_thread:
 * @out_driver                : Output video driver
 * @out_data                  : Output video data
 * @input                     : Input input driver
 * @input_data                : Input input data
 * @driver                    : Input Video driver
 * @info                      : Video info handle.
 *
 * Creates, initializes and starts a video driver in a new thread.
 * Access to video driver will be mediated through this driver.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool video_init_thread(
      const video_driver_t **out_driver, void **out_data,
      input_driver_t **input, void **input_data,
      const video_driver_t *driver, const video_info_t info);

bool video_thread_font_init(
      const void **font_driver,
      void **font_handle,
      void *data,
      const char *font_path,
      float font_size,
      const font_renderer_t *backend,
      custom_font_command_method_t func,
      bool is_threaded);

/* Blocking run-on-video-thread: func(data) executes on the video
 * thread while the caller waits, or directly when the wrapper is
 * not active. The mechanism behind video_thread_texture_handle,
 * exported for any main-thread code that must touch video-thread-
 * owned state. */
uintptr_t video_thread_run_blocking(custom_command_method_t func,
      void *data);

uintptr_t video_thread_texture_handle(void *data,
      custom_command_method_t func);

/* Upload @img on the video thread without blocking the caller. The
 * video thread runs the upload at its next wake, calls release(img),
 * and parks the handle; the main thread then gets done(user, handle)
 * from video_thread_async_poll(), which video_thread_frame() runs
 * every frame. If the wrapper is torn down first, in-flight loads are
 * released and delivered with handle 0. Returns false (and takes no
 * ownership) when the wrapper is not active - the caller does the
 * synchronous load instead. Main thread only. */
bool video_thread_texture_load_async(void *img,
      enum texture_filter_type filter,
      video_thread_async_done_t done, void *user,
      video_thread_async_release_t release);

/* Post a caller-owned node (see video_thread_async_load_t) for the
 * video thread: kind, img, handle (UPDATE), filter (LOAD), done, user
 * and release filled in by the caller, next left alone. The node is
 * the wrapper's from the return until done() has run, which happens
 * from video_thread_async_poll() on the main thread, or from the
 * wrapper's teardown with handle 0. No allocation: this is the
 * streaming path, one embedded node per surface. Returns false, having
 * taken nothing, when the wrapper is not active or the caller is the
 * video thread; the caller then runs the operation synchronously.
 * Main thread only. */
bool video_thread_async_post(video_thread_async_load_t *n);

/* Whether the wrapped driver updates textures in place; false while
 * no wrapper is up. video_driver_texture_can_update() asks this so a
 * wrapper forwarder is never mistaken for a capability. */
bool video_thread_texture_can_update(void);

/* Deliver completed asynchronous uploads to their done() callbacks.
 * Main thread; video_thread_frame() calls it, callers that upload
 * while no frames are being pushed can call it themselves. */
void video_thread_async_poll(void);

/* Barrier: wait until the video thread is idle (no pending frame).
 * Must be called from the main thread before freeing GPU resources
 * that an in-flight frame might reference.  No-op on non-threaded
 * video or when called from the video thread. */
/* The context's last answer to "have you anything to present to",
 * polled on the video thread after each frame and published under
 * thr->lock. False only when the wrapper is active and the context
 * said so; true in every other case, including when there is no
 * wrapper, so callers need no threading test of their own. */
bool video_thread_presentable(void);

bool video_thread_presenter_stats(uint64_t *repeats, bool *display_phase);

/* Display pacing statistics: whether it is on, and the core and render
 * times it is reserving, in microseconds. From the published snapshot,
 * so the two the main thread feeds are a frame behind. Returns whether
 * the wrapper is up at all. */
bool video_thread_pacing_stats(bool *display_pacing,
      retro_time_t *core_time, retro_time_t *render_time);

/* Latency from the core's handover to the present, for the overlay:
 * the moving average and the session's worst, and whether the present
 * end came from the display. False with no wrapper. */
bool video_thread_latency_stats(retro_time_t *avg, retro_time_t *worst,
      bool *from_display);

/* video_st->swap_count is written by the video thread while the wrapper
 * is installed; this reads it from the published snapshot, which every
 * advance of it is published with. Without the wrapper (or from the
 * video thread) it is the plain value. */
uint64_t video_thread_swap_count(void);

/* False when the wrapper is not active. Main thread. */
bool video_thread_get_handoff_stats(video_thread_handoff_stats_t *out);

/* On the main thread, on Cocoa: run the trampoline mode briefly so a
 * job the video thread marshalled to the main thread can run. Anywhere
 * else, nothing. For a main-thread wait on something the video thread
 * will signal - a ring fence - where the video thread may need the
 * main thread first. */
void video_thread_main_pump(void);

/* From the video thread, while it is answering a synchronous command:
 * runs fn on the thread waiting for the reply and returns when it has
 * run. That thread is the core's, which is where a driver must go to
 * touch the core's GL context under the hardware ring. From any other
 * thread, or with no waiter, fn simply runs on the caller. */
void video_thread_call_on_waiter(void (*fn)(void *data), void *data);

void video_thread_wait_idle(void);

/* GPU recording under the wrapper, main thread. The video thread reads
 * back each frame it draws through a dedicated recording reader; this
 * takes the newest since the last call without waiting for one.
 * Returns 1 with *frame set (width * height BGR24 rows, bottom up, as
 * read_viewport() leaves them), 0 when nothing new has arrived since a
 * frame was last taken, -1 when there is nothing to record yet, and -2
 * when the driver has no recording reader, for the caller to use
 * read_viewport(). Starts the readbacks on first use and again at a new
 * output size. Viewport changes are scaled and letterboxed by the worker. */
int video_thread_record_take(void *data, unsigned dims,
      const uint8_t **frame);
/* Stops the readbacks; the buffers go once no frame names them. */
void video_thread_record_stop(void *data);

#ifdef HAVE_GFX_WIDGETS
/* Main thread: stages the on-screen panels' text to travel with the
 * next frame pushed, for the widgets the worker draws. */
void video_thread_status_text(const char *s);
#endif

#ifdef HAVE_VIDEO_FILTER
/* Main thread: the next frame pushed is the core's, unfiltered, in a
 * format of in_bpp bytes per pixel; the worker runs the software
 * filter on it before drawing. */
void video_thread_defer_filter(unsigned in_bpp);
#endif

/* Source pixel formats the video thread converts on the frame's way to
 * the driver, in place of the main thread doing so before handover */
enum video_thread_convert
{
   VIDEO_THREAD_CONVERT_NONE = 0,
   VIDEO_THREAD_CONVERT_0RGB1555,    /* to RGB565 through the scaler */
   VIDEO_THREAD_CONVERT_XRGB2101010  /* to XRGB8888 */
};

/* Stages the next frame pushed for conversion on the video thread */
void video_thread_defer_convert(enum video_thread_convert kind);

RETRO_END_DECLS

#endif /* HAVE_THREADS */

#endif
