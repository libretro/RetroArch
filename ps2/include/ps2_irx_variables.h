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

#ifndef PS2_IRX_VARIABLES_H
#define PS2_IRX_VARIABLES_H

extern unsigned char sio2man_irx[] __attribute__((aligned(16)));
extern unsigned int size_sio2man_irx;

extern unsigned char mcman_irx[] __attribute__((aligned(16)));
extern unsigned int size_mcman_irx;

extern unsigned char mcserv_irx[] __attribute__((aligned(16)));
extern unsigned int size_mcserv_irx;

extern unsigned char mtapman_irx[] __attribute__((aligned(16)));
extern unsigned int size_mtapman_irx;

extern unsigned char padman_irx[] __attribute__((aligned(16)));
extern unsigned int size_padman_irx;

extern unsigned char iomanX_irx[] __attribute__((aligned(16)));
extern unsigned int size_iomanX_irx;

extern unsigned char fileXio_irx[] __attribute__((aligned(16)));
extern unsigned int size_fileXio_irx;

extern unsigned char ps2dev9_irx[] __attribute__((aligned(16)));
extern unsigned int size_ps2dev9_irx;

extern unsigned char ps2atad_irx[] __attribute__((aligned(16)));
extern unsigned int size_ps2atad_irx;

extern unsigned char ps2hdd_irx[] __attribute__((aligned(16)));
extern unsigned int size_ps2hdd_irx;

extern unsigned char ps2fs_irx[] __attribute__((aligned(16)));
extern unsigned int size_ps2fs_irx;

extern unsigned char usbd_irx[] __attribute__((aligned(16)));
extern unsigned int size_usbd_irx;

extern unsigned char bdm_irx[] __attribute__((aligned(16)));
extern unsigned int size_bdm_irx;

extern unsigned char bdmfs_vfat_irx[] __attribute__((aligned(16)));
extern unsigned int size_bdmfs_vfat_irx;

extern unsigned char usbmass_bd_irx[] __attribute__((aligned(16)));
extern unsigned int size_usbmass_bd_irx;

extern unsigned char cdfs_irx[] __attribute__((aligned(16)));
extern unsigned int size_cdfs_irx;

extern unsigned char libsd_irx[] __attribute__((aligned(16)));
extern unsigned int size_libsd_irx;

extern unsigned char audsrv_irx[] __attribute__((aligned(16)));
extern unsigned int size_audsrv_irx;

extern unsigned char ps2dev9_irx[] __attribute__((aligned(16)));
extern unsigned int size_ps2dev9_irx;

extern unsigned char ps2atad_irx[] __attribute__((aligned(16)));
extern unsigned int size_ps2atad_irx;

extern unsigned char ps2hdd_irx[] __attribute__((aligned(16)));
extern unsigned int size_ps2hdd_irx;

extern unsigned char ps2fs_irx[] __attribute__((aligned(16)));
extern unsigned int size_ps2fs_irx;

extern unsigned char poweroff_irx[] __attribute__((aligned(16)));
extern unsigned int size_poweroff_irx;

#endif /* PS2_IRX_VARIABLES_H */
