/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2018 - Francisco Javier Trujillo Mata - fjtrujy
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 * * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PS2_DEVICES_H
#define PS2_DEVICES_H

#include <stdbool.h>

enum BootDeviceIDs{
   BOOT_DEVICE_UNKNOWN = -1,
   BOOT_DEVICE_MC0 = 0,
   BOOT_DEVICE_MC1,
   BOOT_DEVICE_CDROM,
   BOOT_DEVICE_CDFS,
   BOOT_DEVICE_MASS,
   BOOT_DEVICE_MASS0,
   BOOT_DEVICE_HDD,
   BOOT_DEVICE_HDD0,
   BOOT_DEVICE_HOST,
   BOOT_DEVICE_HOST0,
   BOOT_DEVICE_HOST1,
   BOOT_DEVICE_HOST2,
   BOOT_DEVICE_HOST3,
   BOOT_DEVICE_HOST4,
   BOOT_DEVICE_HOST5,
   BOOT_DEVICE_HOST6,
   BOOT_DEVICE_HOST7,
   BOOT_DEVICE_HOST8,
   BOOT_DEVICE_HOST9,
   BOOT_DEVICE_COUNT,
};

char *rootDevicePath(enum BootDeviceIDs device_id);

enum BootDeviceIDs getBootDeviceID(char *path);

bool waitUntilDeviceIsReady(enum BootDeviceIDs device_id);

#endif
