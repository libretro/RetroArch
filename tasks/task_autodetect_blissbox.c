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
#include <stdint.h>

#if defined(__linux__)
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

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
   {"ATARI", 0},
   {"COL", 1},
   {"gx4000", 2},
   {"SATURN_DIGITAL", 3},
   {"A7800", 4},
   {"VEC", 5},
   {"A5200", 6},
   {"HPD", 7},
   {"SATURN_ANALOG", 8},
   {"GC", 9},
   {"ATMARK", 10},
   {"JAG", 11},
   {"PSX_WHEEL", 12},
   {"WII_NUNCHUK", 13},
   {"INTELI", 14},
   {"DC_ASCI", 15},
   {"DC_PAD", 16},
   {"NES", 17},
   {"GC_WHEEL", 18},
   {"N64", 19},
   {"GEN_3", 20},
   {"GEN_6", 21},
   {"SMS", 22},
   {"TG16", 23},
   {"CD32", 24},
   {"THREE_DO", 25},
   {"PC_FX", 26},
   {"SNES", 27},
   {"NES_GUN", 28},
   {"V_BOY", 29},
   {"NES_ARKANOID", 30},
   {"WII_CLASSIC", 31},
   {"WII_MPLUS", 32},
   {"CDI", 33},
   {"SAC", 34},
   {"DC_TWIN", 35},
   {"NES_POWERPAD", 36},
   {"THREE_DO_ANALOG", 37},
   {"GRAVIS_EX", 38},
   {"MSSW", 39},
   {"HAMMERHEAD", 40},
   {"PADDLES", 41},
   {"BALLY", 42},
   {"ATARI_KEYPAD", 43},
   {"ZXSINC", 44},
   {"SPEEK", 45},
   {"PC_GAMEPAD", 46},
   {"SNESS_NTT", 47},
   {"COL_FLASH_BACK", 48},
   {"NEO", 49},
   {"A5200_TB", 50},
   {"PSX_NEGCON", 51},
   {"FC_NES", 52},
   {"FC_ARKANOID", 53},
   {"TG16_6BUTTON", 54},
   {"Unassigned", 55},
   {"ARCADE", 56},
   {"SUPERGUN1", 57},
   {"SATURN_GUN", 58},
   {"SMS_GUN", 59},
   {"DC_GUN", 60},
   {"PADDLES_GEMINI", 61},
   {"DC_PAD_RF", 62},
   {"FC_POWERPAD", 63},
   {"ATARI_TB", 64},
   {"PSX_DIGITAL", 65},
   {"WII_DRUM", 66},
   {"WII_GUITAR", 67},
   {"WII_DJHERO", 68},
   {"WII_TABLET", 69},
   {"A2600_TB", 70},
   {"FC_NTT", 71},
   {"FAIRC", 72},
   {"DC_GO_FISH", 73},
   {"DC_MARACA", 74},
   {"PSX_FS", 83},
   {"PSX_DS", 115},
   {"PSX_DS_GS", 119},
   {"PSX_DS2", 121},
   {"PSX_JOGCON", 127},
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
   BOOL ret                             = FALSE;
   GUID guidDeviceInterface             = {0};
   PSP_DEVICE_INTERFACE_DETAIL_DATA
      pInterfaceDetailData              = NULL;
   ULONG required_length                = 0;
   LPTSTR lp_device_path                = NULL;
   char *device_path                    = NULL;
   DWORD index                          = 0;
   size_t len                           = 0;
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
      RARCH_ERR("[Autoconf] Error in SetupDiGetClassDevs: %d.\n",
            GetLastError());
      goto done;
   }

   /* Enumerate all the device interfaces in the device information set. */
   device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

   while (!ret)
   {
      ret = SetupDiEnumDeviceInfo(hDeviceInfo, index, &device_info_data);

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
            RARCH_ERR("[Autoconf] Error in SetupDiEnumDeviceInterfaces: %d.\n", GetLastError());
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
                  RARCH_ERR("[Autoconf] Error allocating memory for the device detail buffer.\n");
                  goto done;
               }
            }
            else
            {
               RARCH_ERR("[Autoconf] Other error: %d.\n", GetLastError());
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

            /* NULL-check lp_device_path: strlcpy below NULL-derefs
             * on LocalAlloc failure.  Skip this device; the inner
             * 'for (i = 0; SetupDiEnumDeviceInterfaces(...); i++)'
             * loop advances to the next interface index.  The
             * outer 'while (!ret)' loop is unaffected (it keys off
             * SetupDiEnumDeviceInfo returns, not this iterator). */
            if (!lp_device_path)
               continue;

            strlcpy(lp_device_path,
                  pInterfaceDetailData->DevicePath, nLength);

            device_path    = (char*)malloc(nLength);

            /* NULL-check device_path: the indexed write below
             * NULL-derefs on OOM.  Free the LocalAlloc'd
             * lp_device_path we just populated and skip this
             * device. */
            if (!device_path)
            {
               LocalFree(lp_device_path);
               lp_device_path = NULL;
               continue;
            }

            for (len = 0; len < nLength; len++)
               device_path[len] = lp_device_path[len];

            lp_device_path[nLength - 1] = 0;
         }

         if (strstr(device_path, vidPidString))
            goto found;
      }

      ret = FALSE;
      index++;
   }

   if (!lp_device_path)
   {
      RARCH_ERR("[Autoconf] No devicepath. Error %d.", GetLastError());
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
         /* TODO/FIXME - localize */
         const char *_msg = "Bliss-Box already in use. Please make sure other programs are not using it.";
         RARCH_ERR("[Autoconf] Can't open device for reading and writing: %d.", GetLastError());
         runloop_msg_queue_push(_msg, strlen(_msg), 2, 300, false, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
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
      RARCH_ERR("[Autoconf] Could not destroy device info list.\n");

   /* Device is not connected */
   if (!hDeviceHandle || hDeviceHandle == INVALID_HANDLE_VALUE)
      return NULL;

   report[0] = BLISSBOX_USB_FEATURE_REPORT_ID;

   HidD_GetFeature(hDeviceHandle, report, sizeof(report));

   CloseHandle(hDeviceHandle);

   for (i = 0; i < ARRAY_SIZE(blissbox_pad_types); i++)
   {
      const blissbox_pad_type_t *pad = &blissbox_pad_types[i];

      if (!pad || !pad->name || !*pad->name)
         continue;

      if (pad->index == report[0])
         return pad;
   }

   RARCH_WARN("[Autoconf] Could not find connected pad in Bliss-Box port#%d.\n", pid - BLISSBOX_PID);
#endif

   return NULL;
}
#else
#if defined(__linux__)
static int input_autoconfigure_hidraw_vid_pid_match(const char *hidraw_name, int vid, int pid, int *reported_vid, int *reported_pid)
{
   FILE *file = NULL;
   char line[128];
   char uevent_path[128];
   unsigned int parsed_vid = 0;
   unsigned int parsed_pid = 0;
   unsigned int ignore = 0;
   bool found = false;
   bool matched = false;

   if (!hidraw_name)
      return 0;

   snprintf(uevent_path, sizeof(uevent_path), "/sys/class/hidraw/%s/device/uevent", hidraw_name);
   file = fopen(uevent_path, "r");
   if (!file)
      return 0;

   while (fgets(line, sizeof(line), file))
   {
      if (strncmp(line, "HID_ID=", 7) == 0)
      {
         if (sscanf(line + 7, "%x:%x:%x", &ignore, &parsed_vid, &parsed_pid) == 3)
         {
            found = true;
            break;
         }
      }
   }

   if (found)
   {
      if (reported_vid)
         *reported_vid = (int)parsed_vid;
      if (reported_pid)
         *reported_pid = (int)parsed_pid;

      matched = ((parsed_vid == (unsigned)vid) && (parsed_pid == (unsigned)pid)) ? true : false;
   }

   fclose(file);
   return matched;
}

static const blissbox_pad_type_t* input_autoconfigure_get_blissbox_pad_type_hidraw_scan(int vid, int pid)
{
   DIR *dev_dir = NULL;
   struct dirent *dir_entry = NULL;
   int fd = -1;
   int ret;
   unsigned i;
   int candidate_index;
   struct hidraw_devinfo info;
   unsigned char answer[USB_PACKET_CTRL_LEN] = {0};
   const char candidate_name[] = "hidraw";
   size_t candidate_name_len = sizeof(candidate_name) - 1;
   char device_path[64];
   const char *name_suffix = NULL;
   const blissbox_pad_type_t *pad_type = NULL;

   dev_dir = opendir("/dev");
   if (!dev_dir)
      return NULL;

   while ((dir_entry = readdir(dev_dir)) != NULL)
   {
      unsigned long index = 0;
      bool has_digits = false;
      int local_ret;

      if (strncmp(dir_entry->d_name, candidate_name, candidate_name_len) != 0)
         continue;

      name_suffix = dir_entry->d_name + candidate_name_len;
      if (!*name_suffix)
         continue;

      for (index = 0; *name_suffix; name_suffix++)
      {
         if (!isdigit((unsigned char)*name_suffix))
         {
            has_digits = false;
            break;
         }
         has_digits = true;
         index = index * 10 + (unsigned long)(*name_suffix - '0');
         if (index > 9999)
            break;
      }

      if (!has_digits)
         continue;

      snprintf(device_path, sizeof(device_path), "/dev/%s", dir_entry->d_name);
      {
         int reported_vid;
         int reported_pid;

         reported_vid = 0;
         reported_pid = 0;
         if (!input_autoconfigure_hidraw_vid_pid_match(dir_entry->d_name, vid, pid, &reported_vid, &reported_pid))
         {
            if (reported_vid || reported_pid)
            {
               RARCH_LOG("[Autoconf]   Skipping non-bliss hidraw device: %s (sysfs=%04x:%04x)\n",
                     device_path, reported_vid, reported_pid);
            }
            else
               RARCH_LOG("[Autoconf]   Skipping non-bliss hidraw device: %s\n", device_path);
            continue;
         }

         RARCH_LOG("[Autoconf] Probing hidraw candidate: %s (sysfs=%04x:%04x)\n", device_path, reported_vid, reported_pid);
      }

      fd = open(device_path, O_RDWR | O_NONBLOCK);
      if (fd < 0)
      {
         RARCH_LOG("[Autoconf]   open failed for %s: %d\n", device_path, errno);
         continue;
      }

      ret = ioctl(fd, HIDIOCGRAWINFO, &info);
      if (ret == 0 && info.vendor == vid && info.product == pid)
      {
         memset(answer, 0, sizeof(answer));
         answer[0] = BLISSBOX_USB_FEATURE_REPORT_ID;
         local_ret = ioctl(fd, HIDIOCGFEATURE(USB_PACKET_CTRL_LEN), answer);
         if (local_ret == USB_PACKET_CTRL_LEN)
         {
            int candidate_index_local = answer[0];

            candidate_index = answer[0];
            if (candidate_index_local == BLISSBOX_USB_FEATURE_REPORT_ID && USB_PACKET_CTRL_LEN > 1)
               candidate_index = answer[1];

            RARCH_LOG("[Autoconf]   raw feature report: [%02x %02x %02x %02x %02x] candidate=%d\n",
                  answer[0], answer[1], answer[2], answer[3], answer[4], candidate_index);

            for (i = 0; i < ARRAY_SIZE(blissbox_pad_types); i++)
            {
               const blissbox_pad_type_t *pad = &blissbox_pad_types[i];

               if (!pad || !pad->name || !*pad->name)
                  continue;

               if (pad->index == candidate_index)
               {
                  pad_type = pad;
                  break;
               }
            }

            if (pad_type)
            {
               close(fd);
               break;
            }
            RARCH_LOG("[Autoconf]   candidate index %d not mapped, continuing\n", candidate_index);
         }
         else
            RARCH_LOG("[Autoconf]   HIDIOCGFEATURE failed for %s: %d\n", device_path, errno);
      }
      else
         RARCH_LOG("[Autoconf]   HIDIOCGRAWINFO mismatch/failed for %s (ret=%d, vid=%04x pid=%04x), skipping\n",
               device_path, ret, info.vendor, info.product);

      close(fd);
      fd = -1;
   }

   if (fd >= 0)
      close(fd);
   closedir(dev_dir);
   return pad_type;
}

#endif

static const blissbox_pad_type_t* input_autoconfigure_get_blissbox_pad_type_libusb(int vid, int pid)
{
#ifdef HAVE_LIBUSB
   unsigned i;
   unsigned char answer[USB_PACKET_CTRL_LEN] = {0};
   int ret                                   = libusb_init(NULL);

   if (ret < 0)
   {
      RARCH_ERR("[Autoconf] Could not initialize libusb.\n");
      return NULL;
   }
   autoconfig_libusb_handle = libusb_open_device_with_vid_pid(NULL, vid, pid);

   if (!autoconfig_libusb_handle)
   {
      RARCH_ERR("[Autoconf] Could not find or open libusb device %d:%d.\n", vid, pid);
      goto error;
   }

#ifdef __linux__
   libusb_detach_kernel_driver(autoconfig_libusb_handle, 0);
#endif

   ret = libusb_set_configuration(autoconfig_libusb_handle, 1);
   if (ret < 0)
   {
      RARCH_ERR("[Autoconf] Error during libusb_set_configuration.\n");
      goto error;
   }

   ret = libusb_claim_interface(autoconfig_libusb_handle, 0);
   if (ret < 0)
   {
      RARCH_ERR("[Autoconf] Error during libusb_claim_interface.\n");
      goto error;
   }

   ret = libusb_control_transfer(autoconfig_libusb_handle, USB_CTRL_IN, USB_HID_GET_REPORT, BLISSBOX_USB_FEATURE_REPORT_ID, 0, answer, USB_PACKET_CTRL_LEN, USB_TIMEOUT);

   if (ret < 0)
   {
      RARCH_ERR("[Autoconf] Error during libusb_control_transfer.\n");
      goto error;
   }

   if (ret != USB_PACKET_CTRL_LEN)
   {
      RARCH_ERR("[Autoconf] Unexpected Bliss-Box libusb control transfer length: %d (expected %d).\n",
            ret, USB_PACKET_CTRL_LEN);
      goto error;
   }

   libusb_release_interface(autoconfig_libusb_handle, 0);

#ifdef __linux__
   libusb_attach_kernel_driver(autoconfig_libusb_handle, 0);
#endif

   libusb_close(autoconfig_libusb_handle);
   autoconfig_libusb_handle = NULL;
   libusb_exit(NULL);

   for (i = 0; i < ARRAY_SIZE(blissbox_pad_types); i++)
   {
      const blissbox_pad_type_t *pad = &blissbox_pad_types[i];

      if (!pad || !pad->name || !*pad->name)
         continue;

      if (pad->index == answer[0])
      {
         return pad;
      }
   }

   RARCH_LOG("[Autoconf] Could not find connected pad in Bliss-Box port#%d.\n", pid - BLISSBOX_PID);

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
#if defined(__linux__)
   const blissbox_pad_type_t *pad = input_autoconfigure_get_blissbox_pad_type_hidraw_scan(vid, pid);
   if (pad)
      return pad;
#endif
   return input_autoconfigure_get_blissbox_pad_type_libusb(vid, pid);
#endif
}

void input_autoconfigure_blissbox_override_handler(int vid, int pid,
      char *s, size_t len)
{
   if (pid == BLISSBOX_UPDATE_MODE_PID)
   {
      RARCH_WARN("[Autoconf] Bliss-Box in update mode detected. Ignoring.\n");
   }
   else if (pid == BLISSBOX_OLD_PID)
   {
      RARCH_WARN("[Autoconf] Bliss-Box 1.0 firmware detected. Please update to 2.0 or later.\n");
   }
   else if (pid >= BLISSBOX_PID && pid <= BLISSBOX_PID + BLISSBOX_MAX_PAD_INDEX)
   {
      const blissbox_pad_type_t *pad;
      int index      = pid - BLISSBOX_PID;

      if (blissbox_pads[index])
         pad = blissbox_pads[index];
      else
      {
         pad = input_autoconfigure_get_blissbox_pad_type(vid, pid);
      }

      if (!pad)
         RARCH_WARN("[Autoconf] [Blissbox] Platform probe returned null for vid=%04x pid=%04x.\n", vid, pid);

      if (pad && pad->name && *pad->name)
      {
         /* override name given to autoconfig so it knows what kind of pad this is */
         if (len > 0)
         {
            size_t _len = strlcpy(s, "Bliss-Box 4-Play ", len);
            strlcpy(s + _len, pad->name, len - _len);
         }

         RARCH_LOG("[Autoconf] Bliss-Box port %d detected as %s.\n", index, pad->name);

         blissbox_pads[index] = pad;
      }
      /* use NULL entry to mark as an unconnected port */
      else
      {
         if (len > 0)
            strlcpy(s, "Bliss-Box 4-Play", len);
         blissbox_pads[index] = &blissbox_pad_types[ARRAY_SIZE(blissbox_pad_types) - 1];
      }
   }
   else
   {
      RARCH_WARN("[Autoconf] [Blissbox] Unexpected PID %04x for VID %04x.\n", pid, vid);
   }
}
