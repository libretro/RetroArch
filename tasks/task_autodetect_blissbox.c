/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <string/stdstring.h>

#include "../verbosity.h"
#include "../runloop.h"

#include "tasks_internal.h"

#ifdef HAVE_LIBUSB
#ifdef __FreeBSD__
#include <libusb.h>
#else
#include <libusb-1.0/libusb.h>
#endif
#endif

#if defined(_WIN32) && !defined(_XBOX) && !defined(_MSC_VER) && _WIN32_WINNT >= 0x0500
/* MinGW Win32 HID API */
#include <minwindef.h>
#include <wtypes.h>
#include <tchar.h>
#ifdef __NO_INLINE__
/* Workaround MinGW issue where compiling without -O2 (which sets __NO_INLINE__) causes the strsafe functions
 * to never be defined (only declared).
 */
#define __CRT_STRSAFE_IMPL
#endif
#include <strsafe.h>
#include <guiddef.h>
#include <ks.h>
#include <setupapi.h>
#include <winapifamily.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <hidsdi.h>
#ifdef __cplusplus
}
#endif

/* Why doesn't including cguid.h work to get a GUID_NULL instead? */
#ifdef __cplusplus
EXTERN_C __attribute__((weak))
const GUID GUID_NULL = {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};
#else
__attribute__((weak))
const GUID GUID_NULL = {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};
#endif
#endif

#include "../input/include/blissbox.h"

/* HID Class-Specific Requests values. See section 7.2 of the HID specifications */
#define USB_HID_GET_REPORT 0x01
#define USB_CTRL_IN LIBUSB_ENDPOINT_IN|LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE
#define USB_PACKET_CTRL_LEN 5
#define USB_TIMEOUT 5000 /* timeout in ms */

static const blissbox_pad_type_t blissbox_pad_types[] =
{
   {"A5200", 6},
   {"A5200_TB", 50},
   {"A7800", 4},
   {"ATARI", 0},
   {"ATARI_KEYPAD", 43},
   {"ATMARK", 10},
   {"BALLY", 42},
   {"CD32", 24},
   {"CDI", 33},
   {"COL", 1},
   {"COL_FLASHBACK", 48}, /* 3.0 */
   {"DC_ASCI", 15},
   {"DC_PAD", 16},
   {"DC_TWIN", 35}, /* 3.0 */
   {"FC_ARKANOID", 53},
   {"FC_NES", 52},
   {"GC", 9},
   {"GC_WHEEL", 18},
   {"GEN_3", 20},
   {"GEN_6", 21},
   {"GRAVIS_EX", 38},
   {"HAMMERHEAD", 40},
   {"HPD", 7},
   {"INTELI", 14},
   {"JAG", 11},
   {"MSSW", 39},
   {"N64", 19},
   {"NEO", 49},
   {"NES", 17},
   {"NES_ARKANOID", 30},
   {"NES_GUN", 28},
   {"NES_POWERPAD", 36},
   {"PADDLES", 41},
   {"PC_FX", 26},
   {"PC_GAMEPAD", 46},
   {"PSX_DIGITAL", 65},
   {"PSX_DS", 115},
   {"PSX_DS2", 121},
   {"PSX_FS", 83},
   {"PSX_JOGCON", 227}, /* 3.0 */
   {"PSX_NEGCON", 51},
   {"PSX_WHEEL", 12},
   {"SAC", 34},
   {"SATURN_ANALOG", 8},
   {"SATURN_DIGITAL", 3},
   {"SMS", 22},
   {"SNES", 27},
   {"SNESS_NTT", 47}, /* 3.0 */
   {"SPEEK", 45},
   {"TG16", 23},
   {"TG16_6BUTTON", 54}, /* 3.0 */
   {"THREE_DO", 25},
   {"THREE_DO_ANALOG", 37},
   {"VEC", 5},
   {"V_BOY", 29},
   {"WII_CLASSIC", 31},
   {"WII_DRUM", 55}, /* 3.0 */
   {"WII_MPLUS", 32},
   {"WII_NUNCHUK", 13},
   {"ZXSINC", 44},
   {"gx4000", 2},
   {NULL, 0}, /* used to mark unconnected ports, do not remove */
};

/* TODO/FIXME - global state - perhaps move outside this file */
/* Only one blissbox per machine is currently supported */
static const blissbox_pad_type_t *blissbox_pads[BLISSBOX_MAX_PADS] = {NULL};
#ifdef HAVE_LIBUSB
static struct libusb_device_handle *autoconfig_libusb_handle = NULL;
#endif

#ifdef _WIN32
static const blissbox_pad_type_t* input_autoconfigure_get_blissbox_pad_type_win32(int vid, int pid)
{
   /* TODO: Remove the check for !defined(_MSC_VER) after making sure this builds on MSVC */

   /* HID API is available since Windows 2000 */
#if defined(_WIN32) && !defined(_XBOX) && !defined(_MSC_VER) && _WIN32_WINNT >= 0x0500
   HDEVINFO hDeviceInfo;
   SP_DEVINFO_DATA device_info_data;
   SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
   HANDLE hDeviceHandle                 = INVALID_HANDLE_VALUE;
   BOOL bResult                         = TRUE;
   BOOL success                         = FALSE;
   GUID guidDeviceInterface             = {0};
   PSP_DEVICE_INTERFACE_DETAIL_DATA
      pInterfaceDetailData              = NULL;
   ULONG required_length                = 0;
   LPTSTR lp_device_path                = NULL;
   char *device_path                    = NULL;
   DWORD index                          = 0;
   unsigned len                         = 0;
   unsigned i                           = 0;
   char vidPidString[32]                = {0};
   char report[USB_PACKET_CTRL_LEN + 1] = {0};

   snprintf(vidPidString, sizeof(vidPidString), "vid_%04x&pid_%04x", vid, pid);

   HidD_GetHidGuid(&guidDeviceInterface);

   if (!memcmp(&guidDeviceInterface, &GUID_NULL, sizeof(GUID_NULL)))
     return NULL;

   /* Get information about all the installed devices for the specified
    * device interface class.
    */
   hDeviceInfo = SetupDiGetClassDevs(
    &guidDeviceInterface,
    NULL,
    NULL,
    DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

   if (hDeviceInfo == INVALID_HANDLE_VALUE)
   {
      RARCH_ERR("[Autoconf]: Error in SetupDiGetClassDevs: %d.\n",
            GetLastError());
      goto done;
   }

   /* Enumerate all the device interfaces in the device information set. */
   device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

   while (!success)
   {
      success = SetupDiEnumDeviceInfo(hDeviceInfo, index, &device_info_data);

      /* Reset for this iteration */
      if (lp_device_path)
      {
         LocalFree(lp_device_path);
         lp_device_path = NULL;
      }

      if (pInterfaceDetailData)
      {
         LocalFree(pInterfaceDetailData);
         pInterfaceDetailData = NULL;
      }

      /* Check if this is the last item */
      if (GetLastError() == ERROR_NO_MORE_ITEMS)
         break;

      deviceInterfaceData.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);

      /* Get information about the device interface. */
      for (i = 0; (bResult = SetupDiEnumDeviceInterfaces(
         hDeviceInfo,
         &device_info_data,
         &guidDeviceInterface,
         i,
         &deviceInterfaceData)); i++)
      {
         /* Check if this is the last item */
         if (GetLastError() == ERROR_NO_MORE_ITEMS)
            break;

         /* Check for some other error */
         if (!bResult)
         {
            RARCH_ERR("[Autoconf]: Error in SetupDiEnumDeviceInterfaces: %d.\n", GetLastError());
            goto done;
         }

         /* Interface data is returned in SP_DEVICE_INTERFACE_DETAIL_DATA
          * which we need to allocate, so we have to call this function twice.
          * First to get the size so that we know how much to allocate, and
          * second to do the actual call with the allocated buffer.
          */

         bResult = SetupDiGetDeviceInterfaceDetail(
            hDeviceInfo,
            &deviceInterfaceData,
            NULL, 0,
            &required_length,
            NULL);

         /* Check for some other error */
         if (!bResult)
         {
            if (     (ERROR_INSUFFICIENT_BUFFER == GetLastError()) 
                  && (required_length > 0))
            {
               /* we got the size, now allocate buffer */
               pInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)
                  LocalAlloc(LPTR, required_length);

               if (!pInterfaceDetailData)
               {
                  RARCH_ERR("[Autoconf]: Error allocating memory for the device detail buffer.\n");
                  goto done;
               }
            }
            else
            {
               RARCH_ERR("[Autoconf]: Other error: %d.\n", GetLastError());
               goto done;
            }
         }

         /* get the interface detailed data */
         pInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

         /* Now call it with the correct size and allocated buffer */
         bResult = SetupDiGetDeviceInterfaceDetail(
            hDeviceInfo,
            &deviceInterfaceData,
            pInterfaceDetailData,
            required_length,
            NULL,
            &device_info_data);

         /* Check for some other error */
         if (!bResult)
           goto done;

         /* copy device path */
         {
            size_t nLength = _tcslen(pInterfaceDetailData->DevicePath) + 1;
            lp_device_path = (TCHAR*)LocalAlloc(LPTR, nLength * sizeof(TCHAR));

            strlcpy(lp_device_path,
                  pInterfaceDetailData->DevicePath, nLength);

            device_path    = (char*)malloc(nLength);

            for (len = 0; len < nLength; len++)
               device_path[len] = lp_device_path[len];

            lp_device_path[nLength - 1] = 0;
         }

         if (strstr(device_path, vidPidString))
            goto found;
      }

      success = FALSE;
      index++;
   }

   if (!lp_device_path)
   {
      RARCH_ERR("[Autoconf]: No devicepath. Error %d.", GetLastError());
      goto done;
   }

found:
   /* Open the device */
   hDeviceHandle = CreateFileA(
      device_path,
      GENERIC_READ,  /* | GENERIC_WRITE,*/
      FILE_SHARE_READ,  /* | FILE_SHARE_WRITE,*/
      NULL,
      OPEN_EXISTING,
      0,  /*FILE_FLAG_OVERLAPPED,*/
      NULL);

   if (hDeviceHandle == INVALID_HANDLE_VALUE)
   {
      /* Windows sometimes erroneously fails to open with a sharing violation:
       * https://github.com/signal11/hidapi/issues/231
       * If this happens, trying again with read + write usually works for some reason.
       */

      /* Open the device */
      hDeviceHandle = CreateFileA(
         device_path,
         GENERIC_READ | GENERIC_WRITE,
         FILE_SHARE_READ | FILE_SHARE_WRITE,
         NULL,
         OPEN_EXISTING,
         0,  /*FILE_FLAG_OVERLAPPED,*/
         NULL);

      if (hDeviceHandle == INVALID_HANDLE_VALUE)
      {
         RARCH_ERR("[Autoconf]: Can't open device for reading and writing: %d.", GetLastError());
         runloop_msg_queue_push("Bliss-Box already in use. Please make sure other programs are not using it.", 2, 300, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         goto done;
      }
   }

done:
   free(device_path);
   LocalFree(lp_device_path);
   LocalFree(pInterfaceDetailData);
   bResult              = SetupDiDestroyDeviceInfoList(hDeviceInfo);
   device_path          = NULL;
   lp_device_path       = NULL;
   pInterfaceDetailData = NULL;

   if (!bResult)
      RARCH_ERR("[Autoconf]: Could not destroy device info list.\n");

   /* Device is not connected */
   if (!hDeviceHandle || hDeviceHandle == INVALID_HANDLE_VALUE)
      return NULL;

   report[0] = BLISSBOX_USB_FEATURE_REPORT_ID;

   HidD_GetFeature(hDeviceHandle, report, sizeof(report));

   CloseHandle(hDeviceHandle);

   for (i = 0; i < ARRAY_SIZE(blissbox_pad_types); i++)
   {
      const blissbox_pad_type_t *pad = &blissbox_pad_types[i];

      if (!pad || string_is_empty(pad->name))
         continue;

      if (pad->index == report[0])
         return pad;
   }

   RARCH_LOG("[Autoconf]: Could not find connected pad in Bliss-Box port#%d.\n", pid - BLISSBOX_PID);
#endif

   return NULL;
}
#else
static const blissbox_pad_type_t* input_autoconfigure_get_blissbox_pad_type_libusb(int vid, int pid)
{
#ifdef HAVE_LIBUSB
   unsigned i;
   unsigned char answer[USB_PACKET_CTRL_LEN] = {0};
   int ret                                   = libusb_init(NULL);

   if (ret < 0)
   {
      RARCH_ERR("[Autoconf]: Could not initialize libusb.\n");
      return NULL;
   }

   autoconfig_libusb_handle = libusb_open_device_with_vid_pid(NULL, vid, pid);

   if (!autoconfig_libusb_handle)
   {
      RARCH_ERR("[Autoconf]: Could not find or open libusb device %d:%d.\n", vid, pid);
      goto error;
   }

#ifdef __linux__
   libusb_detach_kernel_driver(autoconfig_libusb_handle, 0);
#endif

   ret = libusb_set_configuration(autoconfig_libusb_handle, 1);

   if (ret < 0)
   {
      RARCH_ERR("[Autoconf]: Error during libusb_set_configuration.\n");
      goto error;
   }

   ret = libusb_claim_interface(autoconfig_libusb_handle, 0);

   if (ret < 0)
   {
      RARCH_ERR("[Autoconf]: Error during libusb_claim_interface.\n");
      goto error;
   }

   ret = libusb_control_transfer(autoconfig_libusb_handle, USB_CTRL_IN, USB_HID_GET_REPORT, BLISSBOX_USB_FEATURE_REPORT_ID, 0, answer, USB_PACKET_CTRL_LEN, USB_TIMEOUT);

   if (ret < 0)
      RARCH_ERR("[Autoconf]: Error during libusb_control_transfer.\n");

   libusb_release_interface(autoconfig_libusb_handle, 0);

#ifdef __linux__
   libusb_attach_kernel_driver(autoconfig_libusb_handle, 0);
#endif

   libusb_close(autoconfig_libusb_handle);
   libusb_exit(NULL);

   for (i = 0; i < ARRAY_SIZE(blissbox_pad_types); i++)
   {
      const blissbox_pad_type_t *pad = &blissbox_pad_types[i];

      if (!pad || string_is_empty(pad->name))
         continue;

      if (pad->index == answer[0])
         return pad;
   }

   RARCH_LOG("[Autoconf]: Could not find connected pad in Bliss-Box port#%d.\n", pid - BLISSBOX_PID);

   return NULL;

error:
   libusb_close(autoconfig_libusb_handle);
   libusb_exit(NULL);
#endif

   return NULL;
}
#endif

static const blissbox_pad_type_t* input_autoconfigure_get_blissbox_pad_type(int vid, int pid)
{
#if defined(_WIN32)
#if defined(_MSC_VER) || defined(_XBOX)
   /* no MSVC/XBOX support */
   return NULL;
#else
   /* MinGW */
   return input_autoconfigure_get_blissbox_pad_type_win32(vid, pid);
#endif
#else
   return input_autoconfigure_get_blissbox_pad_type_libusb(vid, pid);
#endif
}

void input_autoconfigure_blissbox_override_handler(
      int vid, int pid, char *device_name, size_t len)
{
   if (pid == BLISSBOX_UPDATE_MODE_PID)
      RARCH_LOG("[Autoconf]: Bliss-Box in update mode detected. Ignoring.\n");
   else if (pid == BLISSBOX_OLD_PID)
      RARCH_LOG("[Autoconf]: Bliss-Box 1.0 firmware detected. Please update to 2.0 or later.\n");
   else if (pid >= BLISSBOX_PID && pid <= BLISSBOX_PID + BLISSBOX_MAX_PAD_INDEX)
   {
      const blissbox_pad_type_t *pad;
      int index      = pid - BLISSBOX_PID;

      RARCH_LOG("[Autoconf]: Bliss-Box detected. Getting pad type...\n");

      if (blissbox_pads[index])
         pad = blissbox_pads[index];
      else
         pad = input_autoconfigure_get_blissbox_pad_type(vid, pid);

      if (pad && !string_is_empty(pad->name))
      {
         RARCH_LOG("[Autoconf]: Found Bliss-Box pad type: %s (%d) in port#%d\n", pad->name, pad->index, index);

         /* override name given to autoconfig so it knows what kind of pad this is */
         if (len > 0)
         {
            strlcpy(device_name, "Bliss-Box 4-Play ", len);
            strlcat(device_name, pad->name, len);
         }

         blissbox_pads[index] = pad;
      }
      /* use NULL entry to mark as an unconnected port */
      else
         blissbox_pads[index] = &blissbox_pad_types[ARRAY_SIZE(blissbox_pad_types) - 1];
   }
}
