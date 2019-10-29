/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C) 2016-2019 - Andrés Suárez
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
#include <lists/dir_list.h>
#include <file/file_path.h>
#include <file/config_file.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

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

#include "../configuration.h"
#include "../file_path_special.h"
#include "../list_special.h"
#include "../verbosity.h"
#include "../retroarch.h"

#include "tasks_internal.h"

/* HID Class-Specific Requests values. See section 7.2 of the HID specifications */
#define USB_HID_GET_REPORT 0x01
#define USB_CTRL_IN LIBUSB_ENDPOINT_IN|LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE
#define USB_PACKET_CTRL_LEN 5
#define USB_TIMEOUT 5000 /* timeout in ms */

/* only one blissbox per machine is currently supported */
static const blissbox_pad_type_t *blissbox_pads[BLISSBOX_MAX_PADS] = {NULL};

#ifdef HAVE_LIBUSB
static struct libusb_device_handle *autoconfig_libusb_handle = NULL;
#endif

typedef struct autoconfig_disconnect autoconfig_disconnect_t;
typedef struct autoconfig_params     autoconfig_params_t;

struct autoconfig_disconnect
{
   unsigned idx;
   char *msg;
};

struct autoconfig_params
{
   int32_t vid;
   int32_t pid;
   unsigned idx;
   uint32_t max_users;
   char  *name;
   char  *autoconfig_directory;
};

static bool input_autoconfigured[MAX_USERS];
static unsigned input_device_name_index[MAX_INPUT_DEVICES];
static bool input_autoconfigure_swap_override;

/* TODO/FIXME - Not thread safe to access this
 * on main thread as well in its current state -
 * menu_input.c - menu_event calls this function
 * right now, while the underlying variable can
 * be modified by a task thread. */
bool input_autoconfigure_get_swap_override(void)
{
   return input_autoconfigure_swap_override;
}

/* Adds an index for devices with the same name,
 * so they can be identified in the GUI. */
void input_autoconfigure_joypad_reindex_devices(void)
{
   unsigned i, j, k;

   for(i = 0; i < MAX_INPUT_DEVICES; i++)
      input_device_name_index[i] = 0;

   for(i = 0; i < MAX_INPUT_DEVICES; i++)
   {
      const char *tmp = input_config_get_device_name(i);
      if ( !tmp || input_device_name_index[i] )
         continue;

      k = 2; /*Additional devices start at two*/

      for(j = i+1; j < MAX_INPUT_DEVICES; j++)
      {
         const char *other = input_config_get_device_name(j);

         if (!other)
            continue;

         /*another device with the same name found, for the first time*/
         if (string_is_equal(tmp, other) &&
               input_device_name_index[j]==0 )
         {
            /*Mark the first device of the set*/
            input_device_name_index[i] = 1;
            /*count this additional device, from two up*/
            input_device_name_index[j] = k++;
         }
      }
   }
}

static int input_autoconfigure_joypad_try_from_conf(config_file_t *conf,
      autoconfig_params_t *params)
{
   char ident[256];
   char input_driver[32];
   int tmp_int                = 0;
   int              input_vid = 0;
   int              input_pid = 0;
   int                  score = 0;

   ident[0] = input_driver[0] = '\0';

   config_get_array(conf, "input_device", ident, sizeof(ident));
   config_get_array(conf, "input_driver", input_driver, sizeof(input_driver));

   if (config_get_int  (conf, "input_vendor_id", &tmp_int))
      input_vid = tmp_int;

   if (config_get_int  (conf, "input_product_id", &tmp_int))
      input_pid = tmp_int;

   if (params->vid == BLISSBOX_VID)
      input_pid = BLISSBOX_PID;

   /* Check for VID/PID */
   if (     (params->vid == input_vid)
         && (params->pid == input_pid)
         && (params->vid != 0)
         && (params->pid != 0)
         && (params->vid != BLISSBOX_VID)
         && (params->pid != BLISSBOX_PID))
      score += 3;

   /* Check for name match */
   if (!string_is_empty(params->name)
         && !string_is_empty(ident)
         && string_is_equal(ident, params->name))
      score += 2;

   return score;
}

static void input_autoconfigure_joypad_add(config_file_t *conf,
      autoconfig_params_t *params, retro_task_t *task)
{
   char msg[128], display_name[128], device_type[128];
   /* This will be the case if input driver is reinitialized.
    * No reason to spam autoconfigure messages every time. */
   bool block_osd_spam                =
#if defined(HAVE_LIBNX) && defined(HAVE_MENU_WIDGETS)
      true;
#else
      input_autoconfigured[params->idx]
      && !string_is_empty(params->name);
#endif

   msg[0] = display_name[0] = device_type[0] = '\0';

   config_get_array(conf, "input_device_display_name",
         display_name, sizeof(display_name));
   config_get_array(conf, "input_device_type", device_type,
         sizeof(device_type));

   input_autoconfigured[params->idx] = true;

   input_autoconfigure_joypad_conf(conf,
         input_autoconf_binds[params->idx]);

   if (string_is_equal(device_type, "remote"))
   {
      static bool remote_is_bound        = false;
      const char *autoconfig_str         = (string_is_empty(display_name) &&
            !string_is_empty(params->name)) ? params->name : (!string_is_empty(display_name) ? display_name : "N/A");
      strlcpy(msg, autoconfig_str, sizeof(msg));
      strlcat(msg, " configured.", sizeof(msg));

      if (!remote_is_bound)
      {
         task_free_title(task);
         task_set_title(task, strdup(msg));
      }
      remote_is_bound = true;
      if (params->idx == 0)
         input_autoconfigure_swap_override = true;
   }
   else
   {
      bool tmp                    = false;
      const char *autoconfig_str  = (string_is_empty(display_name) &&
            !string_is_empty(params->name))
            ? params->name : (!string_is_empty(display_name) ? display_name : "N/A");

      snprintf(msg, sizeof(msg), "%s %s #%u.",
            autoconfig_str,
            msg_hash_to_str(MSG_DEVICE_CONFIGURED_IN_PORT),
            params->idx);

      /* allow overriding the swap menu controls for player 1*/
      if (params->idx == 0)
      {
         if (config_get_bool(conf, "input_swap_override", &tmp))
            input_autoconfigure_swap_override = tmp;
         else
            input_autoconfigure_swap_override = false;
      }

      if (!block_osd_spam)
      {
         task_free_title(task);
         task_set_title(task, strdup(msg));
      }
   }
   if (!string_is_empty(display_name))
      input_config_set_device_display_name(params->idx, display_name);
   else
      input_config_set_device_display_name(params->idx, params->name);
   if (!string_is_empty(conf->path))
   {
      input_config_set_device_config_name(params->idx, path_basename(conf->path));
      input_config_set_device_config_path(params->idx, conf->path);
   }
   else
   {
      input_config_set_device_config_name(params->idx, "N/A");
      input_config_set_device_config_path(params->idx, "N/A");
   }

   input_autoconfigure_joypad_reindex_devices();
}

static int input_autoconfigure_joypad_from_conf(
      config_file_t *conf, autoconfig_params_t *params, retro_task_t *task)
{
   int ret = input_autoconfigure_joypad_try_from_conf(conf,
         params);

   if (ret)
      input_autoconfigure_joypad_add(conf, params, task);

   config_file_free(conf);

   return ret;
}

static bool input_autoconfigure_joypad_from_conf_dir(
      autoconfig_params_t *params, retro_task_t *task)
{
   size_t i;
   char path[PATH_MAX_LENGTH];
   char best_path[PATH_MAX_LENGTH];
   int ret                    = 0;
   int index                  = -1;
   int current_best           = 0;
   config_file_t *best_conf   = NULL;
   struct string_list *list   = NULL;

   best_path[0]               = '\0';
   path[0]                    = '\0';

   fill_pathname_application_special(path, sizeof(path),
         APPLICATION_SPECIAL_DIRECTORY_AUTOCONFIG);

   list = dir_list_new_special(path, DIR_LIST_AUTOCONFIG, "cfg");

   if (!list || !list->size)
   {
      if (list)
      {
         string_list_free(list);
         list = NULL;
      }
      if (!string_is_empty(params->autoconfig_directory))
         list = dir_list_new_special(params->autoconfig_directory,
               DIR_LIST_AUTOCONFIG, "cfg");
   }

   if (!list)
   {
      RARCH_LOG("[Autoconf]: No profiles found.\n");
      return false;
   }

   RARCH_LOG("[Autoconf]: %d profiles found.\n", (int)list->size);

   for (i = 0; i < list->size; i++)
   {
      int res;
      config_file_t *conf = config_file_new_from_path_to_string(list->elems[i].data);
      
      if (!conf)
         continue;

      res  = input_autoconfigure_joypad_try_from_conf(conf, params);

      if (res >= current_best)
      {
         index        = (int)i;
         current_best = res;
         if (best_conf)
            config_file_free(best_conf);
         strlcpy(best_path, list->elems[i].data, sizeof(best_path));
         best_conf    = NULL;
         best_conf    = conf;
      }
      else
         config_file_free(conf);
   }

   if (index >= 0 && current_best > 0 && best_conf)
   {
      RARCH_LOG("[Autoconf]: selected configuration: %s\n", best_path);
      input_autoconfigure_joypad_add(best_conf, params, task);
      ret = 1;
   }

   if (best_conf)
      config_file_free(best_conf);

   string_list_free(list);

   if (ret == 0)
      return false;
   return true;
}

static bool input_autoconfigure_joypad_from_conf_internal(
      autoconfig_params_t *params, retro_task_t *task)
{
   size_t i;

   /* Load internal autoconfig files  */
   for (i = 0; input_builtin_autoconfs[i]; i++)
   {
      config_file_t *conf = config_file_new_from_string(
            input_builtin_autoconfs[i], NULL);
      if (conf && input_autoconfigure_joypad_from_conf(conf, params, task))
        return true;
   }

   if (string_is_empty(params->autoconfig_directory))
      return true;
   return false;
}

static void input_autoconfigure_params_free(autoconfig_params_t *params)
{
   if (!params)
      return;
   if (!string_is_empty(params->name))
      free(params->name);
   if (!string_is_empty(params->autoconfig_directory))
      free(params->autoconfig_directory);
   params->name                 = NULL;
   params->autoconfig_directory = NULL;
}

#ifdef _WIN32
static const blissbox_pad_type_t* input_autoconfigure_get_blissbox_pad_type_win32(int vid, int pid)
{
   /* TODO: Remove the check for !defined(_MSC_VER) after making sure this builds on MSVC */

   /* HID API is available since Windows 2000 */
#if defined(_WIN32) && !defined(_XBOX) && !defined(_MSC_VER) && _WIN32_WINNT >= 0x0500
   HDEVINFO hDeviceInfo;
   SP_DEVINFO_DATA DeviceInfoData;
   SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
   HANDLE hDeviceHandle                 = INVALID_HANDLE_VALUE;
   BOOL bResult                         = TRUE;
   BOOL success                         = FALSE;
   GUID guidDeviceInterface             = {0};
   PSP_DEVICE_INTERFACE_DETAIL_DATA
      pInterfaceDetailData              = NULL;
   ULONG requiredLength                 = 0;
   LPTSTR lpDevicePath                  = NULL;
   char *devicePath                     = NULL;
   DWORD index                          = 0;
   DWORD intIndex                       = 0;
   size_t nLength                       = 0;
   unsigned len                         = 0;
   unsigned i                           = 0;
   char vidPidString[32]                = {0};
   char vidString[5]                    = {0};
   char pidString[5]                    = {0};
   char report[USB_PACKET_CTRL_LEN + 1] = {0};

   snprintf(vidString, sizeof(vidString), "%04x", vid);
   snprintf(pidString, sizeof(pidString), "%04x", pid);

   strlcat(vidPidString, "vid_", sizeof(vidPidString));
   strlcat(vidPidString, vidString, sizeof(vidPidString));
   strlcat(vidPidString, "&pid_", sizeof(vidPidString));
   strlcat(vidPidString, pidString, sizeof(vidPidString));

   HidD_GetHidGuid(&guidDeviceInterface);

   if (!memcmp(&guidDeviceInterface, &GUID_NULL, sizeof(GUID_NULL)))
   {
     RARCH_ERR("[Autoconf]: null guid\n");
     return NULL;
   }

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
   DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

   while (!success)
   {
      success = SetupDiEnumDeviceInfo(hDeviceInfo, index, &DeviceInfoData);

      /* Reset for this iteration */
      if (lpDevicePath)
      {
         LocalFree(lpDevicePath);
         lpDevicePath = NULL;
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
      for (intIndex = 0; (bResult = SetupDiEnumDeviceInterfaces(
         hDeviceInfo,
         &DeviceInfoData,
         &guidDeviceInterface,
         intIndex,
         &deviceInterfaceData)); intIndex++)
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
            &requiredLength,
            NULL);

         /* Check for some other error */
         if (!bResult)
         {
            if ((ERROR_INSUFFICIENT_BUFFER == GetLastError()) && (requiredLength > 0))
            {
               /* we got the size, now allocate buffer */
               pInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)LocalAlloc(LPTR, requiredLength);

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
            requiredLength,
            NULL,
            &DeviceInfoData);

         /* Check for some other error */
         if (!bResult)
         {
           RARCH_LOG("[Autoconf]: Error in SetupDiGetDeviceInterfaceDetail: %d.\n", GetLastError());
           goto done;
         }

         /* copy device path */
         nLength      = _tcslen(pInterfaceDetailData->DevicePath) + 1;
         lpDevicePath = (TCHAR*)LocalAlloc(LPTR, nLength * sizeof(TCHAR));

         StringCchCopy(lpDevicePath, nLength, pInterfaceDetailData->DevicePath);

         devicePath   = (char*)malloc(nLength);

         for (len = 0; len < nLength; len++)
            devicePath[len] = lpDevicePath[len];

         lpDevicePath[nLength - 1] = 0;

         if (strstr(devicePath, vidPidString))
            goto found;
      }

      success = FALSE;
      index++;
   }

   if (!lpDevicePath)
   {
      RARCH_ERR("[Autoconf]: No devicepath. Error %d.", GetLastError());
      goto done;
   }

found:
   /* Open the device */
   hDeviceHandle = CreateFileA(
      devicePath,
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
         devicePath,
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
   free(devicePath);
   LocalFree(lpDevicePath);
   LocalFree(pInterfaceDetailData);
   bResult              = SetupDiDestroyDeviceInfoList(hDeviceInfo);

   devicePath           = NULL;
   lpDevicePath         = NULL;
   pInterfaceDetailData = NULL;

   if (!bResult)
      RARCH_ERR("[Autoconf]: Could not destroy device info list.\n");

   if (!hDeviceHandle || hDeviceHandle == INVALID_HANDLE_VALUE)
   {
      /* device is not connected */
      return NULL;
   }

   report[0] = BLISSBOX_USB_FEATURE_REPORT_ID;

   HidD_GetFeature(hDeviceHandle, report, sizeof(report));

   CloseHandle(hDeviceHandle);

   for (i = 0; i < sizeof(blissbox_pad_types) / sizeof(blissbox_pad_types[0]); i++)
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

   for (i = 0; i < sizeof(blissbox_pad_types) / sizeof(blissbox_pad_types[0]); i++)
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

static void input_autoconfigure_override_handler(autoconfig_params_t *params)
{
   if (params->vid == BLISSBOX_VID)
   {
      if (params->pid == BLISSBOX_UPDATE_MODE_PID)
         RARCH_LOG("[Autoconf]: Bliss-Box in update mode detected. Ignoring.\n");
      else if (params->pid == BLISSBOX_OLD_PID)
         RARCH_LOG("[Autoconf]: Bliss-Box 1.0 firmware detected. Please update to 2.0 or later.\n");
      else if (params->pid >= BLISSBOX_PID && params->pid <= BLISSBOX_PID + BLISSBOX_MAX_PAD_INDEX)
      {
         const blissbox_pad_type_t *pad;
         char name[255] = {0};
         int index      = params->pid - BLISSBOX_PID;

         RARCH_LOG("[Autoconf]: Bliss-Box detected. Getting pad type...\n");

         if (blissbox_pads[index])
            pad = blissbox_pads[index];
         else
            pad = input_autoconfigure_get_blissbox_pad_type(params->vid, params->pid);

         if (pad && !string_is_empty(pad->name))
         {
            RARCH_LOG("[Autoconf]: Found Bliss-Box pad type: %s (%d) in port#%d\n", pad->name, pad->index, index);

            if (params->name)
               free(params->name);

            /* override name given to autoconfig so it knows what kind of pad this is */
            strlcat(name, "Bliss-Box 4-Play ", sizeof(name));
            strlcat(name, pad->name, sizeof(name));

            params->name = strdup(name);

            blissbox_pads[index] = pad;
         }
         /* use NULL entry to mark as an unconnected port */
         else
            blissbox_pads[index] = &blissbox_pad_types[ARRAY_SIZE(blissbox_pad_types) - 1];
      }
   }
}

static void input_autoconfigure_connect_handler(retro_task_t *task)
{
   autoconfig_params_t *params = (autoconfig_params_t*)task->state;

   if (!params || string_is_empty(params->name))
      goto end;

   if (     !input_autoconfigure_joypad_from_conf_dir(params, task)
         && !input_autoconfigure_joypad_from_conf_internal(params, task))
   {
      char msg[255];

      msg[0] = '\0';
#ifdef ANDROID
      if (!string_is_empty(params->name))
         free(params->name);
      params->name = strdup("Android Gamepad");

      if (input_autoconfigure_joypad_from_conf_internal(params, task))
      {
         RARCH_LOG("[Autoconf]: no profiles found for %s (%d/%d). Using fallback\n",
               !string_is_empty(params->name) ? params->name : "N/A",
               params->vid, params->pid);

         snprintf(msg, sizeof(msg), "%s (%ld/%ld) %s.",
               !string_is_empty(params->name) ? params->name : "N/A",
               (long)params->vid, (long)params->pid,
               msg_hash_to_str(MSG_DEVICE_NOT_CONFIGURED_FALLBACK));
      }
#else
      RARCH_LOG("[Autoconf]: no profiles found for %s (%d/%d).\n",
            !string_is_empty(params->name) ? params->name : "N/A",
            params->vid, params->pid);

      snprintf(msg, sizeof(msg), "%s (%ld/%ld) %s.",
            !string_is_empty(params->name) ? params->name : "N/A",
            (long)params->vid, (long)params->pid,
            msg_hash_to_str(MSG_DEVICE_NOT_CONFIGURED));
#endif
      task_free_title(task);
      task_set_title(task, strdup(msg));
   }

end:
   if (params)
   {
      input_autoconfigure_params_free(params);
      free(params);
   }
   task_set_finished(task, true);
}

static void input_autoconfigure_disconnect_handler(retro_task_t *task)
{
   autoconfig_disconnect_t *params = (autoconfig_disconnect_t*)task->state;

   task_set_title(task, strdup(params->msg));

   task_set_finished(task, true);

   RARCH_LOG("%s: %s\n", msg_hash_to_str(MSG_AUTODETECT), params->msg);

   if (!string_is_empty(params->msg))
      free(params->msg);
   free(params);
}

bool input_autoconfigure_disconnect(unsigned i, const char *ident)
{
   char msg[255];
   retro_task_t         *task      = task_init();
   autoconfig_disconnect_t *state  = (autoconfig_disconnect_t*)calloc(1, sizeof(*state));

   if (!state || !task)
      goto error;

   msg[0]      = '\0';

   state->idx  = i;

   snprintf(msg, sizeof(msg), "%s #%u (%s).",
         msg_hash_to_str(MSG_DEVICE_DISCONNECTED_FROM_PORT),
         i, ident);

   state->msg    = strdup(msg);

   input_config_clear_device_name(state->idx);
   input_config_clear_device_display_name(state->idx);
   input_config_clear_device_config_name(state->idx);
   input_config_clear_device_config_path(state->idx);

   task->state   = state;
   task->handler = input_autoconfigure_disconnect_handler;

   task_queue_push(task);

   return true;

error:
   if (state)
   {
      if (!string_is_empty(state->msg))
         free(state->msg);
      free(state);
   }
   if (task)
      free(task);

   return false;
}

void input_autoconfigure_reset(void)
{
   unsigned i, j;

   for (i = 0; i < MAX_USERS; i++)
   {
      for (j = 0; j < RARCH_BIND_LIST_END; j++)
      {
         input_autoconf_binds[i][j].joykey  = NO_BTN;
         input_autoconf_binds[i][j].joyaxis = AXIS_NONE;
      }
      input_device_name_index[i] = 0;
      input_autoconfigured[i]    = 0;
   }

   input_autoconfigure_swap_override = false;
}

bool input_is_autoconfigured(unsigned i)
{
   return input_autoconfigured[i];
}

unsigned input_autoconfigure_get_device_name_index(unsigned i)
{
   return input_device_name_index[i];
}

void input_autoconfigure_connect(
      const char *name,
      const char *display_name,
      const char *driver,
      unsigned idx,
      unsigned vid,
      unsigned pid)
{
   unsigned i;
   retro_task_t         *task = task_init();
   autoconfig_params_t *state = (autoconfig_params_t*)calloc(1, sizeof(*state));
   settings_t       *settings = config_get_ptr();
   const char *dir_autoconf   = settings ? settings->paths.directory_autoconfig : NULL;
   bool autodetect_enable     = settings ? settings->bools.input_autodetect_enable : false;

   if (!task || !state || !autodetect_enable)
   {
      if (state)
      {
         input_autoconfigure_params_free(state);
         free(state);
      }
      if (task)
         free(task);

      input_config_set_device_name(idx, name);
      return;
   }

   if (!string_is_empty(name))
      state->name                 = strdup(name);

   if (!string_is_empty(dir_autoconf))
      state->autoconfig_directory = strdup(dir_autoconf);

   state->idx                     = idx;
   state->vid                     = vid;
   state->pid                     = pid;
   state->max_users               = *(
         input_driver_get_uint(INPUT_ACTION_MAX_USERS));

   input_autoconfigure_override_handler(state);

   if (!string_is_empty(state->name))
      input_config_set_device_name(state->idx, state->name);
   input_config_set_pid(state->idx, state->pid);
   input_config_set_vid(state->idx, state->vid);

   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      input_autoconf_binds[state->idx][i].joykey           = NO_BTN;
      input_autoconf_binds[state->idx][i].joyaxis          = AXIS_NONE;
      if (
            !string_is_empty(input_autoconf_binds[state->idx][i].joykey_label))
         free(input_autoconf_binds[state->idx][i].joykey_label);
      if (
            !string_is_empty(input_autoconf_binds[state->idx][i].joyaxis_label))
         free(input_autoconf_binds[state->idx][i].joyaxis_label);
      input_autoconf_binds[state->idx][i].joykey_label      = NULL;
      input_autoconf_binds[state->idx][i].joyaxis_label     = NULL;
   }

   input_autoconfigured[state->idx] = false;

   task->state                      = state;
   task->handler                    = input_autoconfigure_connect_handler;

   task_queue_push(task);
}
