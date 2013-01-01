/* RetroArch - A frontend for libretro.
* Copyright (C) 2010-2013 - Hans-Kristian Arntzen
* Copyright (C) 2011-2013 - Daniel De Matteis
*
* RetroArch is free software: you can redistribute it and/or modify it under the terms
* of the GNU General Public License as published by the Free Software Found-
* ation, either version 3 of the License, or (at your option) any later version.
*
* RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
* PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with RetroArch.
* If not, see <http://www.gnu.org/licenses/>.
*/

#include "iosupport.h"
#include "undocumented.h"

#include <stdio.h>

#define CTLCODE(DeviceType, Function, Method, Access) ( ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method)  )
#define FSCTL_DISMOUNT_VOLUME  CTLCODE( FILE_DEVICE_FILE_SYSTEM, 0x08, METHOD_BUFFERED, FILE_ANY_ACCESS )

HRESULT xbox_io_mount(char *szDrive, char *szDevice)
{
   char szSourceDevice[48];
   char szDestinationDrive[16];

   snprintf(szSourceDevice, sizeof(szSourceDevice), "\\Device\\%s", szDevice);
   snprintf(szDestinationDrive, sizeof(szDestinationDrive), "\\??\\%s", szDrive);
   RARCH_LOG("xbox_io_mount() - source device: %s.\n", szSourceDevice);
   RARCH_LOG("xbox_io_mount() - destination drive: %s.\n", szDestinationDrive);

   STRING DeviceName =
   {
      strlen(szSourceDevice),
      strlen(szSourceDevice) + 1,
      szSourceDevice
   };

   STRING LinkName =
   {
      strlen(szDestinationDrive),
      strlen(szDestinationDrive) + 1,
      szDestinationDrive
   };

   IoCreateSymbolicLink(&LinkName, &DeviceName);

   return S_OK;
}

HRESULT xbox_io_unmount(char *szDrive)
{
   char szDestinationDrive[16];
   snprintf(szDestinationDrive, sizeof(szDestinationDrive), "\\??\\%s", szDrive);

   STRING LinkName =
   {
      strlen(szDestinationDrive),
      strlen(szDestinationDrive) + 1,
      szDestinationDrive
   };

   IoDeleteSymbolicLink(&LinkName);

   return S_OK;
}

HRESULT xbox_io_remount(char *szDrive, char *szDevice)
{
   char szSourceDevice[48];
   snprintf(szSourceDevice, sizeof(szSourceDevice), "\\Device\\%s", szDevice);

   xbox_io_unmount(szDrive);

   ANSI_STRING filename;
   OBJECT_ATTRIBUTES attributes;
   IO_STATUS_BLOCK status;
   HANDLE hDevice;
   NTSTATUS error;
   DWORD dummy;

   RtlInitAnsiString(&filename, szSourceDevice);
   InitializeObjectAttributes(&attributes, &filename, OBJ_CASE_INSENSITIVE, NULL);

   if (!NT_SUCCESS(error = NtCreateFile(&hDevice, GENERIC_READ |
	                                     SYNCHRONIZE | FILE_READ_ATTRIBUTES, &attributes, &status, NULL, 0,
	                                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN,
	                                     FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT)))
   {
      return E_FAIL;
   }

   if (!DeviceIoControl(hDevice, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &dummy, NULL))
   {
      CloseHandle(hDevice);
      return E_FAIL;
   }

   CloseHandle(hDevice);
   xbox_io_mount(szDrive, szDevice);

   return S_OK;
}

HRESULT xbox_io_remap(char *szMapping)
{
   char szMap[32];
   strlcpy(szMap, szMapping, sizeof(szMap));

   char *pComma = strstr(szMap, ",");

   if (pComma)
   {
      *pComma = 0;

      // map device to drive letter
      xbox_io_unmount(szMap);
      xbox_io_mount(szMap, &pComma[1]);
      return S_OK;
   }

   return E_FAIL;
}

HRESULT xbox_io_shutdown(void)
{
   HalInitiateShutdown();
   return S_OK;
}
