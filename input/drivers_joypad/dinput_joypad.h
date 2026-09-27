/*  RetroArch - A frontend for libretro.
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

/* Shared declarations for the DirectInput pad array.
 *
 * g_pads[] is defined in dinput_joypad.c and read by
 * xinput_hybrid_joypad.c, which cross-references DirectInput to
 * recover the VID/PID that XInput does not expose. Both files used to
 * carry their own copy of struct dinput_joypad_data under the same
 * __DINPUT_JOYPAD_H guard, and the copies had drifted apart: the
 * definer's was 432 bytes, the reader's 400, because four rumble
 * storage fields were added to one and not the other.
 *
 * That was latent rather than live. Each file also had its own static
 * copies of the accessors, and only one joypad driver is active at a
 * time, so whichever of the two was running used its own view of the
 * array consistently and the other never touched it. The array was
 * over-allocated, not overrun. It would have become live the moment
 * the two drivers cooperated on the same entries, or the moment
 * someone added a field to one copy and not the other again - which
 * is how it got here.
 *
 * One declaration, here.
 */

#ifndef __DINPUT_JOYPAD_H
#define __DINPUT_JOYPAD_H

#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>
#include <retro_atomic.h>

#define WIN32_LEAN_AND_MEAN
#include <dinput.h>

#include "../input_driver.h"

/* For DIJOYSTATE2 struct, rgbButtons will always have 128 elements */
#define ARRAY_SIZE_RGB_BUTTONS 128

/* DirectInput POV value indicating the hat is centred (no direction pressed).
 * rgdwPOV[] returns this sentinel when the hat is released. */
#define DINPUT_POV_CENTERED 0xFFFFFFFFu

RETRO_BEGIN_DECLS

struct dinput_joypad_data
{
   LPDIRECTINPUTDEVICE8 joypad;
   DIJOYSTATE2          joy_state;
   char                *joy_name;
   char                *joy_friendly_name;
   int32_t              vid;
   int32_t              pid;
   LPDIRECTINPUTEFFECT  rumble_iface[2];
   DIEFFECT             rumble_props;
   /* Persistent storage for fields referenced by rumble_props pointers.
    * Previously these lived on the stack in dinput_create_rumble_effects(),
    * causing rumble_props to hold dangling pointers after that call returned. */
   DWORD                rumble_axis;
   LONG                 rumble_direction;
   DIENVELOPE           rumble_envelope;
   DICONSTANTFORCE      rumble_force;
};

/* TODO/FIXME - globals referenced outside; candidate for context-struct refactor */
extern struct dinput_joypad_data g_pads[MAX_USERS];
extern unsigned g_joypad_cnt;

/* Joypad-owned DirectInput context, separate from the shared
 * keyboard/mouse context (g_dinput_ctx) in dinput.c. */
extern LPDIRECTINPUT8 g_dinput_joypad_ctx;

/* True while this generation's pad enumeration is in flight: set when
 * its job is started, cleared when it publishes or is abandoned. Main
 * thread only. */
extern volatile bool g_dinput_enum_inflight;

/* One pad enumeration. EnumDevices() runs on the task queue and fills
 * the job, never g_pads[]. The driver's poll() notices the walk has
 * ended and moves the pads into g_pads[] in one step; no task callback
 * is involved, so a walk the task queue lets go of (the task is pushed
 * detachable) still reports in. destroy() never waits for a walk: it
 * marks the job abandoned and drops its reference. The walk holds the
 * other, and whichever side lets go last frees the job.
 *
 * The walk honours the task queue's detachable contract: it touches
 * only the job, and its task's flags. No logging, no driver globals. */
struct dinput_enum_job;

/* run: the walk, on the task queue. done: the driver's own step after
 * the pads have been moved into g_pads[], on the main thread. */
typedef void (*dinput_enum_fn)(struct dinput_enum_job *job);

struct dinput_enum_job
{
   struct dinput_joypad_data pads[MAX_USERS];
   /* Hybrid driver: XInput user per pad, -1 for a DirectInput pad. */
   int                       xuser[MAX_USERS];
   /* Its own reference, so an abandoned walk keeps the context alive
    * after destroy() drops the driver's. */
   LPDIRECTINPUT8            ctx;
   /* Captured on the main thread at start; the walk sets the
    * cooperative level against it. */
   HWND                      hwnd;
   /* Hybrid driver: the walk's RAWINPUT HID snapshot. */
   void                     *hid_cache;
   unsigned                  hid_cache_cnt;
   unsigned                  cnt;
   /* Hybrid driver: snapshot of the XInput state the walk reads. */
   unsigned                  next_xuser;
   bool                      xinput_connected[4];
   bool                      block_xinput;
   dinput_enum_fn            run;
   dinput_enum_fn            done;
   /* 1 once the walk has returned; its writes to the job happen
    * before. */
   retro_atomic_int_t        walk_done;
   /* Set by destroy(); read by the walk to stop early. */
   retro_atomic_int_t        abandoned;
   /* Driver side and walk side, one each. */
   retro_atomic_int_t        refs;
};

/* The in-flight job of the current generation, or NULL. */
extern struct dinput_enum_job *g_dinput_enum_job;

/* Allocates a job holding its own reference to g_dinput_joypad_ctx. */
struct dinput_enum_job *dinput_enum_job_new(void);

/* Makes the job current and queues run on the task queue as a
 * detachable task, or runs it inline if no task can be allocated. */
void dinput_enum_job_start(struct dinput_enum_job *job,
      dinput_enum_fn run, dinput_enum_fn done);

/* From poll(), main thread: if the current walk has ended, moves its
 * pads into g_pads[], ends the in-flight state and calls its done. */
void dinput_enum_job_poll(void);

/* destroy(): detach the in-flight job, if any, without waiting. */
void dinput_enum_job_abandon(void);

/* Rumble parameters point into the pad that owns them; re-aim them
 * after the pad is copied. */
void dinput_pad_rebind_rumble(struct dinput_joypad_data *pad);

/* Stops and releases the pad's effects and device, frees its names,
 * and zeroes it. */
void dinput_pad_release(struct dinput_joypad_data *pad);

/* Logs what the walk could not: a DirectInput pad whose rumble
 * effects were not created. Main thread. */
void dinput_pad_report_rumble(unsigned port,
      const struct dinput_joypad_data *pad);

RETRO_END_DECLS

#endif
