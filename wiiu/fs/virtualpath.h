 /****************************************************************************
 * Copyright (C) 2010
 * by Dimok
 *
 * Original VIRTUAL_PART Struct
 * Copyright (C) 2008
 * Joseph Jordan <joe.ftpii@psychlaw.com.au>
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
 *
 * for WiiXplorer 2010
 ***************************************************************************/
#ifndef _VIRTUALPATH_H_
#define _VIRTUALPATH_H_

#ifdef __cplusplus
extern "C"{
#endif

#include <gctypes.h>

typedef struct {
	char *name;
	char *alias;
	char *prefix;
	bool inserted;
} VIRTUAL_PARTITION;

extern VIRTUAL_PARTITION * VIRTUAL_PARTITIONS;
extern u8 MAX_VIRTUAL_PARTITIONS;

void VirtualMountDevice(const char * devicepath);
void AddVirtualPath(const char *name, const char *alias, const char *prefix);
void MountVirtualDevices();
void UnmountVirtualPaths();

#ifdef __cplusplus
}
#endif

#endif /* _VIRTUALPART_H_ */
