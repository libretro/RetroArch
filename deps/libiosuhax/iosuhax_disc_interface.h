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
#ifndef _IOSUHAX_DISC_INTERFACE_H_
#define _IOSUHAX_DISC_INTERFACE_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEVICE_TYPE_WII_U_SD        (('W'<<24)|('U'<<16)|('S'<<8)|'D')
#define DEVICE_TYPE_WII_U_USB       (('W'<<24)|('U'<<16)|('S'<<8)|'B')
#define FEATURE_WII_U_SD            0x00001000
#define FEATURE_WII_U_USB           0x00002000

#ifndef OGC_DISC_IO_INCLUDE
typedef uint32_t sec_t;

#define FEATURE_MEDIUM_CANREAD      0x00000001
#define FEATURE_MEDIUM_CANWRITE     0x00000002

typedef bool (* FN_MEDIUM_STARTUP)(void) ;
typedef bool (* FN_MEDIUM_ISINSERTED)(void) ;
typedef bool (* FN_MEDIUM_READSECTORS)(uint32_t sector, uint32_t numSectors, void* buffer) ;
typedef bool (* FN_MEDIUM_WRITESECTORS)(uint32_t sector, uint32_t numSectors, const void* buffer) ;
typedef bool (* FN_MEDIUM_CLEARSTATUS)(void) ;
typedef bool (* FN_MEDIUM_SHUTDOWN)(void) ;

struct DISC_INTERFACE_STRUCT {
	unsigned long			ioType ;
	unsigned long			features ;
	FN_MEDIUM_STARTUP		startup ;
	FN_MEDIUM_ISINSERTED	isInserted ;
	FN_MEDIUM_READSECTORS	readSectors ;
	FN_MEDIUM_WRITESECTORS	writeSectors ;
	FN_MEDIUM_CLEARSTATUS	clearStatus ;
	FN_MEDIUM_SHUTDOWN		shutdown ;
} ;

typedef struct DISC_INTERFACE_STRUCT DISC_INTERFACE ;
#endif

extern const DISC_INTERFACE IOSUHAX_sdio_disc_interface;
extern const DISC_INTERFACE IOSUHAX_usb_disc_interface;

#ifdef __cplusplus
}
#endif

#endif
