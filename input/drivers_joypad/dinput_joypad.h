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
#include <string.h>
#include <boolean.h>
#include <retro_miscellaneous.h>
#include <retro_common_api.h>
#include <retro_atomic.h>

#define WIN32_LEAN_AND_MEAN
#include <dinput.h>

#include "../input_driver.h"
#include "../include/input_pad_view.h"
#include "../common/win_pad_ident.h"

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
   /* joy_state as the poll left it, lent to the readers
    * (input_pad_view.h): 128 buttons, eight axes, four hats */
   input_pad_view_t     view;
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
   /* A DualShock 4's or DualSense's Bluetooth address; 0 for another */
   uint64_t             link_addr;
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

/* A hat's POV, hundredths of a degree clockwise from up, as a view's
 * hat bits (up 1, down 2, left 4, right 8): the four ways and the four
 * between them; anything else, centred included, holds none */
static INLINE uint16_t dinput_pov_hat(DWORD pov)
{
   switch (pov)
   {
      case 0:     return 1;
      case 4500:  return 1 | 8;
      case 9000:  return 8;
      case 13500: return 2 | 8;
      case 18000: return 2;
      case 22500: return 2 | 4;
      case 27000: return 4;
      case 31500: return 1 | 4;
      default:    break;
   }
   return 0;
}

/* @pad's view of its joy_state, after the poll has read it */
static INLINE void dinput_pad_make_view(struct dinput_joypad_data *pad)
{
   unsigned i;
   const DIJOYSTATE2 *js = &pad->joy_state;
   input_pad_view_t  *v  = &pad->view;

   memset(&v->buttons, 0, sizeof(v->buttons));
   for (i = 0; i < ARRAY_SIZE_RGB_BUTTONS; i++)
      if (js->rgbButtons[i])
         BIT256_SET(v->buttons, i);
   v->axes[0] = (int16_t)js->lX;
   v->axes[1] = (int16_t)js->lY;
   v->axes[2] = (int16_t)js->lZ;
   v->axes[3] = (int16_t)js->lRx;
   v->axes[4] = (int16_t)js->lRy;
   v->axes[5] = (int16_t)js->lRz;
   v->axes[6] = (int16_t)js->rglSlider[0];
   v->axes[7] = (int16_t)js->rglSlider[1];
   v->hats    = 0;
   for (i = 0; i < 4; i++)
      v->hats |= (uint16_t)(dinput_pov_hat(js->rgdwPOV[i]) << (i * 4));
}

/* @pad's state with nothing held: buttons up, axes at 0, hats centred */
static INLINE void dinput_pad_release_state(struct dinput_joypad_data *pad)
{
   unsigned h;
   memset(&pad->joy_state, 0, sizeof(pad->joy_state));
   /* zero is a valid POV direction (north): a hat is centred apart */
   for (h = 0; h < ARRAY_SIZE(pad->joy_state.rgdwPOV); h++)
      pad->joy_state.rgdwPOV[h] = DINPUT_POV_CENTERED;
   memset(&pad->view, 0, sizeof(pad->view));
}

RETRO_END_DECLS

/* A DualShock 4's or DualSense's Bluetooth address, from the HID path
 * DirectInput gives for @dev; 0 for any other pad. A pad on cable and
 * Bluetooth at once is two devices to DirectInput and one pad. For the
 * enumeration's walk, off the frontend's thread: the pad answers. */
static INLINE uint64_t dinput_pad_link_addr(LPDIRECTINPUTDEVICE8 dev,
      int32_t vid, int32_t pid)
{
   DIPROPGUIDANDPATH gp;
   char path[MAX_PATH];

   if (vid != 0x054C)
      return 0;
   memset(&gp, 0, sizeof(gp));
   gp.diph.dwSize       = sizeof(gp);
   gp.diph.dwHeaderSize = sizeof(DIPROPHEADER);
   gp.diph.dwHow        = DIPH_DEVICE;
   if (     FAILED(IDirectInputDevice8_GetProperty(dev, DIPROP_GUIDANDPATH,
                  &gp.diph))
         || WideCharToMultiByte(CP_ACP, 0, gp.wszPath, -1, path,
                  sizeof(path), NULL, NULL) <= 0)
      return 0;
   return win_pad_sony_addr(path, (uint16_t)vid, (uint16_t)pid);
}

/* Whether one of the first @cnt pads has Bluetooth address @addr: this
 * is its other link, and not read */
static INLINE bool dinput_pad_is_link(const struct dinput_joypad_data *pads,
      unsigned cnt, uint64_t addr)
{
   unsigned i;
   if (addr)
      for (i = 0; i < cnt; i++)
         if (pads[i].link_addr == addr)
            return true;
   return false;
}

#endif
