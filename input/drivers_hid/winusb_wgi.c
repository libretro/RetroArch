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

/* Windows.Gaming.Input for winusb's Xbox pads (winusb_wgi.h).
 *
 * combase.dll is loaded, not linked: before Windows 10 there is no
 * Windows.Gaming.Input, and nothing is paired. No thread's COM
 * apartment is set: the process is kept in the multithreaded one, as
 * SDL keeps it, so the frontend's thread calls in as it is.
 *
 * Readings are taken by the frontend's thread at its poll. Rumble goes
 * through a thread of this file's, woken by an event: Windows may
 * take its time over a write to the pad. A pairing's motors pass to it
 * through an atomic; one let go is released by it, then free again. */

#define COBJMACROS
#include <windows.h>
#include <roapi.h>
#include <windows.gaming.input.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "winusb_wgi.h"
#include "../../verbosity.h"

#define WGI_PADS 8

/* a pairing's state */
#define WGI_FREE    0
#define WGI_LIVE    1
#define WGI_CLOSING 2

/* motors asked for: strong << 8 | weak, with this set until sent */
#define WGI_DUE     0x10000

typedef __x_ABI_CWindows_CGaming_CInput_CIGamepad wgi_gamepad_t;
typedef __x_ABI_CWindows_CGaming_CInput_CIGamepadStatics wgi_statics_t;
typedef __FIVectorView_1_Windows__CGaming__CInput__CGamepad wgi_list_t;
typedef __x_ABI_CWindows_CGaming_CInput_CGamepadReading wgi_reading_t;
typedef __x_ABI_CWindows_CGaming_CInput_CGamepadVibration wgi_vibration_t;

typedef HRESULT (WINAPI *wgi_mta_usage_t)(HANDLE*);
typedef HRESULT (WINAPI *wgi_factory_t)(HSTRING, REFIID, void**);
typedef HRESULT (WINAPI *wgi_string_t)(PCWSTR, UINT32, HSTRING_HEADER*,
      HSTRING*);

struct winusb_wgi_pad
{
   wgi_gamepad_t *pad;
   IUnknown *id;             /* its identity, to know it again */
   retro_atomic_int_t state;
   retro_atomic_int_t motors;
   int sent;                 /* the thread's: as last sent */
};

static const GUID wgi_iid_statics = { 0x8bbce529, 0xd49c, 0x39e9,
   { 0x95, 0x60, 0xe4, 0x7d, 0xde, 0x96, 0xb7, 0xc8 } };
static const GUID wgi_iid_unknown = { 0x00000000, 0x0000, 0x0000,
   { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };

static struct
{
   HMODULE combase;
   HANDLE mta;
   wgi_statics_t *statics;
   HANDLE wake;
   sthread_t *thread;
   retro_atomic_int_t quit;
   struct winusb_wgi_pad pads[WGI_PADS];
} wgi;

static void wgi_send(wgi_gamepad_t *pad, int motors)
{
   wgi_vibration_t v;
   v.LeftMotor    = (double)((motors >> 8) & 0xff) / 255.0;
   v.RightMotor   = (double)(motors & 0xff) / 255.0;
   v.LeftTrigger  = 0.0;
   v.RightTrigger = 0.0;
   __x_ABI_CWindows_CGaming_CInput_CIGamepad_put_Vibration(pad, v);
}

static void wgi_rumbler(void *data)
{
   (void)data;
   for (;;)
   {
      unsigned i;
      WaitForSingleObject(wgi.wake, INFINITE);
      for (i = 0; i < WGI_PADS; i++)
      {
         struct winusb_wgi_pad *p = &wgi.pads[i];
         int state = retro_atomic_load_acquire_int(&p->state);
         if (state == WGI_LIVE)
         {
            int m = retro_atomic_load_acquire_int(&p->motors);
            if ((m & WGI_DUE) && (m & 0xffff) != p->sent)
            {
               p->sent = m & 0xffff;
               wgi_send(p->pad, p->sent);
            }
         }
         else if (state == WGI_CLOSING)
         {
            if (p->sent)
               wgi_send(p->pad, 0);
            __x_ABI_CWindows_CGaming_CInput_CIGamepad_Release(p->pad);
            IUnknown_Release(p->id);
            p->pad  = NULL;
            p->id   = NULL;
            p->sent = 0;
            retro_atomic_store_release_int(&p->state, WGI_FREE);
         }
      }
      if (retro_atomic_load_acquire_int(&wgi.quit))
         break;
   }
}

static HRESULT wgi_factory(wgi_factory_t get, wgi_string_t make,
      const wchar_t *name, UINT32 len, const GUID *iid, void **out)
{
   HSTRING_HEADER header;
   HSTRING s;
   HRESULT hr = make(name, len, &header, &s);
   if (FAILED(hr))
      return hr;
   return get(s, iid, out);
}

bool winusb_wgi_start(void)
{
   static const wchar_t gamepad[] = L"Windows.Gaming.Input.Gamepad";
   wgi_mta_usage_t mta_usage;
   wgi_factory_t get;
   wgi_string_t make;
   unsigned i;

   if (wgi.statics)
      return true;
   if (!(wgi.combase = LoadLibraryA("combase.dll")))
      return false;
   mta_usage = (wgi_mta_usage_t)GetProcAddress(wgi.combase,
         "CoIncrementMTAUsage");
   get       = (wgi_factory_t)GetProcAddress(wgi.combase,
         "RoGetActivationFactory");
   make      = (wgi_string_t)GetProcAddress(wgi.combase,
         "WindowsCreateStringReference");
   if (     !mta_usage || !get || !make
         || FAILED(mta_usage(&wgi.mta))
         || FAILED(wgi_factory(get, make, gamepad,
               (UINT32)(sizeof(gamepad) / sizeof(gamepad[0]) - 1),
               &wgi_iid_statics, (void**)&wgi.statics)))
   {
      wgi.statics = NULL;
      FreeLibrary(wgi.combase);
      wgi.combase = NULL;
      return false;
   }
   for (i = 0; i < WGI_PADS; i++)
   {
      retro_atomic_int_init(&wgi.pads[i].state, WGI_FREE);
      retro_atomic_int_init(&wgi.pads[i].motors, 0);
   }
   retro_atomic_int_init(&wgi.quit, 0);
   if (     !(wgi.wake = CreateEventA(NULL, FALSE, FALSE, NULL))
         || !(wgi.thread = sthread_create(wgi_rumbler, NULL)))
   {
      winusb_wgi_stop();
      return false;
   }
   RARCH_LOG("[HID] Xbox pads are paired with Windows.Gaming.Input.\n");
   return true;
}

void winusb_wgi_stop(void)
{
   unsigned i;
   if (wgi.thread)
   {
      for (i = 0; i < WGI_PADS; i++)
         if (retro_atomic_load_acquire_int(&wgi.pads[i].state) == WGI_LIVE)
            retro_atomic_store_release_int(&wgi.pads[i].state, WGI_CLOSING);
      retro_atomic_store_release_int(&wgi.quit, 1);
      SetEvent(wgi.wake);
      sthread_join(wgi.thread);
      wgi.thread = NULL;
   }
   if (wgi.wake)
      CloseHandle(wgi.wake);
   wgi.wake = NULL;
   if (wgi.statics)
      __x_ABI_CWindows_CGaming_CInput_CIGamepadStatics_Release(wgi.statics);
   wgi.statics = NULL;
   /* the process's hold on the multithreaded apartment stays: COM is
    * not to be left by a library that did not join it */
   if (wgi.combase)
      FreeLibrary(wgi.combase);
   wgi.combase = NULL;
}

static void wgi_look(const wgi_reading_t *r, winusb_wgi_look_t *look)
{
   static const unsigned order[10] = {
      0x4, 0x8, 0x10, 0x20,   /* A, B, X, Y */
      0x400, 0x800,           /* LB, RB */
      0x2, 0x1,               /* View, Menu */
      0x1000, 0x2000 };       /* LS, RS */
   unsigned i;
   look->sticks[0] = (int16_t)(r->LeftThumbstickX   *  32767.0);
   look->sticks[1] = (int16_t)(r->LeftThumbstickY   * -32767.0);
   look->sticks[2] = (int16_t)(r->RightThumbstickX  *  32767.0);
   look->sticks[3] = (int16_t)(r->RightThumbstickY  * -32767.0);
   look->buttons   = 0;
   for (i = 0; i < 10; i++)
      if ((unsigned)r->Buttons & order[i])
         look->buttons |= (uint16_t)(1u << i);
}

static bool wgi_taken(IUnknown *id)
{
   unsigned i;
   for (i = 0; i < WGI_PADS; i++)
      if (     retro_atomic_load_acquire_int(&wgi.pads[i].state) != WGI_FREE
            && wgi.pads[i].id == id)
         return true;
   return false;
}

winusb_wgi_pad_t *winusb_wgi_pair(const int16_t *axes, uint16_t buttons)
{
   wgi_list_t *list = NULL;
   wgi_gamepad_t *found = NULL;
   IUnknown *found_id   = NULL;
   unsigned i, alike = 0, slot;
   UINT32 n = 0;

   if (!wgi.statics)
      return NULL;
   for (slot = 0; slot < WGI_PADS; slot++)
      if (retro_atomic_load_acquire_int(&wgi.pads[slot].state) == WGI_FREE)
         break;
   if (slot == WGI_PADS)
      return NULL;
   if (     FAILED(__x_ABI_CWindows_CGaming_CInput_CIGamepadStatics_get_Gamepads(
               wgi.statics, &list)) || !list)
      return NULL;
   __FIVectorView_1_Windows__CGaming__CInput__CGamepad_get_Size(list, &n);
   for (i = 0; i < n; i++)
   {
      wgi_gamepad_t *pad = NULL;
      IUnknown *id       = NULL;
      wgi_reading_t r;
      winusb_wgi_look_t look;

      if (     FAILED(__FIVectorView_1_Windows__CGaming__CInput__CGamepad_GetAt(
                  list, i, &pad)) || !pad)
         continue;
      if (     SUCCEEDED(__x_ABI_CWindows_CGaming_CInput_CIGamepad_QueryInterface(
                  pad, &wgi_iid_unknown, (void**)&id))
            && !wgi_taken(id)
            && SUCCEEDED(__x_ABI_CWindows_CGaming_CInput_CIGamepad_GetCurrentReading(
                  pad, &r)))
      {
         wgi_look(&r, &look);
         if (winusb_wgi_alike(&look, axes, buttons))
         {
            if (alike++ == 0)
            {
               found    = pad;
               found_id = id;
               continue;
            }
         }
      }
      if (id)
         IUnknown_Release(id);
      __x_ABI_CWindows_CGaming_CInput_CIGamepad_Release(pad);
   }
   __FIVectorView_1_Windows__CGaming__CInput__CGamepad_Release(list);

   /* two alike - both at rest, say - are told apart when one moves */
   if (alike != 1)
   {
      if (found)
      {
         IUnknown_Release(found_id);
         __x_ABI_CWindows_CGaming_CInput_CIGamepad_Release(found);
      }
      return NULL;
   }
   wgi.pads[slot].pad  = found;
   wgi.pads[slot].id   = found_id;
   wgi.pads[slot].sent = 0;
   retro_atomic_store_release_int(&wgi.pads[slot].motors, 0);
   retro_atomic_store_release_int(&wgi.pads[slot].state, WGI_LIVE);
   return &wgi.pads[slot];
}

bool winusb_wgi_triggers(winusb_wgi_pad_t *pad, int16_t *left,
      int16_t *right)
{
   wgi_reading_t r;
   if (     !pad
         || FAILED(__x_ABI_CWindows_CGaming_CInput_CIGamepad_GetCurrentReading(
               pad->pad, &r)))
      return false;
   *left  = (int16_t)(r.LeftTrigger  * 32767.0);
   *right = (int16_t)(r.RightTrigger * 32767.0);
   return true;
}

void winusb_wgi_rumble(winusb_wgi_pad_t *pad, uint8_t strong, uint8_t weak)
{
   if (!pad)
      return;
   retro_atomic_store_release_int(&pad->motors,
         WGI_DUE | ((int)strong << 8) | weak);
   SetEvent(wgi.wake);
}

void winusb_wgi_unpair(winusb_wgi_pad_t *pad)
{
   if (!pad)
      return;
   retro_atomic_store_release_int(&pad->state, WGI_CLOSING);
   SetEvent(wgi.wake);
}
