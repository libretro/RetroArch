/****************************************************************************
 * TinySMB
 * Nintendo Wii/GameCube SMB implementation
 *
 * Copyright softdev
 * Modified by Tantric to utilize NTLM authentication
 * PathInfo added by rodries
 * SMB devoptab by scip, rodries
 *
 * You will find WireShark (http://www.wireshark.org/)
 * invaluable for debugging SAMBA implementations.
 *
 * Recommended Reading
 *	Implementing CIFS - Christopher R Hertel
 *	http://www.ubiqx.org/cifs/SMB.html
 *
 * License:
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 ****************************************************************************/

#ifndef __NBTSMB_H__
#define __NBTSMB_H__

#include <gctypes.h>
#include <sys/statvfs.h>

#define SMB_MAXPATH					4096

/**
* SMB Error codes
*/
#define SMB_SUCCESS					0
#define SMB_ERROR				   -1
#define SMB_BAD_PROTOCOL		   -2
#define SMB_BAD_COMMAND			   -3
#define SMB_PROTO_FAIL			   -4
#define SMB_NOT_USER			   -5
#define SMB_BAD_KEYLEN			   -6
#define SMB_BAD_DATALEN			   -7
#define SMB_BAD_LOGINDATA		   -8

/**
* SMB File Open Function
*/
#define SMB_OF_OPEN					1
#define SMB_OF_TRUNCATE				2
#define SMB_OF_CREATE				16

/**
* FileSearch
*/
#define SMB_SRCH_READONLY  			1
#define SMB_SRCH_HIDDEN				2
#define SMB_SRCH_SYSTEM				4
#define SMB_SRCH_VOLUME				8
#define SMB_SRCH_DIRECTORY			16
#define SMB_SRCH_ARCHIVE			32

/**
* SMB File Access Modes
*/
#define SMB_OPEN_READING			0
#define SMB_OPEN_WRITING			1
#define SMB_OPEN_READWRITE			2
#define SMB_OPEN_COMPATIBLE			0
#define SMB_DENY_READWRITE			0x10
#define SMB_DENY_WRITE				0x20
#define SMB_DENY_READ				0x30
#define SMB_DENY_NONE				0x40

#ifdef __cplusplus
extern "C" {
#endif

/***
 * SMB Connection Handle
 */
typedef u32 SMBCONN;

/***
 * SMB File Handle
 */
typedef void* SMBFILE;

/*** SMB_FILEENTRY
     SMB Long Filename Directory Entry
 ***/
 typedef struct
{
  u64 size;
  u64 ctime;
  u64 atime;
  u64 mtime;
  u32 attributes;
  u16 sid;
  char name[768]; //unicode
} SMBDIRENTRY;

/**
 * Prototypes
 */

/*** Functions to be used with stdio API ***/
bool smbInitDevice(const char* name, const char *user, const char *password, const char *share,	const char *ip);
bool smbInit(const char *user, const char *password, const char *share,	const char *ip);
void smbClose(const char* name);
bool smbCheckConnection(const char* name);
void smbSetSearchFlags(unsigned short flags);

/*** Session ***/
s32 SMB_Connect(SMBCONN *smbhndl, const char *user, const char *password, const char *share, const char *IP);
void SMB_Close(SMBCONN smbhndl);
s32 SMB_Reconnect(SMBCONN *_smbhndl, bool test_conn);

/*** File Find ***/
s32 SMB_PathInfo(const char *filename, SMBDIRENTRY *sdir, SMBCONN smbhndl);
s32 SMB_FindFirst(const char *filename, unsigned short flags, SMBDIRENTRY *sdir, SMBCONN smbhndl);
s32 SMB_FindNext(SMBDIRENTRY *sdir,SMBCONN smbhndl);
s32 SMB_FindClose(SMBDIRENTRY *sdir,SMBCONN smbhndl);

/*** File I/O ***/
SMBFILE SMB_OpenFile(const char *filename, unsigned short access, unsigned short creation,SMBCONN smbhndl);
void SMB_CloseFile(SMBFILE sfid);
s32 SMB_ReadFile(char *buffer, size_t size, off_t offset, SMBFILE sfid);
s32 SMB_WriteFile(const char *buffer, size_t size, off_t offset, SMBFILE sfid);
s32 SMB_CreateDirectory(const char *dirname, SMBCONN smbhndl);
s32 SMB_DeleteDirectory(const char *dirname, SMBCONN smbhndl);
s32 SMB_DeleteFile(const char *filename, SMBCONN smbhndl);
s32 SMB_Rename(const char *filename, const char * newname, SMBCONN smbhndl);
s32 SMB_DiskInformation(struct statvfs *buf, SMBCONN smbhndl);

#ifdef __cplusplus
	}
#endif

#endif
