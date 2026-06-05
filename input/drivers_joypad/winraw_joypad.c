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
#include <compat/strl.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../config.def.h"
#include "../../tasks/tasks_internal.h"
#include "../input_driver.h"
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

#ifndef HID_USAGE_GENERIC_DIAL
#define HID_USAGE_GENERIC_DIAL      0x37
#endif

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
} winraw_joypad_joypad_data_t;

/* ------------------------------------------------------------------ */
/* Static globals                                                      */
/* ------------------------------------------------------------------ */

/* TODO/FIXME - static globals */
static winraw_joypad_joypad_data_t winraw_joypad_pads[MAX_USERS];
static unsigned winraw_joypad_pad_count          = 0;
static HWND     winraw_joypad_msg_window         = NULL;
static bool     winraw_joypad_initialised        = false;

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
/* Device arrival / removal                                            */
/* ------------------------------------------------------------------ */

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

   /* ---- Handle reconnection race condition ----
    * Windows can send GIDC_ARRIVAL for a newly-assigned handle *before*
    * GIDC_REMOVAL for the old handle of the same physical device.
    * If we find an existing slot with the same VID:PID but a different
    * (now-stale) handle, evict it first to prevent double-registered
    * inputs from two slots reading the same physical controller.
    * We also try to reuse the same slot so that port mapping is
    * preserved across disconnect/reconnect cycles. */
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
            reuse_slot = (int)i;
            break; /* Only one stale entry per VID:PID expected */
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

   /* --- Get the device path so we can open it for the 
    * product string --- */
   GetRawInputDeviceInfoA(hDevice, RIDI_DEVICENAME, NULL, &name_size);
   if (name_size > 0 && name_size < sizeof(device_path))
   {
      GetRawInputDeviceInfoA(hDevice, RIDI_DEVICENAME,
         device_path, &name_size);

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
            unsigned axis_idx = 0;
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
                  if (axis_idx < RAWINPUT_MAX_AXES)
                     axis_idx++;
               }
               /* else: unknown/vendor value cap — skip */
            }
            pad->num_axes = (uint16_t)axis_idx;
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

   return true;
}

static void winraw_joypad_remove_device(HANDLE hDevice)
{
   int slot = winraw_joypad_find_pad(hDevice);
   if (slot < 0)
      return;

   {
      winraw_joypad_joypad_data_t *pad = &winraw_joypad_pads[slot];

      RARCH_LOG("[RawInput Joypad] Device removed from slot %d: \"%s\".\n",
            slot, pad->name);

      input_autoconfigure_disconnect((unsigned)slot, pad->name);

      if (pad->btn_caps)
         free(pad->btn_caps);
      if (pad->val_caps)
         free(pad->val_caps);
      if (pad->preparsed)
         free(pad->preparsed);

      memset(pad, 0, sizeof(*pad));
      /* pad->connected is now false from the memset */
   }

   /* Recalculate pad_count so it reflects the highest connected slot + 1.
    * Without this, pad_count only ever grows, which is benign for
    * find_pad / find_free_slot scanning, but can cause autoconfig
    * to keep seeing stale slots as "in use" during reconnect. */
   {
      unsigned i;
      unsigned new_count = 0;
      for (i = 0; i < MAX_USERS; i++)
      {
         if (winraw_joypad_pads[i].connected)
            new_count = i + 1;
      }
      winraw_joypad_pad_count = new_count;
   }
}

/* ------------------------------------------------------------------ */
/* Raw-input report parsing                                            */
/* ------------------------------------------------------------------ */

static void winraw_joypad_parse_hid_report(winraw_joypad_joypad_data_t *pad,
      const BYTE *raw_data, DWORD raw_data_size)
{
   ULONG   usage_count;
   USAGE   usages[RAWINPUT_MAX_BUTTONS];
   unsigned i;
   unsigned num_buttons;

   if (!pad || !pad->preparsed || !raw_data || raw_data_size == 0)
      return;

   num_buttons = pad->num_buttons;

   /* --- Buttons --- */
   /* Clear only the buttons this device actually has, not the full 128 */
   if (num_buttons > 0)
      memset(pad->buttons, 0, num_buttons * sizeof(pad->buttons[0]));

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
      unsigned axis_idx   = 0;
      unsigned hat_idx    = 0;
      unsigned max_axes   = pad->num_axes;
      unsigned max_hats   = pad->num_hats;

      for (i = 0; i < num_val_caps; i++)
      {
         ULONG value = 0;
         USAGE usage = pad->val_caps[i].IsRange
                     ? pad->val_caps[i].Range.UsageMin
                     : pad->val_caps[i].NotRange.Usage;

         if (HidP_GetUsageValue(HidP_Input,
                  pad->val_caps[i].UsagePage,
                  0, usage,
                  &value, pad->preparsed,
                  (PCHAR)raw_data, raw_data_size) != HIDP_STATUS_SUCCESS)
            continue;

         if (usage == HID_USAGE_GENERIC_HATSWITCH)
         {
            if (hat_idx < max_hats)
            {
               pad->hats[hat_idx] = winraw_joypad_hat_value_to_bitmask(
                     (LONG)value,
                     pad->val_caps[i].LogicalMin,
                     pad->val_caps[i].LogicalMax);
               hat_idx++;
            }
         }
         else if (winraw_joypad_is_axis_usage(&pad->val_caps[i]))
         {
            if (axis_idx < max_axes)
            {
               LONG signed_value = (pad->val_caps[i].LogicalMin < 0)
                  ? winraw_joypad_sign_extend(value, pad->val_caps[i].BitSize)
                  : (LONG)value;
               pad->axes[axis_idx] = winraw_joypad_scale_axis(
                     signed_value,
                     pad->val_caps[i].LogicalMin,
                     pad->val_caps[i].LogicalMax);
               axis_idx++;
            }
         }
         /* else: unknown/vendor value cap — skip */
      }
   }
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
   winraw_joypad_pad_count = 0;

   if (!winraw_joypad_create_msg_window())
      return NULL;

   if (!winraw_joypad_register_devices())
   {
      DestroyWindow(winraw_joypad_msg_window);
      winraw_joypad_msg_window = NULL;
      return NULL;
   }

   winraw_joypad_enumerate_devices();
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
   winraw_joypad_pad_count = 0;

   if (winraw_joypad_msg_window)
   {
      DestroyWindow(winraw_joypad_msg_window);
      winraw_joypad_msg_window = NULL;
   }

   winraw_joypad_initialised = false;

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

   if (port >= MAX_USERS)
      return 0;

   pad = &winraw_joypad_pads[port];
   if (!pad->connected)
      return 0;

   joy_idx   = joypad_info->joy_idx;
   threshold = (int32_t)(joypad_info->axis_threshold * 0x8000);

   if (joy_idx >= MAX_USERS)
      return 0;

   /* If joy_idx differs from port, we need that pad instead */
   if (joy_idx != port)
   {
      pad = &winraw_joypad_pads[joy_idx];
      if (!pad->connected)
         return 0;
   }

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

static void winraw_joypad_joypad_poll(void)
{
   MSG msg;
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

input_device_driver_t winraw_joypad = {
   winraw_joypad_joypad_init,
   winraw_joypad_joypad_query_pad,
   winraw_joypad_joypad_destroy,
   winraw_joypad_joypad_button,
   winraw_joypad_joypad_state,
   winraw_joypad_joypad_get_buttons,
   winraw_joypad_joypad_axis,
   winraw_joypad_joypad_poll,
   NULL,                         /* set_rumble   (not supported via RawInput) */
   NULL,                         /* rumble_gain  */
   NULL,                         /* set_sensor_state */
   NULL,                         /* get_sensor_input */
   winraw_joypad_joypad_name,
   "winraw_joypad",
};
