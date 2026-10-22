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

/* Windows' own Bluetooth stack, through bthprops.cpl, loaded when the
 * driver starts: nothing is linked, and a Windows without it has no
 * Bluetooth here.
 *
 * Pairing is what the menu's list is for, and Wii controllers are why:
 * Windows' settings often cannot pair one. As Dolphin pairs them, one
 * whose sync button was pressed is paired with the radio's own address
 * as its PIN, after any pairing Windows remembers but cannot reach is
 * forgotten; then, as for any other device, its HID service is turned
 * on, and Windows makes it a HID device, which the hid joypad driver
 * finds.
 *
 * A search, a pairing and a forgetting each block for seconds, so each
 * runs on a thread of the driver's: the menu's calls only start it.
 * The scan's list passes to the frontend's thread at scan_end, after
 * the thread is joined; a pairing works on a copy of its device. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <bluetoothapis.h>

#include <boolean.h>
#include <compat/strl.h>
#include <lists/string_list.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "../bluetooth_driver.h"
#include "../../verbosity.h"

#define WINBT_MAX_DEVICES 32
#ifndef BLUETOOTH_SERVICE_ENABLE
#define BLUETOOTH_SERVICE_ENABLE 0x01
#endif
/* the inquiry, in units of 1.28 s: within the scan window */
#define WINBT_INQUIRY     6

typedef HBLUETOOTH_RADIO_FIND (WINAPI *winbt_find_radio_t)(
      const BLUETOOTH_FIND_RADIO_PARAMS*, HANDLE*);
typedef BOOL (WINAPI *winbt_radio_close_t)(HBLUETOOTH_RADIO_FIND);
typedef DWORD (WINAPI *winbt_radio_info_t)(HANDLE, BLUETOOTH_RADIO_INFO*);
typedef HBLUETOOTH_DEVICE_FIND (WINAPI *winbt_find_first_t)(
      const BLUETOOTH_DEVICE_SEARCH_PARAMS*, BLUETOOTH_DEVICE_INFO*);
typedef BOOL (WINAPI *winbt_find_next_t)(HBLUETOOTH_DEVICE_FIND,
      BLUETOOTH_DEVICE_INFO*);
typedef BOOL (WINAPI *winbt_find_close_t)(HBLUETOOTH_DEVICE_FIND);
typedef DWORD (WINAPI *winbt_authenticate_t)(HWND, HANDLE,
      BLUETOOTH_DEVICE_INFO*, PWSTR, ULONG);
typedef DWORD (WINAPI *winbt_service_t)(HANDLE, const BLUETOOTH_DEVICE_INFO*,
      const GUID*, DWORD);
typedef DWORD (WINAPI *winbt_remove_t)(const BLUETOOTH_ADDRESS*);

/* the Bluetooth HID service, HumanInterfaceDeviceServiceClass_UUID */
static const GUID winbt_hid_service = { 0x00001124, 0x0000, 0x1000,
   { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb } };

typedef struct
{
   HMODULE lib;
   winbt_find_radio_t find_radio;
   winbt_radio_close_t radio_close;
   winbt_radio_info_t radio_info;
   winbt_find_first_t find_first;
   winbt_find_next_t find_next;
   winbt_find_close_t find_close;
   winbt_authenticate_t authenticate;
   winbt_service_t service;
   winbt_remove_t remove;

   /* the frontend's: as the last scan found them */
   BLUETOOTH_DEVICE_INFO devices[WINBT_MAX_DEVICES];
   unsigned count;

   /* the scan thread's until it is joined */
   sthread_t *scan;
   BLUETOOTH_DEVICE_INFO found[WINBT_MAX_DEVICES];
   unsigned found_count;

   /* a pairing or forgetting: one at a time, on a copy */
   sthread_t *action;
   retro_atomic_int_t action_busy;
   BLUETOOTH_DEVICE_INFO target;
   bool target_remove;
} winbt_t;

/* A Wii controller, by the name it gives: the remotes, the Wii U Pro
 * Controller and the Balance Board. */
static bool winbt_is_wii(const BLUETOOTH_DEVICE_INFO *d)
{
   return wcsncmp(d->szName, L"Nintendo RVL-", 13) == 0;
}

static HANDLE winbt_radio(winbt_t *bt)
{
   BLUETOOTH_FIND_RADIO_PARAMS p;
   HANDLE radio = NULL;
   HBLUETOOTH_RADIO_FIND find;

   p.dwSize = sizeof(p);
   if (!(find = bt->find_radio(&p, &radio)))
      return NULL;
   bt->radio_close(find);
   return radio;
}

static void winbt_scan_thread(void *data)
{
   winbt_t *bt = (winbt_t*)data;
   BLUETOOTH_DEVICE_SEARCH_PARAMS p;
   BLUETOOTH_DEVICE_INFO d;
   HBLUETOOTH_DEVICE_FIND find;
   HANDLE radio;

   bt->found_count = 0;
   if (!(radio = winbt_radio(bt)))
   {
      RARCH_WARN("[Bluetooth] No Bluetooth radio.\n");
      return;
   }
   memset(&p, 0, sizeof(p));
   p.dwSize               = sizeof(p);
   p.fReturnAuthenticated = TRUE;
   p.fReturnRemembered    = TRUE;
   p.fReturnUnknown       = TRUE;
   p.fReturnConnected     = TRUE;
   p.fIssueInquiry        = TRUE;
   p.cTimeoutMultiplier   = WINBT_INQUIRY;
   p.hRadio               = radio;
   memset(&d, 0, sizeof(d));
   d.dwSize = sizeof(d);
   if ((find = bt->find_first(&p, &d)))
   {
      do
      {
         if (bt->found_count < WINBT_MAX_DEVICES && d.szName[0])
            bt->found[bt->found_count++] = d;
         memset(&d, 0, sizeof(d));
         d.dwSize = sizeof(d);
      } while (bt->find_next(find, &d));
      bt->find_close(find);
   }
   CloseHandle(radio);
}

static void winbt_action_thread(void *data)
{
   winbt_t *bt              = (winbt_t*)data;
   BLUETOOTH_DEVICE_INFO *d = &bt->target;
   char name[BLUETOOTH_MAX_NAME_SIZE];
   HANDLE radio;
   DWORD err = ERROR_SUCCESS;

   WideCharToMultiByte(CP_UTF8, 0, d->szName, -1, name, sizeof(name),
         NULL, NULL);
   name[sizeof(name) - 1] = '\0';
   if (!(radio = winbt_radio(bt)))
   {
      RARCH_WARN("[Bluetooth] No Bluetooth radio.\n");
      retro_atomic_store_release_int(&bt->action_busy, 0);
      return;
   }

   if (bt->target_remove)
   {
      err = bt->remove(&d->Address);
      RARCH_LOG("[Bluetooth] \"%s\" %s.\n", name,
            err == ERROR_SUCCESS ? "forgotten" : "could not be forgotten");
   }
   else
   {
      if (winbt_is_wii(d))
      {
         BLUETOOTH_RADIO_INFO info;
         WCHAR pin[6];
         unsigned i;

         /* a pairing it cannot reach stands in the way of a new one */
         if (d->fRemembered && !d->fConnected)
         {
            bt->remove(&d->Address);
            d->fRemembered = d->fAuthenticated = FALSE;
         }
         memset(&info, 0, sizeof(info));
         info.dwSize = sizeof(info);
         if (     !d->fAuthenticated
               && bt->radio_info(radio, &info) == ERROR_SUCCESS)
         {
            /* paired by its sync button: the radio's address, as it is */
            for (i = 0; i < 6; i++)
               pin[i] = info.address.rgBytes[i];
            err = bt->authenticate(NULL, radio, d, pin, 6);
         }
      }
      else if (!d->fAuthenticated)
         err = bt->authenticate(NULL, radio, d, NULL, 0);
      if (err == ERROR_SUCCESS || err == ERROR_NO_MORE_ITEMS)
         err = bt->service(radio, d, &winbt_hid_service,
               BLUETOOTH_SERVICE_ENABLE);
      if (err == ERROR_SUCCESS)
         RARCH_LOG("[Bluetooth] \"%s\" paired.\n", name);
      else
         RARCH_WARN("[Bluetooth] \"%s\" could not be paired (error %lu).\n",
               name, (unsigned long)err);
   }
   CloseHandle(radio);
   retro_atomic_store_release_int(&bt->action_busy, 0);
}

static void winbt_join_action(winbt_t *bt)
{
   if (bt->action)
      sthread_join(bt->action);
   bt->action = NULL;
}

static void *winbt_init(void)
{
   winbt_t *bt = (winbt_t*)calloc(1, sizeof(*bt));

   if (!bt)
      return NULL;
   if (!(bt->lib = LoadLibraryA("bthprops.cpl")))
   {
      free(bt);
      return NULL;
   }
   bt->find_radio   = (winbt_find_radio_t)GetProcAddress(bt->lib,
         "BluetoothFindFirstRadio");
   bt->radio_close  = (winbt_radio_close_t)GetProcAddress(bt->lib,
         "BluetoothFindRadioClose");
   bt->radio_info   = (winbt_radio_info_t)GetProcAddress(bt->lib,
         "BluetoothGetRadioInfo");
   bt->find_first   = (winbt_find_first_t)GetProcAddress(bt->lib,
         "BluetoothFindFirstDevice");
   bt->find_next    = (winbt_find_next_t)GetProcAddress(bt->lib,
         "BluetoothFindNextDevice");
   bt->find_close   = (winbt_find_close_t)GetProcAddress(bt->lib,
         "BluetoothFindDeviceClose");
   bt->authenticate = (winbt_authenticate_t)GetProcAddress(bt->lib,
         "BluetoothAuthenticateDevice");
   bt->service      = (winbt_service_t)GetProcAddress(bt->lib,
         "BluetoothSetServiceState");
   bt->remove       = (winbt_remove_t)GetProcAddress(bt->lib,
         "BluetoothRemoveDevice");
   if (     !bt->find_radio || !bt->radio_close || !bt->radio_info
         || !bt->find_first || !bt->find_next   || !bt->find_close
         || !bt->authenticate || !bt->service   || !bt->remove)
   {
      FreeLibrary(bt->lib);
      free(bt);
      return NULL;
   }
   retro_atomic_int_init(&bt->action_busy, 0);
   return bt;
}

static void winbt_free(void *data)
{
   winbt_t *bt = (winbt_t*)data;
   if (!bt)
      return;
   if (bt->scan)
      sthread_join(bt->scan);
   winbt_join_action(bt);
   FreeLibrary(bt->lib);
   free(bt);
}

static void winbt_scan_begin(void *data)
{
   winbt_t *bt = (winbt_t*)data;
   if (!bt || bt->scan)
      return;
   bt->scan = sthread_create(winbt_scan_thread, bt);
}

static void winbt_scan_end(void *data)
{
   winbt_t *bt = (winbt_t*)data;
   if (!bt || !bt->scan)
      return;
   sthread_join(bt->scan);
   bt->scan  = NULL;
   memcpy(bt->devices, bt->found, sizeof(bt->found[0]) * bt->found_count);
   bt->count = bt->found_count;
}

static void winbt_get_devices(void *data, struct string_list *list)
{
   winbt_t *bt = (winbt_t*)data;
   union string_list_elem_attr attr;
   unsigned i;

   attr.i = 0;
   if (!bt)
      return;
   for (i = 0; i < bt->count; i++)
   {
      char name[BLUETOOTH_MAX_NAME_SIZE];
      WideCharToMultiByte(CP_UTF8, 0, bt->devices[i].szName, -1, name,
            sizeof(name), NULL, NULL);
      name[sizeof(name) - 1] = '\0';
      string_list_append(list, name, attr);
   }
}

static bool winbt_device_is_connected(void *data, unsigned i)
{
   winbt_t *bt = (winbt_t*)data;
   return bt && i < bt->count && bt->devices[i].fConnected;
}

static void winbt_device_get_sublabel(void *data, char *s, unsigned i,
      size_t len)
{
   winbt_t *bt = (winbt_t*)data;
   const BYTE *a;
   if (!bt || i >= bt->count)
      return;
   a = bt->devices[i].Address.rgBytes;
   snprintf(s, len, "%02X:%02X:%02X:%02X:%02X:%02X",
         a[5], a[4], a[3], a[2], a[1], a[0]);
}

static bool winbt_start_action(winbt_t *bt, unsigned i, bool remove)
{
   if (!bt || i >= bt->count)
      return false;
   if (retro_atomic_load_acquire_int(&bt->action_busy))
      return false;
   winbt_join_action(bt);
   bt->target        = bt->devices[i];
   bt->target_remove = remove;
   retro_atomic_store_release_int(&bt->action_busy, 1);
   if (!(bt->action = sthread_create(winbt_action_thread, bt)))
   {
      retro_atomic_store_release_int(&bt->action_busy, 0);
      return false;
   }
   return true;
}

static bool winbt_connect_device(void *data, unsigned i)
{
   return winbt_start_action((winbt_t*)data, i, false);
}

static bool winbt_remove_device(void *data, unsigned i)
{
   return winbt_start_action((winbt_t*)data, i, true);
}

bluetooth_driver_t bluetooth_windows = {
   winbt_init,
   winbt_free,
   winbt_scan_begin,
   winbt_scan_end,
   winbt_get_devices,
   winbt_device_is_connected,
   winbt_device_get_sublabel,
   winbt_connect_device,
   winbt_remove_device,
   "windows",
};
