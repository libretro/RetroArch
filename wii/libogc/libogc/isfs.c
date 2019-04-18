/*-------------------------------------------------------------

es.c -- ETicket services

Copyright (C) 2008
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)
Hector Martin (marcan)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#if defined(HW_RVL)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <gcutil.h>
#include <ipc.h>

#include "isfs.h"

#define ISFS_STRUCTSIZE				(sizeof(struct isfs_cb))
#define ISFS_HEAPSIZE				(ISFS_STRUCTSIZE<<4)

#define ISFS_FUNCNULL				0
#define ISFS_FUNCGETSTAT			1
#define ISFS_FUNCREADDIR			2
#define ISFS_FUNCGETATTR			3
#define ISFS_FUNCGETUSAGE			4

#define ISFS_IOCTL_FORMAT			1
#define ISFS_IOCTL_GETSTATS			2
#define ISFS_IOCTL_CREATEDIR		3
#define ISFS_IOCTL_READDIR			4
#define ISFS_IOCTL_SETATTR			5
#define ISFS_IOCTL_GETATTR			6
#define ISFS_IOCTL_DELETE			7
#define ISFS_IOCTL_RENAME			8
#define ISFS_IOCTL_CREATEFILE		9
#define ISFS_IOCTL_SETFILEVERCTRL	10
#define ISFS_IOCTL_GETFILESTATS		11
#define ISFS_IOCTL_GETUSAGE			12
#define ISFS_IOCTL_SHUTDOWN			13

struct isfs_cb
{
	char filepath[ISFS_MAXPATH];
	union {
		struct {
			char filepathOld[ISFS_MAXPATH];
			char filepathNew[ISFS_MAXPATH];
		} fsrename;
		struct {
			u32 owner_id;
			u16 group_id;
			char filepath[ISFS_MAXPATH];
			u8 ownerperm;
			u8 groupperm;
			u8 otherperm;
			u8 attributes;
			u8 pad0[2];
		} fsattr;
		struct {
			ioctlv vector[4];
			u32 no_entries;
		} fsreaddir;
		struct {
			ioctlv vector[4];
			u32 usage1;
			u8 pad0[28];
			u32 usage2;
		} fsusage;
		struct {
			u32	a;
			u32	b;
			u32	c;
			u32	d;
			u32	e;
			u32	f;
			u32	g;
		} fsstats;
	};

	isfscallback cb;
	void *usrdata;
	u32 functype;
	void *funcargv[8];
};

static s32 hId = -1;
static s32 _fs_fd = -1;
static char _dev_fs[] ATTRIBUTE_ALIGN(32) = "/dev/fs";

static s32 __isfsGetStatsCB(s32 result,void *usrdata)
{
	struct isfs_cb *param = (struct isfs_cb*)usrdata;
	if(result==0) memcpy(param->funcargv[0],&param->fsstats,sizeof(param->fsstats));
	return result;
}

static s32 __isfsGetAttrCB(s32 result,void *usrdata)
{
	struct isfs_cb *param = (struct isfs_cb*)usrdata;
	if(result==0) {
		*(u32*)(param->funcargv[0]) = param->fsattr.owner_id;
		*(u16*)(param->funcargv[1]) = param->fsattr.group_id;
		*(u8*)(param->funcargv[2]) = param->fsattr.attributes;
		*(u8*)(param->funcargv[3]) = param->fsattr.ownerperm;
		*(u8*)(param->funcargv[4]) = param->fsattr.groupperm;
		*(u8*)(param->funcargv[5]) = param->fsattr.otherperm;
	}
	return result;
}

static s32 __isfsGetUsageCB(s32 result,void *usrdata)
{
	struct isfs_cb *param = (struct isfs_cb*)usrdata;
	if(result==0) {
		*(u32*)(param->funcargv[0]) = param->fsusage.usage1;
		*(u32*)(param->funcargv[1]) = param->fsusage.usage2;
	}
	return result;

}
static s32 __isfsReadDirCB(s32 result,void *usrdata)
{
	struct isfs_cb *param = (struct isfs_cb*)usrdata;
	if(result==0) *(u32*)(param->funcargv[0]) = param->fsreaddir.no_entries;
	return result;
}

static s32 __isfsFunctionCB(s32 result,void *usrdata)
{
	struct isfs_cb *param = (struct isfs_cb*)usrdata;

	if(result>=0) {
		if(param->functype==ISFS_FUNCGETSTAT) __isfsGetStatsCB(result,usrdata);
		else if(param->functype==ISFS_FUNCREADDIR) __isfsReadDirCB(result,usrdata);
		else if(param->functype==ISFS_FUNCGETATTR) __isfsGetAttrCB(result,usrdata);
		else if(param->functype==ISFS_FUNCGETUSAGE) __isfsGetUsageCB(result,usrdata);
	}
	if(param->cb!=NULL) param->cb(result,param->usrdata);

	iosFree(hId,param);
	return result;
}

s32 ISFS_Initialize()
{
	s32 ret = IPC_OK;

	if(_fs_fd<0) {
		_fs_fd = IOS_Open(_dev_fs,0);
		if(_fs_fd<0) return _fs_fd;
	}

	if(hId<0) {
		hId = iosCreateHeap(ISFS_HEAPSIZE);
		if(hId<0) return IPC_ENOMEM;
	}
	return ret;
}

s32 ISFS_Deinitialize()
{
	if(_fs_fd<0) return ISFS_EINVAL;

	IOS_Close(_fs_fd);
	_fs_fd = -1;

	return 0;
}

s32 ISFS_ReadDir(const char *filepath,char *name_list,u32 *num)
{
	s32 ret;
	s32 len,cnt;
	s32 ilen,olen;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL || num==NULL) return ISFS_EINVAL;
	if(name_list!=NULL && ((u32)name_list%32)!=0) return ISFS_EINVAL;

	len = strnlen(filepath,ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	memcpy(param->filepath,filepath,(len+1));

	param->fsreaddir.vector[0].data = param->filepath;
	param->fsreaddir.vector[0].len = ISFS_MAXPATH;
	param->fsreaddir.vector[1].data = &param->fsreaddir.no_entries;
	param->fsreaddir.vector[1].len = sizeof(u32);

	if(name_list!=NULL) {
		ilen = olen = 2;
		cnt = *num;
		param->fsreaddir.no_entries = cnt;
		param->fsreaddir.vector[2].data = name_list;
		param->fsreaddir.vector[2].len = (cnt*13);
		param->fsreaddir.vector[3].data = &param->fsreaddir.no_entries;
		param->fsreaddir.vector[3].len = sizeof(u32);
	} else
		ilen = olen = 1;

	ret = IOS_Ioctlv(_fs_fd,ISFS_IOCTL_READDIR,ilen,olen,param->fsreaddir.vector);
	if(ret==0) *num = param->fsreaddir.no_entries;

	if(param!=NULL) iosFree(hId,param);
	return ret;
}

s32 ISFS_ReadDirAsync(const char *filepath,char *name_list,u32 *num,isfscallback cb,void *usrdata)
{
	s32 len,cnt;
	s32 ilen,olen;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL || num==NULL) return ISFS_EINVAL;
	if(name_list!=NULL && ((u32)name_list%32)!=0) return ISFS_EINVAL;

	len = strnlen(filepath,ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->funcargv[0] = num;
	param->functype = ISFS_FUNCREADDIR;
	memcpy(param->filepath,filepath,(len+1));

	param->fsreaddir.vector[0].data = param->filepath;
	param->fsreaddir.vector[0].len = ISFS_MAXPATH;
	param->fsreaddir.vector[1].data = &param->fsreaddir.no_entries;
	param->fsreaddir.vector[1].len = sizeof(u32);

	if(name_list!=NULL) {
		ilen = olen = 2;
		cnt = *num;
		param->fsreaddir.no_entries = cnt;
		param->fsreaddir.vector[2].data = name_list;
		param->fsreaddir.vector[2].len = (cnt*13);
		param->fsreaddir.vector[3].data = &param->fsreaddir.no_entries;
		param->fsreaddir.vector[3].len = sizeof(u32);
	} else
		ilen = olen = 1;

	return IOS_IoctlvAsync(_fs_fd,ISFS_IOCTL_READDIR,ilen,olen,param->fsreaddir.vector,__isfsFunctionCB,param);
}

s32 ISFS_CreateDir(const char *filepath,u8 attributes,u8 owner_perm,u8 group_perm,u8 other_perm)
{
	s32 ret;
	s32 len;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL) return ISFS_EINVAL;

	len = strnlen(filepath,ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	memcpy(param->fsattr.filepath ,filepath,(len+1));

	param->fsattr.attributes = attributes;
	param->fsattr.ownerperm = owner_perm;
	param->fsattr.groupperm = group_perm;
	param->fsattr.otherperm = other_perm;
	ret = IOS_Ioctl(_fs_fd,ISFS_IOCTL_CREATEDIR,&param->fsattr,sizeof(param->fsattr),NULL,0);

	if(param!=NULL) iosFree(hId,param);
	return ret;
}

s32 ISFS_CreateDirAsync(const char *filepath,u8 attributes,u8 owner_perm,u8 group_perm,u8 other_perm,isfscallback cb,void *usrdata)
{
	s32 len;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL) return ISFS_EINVAL;

	len = strnlen(filepath,ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCNULL;
	memcpy(param->fsattr.filepath,filepath,(len+1));

	param->fsattr.attributes = attributes;
	param->fsattr.ownerperm = owner_perm;
	param->fsattr.groupperm = group_perm;
	param->fsattr.otherperm = other_perm;
	return IOS_IoctlAsync(_fs_fd,ISFS_IOCTL_CREATEDIR,&param->fsattr,sizeof(param->fsattr),NULL,0,__isfsFunctionCB,param);
}

s32 ISFS_Open(const char *filepath,u8 mode)
{
	s32 ret;
	s32 len;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL) return ISFS_EINVAL;

	len = strnlen(filepath,ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	memcpy(param->filepath,filepath,(len+1));
	ret = IOS_Open(param->filepath,mode);

	if(param!=NULL) iosFree(hId,param);
	return ret;
}

s32 ISFS_OpenAsync(const char *filepath,u8 mode,isfscallback cb,void *usrdata)
{
	s32 len;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL) return ISFS_EINVAL;

	len = strnlen(filepath,ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCNULL;
	memcpy(param->filepath,filepath,(len+1));
	return IOS_OpenAsync(param->filepath,mode,__isfsFunctionCB,param);
}

s32 ISFS_Format()
{
	if(_fs_fd<0) return ISFS_EINVAL;

	return IOS_Ioctl(_fs_fd,ISFS_IOCTL_FORMAT,NULL,0,NULL,0);
}

s32 ISFS_FormatAsync(isfscallback cb,void *usrdata)
{
	struct isfs_cb *param;

	if(_fs_fd<0) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCNULL;
	return IOS_IoctlAsync(_fs_fd,ISFS_IOCTL_FORMAT,NULL,0,NULL,0,__isfsFunctionCB,param);
}

s32 ISFS_GetStats(void *stats)
{
	s32 ret = ISFS_OK;
	struct isfs_cb *param;

	if(_fs_fd<0 || stats==NULL) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	ret = IOS_Ioctl(_fs_fd,ISFS_IOCTL_GETSTATS,NULL,0,&param->fsstats,sizeof(param->fsstats));
	if(ret==IPC_OK) memcpy(stats,&param->fsstats,sizeof(param->fsstats));

	if(param!=NULL) iosFree(hId,param);
	return ret;
}

s32 ISFS_GetStatsAsync(void *stats,isfscallback cb,void *usrdata)
{
	struct isfs_cb *param;

	if(_fs_fd<0 || stats==NULL) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCGETSTAT;
	param->funcargv[0] = stats;
	return IOS_IoctlAsync(_fs_fd,ISFS_IOCTL_GETSTATS,NULL,0,&param->fsstats,sizeof(param->fsstats),__isfsFunctionCB,param);
}

s32 ISFS_Write(s32 fd,const void *buffer,u32 length)
{
	if(length<=0 || buffer==NULL) return ISFS_EINVAL;

	return IOS_Write(fd,buffer,length);
}

s32 ISFS_WriteAsync(s32 fd,const void *buffer,u32 length,isfscallback cb,void *usrdata)
{
	struct isfs_cb *param;

	if(length<=0 || buffer==NULL) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCNULL;
	return IOS_WriteAsync(fd,buffer,length,__isfsFunctionCB,param);
}

s32 ISFS_Read(s32 fd,void *buffer,u32 length)
{
	if(length<=0 || buffer==NULL || ((u32)buffer%32)!=0) return ISFS_EINVAL;

	return IOS_Read(fd,buffer,length);
}

s32 ISFS_ReadAsync(s32 fd,void *buffer,u32 length,isfscallback cb,void *usrdata)
{
	struct isfs_cb *param;

	if(length<=0 || buffer==NULL || ((u32)buffer%32)!=0) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCNULL;
	return IOS_ReadAsync(fd,buffer,length,__isfsFunctionCB,param);
}

s32 ISFS_Seek(s32 fd,s32 where,s32 whence)
{
	return IOS_Seek(fd,where,whence);
}

s32 ISFS_SeekAsync(s32 fd,s32 where,s32 whence,isfscallback cb,void *usrdata)
{
	struct isfs_cb *param;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCNULL;
	return IOS_SeekAsync(fd,where,whence,__isfsFunctionCB,param);
}

s32 ISFS_CreateFile(const char *filepath,u8 attributes,u8 owner_perm,u8 group_perm,u8 other_perm)
{
	s32 ret;
	s32 len;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL) return ISFS_EINVAL;

	len = strnlen(filepath,ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	memcpy(param->fsattr.filepath,filepath,(len+1));

	param->fsattr.attributes = attributes;
	param->fsattr.ownerperm = owner_perm;
	param->fsattr.groupperm = group_perm;
	param->fsattr.otherperm = other_perm;
	ret = IOS_Ioctl(_fs_fd,ISFS_IOCTL_CREATEFILE,&param->fsattr,sizeof(param->fsattr),NULL,0);

	if(param!=NULL) iosFree(hId,param);
	return ret;
}

s32 ISFS_CreateFileAsync(const char *filepath,u8 attributes,u8 owner_perm,u8 group_perm,u8 other_perm,isfscallback cb,void *usrdata)
{
	s32 len;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL) return ISFS_EINVAL;

	len = strnlen(filepath,ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCNULL;
	memcpy(param->fsattr.filepath,filepath,(len+1));

	param->fsattr.attributes = attributes;
	param->fsattr.ownerperm = owner_perm;
	param->fsattr.groupperm = group_perm;
	param->fsattr.otherperm = other_perm;
	return IOS_IoctlAsync(_fs_fd,ISFS_IOCTL_CREATEFILE,&param->fsattr,sizeof(param->fsattr),NULL,0,__isfsFunctionCB,param);
}

s32 ISFS_Delete(const char *filepath)
{
	s32 ret;
	s32 len;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL) return ISFS_EINVAL;

	len = strnlen(filepath,ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	memcpy(param->filepath,filepath,(len+1));
	ret = IOS_Ioctl(_fs_fd,ISFS_IOCTL_DELETE,param->filepath,ISFS_MAXPATH,NULL,0);

	if(param!=NULL) iosFree(hId,param);
	return ret;
}

s32 ISFS_DeleteAsync(const char *filepath,isfscallback cb,void *usrdata)
{
	s32 len;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL) return ISFS_EINVAL;

	len = strnlen(filepath,ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCNULL;
	memcpy(param->filepath,filepath,(len+1));
	return IOS_IoctlAsync(_fs_fd,ISFS_IOCTL_DELETE,param->filepath,ISFS_MAXPATH,NULL,0,__isfsFunctionCB,param);
}

s32 ISFS_Close(s32 fd)
{
	if(fd<0) return 0;

	return IOS_Close(fd);
}

s32 ISFS_CloseAsync(s32 fd,isfscallback cb,void *usrdata)
{
	struct isfs_cb *param;

	if(fd<0) return 0;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCNULL;
	return IOS_CloseAsync(fd,__isfsFunctionCB,param);
}

s32 ISFS_GetFileStats(s32 fd,fstats *status)
{
	if(status==NULL || ((u32)status%32)!=0) return ISFS_EINVAL;

	return IOS_Ioctl(fd,ISFS_IOCTL_GETFILESTATS,NULL,0,status,sizeof(fstats));
}

s32 ISFS_GetFileStatsAsync(s32 fd,fstats *status,isfscallback cb,void *usrdata)
{
	struct isfs_cb *param;

	if(status==NULL || ((u32)status%32)!=0) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCNULL;
	return IOS_IoctlAsync(fd,ISFS_IOCTL_GETFILESTATS,NULL,0,status,sizeof(fstats),__isfsFunctionCB,param);
}

s32 ISFS_GetAttr(const char *filepath,u32 *ownerID,u16 *groupID,u8 *attributes,u8 *ownerperm,u8 *groupperm,u8 *otherperm)
{
	s32 ret;
	s32 len;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL || ownerID==NULL || groupID==NULL
		|| attributes==NULL || ownerperm==NULL || groupperm==NULL || otherperm==NULL) return ISFS_EINVAL;

	len = strnlen(filepath,ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	memcpy(param->filepath,filepath,(len+1));
	ret = IOS_Ioctl(_fs_fd,ISFS_IOCTL_GETATTR,param->filepath,ISFS_MAXPATH,&param->fsattr,sizeof(param->fsattr));
	if(ret==IPC_OK) {
		*ownerID = param->fsattr.owner_id;
		*groupID = param->fsattr.group_id;
		*ownerperm = param->fsattr.ownerperm;
		*groupperm = param->fsattr.groupperm;
		*otherperm = param->fsattr.otherperm;
		*attributes = param->fsattr.attributes;
	}

	if(param!=NULL) iosFree(hId,param);
	return ret;
}

s32 ISFS_GetAttrAsync(const char *filepath,u32 *ownerID,u16 *groupID,u8 *attributes,u8 *ownerperm,u8 *groupperm,u8 *otherperm,isfscallback cb,void *usrdata)
{
	s32 len;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL || ownerID==NULL || groupID==NULL
		|| attributes==NULL || ownerperm==NULL || groupperm==NULL || otherperm==NULL) return ISFS_EINVAL;

	len = strnlen(filepath,ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCGETATTR;
	param->funcargv[0] = ownerID;
	param->funcargv[1] = groupID;
	param->funcargv[2] = attributes;
	param->funcargv[3] = ownerperm;
	param->funcargv[4] = groupperm;
	param->funcargv[5] = otherperm;
	memcpy(param->filepath,filepath,(len+1));
	return IOS_IoctlAsync(_fs_fd,ISFS_IOCTL_GETATTR,param->filepath,ISFS_MAXPATH,&param->fsattr,sizeof(param->fsattr),__isfsFunctionCB,param);
}

s32 ISFS_Rename(const char *filepathOld,const char *filepathNew)
{
	s32 ret;
	s32 len0,len1;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepathOld==NULL || filepathNew==NULL) return ISFS_EINVAL;

	len0 = strnlen(filepathOld,ISFS_MAXPATH);
	if(len0>=ISFS_MAXPATH) return ISFS_EINVAL;

	len1 = strnlen(filepathNew,ISFS_MAXPATH);
	if(len1>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	memcpy(param->fsrename.filepathOld,filepathOld,(len0+1));
	memcpy(param->fsrename.filepathNew,filepathNew,(len1+1));
	ret = IOS_Ioctl(_fs_fd,ISFS_IOCTL_RENAME,&param->fsrename,sizeof(param->fsrename),NULL,0);

	if(param!=NULL) iosFree(hId,param);
	return ret;
}

s32 ISFS_RenameAsync(const char *filepathOld,const char *filepathNew,isfscallback cb,void *usrdata)
{
	s32 len0,len1;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepathOld==NULL || filepathNew==NULL) return ISFS_EINVAL;

	len0 = strnlen(filepathOld,ISFS_MAXPATH);
	if(len0>=ISFS_MAXPATH) return ISFS_EINVAL;

	len1 = strnlen(filepathNew,ISFS_MAXPATH);
	if(len1>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId,ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCNULL;
	memcpy(param->fsrename.filepathOld,filepathOld,(len0+1));
	memcpy(param->fsrename.filepathNew,filepathNew,(len1+1));
	return IOS_IoctlAsync(_fs_fd,ISFS_IOCTL_RENAME,&param->fsrename,sizeof(param->fsrename),NULL,0,__isfsFunctionCB,param);
}

s32 ISFS_SetAttr(const char *filepath,u32 ownerID,u16 groupID,u8 attributes,u8 ownerperm,u8 groupperm,u8 otherperm)
{
	s32 ret, len;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL) return ISFS_EINVAL;

	len = strnlen(filepath, ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId, ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	memcpy(param->fsattr.filepath, filepath, (len+1));
	param->fsattr.owner_id = ownerID;
	param->fsattr.group_id = groupID;
	param->fsattr.ownerperm = ownerperm;
	param->fsattr.groupperm = groupperm;
	param->fsattr.otherperm = otherperm;
	param->fsattr.attributes = attributes;

	ret = IOS_Ioctl(_fs_fd,ISFS_IOCTL_SETATTR,&param->fsattr,sizeof(param->fsattr),NULL,0);

	if(param!=NULL) iosFree(hId,param);
	return ret;
}

s32 ISFS_SetAttrAsync(const char *filepath,u32 ownerID,u16 groupID,u8 attributes,u8 ownerperm,u8 groupperm,u8 otherperm,isfscallback cb,void *usrdata)
{
	s32 len;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL) return ISFS_EINVAL;

	len = strnlen(filepath, ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId, ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCNULL;
	memcpy(param->fsattr.filepath, filepath, (len+1));
	param->fsattr.owner_id = ownerID;
	param->fsattr.group_id = groupID;
	param->fsattr.ownerperm = ownerperm;
	param->fsattr.groupperm = groupperm;
	param->fsattr.otherperm = otherperm;
	param->fsattr.attributes = attributes;
	return IOS_IoctlAsync(_fs_fd,ISFS_IOCTL_SETATTR,&param->fsattr,sizeof(param->fsattr),NULL,0,__isfsFunctionCB,param);
}

s32 ISFS_GetUsage(const char* filepath, u32* usage1, u32* usage2)
{
	s32 ret,len;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL || usage1==NULL || usage2 == NULL)
		return ISFS_EINVAL;

	len = strnlen(filepath, ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId, ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	memcpy(param->filepath,filepath,(len+1));

	param->fsusage.vector[0].data = param->filepath;
	param->fsusage.vector[0].len = ISFS_MAXPATH;
	param->fsusage.vector[1].data = &param->fsusage.usage1;
	param->fsusage.vector[1].len = sizeof(u32);
	param->fsusage.vector[2].data = &param->fsusage.usage2;
	param->fsusage.vector[2].len = sizeof(u32);
	ret = IOS_Ioctlv(_fs_fd,ISFS_IOCTL_GETUSAGE,1,2,param->fsusage.vector);
	if(ret==IPC_OK) {
		*usage1 = param->fsusage.usage1;
		*usage2 = param->fsusage.usage2;
	}

	if(param!=NULL) iosFree(hId, param);
	return ret;
}

s32 ISFS_GetUsageAsync(const char* filepath, u32* usage1, u32* usage2,isfscallback cb,void *usrdata)
{
	s32 len;
	struct isfs_cb *param;

	if(_fs_fd<0 || filepath==NULL || usage1==NULL || usage2 == NULL)
		return ISFS_EINVAL;

	len = strnlen(filepath, ISFS_MAXPATH);
	if(len>=ISFS_MAXPATH) return ISFS_EINVAL;

	param = (struct isfs_cb*)iosAlloc(hId, ISFS_STRUCTSIZE);
	if(param==NULL) return ISFS_ENOMEM;

	param->cb = cb;
	param->usrdata = usrdata;
	param->functype = ISFS_FUNCGETUSAGE;
	param->funcargv[0] = usage1;
	param->funcargv[1] = usage2;
	memcpy(param->filepath,filepath,(len+1));

	param->fsusage.vector[0].data = param->filepath;
	param->fsusage.vector[0].len = ISFS_MAXPATH;
	param->fsusage.vector[1].data = &param->fsusage.usage1;
	param->fsusage.vector[1].len = sizeof(u32);
	param->fsusage.vector[2].data = &param->fsusage.usage2;
	param->fsusage.vector[2].len = sizeof(u32);
	return IOS_IoctlvAsync(_fs_fd,ISFS_IOCTL_GETUSAGE,1,2,param->fsusage.vector,__isfsFunctionCB,param);
}

#endif /* defined(HW_RVL) */
