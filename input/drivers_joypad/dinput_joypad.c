/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2020 - Daniel De Matteis
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

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#define WIN32_LEAN_AND_MEAN
#include <windowsx.h>

#include <dinput.h>
#include <mmsystem.h>

#include <boolean.h>
#include <compat/strl.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../tasks/tasks_internal.h"
#include "../input_keymaps.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

#include <queues/task_queue.h>
#include <retro_timers.h>

#ifndef __DINPUT_JOYPAD_H
#define __DINPUT_JOYPAD_H

#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

#define WIN32_LEAN_AND_MEAN
#include <dinput.h>

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

RETRO_END_DECLS

#endif

/* TODO/FIXME - globals referenced outside; candidate for context-struct refactor */
struct dinput_joypad_data g_pads[MAX_USERS];
unsigned g_joypad_cnt;

/* Joypad-owned DirectInput context, separate from the shared
 * keyboard/mouse context (g_dinput_ctx) in dinput.c: pad enumeration
 * and device creation run on the task queue - possibly a worker
 * thread - and a private context keeps that activity off the COM
 * object the main thread polls every frame. Created synchronously at
 * init (cheap; also preserves the driver-fallback semantics of a
 * failed DirectInput8Create()), used by the enumeration task,
 * released in dinput_joypad_destroy(). */
LPDIRECTINPUT8 g_dinput_joypad_ctx   = NULL;

/* True while a pad enumeration task is in flight. Written on the
 * main thread at task push and cleared at the end of the task's
 * main-thread callback, so main-thread waiters pumping
 * task_queue_check() observe the transition without extra
 * synchronization. */
volatile bool g_dinput_enum_inflight = false;

/* Defined in dinput.c / dinput_input.c */
extern LPDIRECTINPUT8 g_dinput_ctx;

#ifndef __DINPUT_JOYPAD_INL_H
#define __DINPUT_JOYPAD_INL_H

#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

#include <windowsx.h>
#include <dinput.h>
#include <mmsystem.h>

/* Forward declarations */
extern struct dinput_joypad_data g_pads[MAX_USERS];
extern unsigned g_joypad_cnt;
extern LPDIRECTINPUT8 g_dinput_ctx;
extern LPDIRECTINPUT8 g_dinput_joypad_ctx;
extern volatile bool g_dinput_enum_inflight;

void dinput_destroy_context(void);
bool dinput_init_context(void);

static void dinput_create_rumble_effects(struct dinput_joypad_data *pad,
      LPDIRECTINPUTDEVICE8 dev)
{
   /* Store rumble parameters in the pad struct so that rumble_props pointers
    * remain valid for the lifetime of the pad (fixes dangling-pointer UB). */
   pad->rumble_force.lMagnitude  = 0;
   pad->rumble_direction         = 0;
   pad->rumble_axis              = DIJOFS_X;

   pad->rumble_envelope.dwSize        = sizeof(DIENVELOPE);
   pad->rumble_envelope.dwAttackLevel  = 5000;
   pad->rumble_envelope.dwAttackTime   = 250000;
   pad->rumble_envelope.dwFadeLevel    = 0;
   pad->rumble_envelope.dwFadeTime     = 250000;

   pad->rumble_props.dwSize                  = sizeof(DIEFFECT);
   pad->rumble_props.dwFlags                 = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
   pad->rumble_props.dwDuration              = INFINITE;
   pad->rumble_props.dwStartDelay            = 0;
   pad->rumble_props.dwTriggerButton         = DIEB_NOTRIGGER;
   pad->rumble_props.dwTriggerRepeatInterval = 0;
   pad->rumble_props.cAxes                   = 1;
   pad->rumble_props.rgdwAxes                = &pad->rumble_axis;
   pad->rumble_props.rglDirection            = &pad->rumble_direction;
   pad->rumble_props.lpEnvelope              = &pad->rumble_envelope;
   pad->rumble_props.cbTypeSpecificParams    = sizeof(DICONSTANTFORCE);
   pad->rumble_props.lpvTypeSpecificParams   = &pad->rumble_force;
   pad->rumble_props.dwGain                  = 0;

   /* --- strong motor (X axis) --- */
#ifdef __cplusplus
   if (IDirectInputDevice8_CreateEffect(dev, GUID_ConstantForce,
         &pad->rumble_props, &pad->rumble_iface[0], NULL) != DI_OK)
#else
   if (IDirectInputDevice8_CreateEffect(dev, &GUID_ConstantForce,
         &pad->rumble_props, &pad->rumble_iface[0], NULL) != DI_OK)
#endif
      RARCH_WARN("[DInput] Strong rumble unavailable.\n");

   /* --- weak motor (Y axis) --- */
   pad->rumble_axis = DIJOFS_Y;
#ifdef __cplusplus
   if (IDirectInputDevice8_CreateEffect(dev, GUID_ConstantForce,
         &pad->rumble_props, &pad->rumble_iface[1], NULL) != DI_OK)
#else
   if (IDirectInputDevice8_CreateEffect(dev, &GUID_ConstantForce,
         &pad->rumble_props, &pad->rumble_iface[1], NULL) != DI_OK)
#endif
      RARCH_WARN("[DInput] Weak rumble unavailable.\n");
}

static BOOL CALLBACK enum_axes_cb(
      const DIDEVICEOBJECTINSTANCE *inst, void *p)
{
   DIPROPRANGE          range;
   LPDIRECTINPUTDEVICE8 joypad = (LPDIRECTINPUTDEVICE8)p;

   range.diph.dwSize       = sizeof(DIPROPRANGE);
   range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
   range.diph.dwHow        = DIPH_BYID;
   range.diph.dwObj        = inst->dwType;
   range.lMin              = -0x7fff;
   range.lMax              =  0x7fff;

   IDirectInputDevice8_SetProperty(joypad, DIPROP_RANGE, &range.diph);
   return DIENUM_CONTINUE;
}

/* Offset table for DIJOYSTATE2 axis fields — single bounds-check + indexed
 * load instead of a switch/jump-table.  All target fields are LONG. */
static const size_t dinput_axis_offsets[8] = {
   offsetof(DIJOYSTATE2, lX),
   offsetof(DIJOYSTATE2, lY),
   offsetof(DIJOYSTATE2, lZ),
   offsetof(DIJOYSTATE2, lRx),
   offsetof(DIJOYSTATE2, lRy),
   offsetof(DIJOYSTATE2, lRz),
   offsetof(DIJOYSTATE2, rglSlider[0]),
   offsetof(DIJOYSTATE2, rglSlider[1]),
};

static int16_t dinput_joypad_get_axis_val(
      const struct dinput_joypad_data *pad, unsigned axis)
{
   if (axis < 8)
      return (int16_t)(*(const LONG*)
            ((const char*)&pad->joy_state + dinput_axis_offsets[axis]));
   return 0;
}

/* Pre-computed diagonal POV values (hundredths of degrees).
 * Avoids repeated arithmetic in the hot path. */
#define DINPUT_POV_NE  (JOY_POVRIGHT   / 2)                  /*  4500 */
#define DINPUT_POV_SE  (JOY_POVRIGHT   + JOY_POVRIGHT / 2)   /* 13500 */
#define DINPUT_POV_SW  (JOY_POVBACKWARD + JOY_POVRIGHT / 2)  /* 22500 */
#define DINPUT_POV_NW  (JOY_POVLEFT    + JOY_POVRIGHT / 2)   /* 31500 */

static int32_t dinput_joypad_button_state(
      const struct dinput_joypad_data *pad,
      uint16_t joykey)
{
   unsigned hat_dir = GET_HAT_DIR(joykey);

   if (hat_dir)
   {
      unsigned h = GET_HAT(joykey);

      if (h < 4)
      {
         /* 0xFFFFFFFF == centred, no direction active */
         unsigned pov = pad->joy_state.rgdwPOV[h];
         if (pov == DINPUT_POV_CENTERED)
            return 0;

         switch (hat_dir)
         {
            case HAT_UP_MASK:
               return (pov == JOY_POVFORWARD)
                   || (pov == DINPUT_POV_NE)
                   || (pov == DINPUT_POV_NW);

            case HAT_RIGHT_MASK:
               return (pov == JOY_POVRIGHT)
                   || (pov == DINPUT_POV_NE)
                   || (pov == DINPUT_POV_SE);

            case HAT_DOWN_MASK:
               return (pov == JOY_POVBACKWARD)
                   || (pov == DINPUT_POV_SE)
                   || (pov == DINPUT_POV_SW);

            case HAT_LEFT_MASK:
               return (pov == JOY_POVLEFT)
                   || (pov == DINPUT_POV_SW)
                   || (pov == DINPUT_POV_NW);

            default:
               break;
         }
      }
      /* hat requested but index out of range */
      return 0;
   }

   if (joykey < ARRAY_SIZE_RGB_BUTTONS)
      return pad->joy_state.rgbButtons[joykey] ? 1 : 0;

   return 0;
}

static int16_t dinput_joypad_axis_state(
      const struct dinput_joypad_data *pad,
      uint32_t joyaxis)
{
   unsigned axis;
   int16_t  val;

   if (AXIS_NEG_GET(joyaxis) <= 7)
   {
      axis = (unsigned)AXIS_NEG_GET(joyaxis);
      val  = dinput_joypad_get_axis_val(pad, axis);
      if (val < 0)
         return val;
   }
   else if (AXIS_POS_GET(joyaxis) <= 7)
   {
      axis = (unsigned)AXIS_POS_GET(joyaxis);
      val  = dinput_joypad_get_axis_val(pad, axis);
      if (val > 0)
         return val;
   }

   return 0;
}

static int32_t dinput_joypad_button(unsigned port, uint16_t joykey)
{
   if (port >= MAX_USERS)
      return 0;
   if (!g_pads[port].joypad)
      return 0;
   return dinput_joypad_button_state(&g_pads[port], joykey);
}

static int16_t dinput_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (port >= MAX_USERS)
      return 0;
   if (!g_pads[port].joypad)
      return 0;
   return dinput_joypad_axis_state(&g_pads[port], joyaxis);
}

static int16_t dinput_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   unsigned i;
   int16_t  ret;
   uint16_t port_idx;
   const struct dinput_joypad_data *pad;
   /* Pre-convert the floating-point threshold to an integer value once,
    * eliminating a float cast + division per bind per frame. */
   int32_t  axis_thresh_abs;

   (void)port; /* joy_idx from joypad_info is authoritative */

   port_idx = joypad_info->joy_idx;
   if (port_idx >= MAX_USERS)
      return 0;

   pad = &g_pads[port_idx];
   if (!pad->joypad)
      return 0;

   axis_thresh_abs = (int32_t)(joypad_info->axis_threshold * 0x8000);

   ret = 0;
   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey  != NO_BTN)
         ? binds[i].joykey   : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis  != AXIS_NONE)
         ? binds[i].joyaxis  : joypad_info->auto_binds[i].joyaxis;

      if (     (uint16_t)joykey != NO_BTN
            && dinput_joypad_button_state(pad, (uint16_t)joykey))
         ret |= (1 << i);
      else if (joyaxis != AXIS_NONE
            && abs(dinput_joypad_axis_state(pad, joyaxis))
                  > axis_thresh_abs)
         ret |= (1 << i);
   }

   return ret;
}

static bool dinput_joypad_query_pad(unsigned port)
{
   return port < MAX_USERS && g_pads[port].joypad;
}

static bool dinput_joypad_set_rumble(unsigned port,
      enum retro_rumble_effect type, uint16_t strength)
{
   /* RETRO_RUMBLE_STRONG == 0, RETRO_RUMBLE_WEAK == 1 — map directly */
   int iface_idx = (type == RETRO_RUMBLE_STRONG) ? 0 : 1;

   if (port >= g_joypad_cnt || !g_pads[port].rumble_iface[iface_idx])
      return false;

   if (strength)
   {
      /* Integer approximation: avoids float/double entirely.
       * strength in [0, 65535], DI_FFNOMINALMAX = 10000.
       * Result fits in DWORD. */
      g_pads[port].rumble_props.dwGain =
            (DWORD)(((unsigned)strength * (unsigned)DI_FFNOMINALMAX) / 65535u);
      IDirectInputEffect_SetParameters(g_pads[port].rumble_iface[iface_idx],
            &g_pads[port].rumble_props, DIEP_GAIN | DIEP_START);
   }
   else
      IDirectInputEffect_Stop(g_pads[port].rumble_iface[iface_idx]);

   return true;
}

static bool dinput_joypad_ctx_create(void)
{
   if (!g_dinput_joypad_ctx)
   {
#ifdef __cplusplus
      if (!(SUCCEEDED(DirectInput8Create(
                     GetModuleHandle(NULL), DIRECTINPUT_VERSION,
                     IID_IDirectInput8,
                     (void**)&g_dinput_joypad_ctx, NULL))))
#else
      if (!(SUCCEEDED(DirectInput8Create(
                     GetModuleHandle(NULL), DIRECTINPUT_VERSION,
                     &IID_IDirectInput8,
                     (void**)&g_dinput_joypad_ctx, NULL))))
#endif
         return false;
   }
   return true;
}

/* Blocks until any in-flight pad enumeration task has fully
 * finished, including its main-thread callback. Must be called from
 * the main thread. IDirectInput8_EnumDevices() cannot be cancelled,
 * so teardown joins it; the stall this can incur only occurs if the
 * user quits or hotplugs while an enumeration is still running,
 * instead of unconditionally blocking startup as the old
 * synchronous enumeration did. */
static void dinput_joypad_enum_wait(void)
{
   while (g_dinput_enum_inflight)
   {
      task_queue_check();
      retro_sleep(1);
   }
}

static void dinput_joypad_destroy(void)
{
   unsigned i;

   /* Join any in-flight enumeration before touching pad state. */
   dinput_joypad_enum_wait();

   for (i = 0; i < MAX_USERS; i++)
   {
      if (g_pads[i].joypad)
      {
         unsigned r;
         for (r = 0; r < 2; r++)
         {
            if (g_pads[i].rumble_iface[r])
            {
               IDirectInputEffect_Stop(g_pads[i].rumble_iface[r]);
               IDirectInputEffect_Release(g_pads[i].rumble_iface[r]);
            }
         }

         IDirectInputDevice8_Unacquire(g_pads[i].joypad);
         IDirectInputDevice8_Release(g_pads[i].joypad);
      }

      free(g_pads[i].joy_name);
      g_pads[i].joy_name = NULL;
      free(g_pads[i].joy_friendly_name);
      g_pads[i].joy_friendly_name = NULL;

      input_config_clear_device_name(i);
   }

   g_joypad_cnt = 0;
   memset(g_pads, 0, sizeof(g_pads));

   if (g_dinput_joypad_ctx)
   {
      IDirectInput8_Release(g_dinput_joypad_ctx);
      g_dinput_joypad_ctx = NULL;
   }
}

static const char *dinput_joypad_name(unsigned port)
{
   if (port < MAX_USERS)
      return g_pads[port].joy_name;
   return NULL;
}

#endif /* __DINPUT_JOYPAD_INL_H */

#ifndef __DINPUT_JOYPAD_EXCL_INL_H
#define __DINPUT_JOYPAD_EXCL_INL_H

#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

#include <windowsx.h>
#include <dinput.h>
#include <mmsystem.h>

/* ---------------------------------------------------------------------------
 * dinput_joypad_cleanup_pad
 *
 * Releases the DirectInput device and frees heap-allocated name strings
 * for the pad at the given index, then zeroes the slot.  Used both for
 * error roll-back during enumeration and for disconnect cleanup.
 * --------------------------------------------------------------------------*/
static void dinput_joypad_cleanup_pad(unsigned idx)
{
   struct dinput_joypad_data *pad = &g_pads[idx];

   if (pad->joypad)
   {
      IDirectInputDevice8_Unacquire(pad->joypad);
      IDirectInputDevice8_Release(pad->joypad);
   }

   free(pad->joy_name);
   free(pad->joy_friendly_name);

   ZeroMemory(pad, sizeof(*pad));
}


static void dinput_joypad_poll(void)
{
   unsigned i;
   /* Only iterate connected pad slots — avoids touching empty entries.
    * Note: disconnect via cleanup_pad zeroes the slot but does not compact
    * the array, so mid-array gaps have pad->joypad == NULL and are skipped
    * by the existing NULL check below. */
   for (i = 0; i < g_joypad_cnt; i++)
   {
      HRESULT ret;
      unsigned h;
      struct dinput_joypad_data *pad = &g_pads[i];

      if (!pad->joypad)
         continue;

      /* If Poll() fails, attempt to re-acquire and poll again.
       * If that also fails, skip this pad for this frame.
       * Zero the cached state first: DirectInput devices lose
       * acquisition on focus loss (alt-tab, UAC prompt, screen
       * saver), and 'joy_state' would otherwise keep reporting
       * whatever was held at that moment - including rgdwPOV[],
       * so a D-pad direction held when focus was lost is seen as
       * still held for the whole unfocused period and beyond. */
      if (FAILED(IDirectInputDevice8_Poll(pad->joypad)))
         if (  FAILED(IDirectInputDevice8_Acquire(pad->joypad))
            || FAILED(IDirectInputDevice8_Poll(pad->joypad)))
         {
            memset(&pad->joy_state, 0, sizeof(pad->joy_state));
            /* Centre all hats - zero is a valid POV direction
             * (north), so memset alone would report 'up' held */
            for (h = 0; h < ARRAY_SIZE(pad->joy_state.rgdwPOV); h++)
               pad->joy_state.rgdwPOV[h] = DINPUT_POV_CENTERED;
            continue;
         }

      ret = IDirectInputDevice8_GetDeviceState(pad->joypad,
            sizeof(DIJOYSTATE2), &pad->joy_state);

      if (ret == DIERR_INPUTLOST || ret == DIERR_NOTACQUIRED)
      {
         input_autoconfigure_disconnect(i, g_pads[i].joy_friendly_name);
         dinput_joypad_cleanup_pad(i);
      }
      /* GetDeviceState writes the full DIJOYSTATE2 on success, so no
       * pre-zeroing is needed.  On failure the pad is cleaned up above,
       * which zeroes the entire slot including joy_state. */
   }
}

/* ---------------------------------------------------------------------------
 * dinput_joypad_tchar_to_str
 *
 * Helper: safely converts a TCHAR string to a heap-allocated char* that
 * is valid in both ANSI and Unicode builds.
 *
 * Under ANSI builds (TCHAR == char) this is equivalent to strdup().
 * Under Unicode builds (TCHAR == wchar_t) the original code cast the
 * wide pointer directly to char*, silently producing garbage.  We use
 * WideCharToMultiByte() instead.
 *
 * Returns NULL on allocation failure; the caller must free() the result.
 * --------------------------------------------------------------------------*/
static char *dinput_joypad_tchar_to_str(const TCHAR *src)
{
#ifdef UNICODE
   int   len;
   char *out;

   if (!src)
      return NULL;

   /* Query required buffer size (includes NUL terminator). */
   len = WideCharToMultiByte(CP_UTF8, 0, src, -1, NULL, 0, NULL, NULL);
   if (len <= 0)
      return NULL;

   out = (char*)malloc((size_t)len);
   if (!out)
      return NULL;

   WideCharToMultiByte(CP_UTF8, 0, src, -1, out, len, NULL, NULL);
   return out;
#else
   if (!src)
      return NULL;
   return strdup(src);
#endif
}

static BOOL CALLBACK enum_joypad_cb(const DIDEVICEINSTANCE *inst, void *p)
{
   /* May run on a task queue worker thread while the main thread is
    * polling: create and configure the device through a local
    * pointer, fill the g_pads[] entry, and only then publish the
    * device pointer (which gates the main-thread accessors) and the
    * pad count (which gates poll), each behind a barrier. */
   LPDIRECTINPUTDEVICE8 dev       = NULL;
   struct dinput_joypad_data *pad = NULL;
   unsigned idx;

   (void)p; /* unused */

   if (g_joypad_cnt == MAX_USERS)
      return DIENUM_STOP;

   idx = g_joypad_cnt;
   pad = &g_pads[idx];

#ifdef __cplusplus
   if (FAILED(IDirectInput8_CreateDevice(
               g_dinput_joypad_ctx, inst->guidInstance, &dev, NULL)))
#else
   if (FAILED(IDirectInput8_CreateDevice(
               g_dinput_joypad_ctx, &inst->guidInstance, &dev, NULL)))
#endif
      return DIENUM_CONTINUE;

   /* Bug fixed: use the safe TCHAR helper instead of a raw cast that
    * would produce garbage under Unicode builds. */
   pad->joy_name          =
      dinput_joypad_tchar_to_str(inst->tszProductName);
   pad->joy_friendly_name =
      dinput_joypad_tchar_to_str(inst->tszInstanceName);

   /* VID is in the low 16 bits of guidProduct.Data1,
    * PID is in the high 16 bits. */
   pad->vid = (int32_t)(inst->guidProduct.Data1 & 0xFFFFu);
   pad->pid = (int32_t)(inst->guidProduct.Data1 >> 16);

   /* Set data format to extended joystick state.
    * If either SetDataFormat or SetCooperativeLevel fails the device
    * is unusable - release everything and move on to the next pad. */
   if (FAILED(IDirectInputDevice8_SetDataFormat(dev, &c_dfDIJoystick2)))
      goto cfg_failed;

   if (FAILED(IDirectInputDevice8_SetCooperativeLevel(dev,
         (HWND)video_driver_window_get(),
         DISCL_EXCLUSIVE | DISCL_BACKGROUND)))
      goto cfg_failed;

   IDirectInputDevice8_EnumObjects(dev, enum_axes_cb,
         dev, DIDFT_ABSAXIS);

   dinput_create_rumble_effects(pad, dev);

   /* Publish: entry fields first, then the device pointer, then the
    * count. Autoconfiguration happens later, on the main thread, in
    * the enumeration task callback. */
   MemoryBarrier();
   pad->joypad = dev;
   MemoryBarrier();
   g_joypad_cnt = idx + 1;
   return DIENUM_CONTINUE;

cfg_failed:
   /* The device is unusable - release everything and move on to the
    * next pad. The g_pads[] entry was never published (joypad still
    * NULL), so main-thread readers never saw it. */
   IDirectInputDevice8_Release(dev);
   free(pad->joy_name);
   free(pad->joy_friendly_name);
   ZeroMemory(pad, sizeof(*pad));
   return DIENUM_CONTINUE;
}

static void dinput_joypad_autoconf_flush(void)
{
   unsigned i;
   for (i = 0; i < g_joypad_cnt; i++)
   {
      if (!g_pads[i].joypad)
         continue;
      input_autoconfigure_connect(
            g_pads[i].joy_name,
            g_pads[i].joy_friendly_name,
            NULL,
            dinput_joypad.ident,
            i,
            g_pads[i].vid,
            g_pads[i].pid);
   }
}

static void dinput_joypad_enum_task_handler(retro_task_t *task)
{
   /* EnumDevices() walks the whole HID/PnP tree synchronously and
    * can stall for seconds if a device driver stack (e.g. Bluetooth)
    * is still coming up after a fresh boot - which is exactly why it
    * runs here, on the task queue, instead of blocking startup. */
   IDirectInput8_EnumDevices(g_dinput_joypad_ctx, DI8DEVCLASS_GAMECTRL,
         enum_joypad_cb, NULL, DIEDFL_ATTACHEDONLY);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void dinput_joypad_enum_task_cb(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   /* Main thread: fire autoconfiguration for the published pads,
    * then mark the enumeration as complete for waiters. */
   dinput_joypad_autoconf_flush();
   g_dinput_enum_inflight = false;
}

static void *dinput_joypad_init(void *data)
{
   retro_task_t *task = NULL;

   if (!dinput_joypad_ctx_create())
      return NULL;
   /* Zero the pad array; dinput_joypad_destroy() uses memset() on the same
    * region, so we stay consistent and avoid partial-initialisation bugs. */
   ZeroMemory(g_pads, sizeof(g_pads));

   if (!(task = task_init()))
   {
      /* Allocation failure: fall back to the old synchronous
       * enumeration. */
      IDirectInput8_EnumDevices(g_dinput_joypad_ctx, DI8DEVCLASS_GAMECTRL,
            enum_joypad_cb, NULL, DIEDFL_ATTACHEDONLY);
      dinput_joypad_autoconf_flush();
      return (void*)-1;
   }

   g_dinput_enum_inflight = true;
   task->handler  = dinput_joypad_enum_task_handler;
   task->state    = NULL;
   task->title    = NULL;
   task->callback = dinput_joypad_enum_task_cb;
   task->cleanup  = NULL;
   task->flags   |= RETRO_TASK_FLG_MUTE;
   task_queue_push(task);

   return (void*)-1;
}

#endif /* __DINPUT_JOYPAD_EXCL_INL_H */

input_device_driver_t dinput_joypad = {
   dinput_joypad_init,
   dinput_joypad_query_pad,
   dinput_joypad_destroy,
   dinput_joypad_button,
   dinput_joypad_state,
   NULL,
   dinput_joypad_axis,
   dinput_joypad_poll,
   dinput_joypad_set_rumble,
   NULL, /* set_rumble_gain */
   NULL, /* set_sensor_state */
   NULL, /* get_sensor_input */
   dinput_joypad_name,
   "dinput",
};
