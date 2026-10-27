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

/* Pads with a handler in input/connect, two ways:
 *
 * - on Windows' own HID driver, as Windows installs every pad: opened
 *   through its HID interface and read as hidraw is on Linux, nothing
 *   to set up. Not while beside another joypad driver, which sees them
 *   too.
 * - bound to Windows' WinUSB driver, reached through winusb.dll: the
 *   DualShock 3, which Windows' HID driver will not pass the report
 *   that starts it. Bound to WinUSB, the pad is the program's, and its
 *   handler starts it.
 *
 * The Switch 2 Pro Controller is on Windows' HID driver, but says
 * nothing until it is woken through the bulk pipes of its second
 * interface, which the pad itself has Windows bind to WinUSB.
 *
 * winusb.dll and hid.dll are loaded when the driver starts: nothing is
 * linked, and a Windows without one has no pads that way.
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
#include <hidsdi.h>
#include <hidpi.h>

#include <boolean.h>
#include <compat/strl.h>
#include <compat/strcasestr.h>
#include <string/stdstring.h>
#include <retro_atomic.h>
#include <retro_miscellaneous.h>
#include <rthreads/rthreads.h>
#include <features/features_cpu.h>

#include "../connect/joypad_connection.h"
#include "../common/hid_report_desc.h"
#include "../common/hid_sjoy_output.h"
#include "winusb_wgi.h"
#include "winusb_bind.h"
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
   /* for the Switch 2 Pro Controller's wake alone: may be NULL */
   BOOL (WINAPI *WritePipe)(void*, UCHAR, UCHAR*, ULONG, ULONG*,
         OVERLAPPED*);
   BOOL (WINAPI *QueryInterfaceSettings)(void*, UCHAR, void*);
   BOOL (WINAPI *QueryPipe)(void*, UCHAR, UCHAR, void*);
} winusb_api_t;

/* USB_INTERFACE_DESCRIPTOR and WINUSB_PIPE_INFORMATION */
typedef struct
{
   UCHAR bLength;
   UCHAR bDescriptorType;
   UCHAR bInterfaceNumber;
   UCHAR bAlternateSetting;
   UCHAR bNumEndpoints;
   UCHAR bInterfaceClass;
   UCHAR bInterfaceSubClass;
   UCHAR bInterfaceProtocol;
   UCHAR iInterface;
} winusb_interface_desc_t;

typedef struct
{
   int PipeType;
   UCHAR PipeId;
   USHORT MaximumPacketSize;
   UCHAR Interval;
} winusb_pipe_info_t;

#define WINUSB_PIPE_BULK      2
#define WINUSB_PIPE_INTERRUPT 3

/* hid.dll's, and kernel32's reads and writes, for a pad on Windows'
 * HID driver: a test stands in for the device through them. */
typedef struct
{
   HMODULE lib;
   BOOLEAN (WINAPI *GetAttributes)(HANDLE, PHIDD_ATTRIBUTES);
   BOOLEAN (WINAPI *GetPreparsedData)(HANDLE, PHIDP_PREPARSED_DATA*);
   BOOLEAN (WINAPI *FreePreparsedData)(PHIDP_PREPARSED_DATA);
   NTSTATUS (WINAPI *GetCaps)(PHIDP_PREPARSED_DATA, PHIDP_CAPS);
   NTSTATUS (WINAPI *GetButtonCaps)(HIDP_REPORT_TYPE, PHIDP_BUTTON_CAPS,
         PUSHORT, PHIDP_PREPARSED_DATA);
   NTSTATUS (WINAPI *GetValueCaps)(HIDP_REPORT_TYPE, PHIDP_VALUE_CAPS,
         PUSHORT, PHIDP_PREPARSED_DATA);
   BOOLEAN (WINAPI *GetFeature)(HANDLE, PVOID, ULONG);
   BOOLEAN (WINAPI *SetFeature)(HANDLE, PVOID, ULONG);
   BOOLEAN (WINAPI *GetProductString)(HANDLE, PVOID, ULONG);
   NTSTATUS (WINAPI *GetUsages)(HIDP_REPORT_TYPE, USAGE, USHORT, PUSAGE,
         PULONG, PHIDP_PREPARSED_DATA, PCHAR, ULONG);
   NTSTATUS (WINAPI *GetUsageValue)(HIDP_REPORT_TYPE, USAGE, USHORT, USAGE,
         PULONG, PHIDP_PREPARSED_DATA, PCHAR, ULONG);
   /* for an output report: a rumbling adapter's, read as no rumble
    * where missing */
   NTSTATUS (WINAPI *SetUsageValue)(HIDP_REPORT_TYPE, USAGE, USHORT, USAGE,
         ULONG, PHIDP_PREPARSED_DATA, PCHAR, ULONG);
   NTSTATUS (WINAPI *SetUsageValueArray)(HIDP_REPORT_TYPE, USAGE, USHORT,
         USAGE, PCHAR, USHORT, PHIDP_PREPARSED_DATA, PCHAR, ULONG);
   BOOL (WINAPI *Read)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
   BOOL (WINAPI *Write)(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
   BOOL (WINAPI *Result)(HANDLE, LPOVERLAPPED, LPDWORD, BOOL);
   BOOL (WINAPI *Cancel)(HANDLE);
} winusb_hidclass_api_t;

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
typedef DWORD (WINAPI *winusb_cm_locate_t)(DWORD*, const char*, ULONG);
typedef DWORD (WINAPI *winusb_cm_parent_t)(DWORD*, DWORD, ULONG);

#define WINUSB_CM_FILTER_ALL_INTERFACES 0x1
#define WINUSB_CM_DEVICEINTERFACE       0
#define WINUSB_CM_INTERFACE_ARRIVAL     0

/* WinUSB's reads are a DualShock 3's 64; Windows' HID driver asks for
 * a device's longest input report, which a DualShock 4 over Bluetooth
 * declares at 547 bytes */
#define WINUSB_REPORT_MAX 640
#define WINUSB_USB_REPORT 64
#define WINUSB_OUT_MAX    640
#define WINUSB_EP_IN      0x81
/* an index into buf, with this set when the writer has left a report
 * there the frontend has not taken */
#define WINUSB_FRESH      4

/* A pad on Windows' HID driver with no handler, read from the layout
 * its report descriptor gives, as the Mac's driver reads such a pad -
 * and numbered as it does, so a profile made there serves here: its
 * buttons in usage order, its axes X, Y, Rx, Ry, Z, Rz then rudder,
 * throttle, steering, accelerator and brake, its first hat as the
 * D-pad. Windows parses the descriptor; nothing here does.
 *
 * An Xbox pad on Windows' HID driver - "IG_" in its instance - has both
 * triggers on Z, the left above its middle and the right below; they
 * are split onto axes 4 and 5, 0 at rest, as SDL splits them; once
 * it is paired with its gamepad in Windows.Gaming.Input, they and its
 * rumble are that gamepad's (winusb_wgi.h). */
#define WINUSB_GEN_AXES    11
#define WINUSB_GEN_VALUES  16
#define WINUSB_GEN_HAT     0xff
#define WINUSB_GEN_LATER   0xfe /* an axis named twice, not yet placed */
#define WINUSB_GEN_TRIGGERS 0xfd

typedef struct
{
   USAGE page;
   USAGE usage;
   LONG min;
   LONG max;
   USHORT bits;
   uint8_t id;          /* the axis, or WINUSB_GEN_HAT */
} winusb_gen_value_t;

typedef struct winusb_generic
{
   struct winusb_device *dev;
   PHIDP_PREPARSED_DATA pp;
   USAGE btn_page;      /* 0: none */
   USAGE btn_min;
   unsigned values;
   winusb_gen_value_t value[WINUSB_GEN_VALUES];
   input_pad_view_t view; /* its buttons, axes and hat, as decoded */
   hid_sjoy_output_t out; /* a PlayStation adapter's rumble */
   uint8_t motors[2];     /* strong, weak: what the core asked last */
   bool motors_due;
   bool xbox;             /* an Xbox pad: see above */
   winusb_wgi_pad_t *wgi; /* its gamepad, once paired */
   retro_time_t wgi_tried;
} winusb_generic_t;

typedef struct winusb_device
{
   struct winusb_device *next;
   struct winusb_hid *hid;
   HANDLE file;
   void *usb;           /* WinUSB's; NULL on Windows' HID driver */
   /* the Switch 2 Pro Controller's second interface, woken by the
    * reader before it reads */
   HANDLE bulk_file;
   void *bulk_usb;
   UCHAR bulk_out;
   UCHAR bulk_in;
   /* its player, as pad_connection's; and the one its lights show,
    * the reader's: -1 before the first */
   retro_atomic_int_t player;
   int lit;
   HANDLE ctrl_event;
   HANDLE quit_event;
   const char *tag;     /* for the log */
   bool hidclass;       /* on Windows' HID driver */
   bool strip_id;       /* its reports have no ID: Windows puts a 0 first */
   bool bluetooth;      /* on Windows' HID driver, through Bluetooth */
   bool generic;        /* there, with no handler: read as gen says */
   winusb_generic_t gen;
   ULONG in_len;        /* what a read asks for */
   ULONG out_len;       /* an output report's length on this device */
   /* on WinUSB: the interrupt pipe read, and the one written - 0 where
    * output goes as SET_REPORT on the control pipe */
   UCHAR ep_in;
   UCHAR ep_out;

   /* writes, the frontend's: one in flight, the newest waiting behind */
   OVERLAPPED out_ov;
   bool out_busy;
   bool out_failed;     /* a write refused, said once */
   bool out_waiting;
   uint8_t out[2][WINUSB_OUT_MAX];
   ULONG out_n[2];      /* each one's length, as it goes on WinUSB */
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
   /* for the log when the pad goes: the reader's, read after the join */
   unsigned reports;
   unsigned unseen;       /* replaced before the poll took them */
   retro_time_t gap_max;  /* longest wait between two reports */
   retro_time_t last;
   /* the poll's */
   unsigned taken;
} winusb_device_t;

/* pads another program has open: WinUSB lets one program at a time
 * have a device, so one held elsewhere (ScpToolkit, an SDL with
 * libusb) is left to it, said once, and tried again at each rescan */
#define WINUSB_BUSY_MAX 4

typedef struct winusb_hid
{
   joypad_connection_t *slots;
   winusb_device_t *devices;
   char busy[WINUSB_BUSY_MAX][MAX_PATH];
   unsigned busy_count;
   unsigned busy_seen; /* bit n: busy[n] was there at this scan */
   HMODULE cfgmgr;
   winusb_cm_unregister_t cm_unregister;
   winusb_cm_locate_t cm_locate;
   winusb_cm_parent_t cm_parent;
   HANDLE notification;
   retro_atomic_int_t rescan;
   bool wgi;            /* Windows.Gaming.Input is there */
} winusb_hid_t;

extern hid_driver_t winusb_hid;

/* filled in by the driver's start, or by a test before it */
static winusb_api_t winusb_api;
static winusb_hidclass_api_t winusb_hidclass_api;

static bool winusb_hidclass_api_load(void)
{
   HMODULE lib;

   if (winusb_hidclass_api.GetAttributes)
      return true;
   if (!(lib = LoadLibraryA("hid.dll")))
      return false;
   winusb_hidclass_api.GetAttributes     = (BOOLEAN (WINAPI*)(HANDLE,
            PHIDD_ATTRIBUTES))GetProcAddress(lib, "HidD_GetAttributes");
   winusb_hidclass_api.GetPreparsedData  = (BOOLEAN (WINAPI*)(HANDLE,
            PHIDP_PREPARSED_DATA*))GetProcAddress(lib, "HidD_GetPreparsedData");
   winusb_hidclass_api.FreePreparsedData = (BOOLEAN (WINAPI*)(
            PHIDP_PREPARSED_DATA))GetProcAddress(lib, "HidD_FreePreparsedData");
   winusb_hidclass_api.GetCaps           = (NTSTATUS (WINAPI*)(
            PHIDP_PREPARSED_DATA, PHIDP_CAPS))GetProcAddress(lib, "HidP_GetCaps");
   winusb_hidclass_api.GetButtonCaps     = (NTSTATUS (WINAPI*)(
            HIDP_REPORT_TYPE, PHIDP_BUTTON_CAPS, PUSHORT,
            PHIDP_PREPARSED_DATA))GetProcAddress(lib, "HidP_GetButtonCaps");
   winusb_hidclass_api.GetValueCaps      = (NTSTATUS (WINAPI*)(
            HIDP_REPORT_TYPE, PHIDP_VALUE_CAPS, PUSHORT,
            PHIDP_PREPARSED_DATA))GetProcAddress(lib, "HidP_GetValueCaps");
   winusb_hidclass_api.GetFeature        = (BOOLEAN (WINAPI*)(HANDLE, PVOID,
            ULONG))GetProcAddress(lib, "HidD_GetFeature");
   winusb_hidclass_api.SetFeature        = (BOOLEAN (WINAPI*)(HANDLE, PVOID,
            ULONG))GetProcAddress(lib, "HidD_SetFeature");
   winusb_hidclass_api.GetProductString  = (BOOLEAN (WINAPI*)(HANDLE, PVOID,
            ULONG))GetProcAddress(lib, "HidD_GetProductString");
   winusb_hidclass_api.GetUsages         = (NTSTATUS (WINAPI*)(
            HIDP_REPORT_TYPE, USAGE, USHORT, PUSAGE, PULONG,
            PHIDP_PREPARSED_DATA, PCHAR, ULONG))
      GetProcAddress(lib, "HidP_GetUsages");
   winusb_hidclass_api.GetUsageValue     = (NTSTATUS (WINAPI*)(
            HIDP_REPORT_TYPE, USAGE, USHORT, USAGE, PULONG,
            PHIDP_PREPARSED_DATA, PCHAR, ULONG))
      GetProcAddress(lib, "HidP_GetUsageValue");
   winusb_hidclass_api.SetUsageValue     = (NTSTATUS (WINAPI*)(
            HIDP_REPORT_TYPE, USAGE, USHORT, USAGE, ULONG,
            PHIDP_PREPARSED_DATA, PCHAR, ULONG))
      GetProcAddress(lib, "HidP_SetUsageValue");
   winusb_hidclass_api.SetUsageValueArray = (NTSTATUS (WINAPI*)(
            HIDP_REPORT_TYPE, USAGE, USHORT, USAGE, PCHAR, USHORT,
            PHIDP_PREPARSED_DATA, PCHAR, ULONG))
      GetProcAddress(lib, "HidP_SetUsageValueArray");
   if (     !winusb_hidclass_api.GetAttributes
         || !winusb_hidclass_api.GetPreparsedData
         || !winusb_hidclass_api.FreePreparsedData
         || !winusb_hidclass_api.GetCaps
         || !winusb_hidclass_api.GetButtonCaps
         || !winusb_hidclass_api.GetValueCaps
         || !winusb_hidclass_api.GetFeature
         || !winusb_hidclass_api.SetFeature
         || !winusb_hidclass_api.GetProductString
         || !winusb_hidclass_api.GetUsages
         || !winusb_hidclass_api.GetUsageValue)
   {
      FreeLibrary(lib);
      memset(&winusb_hidclass_api, 0, sizeof(winusb_hidclass_api));
      return false;
   }
   winusb_hidclass_api.Read   = ReadFile;
   winusb_hidclass_api.Write  = WriteFile;
   winusb_hidclass_api.Result = GetOverlappedResult;
   winusb_hidclass_api.Cancel = CancelIo;
   winusb_hidclass_api.lib    = lib;
   return true;
}

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
   winusb_api.WritePipe           = (BOOL (WINAPI*)(void*, UCHAR, UCHAR*,
            ULONG, ULONG*, OVERLAPPED*))
      GetProcAddress(lib, "WinUsb_WritePipe");
   winusb_api.QueryInterfaceSettings = (BOOL (WINAPI*)(void*, UCHAR, void*))
      GetProcAddress(lib, "WinUsb_QueryInterfaceSettings");
   winusb_api.QueryPipe           = (BOOL (WINAPI*)(void*, UCHAR, UCHAR,
            void*))
      GetProcAddress(lib, "WinUsb_QueryPipe");
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

/* The DualShock 3: on WinUSB, and only there. Every other pad is
 * taken on Windows' HID driver. */
/* Pads read through WinUSB: those Windows' HID driver cannot run */
static bool winusb_hid_handled(uint16_t vid, uint16_t pid)
{
   joypad_connection_entry_t *entry = find_connection_entry(vid, pid, "");
   return entry && (   entry->iface == &pad_connection_ps3
                    || entry->iface == &pad_connection_wiiugca);
}

static bool winusb_hid_is_xid(HDEVINFO devs, SP_DEVINFO_DATA *info,
      unsigned vid, unsigned pid)
{
   char ids[512];
   memset(ids, 0, sizeof(ids));
   return SetupDiGetDeviceRegistryPropertyA(devs, info, SPDRP_COMPATIBLEIDS,
            NULL, (BYTE*)ids, sizeof(ids) - 2, NULL)
      && winusb_is_xid(ids, vid, pid);
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

#define WINUSB_SWITCH2_PID 0x2069

/* One transfer on a pipe of the Switch 2 Pro Controller's second
 * interface, on the reader's thread, given up after @ms: the length
 * moved, or -1. */
static int32_t winusb_switch2_transfer(winusb_device_t *dev, UCHAR pipe,
      uint8_t *data, ULONG len, DWORD ms)
{
   OVERLAPPED ov;
   HANDLE events[2];
   ULONG got = 0;
   BOOL ok;

   memset(&ov, 0, sizeof(ov));
   ov.hEvent = dev->ctrl_event;
   ResetEvent(ov.hEvent);
   ok = (pipe & 0x80)
      ? winusb_api.ReadPipe(dev->bulk_usb, pipe, data, len, NULL, &ov)
      : winusb_api.WritePipe(dev->bulk_usb, pipe, data, len, NULL, &ov);
   if (!ok && GetLastError() != ERROR_IO_PENDING)
      return -1;
   if (!ok)
   {
      events[0] = dev->quit_event;
      events[1] = ov.hEvent;
      if (WaitForMultipleObjects(2, events, FALSE, ms) != WAIT_OBJECT_0 + 1)
         winusb_api.AbortPipe(dev->bulk_usb, pipe);
   }
   if (!winusb_api.GetOverlappedResult(dev->bulk_usb, &ov, &got, TRUE))
      return -1;
   return (int32_t)got;
}

/* The player's lights, as the console lights them, @player from 1; 0
 * puts them out. Taken once the pad is reporting, not while it wakes. */
static void winusb_switch2_lights(winusb_device_t *dev, unsigned player)
{
   static const uint8_t pattern[8] = {
      0x01, 0x03, 0x07, 0x0f, 0x09, 0x05, 0x0d, 0x06 };
   uint8_t lights[16] = {
      0x09, 0x91, 0x00, 0x07, 0x00, 0x08, 0x00, 0x00 };
   uint8_t reply[64];

   memset(lights + 8, 0, 8);
   lights[8] = player ? pattern[(player - 1) % 8] : 0;
   if (     winusb_switch2_transfer(dev, dev->bulk_out, lights,
               sizeof(lights), 1000) != (int32_t)sizeof(lights))
      RARCH_WARN("[WinUSB] \"%s\": its lights were not taken.\n",
            dev->name);
   winusb_switch2_transfer(dev, dev->bulk_in, reply, sizeof(reply), 100);
}

/* Each command is eight bytes and its data, the data's length in its
 * sixth; answered on the pipe in. */
static void winusb_switch2_wake(winusb_device_t *dev)
{
   static const uint8_t wake[] = {
      0x07, 0x91, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
      /* the features reported, then turned on */
      0x0c, 0x91, 0x00, 0x02, 0x00, 0x04, 0x00, 0x00,
      0x27, 0x00, 0x00, 0x00,
      0x11, 0x91, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
      0x0a, 0x91, 0x00, 0x08, 0x00, 0x14, 0x00, 0x00,
      0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0x35, 0x00, 0x46, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x0c, 0x91, 0x00, 0x04, 0x00, 0x04, 0x00, 0x00,
      0x27, 0x00, 0x00, 0x00,
      0x01, 0x91, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00,
      /* rumble */
      0x01, 0x91, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
      0x08, 0x91, 0x00, 0x02, 0x00, 0x04, 0x00, 0x00,
      0x01, 0x00, 0x00, 0x00,
      /* the report's shape, then the reports */
      0x03, 0x91, 0x00, 0x0a, 0x00, 0x04, 0x00, 0x00,
      0x05, 0x00, 0x00, 0x00,
      0x03, 0x91, 0x00, 0x0d, 0x00, 0x08, 0x00, 0x00,
      0x01, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
   };
   uint8_t reply[64];
   size_t at = 0;
   unsigned sent = 0;

   while (at + 8 <= sizeof(wake))
   {
      size_t len = 8 + (size_t)wake[at + 5];
      if (     winusb_switch2_transfer(dev, dev->bulk_out,
                  (uint8_t*)wake + at, (ULONG)len, 1000) == (int32_t)len)
         sent++;
      winusb_switch2_transfer(dev, dev->bulk_in, reply, sizeof(reply), 100);
      at += len;
   }
   if (sent < 10)
      RARCH_WARN("[WinUSB] \"%s\": %u of 10 wake commands taken.\n",
            dev->name, sent);
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
   /* as the main, video and audio threads are: a report waiting
    * behind them is a press or release seen late */
   sthread_raise_current_priority();
   if (dev->bulk_usb)
      winusb_switch2_wake(dev);

   while (!retro_atomic_load_acquire_int(&dev->quit))
   {
      ULONG got = 0;
      int w     = dev->write_idx;
      BOOL ok;

      ResetEvent(ov.hEvent);
      if (dev->hidclass)
      {
         DWORD n = 0;
         ok = winusb_hidclass_api.Read(dev->file, dev->buf[w], dev->in_len,
               NULL, &ov);
         if (!ok && GetLastError() == ERROR_IO_PENDING)
         {
            /* cancelled by this thread, which made the read */
            if (WaitForMultipleObjects(2, events, FALSE, INFINITE)
                  != WAIT_OBJECT_0 + 1)
               winusb_hidclass_api.Cancel(dev->file);
            ok = winusb_hidclass_api.Result(dev->file, &ov, &n, TRUE);
         }
         else if (ok)
            ok = winusb_hidclass_api.Result(dev->file, &ov, &n, TRUE);
         got = n;
      }
      else
      {
         ok = winusb_api.ReadPipe(dev->usb, dev->ep_in, dev->buf[w],
               dev->in_len, NULL, &ov);
         if (!ok && GetLastError() == ERROR_IO_PENDING)
         {
            /* Abort only after the reader has submitted its operation. */
            if (WaitForMultipleObjects(2, events, FALSE, INFINITE)
                  != WAIT_OBJECT_0 + 1)
               winusb_api.AbortPipe(dev->usb, dev->ep_in);
            ok = winusb_api.GetOverlappedResult(dev->usb, &ov, &got, TRUE);
         }
         else if (ok)
            ok = winusb_api.GetOverlappedResult(dev->usb, &ov, &got, TRUE);
      }
      if (!ok)
      {
         /* stopped by the frontend, or the pad is gone */
         if (!retro_atomic_load_acquire_int(&dev->quit))
            retro_atomic_store_release_int(&dev->gone, 1);
         break;
      }
      if (got)
      {
         int old;
         retro_time_t now = cpu_features_get_time_usec();
         dev->len[w]    = got;
         old            = retro_atomic_exchange_int(&dev->middle,
               w | WINUSB_FRESH);
         dev->write_idx = old & 3;
         dev->reports++;
         if (old & WINUSB_FRESH)
            dev->unseen++;
         if (dev->last && now - dev->last > dev->gap_max)
            dev->gap_max = now - dev->last;
         dev->last = now;
         if (dev->bulk_usb)
         {
            /* the port it is read on, or its slot until that is said */
            int player = retro_atomic_load_acquire_int(&dev->player);
            if (player != dev->lit)
            {
               dev->lit = player;
               winusb_switch2_lights(dev, player
                     ? (unsigned)(player - 1) : (unsigned)dev->slot + 1);
            }
         }
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
      RARCH_DBG("%s \"%s\": %u reports, %u taken by the poll, %u replaced"
            " before it took them; %u ms at most between two.\n",
            dev->tag, dev->name, dev->reports, dev->taken, dev->unseen,
            (unsigned)(dev->gap_max / 1000));
   }
   if (dev->out_busy)
   {
      DWORD n;
      ULONG got;
      /* the frontend's thread made the write */
      if (dev->hidclass)
      {
         winusb_hidclass_api.Cancel(dev->file);
         winusb_hidclass_api.Result(dev->file, &dev->out_ov, &n, TRUE);
      }
      else
      {
         winusb_api.AbortPipe(dev->usb, dev->ep_out);
         winusb_api.GetOverlappedResult(dev->usb, &dev->out_ov, &got, TRUE);
      }
   }
   winusb_wgi_unpair(dev->gen.wgi);
   if (dev->gen.pp)
      winusb_hidclass_api.FreePreparsedData(dev->gen.pp);
   if (dev->usb)
      winusb_api.Free(dev->usb);
   if (dev->bulk_usb)
      winusb_api.Free(dev->bulk_usb);
   if (dev->bulk_file && dev->bulk_file != INVALID_HANDLE_VALUE)
      CloseHandle(dev->bulk_file);
   if (dev->file != INVALID_HANDLE_VALUE)
      CloseHandle(dev->file);
   if (dev->out_ov.hEvent)
      CloseHandle(dev->out_ov.hEvent);
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
static int winusb_hid_busy_find(winusb_hid_t *hid, const char *instance)
{
   unsigned i;
   for (i = 0; i < hid->busy_count; i++)
      if (string_is_equal(hid->busy[i], instance))
         return (int)i;
   return -1;
}

static void winusb_hid_busy_drop(winusb_hid_t *hid, unsigned i)
{
   unsigned last = --hid->busy_count;
   if (i != last)
   {
      memmove(hid->busy[i], hid->busy[last], sizeof(hid->busy[i]));
      if (hid->busy_seen & (1u << last))
         hid->busy_seen |= (1u << i);
      else
         hid->busy_seen &= ~(1u << i);
   }
   hid->busy_seen &= ~(1u << last);
}

/* The generic pad's layout: where its buttons start, and which of its
 * values are axes, in the Mac's order, and its hat. */
static void winusb_generic_setup(winusb_generic_t *gen,
      PHIDP_PREPARSED_DATA pp, const HIDP_BUTTON_CAPS *bcaps, USHORT nb,
      const HIDP_VALUE_CAPS *vcaps, USHORT nv, bool xinput)
{
   bool taken[WINUSB_GEN_AXES];
   bool hat = false;
   USHORT i;
   unsigned a;

   gen->pp      = pp;
   gen->values  = 0;
   memset(&gen->view, 0, sizeof(gen->view));
   for (a = 0; a < WINUSB_GEN_AXES; a++)
      taken[a]      = false;
   gen->xbox     = xinput;
   gen->wgi      = NULL;
   if (xinput)
      taken[4] = taken[5] = true;
   gen->btn_page = 0;
   for (i = 0; i < nb && !gen->btn_page; i++)
   {
      if (bcaps[i].UsagePage != 0x09)
         continue;
      gen->btn_page = 0x09;
      gen->btn_min  = bcaps[i].IsRange
         ? bcaps[i].Range.UsageMin : bcaps[i].NotRange.Usage;
   }
   for (i = 0; i < nv && gen->values < WINUSB_GEN_VALUES; i++)
   {
      winusb_gen_value_t *v = &gen->value[gen->values];
      USAGE usage           = vcaps[i].IsRange
         ? vcaps[i].Range.UsageMin : vcaps[i].NotRange.Usage;
      int index;

      v->page  = vcaps[i].UsagePage;
      v->usage = usage;
      v->min   = vcaps[i].LogicalMin;
      v->max   = vcaps[i].LogicalMax;
      v->bits  = vcaps[i].BitSize;
      /* a maximum written with its top bit set, as an Xbox pad's 65535
       * is, comes negative: unsigned, as hid_report_desc.h reads it */
      if (v->min >= 0 && v->max < 0 && v->bits > 0 && v->bits < 32)
         v->max = (LONG)((1UL << v->bits) - 1);
      if (v->page == 0x01 && usage == 0x39)
      {
         if (hat)
            continue;
         hat   = true;
         v->id = WINUSB_GEN_HAT;
         gen->values++;
         continue;
      }
      if (xinput && v->page == 0x01 && usage == 0x32)
      {
         v->id = WINUSB_GEN_TRIGGERS;
         gen->values++;
         continue;
      }
      if ((index = hid_desc_axis_index(v->page, usage)) < 0)
         continue;
      /* one named again is given a free axis once every other has its
       * own, as on the Mac */
      if (taken[index])
         v->id = WINUSB_GEN_LATER;
      else
      {
         taken[index] = true;
         v->id        = (uint8_t)index;
      }
      gen->values++;
   }
   for (i = 0; i < gen->values; i++)
   {
      if (gen->value[i].id != WINUSB_GEN_LATER)
         continue;
      a = 0;
      while (a < WINUSB_GEN_AXES && taken[a])
         a++;
      if (a < WINUSB_GEN_AXES)
      {
         taken[a]          = true;
         gen->value[i].id  = (uint8_t)a;
      }
   }
}

static int16_t winusb_generic_scale(LONG v, LONG min, LONG max)
{
   LONG range = max - min;
   if (range <= 0)
      return 0;
   if (v < min)
      v = min;
   else if (v > max)
      v = max;
   return (int16_t)((((int64_t)(v - min) * 65534) / range) - 32767);
}

static void winusb_hidclass_write(winusb_device_t *dev,
      const uint8_t *s, size_t len);

/* The motors as the core last asked, in the adapter's output report:
 * at a poll, as every pad's rumble goes. */
static void winusb_generic_send_rumble(winusb_generic_t *gen)
{
   winusb_device_t *dev = gen->dev;
   uint8_t r[WINUSB_OUT_MAX];

   gen->motors_due = false;
   if (     dev->out_len <= sizeof(r)
         && hid_sjoy_output_report(&gen->out, gen->pp, r, dev->out_len,
               gen->motors[0], gen->motors[1],
               winusb_hidclass_api.SetUsageValue,
               winusb_hidclass_api.SetUsageValueArray))
      winusb_hidclass_write(dev, r, dev->out_len);
}

/* One report into the pad's state; a report that does not carry a
 * value - a device that splits its state across report IDs - leaves
 * it as it was. */
static void winusb_generic_packet(void *data, uint8_t *report,
      uint16_t size)
{
   winusb_generic_t *gen = (winusb_generic_t*)data;
   unsigned i;

   if (!gen || !gen->pp)
      return;
   if (gen->btn_page)
   {
      USAGE usages[64];
      ULONG n = 64;
      if (winusb_hidclass_api.GetUsages(HidP_Input, gen->btn_page, 0,
               usages, &n, gen->pp, (PCHAR)report, size)
            == HIDP_STATUS_SUCCESS)
      {
         uint32_t b = 0;
         ULONG k;
         for (k = 0; k < n; k++)
         {
            unsigned idx = (unsigned)(USAGE)(usages[k] - gen->btn_min);
            if (idx < 32)
               b |= 1u << idx;
         }
         pad_view_set_buttons(&gen->view, b, false);
      }
   }
   for (i = 0; i < gen->values; i++)
   {
      const winusb_gen_value_t *v = &gen->value[i];
      ULONG raw                   = 0;
      LONG val;

      if (winusb_hidclass_api.GetUsageValue(HidP_Input, v->page, 0, v->usage,
               &raw, gen->pp, (PCHAR)report, size) != HIDP_STATUS_SUCCESS)
         continue;
      val = (LONG)raw;
      /* a signed field comes unsigned: extended from its width */
      if (v->min < 0 && v->bits > 0 && v->bits < 32
            && (raw & (1UL << (v->bits - 1))))
         val = (LONG)(raw | ~((1UL << v->bits) - 1));
      if (v->id == WINUSB_GEN_LATER)
         continue;
      if (v->id == WINUSB_GEN_HAT)
      {
         /* eight ways from the top, clockwise, as up 1, down 2, left 4
          * and right 8; out of range is rest */
         static const uint8_t hat[8] = { 1, 1 | 8, 8, 8 | 2, 2, 2 | 4, 4, 4 | 1 };
         LONG way = val - v->min;
         if (v->max - v->min == 3)
            way *= 2;
         gen->view.hats = (val < v->min || val > v->max || way < 0 || way > 7)
            ? 0 : hat[way];
      }
      else if (v->id == WINUSB_GEN_TRIGGERS && !gen->wgi)
      {
         LONG mid = v->min + (v->max - v->min + 1) / 2;
         LONG d   = val - mid;
         gen->view.axes[4] = (int16_t)(d > 0 && v->max > mid
               ? (int64_t)d * 32767 / (v->max - mid) : 0);
         gen->view.axes[5] = (int16_t)(d < 0 && mid > v->min
               ? (int64_t)-d * 32767 / (mid - v->min) : 0);
      }
      else if (v->id < WINUSB_GEN_AXES)
         gen->view.axes[v->id] = winusb_generic_scale(val, v->min, v->max);
   }
   if (gen->motors_due && gen->out.fields)
      winusb_generic_send_rumble(gen);
}

static void *winusb_generic_init(void *data, uint32_t slot,
      hid_driver_t *driver)
{
   winusb_device_t *dev = (winusb_device_t*)data;
   dev->gen.dev         = dev;
   return &dev->gen;
}

static void winusb_generic_deinit(void *data) { }

static void winusb_generic_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength)
{
   winusb_generic_t *gen = (winusb_generic_t*)data;
   unsigned idx          = (effect == RETRO_RUMBLE_STRONG) ? 0 : 1;
   uint8_t motor         = (uint8_t)(strength >> 8);
   if (gen && (gen->out.fields || gen->xbox) && gen->motors[idx] != motor)
   {
      gen->motors[idx] = motor;
      gen->motors_due  = true;
   }
}

static void winusb_generic_get_buttons(void *data, input_bits_t *state)
{
   winusb_generic_t *gen = (winusb_generic_t*)data;
   if (gen)
      *state = gen->view.buttons;
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t winusb_generic_axis(void *data, unsigned axis)
{
   winusb_generic_t *gen = (winusb_generic_t*)data;
   return (gen && axis < WINUSB_GEN_AXES) ? gen->view.axes[axis] : 0;
}

static const char *winusb_generic_name(void *data)
{
   winusb_generic_t *gen = (winusb_generic_t*)data;
   return gen ? gen->dev->name : NULL;
}

static int32_t winusb_generic_button(void *data, uint16_t joykey)
{
   winusb_generic_t *gen = (winusb_generic_t*)data;
   return (gen && joykey < 32)
      ? (BIT256_GET(gen->view.buttons, joykey) ? 1 : 0) : 0;
}

static const input_pad_view_t *winusb_generic_view(void *data)
{
   return &((winusb_generic_t*)data)->view;
}

static pad_connection_interface_t winusb_generic_iface = {
   winusb_generic_init,
   winusb_generic_deinit,
   winusb_generic_packet,
   winusb_generic_rumble,
   winusb_generic_get_buttons,
   winusb_generic_axis,
   winusb_generic_name,
   winusb_generic_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   NULL,  /* set_player */
   winusb_generic_view
};

/* A pad on Windows' HID driver: how long its reports are, and whether
 * they have IDs - without, Windows puts a 0 first, which hidraw does
 * not, and the handlers are given the report without it. */
static bool winusb_hidclass_setup(winusb_device_t *dev)
{
   PHIDP_PREPARSED_DATA pp = NULL;
   HIDP_CAPS caps;
   bool ok = false;

   if (!winusb_hidclass_api.GetPreparsedData(dev->file, &pp))
      return false;
   if (winusb_hidclass_api.GetCaps(pp, &caps) == HIDP_STATUS_SUCCESS)
   {
      HIDP_BUTTON_CAPS bcaps[16];
      HIDP_VALUE_CAPS vcaps[16];
      USHORT nb = caps.NumberInputButtonCaps;
      USHORT nv = caps.NumberInputValueCaps;
      bool ids  = false;
      USHORT i;

      if (nb > 16)
         nb = 16;
      if (nv > 16)
         nv = 16;
      /* asked for no more than fit: the rest are the same report's */
      if (nb && winusb_hidclass_api.GetButtonCaps(HidP_Input, bcaps, &nb, pp)
            == HIDP_STATUS_SUCCESS)
         for (i = 0; i < nb; i++)
            ids |= bcaps[i].ReportID != 0;
      if (nv && winusb_hidclass_api.GetValueCaps(HidP_Input, vcaps, &nv, pp)
            == HIDP_STATUS_SUCCESS)
         for (i = 0; i < nv; i++)
            ids |= vcaps[i].ReportID != 0;
      dev->in_len   = caps.InputReportByteLength;
      dev->out_len  = caps.OutputReportByteLength;
      /* Windows' parser wants the report as Windows gave it */
      dev->strip_id = !ids && !dev->generic;
      ok            =    dev->in_len  > 1 && dev->in_len  <= WINUSB_REPORT_MAX
                      && dev->out_len <= WINUSB_OUT_MAX;
      if (ok && dev->generic)
      {
         winusb_generic_setup(&dev->gen, pp, bcaps, nb, vcaps, nv,
               compat_strcasestr(dev->instance, "&IG_") != NULL);
         if (hid_sjoy_rumbles(dev->vid, dev->pid))
            RARCH_LOG("%s %04x:%04x: %s (%u output values, %u bytes).\n",
                  dev->tag, dev->vid, dev->pid,
                  hid_sjoy_output_setup(&dev->gen.out, pp,
                     caps.NumberOutputValueCaps,
                     winusb_hidclass_api.GetValueCaps)
                  ? "rumbles" : "no rumble report",
                  (unsigned)caps.NumberOutputValueCaps,
                  (unsigned)dev->out_len);
         pp = NULL; /* the pad's now, freed with it */
      }
   }
   if (pp)
      winusb_hidclass_api.FreePreparsedData(pp);
   return ok;
}

/* The Switch 2 Pro Controller's second interface, on the pad whose HID
 * collection is @instance: the one under the class the pad gives it
 * whose parent, the pad, is the collection's grandparent. */
static bool winusb_switch2_bulk_path(winusb_hid_t *hid, const char *instance,
      char *path, size_t len)
{
   static const GUID wake_guid = { 0x6f13725e, 0xef0e, 0x4fd3,
      { 0xae, 0x5f, 0xb2, 0xde, 0x98, 0x9e, 0xc8, 0x25 } };
   HDEVINFO ifs;
   DWORD i, pad = 0, up = 0;
   bool found = false;

   if (     !hid->cm_locate || !hid->cm_parent
         || hid->cm_locate(&pad, instance, 0)
         || hid->cm_parent(&up, pad, 0)
         || hid->cm_parent(&pad, up, 0))
      return false;
   ifs = SetupDiGetClassDevsA(&wake_guid, NULL, NULL,
         DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
   if (ifs == INVALID_HANDLE_VALUE)
      return false;
   for (i = 0; !found; i++)
   {
      SP_DEVICE_INTERFACE_DATA ifd;
      SP_DEVINFO_DATA info;
      union
      {
         SP_DEVICE_INTERFACE_DETAIL_DATA_A d;
         char b[sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A) + MAX_PATH];
      } detail;

      ifd.cbSize = sizeof(ifd);
      if (!SetupDiEnumDeviceInterfaces(ifs, NULL, &wake_guid, i, &ifd))
         break;
      info.cbSize     = sizeof(info);
      detail.d.cbSize = sizeof(detail.d);
      if (     SetupDiGetDeviceInterfaceDetailA(ifs, &ifd, &detail.d,
                  sizeof(detail), NULL, &info)
            && !hid->cm_parent(&up, info.DevInst, 0)
            && up == pad)
      {
         strlcpy(path, detail.d.DevicePath, len);
         found = true;
      }
   }
   SetupDiDestroyDeviceInfoList(ifs);
   return found;
}

/* A pad on WinUSB: its interrupt pipes. The DualShock 3 takes its
 * output as SET_REPORT on the control pipe, as Sony's driver sends it;
 * the rest on their interrupt OUT pipe, as libusb sends it. */
static void winusb_hid_pipes(winusb_device_t *dev, bool control_out)
{
   winusb_interface_desc_t desc;
   UCHAR i;

   dev->ep_in  = WINUSB_EP_IN;
   dev->ep_out = 0;
   memset(&desc, 0, sizeof(desc));
   if (     !winusb_api.QueryInterfaceSettings || !winusb_api.QueryPipe
         || !winusb_api.QueryInterfaceSettings(dev->usb, 0, &desc))
      return;
   for (i = 0; i < desc.bNumEndpoints; i++)
   {
      winusb_pipe_info_t pipe;
      memset(&pipe, 0, sizeof(pipe));
      if (     !winusb_api.QueryPipe(dev->usb, 0, i, &pipe)
            || pipe.PipeType != WINUSB_PIPE_INTERRUPT)
         continue;
      if (pipe.PipeId & 0x80)
         dev->ep_in = pipe.PipeId;
      else if (     !control_out && winusb_api.WritePipe
               && pipe.MaximumPacketSize
               && pipe.MaximumPacketSize <= WINUSB_OUT_MAX)
      {
         dev->ep_out  = pipe.PipeId;
         dev->out_len = pipe.MaximumPacketSize;
      }
   }
}

/* That interface opened, and its bulk pipes found, for the reader. */
static bool winusb_switch2_open(winusb_hid_t *hid, winusb_device_t *dev)
{
   char path[MAX_PATH];
   winusb_interface_desc_t desc;
   UCHAR i;

   if (     !winusb_api.WritePipe || !winusb_api.QueryInterfaceSettings
         || !winusb_api.QueryPipe
         || !winusb_switch2_bulk_path(hid, dev->instance, path, sizeof(path)))
      return false;
   dev->bulk_file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
   if (     dev->bulk_file == INVALID_HANDLE_VALUE
         || !(dev->ctrl_event = CreateEventA(NULL, TRUE, FALSE, NULL))
         || !winusb_api.Initialize(dev->bulk_file, &dev->bulk_usb))
   {
      dev->bulk_usb = NULL;
      return false;
   }
   memset(&desc, 0, sizeof(desc));
   if (winusb_api.QueryInterfaceSettings(dev->bulk_usb, 0, &desc))
      for (i = 0; i < desc.bNumEndpoints; i++)
      {
         winusb_pipe_info_t pipe;
         memset(&pipe, 0, sizeof(pipe));
         if (     !winusb_api.QueryPipe(dev->bulk_usb, 0, i, &pipe)
               || pipe.PipeType != WINUSB_PIPE_BULK)
            continue;
         if (pipe.PipeId & 0x80)
            dev->bulk_in  = pipe.PipeId;
         else
            dev->bulk_out = pipe.PipeId;
      }
   return dev->bulk_in && dev->bulk_out;
}

static void winusb_hid_open(winusb_hid_t *hid, const char *path,
      const char *instance, uint16_t vid, uint16_t pid, bool hidclass,
      bool xid)
{
   int number;
   winusb_device_t *dev;
   joypad_connection_entry_t *entry;

   if (winusb_hid_find(hid, instance))
      return;
   if (!(dev = (winusb_device_t*)calloc(1, sizeof(*dev))))
      return;
   entry          = xid ? NULL : find_connection_entry(vid, pid, "");
   dev->hid       = hid;
   dev->slot      = -1;
   dev->vid       = vid;
   dev->pid       = pid;
   dev->hidclass  = hidclass;
   dev->tag       = hidclass ? "[HID]" : "[WinUSB]";
   dev->in_len    = WINUSB_USB_REPORT;
   dev->read_idx  = 0;
   dev->write_idx = 1;
   retro_atomic_int_init(&dev->middle, 2);
   retro_atomic_int_init(&dev->quit, 0);
   retro_atomic_int_init(&dev->gone, 0);
   retro_atomic_int_init(&dev->player, 0);
   dev->generic   = hidclass && !entry;
   strlcpy(dev->instance, instance, sizeof(dev->instance));
   strlcpy(dev->name, entry ? entry->name
         : xid ? pad_connection_xid.get_name(NULL) : "HID Gamepad",
         sizeof(dev->name));
   /* the Bluetooth HID service's id is in a Bluetooth pad's instance */
   dev->bluetooth = hidclass && compat_strcasestr(instance,
         "00001124-0000-1000-8000-00805f9b34fb") != NULL;

   dev->file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
   if (dev->file == INVALID_HANDLE_VALUE)
   {
      DWORD err = GetLastError();
      if (err == ERROR_ACCESS_DENIED || err == ERROR_SHARING_VIOLATION)
      {
         if (     winusb_hid_busy_find(hid, instance) < 0
               && hid->busy_count < WINUSB_BUSY_MAX)
         {
            strlcpy(hid->busy[hid->busy_count++], instance,
                  sizeof(hid->busy[0]));
            RARCH_LOG("%s \"%s\" (%s) is in use by another"
                  " program; it is left to that one.\n",
                  dev->tag, dev->name, instance);
         }
      }
      else
         RARCH_WARN("%s %s cannot be opened (error %lu).\n",
               dev->tag, path, (unsigned long)err);
      free(dev);
      return;
   }
   {
      int busy = winusb_hid_busy_find(hid, instance);
      if (busy >= 0)
         winusb_hid_busy_drop(hid, (unsigned)busy);
   }
   /* known by the name it gives, as on the Mac, and its handler found
    * by it too: some ids are a chip's, shared by pads of every layout */
   if (hidclass)
   {
      WCHAR wname[128];
      char product[sizeof(dev->name)];
      product[0] = '\0';
      memset(wname, 0, sizeof(wname));
      if (     winusb_hidclass_api.GetProductString(dev->file, wname,
                  sizeof(wname) - sizeof(WCHAR))
            && wname[0])
         WideCharToMultiByte(CP_UTF8, 0, wname, -1, product,
               sizeof(product) - 1, NULL, NULL);
      product[sizeof(product) - 1] = '\0';
      entry        = find_connection_entry(vid, pid, product);
      RARCH_DBG("%s %04x:%04x names itself \"%s\"%s.\n", dev->tag,
            vid, pid, product, entry ? "" : ": read from its descriptor");
      dev->generic = !entry;
      if (entry)
         strlcpy(dev->name, entry->name, sizeof(dev->name));
      else if (product[0])
         strlcpy(dev->name, product, sizeof(dev->name));
   }
   if (     !(dev->quit_event = CreateEventA(NULL, TRUE, FALSE, NULL))
         || (hidclass
            ?    !(dev->out_ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL))
              || !winusb_hidclass_setup(dev)
            :    !(dev->ctrl_event = CreateEventA(NULL, TRUE, FALSE, NULL))
              || !winusb_api.Initialize(dev->file, &dev->usb)))
   {
      RARCH_WARN("%s %s cannot be used (error %lu).\n",
            dev->tag, path, (unsigned long)GetLastError());
      dev->usb = NULL;
      winusb_hid_close(dev);
      return;
   }
   if (!hidclass)
   {
      winusb_hid_pipes(dev, entry && entry->iface == &pad_connection_ps3);
      if (     dev->ep_out
            && !(dev->out_ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL)))
         dev->ep_out = 0;
   }
   /* held by another program, that one woke it */
   if (     hidclass && vid == 0x057e && pid == WINUSB_SWITCH2_PID
         && !winusb_switch2_open(hid, dev))
   {
      RARCH_WARN("%s \"%s\" cannot be woken (error %lu); read as it is.\n",
            dev->tag, dev->name, (unsigned long)GetLastError());
      if (dev->bulk_usb)
         winusb_api.Free(dev->bulk_usb);
      dev->bulk_usb = NULL;
   }

   /* the pad's handler starts it here */
   if (xid)
   {
      joypad_connection_entry_t xid_entry;
      xid_entry.name  = dev->name;
      xid_entry.vid   = vid;
      xid_entry.pid   = pid;
      xid_entry.iface = &pad_connection_xid;
      dev->slot       = pad_connection_pad_init_entry(hid->slots, &xid_entry,
            dev, &winusb_hid);
   }
   else if (dev->generic)
   {
      joypad_connection_entry_t gen_entry;
      gen_entry.name  = dev->name;
      gen_entry.vid   = vid;
      gen_entry.pid   = pid;
      gen_entry.iface = &winusb_generic_iface;
      dev->slot       = pad_connection_pad_init_entry(hid->slots, &gen_entry,
            dev, &winusb_hid);
   }
   else
      dev->slot = pad_connection_pad_init(hid->slots, dev->name, vid, pid,
            dev, &winusb_hid);
   if (dev->slot < 0 || !hid->slots[dev->slot].connection)
   {
      RARCH_ERR("%s \"%s\" could not be started.\n", dev->tag, dev->name);
      if (dev->slot >= 0)
         pad_connection_pad_deinit(&hid->slots[dev->slot], dev->slot);
      winusb_hid_close(dev);
      return;
   }
   retro_atomic_int_init(&dev->player, retro_atomic_load_acquire_int(
            &hid->slots[dev->slot].player));
   dev->lit = -1;
   if (!(dev->reader = sthread_create(winusb_hid_reader, dev)))
   {
      pad_connection_pad_deinit(&hid->slots[dev->slot], dev->slot);
      winusb_hid_close(dev);
      return;
   }

   /* its device number: its slot where this is the joypad driver's,
    * one from the frontend's pool where it is beside another */
   if ((number = input_driver_hid_claim(hid, (unsigned)dev->slot,
               dev->name, vid, pid)) < 0)
   {
      RARCH_WARN("%s \"%s\": no device number left.\n", dev->tag, dev->name);
      pad_connection_pad_deinit(&hid->slots[dev->slot], dev->slot);
      winusb_hid_close(dev);
      return;
   }

   dev->next    = hid->devices;
   hid->devices = dev;
   RARCH_LOG("%s \"%s\" is device %d.\n", dev->tag, dev->name, number + 1);
   input_autoconfigure_connect(dev->name, NULL, NULL, "hid",
         (unsigned)number, vid, pid);
}

/* A pad let go: @gone when it left, and is said to have; not when the
 * driver is only stopping, as every joypad driver does at a restart -
 * the pad is still there, and is reported again when one starts. */
static void winusb_hid_remove(winusb_hid_t *hid, winusb_device_t *dev,
      bool gone)
{
   int number;
   winusb_device_t **link;

   for (link = &hid->devices; *link; link = &(*link)->next)
   {
      if (*link != dev)
         continue;
      *link = dev->next;
      if (     gone
            && (number = input_driver_hid_number(hid, (unsigned)dev->slot)) >= 0)
         input_autoconfigure_disconnect((unsigned)number, dev->name);
      input_driver_hid_release(hid, (unsigned)dev->slot);
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

/* Pads on Windows' HID driver: every gamepad or joystick collection,
 * not the touchpad or audio controls a pad may add beside, read
 * through its handler or, with none, from its layout. Each is looked at through a handle
 * that may neither read nor write, which any program may open. */
static void winusb_hidclass_scan(winusb_hid_t *hid)
{
   /* GUID_DEVINTERFACE_HID */
   static const GUID hid_guid = { 0x4d1e55b2, 0xf16f, 0x11cf,
      { 0x88, 0xcb, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } };
   HDEVINFO ifs;
   DWORD i;

   if (!winusb_hidclass_api.GetAttributes || input_driver_hid_beside(hid))
      return;
   ifs = SetupDiGetClassDevsA(&hid_guid, NULL, NULL,
         DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
   if (ifs == INVALID_HANDLE_VALUE)
      return;
   for (i = 0; ; i++)
   {
      SP_DEVICE_INTERFACE_DATA ifd;
      SP_DEVINFO_DATA info;
      HIDD_ATTRIBUTES attr;
      PHIDP_PREPARSED_DATA pp;
      HIDP_CAPS caps;
      HANDLE probe;
      bool pad = false;
      char instance[MAX_PATH];
      union
      {
         SP_DEVICE_INTERFACE_DETAIL_DATA_A d;
         char b[sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A) + MAX_PATH];
      } detail;

      ifd.cbSize = sizeof(ifd);
      if (!SetupDiEnumDeviceInterfaces(ifs, NULL, &hid_guid, i, &ifd))
         break;
      info.cbSize     = sizeof(info);
      detail.d.cbSize = sizeof(detail.d);
      if (     !SetupDiGetDeviceInterfaceDetailA(ifs, &ifd, &detail.d,
                  sizeof(detail), NULL, &info)
            || !SetupDiGetDeviceInstanceIdA(ifs, &info, instance,
                  sizeof(instance), NULL))
         continue;
      if (winusb_hid_find(hid, instance))
         continue;
      {
         int busy = winusb_hid_busy_find(hid, instance);
         if (busy >= 0)
            hid->busy_seen |= (1u << busy);
      }
      probe = CreateFileA(detail.d.DevicePath, 0,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
      if (probe == INVALID_HANDLE_VALUE)
         continue;
      attr.Size = sizeof(attr);
      memset(&caps, 0, sizeof(caps));
      /* a pad with a handler, or none - read from its layout - but
       * not the DualShock 3, which Windows' HID driver will not start */
      if (     winusb_hidclass_api.GetAttributes(probe, &attr)
            && !winusb_hid_handled(attr.VendorID, attr.ProductID)
            && winusb_hidclass_api.GetPreparsedData(probe, &pp))
      {
         if (winusb_hidclass_api.GetCaps(pp, &caps) == HIDP_STATUS_SUCCESS)
            pad =    caps.UsagePage == 0x01
                  && (caps.Usage == 0x04 || caps.Usage == 0x05
                     || caps.Usage == 0x08);
         /* a pad with a handler, whatever its collection says */
         if (     !pad
               && pad_connection_has_ids(attr.VendorID, attr.ProductID))
            pad = true;
         RARCH_DBG("[HID] %04x:%04x, usage %02x:%02x: %s.\n",
               attr.VendorID, attr.ProductID, caps.UsagePage, caps.Usage,
               pad ? "a pad" : "not a pad");
         winusb_hidclass_api.FreePreparsedData(pp);
      }
      CloseHandle(probe);
      /* the Switch 2 Pro Controller once its second interface is there
       * to wake it: told of its arrival, this looks again */
      if (     pad && attr.VendorID == 0x057e
            && attr.ProductID == WINUSB_SWITCH2_PID)
      {
         char bulk[MAX_PATH];
         if (!winusb_switch2_bulk_path(hid, instance, bulk, sizeof(bulk)))
         {
            RARCH_LOG("[HID] %s: waiting for its WinUSB interface.\n",
                  instance);
            pad = false;
         }
      }
      if (pad)
         winusb_hid_open(hid, detail.d.DevicePath, instance,
               attr.VendorID, attr.ProductID, true, false);
   }
   SetupDiDestroyDeviceInfoList(ifs);
}

/* USB devices bound to WinUSB whose ids name a handled pad. */
static void winusb_hid_scan(winusb_hid_t *hid)
{
   DWORD i;
   HDEVINFO devs;
#ifdef HAVE_WINUSB_BIND
   /* a Wii U GameCube adapter, or an original Xbox pad, on a driver
    * other than WinUSB */
   bool gca_elsewhere = false;
   bool xid_elsewhere = false;
#endif

   hid->busy_seen = 0;
   devs           = winusb_api.Initialize
      ? SetupDiGetClassDevsA(NULL, "USB", NULL,
            DIGCF_ALLCLASSES | DIGCF_PRESENT)
      : INVALID_HANDLE_VALUE;
   for (i = 0; devs != INVALID_HANDLE_VALUE; i++)
   {
      SP_DEVINFO_DATA info;
      char service[64];
      char instance[MAX_PATH];
      char path[MAX_PATH];
      unsigned vid, pid;
      const char *ids;
      bool xid;

      info.cbSize = sizeof(info);
      if (!SetupDiEnumDeviceInfo(devs, i, &info))
         break;
      if (     !SetupDiGetDeviceInstanceIdA(devs, &info, instance,
                  sizeof(instance), NULL)
            || !(ids = strstr(instance, "VID_"))
            || sscanf(ids, "VID_%4x&PID_%4x", &vid, &pid) != 2)
         continue;
      xid = winusb_hid_is_xid(devs, &info, vid, pid);
      if (     (!xid && !winusb_hid_handled((uint16_t)vid, (uint16_t)pid))
            || winusb_hid_find(hid, instance))
         continue;
      if (!SetupDiGetDeviceRegistryPropertyA(devs, &info, SPDRP_SERVICE,
                  NULL, (BYTE*)service, sizeof(service), NULL))
         service[0] = '\0';
      if (!string_is_equal_noncase(service, "WinUSB"))
      {
         RARCH_LOG("[WinUSB] %s is not bound to WinUSB.\n", instance);
#ifdef HAVE_WINUSB_BIND
         if (winusb_bind_is_gca(vid, pid))
            gca_elsewhere = true;
         /* with no driver at all; one on XBCD is read by DirectInput */
         else if (xid && !*service)
            xid_elsewhere = true;
#endif
         continue;
      }
      {
         int busy = winusb_hid_busy_find(hid, instance);
         if (busy >= 0)
            hid->busy_seen |= (1u << busy);
      }
      if (winusb_hid_interface_path(devs, &info, instance,
               path, sizeof(path)))
         winusb_hid_open(hid, path, instance, (uint16_t)vid, (uint16_t)pid,
               false, xid);
   }
   if (devs != INVALID_HANDLE_VALUE)
      SetupDiDestroyDeviceInfoList(devs);
   winusb_hidclass_scan(hid);
#ifdef HAVE_WINUSB_BIND
   /* one Windows' own driver could not run either: the menu offers to
    * move it to WinUSB */
   if (gca_elsewhere)
   {
      winusb_device_t *dev;
      for (dev = hid->devices; dev; dev = dev->next)
         if (winusb_bind_is_gca(dev->vid, dev->pid))
            gca_elsewhere = false;
   }
   winusb_bind_gca_seen(gca_elsewhere);
   winusb_bind_xid_seen(xid_elsewhere);
#endif
   /* one unplugged while held elsewhere is said again when it is back */
   for (i = hid->busy_count; i-- > 0; )
      if (!(hid->busy_seen & (1u << i)))
         winusb_hid_busy_drop(hid, (unsigned)i);
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
      winusb_hid_remove(hid, hid->devices, false);
   if (hid->wgi)
      winusb_wgi_stop();
   if (hid->slots)
      pad_connection_destroy(hid->slots);
   free(hid);
}

static void *winusb_hid_init(void)
{
   winusb_hid_t *hid;

   /* either way of reaching pads will do */
   bool usb_ok = winusb_api_load();
   bool hid_ok = winusb_hidclass_api_load();

   if (!usb_ok && !hid_ok)
      return NULL;
   if (!(hid = (winusb_hid_t*)calloc(1, sizeof(*hid))))
      return NULL;
   if (!(hid->slots = pad_connection_init(MAX_USERS)))
   {
      free(hid);
      return NULL;
   }
   retro_atomic_int_init(&hid->rescan, 0);
   hid->wgi = hid_ok && winusb_wgi_start();

   /* listening before the scan, so a pad bound during it is not
    * missed; without it (before Windows 8) the pads bound at start */
   if ((hid->cfgmgr = LoadLibraryA("cfgmgr32.dll")))
   {
      winusb_cm_register_t reg = (winusb_cm_register_t)
         GetProcAddress(hid->cfgmgr, "CM_Register_Notification");
      hid->cm_unregister       = (winusb_cm_unregister_t)
         GetProcAddress(hid->cfgmgr, "CM_Unregister_Notification");
      hid->cm_locate           = (winusb_cm_locate_t)
         GetProcAddress(hid->cfgmgr, "CM_Locate_DevNodeA");
      hid->cm_parent           = (winusb_cm_parent_t)
         GetProcAddress(hid->cfgmgr, "CM_Get_Parent");
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

/* Writes to a pad on Windows' HID driver, from the frontend's thread,
 * which never waits on one: a write goes out at once if none is in
 * flight, and otherwise waits behind it, the newest replacing the one
 * waiting, to go at a later poll. Windows' HID driver wants each output
 * report at the device's length, so it goes padded with noughts; a
 * WinUSB pad's interrupt pipe takes it at its own. */
static bool winusb_hidclass_in_flight(winusb_device_t *dev)
{
   DWORD n;
   ULONG got;
   if (!dev->out_busy)
      return false;
   if (     !(dev->hidclass
               ? winusb_hidclass_api.Result(dev->file, &dev->out_ov, &n, FALSE)
               : winusb_api.GetOverlappedResult(dev->usb, &dev->out_ov,
                  &got, FALSE))
         && GetLastError() == ERROR_IO_INCOMPLETE)
      return true;
   dev->out_busy = false;
   return false;
}

static void winusb_hidclass_start_write(winusb_device_t *dev)
{
   ResetEvent(dev->out_ov.hEvent);
   if (     (dev->hidclass
               ? winusb_hidclass_api.Write(dev->file, dev->out[0],
                  dev->out_len, NULL, &dev->out_ov)
               : winusb_api.WritePipe(dev->usb, dev->ep_out, dev->out[0],
                  dev->out_n[0], NULL, &dev->out_ov))
         || GetLastError() == ERROR_IO_PENDING)
      dev->out_busy = true;
   else if (!dev->out_failed)
   {
      dev->out_failed = true;
      RARCH_DBG("%s \"%s\": output report 0x%02x refused (error %lu).\n",
            dev->tag, dev->name, dev->out[0][0],
            (unsigned long)GetLastError());
   }
}

static void winusb_hidclass_flush(winusb_device_t *dev)
{
   if (winusb_hidclass_in_flight(dev))
      return;
   memmove(dev->out[0], dev->out[1], dev->out_len);
   dev->out_n[0]    = dev->out_n[1];
   dev->out_waiting = false;
   winusb_hidclass_start_write(dev);
}

static void winusb_hidclass_write(winusb_device_t *dev,
      const uint8_t *s, size_t len)
{
   /* longer than the device's reports only by noughts - a handler's
    * fixed-size buffer - it goes at the device's length */
   while (len > dev->out_len && !s[len - 1])
      len--;
   if (!dev->out_len || len > dev->out_len)
   {
      if (!dev->out_failed)
      {
         dev->out_failed = true;
         RARCH_DBG("%s \"%s\": output report 0x%02x longer than its %u bytes.\n",
               dev->tag, dev->name, len ? s[0] : 0, (unsigned)dev->out_len);
      }
      return;
   }
   if (winusb_hidclass_in_flight(dev))
   {
      memcpy(dev->out[1], s, len);
      memset(dev->out[1] + len, 0, dev->out_len - len);
      dev->out_n[1]    = (ULONG)len;
      dev->out_waiting = true;
      return;
   }
   memcpy(dev->out[0], s, len);
   memset(dev->out[0] + len, 0, dev->out_len - len);
   dev->out_n[0]    = (ULONG)len;
   dev->out_waiting = false;
   winusb_hidclass_start_write(dev);
}

/* An Xbox pad's gamepad in Windows.Gaming.Input: looked for while it
 * has none, at most every 100 ms - two alike are told apart once one
 * moves - then its triggers read there at each poll, and its rumble
 * sent there. */
static void winusb_hid_wgi(winusb_device_t *dev)
{
   winusb_generic_t *gen = &dev->gen;

   if (!gen->wgi)
   {
      retro_time_t now = cpu_features_get_time_usec();
      uint16_t buttons = 0;
      unsigned i;
      if (gen->wgi_tried && now - gen->wgi_tried < 100000)
         return;
      gen->wgi_tried = now;
      for (i = 0; i < 10; i++)
         if (BIT256_GET(gen->view.buttons, i))
            buttons |= (uint16_t)(1u << i);
      if (!(gen->wgi = winusb_wgi_pair(gen->view.axes, buttons)))
         return;
      RARCH_LOG("%s \"%s\": triggers and rumble through"
            " Windows.Gaming.Input.\n", dev->tag, dev->name);
      gen->motors_due = gen->motors[0] || gen->motors[1];
   }
   winusb_wgi_triggers(gen->wgi, &gen->view.axes[4], &gen->view.axes[5]);
   if (gen->motors_due)
   {
      winusb_wgi_rumble(gen->wgi, gen->motors[0], gen->motors[1]);
      gen->motors_due = false;
   }
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
         unsigned skip;
         dev->read_idx = retro_atomic_exchange_int(&dev->middle,
               dev->read_idx) & 3;
         dev->taken++;
         skip = (dev->strip_id && dev->len[dev->read_idx]) ? 1 : 0;
         pad_connection_packet(&hid->slots[dev->slot], dev->slot,
               dev->buf[dev->read_idx] + skip,
               (uint32_t)dev->len[dev->read_idx] - skip);
      }
      else
         pad_connection_idle(&hid->slots[dev->slot]);
      if (dev->gen.xbox && hid->wgi)
         winusb_hid_wgi(dev);
      if (dev->out_waiting)
         winusb_hidclass_flush(dev);
      if (retro_atomic_load_acquire_int(&dev->gone))
      {
         RARCH_LOG("%s \"%s\" is gone.\n", dev->tag, dev->name);
         winusb_hid_remove(hid, dev, true);
      }
   }
}

/* Lent: the reader threads only pass reports on; the handlers are
 * given them in the poll. */
static const input_pad_view_t *winusb_hid_joypad_view(void *data,
      unsigned pad)
{
   winusb_hid_t *hid = (winusb_hid_t*)data;
   if (!hid || pad >= MAX_USERS)
      return NULL;
   return pad_connection_view(&hid->slots[pad]);
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
   unsigned hat_dir;

   if (port >= MAX_USERS)
      return 0;
   /* the hat of a pad read from its layout; handlers give theirs as
    * the RetroPad's D-pad */
   if ((hat_dir = GET_HAT_DIR(joykey)))
   {
      winusb_hid_t *hid = (winusb_hid_t*)data;
      const input_pad_view_t *view;
      if (     !hid || GET_HAT(joykey) != 0
            || !(view = pad_connection_view(&hid->slots[port])))
         return 0;
      switch (hat_dir)
      {
         case HAT_UP_MASK:    return (view->hats & 1) != 0;
         case HAT_DOWN_MASK:  return (view->hats & 2) != 0;
         case HAT_LEFT_MASK:  return (view->hats & 4) != 0;
         case HAT_RIGHT_MASK: return (view->hats & 8) != 0;
      }
      return 0;
   }
   /* every one get_buttons() says: a home button's menu toggle is
    * past the first 32 */
   if (joykey >= 256)
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

static void winusb_hid_set_player(void *data, unsigned pad, unsigned player)
{
   winusb_hid_t *hid = (winusb_hid_t*)data;
   if (!hid || pad >= MAX_USERS)
      return;
   /* a Switch 2 Pro Controller's lights are its reader's to send */
   {
      winusb_device_t *dev;
      for (dev = hid->devices; dev; dev = dev->next)
         if (dev->slot == (int32_t)pad && dev->bulk_usb)
            retro_atomic_store_release_int(&dev->player, (int)player + 1);
   }
   pad_connection_set_player(&hid->slots[pad], player);
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
   if (dev->hidclass)
   {
      if (report_type == HID_REPORT_OUTPUT)
      {
         winusb_hidclass_write(dev, s, len);
         return (int32_t)len;
      }
      if (report_type == HID_REPORT_FEATURE)
         return winusb_hidclass_api.SetFeature(dev->file, s, (ULONG)len)
            ? (int32_t)len : -1;
      return -1;
   }
   return winusb_hid_control(dev, 0x21, 0x09,
         (USHORT)((report_type << 8) | report_id), s, len);
}

static int32_t winusb_hid_get_report(void *handle, uint8_t report_type,
      uint8_t report_id, uint8_t *s, size_t len)
{
   winusb_device_t *dev = (winusb_device_t*)handle;
   if (!dev || !len)
      return -1;
   if (dev->hidclass)
   {
      if (report_type != HID_REPORT_FEATURE)
         return -1;
      s[0] = report_id;
      return winusb_hidclass_api.GetFeature(dev->file, s, (ULONG)len)
         ? (int32_t)len : -1;
   }
   return winusb_hid_control(dev, 0xa1, 0x01,
         (USHORT)((report_type << 8) | report_id), s, len);
}

static void winusb_hid_send_control(void *handle, uint8_t *s, size_t len)
{
   winusb_device_t *dev = (winusb_device_t*)handle;
   if (!len)
      return;
   if (dev && !dev->hidclass && dev->ep_out)
      winusb_hidclass_write(dev, s, len);
   else
      winusb_hid_set_report(handle, HID_REPORT_OUTPUT, s[0], s, len);
}

static bool winusb_hid_is_bluetooth(void *handle)
{
   winusb_device_t *dev = (winusb_device_t*)handle;
   return dev && dev->bluetooth;
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

/* A pad's motion sensors, where its handler reads them. */
static bool winusb_hid_set_sensor_state(void *data, unsigned pad,
      enum retro_sensor_action action, unsigned rate)
{
   winusb_hid_t *hid = (winusb_hid_t*)data;
   (void)rate;
   if (!hid || pad >= MAX_USERS)
      return false;
   return pad_connection_set_sensor(&hid->slots[pad], action);
}

static bool winusb_hid_get_sensor_input(void *data, unsigned pad,
      unsigned id, float *value)
{
   winusb_hid_t *hid = (winusb_hid_t*)data;
   if (!hid || pad >= MAX_USERS || !value)
      return false;
   return pad_connection_sensor(&hid->slots[pad], id, value);
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
   winusb_hid_set_bt_host,
   winusb_hid_is_bluetooth,
   winusb_hid_set_player,
   winusb_hid_joypad_view,
   winusb_hid_set_sensor_state,
   winusb_hid_get_sensor_input
};
