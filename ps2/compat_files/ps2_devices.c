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

#include <ps2_devices.h>

#include <stdio.h>
#include <kernel.h>
#include <string.h>
#include <fileXio_rpc.h>

#define DEVICE_SLASH "/"

#define DEVICE_MC0 "mc0:"
#define DEVICE_MC1 "mc1:"
#define DEVICE_CDROM "cdrom0:"
#define DEVICE_CDFS "cdfs:"
#define DEVICE_MASS "mass:"
#define DEVICE_MASS0 "mass0:"
#define DEVICE_HDD "hdd:"
#define DEVICE_HDD0 "hdd0:"
#define DEVICE_HOST "host:"
#define DEVICE_HOST0 "host0:"
#define DEVICE_HOST1 "host1:"
#define DEVICE_HOST2 "host2:"
#define DEVICE_HOST3 "host3:"
#define DEVICE_HOST4 "host4:"
#define DEVICE_HOST5 "host5:"
#define DEVICE_HOST6 "host6:"
#define DEVICE_HOST7 "host7:"
#define DEVICE_HOST8 "host8:"
#define DEVICE_HOST9 "host9:"

#define DEVICE_MC0_PATH DEVICE_MC0 DEVICE_SLASH
#define DEVICE_MC1_PATH DEVICE_MC1 DEVICE_SLASH
#define DEVICE_CDFS_PATH DEVICE_CDFS DEVICE_SLASH
#define DEVICE_CDROM_PATH DEVICE_CDROM DEVICE_SLASH
#define DEVICE_MASS_PATH DEVICE_MASS DEVICE_SLASH
#define DEVICE_MASS0_PATH DEVICE_MASS0 DEVICE_SLASH
#define DEVICE_HDD_PATH DEVICE_HDD DEVICE_SLASH
#define DEVICE_HDD0_PATH DEVICE_HDD0 DEVICE_SLASH
#define DEVICE_HOST_PATH DEVICE_HOST DEVICE_SLASH
#define DEVICE_HOST0_PATH DEVICE_HOST0 DEVICE_SLASH
#define DEVICE_HOST1_PATH DEVICE_HOST1 DEVICE_SLASH
#define DEVICE_HOST2_PATH DEVICE_HOST2 DEVICE_SLASH
#define DEVICE_HOST3_PATH DEVICE_HOST3 DEVICE_SLASH
#define DEVICE_HOST4_PATH DEVICE_HOST4 DEVICE_SLASH
#define DEVICE_HOST5_PATH DEVICE_HOST5 DEVICE_SLASH
#define DEVICE_HOST6_PATH DEVICE_HOST6 DEVICE_SLASH
#define DEVICE_HOST7_PATH DEVICE_HOST7 DEVICE_SLASH
#define DEVICE_HOST8_PATH DEVICE_HOST8 DEVICE_SLASH
#define DEVICE_HOST9_PATH DEVICE_HOST9 DEVICE_SLASH

char *rootDevicePath(enum BootDeviceIDs device_id)
{
   switch (device_id)
   {
      case BOOT_DEVICE_MC0:
         return DEVICE_MC0_PATH;
      case BOOT_DEVICE_MC1:
         return DEVICE_MC1_PATH;
      case BOOT_DEVICE_CDROM:
         return DEVICE_CDROM_PATH;
      case BOOT_DEVICE_CDFS:
         return DEVICE_CDFS_PATH;
      case BOOT_DEVICE_MASS:
         return DEVICE_MASS_PATH;
      case BOOT_DEVICE_MASS0:
         return DEVICE_MASS_PATH;
      case BOOT_DEVICE_HDD:
         return DEVICE_HDD_PATH;
      case BOOT_DEVICE_HDD0:
         return DEVICE_HDD0_PATH;
      case BOOT_DEVICE_HOST:
         return DEVICE_HOST_PATH;
      case BOOT_DEVICE_HOST0:
         return DEVICE_HOST0_PATH;
      case BOOT_DEVICE_HOST1:
         return DEVICE_HOST1_PATH;
      case BOOT_DEVICE_HOST2:
         return DEVICE_HOST2_PATH;
      case BOOT_DEVICE_HOST3:
         return DEVICE_HOST3_PATH;
      case BOOT_DEVICE_HOST4:
         return DEVICE_HOST4_PATH;
      case BOOT_DEVICE_HOST5:
         return DEVICE_HOST5_PATH;
      case BOOT_DEVICE_HOST6:
         return DEVICE_HOST6_PATH;
      case BOOT_DEVICE_HOST7:
         return DEVICE_HOST7_PATH;
      case BOOT_DEVICE_HOST8:
         return DEVICE_HOST8_PATH;
      case BOOT_DEVICE_HOST9:
         return DEVICE_HOST9_PATH;
      default:
         return "";
   }
}

enum BootDeviceIDs getBootDeviceID(char *path)
{
   if (!strncmp(path, DEVICE_MC0, 4))
      return BOOT_DEVICE_MC0;
   else if (!strncmp(path, DEVICE_MC1, 4))
      return BOOT_DEVICE_MC1;
   else if (!strncmp(path, DEVICE_CDROM, 7))
      return BOOT_DEVICE_CDROM;
   else if (!strncmp(path, DEVICE_CDFS, 5))
      return BOOT_DEVICE_CDFS;
   else if (!strncmp(path, DEVICE_MASS, 5))
      return BOOT_DEVICE_MASS;
   else if (!strncmp(path, DEVICE_MASS0, 6))
      return BOOT_DEVICE_MASS0;
   else if (!strncmp(path, DEVICE_HDD, 4))
      return BOOT_DEVICE_HDD;
   else if (!strncmp(path, DEVICE_HDD0, 5))
      return BOOT_DEVICE_HDD0;
   else if (!strncmp(path, DEVICE_HOST, 5))
      return BOOT_DEVICE_HOST;
   else if (!strncmp(path, DEVICE_HOST0, 6))
      return BOOT_DEVICE_HOST0;
   else if (!strncmp(path, DEVICE_HOST1, 6))
      return BOOT_DEVICE_HOST1;
   else if (!strncmp(path, DEVICE_HOST2, 6))
      return BOOT_DEVICE_HOST2;
   else if (!strncmp(path, DEVICE_HOST3, 6))
      return BOOT_DEVICE_HOST3;
   else if (!strncmp(path, DEVICE_HOST4, 6))
      return BOOT_DEVICE_HOST4;
   else if (!strncmp(path, DEVICE_HOST5, 6))
      return BOOT_DEVICE_HOST5;
   else if (!strncmp(path, DEVICE_HOST6, 6))
      return BOOT_DEVICE_HOST6;
   else if (!strncmp(path, DEVICE_HOST7, 6))
      return BOOT_DEVICE_HOST7;
   else if (!strncmp(path, DEVICE_HOST8, 6))
      return BOOT_DEVICE_HOST8;
   else if (!strncmp(path, DEVICE_HOST9, 6))
      return BOOT_DEVICE_HOST9;
   else
      return BOOT_DEVICE_UNKNOWN;
}

/* HACK! If booting from a USB device, keep trying to
 * open this program again until it succeeds.
 *
 * This will ensure that the emulator will be able to load its files.
 */

bool waitUntilDeviceIsReady(enum BootDeviceIDs device_id)
{
   int openFile = - 1;
   int retries = 3; /* just in case we tried a unit that is not working/connected */
   char *rootDevice = rootDevicePath(device_id);

   while(openFile < 0 && retries > 0)
   {
      openFile = fileXioDopen(rootDevice);
      /* Wait untill the device is ready */
      nopdelay();
      nopdelay();
      nopdelay();
      nopdelay();
      nopdelay();
      nopdelay();
      nopdelay();
      nopdelay();

      retries--;
   };
   if (openFile > 0) {
      fileXioDclose(openFile);
   }
   
   return openFile >= 0;
}
