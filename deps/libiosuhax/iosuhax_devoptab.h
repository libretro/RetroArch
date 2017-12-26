/***************************************************************************
 * Copyright (C) 2015
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
#ifndef __IOSUHAX_DEVOPTAB_H_
#define __IOSUHAX_DEVOPTAB_H_

#ifdef __cplusplus
extern "C" {
#endif

//! virtual name example:   sd or odd (for sd:/ or odd:/ access)
//! fsaFd:                  fd received by IOSUHAX_FSA_Open();
//! dev_path:               (optional) if a device should be mounted to the mount_path. If NULL no IOSUHAX_FSA_Mount is not executed.
//! mount_path:             path to map to virtual device name
int mount_fs(const char *virt_name, int fsaFd, const char *dev_path, const char *mount_path);
int unmount_fs(const char *virt_name);

#ifdef __cplusplus
}
#endif

#endif // __IOSUHAX_DEVOPTAB_H_
