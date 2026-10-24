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
 * the thread is joined; a pairing works on a copy of its device. A
 * thread still running when the driver is freed is let go, not waited
 * for - a pairing can wait on Windows for good - and the last of it and
 * the driver to be done frees the driver.
 *
 * A Wii controller's PIN request is answered here, so Windows asks the
 * user for none. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <setupapi.h>
#include <bluetoothapis.h>

#include <boolean.h>
#include <compat/strl.h>
#include <lists/string_list.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "../bluetooth_driver.h"
#include "../../msg_hash.h"
#include "../../runloop.h"
#include "../../verbosity.h"

#define WINBT_MAX_DEVICES 32
#ifndef BLUETOOTH_SERVICE_ENABLE
#define BLUETOOTH_SERVICE_ENABLE 0x01
#endif
/* the inquiry, in units of 1.28 s: a controller in pairing mode
 * answers within the first */
#define WINBT_INQUIRY     4

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
typedef DWORD (WINAPI *winbt_device_info_t)(HANDLE, BLUETOOTH_DEVICE_INFO*);
typedef DWORD (WINAPI *winbt_register_auth_t)(const BLUETOOTH_DEVICE_INFO*,
      HBLUETOOTH_AUTHENTICATION_REGISTRATION*,
      PFN_AUTHENTICATION_CALLBACK_EX, PVOID);
typedef BOOL (WINAPI *winbt_unregister_auth_t)(
      HBLUETOOTH_AUTHENTICATION_REGISTRATION);
typedef DWORD (WINAPI *winbt_send_auth_t)(HANDLE,
      PBLUETOOTH_AUTHENTICATE_RESPONSE);
typedef DWORD (WINAPI *winbt_authenticate_ex_t)(HWND, HANDLE,
      BLUETOOTH_DEVICE_INFO*, PBLUETOOTH_OOB_DATA_INFO,
      int); /* AUTHENTICATION_REQUIREMENTS, named otherwise by mingw */

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
   winbt_device_info_t device_info;
   /* Vista's: where these are missing, the PIN is given the older way */
   winbt_register_auth_t register_auth;
   winbt_unregister_auth_t unregister_auth;
   winbt_send_auth_t send_auth;
   winbt_authenticate_ex_t authenticate_ex;

   /* the driver's own, and one for each thread running */
   retro_atomic_int_t refs;

   /* the frontend's: as the last scan found them */
   BLUETOOTH_DEVICE_INFO devices[WINBT_MAX_DEVICES];
   unsigned count;

   /* the scan thread's until it is joined */
   sthread_t *scan;
   retro_atomic_int_t scan_done;
   /* the adapter is on Windows' generic driver: told when a scan found
    * nothing */
   retro_atomic_int_t generic;
   BLUETOOTH_DEVICE_INFO found[WINBT_MAX_DEVICES];
   unsigned found_count;

   /* a pairing or forgetting: one at a time, on a copy */
   sthread_t *action;
   retro_atomic_int_t action_busy;
   BLUETOOTH_DEVICE_INFO target;
   bool target_remove;
   /* the pairing's, for its PIN request */
   HANDLE auth_radio;
   UCHAR pin[6];
} winbt_t;

static void winbt_unref(winbt_t *bt)
{
   if (retro_atomic_fetch_sub_int(&bt->refs, 1) == 1)
   {
      FreeLibrary(bt->lib);
      free(bt);
   }
}

static BOOL CALLBACK winbt_auth_answer(LPVOID param,
      PBLUETOOTH_AUTHENTICATION_CALLBACK_PARAMS p)
{
   winbt_t *bt = (winbt_t*)param;
   BLUETOOTH_AUTHENTICATE_RESPONSE r;

   memset(&r, 0, sizeof(r));
   r.bthAddressRemote = p->deviceInfo.Address;
   r.authMethod       = p->authenticationMethod;
   if (p->authenticationMethod == BLUETOOTH_AUTHENTICATION_METHOD_LEGACY)
   {
      memcpy(r.pinInfo.pin, bt->pin, sizeof(bt->pin));
      r.pinInfo.pinLength = sizeof(bt->pin);
   }
   else if (p->authenticationMethod
         == BLUETOOTH_AUTHENTICATION_METHOD_NUMERIC_COMPARISON)
      r.numericCompInfo.NumericValue = p->Numeric_Value;
   else
      r.negativeResponse = TRUE;
   bt->send_auth(bt->auth_radio, &r);
   return TRUE;
}

/* Paired with the radio's address as its PIN, given when Windows asks */
static DWORD winbt_pair_wii(winbt_t *bt, HANDLE radio,
      BLUETOOTH_DEVICE_INFO *d, const BLUETOOTH_ADDRESS *own)
{
   HBLUETOOTH_AUTHENTICATION_REGISTRATION reg = NULL;
   WCHAR pin[6];
   DWORD err;
   unsigned i;

   for (i = 0; i < 6; i++)
   {
      bt->pin[i] = own->rgBytes[i];
      pin[i]     = own->rgBytes[i];
   }
   bt->auth_radio = radio;
   if (     !bt->register_auth || !bt->unregister_auth
         || !bt->send_auth     || !bt->authenticate_ex
         || bt->register_auth(d, &reg, winbt_auth_answer, bt)
            != ERROR_SUCCESS)
      return bt->authenticate(NULL, radio, d, pin, 6);
   err = bt->authenticate_ex(NULL, radio, d, NULL, MITMProtectionNotRequired);
   bt->unregister_auth(reg);
   return err;
}

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

/* Whether the radio is on Windows' own driver, bth.inf, rather than
 * its maker's: the USB device in the Bluetooth class, by its driver's
 * INF. That one often finds no classic device at all - no phone, no
 * Wii controller - and says nothing of it. */
static bool winbt_generic_driver(void)
{
   /* GUID_DEVCLASS_BLUETOOTH */
   static const GUID bt_class = { 0xe0cbf06c, 0xcd8b, 0x4647,
      { 0xbb, 0x8a, 0x26, 0x3b, 0x43, 0xf0, 0xf9, 0x74 } };
   SP_DEVINFO_DATA info;
   DWORD i;
   bool generic = false;
   HDEVINFO devs = SetupDiGetClassDevsA(&bt_class, NULL, NULL,
         DIGCF_PRESENT);

   if (devs == INVALID_HANDLE_VALUE)
      return false;
   for (i = 0; !generic; i++)
   {
      char ids[512];
      char inf[MAX_PATH];
      DWORD size = sizeof(inf);
      DWORD type = 0;
      HKEY key;

      info.cbSize = sizeof(info);
      if (!SetupDiEnumDeviceInfo(devs, i, &info))
         break;
      memset(ids, 0, sizeof(ids));
      if (     !SetupDiGetDeviceRegistryPropertyA(devs, &info,
                  SPDRP_HARDWAREID, NULL, (PBYTE)ids, sizeof(ids) - 2,
                  NULL)
            || strncmp(ids, "USB\\", 4))
         continue;
      key = SetupDiOpenDevRegKey(devs, &info, DICS_FLAG_GLOBAL, 0,
            DIREG_DRV, KEY_READ);
      if (key == INVALID_HANDLE_VALUE)
         continue;
      if (     RegQueryValueExA(key, "InfPath", NULL, &type, (LPBYTE)inf,
                  &size) == ERROR_SUCCESS
            && type == REG_SZ)
      {
         inf[sizeof(inf) - 1] = '\0';
         generic = (_stricmp(inf, "bth.inf") == 0);
      }
      RegCloseKey(key);
   }
   SetupDiDestroyDeviceInfoList(devs);
   return generic;
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
      retro_atomic_store_release_int(&bt->scan_done, 1);
      winbt_unref(bt);
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
         if (bt->found_count < WINBT_MAX_DEVICES)
            bt->found[bt->found_count++] = d;
         memset(&d, 0, sizeof(d));
         d.dwSize = sizeof(d);
      } while (bt->find_next(find, &d));
      bt->find_close(find);
   }
   else
      RARCH_DBG("[Bluetooth] The search found nothing (error %lu).\n",
            (unsigned long)GetLastError());
   CloseHandle(radio);
   if (!bt->found_count && winbt_generic_driver())
   {
      RARCH_WARN("[Bluetooth] The adapter is on Windows' generic driver,"
            " which often finds nothing; its maker's driver is needed.\n");
      retro_atomic_store_release_int(&bt->generic, 1);
   }
   else
      retro_atomic_store_release_int(&bt->generic, 0);
   retro_atomic_store_release_int(&bt->scan_done, 1);
   winbt_unref(bt);
}

/* on screen, from the pairing's thread: the queue takes it across */
static void winbt_notify(enum msg_hash_enums id, const char *name)
{
   char msg[256];
   size_t len = (size_t)snprintf(msg, sizeof(msg), msg_hash_to_str(id), name);
   if (len >= sizeof(msg))
      len = sizeof(msg) - 1;
   runloop_msg_queue_push(msg, len, 1, 180, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
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
      winbt_unref(bt);
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
      /* a name the search had not heard yet */
      if (!d->szName[0] && bt->device_info(radio, d) == ERROR_SUCCESS)
      {
         WideCharToMultiByte(CP_UTF8, 0, d->szName, -1, name,
               sizeof(name), NULL, NULL);
         name[sizeof(name) - 1] = '\0';
      }
      winbt_notify(MSG_BLUETOOTH_PAIRING, name);
      if (winbt_is_wii(d))
      {
         BLUETOOTH_RADIO_INFO info;

         RARCH_LOG("[Bluetooth] Pairing \"%s\" as a Wii controller.\n",
               name);

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
            err = winbt_pair_wii(bt, radio, d, &info.address);
      }
      else if (!d->fAuthenticated)
         err = bt->authenticate(NULL, radio, d, NULL, 0);
      if (err == ERROR_SUCCESS || err == ERROR_NO_MORE_ITEMS)
         err = bt->service(radio, d, &winbt_hid_service,
               BLUETOOTH_SERVICE_ENABLE);
      if (err == ERROR_SUCCESS)
      {
         RARCH_LOG("[Bluetooth] \"%s\" paired.\n", name);
         winbt_notify(MSG_BLUETOOTH_PAIRED, name);
      }
      else
      {
         RARCH_WARN("[Bluetooth] \"%s\" could not be paired (error %lu).\n",
               name, (unsigned long)err);
         winbt_notify(winbt_is_wii(d) ? MSG_BLUETOOTH_PAIRING_FAILED_WII
               : MSG_BLUETOOTH_PAIRING_FAILED, name);
      }
   }
   CloseHandle(radio);
   retro_atomic_store_release_int(&bt->action_busy, 0);
   winbt_unref(bt);
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
   bt->device_info  = (winbt_device_info_t)GetProcAddress(bt->lib,
         "BluetoothGetDeviceInfo");
   bt->register_auth   = (winbt_register_auth_t)GetProcAddress(bt->lib,
         "BluetoothRegisterForAuthenticationEx");
   bt->unregister_auth = (winbt_unregister_auth_t)GetProcAddress(bt->lib,
         "BluetoothUnregisterAuthentication");
   bt->send_auth       = (winbt_send_auth_t)GetProcAddress(bt->lib,
         "BluetoothSendAuthenticationResponseEx");
   bt->authenticate_ex = (winbt_authenticate_ex_t)GetProcAddress(bt->lib,
         "BluetoothAuthenticateDeviceEx");
   if (     !bt->find_radio || !bt->radio_close || !bt->radio_info
         || !bt->find_first || !bt->find_next   || !bt->find_close
         || !bt->authenticate || !bt->service   || !bt->remove
         || !bt->device_info)
   {
      FreeLibrary(bt->lib);
      free(bt);
      return NULL;
   }
   retro_atomic_int_init(&bt->refs, 1);
   retro_atomic_int_init(&bt->action_busy, 0);
   retro_atomic_int_init(&bt->scan_done, 1);
   retro_atomic_int_init(&bt->generic, 0);
   return bt;
}

static void winbt_free(void *data)
{
   winbt_t *bt = (winbt_t*)data;
   if (!bt)
      return;
   if (bt->scan)
      sthread_detach(bt->scan);
   if (bt->action)
      sthread_detach(bt->action);
   bt->scan   = NULL;
   bt->action = NULL;
   winbt_unref(bt);
}

static void winbt_scan_begin(void *data)
{
   winbt_t *bt = (winbt_t*)data;
   if (!bt || bt->scan)
      return;
   retro_atomic_store_release_int(&bt->scan_done, 0);
   retro_atomic_inc_int(&bt->refs);
   if (!(bt->scan = sthread_create(winbt_scan_thread, bt)))
   {
      retro_atomic_dec_int(&bt->refs);
      retro_atomic_store_release_int(&bt->scan_done, 1);
   }
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
   RARCH_LOG("[Bluetooth] Scan found %u device%s.\n", bt->count,
         bt->count == 1 ? "" : "s");
}

static void winbt_device_get_sublabel(void *data, char *s, unsigned i,
      size_t len);

static bool winbt_scan_done(void *data)
{
   winbt_t *bt = (winbt_t*)data;
   return !bt || retro_atomic_load_acquire_int(&bt->scan_done);
}

static bool winbt_adapter_generic(void *data)
{
   winbt_t *bt = (winbt_t*)data;
   return bt && retro_atomic_load_acquire_int(&bt->generic);
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
      /* one that has not said its name yet: by its address */
      if (!name[0])
         winbt_device_get_sublabel(bt, name, i, sizeof(name));
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
   retro_atomic_inc_int(&bt->refs);
   if (!(bt->action = sthread_create(winbt_action_thread, bt)))
   {
      retro_atomic_dec_int(&bt->refs);
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
   winbt_scan_done,
   winbt_adapter_generic
};
