/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2026 - Daniel De Matteis
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

/* Pads bound to Windows' own WinUSB driver, reached through winusb.dll,
 * which is loaded when the driver starts: nothing is linked, and a
 * Windows without it has no pads here. Windows' HID driver will not
 * pass a DualShock 3 the report that starts it; bound to WinUSB, the
 * pad is the program's, and its handler starts it.
 *
 * Each pad has a thread of its own that waits on its reports and hands
 * the newest to the frontend through three buffers swapped with an
 * atomic exchange: the frontend takes it at the poll without a call
 * into the system, and nothing takes a lock. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <setupapi.h>

#include <boolean.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <retro_atomic.h>
#include <retro_miscellaneous.h>
#include <rthreads/rthreads.h>

#include "../connect/joypad_connection.h"
#include "../input_defines.h"
#include "../../tasks/tasks_internal.h"
#include "../input_driver.h"
#include "../../verbosity.h"

/* winusb.h's, declared here: older SDKs do not have it */
typedef struct
{
   UCHAR  RequestType;
   UCHAR  Request;
   USHORT Value;
   USHORT Index;
   USHORT Length;
} winusb_setup_packet_t;

typedef char winusb_setup_packet_size[
   (sizeof(winusb_setup_packet_t) == 8) ? 1 : -1];

typedef struct
{
   HMODULE lib;
   BOOL (WINAPI *Initialize)(HANDLE, void**);
   BOOL (WINAPI *Free)(void*);
   BOOL (WINAPI *ControlTransfer)(void*, winusb_setup_packet_t,
         UCHAR*, ULONG, ULONG*, OVERLAPPED*);
   BOOL (WINAPI *ReadPipe)(void*, UCHAR, UCHAR*, ULONG, ULONG*,
         OVERLAPPED*);
   BOOL (WINAPI *AbortPipe)(void*, UCHAR);
   BOOL (WINAPI *GetOverlappedResult)(void*, OVERLAPPED*, ULONG*, BOOL);
} winusb_api_t;

/* cfgmgr32.h's, from Windows 8: told of a device interface arriving, a
 * pad bound while the driver runs is looked for. */
typedef struct
{
   DWORD cbSize;
   DWORD Flags;
   DWORD FilterType;
   DWORD Reserved;
   union
   {
      GUID  ClassGuid;
      WCHAR InstanceId[200];
   } u;
} winusb_cm_filter_t;

typedef DWORD (CALLBACK *winusb_cm_callback_t)(HANDLE, void*, DWORD,
      void*, DWORD);
typedef DWORD (WINAPI *winusb_cm_register_t)(winusb_cm_filter_t*, void*,
      winusb_cm_callback_t, HANDLE*);
typedef DWORD (WINAPI *winusb_cm_unregister_t)(HANDLE);

#define WINUSB_CM_FILTER_ALL_INTERFACES 0x1
#define WINUSB_CM_DEVICEINTERFACE       0
#define WINUSB_CM_INTERFACE_ARRIVAL     0

#define WINUSB_REPORT_MAX 64
#define WINUSB_EP_IN      0x81
/* an index into buf, with this set when the writer has left a report
 * there the frontend has not taken */
#define WINUSB_FRESH      4

typedef struct winusb_device
{
   struct winusb_device *next;
   struct winusb_hid *hid;
   HANDLE file;
   void *usb;
   HANDLE ctrl_event;
   HANDLE quit_event;
   sthread_t *reader;
   int32_t slot;
   uint16_t vid;
   uint16_t pid;
   char instance[MAX_PATH];
   char name[NAME_MAX_LENGTH];

   /* three report buffers: the reader's, the one in between, and the
    * frontend's; 'middle' is the one in between, swapped by both */
   uint8_t buf[3][WINUSB_REPORT_MAX];
   ULONG len[3];
   retro_atomic_int_t middle;
   int write_idx;   /* the reader's */
   int read_idx;    /* the frontend's */
   retro_atomic_int_t quit;
   retro_atomic_int_t gone;
} winusb_device_t;

typedef struct winusb_hid
{
   joypad_connection_t *slots;
   winusb_device_t *devices;
   HMODULE cfgmgr;
   winusb_cm_unregister_t cm_unregister;
   HANDLE notification;
   retro_atomic_int_t rescan;
} winusb_hid_t;

extern hid_driver_t winusb_hid;

/* filled in by the driver's start, or by a test before it */
static winusb_api_t winusb_api;

static bool winusb_api_load(void)
{
   HMODULE lib;

   if (winusb_api.Initialize)
      return true;
   if (!(lib = LoadLibraryA("winusb.dll")))
      return false;
   winusb_api.Initialize          = (BOOL (WINAPI*)(HANDLE, void**))
      GetProcAddress(lib, "WinUsb_Initialize");
   winusb_api.Free                = (BOOL (WINAPI*)(void*))
      GetProcAddress(lib, "WinUsb_Free");
   winusb_api.ControlTransfer     = (BOOL (WINAPI*)(void*,
            winusb_setup_packet_t, UCHAR*, ULONG, ULONG*, OVERLAPPED*))
      GetProcAddress(lib, "WinUsb_ControlTransfer");
   winusb_api.ReadPipe            = (BOOL (WINAPI*)(void*, UCHAR, UCHAR*,
            ULONG, ULONG*, OVERLAPPED*))
      GetProcAddress(lib, "WinUsb_ReadPipe");
   winusb_api.AbortPipe           = (BOOL (WINAPI*)(void*, UCHAR))
      GetProcAddress(lib, "WinUsb_AbortPipe");
   winusb_api.GetOverlappedResult = (BOOL (WINAPI*)(void*, OVERLAPPED*,
            ULONG*, BOOL))
      GetProcAddress(lib, "WinUsb_GetOverlappedResult");
   if (     !winusb_api.Initialize || !winusb_api.Free
         || !winusb_api.ControlTransfer || !winusb_api.ReadPipe
         || !winusb_api.AbortPipe || !winusb_api.GetOverlappedResult)
   {
      FreeLibrary(lib);
      memset(&winusb_api, 0, sizeof(winusb_api));
      return false;
   }
   winusb_api.lib = lib;
   return true;
}

/* The pads whose handlers this driver can start. */
static bool winusb_hid_handled(uint16_t vid, uint16_t pid)
{
   joypad_connection_entry_t *entry = find_connection_entry(vid, pid, "");
   return entry && entry->iface == &pad_connection_ps3;
}

/* A control transfer on the default pipe, waited for: the length moved,
 * or -1. */
static int32_t winusb_hid_control(winusb_device_t *dev, UCHAR type,
      UCHAR request, USHORT value, uint8_t *data, size_t len)
{
   OVERLAPPED ov;
   ULONG got = 0;
   winusb_setup_packet_t setup;

   setup.RequestType = type;
   setup.Request     = request;
   setup.Value       = value;
   setup.Index       = 0; /* the interface */
   setup.Length      = (USHORT)len;
   memset(&ov, 0, sizeof(ov));
   ov.hEvent         = dev->ctrl_event;
   ResetEvent(ov.hEvent);
   if (     !winusb_api.ControlTransfer(dev->usb, setup, data, (ULONG)len,
               NULL, &ov)
         && GetLastError() != ERROR_IO_PENDING)
      return -1;
   if (!winusb_api.GetOverlappedResult(dev->usb, &ov, &got, TRUE))
      return -1;
   return (int32_t)got;
}

static void winusb_hid_reader(void *data)
{
   OVERLAPPED ov;
   HANDLE events[2];
   winusb_device_t *dev = (winusb_device_t*)data;

   memset(&ov, 0, sizeof(ov));
   if (!(ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL)))
   {
      retro_atomic_store_release_int(&dev->gone, 1);
      return;
   }
   events[0] = dev->quit_event;
   events[1] = ov.hEvent;

   while (!retro_atomic_load_acquire_int(&dev->quit))
   {
      ULONG got = 0;
      int w     = dev->write_idx;
      BOOL ok;

      ResetEvent(ov.hEvent);
      ok = winusb_api.ReadPipe(dev->usb, WINUSB_EP_IN, dev->buf[w],
            WINUSB_REPORT_MAX, NULL, &ov);
      if (!ok && GetLastError() == ERROR_IO_PENDING)
      {
         /* Abort only after the reader has submitted its operation. */
         if (WaitForMultipleObjects(2, events, FALSE, INFINITE)
               != WAIT_OBJECT_0 + 1)
            winusb_api.AbortPipe(dev->usb, WINUSB_EP_IN);
         ok = winusb_api.GetOverlappedResult(dev->usb, &ov, &got, TRUE);
      }
      else if (ok)
         ok = winusb_api.GetOverlappedResult(dev->usb, &ov, &got, TRUE);
      if (!ok)
      {
         /* stopped by the frontend, or the pad is gone */
         if (!retro_atomic_load_acquire_int(&dev->quit))
            retro_atomic_store_release_int(&dev->gone, 1);
         break;
      }
      if (got)
      {
         dev->len[w]    = got;
         dev->write_idx = retro_atomic_exchange_int(&dev->middle,
               w | WINUSB_FRESH) & 3;
      }
   }
   CloseHandle(ov.hEvent);
}

static void winusb_hid_close(winusb_device_t *dev)
{
   if (dev->reader)
   {
      retro_atomic_store_release_int(&dev->quit, 1);
      SetEvent(dev->quit_event);
      sthread_join(dev->reader);
   }
   if (dev->usb)
      winusb_api.Free(dev->usb);
   if (dev->file != INVALID_HANDLE_VALUE)
      CloseHandle(dev->file);
   if (dev->ctrl_event)
      CloseHandle(dev->ctrl_event);
   if (dev->quit_event)
      CloseHandle(dev->quit_event);
   free(dev);
}

static winusb_device_t *winusb_hid_find(winusb_hid_t *hid,
      const char *instance)
{
   winusb_device_t *dev;
   for (dev = hid->devices; dev; dev = dev->next)
      if (string_is_equal_noncase(dev->instance, instance))
         return dev;
   return NULL;
}

/* A pad's interface opened, its reader started and its handler given
 * it. */
static void winusb_hid_open(winusb_hid_t *hid, const char *path,
      const char *instance, uint16_t vid, uint16_t pid)
{
   winusb_device_t *dev;

   if (winusb_hid_find(hid, instance))
      return;
   if (!(dev = (winusb_device_t*)calloc(1, sizeof(*dev))))
      return;
   dev->hid       = hid;
   dev->slot      = -1;
   dev->vid       = vid;
   dev->pid       = pid;
   dev->read_idx  = 0;
   dev->write_idx = 1;
   retro_atomic_int_init(&dev->middle, 2);
   retro_atomic_int_init(&dev->quit, 0);
   retro_atomic_int_init(&dev->gone, 0);
   strlcpy(dev->instance, instance, sizeof(dev->instance));
   strlcpy(dev->name, "PLAYSTATION(R)3 Controller", sizeof(dev->name));

   dev->file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
   if (dev->file == INVALID_HANDLE_VALUE)
   {
      RARCH_WARN("[WinUSB] %s cannot be opened (error %lu).\n",
            path, (unsigned long)GetLastError());
      free(dev);
      return;
   }
   if (     !(dev->ctrl_event = CreateEventA(NULL, TRUE, FALSE, NULL))
         || !(dev->quit_event = CreateEventA(NULL, TRUE, FALSE, NULL))
         || !winusb_api.Initialize(dev->file, &dev->usb))
   {
      RARCH_WARN("[WinUSB] %s cannot be used (error %lu).\n",
            path, (unsigned long)GetLastError());
      dev->usb = NULL;
      winusb_hid_close(dev);
      return;
   }

   /* the pad's handler starts it here, through the control pipe */
   dev->slot = pad_connection_pad_init(hid->slots, dev->name, vid, pid,
         dev, &winusb_hid);
   if (dev->slot < 0 || !hid->slots[dev->slot].connection)
   {
      RARCH_ERR("[WinUSB] \"%s\" could not be started.\n", dev->name);
      if (dev->slot >= 0)
         pad_connection_pad_deinit(&hid->slots[dev->slot], dev->slot);
      winusb_hid_close(dev);
      return;
   }
   if (!(dev->reader = sthread_create(winusb_hid_reader, dev)))
   {
      pad_connection_pad_deinit(&hid->slots[dev->slot], dev->slot);
      winusb_hid_close(dev);
      return;
   }

   dev->next    = hid->devices;
   hid->devices = dev;
   RARCH_LOG("[WinUSB] \"%s\" on port %d.\n", dev->name, dev->slot + 1);
   input_autoconfigure_connect(dev->name, NULL, NULL, "hid",
         dev->slot, vid, pid);
}

static void winusb_hid_remove(winusb_hid_t *hid, winusb_device_t *dev)
{
   winusb_device_t **link;

   for (link = &hid->devices; *link; link = &(*link)->next)
   {
      if (*link != dev)
         continue;
      *link = dev->next;
      input_autoconfigure_disconnect(dev->slot, dev->name);
      pad_connection_pad_deinit(&hid->slots[dev->slot], dev->slot);
      winusb_hid_close(dev);
      return;
   }
}

/* "{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}" */
static bool winusb_hid_parse_guid(const char *s, GUID *guid)
{
   unsigned b[8], d1, d2, d3;
   int i;
   if (sscanf(s, "{%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x}",
            &d1, &d2, &d3, &b[0], &b[1], &b[2], &b[3], &b[4], &b[5],
            &b[6], &b[7]) != 11)
      return false;
   guid->Data1 = d1;
   guid->Data2 = (unsigned short)d2;
   guid->Data3 = (unsigned short)d3;
   for (i = 0; i < 8; i++)
      guid->Data4[i] = (unsigned char)b[i];
   return true;
}

/* The device's path under an interface class, matched by its instance
 * where devices share the class. */
static bool winusb_hid_class_path(const GUID *guid, const char *instance,
      char *path, size_t len)
{
   HDEVINFO ifs;
   DWORD i;
   bool found = false;

   ifs = SetupDiGetClassDevsA(guid, NULL, NULL,
         DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
   if (ifs == INVALID_HANDLE_VALUE)
      return false;
   for (i = 0; !found; i++)
   {
      SP_DEVICE_INTERFACE_DATA ifd;
      SP_DEVINFO_DATA ifinfo;
      char id[MAX_PATH];
      union
      {
         SP_DEVICE_INTERFACE_DETAIL_DATA_A d;
         char b[sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A) + MAX_PATH];
      } detail;

      ifd.cbSize = sizeof(ifd);
      if (!SetupDiEnumDeviceInterfaces(ifs, NULL, guid, i, &ifd))
         break;
      ifinfo.cbSize   = sizeof(ifinfo);
      detail.d.cbSize = sizeof(detail.d);
      if (     !SetupDiGetDeviceInterfaceDetailA(ifs, &ifd, &detail.d,
                  sizeof(detail), NULL, &ifinfo)
            || !SetupDiGetDeviceInstanceIdA(ifs, &ifinfo, id, sizeof(id),
                  NULL))
         continue;
      if (string_is_equal_noncase(id, instance))
      {
         strlcpy(path, detail.d.DevicePath, len);
         found = true;
      }
   }
   SetupDiDestroyDeviceInfoList(ifs);
   return found;
}

/* A path WinUSB can be opened on: the USB device interface the hub
 * gives every device, which serves however the device was bound to
 * WinUSB, or else the interface its driver package named among the
 * device's settings. */
static bool winusb_hid_interface_path(HDEVINFO devs, SP_DEVINFO_DATA *info,
      const char *instance, char *path, size_t len)
{
   /* GUID_DEVINTERFACE_USB_DEVICE */
   static const GUID usb_device = { 0xa5dcbf10, 0x6530, 0x11d2,
      { 0x90, 0x1f, 0x00, 0xc0, 0x4f, 0xb9, 0x51, 0xed } };
   char value[256];
   DWORD type, size = sizeof(value) - 2;
   GUID guid;
   HKEY key;

   if (winusb_hid_class_path(&usb_device, instance, path, len))
      return true;
   key = SetupDiOpenDevRegKey(devs, info, DICS_FLAG_GLOBAL, 0,
         DIREG_DEV, KEY_READ);
   if (key == INVALID_HANDLE_VALUE)
      return false;
   memset(value, 0, sizeof(value));
   if (RegQueryValueExA(key, "DeviceInterfaceGUIDs", NULL, &type,
            (BYTE*)value, &size) != ERROR_SUCCESS)
   {
      size = sizeof(value) - 2;
      if (RegQueryValueExA(key, "DeviceInterfaceGUID", NULL, &type,
               (BYTE*)value, &size) != ERROR_SUCCESS)
         *value = '\0';
   }
   RegCloseKey(key);
   return winusb_hid_parse_guid(value, &guid)
      && winusb_hid_class_path(&guid, instance, path, len);
}

/* USB devices bound to WinUSB whose ids name a handled pad. */
static void winusb_hid_scan(winusb_hid_t *hid)
{
   DWORD i;
   HDEVINFO devs = SetupDiGetClassDevsA(NULL, "USB", NULL,
         DIGCF_ALLCLASSES | DIGCF_PRESENT);

   if (devs == INVALID_HANDLE_VALUE)
      return;
   for (i = 0; ; i++)
   {
      SP_DEVINFO_DATA info;
      char service[64];
      char instance[MAX_PATH];
      char path[MAX_PATH];
      unsigned vid, pid;
      const char *ids;

      info.cbSize = sizeof(info);
      if (!SetupDiEnumDeviceInfo(devs, i, &info))
         break;
      if (     !SetupDiGetDeviceInstanceIdA(devs, &info, instance,
                  sizeof(instance), NULL)
            || !(ids = strstr(instance, "VID_"))
            || sscanf(ids, "VID_%4x&PID_%4x", &vid, &pid) != 2
            || !winusb_hid_handled((uint16_t)vid, (uint16_t)pid)
            || winusb_hid_find(hid, instance))
         continue;
      if (     !SetupDiGetDeviceRegistryPropertyA(devs, &info, SPDRP_SERVICE,
                  NULL, (BYTE*)service, sizeof(service), NULL)
            || !string_is_equal_noncase(service, "WinUSB"))
      {
         RARCH_LOG("[WinUSB] %s is not bound to WinUSB.\n", instance);
         continue;
      }
      if (winusb_hid_interface_path(devs, &info, instance,
               path, sizeof(path)))
         winusb_hid_open(hid, path, instance, (uint16_t)vid, (uint16_t)pid);
   }
   SetupDiDestroyDeviceInfoList(devs);
}

static DWORD CALLBACK winusb_hid_notified(HANDLE notification,
      void *context, DWORD action, void *data, DWORD size)
{
   winusb_hid_t *hid = (winusb_hid_t*)context;
   if (action == WINUSB_CM_INTERFACE_ARRIVAL)
      retro_atomic_store_release_int(&hid->rescan, 1);
   return ERROR_SUCCESS;
}

static void winusb_hid_free(const void *data)
{
   winusb_hid_t *hid = (winusb_hid_t*)data;

   if (!hid)
      return;
   /* no callback after this */
   if (hid->notification)
      hid->cm_unregister(hid->notification);
   if (hid->cfgmgr)
      FreeLibrary(hid->cfgmgr);
   while (hid->devices)
      winusb_hid_remove(hid, hid->devices);
   if (hid->slots)
      pad_connection_destroy(hid->slots);
   free(hid);
}

static void *winusb_hid_init(void)
{
   winusb_hid_t *hid;

   if (!winusb_api_load())
      return NULL;
   if (!(hid = (winusb_hid_t*)calloc(1, sizeof(*hid))))
      return NULL;
   if (!(hid->slots = pad_connection_init(MAX_USERS)))
   {
      free(hid);
      return NULL;
   }
   retro_atomic_int_init(&hid->rescan, 0);

   /* listening before the scan, so a pad bound during it is not
    * missed; without it (before Windows 8) the pads bound at start */
   if ((hid->cfgmgr = LoadLibraryA("cfgmgr32.dll")))
   {
      winusb_cm_register_t reg = (winusb_cm_register_t)
         GetProcAddress(hid->cfgmgr, "CM_Register_Notification");
      hid->cm_unregister       = (winusb_cm_unregister_t)
         GetProcAddress(hid->cfgmgr, "CM_Unregister_Notification");
      if (reg && hid->cm_unregister)
      {
         winusb_cm_filter_t filter;
         memset(&filter, 0, sizeof(filter));
         filter.cbSize     = sizeof(filter);
         filter.Flags      = WINUSB_CM_FILTER_ALL_INTERFACES;
         filter.FilterType = WINUSB_CM_DEVICEINTERFACE;
         if (reg(&filter, hid, winusb_hid_notified, &hid->notification)
               != ERROR_SUCCESS)
            hid->notification = NULL;
      }
   }
   winusb_hid_scan(hid);
   return hid;
}

static void winusb_hid_poll(void *data)
{
   winusb_hid_t *hid = (winusb_hid_t*)data;
   winusb_device_t *dev, *next;

   if (!hid)
      return;
   if (     retro_atomic_load_acquire_int(&hid->rescan)
         && retro_atomic_exchange_int(&hid->rescan, 0))
      winusb_hid_scan(hid);

   for (dev = hid->devices; dev; dev = next)
   {
      next = dev->next;
      if (retro_atomic_load_acquire_int(&dev->middle) & WINUSB_FRESH)
      {
         dev->read_idx = retro_atomic_exchange_int(&dev->middle,
               dev->read_idx) & 3;
         pad_connection_packet(&hid->slots[dev->slot], dev->slot,
               dev->buf[dev->read_idx], (uint32_t)dev->len[dev->read_idx]);
      }
      if (retro_atomic_load_acquire_int(&dev->gone))
      {
         RARCH_LOG("[WinUSB] \"%s\" is gone.\n", dev->name);
         winusb_hid_remove(hid, dev);
      }
   }
}

static bool winusb_hid_joypad_query(void *data, unsigned pad)
{
   return pad < MAX_USERS;
}

static const char *winusb_hid_joypad_name(void *data, unsigned pad)
{
   winusb_hid_t *hid = (winusb_hid_t*)data;
   if (!hid || pad >= MAX_USERS)
      return NULL;
   return pad_connection_get_name(&hid->slots[pad], pad);
}

static void winusb_hid_joypad_get_buttons(void *data, unsigned port,
      input_bits_t *state)
{
   winusb_hid_t *hid = (winusb_hid_t*)data;
   if (hid && port < MAX_USERS)
      pad_connection_get_buttons(&hid->slots[port], port, state);
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t winusb_hid_joypad_button(void *data,
      unsigned port, uint16_t joykey)
{
   input_bits_t buttons;

   if (port >= MAX_USERS || GET_HAT_DIR(joykey) || joykey >= 32)
      return 0;
   winusb_hid_joypad_get_buttons(data, port, &buttons);
   return BIT256_GET(buttons, joykey) != 0;
}

static int16_t winusb_hid_joypad_axis(void *data,
      unsigned port, uint32_t joyaxis)
{
   winusb_hid_t *hid = (winusb_hid_t*)data;

   if (!hid || port >= MAX_USERS)
      return 0;
   /* the sticks, then what a handler has past them: a DualShock 3's
    * pressures */
   if (AXIS_NEG_GET(joyaxis) < 16)
   {
      int16_t val = pad_connection_get_axis(&hid->slots[port],
            port, AXIS_NEG_GET(joyaxis));
      if (val < 0)
         return val;
   }
   else if (AXIS_POS_GET(joyaxis) < 16)
   {
      int16_t val = pad_connection_get_axis(&hid->slots[port],
            port, AXIS_POS_GET(joyaxis));
      if (val > 0)
         return val;
   }
   return 0;
}

static int16_t winusb_hid_joypad_state(
      void *data,
      rarch_joypad_info_t *joypad_info,
      const uint16_t *joykeys,
      const uint32_t *joyaxes,
      unsigned port)
{
   unsigned i;
   int16_t ret       = 0;
   uint16_t port_idx = joypad_info->joy_idx;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      if (     joykeys[i] != NO_BTN
            && winusb_hid_joypad_button(data, port_idx, joykeys[i]))
         ret |= (1 << i);
      else if (joyaxes[i] != AXIS_NONE
            && ((float)abs(winusb_hid_joypad_axis(data, port_idx, joyaxes[i]))
               / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }
   return ret;
}

static bool winusb_hid_joypad_rumble(void *data, unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   winusb_hid_t *hid = (winusb_hid_t*)data;
   if (!hid || pad >= MAX_USERS)
      return false;
   return pad_connection_rumble(&hid->slots[pad], pad, effect, strength);
}

/* HID class requests on the control pipe: SET_REPORT and GET_REPORT,
 * the report's type and ID in the value. The report goes with its ID
 * first. */
static int32_t winusb_hid_set_report(void *handle, uint8_t report_type,
      uint8_t report_id, uint8_t *s, size_t len)
{
   winusb_device_t *dev = (winusb_device_t*)handle;
   if (!dev || !len || s[0] != report_id)
      return -1;
   return winusb_hid_control(dev, 0x21, 0x09,
         (USHORT)((report_type << 8) | report_id), s, len);
}

static int32_t winusb_hid_get_report(void *handle, uint8_t report_type,
      uint8_t report_id, uint8_t *s, size_t len)
{
   winusb_device_t *dev = (winusb_device_t*)handle;
   if (!dev || !len)
      return -1;
   return winusb_hid_control(dev, 0xa1, 0x01,
         (USHORT)((report_type << 8) | report_id), s, len);
}

static void winusb_hid_send_control(void *handle, uint8_t *s, size_t len)
{
   if (len)
      winusb_hid_set_report(handle, HID_REPORT_OUTPUT, s[0], s, len);
}

static winusb_device_t *winusb_hid_pad(void *data, unsigned pad)
{
   winusb_hid_t *hid = (winusb_hid_t*)data;
   winusb_device_t *dev;
   if (!hid)
      return NULL;
   for (dev = hid->devices; dev; dev = dev->next)
      if (dev->slot == (int32_t)pad)
         return dev;
   return NULL;
}

/* A DualShock 3 keeps the address it connects to over Bluetooth in
 * feature report 0xF5, from its third byte. */
static bool winusb_hid_get_bt_host(void *data, unsigned pad, uint8_t *addr)
{
   uint8_t buf[8];
   winusb_device_t *dev = winusb_hid_pad(data, pad);

   if (!dev)
      return false;
   memset(buf, 0, sizeof(buf));
   if (winusb_hid_get_report(dev, HID_REPORT_FEATURE, 0xf5, buf,
            sizeof(buf)) < (int32_t)sizeof(buf))
      return false;
   memcpy(addr, buf + 2, 6);
   return true;
}

static bool winusb_hid_set_bt_host(void *data, unsigned pad,
      const uint8_t *addr)
{
   uint8_t buf[8];
   winusb_device_t *dev = winusb_hid_pad(data, pad);

   if (!dev)
      return false;
   buf[0] = 0xf5;
   buf[1] = 0x00;
   memcpy(buf + 2, addr, 6);
   return winusb_hid_set_report(dev, HID_REPORT_FEATURE, 0xf5, buf,
         sizeof(buf)) >= 0;
}

hid_driver_t winusb_hid = {
   winusb_hid_init,
   winusb_hid_joypad_query,
   winusb_hid_free,
   winusb_hid_joypad_button,
   winusb_hid_joypad_state,
   winusb_hid_joypad_get_buttons,
   winusb_hid_joypad_axis,
   winusb_hid_poll,
   winusb_hid_joypad_rumble,
   winusb_hid_joypad_name,
   "winusb",
   winusb_hid_send_control,
   winusb_hid_set_report,
   winusb_hid_get_report,
   NULL, /* set_idle */
   NULL, /* set_protocol */
   NULL, /* read */
   winusb_hid_get_bt_host,
   winusb_hid_set_bt_host
};
