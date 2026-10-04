/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2024 - Daniel De Matteis
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

/* Windows RawInput-based joypad driver.
 *
 * Uses the HID (Human Interface Device) subset of Windows Raw Input API to
 * read gamepad/joystick state without DirectInput or XInput dependencies.
 *
 * Advantages:
 *   - No dependency on dinput8.dll or xinput DLLs
 *   - Lower latency than DirectInput for HID gamepads
 *   - Supports hotplugging via WM_INPUT_DEVICE_CHANGE
 *   - Access to the full HID report (all buttons/axes the device exposes)
 *
 * Limitations:
 *   - XInput-only controllers (Xbox 360/One when using the XInput-only driver
 *     path) will NOT appear here; pair with xinput_joypad for those.
 *   - No force-feedback / rumble (HID PID output reports are not implemented).
 */

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <setupapi.h>

#include <boolean.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <retro_atomic.h>
#include <compat/strl.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../config.def.h"
#include "../../tasks/tasks_internal.h"
#include "../input_driver.h"
#include "../common/sony_pad_output.h"
#include "../common/output_writer.h"
#include "../common/sony_pad_motion.h"
#include "../../verbosity.h"

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

#define RAWINPUT_MAX_BUTTONS     128
#define RAWINPUT_MAX_AXES          8
#define RAWINPUT_MAX_HATS          4

/* Axis range is normalised to [-0x7fff, +0x7fff] for RetroArch. */
#define RAWINPUT_AXIS_MIN    (-0x7fff)
#define RAWINPUT_AXIS_MAX    ( 0x7fff)

/* HID usage page / usage for gamepads & joysticks */
#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC      0x01
#endif
#ifndef HID_USAGE_GENERIC_JOYSTICK
#define HID_USAGE_GENERIC_JOYSTICK  0x04
#endif
#ifndef HID_USAGE_GENERIC_GAMEPAD
#define HID_USAGE_GENERIC_GAMEPAD   0x05
#endif

/* Hat switch (POV) HID usage */
#ifndef HID_USAGE_GENERIC_HATSWITCH
#define HID_USAGE_GENERIC_HATSWITCH 0x39
#endif

/* Generic Desktop axis usages: X(0x30)..Dial(0x37), plus Slider(0x36).
 * We only treat value caps within this range on the Generic Desktop page
 * as real analog axes.  Anything else (e.g. battery level, vendor-defined
 * values) is ignored to avoid phantom axis readings. */
#ifndef HID_USAGE_GENERIC_X
#define HID_USAGE_GENERIC_X         0x30
#endif

#ifndef HID_USAGE_GENERIC_SLIDER
#define HID_USAGE_GENERIC_SLIDER    0x36
#endif

#ifndef HID_USAGE_GENERIC_DIAL
#define HID_USAGE_GENERIC_DIAL      0x37
#endif

/* Axis slots follow DirectInput's DIJOYSTATE2 layout - lX, lY, lZ,
 * lRx, lRy, lRz, then rglSlider[0..1] - so a dinput autoconfig
 * profile binds the same physical axes under this driver. X..Rz take
 * their fixed slot; Slider and Dial share the two slider slots in
 * value-cap order. *sliders counts the slider slots handed out so far
 * and must start at zero for each walk of val_caps[]. Returns -1 for
 * an axis left without a slot. Only called on axis usages. */
static INLINE int winraw_joypad_axis_slot(USAGE usage, unsigned *sliders)
{
   if (usage < HID_USAGE_GENERIC_SLIDER)
      return (int)(usage - HID_USAGE_GENERIC_X);
   if (*sliders < 2)
      return 6 + (int)(*sliders)++;
   return -1;
}

static INLINE bool winraw_joypad_is_axis_usage(const HIDP_VALUE_CAPS *vcap)
{
   USAGE usage = vcap->IsRange
               ? vcap->Range.UsageMin
               : vcap->NotRange.Usage;

   /* Must be on the Generic Desktop usage page */
   if (vcap->UsagePage != HID_USAGE_PAGE_GENERIC)
      return false;

   /* Accept X, Y, Z, Rx, Ry, Rz, Slider, Dial (0x30..0x37) */
   return (usage >= HID_USAGE_GENERIC_X && usage <= HID_USAGE_GENERIC_DIAL);
}

/* ------------------------------------------------------------------ */
/* Per-pad state                                                       */
/* ------------------------------------------------------------------ */

typedef struct winraw_joypad_joypad_data
{
   /* --- Hot fields (accessed every WM_INPUT + every frame poll) --- */
   /* Keeping these together maximises L1 cache line utilisation.     */
   HANDLE              hDevice;      /* RawInput device handle           */
   bool                connected;

   uint16_t            num_buttons;
   uint16_t            num_axes;
   uint16_t            num_hats;
   USAGE               btn_usage_min;

   int16_t             axes[RAWINPUT_MAX_AXES];        /* 16 bytes */
   uint8_t             hats[RAWINPUT_MAX_HATS];        /*  4 bytes */
   bool                buttons[RAWINPUT_MAX_BUTTONS];  /* 128 bytes */

   /* --- Cold fields (accessed only at device add/remove) ---------- */
   PHIDP_PREPARSED_DATA preparsed;   /* HID preparsed data (opaque blob) */
   HIDP_CAPS           caps;         /* HID device capabilities          */
   HIDP_BUTTON_CAPS   *btn_caps;
   HIDP_VALUE_CAPS    *val_caps;
   USAGE               btn_usage_max;

   uint16_t            vid;
   uint16_t            pid;
   char                name[256];
   /* The device interface path, which is what tells one controller
    * from another of the same model. Empty if Windows gave none. */
   char                path[512];

   /* --- Motion sensors (a DualShock 4 or a DualSense) -------------- */
   /* The pad's gyroscope and accelerometer are in its input report,
    * outside what the report's HID description covers
    * (input/common/sony_pad_motion.h). They are read out of a report
    * only while a core has asked for them: motion_on. */
   /* Which of XInput's pads this is, or -1 for one read from raw
    * input. See winraw_joypad_xinput_rescan(). */
   int8_t              xuser;
   uint8_t             motion_model; /* enum sony_pad_model, or none    */
   uint8_t             motion_on;    /* bit 0 accelerometer, 1 gyroscope */
   bool                motion_seen;  /* a report has had them            */
   sony_pad_motion_t   motion;
} winraw_joypad_joypad_data_t;

/* ------------------------------------------------------------------ */
/* Static globals                                                      */
/* ------------------------------------------------------------------ */

/* TODO/FIXME - static globals */
static winraw_joypad_joypad_data_t winraw_joypad_pads[MAX_USERS];
/* the motion sensors a core has asked of each port (bit 0 the
 * accelerometer, 1 the gyroscope): kept while pads come and go */
static uint8_t winraw_joypad_motion_want[MAX_USERS];
static unsigned winraw_joypad_pad_count          = 0;
static HWND     winraw_joypad_msg_window         = NULL;
static bool     winraw_joypad_initialised        = false;

/* Read by the poll (see the note above winraw_joypad_joypad_poll()):
 * whether it is, the thread the window was then made on, and whether
 * making it failed. */
static bool     winraw_joypad_by_poll            = false;
static DWORD    winraw_joypad_window_tid         = 0;
static bool     winraw_joypad_window_failed      = false;
/* Polls since the window's own messages were last looked for. */
static unsigned winraw_joypad_polls_since_look   = 0;

/* Reports read in bulk and not parsed yet: for each controller, the
 * newest report of each report ID. See winraw_joypad_take_hid(). */
#define WINRAW_JOYPAD_HELD_IDS   4
#define WINRAW_JOYPAD_HELD_BYTES 128

typedef struct
{
   uint8_t  count;                              /* report IDs held */
   uint8_t  order[WINRAW_JOYPAD_HELD_IDS];      /* their slots, the newest last */
   uint16_t size[WINRAW_JOYPAD_HELD_IDS];
   BYTE     data[WINRAW_JOYPAD_HELD_IDS][WINRAW_JOYPAD_HELD_BYTES];
} winraw_joypad_held_t;

static winraw_joypad_held_t winraw_joypad_held[MAX_USERS];

/* Reports handed over by bulk reads, and reports parsed: logged when
 * the driver is destroyed. */
static unsigned long winraw_joypad_reports_taken;
static unsigned long winraw_joypad_reports_parsed;

/* winraw_input.c */
extern bool winraw_raw_input_polled(void);
extern void winraw_queue_read(void);
extern void winraw_queue_claim_thread(bool claim);

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/* Convert an HID hat-switch value (0..7 for 8-way, 8 or 0xF = centred)
 * into the bitmask format RetroArch expects for HAT_UP_MASK etc.
 * Uses a lookup table to avoid branching in the per-report hot path. */
static const uint8_t winraw_joypad_hat_lut[8] = {
   (1 << 0),                    /* 0: N  = up          */
   (1 << 0) | (1 << 3),         /* 1: NE = up+right    */
   (1 << 3),                     /* 2: E  = right       */
   (1 << 1) | (1 << 3),         /* 3: SE = down+right  */
   (1 << 1),                     /* 4: S  = down        */
   (1 << 1) | (1 << 2),         /* 5: SW = down+left   */
   (1 << 2),                     /* 6: W  = left        */
   (1 << 0) | (1 << 2),         /* 7: NW = up+left     */
};

static uint8_t winraw_joypad_hat_value_to_bitmask(LONG value, LONG logical_min, LONG logical_max)
{
   int dir;

   if (value < logical_min || value > logical_max)
      return 0; /* centred */

   dir = (int)(value - logical_min);

   /* 4-way -> 8-way */
   if ((logical_max - logical_min + 1) == 4)
      dir *= 2;

   if (dir >= 0 && dir < 8)
      return winraw_joypad_hat_lut[dir];

   return 0;
}

/* HidP_GetUsageValue always returns an unsigned ULONG, but when the HID
 * descriptor declares a signed logical range (LogicalMin < 0) the value
 * is actually two's-complement in the report field.  We need to
 * sign-extend it manually using the field's bit size. */
static INLINE LONG winraw_joypad_sign_extend(ULONG value, USHORT bit_size)
{
   /* If the top bit of the field is set, the value is negative in the
    * HID descriptor's signed interpretation. */
   if (bit_size > 0 && bit_size < 32)
   {
      ULONG sign_bit = 1UL << (bit_size - 1);
      if (value & sign_bit)
         value |= ~((1UL << bit_size) - 1); /* sign-extend */
   }
   return (LONG)value;
}

/* Scale a raw HID axis value from [logical_min .. logical_max]
 * into RetroArch's signed 16-bit range [-0x7fff .. +0x7fff].
 * Uses pure integer arithmetic to avoid FPU overhead in the
 * per-report hot path. */
static int16_t winraw_joypad_scale_axis(LONG value, LONG logical_min,
   LONG logical_max)
{
   LONG range = logical_max - logical_min;

   if (range <= 0)
      return 0;

   /* Clamp to the declared logical range */
   if (value < logical_min)
      value = logical_min;
   else if (value > logical_max)
      value = logical_max;

   /* Map [logical_min .. logical_max] -> [-0x7fff .. +0x7fff]
    * Formula: result = ((value - min) * 2 * 0x7fff) / range - 0x7fff
    * Using 64-bit intermediate to prevent overflow. */
   {
      int32_t result = (int32_t)(
         ((int64_t)(value - logical_min) * (2 * 0x7fff)) / range - 0x7fff);

      if (result < -0x7fff)
         result = -0x7fff;
      else if (result > 0x7fff)
         result = 0x7fff;

      return (int16_t)result;
   }
}

/* Look up a pad slot by HANDLE.  Returns index or -1. */
/* ------------------------------------------------------------------ */
/* Rumble                                                              */
/* ------------------------------------------------------------------ */

/* Raw input reads a controller; it has no way to write to one, and
 * this driver had no rumble at all. A DualShock 4 or a DualSense
 * rumbles on an output report in its own format
 * (input/common/sony_pad_output.h), and Windows lets a controller's
 * HID device be opened for writing. So for those pads the device is
 * opened when the pad arrives and the report written when the
 * strength changes.
 *
 * Not from the frontend's thread: a write to a controller is a
 * transfer on its link, which takes about a millisecond over USB and
 * can take many over Bluetooth, and the frame is not to wait for it.
 * The writes are made by a thread of the driver's own, started when
 * the first such pad arrives and stopped with the driver.
 *
 * Nothing is shared between the two under a lock; what passes
 * between them passes through retro_atomic:
 *
 * - the strength wanted, one word a pad (.want). set_rumble() stores
 *   it and wakes the thread, which writes what is wanted now - not
 *   every step on the way to it;
 *
 * - the device itself. An open device belongs to one side at a time.
 *   The driver opens it and hands it over through the pad's inbox
 *   (.inbox, a pointer swapped in); from then on it is the thread's,
 *   which writes to it and is the one to close it. A pad that goes
 *   is told the same way: "close" is swapped into the inbox, and the
 *   thread stills the motors and closes the device when it gets to
 *   it. So unplugging a pad never waits on a write, and no handle is
 *   closed under a write in progress. A device that was handed over
 *   and never taken - the pad went again first - comes back out of
 *   the swap and is closed by the driver.
 *
 * Stopping the driver stops the thread, which stills and closes what
 * it holds before it ends.
 *
 * An Xbox pad - any pad Windows drives through XInput - is the other
 * kind of device the thread writes to. Raw input reads it as a plain
 * HID device, with no way to rumble it; XInput has the way
 * (XInputSetState) and its own numbering of the pads, one to four,
 * with nothing that says which raw input device is which of its
 * pads. The thread works that out, when a pad is first to rumble:
 *
 * - with one such pad here and one pad XInput has that is not yet
 *   taken, they are the same pad;
 * - with more, the pad is the one of XInput's whose buttons are held
 *   as this pad's are (the driver publishes each such pad's buttons
 *   as it parses them, .buttons). With no button held there is
 *   nothing to tell them by, and the pad is not rumbled until there
 *   is: better none than another player's.
 *
 * XInput itself is loaded by the thread, when it first has such a
 * pad, and asked only about pads that are there.
 *
 * Controllers that are neither have no rumble here still. */

/* XInput, by its own layout and not its header, which not every
 * toolchain has: loaded when needed, called through these. */
typedef struct
{
   DWORD packet;
   WORD  buttons;
   BYTE  left_trigger, right_trigger;
   SHORT lx, ly, rx, ry;
} winraw_xinput_state_t;

typedef struct
{
   WORD left_motor, right_motor;
} winraw_xinput_vibration_t;

typedef DWORD (WINAPI *winraw_xinput_get_state_t)(DWORD, winraw_xinput_state_t*);
typedef DWORD (WINAPI *winraw_xinput_set_state_t)(DWORD, winraw_xinput_vibration_t*);

#define WINRAW_XINPUT_PADS 4

/* a device to write to: a Sony pad open for writing, with what its
 * reports are to look like, or one of XInput's pads */
typedef struct
{
   bool     xinput;     /* an XInput pad; the rest is a Sony pad's    */
   int      xuser;      /* which of XInput's, or -1: not known yet    */
   HANDLE   handle;
   uint8_t  model;      /* enum sony_pad_model                        */
   bool     bluetooth;
   bool     v2;         /* DualSense: the newer way of the motors     */
   USHORT   report_len; /* the length Windows wants of a write        */
} winraw_joypad_out_dev_t;

/* "close what you hold", in a pad's inbox */
#define WINRAW_JOYPAD_OUT_CLOSE ((void*)(uintptr_t)1)

typedef struct
{
   /* from the driver to the thread: a device to take over
    * (winraw_joypad_out_dev_t*), WINRAW_JOYPAD_OUT_CLOSE, or NULL */
   retro_atomic_ptr_t inbox;
   /* wanted: the strong motor in the low sixteen bits, the weak in
    * the high. Stored by set_rumble(), loaded by the thread. */
   retro_atomic_int_t want;
   /* the player its lights are to show, from 1; 0 to leave them
    * alone; -1 to put them out. Stored by the driver's poll, loaded
    * by the thread. */
   retro_atomic_int_t player;
   /* an XInput pad's buttons as raw input reports them, one bit each
    * in XInput's own order. Stored by the driver as it parses a
    * report, loaded by the thread to tell which of XInput's pads this
    * is. */
   retro_atomic_int_t buttons;
   /* the driver's own: a device was handed over for this pad; and it
    * is an XInput pad, whose buttons are to be published */
   bool present;
   bool xinput;
   int  player_asked;   /* the player last stored for the thread     */
   /* the thread's own */
   winraw_joypad_out_dev_t *dev;
   int  sent;           /* what it last wrote                         */
   int  sent_player;    /* and the player its lights were last given  */
   bool failed;         /* a write failed, and it has been said       */
} winraw_joypad_out_t;

static winraw_joypad_out_t winraw_joypad_out[MAX_USERS];
/* the thread's own: XInput, once loaded, and which pad of this
 * driver's each of XInput's pads is (a slot, or -1) */
static HMODULE                   winraw_xinput_dll;
static bool                      winraw_xinput_tried;
static winraw_xinput_get_state_t winraw_xinput_get_state;
static winraw_xinput_set_state_t winraw_xinput_set_state;
static int                       winraw_xinput_taken[WINRAW_XINPUT_PADS];
/* the thread: input/common/output_writer.h's */
static input_output_writer_t *winraw_joypad_out_writer;
/* counted for the log; the thread's while it runs */
static unsigned long       winraw_joypad_out_writes;

/* how long a write may take before it is given up */
#define WINRAW_JOYPAD_OUT_TIMEOUT_MS 250

/* The thread's: XInput is loaded, the newest there is. */
static bool winraw_xinput_load(void)
{
   static const char *names[] = {
      "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
   unsigned i;

   if (winraw_xinput_tried)
      return winraw_xinput_set_state != NULL;
   winraw_xinput_tried = true;
   for (i = 0; i < WINRAW_XINPUT_PADS; i++)
      winraw_xinput_taken[i] = -1;
   for (i = 0; i < ARRAY_SIZE(names) && !winraw_xinput_dll; i++)
      winraw_xinput_dll = LoadLibraryA(names[i]);
   if (winraw_xinput_dll)
   {
      winraw_xinput_get_state = (winraw_xinput_get_state_t)
         GetProcAddress(winraw_xinput_dll, "XInputGetState");
      winraw_xinput_set_state = (winraw_xinput_set_state_t)
         GetProcAddress(winraw_xinput_dll, "XInputSetState");
   }
   if (!winraw_xinput_get_state || !winraw_xinput_set_state)
   {
      winraw_xinput_get_state = NULL;
      winraw_xinput_set_state = NULL;
      RARCH_LOG("[RawInput Joypad] Rumble: XInput could not be loaded;"
            " Xbox pads will not rumble.\n");
      return false;
   }
   return true;
}

/* XInput's buttons, in the order raw input numbers an Xbox pad's:
 * A B X Y, the shoulders, Back and Start, the sticks. */
static int winraw_xinput_buttons(WORD b)
{
   return  ((b & 0x1000) ? 0x001 : 0) | ((b & 0x2000) ? 0x002 : 0)
         | ((b & 0x4000) ? 0x004 : 0) | ((b & 0x8000) ? 0x008 : 0)
         | ((b & 0x0100) ? 0x010 : 0) | ((b & 0x0200) ? 0x020 : 0)
         | ((b & 0x0020) ? 0x040 : 0) | ((b & 0x0010) ? 0x080 : 0)
         | ((b & 0x0040) ? 0x100 : 0) | ((b & 0x0080) ? 0x200 : 0);
}

/* The thread's: which of XInput's pads the pad in @slot is, worked
 * out as far as it can be now. True if it is known. */
static bool winraw_xinput_find(unsigned slot, winraw_joypad_out_dev_t *dev)
{
   unsigned i, free_pads = 0, unknown = 0;
   int only   = -1, match = -1, matches = 0;
   int mine   = retro_atomic_load_acquire_int(&winraw_joypad_out[slot].buttons);

   if (dev->xuser >= 0)
      return true;

   /* how many of this driver's XInput pads are not yet told apart */
   for (i = 0; i < MAX_USERS; i++)
      if (     winraw_joypad_out[i].dev
            && winraw_joypad_out[i].dev->xinput
            && winraw_joypad_out[i].dev->xuser < 0)
         unknown++;

   for (i = 0; i < WINRAW_XINPUT_PADS; i++)
   {
      winraw_xinput_state_t st;
      if (winraw_xinput_taken[i] >= 0)
         continue;
      memset(&st, 0, sizeof(st));
      if (winraw_xinput_get_state(i, &st) != ERROR_SUCCESS)
         continue;
      free_pads++;
      only = (int)i;
      if (mine && winraw_xinput_buttons(st.buttons) == mine)
      {
         match = (int)i;
         matches++;
      }
   }

   if (free_pads == 1 && unknown == 1)
      dev->xuser = only;
   else if (matches == 1)
      dev->xuser = match;
   if (dev->xuser < 0)
      return false;
   winraw_xinput_taken[dev->xuser] = (int)slot;
   RARCH_LOG("[RawInput Joypad] Rumble: the controller in slot %u is"
         " XInput's pad %d.\n", slot, dev->xuser + 1);
   return true;
}

/* The thread's: an XInput pad's motors are set. False if it could not
 * be done - the pad is not known yet, or has gone. */
static bool winraw_xinput_write(unsigned slot, winraw_joypad_out_dev_t *dev,
      int want)
{
   winraw_xinput_vibration_t v;

   if (!winraw_xinput_load() || !winraw_xinput_find(slot, dev))
      return false;
   v.left_motor  = (WORD)((unsigned)want & 0xFFFF);
   v.right_motor = (WORD)(((unsigned)want >> 16) & 0xFFFF);
   if (winraw_xinput_set_state((DWORD)dev->xuser, &v) == ERROR_SUCCESS)
      return true;
   /* XInput no longer has that pad: it is looked for again */
   winraw_xinput_taken[dev->xuser] = -1;
   dev->xuser                      = -1;
   return false;
}

static bool winraw_joypad_out_write(const winraw_joypad_out_dev_t *dev,
      int want, int player)
{
   uint8_t buf[1024];
   OVERLAPPED ov;
   DWORD written = 0;
   bool ok       = false;
   size_t report = sony_pad_output_report(buf, sizeof(buf),
         (enum sony_pad_model)dev->model, dev->bluetooth, dev->v2,
         (uint8_t)(((unsigned)want & 0xFFFF) >> 8),
         (uint8_t)((((unsigned)want >> 16) & 0xFFFF) >> 8),
         player);
   size_t len    = report;

   if (!report)
      return false;
   /* Windows takes a write of the length the device's descriptor
    * gives its longest output report, no shorter; the rest is zeros */
   if (dev->report_len > len && dev->report_len <= sizeof(buf))
      len = dev->report_len;

   /* The device is opened for overlapped writes so that one to a pad
    * that has gone quiet - out of range, its battery flat - is given
    * up after a while and not waited on for as long as Windows
    * would: the other pads' writes, and the driver stopping, wait
    * behind it. */
   memset(&ov, 0, sizeof(ov));
   ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
   if (ov.hEvent)
   {
      if (WriteFile(dev->handle, buf, (DWORD)len, &written, &ov))
         ok = true;
      else if (GetLastError() == ERROR_IO_PENDING)
      {
         if (WaitForSingleObject(ov.hEvent, WINRAW_JOYPAD_OUT_TIMEOUT_MS)
               != WAIT_OBJECT_0)
            CancelIo(dev->handle);
         ok = GetOverlappedResult(dev->handle, &ov, &written, TRUE) != 0;
      }
      CloseHandle(ov.hEvent);
   }
   if (ok)
      return true;
   /* some links take an output report only as a control request, and
    * that one at the report's own length */
   return HidD_SetOutputReport(dev->handle, buf, (ULONG)report) != 0;
}

/* A device that is no longer to be written to is closed, by whichever
 * side holds it. */
static void winraw_joypad_out_dev_free(winraw_joypad_out_dev_t *dev)
{
   if (!dev)
      return;
   if (!dev->xinput)
      CloseHandle(dev->handle);
   free(dev);
}

/* The thread's: the device it holds for a pad is stilled, if it was
 * running, and closed. */
static void winraw_joypad_out_drop(winraw_joypad_out_t *out)
{
   if (out->dev)
   {
      if (out->dev->xinput)
      {
         if (out->dev->xuser >= 0)
         {
            if (out->sent)
               winraw_xinput_write((unsigned)(out - winraw_joypad_out),
                     out->dev, 0);
            /* (a write that failed has let the pad go already) */
            if (out->dev->xuser >= 0)
               winraw_xinput_taken[out->dev->xuser] = -1;
         }
      }
      else if (out->sent)
         winraw_joypad_out_write(out->dev, 0, out->sent_player);
      winraw_joypad_out_dev_free(out->dev);
   }
   out->dev         = NULL;
   out->sent        = 0;
   out->sent_player = 0;
   out->failed      = false;
}

/* On the writer's thread, after each wake; @quit for the last call. */
static void winraw_joypad_out_run(void *unused, bool quit)
{
   unsigned i;

   (void)unused;
   for (i = 0; i < MAX_USERS; i++)
   {
      winraw_joypad_out_t *out = &winraw_joypad_out[i];
      void *msg = retro_atomic_exchange_ptr(&out->inbox, NULL);

      /* a device to take over, or word that the pad has gone:
       * either way the one held is done with */
      if (msg)
      {
         winraw_joypad_out_drop(out);
         if (msg != WINRAW_JOYPAD_OUT_CLOSE)
            out->dev = (winraw_joypad_out_dev_t*)msg;
      }

      if (out->dev && !quit)
      {
         int want   = retro_atomic_load_acquire_int(&out->want);
         int player = retro_atomic_load_acquire_int(&out->player);
         /* (an XInput pad's lights are XInput's) */
         if (     want != out->sent
               || (player != out->sent_player && !out->dev->xinput))
         {
            if (out->dev->xinput)
            {
               /* Not written while it is not known which of
                * XInput's pads this is: what is wanted stays
                * wanted, and is tried again at the next change. */
               if (winraw_xinput_write(i, out->dev, want))
               {
                  winraw_joypad_out_writes++;
                  out->sent = want;
               }
            }
            else
            {
               if (winraw_joypad_out_write(out->dev, want, player))
                  winraw_joypad_out_writes++;
               else if (!out->failed)
               {
                  out->failed = true;
                  RARCH_WARN("[RawInput Joypad] Rumble: the write to the"
                        " controller in slot %u failed (error %lu).\n",
                        i, (unsigned long)GetLastError());
               }
               out->sent        = want;
               out->sent_player = player;
            }
         }
      }
      if (quit)
         winraw_joypad_out_drop(out);
   }
}

/* A pad has gone, or its slot is to hold another: the thread is told
 * to close what it holds for it. Nothing is waited for. */
static void winraw_joypad_out_close(unsigned slot)
{
   void *old;
   winraw_joypad_out_t *out = &winraw_joypad_out[slot];

   if (!out->present)
      return;
   out->present = false;
   out->xinput  = false;
   old = retro_atomic_exchange_ptr(&out->inbox, WINRAW_JOYPAD_OUT_CLOSE);
   /* a device handed over and never taken is still the driver's */
   if (old && old != WINRAW_JOYPAD_OUT_CLOSE)
      winraw_joypad_out_dev_free((winraw_joypad_out_dev_t*)old);
   retro_atomic_store_release_int(&out->want, 0);
   retro_atomic_store_release_int(&out->player, 0);
   out->player_asked = 0;
   input_output_writer_wake(winraw_joypad_out_writer);
}

/* The thread is started, if it is not running. */
static bool winraw_joypad_out_start(void)
{
   if (!winraw_joypad_out_writer)
      winraw_joypad_out_writer = input_output_writer_new(
            winraw_joypad_out_run, NULL);
   return winraw_joypad_out_writer != NULL;
}

/* A pad has arrived: if it is one there is a rumble report for, its
 * device is opened for writing and handed to the thread. */
static void winraw_joypad_out_open(unsigned slot,
      uint16_t vid, uint16_t pid, const char *path, USHORT report_len)
{
   void *old;
   winraw_joypad_out_dev_t *dev;
   winraw_joypad_out_t *out  = &winraw_joypad_out[slot];
   enum sony_pad_model model = sony_pad_model(vid, pid);
   /* Windows marks the HID device of a pad it drives through XInput
    * with "IG_" in its path */
   bool xinput               = path
      && (strstr(path, "IG_") || strstr(path, "ig_"));

   if ((model == SONY_PAD_NONE && !xinput) || !path || !*path)
      return;
   if (!(dev = (winraw_joypad_out_dev_t*)calloc(1, sizeof(*dev))))
      return;

   if (xinput)
   {
      /* nothing to open: the thread finds which of XInput's it is */
      dev->xinput = true;
      dev->xuser  = -1;
   }
   else
   {
      dev->handle = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED, NULL);
      if (dev->handle == INVALID_HANDLE_VALUE || !dev->handle)
      {
         RARCH_LOG("[RawInput Joypad] Rumble: the controller in slot %u could"
               " not be opened for writing (error %lu); it will not rumble.\n",
               slot, (unsigned long)GetLastError());
         free(dev);
         return;
      }
      dev->model      = (uint8_t)model;
      dev->report_len = report_len;
      dev->bluetooth  =
            strstr(path, "00001124-0000-1000-8000-00805f9b34fb") != NULL
         || strstr(path, "00001124-0000-1000-8000-00805F9B34FB") != NULL;
   }

   if (!xinput && model == SONY_PAD_DUALSENSE)
   {
      /* the newer way of the motors: the Edge has it; a DualSense
       * from firmware 2.21, which its feature report 0x20 tells */
      dev->v2 = sony_pad_dualsense_is_edge(pid);
      if (!dev->v2)
      {
         uint8_t feature[128];
         memset(feature, 0, sizeof(feature));
         feature[0] = 0x20;
         if (HidD_GetFeature(dev->handle, feature, 64))
            dev->v2 = (unsigned)(feature[44] | (feature[45] << 8)) > 0x0215;
      }
   }

   if (!winraw_joypad_out_start())
   {
      winraw_joypad_out_dev_free(dev);
      return;
   }

   if (xinput)
      RARCH_LOG("[RawInput Joypad] Rumble: the controller in slot %u is an"
            " XInput pad; it is rumbled through XInput, from the driver's"
            " own thread.\n", slot);
   else
      RARCH_LOG("[RawInput Joypad] Rumble: the controller in slot %u is a %s"
            " over %s; it is written to from the driver's own thread.\n", slot,
            model == SONY_PAD_DS4 ? "DualShock 4"
            : (dev->v2 ? "DualSense (newer motor control)" : "DualSense"),
            dev->bluetooth ? "Bluetooth" : "USB");

   /* handed over: the thread closes whatever it held for this slot
    * and takes this one. One handed over before and not yet taken is
    * still the driver's to close. */
   retro_atomic_store_release_int(&out->want, 0);
   retro_atomic_store_release_int(&out->buttons, 0);
   old = retro_atomic_exchange_ptr(&out->inbox, dev);
   if (old && old != WINRAW_JOYPAD_OUT_CLOSE)
      winraw_joypad_out_dev_free((winraw_joypad_out_dev_t*)old);
   out->present = true;
   out->xinput  = xinput;
   input_output_writer_wake(winraw_joypad_out_writer);
}

/* A pad read through XInput: which of XInput's it is is known. */
static void winraw_joypad_out_open_xinput(unsigned slot, int xuser)
{
   void *old;
   winraw_joypad_out_t *out = &winraw_joypad_out[slot];
   winraw_joypad_out_dev_t *dev = (winraw_joypad_out_dev_t*)
      calloc(1, sizeof(*dev));

   if (!dev || !winraw_joypad_out_start())
   {
      free(dev);
      return;
   }
   dev->xinput = true;
   dev->xuser  = xuser;
   retro_atomic_store_release_int(&out->want, 0);
   old = retro_atomic_exchange_ptr(&out->inbox, dev);
   if (old && old != WINRAW_JOYPAD_OUT_CLOSE)
      winraw_joypad_out_dev_free((winraw_joypad_out_dev_t*)old);
   out->present = true;
   out->xinput  = false; /* nothing to tell apart: no buttons published */
   input_output_writer_wake(winraw_joypad_out_writer);
}

/* The driver is going: the thread is stopped, which stills and closes
 * every device it holds; one it never took is closed here. */
static void winraw_joypad_out_stop(void)
{
   unsigned i;

   input_output_writer_free(winraw_joypad_out_writer);
   winraw_joypad_out_writer = NULL;
   for (i = 0; i < MAX_USERS; i++)
   {
      winraw_joypad_out_t *out = &winraw_joypad_out[i];
      void *old = retro_atomic_exchange_ptr(&out->inbox, NULL);
      if (old && old != WINRAW_JOYPAD_OUT_CLOSE)
         winraw_joypad_out_dev_free((winraw_joypad_out_dev_t*)old);
      out->present = false;
      out->xinput  = false;
      retro_atomic_store_release_int(&out->want, 0);
   }
   /* the thread has ended: what was its own is put away */
   if (winraw_xinput_dll)
      FreeLibrary(winraw_xinput_dll);
   winraw_xinput_dll       = NULL;
   winraw_xinput_get_state = NULL;
   winraw_xinput_set_state = NULL;
   winraw_xinput_tried     = false;
   if (winraw_joypad_out_writes)
      RARCH_DBG("[RawInput Joypad] Rumble: %lu report(s) written.\n",
            winraw_joypad_out_writes);
   winraw_joypad_out_writes = 0;
}

static bool winraw_joypad_joypad_set_rumble(unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   int want;
   winraw_joypad_out_t *out;

   if (port >= MAX_USERS)
      return false;
   out = &winraw_joypad_out[port];
   if (!out->present || !winraw_joypad_out_writer)
      return false;

   /* one writer - the frontend's thread - so load, change, store */
   want = retro_atomic_load_relaxed_int(&out->want);
   if (effect == RETRO_RUMBLE_STRONG)
      want = (int)(((unsigned)want & 0xFFFF0000u) | (unsigned)strength);
   else
      want = (int)(((unsigned)want & 0xFFFFu) | ((unsigned)strength << 16));
   retro_atomic_store_release_int(&out->want, want);
   input_output_writer_wake(winraw_joypad_out_writer);
   return true;
}

static int winraw_joypad_find_pad(HANDLE hDevice)
{
   unsigned i;
   for (i = 0; i < MAX_USERS; i++)
   {
      if (winraw_joypad_pads[i].connected 
       && winraw_joypad_pads[i].hDevice == hDevice)
         return (int)i;
   }
   return -1;
}

/* Find a free slot.  Returns index or -1. */
static int winraw_joypad_find_free_slot(void)
{
   unsigned i;
   for (i = 0; i < MAX_USERS; i++)
   {
      if (!winraw_joypad_pads[i].connected)
         return (int)i;
   }
   return -1;
}

/* ------------------------------------------------------------------ */
/* Xbox pads read through XInput                                       */
/* ------------------------------------------------------------------ */

/* With "XInput for Xbox Controllers" on, a pad Windows drives through
 * XInput is not taken from raw input, where its triggers are one
 * axis; XInput's pads are listed instead, with the XInput driver's
 * buttons, axes and name, so its autoconfig profiles apply. */
static bool                      winraw_joypad_xinput_own;
static HMODULE                   winraw_joypad_xi_dll;
static winraw_xinput_get_state_t winraw_joypad_xi_get_state;

static bool winraw_joypad_path_is_xinput(const char *path)
{
   return path && (strstr(path, "IG_") || strstr(path, "ig_"));
}

/* pad_count is the highest connected slot + 1: left to grow, stale
 * slots would look in use to autoconfig during a reconnect. */
static void winraw_joypad_recount(void)
{
   unsigned i;
   unsigned new_count = 0;
   for (i = 0; i < MAX_USERS; i++)
      if (winraw_joypad_pads[i].connected)
         new_count = i + 1;
   winraw_joypad_pad_count = new_count;
}

static void winraw_joypad_xinput_load(void)
{
   static const char *names[] = {
      "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
   unsigned i;

   for (i = 0; i < ARRAY_SIZE(names) && !winraw_joypad_xi_dll; i++)
      winraw_joypad_xi_dll = LoadLibraryA(names[i]);
   if (winraw_joypad_xi_dll)
      winraw_joypad_xi_get_state = (winraw_xinput_get_state_t)
         GetProcAddress(winraw_joypad_xi_dll, "XInputGetState");
   if (!winraw_joypad_xi_get_state)
   {
      RARCH_WARN("[RawInput Joypad] XInput could not be loaded; Xbox"
            " controllers are read through raw input.\n");
      winraw_joypad_xinput_own = false;
   }
}

static void winraw_joypad_xinput_fill(winraw_joypad_joypad_data_t *pad,
      const winraw_xinput_state_t *st)
{
   /* the XInput driver's order: A B X Y LB RB Start Back L3 R3 */
   static const WORD bits[10] = { 0x1000, 0x2000, 0x4000, 0x8000,
      0x0100, 0x0200, 0x0010, 0x0020, 0x0040, 0x0080 };
   unsigned i;

   for (i = 0; i < 10; i++)
      pad->buttons[i] = (st->buttons & bits[i]) != 0;
   pad->hats[0] = (uint8_t)(
           ((st->buttons & 0x0001) ? (1 << 0) : 0)
         | ((st->buttons & 0x0002) ? (1 << 1) : 0)
         | ((st->buttons & 0x0004) ? (1 << 2) : 0)
         | ((st->buttons & 0x0008) ? (1 << 3) : 0));
   pad->axes[0] = st->lx == -32768 ? -32767 : st->lx;
   pad->axes[1] = st->ly == -32768 ? -32767 : st->ly;
   pad->axes[2] = st->rx == -32768 ? -32767 : st->rx;
   pad->axes[3] = st->ry == -32768 ? -32767 : st->ry;
   pad->axes[4] = (int16_t)(st->left_trigger  * 32767 / 255);
   pad->axes[5] = (int16_t)(st->right_trigger * 32767 / 255);
}

/* XInput's four pads are looked at: one that has come is given a
 * slot, one that has gone gives its slot up. Called when the driver
 * starts and when raw input reports a device coming or going - asking
 * XInput about a pad that is not there is slow, and is not done on
 * every poll. */
static void winraw_joypad_xinput_rescan(void)
{
   unsigned u, i;

   if (!winraw_joypad_xinput_own)
      return;
   for (u = 0; u < WINRAW_XINPUT_PADS; u++)
   {
      winraw_xinput_state_t st;
      winraw_joypad_joypad_data_t *pad = NULL;
      bool there;

      memset(&st, 0, sizeof(st));
      there = winraw_joypad_xi_get_state(u, &st) == ERROR_SUCCESS;
      for (i = 0; i < MAX_USERS; i++)
         if (     winraw_joypad_pads[i].connected
               && winraw_joypad_pads[i].xuser == (int8_t)u)
            pad = &winraw_joypad_pads[i];

      if (there && !pad)
      {
         int slot = winraw_joypad_find_free_slot();
         if (slot < 0)
            continue;
         pad = &winraw_joypad_pads[slot];
         memset(pad, 0, sizeof(*pad));
         pad->xuser       = (int8_t)u;
         pad->num_buttons = 10;
         pad->num_axes    = 6;
         pad->num_hats    = 1;
         strlcpy(pad->name, "XInput Controller", sizeof(pad->name));
         winraw_joypad_xinput_fill(pad, &st);
         pad->connected   = true;
         winraw_joypad_recount();
         RARCH_LOG("[RawInput Joypad] XInput pad %u connected in slot %d.\n",
               u + 1, slot);
         input_autoconfigure_connect(pad->name, NULL, NULL, "xinput",
               (unsigned)slot, 0, 0);
         winraw_joypad_out_open_xinput((unsigned)slot, (int)u);
      }
      else if (!there && pad)
      {
         unsigned slot = (unsigned)(pad - winraw_joypad_pads);
         RARCH_LOG("[RawInput Joypad] XInput pad %u removed from slot %u.\n",
               u + 1, slot);
         input_autoconfigure_disconnect(slot, pad->name);
         winraw_joypad_out_close(slot);
         memset(pad, 0, sizeof(*pad));
         winraw_joypad_recount();
      }
   }
}

/* Once a poll: the state of each XInput pad that is there. */
static void winraw_joypad_xinput_poll(void)
{
   unsigned i;
   bool gone = false;

   if (!winraw_joypad_xinput_own)
      return;
   for (i = 0; i < winraw_joypad_pad_count; i++)
   {
      winraw_xinput_state_t st;
      winraw_joypad_joypad_data_t *pad = &winraw_joypad_pads[i];
      if (!pad->connected || pad->xuser < 0)
         continue;
      if (winraw_joypad_xi_get_state((DWORD)pad->xuser, &st) == ERROR_SUCCESS)
         winraw_joypad_xinput_fill(pad, &st);
      else
         gone = true;
   }
   if (gone)
      winraw_joypad_xinput_rescan();
}

/* ------------------------------------------------------------------ */
/* Device arrival / removal                                            */
/* ------------------------------------------------------------------ */

/* Whether a raw input handle still names a device. The handle of a
 * controller that has been unplugged does not. */
static bool winraw_joypad_handle_alive(HANDLE hDevice)
{
   RID_DEVICE_INFO info;
   UINT size   = sizeof(info);
   info.cbSize = sizeof(info);
   return GetRawInputDeviceInfoA(hDevice, RIDI_DEVICEINFO,
         &info, &size) != (UINT)-1;
}

static bool winraw_joypad_add_device(HANDLE hDevice)
{
   int slot;
   char device_path[512];
   RID_DEVICE_INFO dev_info;
   UINT name_size                   = 0;
   UINT preparsed_size              = 0;
   UINT dev_info_size               = sizeof(dev_info);
   HANDLE hid_handle                = INVALID_HANDLE_VALUE;
   wchar_t product_string[256]      = {0};
   winraw_joypad_joypad_data_t *pad = NULL;

   /* Already tracked? */
   if (winraw_joypad_find_pad(hDevice) >= 0)
      return true;

   /* Get device info to check it is a HID gamepad/joystick */
   dev_info.cbSize = sizeof(dev_info);
   if (GetRawInputDeviceInfoA(hDevice, RIDI_DEVICEINFO,
            &dev_info, &dev_info_size) == (UINT)-1)
      return false;

   if (dev_info.dwType != RIM_TYPEHID)
      return false;

   /* Only accept generic joystick or gamepad usage pages */
   if (dev_info.hid.usUsagePage != HID_USAGE_PAGE_GENERIC)
      return false;
   if (   dev_info.hid.usUsage != HID_USAGE_GENERIC_JOYSTICK
       && dev_info.hid.usUsage != HID_USAGE_GENERIC_GAMEPAD)
      return false;

   /* The device path, which says which physical controller this is. */
   device_path[0] = '\0';
   GetRawInputDeviceInfoA(hDevice, RIDI_DEVICENAME, NULL, &name_size);
   if (name_size > 0 && name_size < sizeof(device_path))
   {
      if (GetRawInputDeviceInfoA(hDevice, RIDI_DEVICENAME,
               device_path, &name_size) == (UINT)-1)
         device_path[0] = '\0';
   }

   /* XInput's to read, not raw input's: XInput's pads are looked at
    * again, this one among them. */
   if (     winraw_joypad_xinput_own
         && winraw_joypad_path_is_xinput(device_path))
   {
      winraw_joypad_xinput_rescan();
      return false;
   }

   /* ---- Handle reconnection race condition ----
    * Windows can send GIDC_ARRIVAL for a newly-assigned handle *before*
    * GIDC_REMOVAL for the old handle of the same physical device.
    * If we find an existing slot holding that same device under its
    * old, now-stale handle, evict it first to prevent double-registered
    * inputs from two slots reading the same physical controller.
    * We also try to reuse the same slot so that port mapping is
    * preserved across disconnect/reconnect cycles.
    *
    * "The same device" used to be judged by vendor and product ID
    * alone, so a second controller of the same model was taken for the
    * first one reconnecting and evicted it: two identical controllers
    * could not be used together. A slot with the same IDs is the same
    * device only if it has the same device path, or if its handle no
    * longer names a device - it was unplugged, and this is it, or its
    * replacement, coming back. A slot whose handle is still alive and
    * whose path is another is another controller, and is left alone. */
   {
      unsigned i;
      int reuse_slot = -1;
      for (i = 0; i < MAX_USERS; i++)
      {
         winraw_joypad_joypad_data_t *p = &winraw_joypad_pads[i];
         if (   p->connected
             && p->vid == (uint16_t)dev_info.hid.dwVendorId
             && p->pid == (uint16_t)dev_info.hid.dwProductId
             && p->hDevice != hDevice)
         {
            bool same_path = device_path[0] && p->path[0]
               && !lstrcmpiA(p->path, device_path);

            if (!same_path && winraw_joypad_handle_alive(p->hDevice))
               continue; /* another controller of the same model */

            /* Stale entry for the same physical device — clean it up */
            RARCH_LOG("[RawInput Joypad] Evicting stale slot %d "
                  "(handle %p -> %p) for reconnected device "
                  "VID:%04X PID:%04X.\n",
                  (int)i, p->hDevice, hDevice,
                  p->vid, p->pid);

            /* Free resources but DON'T fire autoconfigure disconnect
             * because we'll reuse the same slot and keep the bindings. */
            if (p->btn_caps)
               free(p->btn_caps);
            if (p->val_caps)
               free(p->val_caps);
            if (p->preparsed)
               free(p->preparsed);

            memset(p, 0, sizeof(*p));
            winraw_joypad_held[i].count = 0;
            reuse_slot = (int)i;
            break; /* Only one stale entry per device expected */
         }
      }

      slot = (reuse_slot >= 0) ? reuse_slot : winraw_joypad_find_free_slot();
   }
   if (slot < 0)
   {
      RARCH_WARN("[RawInput Joypad] No free pad slots, ignoring device.\n");
      return false;
   }

   pad = &winraw_joypad_pads[slot];
   memset(pad, 0, sizeof(*pad));

   pad->hDevice = hDevice;
   pad->vid     = (uint16_t)dev_info.hid.dwVendorId;
   pad->pid     = (uint16_t)dev_info.hid.dwProductId;

   strlcpy(pad->path, device_path, sizeof(pad->path));
   pad->xuser = -1;

   /* --- Open the device path for the product string --- */
   if (device_path[0])
   {
      hid_handle = CreateFileA(device_path,
            0, /* No read/write needed, just attributes */
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, 0, NULL);

      if (hid_handle != INVALID_HANDLE_VALUE)
      {
         /* Convert wide string to UTF-8 */
         if (HidD_GetProductString(hid_handle,
             product_string, sizeof(product_string)))
            WideCharToMultiByte(CP_UTF8, 0, product_string, -1,
                  pad->name, sizeof(pad->name), NULL, NULL);
         CloseHandle(hid_handle);
      }
   }

   if (pad->name[0] == '\0')
      snprintf(pad->name, sizeof(pad->name),
            "RawInput Pad (VID:%04X PID:%04X)", pad->vid, pad->pid);

   /* --- Get preparsed data (describes HID report layout) --- */
   GetRawInputDeviceInfoA(hDevice, RIDI_PREPARSEDDATA, NULL, &preparsed_size);
   if (preparsed_size == 0)
   {
      RARCH_ERR("[RawInput Joypad] Could not get preparsed data size for %s.\n", pad->name);
      return false;
   }

   pad->preparsed = (PHIDP_PREPARSED_DATA)calloc(1, preparsed_size);
   if (!pad->preparsed)
      return false;

   if (GetRawInputDeviceInfoA(hDevice, RIDI_PREPARSEDDATA,
            pad->preparsed, &preparsed_size) == (UINT)-1)
   {
      free(pad->preparsed);
      pad->preparsed = NULL;
      return false;
   }

   /* Parse capabilities */
   if (HidP_GetCaps(pad->preparsed, &pad->caps) != HIDP_STATUS_SUCCESS)
   {
      free(pad->preparsed);
      pad->preparsed = NULL;
      return false;
   }

   /* --- Button capabilities --- */
   if (pad->caps.NumberInputButtonCaps > 0)
   {
      USHORT num_btn_caps = pad->caps.NumberInputButtonCaps;
      pad->btn_caps = (HIDP_BUTTON_CAPS*)calloc(num_btn_caps,
         sizeof(HIDP_BUTTON_CAPS));
      if (pad->btn_caps)
      {
         if (HidP_GetButtonCaps(HidP_Input, pad->btn_caps, &num_btn_caps,
                  pad->preparsed) == HIDP_STATUS_SUCCESS)
         {
            unsigned i;
            unsigned total_buttons = 0;
            for (i = 0; i < num_btn_caps; i++)
            {
               if (pad->btn_caps[i].IsRange)
               {
                  unsigned cnt = pad->btn_caps[i].Range.UsageMax
                               - pad->btn_caps[i].Range.UsageMin + 1;
                  if (i == 0)
                  {
                     pad->btn_usage_min = pad->btn_caps[i].Range.UsageMin;
                     pad->btn_usage_max = pad->btn_caps[i].Range.UsageMax;
                  }
                  total_buttons += cnt;
               }
               else
               {
                  if (i == 0)
                  {
                     pad->btn_usage_min = pad->btn_caps[i].NotRange.Usage;
                     pad->btn_usage_max = pad->btn_caps[i].NotRange.Usage;
                  }
                  total_buttons++;
               }
            }
            pad->num_buttons = (uint16_t)MIN(total_buttons,
               RAWINPUT_MAX_BUTTONS);
         }
      }
   }

   /* --- Value (axis/hat) capabilities --- */
   if (pad->caps.NumberInputValueCaps > 0)
   {
      USHORT num_val_caps = pad->caps.NumberInputValueCaps;
      pad->val_caps = (HIDP_VALUE_CAPS*)calloc(num_val_caps,
         sizeof(HIDP_VALUE_CAPS));
      if (pad->val_caps)
      {
         if (HidP_GetValueCaps(HidP_Input, pad->val_caps, &num_val_caps,
                  pad->preparsed) == HIDP_STATUS_SUCCESS)
         {
            unsigned i;
            unsigned num_axes = 0;
            unsigned sliders  = 0;
            unsigned hat_idx  = 0;
            for (i = 0; i < num_val_caps; i++)
            {
               USAGE usage = pad->val_caps[i].IsRange
                           ? pad->val_caps[i].Range.UsageMin
                           : pad->val_caps[i].NotRange.Usage;

               if (usage == HID_USAGE_GENERIC_HATSWITCH)
               {
                  if (hat_idx < RAWINPUT_MAX_HATS)
                     hat_idx++;
               }
               else if (winraw_joypad_is_axis_usage(&pad->val_caps[i]))
               {
                  int slot_idx = winraw_joypad_axis_slot(usage, &sliders);
                  if (slot_idx >= 0 && (unsigned)slot_idx >= num_axes)
                     num_axes = (unsigned)slot_idx + 1;
               }
               /* else: unknown/vendor value cap — skip */
            }
            pad->num_axes = (uint16_t)num_axes;
            pad->num_hats = (uint16_t)hat_idx;
         }
      }
   }

   pad->connected = true;
   if ((unsigned)(slot + 1) > winraw_joypad_pad_count)
      winraw_joypad_pad_count = (unsigned)(slot + 1);

   RARCH_LOG("[RawInput Joypad] Device connected in slot %d: \"%s\" "
         "(VID:%04X PID:%04X) buttons:%u axes:%u hats:%u.\n",
         slot, pad->name, pad->vid, pad->pid,
         pad->num_buttons, pad->num_axes, pad->num_hats);

   /* Fire autoconfig task */
   input_autoconfigure_connect(pad->name, NULL, NULL, "winraw",
         (unsigned)slot, pad->vid, pad->pid);

   /* a pad whose motion sensors can be read out of its reports; what
    * a core asked of this port before - the pad may be one coming
    * back - still holds */
   pad->motion_model = (uint8_t)sony_pad_model(pad->vid, pad->pid);
   pad->motion_on    = (pad->motion_model != SONY_PAD_NONE)
      ? winraw_joypad_motion_want[slot] : 0;
   pad->motion_seen  = false;

   /* a pad there is a rumble report for is opened for writing */
   winraw_joypad_out_close((unsigned)slot);
   winraw_joypad_out_open((unsigned)slot, pad->vid, pad->pid, pad->path,
         pad->caps.OutputReportByteLength);

   return true;
}

static void winraw_joypad_remove_device(HANDLE hDevice)
{
   int slot = winraw_joypad_find_pad(hDevice);
   if (slot < 0)
   {
      /* not one of raw input's pads: it may have been XInput's */
      winraw_joypad_xinput_rescan();
      return;
   }

   {
      winraw_joypad_joypad_data_t *pad = &winraw_joypad_pads[slot];

      RARCH_LOG("[RawInput Joypad] Device removed from slot %d: \"%s\".\n",
            slot, pad->name);

      input_autoconfigure_disconnect((unsigned)slot, pad->name);
      winraw_joypad_out_close((unsigned)slot);

      if (pad->btn_caps)
         free(pad->btn_caps);
      if (pad->val_caps)
         free(pad->val_caps);
      if (pad->preparsed)
         free(pad->preparsed);

      memset(pad, 0, sizeof(*pad));
      /* pad->connected is now false from the memset */
      winraw_joypad_held[slot].count = 0;
   }

   winraw_joypad_recount();
}

/* ------------------------------------------------------------------ */
/* Raw-input report parsing                                            */
/* ------------------------------------------------------------------ */

/* An XInput pad's buttons are published for the rumble thread, which
 * tells by them which of XInput's pads this one is: the first ten,
 * which are XInput's ten. */
static void winraw_joypad_out_publish_buttons(
      const winraw_joypad_joypad_data_t *pad)
{
   unsigned b;
   int mask      = 0;
   unsigned slot = (unsigned)(pad - winraw_joypad_pads);

   if (slot >= MAX_USERS || !winraw_joypad_out[slot].xinput)
      return;
   for (b = 0; b < 10 && b < pad->num_buttons; b++)
      if (pad->buttons[b])
         mask |= 1 << b;
   retro_atomic_store_release_int(&winraw_joypad_out[slot].buttons, mask);
}

static void winraw_joypad_parse_hid_report(winraw_joypad_joypad_data_t *pad,
      const BYTE *raw_data, DWORD raw_data_size)
{
   ULONG   usage_count;
   USAGE   usages[RAWINPUT_MAX_BUTTONS];
   unsigned i;
   unsigned num_buttons;

   if (!pad || !pad->preparsed || !raw_data || raw_data_size == 0)
      return;

   winraw_joypad_reports_parsed++;
   num_buttons = pad->num_buttons;

   /* --- Buttons --- */
   /* Clear only the buttons this device actually has, not the full 128 */
   if (num_buttons > 0)
      memset(pad->buttons, 0, num_buttons * sizeof(pad->buttons[0]));

   /* Hats are cleared for the same reason as buttons: a hat whose
    * usage is absent from this report must read as centred rather
    * than retaining the direction from the previous one. Zero is
    * the centred bitmask here (see winraw_joypad_hat_value_to_bitmask),
    * not a direction. Axes are deliberately not cleared - a missing
    * axis reads as its neutral scaled value only if it was centred,
    * and zeroing them would fight sticks that legitimately rest
    * off-centre. */
   if (pad->num_hats > 0)
      memset(pad->hats, 0, pad->num_hats * sizeof(pad->hats[0]));

   usage_count = (ULONG)num_buttons;
   if (usage_count > RAWINPUT_MAX_BUTTONS)
      usage_count = RAWINPUT_MAX_BUTTONS;

   if (   pad->caps.NumberInputButtonCaps > 0
       && HidP_GetUsages(HidP_Input,
             pad->btn_caps[0].UsagePage,
             0, /* Link collection */
             usages, &usage_count,
             pad->preparsed,
             (PCHAR)raw_data, raw_data_size) == HIDP_STATUS_SUCCESS)
   {
      USAGE btn_min = pad->btn_usage_min;
      for (i = 0; i < usage_count; i++)
      {
         unsigned btn_index = usages[i] - btn_min;
         if (btn_index < num_buttons)
            pad->buttons[btn_index] = true;
      }
   }

   /* --- Values (axes & hats) --- */
   if (pad->val_caps)
   {
      USHORT num_val_caps = pad->caps.NumberInputValueCaps;
      unsigned sliders    = 0;
      unsigned hat_idx    = 0;
      unsigned max_axes   = pad->num_axes;
      unsigned max_hats   = pad->num_hats;

      for (i = 0; i < num_val_caps; i++)
      {
         ULONG value = 0;
         USAGE usage = pad->val_caps[i].IsRange
                     ? pad->val_caps[i].Range.UsageMin
                     : pad->val_caps[i].NotRange.Usage;
         bool  is_hat  = (usage == HID_USAGE_GENERIC_HATSWITCH);
         bool  is_axis = !is_hat && winraw_joypad_is_axis_usage(&pad->val_caps[i]);
         int   slot_idx;

         /* Hats take their index from the position of this cap in
          * val_caps[], axes their DirectInput slot, both matching
          * how num_hats/num_axes were counted at enumeration time.
          * Deriving either from a running counter that only advances
          * on a successful read would shift every subsequent value
          * by one whenever a usage is missing from the current
          * report. */
         if (is_hat)
            slot_idx = (int)hat_idx++;
         else if (is_axis)
            slot_idx = winraw_joypad_axis_slot(usage, &sliders);
         else
            continue; /* unknown/vendor value cap */

         if (slot_idx < 0)
            continue;

         /* HidP_GetUsageValue() returns HIDP_STATUS_INCOMPATIBLE_REPORT_ID
          * when the arriving report does not carry this usage, which is
          * routine for devices that split their state across several
          * report IDs. Leave the (already cleared) entry alone. */
         if (HidP_GetUsageValue(HidP_Input,
                  pad->val_caps[i].UsagePage,
                  0, usage,
                  &value, pad->preparsed,
                  (PCHAR)raw_data, raw_data_size) != HIDP_STATUS_SUCCESS)
            continue;

         if (is_hat)
         {
            if ((unsigned)slot_idx < max_hats)
               pad->hats[slot_idx] = winraw_joypad_hat_value_to_bitmask(
                     (LONG)value,
                     pad->val_caps[i].LogicalMin,
                     pad->val_caps[i].LogicalMax);
         }
         else
         {
            if ((unsigned)slot_idx < max_axes)
            {
               LONG signed_value = (pad->val_caps[i].LogicalMin < 0)
                  ? winraw_joypad_sign_extend(value, pad->val_caps[i].BitSize)
                  : (LONG)value;
               pad->axes[slot_idx] = winraw_joypad_scale_axis(
                     signed_value,
                     pad->val_caps[i].LogicalMin,
                     pad->val_caps[i].LogicalMax);
            }
         }
      }
   }
   winraw_joypad_out_publish_buttons(pad);

   /* the motion sensors, while a core wants them */
   if (     pad->motion_on
         && sony_pad_motion_parse(&pad->motion,
               (enum sony_pad_model)pad->motion_model,
               (const uint8_t*)raw_data, (size_t)raw_data_size))
      pad->motion_seen = true;
}

/* Reports for one of this driver's controllers that were read in bulk
 * by the keyboard and mouse driver (winraw_input.c, "Read by the
 * poll"): a bulk read takes every raw input report waiting on its
 * thread, and this driver's with them.
 *
 * They are not parsed here. A controller sends its whole state in
 * every report, and one that reports a thousand times a second has
 * eight to seventeen of them waiting at each poll; parsing each is a
 * dozen calls into the HID parser, to write a state that the next
 * report overwrites before anything reads it. What the poll leaves
 * behind is the same whether every report is parsed or only the last,
 * so only the last is: it is kept here, and
 * winraw_joypad_parse_held() parses it once the read is done.
 *
 * "The last" is per report ID - the report's first byte - because a
 * device may split its state across several, and an axis that one ID
 * carries is not rewritten by a report with another. The newest of
 * each ID is kept, and they are parsed in the order their newest
 * arrived, which leaves buttons, hats and axes exactly as parsing
 * every report in turn would. A report too big to keep, or a fifth ID,
 * is not guessed about: what is held is parsed first, then it.
 *
 * @data is @count reports of @report_size bytes each. */
static void winraw_joypad_parse_held_slot(int slot)
{
   unsigned i;
   winraw_joypad_held_t *held = &winraw_joypad_held[slot];

   for (i = 0; i < held->count; i++)
   {
      unsigned k = held->order[i];
      winraw_joypad_parse_hid_report(&winraw_joypad_pads[slot],
            held->data[k], held->size[k]);
   }
   held->count = 0;
}

static void winraw_joypad_parse_held(void)
{
   unsigned slot;
   for (slot = 0; slot < MAX_USERS; slot++)
      if (winraw_joypad_held[slot].count)
         winraw_joypad_parse_held_slot((int)slot);
}

static void winraw_joypad_hold_report(int slot, const BYTE *report, DWORD size)
{
   unsigned i, k;
   winraw_joypad_held_t *held = &winraw_joypad_held[slot];

   if (size == 0)
      return;

   if (size > WINRAW_JOYPAD_HELD_BYTES)
   {
      winraw_joypad_parse_held_slot(slot);
      winraw_joypad_parse_hid_report(&winraw_joypad_pads[slot], report, size);
      return;
   }

   for (i = 0; i < held->count; i++)
      if (held->data[held->order[i]][0] == report[0])
         break;

   if (i < held->count)
   {
      /* this ID again: it takes the older one's place, and is now the
       * newest of them all */
      k = held->order[i];
      for (; i + 1 < held->count; i++)
         held->order[i] = held->order[i + 1];
      held->order[held->count - 1] = (uint8_t)k;
   }
   else
   {
      if (held->count == WINRAW_JOYPAD_HELD_IDS)
         winraw_joypad_parse_held_slot(slot);
      /* order[] holds 0..count-1 in some order, so count is free */
      k                            = held->count;
      held->order[held->count++]   = (uint8_t)k;
   }

   memcpy(held->data[k], report, size);
   held->size[k] = (uint16_t)size;
}

void winraw_joypad_take_hid(HANDLE device, const BYTE *data,
      DWORD report_size, DWORD count)
{
   DWORD i;
   int slot = winraw_joypad_find_pad(device);
   if (slot < 0 || report_size == 0)
      return;
   winraw_joypad_reports_taken += count;
   for (i = 0; i < count; i++)
      winraw_joypad_hold_report(slot, data + i * report_size, report_size);
}

/* ------------------------------------------------------------------ */
/* Message-only window for receiving WM_INPUT / WM_INPUT_DEVICE_CHANGE */
/* ------------------------------------------------------------------ */

static LRESULT CALLBACK winraw_joypad_joypad_wndproc(
      HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
   switch (msg)
   {
      case WM_INPUT:
      {
         int slot;
         UINT size = 0;
         RAWINPUT *raw;
         /* Stack buffer sized for typical gamepad HID reports.
          * Avoids the double GetRawInputData call in the common case. */
         BYTE stack_buf[256];

         size = sizeof(stack_buf);
         if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT,
                  stack_buf, &size, sizeof(RAWINPUTHEADER)) != (UINT)-1)
         {
            raw = (RAWINPUT*)stack_buf;
         }
         else
         {
            /* Report didn't fit; query actual size and use alloca */
            size = 0;
            GetRawInputData((HRAWINPUT)lParam, RID_INPUT,
                  NULL, &size, sizeof(RAWINPUTHEADER));
            if (size == 0)
               break;
            raw = (RAWINPUT*)alloca(size);
            if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT,
                     raw, &size, sizeof(RAWINPUTHEADER)) != size)
               break;
         }

         if (raw->header.dwType != RIM_TYPEHID)
            break;

         slot = winraw_joypad_find_pad(raw->header.hDevice);
         if (slot < 0)
            break;

         /* older reports still held from a bulk read come first */
         if (winraw_joypad_held[slot].count)
            winraw_joypad_parse_held_slot(slot);

         winraw_joypad_parse_hid_report(&winraw_joypad_pads[slot],
               raw->data.hid.bRawData,
               raw->data.hid.dwSizeHid * raw->data.hid.dwCount);
         break;
      }

      case WM_INPUT_DEVICE_CHANGE:
      {
         HANDLE hDevice = (HANDLE)lParam;
         if (wParam == GIDC_ARRIVAL)
            winraw_joypad_add_device(hDevice);
         else if (wParam == GIDC_REMOVAL)
            winraw_joypad_remove_device(hDevice);
         break;
      }

      default:
         return DefWindowProcA(hwnd, msg, wParam, lParam);
   }

   return 0;
}

static bool winraw_joypad_create_msg_window(void)
{
   WNDCLASSEXA wc;

   memset(&wc, 0, sizeof(wc));
   wc.cbSize        = sizeof(wc);
   wc.lpfnWndProc   = winraw_joypad_joypad_wndproc;
   wc.hInstance      = GetModuleHandle(NULL);
   wc.lpszClassName  = "RetroArchRawInputJoypad";

   if (!RegisterClassExA(&wc))
   {
      /* Class may already exist from a previous session */
      if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
      {
         RARCH_ERR("[RawInput Joypad] Failed to register window class.\n");
         return false;
      }
   }

   winraw_joypad_msg_window = CreateWindowExA(
         0, "RetroArchRawInputJoypad", "RawInput Joypad",
         0, 0, 0, 0, 0,
         HWND_MESSAGE, NULL, GetModuleHandle(NULL), NULL);

   if (!winraw_joypad_msg_window)
   {
      RARCH_ERR("[RawInput Joypad] Failed to create message window.\n");
      return false;
   }

   return true;
}

static bool winraw_joypad_register_devices(void)
{
   RAWINPUTDEVICE rid[2];

   /* Register for joystick input */
   rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
   rid[0].usUsage     = HID_USAGE_GENERIC_JOYSTICK;
   rid[0].dwFlags     = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
   rid[0].hwndTarget  = winraw_joypad_msg_window;

   /* Register for gamepad input */
   rid[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
   rid[1].usUsage     = HID_USAGE_GENERIC_GAMEPAD;
   rid[1].dwFlags     = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
   rid[1].hwndTarget  = winraw_joypad_msg_window;

   if (!RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE)))
   {
      RARCH_ERR("[RawInput Joypad] Failed to register raw input devices.\n");
      return false;
   }

   return true;
}

/* Enumerate devices already connected at init time */
static void winraw_joypad_enumerate_devices(void)
{
   UINT num_devices = 0;
   RAWINPUTDEVICELIST *dev_list;
   UINT i;

   if (GetRawInputDeviceList(NULL, &num_devices,
       sizeof(RAWINPUTDEVICELIST)) != 0)
      return;
   if (num_devices == 0)
      return;

   dev_list = (RAWINPUTDEVICELIST*)calloc(num_devices,
      sizeof(RAWINPUTDEVICELIST));
   if (!dev_list)
      return;

   if (GetRawInputDeviceList(dev_list, &num_devices,
      sizeof(RAWINPUTDEVICELIST)) == (UINT)-1)
   {
      free(dev_list);
      return;
   }

   for (i = 0; i < num_devices; i++)
   {
      if (dev_list[i].dwType == RIM_TYPEHID)
         winraw_joypad_add_device(dev_list[i].hDevice);
   }

   free(dev_list);
}

/* ------------------------------------------------------------------ */
/* input_device_driver_t interface                                     */
/* ------------------------------------------------------------------ */

static void *winraw_joypad_joypad_init(void *data)
{
   unsigned i;

   memset(winraw_joypad_pads, 0, sizeof(winraw_joypad_pads));
   memset(winraw_joypad_held, 0, sizeof(winraw_joypad_held));
   winraw_joypad_pad_count      = 0;
   winraw_joypad_reports_taken  = 0;
   winraw_joypad_reports_parsed = 0;

   /* Read by the poll: the window is made by the first poll, so that
    * it is the polling thread's. This runs on whichever thread starts
    * the input driver - the video thread, under threaded video. */
   winraw_joypad_by_poll       = winraw_raw_input_polled();
   winraw_joypad_window_failed = false;

   if (!winraw_joypad_by_poll)
   {
      if (!winraw_joypad_create_msg_window())
         return NULL;

      if (!winraw_joypad_register_devices())
      {
         DestroyWindow(winraw_joypad_msg_window);
         winraw_joypad_msg_window = NULL;
         return NULL;
      }
   }

#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
   winraw_joypad_xinput_own = config_get_ptr()->bools.input_winraw_xinput_enable;
#endif
   if (winraw_joypad_xinput_own)
      winraw_joypad_xinput_load();

   winraw_joypad_enumerate_devices();
   winraw_joypad_xinput_rescan();
   winraw_joypad_initialised = true;

   RARCH_LOG("[RawInput Joypad] Initialised, %u device(s) found.\n",
         winraw_joypad_pad_count);

   return (void*)-1;
}

static bool winraw_joypad_joypad_query_pad(unsigned port)
{
   return (port < MAX_USERS && winraw_joypad_pads[port].connected);
}

static void winraw_joypad_joypad_destroy(void)
{
   unsigned i;

   winraw_joypad_out_stop();

   for (i = 0; i < MAX_USERS; i++)
   {
      winraw_joypad_joypad_data_t *pad = &winraw_joypad_pads[i];
      if (!pad->connected)
         continue;

      if (pad->btn_caps)
         free(pad->btn_caps);
      if (pad->val_caps)
         free(pad->val_caps);
      if (pad->preparsed)
         free(pad->preparsed);
   }

   memset(winraw_joypad_pads, 0, sizeof(winraw_joypad_pads));
   memset(winraw_joypad_held, 0, sizeof(winraw_joypad_held));
   winraw_joypad_pad_count = 0;

   if (winraw_joypad_xi_dll)
      FreeLibrary(winraw_joypad_xi_dll);
   winraw_joypad_xi_dll       = NULL;
   winraw_joypad_xi_get_state = NULL;
   winraw_joypad_xinput_own   = false;

   if (winraw_joypad_msg_window)
   {
      /* A window is destroyed by the thread that made it. Read by the
       * poll, that is the polling thread - and this can be another:
       * a controller plugged in restarts the driver from the main
       * window's thread. The window is then asked to close, and goes
       * when its own thread next pumps. */
      if (     winraw_joypad_by_poll
            && GetCurrentThreadId() != winraw_joypad_window_tid)
         PostMessageA(winraw_joypad_msg_window, WM_CLOSE, 0, 0);
      else
         DestroyWindow(winraw_joypad_msg_window);
      if (winraw_joypad_by_poll)
         winraw_queue_claim_thread(false);
      winraw_joypad_msg_window = NULL;
   }

   winraw_joypad_initialised = false;

   if (winraw_joypad_by_poll)
      RARCH_DBG("[RawInput Joypad] Read by the poll: %lu reports read in bulk,"
            " %lu parsed.\n",
            winraw_joypad_reports_taken, winraw_joypad_reports_parsed);
   RARCH_LOG("[RawInput Joypad] Destroyed.\n");
}

static int32_t winraw_joypad_joypad_button(unsigned port, uint16_t joykey)
{
   const winraw_joypad_joypad_data_t *pad;
   unsigned hat_dir;

   if (port >= MAX_USERS)
      return 0;

   pad = &winraw_joypad_pads[port];
   if (!pad->connected)
      return 0;

   hat_dir = GET_HAT_DIR(joykey);

   if (hat_dir)
   {
      unsigned hat_index = GET_HAT(joykey);
      if (hat_index >= pad->num_hats)
         return 0;

      switch (hat_dir)
      {
         case HAT_UP_MASK:
            return (pad->hats[hat_index] & (1 << 0)) ? 1 : 0;
         case HAT_DOWN_MASK:
            return (pad->hats[hat_index] & (1 << 1)) ? 1 : 0;
         case HAT_LEFT_MASK:
            return (pad->hats[hat_index] & (1 << 2)) ? 1 : 0;
         case HAT_RIGHT_MASK:
            return (pad->hats[hat_index] & (1 << 3)) ? 1 : 0;
         default:
            break;
      }
      return 0;
   }

   if (joykey < pad->num_buttons)
      return pad->buttons[joykey] ? 1 : 0;

   return 0;
}

static void winraw_joypad_joypad_get_buttons(unsigned port,
   input_bits_t *state)
{
   unsigned i;
   const winraw_joypad_joypad_data_t *pad;

   if (port >= MAX_USERS)
   {
      BIT256_CLEAR_ALL_PTR(state);
      return;
   }

   pad = &winraw_joypad_pads[port];
   if (!pad->connected)
   {
      BIT256_CLEAR_ALL_PTR(state);
      return;
   }

   BIT256_CLEAR_ALL_PTR(state);
   for (i = 0; i < pad->num_buttons && i < RAWINPUT_MAX_BUTTONS; i++)
   {
      if (pad->buttons[i])
         BIT256_SET_PTR(state, i);
   }
}

static int16_t winraw_joypad_joypad_axis(unsigned port, uint32_t joyaxis)
{
   const winraw_joypad_joypad_data_t *pad;
   int axis     = -1;
   bool is_neg  = false;
   bool is_pos  = false;
   int16_t val;

   if (port >= MAX_USERS)
      return 0;

   pad = &winraw_joypad_pads[port];
   if (!pad->connected)
      return 0;

   if (AXIS_NEG_GET(joyaxis) < AXIS_DIR_NONE)
   {
      axis   = AXIS_NEG_GET(joyaxis);
      is_neg = true;
   }
   else if (AXIS_POS_GET(joyaxis) < AXIS_DIR_NONE)
   {
      axis   = AXIS_POS_GET(joyaxis);
      is_pos = true;
   }
   else
      return 0;

   if (axis < 0 || (unsigned)axis >= pad->num_axes)
      return 0;

   val = pad->axes[axis];

   if (is_neg && val > 0)
      return 0;
   if (is_pos && val < 0)
      return 0;

   return val;
}


/* This mirrors the xinput approach: pack hat state into high bits. */
static int16_t winraw_joypad_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   unsigned i;
   int16_t ret = 0;
   const winraw_joypad_joypad_data_t *pad;
   unsigned joy_idx;
   /* Pre-compute integer threshold to avoid per-bind float division.
    * axis_threshold is in [0.0 .. 1.0]; scale to [0 .. 0x8000]. */
   int32_t threshold;

   /* The pad is the one the player's Device Index names, joy_idx, and
    * only that one: @port is the player. Looking at the slot with the
    * player's own number first, and giving up if it was empty, left a
    * player whose Device Index points at another slot with no buttons
    * whenever that slot of their own held no pad. */
   (void)port;

   joy_idx   = joypad_info->joy_idx;
   threshold = (int32_t)(joypad_info->axis_threshold * 0x8000);

   if (joy_idx >= MAX_USERS)
      return 0;

   pad = &winraw_joypad_pads[joy_idx];
   if (!pad->connected)
      return 0;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-bind fallback */
      const uint64_t joykey  = (binds[i].joykey  != NO_BTN)
                             ?  binds[i].joykey
                             :  joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
                             ?  binds[i].joyaxis
                             :  joypad_info->auto_binds[i].joyaxis;

      /* --- Inlined button check --- */
      if ((uint16_t)joykey != NO_BTN)
      {
         uint16_t key = (uint16_t)joykey;
         unsigned hat_dir = GET_HAT_DIR(key);

         if (hat_dir)
         {
            unsigned hat_index = GET_HAT(key);
            if (hat_index < pad->num_hats)
            {
               uint8_t hv = pad->hats[hat_index];
               switch (hat_dir)
               {
                  case HAT_UP_MASK:    if (hv & (1 << 0)) ret |= (1 << i); break;
                  case HAT_DOWN_MASK:  if (hv & (1 << 1)) ret |= (1 << i); break;
                  case HAT_LEFT_MASK:  if (hv & (1 << 2)) ret |= (1 << i); break;
                  case HAT_RIGHT_MASK: if (hv & (1 << 3)) ret |= (1 << i); break;
                  default: break;
               }
            }
         }
         else if (key < pad->num_buttons && pad->buttons[key])
            ret |= (1 << i);

         /* If button matched, skip axis check for this bind */
         if (ret & (1 << i))
            continue;
      }

      /* --- Inlined axis check --- */
      if (joyaxis != AXIS_NONE)
      {
         int axis;
         int16_t val;

         if (AXIS_NEG_GET(joyaxis) < AXIS_DIR_NONE)
         {
            axis = AXIS_NEG_GET(joyaxis);
            if (axis >= 0 && (unsigned)axis < pad->num_axes)
            {
               val = pad->axes[axis];
               if (val < 0 && abs((int)val) > threshold)
                  ret |= (1 << i);
            }
         }
         else if (AXIS_POS_GET(joyaxis) < AXIS_DIR_NONE)
         {
            axis = AXIS_POS_GET(joyaxis);
            if (axis >= 0 && (unsigned)axis < pad->num_axes)
            {
               val = pad->axes[axis];
               if (val > 0 && val > threshold)
                  ret |= (1 << i);
            }
         }
      }
   }

   return ret;
}

/* Read by the poll
 * ----------------
 * A controller's reports used to be taken wherever this driver's
 * window happened to be pumped. The window was made by init, on the
 * thread that starts the input driver; under threaded video that is
 * the video thread, so the PeekMessage() below, called from the main
 * thread, found nothing, and the reports were taken by the video
 * thread's pump: once per video frame, a PeekMessage() and a
 * GetRawInputData() apiece, and not at all while that thread waited
 * in a present. The poll then read whatever state that had left - a
 * stick or a button up to a frame old.
 *
 * Now, as for the keyboard and mouse (winraw_input.c, "Read by the
 * poll"), the window is made by the first poll, on the polling
 * thread, and the reports waiting are read in bulk when the poll
 * asks: the state is what the controller had sent at that moment, and
 * a frame's reports cost one call instead of two each - and one
 * parse, of the newest, instead of one each
 * (winraw_joypad_take_hid()). The read is
 * the keyboard and mouse driver's own - one read takes everything
 * waiting on the thread, and hands this driver its share through
 * winraw_joypad_take_hid() - so the two drivers make one read between
 * them. With another input driver this is the only reader.
 *
 * Arrivals and removals stay messages. They come through the raw
 * input queue, which a range without WM_INPUT in it does not look at,
 * so the thread's pump - which leaves raw input alone - does not see
 * them, and they are taken here, every eighth poll. The range also
 * lets through a report that arrived since the read; the window
 * procedure takes that as it always did.
 *
 * RETROARCH_RAWINPUT_POLL=0 puts both drivers back as they were. */
static bool winraw_joypad_poll_window_up(void)
{
   if (!winraw_joypad_create_msg_window())
      return false;
   if (!winraw_joypad_register_devices())
   {
      DestroyWindow(winraw_joypad_msg_window);
      winraw_joypad_msg_window = NULL;
      return false;
   }
   winraw_joypad_window_tid = GetCurrentThreadId();
   winraw_queue_claim_thread(true);
   /* the controllers already plugged in are announced to the new
    * window straight away: look at the first poll */
   winraw_joypad_polls_since_look = 7;
   return true;
}

/* For winraw_input.c's answer to whether the input driver can be left
 * running across a video driver restart: this driver is kept or
 * restarted with it, and can stay when it is the joypad driver in use
 * and read by the poll - its window is then the polling thread's, and
 * it holds nothing of the video driver's. */
bool winraw_joypad_survives_video(void)
{
   return winraw_joypad_initialised && winraw_joypad_by_poll;
}

/* "Controller Player Lights": each pad's lights show the port it is
 * on. Which port that is is looked at now and then, not every poll;
 * the thread writes it when it changes. Turned off, the lights that
 * were lit are put out. */
static void winraw_joypad_player_lights(void)
{
/* (the setting exists where the driver is built into RetroArch) */
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
   static unsigned tick;
   unsigned slot, u;
   settings_t *settings;

   if ((tick++ & 31) || !winraw_joypad_out_writer)
      return;
   settings = config_get_ptr();
   for (slot = 0; slot < MAX_USERS; slot++)
   {
      winraw_joypad_out_t *out = &winraw_joypad_out[slot];
      int player               = 0;

      if (!out->present || out->xinput)
         continue;
      if (settings->bools.input_winraw_player_lights)
         for (u = 0; u < MAX_USERS; u++)
            if (settings->uints.input_joypad_index[u] == slot)
            {
               player = (int)u + 1;
               break;
            }
      /* lights this driver lit are put out when they are no longer
       * wanted - the setting turned off, or the pad on no port. Ones
       * it never lit are left alone. */
      if (!player && out->player_asked)
         player = -1;
      if (player != out->player_asked)
      {
         out->player_asked = player;
         retro_atomic_store_release_int(&out->player, player);
         input_output_writer_wake(winraw_joypad_out_writer);
      }
   }
#endif
}

static void winraw_joypad_joypad_poll(void)
{
   MSG msg;

   winraw_joypad_xinput_poll();
   winraw_joypad_player_lights();

   if (winraw_joypad_by_poll)
   {
      if (     !winraw_joypad_msg_window
            && !winraw_joypad_window_failed
            && !winraw_joypad_poll_window_up())
      {
         winraw_joypad_window_failed = true;
         RARCH_ERR("[RawInput Joypad] Could not make the window to read through.\n");
      }

      winraw_queue_read();
      /* the newest report of each controller, once */
      winraw_joypad_parse_held();

      /* Arrivals and removals: looked for every eighth poll, not
       * every one. A controller plugged in is noticed at most seven
       * polls later - some 60 ms at 120 polls a second - and seven
       * polls in eight are a call shorter. The range has to have WM_INPUT in
       * it for the raw input queue to be looked at at all, so the look
       * can also hand over a report - and under Wine it hands over the
       * keyboard's and the mouse's, whichever window is asked for,
       * which is where their reports "taken as messages" came from
       * with video not threaded. Either way the window procedures take
       * what they are given. */
      if (     winraw_joypad_msg_window
            && ++winraw_joypad_polls_since_look >= 8)
      {
         winraw_joypad_polls_since_look = 0;
         while (PeekMessageA(&msg, winraw_joypad_msg_window,
                  WM_INPUT_DEVICE_CHANGE, WM_INPUT, PM_REMOVE))
            DispatchMessageA(&msg);
      }
      return;
   }

   /* Drain all pending messages for our hidden window.
    * TranslateMessage is omitted — we only handle WM_INPUT and
    * WM_INPUT_DEVICE_CHANGE, neither of which needs key translation. */
   while (PeekMessageA(&msg, winraw_joypad_msg_window, 0, 0, PM_REMOVE))
      DispatchMessageA(&msg);
}

static const char *winraw_joypad_joypad_name(unsigned port)
{
   if (port >= MAX_USERS || !winraw_joypad_pads[port].connected)
      return NULL;
   return winraw_joypad_pads[port].name;
}

/* ------------------------------------------------------------------ */
/* Driver table                                                        */
/* ------------------------------------------------------------------ */

/* A core asks for a pad's accelerometer or gyroscope, or says it is
 * done with it. Only a pad whose sensors can be read has them; and
 * they are read out of its reports only from here on.
 *
 * What was asked is the port's, and kept (winraw_joypad_motion_want):
 * a pad unplugged and plugged in again, or the driver restarted
 * because another was, goes on giving the core what it asked for. */
static bool winraw_joypad_joypad_set_sensor_state(unsigned port,
      enum retro_sensor_action action, unsigned rate)
{
   bool has;
   winraw_joypad_joypad_data_t *pad;

   (void)rate;
   if (port >= MAX_USERS)
      return false;
   pad = &winraw_joypad_pads[port];
   has = pad->connected && pad->motion_model != SONY_PAD_NONE;

   switch (action)
   {
      case RETRO_SENSOR_ACCELEROMETER_ENABLE:
         winraw_joypad_motion_want[port] |= 1;
         break;
      case RETRO_SENSOR_GYROSCOPE_ENABLE:
         winraw_joypad_motion_want[port] |= 2;
         break;
      case RETRO_SENSOR_ACCELEROMETER_DISABLE:
         winraw_joypad_motion_want[port] &= (uint8_t)~1;
         break;
      case RETRO_SENSOR_GYROSCOPE_DISABLE:
         winraw_joypad_motion_want[port] &= (uint8_t)~2;
         break;
      case RETRO_SENSOR_ILLUMINANCE_DISABLE:
         /* done with a sensor there never was: that is not a failure */
         return true;
      default:
         return false;
   }

   pad->motion_on = has ? winraw_joypad_motion_want[port] : 0;
   if (!pad->motion_on)
      pad->motion_seen = false;
   /* an enable is taken only where there is a sensor to give; being
    * done with one is never a failure */
   return has
      || action == RETRO_SENSOR_ACCELEROMETER_DISABLE
      || action == RETRO_SENSOR_GYROSCOPE_DISABLE;
}

static bool winraw_joypad_joypad_get_sensor_input(unsigned port,
      unsigned id, float *value)
{
   const winraw_joypad_joypad_data_t *pad;

   if (port >= MAX_USERS || !value)
      return false;
   pad = &winraw_joypad_pads[port];
   /* nothing until a report has had the sensors in it: over Bluetooth
    * none does */
   if (!pad->connected || !pad->motion_seen)
      return false;
   if (id >= RETRO_SENSOR_ACCELEROMETER_X && id <= RETRO_SENSOR_ACCELEROMETER_Z)
   {
      if (!(pad->motion_on & 1))
         return false;
   }
   else if (!(pad->motion_on & 2))
      return false;
   return sony_pad_motion_value(&pad->motion, id, value);
}

input_device_driver_t winraw_joypad = {
   winraw_joypad_joypad_init,
   winraw_joypad_joypad_query_pad,
   winraw_joypad_joypad_destroy,
   winraw_joypad_joypad_button,
   winraw_joypad_joypad_state,
   winraw_joypad_joypad_get_buttons,
   winraw_joypad_joypad_axis,
   winraw_joypad_joypad_poll,
   winraw_joypad_joypad_set_rumble, /* DualShock 4 and DualSense; see "Rumble" */
   NULL,                         /* rumble_gain  */
   winraw_joypad_joypad_set_sensor_state, /* a DualShock 4 or DualSense over USB */
   winraw_joypad_joypad_get_sensor_input,
   winraw_joypad_joypad_name,
   "winraw_joypad",
};
