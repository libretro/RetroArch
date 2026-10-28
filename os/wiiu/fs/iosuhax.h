/***************************************************************************
 * Copyright (C) 2016
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/

/* Altered for RetroArch: the parts of libiosuhax RetroArch uses, taken
 * out of deps/libiosuhax. Only what os/wiiu/main.c calls is public;
 * libfat's sector interface is in iosuhax_disc_interface.h. */

#ifndef __WIIU_IOSUHAX_H
#define __WIIU_IOSUHAX_H

#ifdef __cplusplus
extern "C" {
#endif

/* NULL opens /dev/iosuhax; any other device is checked to be one */
int IOSUHAX_Open(const char *dev);
int IOSUHAX_Close(void);

int IOSUHAX_FSA_Open(void);
int IOSUHAX_FSA_Close(int fsaFd);

/* Makes @mount_path, already mounted in IOSU, reachable through newlib
 * as "@virt_name:/..." on the FSA client @fsaFd. */
int mount_fs(const char *virt_name, int fsaFd, const char *mount_path);
int unmount_fs(const char *virt_name);

#ifdef __cplusplus
}
#endif

#endif
