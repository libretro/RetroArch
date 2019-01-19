/*-------------------------------------------------------------

es.c -- ETicket services

Copyright (C) 2008
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)
Hector Martin (marcan)
Andre Heider (dhewg)

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

#include <sys/iosupport.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <sys/errno.h>
#include "ipc.h"
#include "asm.h"
#include "processor.h"
#include "mutex.h"
#include "es.h"

#define IOCTL_ES_ADDTICKET				0x01
#define IOCTL_ES_ADDTITLESTART			0x02
#define IOCTL_ES_ADDCONTENTSTART		0x03
#define IOCTL_ES_ADDCONTENTDATA			0x04
#define IOCTL_ES_ADDCONTENTFINISH		0x05
#define IOCTL_ES_ADDTITLEFINISH			0x06
#define IOCTL_ES_GETDEVICEID			0x07
#define IOCTL_ES_LAUNCH					0x08
#define IOCTL_ES_OPENCONTENT			0x09
#define IOCTL_ES_READCONTENT			0x0A
#define IOCTL_ES_CLOSECONTENT			0x0B
#define IOCTL_ES_GETOWNEDTITLECNT		0x0C
#define IOCTL_ES_GETOWNEDTITLES			0x0D
#define IOCTL_ES_GETTITLECNT			0x0E
#define IOCTL_ES_GETTITLES				0x0F
#define IOCTL_ES_GETTITLECONTENTSCNT	0x10
#define IOCTL_ES_GETTITLECONTENTS		0x11
#define IOCTL_ES_GETVIEWCNT				0x12
#define IOCTL_ES_GETVIEWS				0x13
#define IOCTL_ES_GETTMDVIEWCNT			0x14
#define IOCTL_ES_GETTMDVIEWS			0x15
//#define IOCTL_ES_GETCONSUMPTION		0x16
#define IOCTL_ES_DELETETITLE			0x17
#define IOCTL_ES_DELETETICKET			0x18
//#define IOCTL_ES_DIGETTMDVIEWSIZE		0x19
//#define IOCTL_ES_DIGETTMDVIEW			0x1A
//#define IOCTL_ES_DIGETTICKETVIEW		0x1B
#define IOCTL_ES_DIVERIFY				0x1C
#define IOCTL_ES_GETTITLEDIR			0x1D
#define IOCTL_ES_GETDEVICECERT			0x1E
#define IOCTL_ES_IMPORTBOOT				0x1F
#define IOCTL_ES_GETTITLEID				0x20
#define IOCTL_ES_SETUID					0x21
#define IOCTL_ES_DELETETITLECONTENT		0x22
#define IOCTL_ES_SEEKCONTENT			0x23
#define IOCTL_ES_OPENTITLECONTENT		0x24
//#define IOCTL_ES_LAUNCHBC				0x25
//#define IOCTL_ES_EXPORTTITLEINIT		0x26
//#define IOCTL_ES_EXPORTCONTENTBEGIN	0x27
//#define IOCTL_ES_EXPORTCONTENTDATA	0x28
//#define IOCTL_ES_EXPORTCONTENTEND		0x29
//#define IOCTL_ES_EXPORTTITLEDONE		0x2A
#define IOCTL_ES_ADDTMD					0x2B
#define IOCTL_ES_ENCRYPT				0x2C
#define IOCTL_ES_DECRYPT				0x2D
#define IOCTL_ES_GETBOOT2VERSION		0x2E
#define IOCTL_ES_ADDTITLECANCEL			0x2F
#define IOCTL_ES_SIGN					0x30
//#define IOCTL_ES_VERIFYSIGN			0x31
#define IOCTL_ES_GETSTOREDCONTENTCNT	0x32
#define IOCTL_ES_GETSTOREDCONTENTS		0x33
#define IOCTL_ES_GETSTOREDTMDSIZE		0x34
#define IOCTL_ES_GETSTOREDTMD			0x35
#define IOCTL_ES_GETSHAREDCONTENTCNT	0x36
#define IOCTL_ES_GETSHAREDCONTENTS		0x37

#define ES_HEAP_SIZE 0x800

#define ISALIGNED(x) ((((u32)x)&0x1F)==0)

static char __es_fs[] ATTRIBUTE_ALIGN(32) = "/dev/es";

static s32 __es_fd = -1;
static s32 __es_hid = -1;

static void __ES_InitFS(void);
static void __ES_DeinitFS(void);

s32 __ES_Init(void)
{
	s32 ret = 0;

	if(__es_hid < 0 ) {
		__es_hid = iosCreateHeap(ES_HEAP_SIZE);
		if(__es_hid < 0)
			return __es_hid;
	}

	if (__es_fd < 0) {
		ret = IOS_Open(__es_fs,0);
		if(ret<0)
			return ret;
		__es_fd = ret;
	}

	__ES_InitFS();

	return 0;
}

s32 __ES_Close(void)
{
	s32 ret;

	if(__es_fd < 0) return 0;

	__ES_DeinitFS();

	ret = IOS_Close(__es_fd);

	__es_fd = -1;

	if(ret<0)
		return ret;

	return 0;
}

// Used by ios.c after reloading IOS
s32 __ES_Reset(void)
{
	__es_fd = -1;
	return 0;
}

s32 ES_GetTitleID(u64 *titleID)
{
	s32 ret;
	u64 title;

	if(__es_fd<0) return ES_ENOTINIT;
	if(!titleID) return ES_EINVAL;

	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETTITLEID,":q",&title);
	if(ret<0) return ret;

	*titleID = title;
	return ret;
}

s32 ES_SetUID(u64 uid)
{
	if(__es_fd<0) return ES_ENOTINIT;

	return IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_SETUID,"q:",uid);
}

s32 ES_GetDataDir(u64 titleID,char *filepath)
{
	s32 ret;

	if(__es_fd<0) return ES_ENOTINIT;
	if(!filepath) return ES_EINVAL;
	if(!ISALIGNED(filepath)) return ES_EALIGN;

	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETTITLEDIR,"q:d",titleID,filepath,30);

	return ret;
}

s32 ES_LaunchTitle(u64 titleID, const tikview *view)
{

	s32 res;
	STACK_ALIGN(u64,title,1,32);
	STACK_ALIGN(ioctlv,vectors,2,32);

	if(__es_fd<0) return ES_ENOTINIT;
	if(!view) return ES_EINVAL;
	if(!ISALIGNED(view)) return ES_EALIGN;

	*title = titleID;
	vectors[0].data = (void*)title;
	vectors[0].len = sizeof(u64);
	vectors[1].data = (void*)view;
	vectors[1].len = sizeof(tikview);
	res = IOS_IoctlvReboot(__es_fd,IOCTL_ES_LAUNCH,2,0,vectors);

	return res;
}

s32 ES_LaunchTitleBackground(u64 titleID, const tikview *view)
{

	s32 res;
	STACK_ALIGN(u64,title,1,32);
	STACK_ALIGN(ioctlv,vectors,2,32);

	if(__es_fd<0) return ES_ENOTINIT;
	if(!view) return ES_EINVAL;
	if(!ISALIGNED(view)) return ES_EALIGN;

	*title = titleID;
	vectors[0].data = (void*)title;
	vectors[0].len = sizeof(u64);
	vectors[1].data = (void*)view;
	vectors[1].len = sizeof(tikview);
	res = IOS_IoctlvRebootBackground(__es_fd,IOCTL_ES_LAUNCH,2,0,vectors);

	return res;
}

s32 ES_GetNumTicketViews(u64 titleID, u32 *cnt)
{
	s32 ret;
	u32 cntviews;

	if(__es_fd<0) return ES_ENOTINIT;
	if(!cnt) return ES_EINVAL;

	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETVIEWCNT,"q:i",titleID,&cntviews);

	if(ret<0) return ret;
	*cnt = cntviews;
	return ret;
}

s32 ES_GetTicketViews(u64 titleID, tikview *views, u32 cnt)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(cnt <= 0) return ES_EINVAL;
	if(!views) return ES_EINVAL;
	if(!ISALIGNED(views)) return ES_EALIGN;

	return IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETVIEWS,"qi:d",titleID,cnt,views,sizeof(tikview)*cnt);
}

s32 ES_GetNumOwnedTitles(u32 *cnt)
{
	s32 ret;
	u32 cnttitles;

	if(__es_fd<0) return ES_ENOTINIT;
	if(cnt == NULL) return ES_EINVAL;

	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETOWNEDTITLECNT,":i",&cnttitles);

	if(ret<0) return ret;
	*cnt = cnttitles;
	return ret;
}

s32 ES_GetOwnedTitles(u64 *titles, u32 cnt)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(cnt <= 0) return ES_EINVAL;
	if(!titles) return ES_EINVAL;
	if(!ISALIGNED(titles)) return ES_EALIGN;

	return IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETOWNEDTITLES,"i:d",cnt,titles,sizeof(u64)*cnt);
}

s32 ES_GetNumTitles(u32 *cnt)
{
	s32 ret;
	u32 cnttitles;

	if(__es_fd<0) return ES_ENOTINIT;
	if(cnt == NULL) return ES_EINVAL;

	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETTITLECNT,":i",&cnttitles);

	if(ret<0) return ret;
	*cnt = cnttitles;
	return ret;
}

s32 ES_GetTitles(u64 *titles, u32 cnt)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(cnt <= 0) return ES_EINVAL;
	if(!titles) return ES_EINVAL;
	if(!ISALIGNED(titles)) return ES_EALIGN;

	return IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETTITLES,"i:d",cnt,titles,sizeof(u64)*cnt);
}

s32 ES_GetNumStoredTMDContents(const signed_blob *stmd, u32 tmd_size, u32 *cnt)
{
	s32 ret;
	u32 cntct;

	if(__es_fd<0) return ES_ENOTINIT;
	if(!cnt) return ES_EINVAL;
	if(!stmd || !IS_VALID_SIGNATURE(stmd)) return ES_EINVAL;
	if(!ISALIGNED(stmd)) return ES_EALIGN;

	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETSTOREDCONTENTCNT,"d:i",stmd,tmd_size,&cntct);

	if(ret<0) return ret;
	*cnt = cntct;
	return ret;
}

s32 ES_GetStoredTMDContents(const signed_blob *stmd, u32 tmd_size, u32 *contents, u32 cnt)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(cnt <= 0) return ES_EINVAL;
	if(!contents) return ES_EINVAL;
	if(!stmd || !IS_VALID_SIGNATURE(stmd)) return ES_EINVAL;
	if(!ISALIGNED(stmd)) return ES_EALIGN;
	if(!ISALIGNED(contents)) return ES_EALIGN;
	if(tmd_size > MAX_SIGNED_TMD_SIZE) return ES_EINVAL;

	return IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETSTOREDCONTENTS,"di:d",stmd,tmd_size,cnt,contents,sizeof(u32)*cnt);
}

s32 ES_GetTitleContentsCount(u64 titleID, u32 *num)
{
	s32 ret;
	u32 _num;

	if(__es_fd<0) return ES_ENOTINIT;
	if(!num) return ES_EINVAL;

	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETTITLECONTENTSCNT,"q:i",titleID,&_num);

	if(ret<0) return ret;
	*num = _num;
	return ret;
}

s32 ES_GetTitleContents(u64 titleID, u8 *data, u32 size)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(size <= 0) return ES_EINVAL;
	if(!data) return ES_EINVAL;
	if(!ISALIGNED(data)) return ES_EALIGN;

	return IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETSTOREDTMD,"qi:d",titleID,size,data,size);
}

s32 ES_GetTMDViewSize(u64 titleID, u32 *size)
{
	s32 ret;
	u32 tmdsize;

	if(__es_fd<0) return ES_ENOTINIT;
	if(!size) return ES_EINVAL;

	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETTMDVIEWCNT,"q:i",titleID,&tmdsize);

	if(ret<0) return ret;
	*size = tmdsize;
	return ret;
}

s32 ES_GetTMDView(u64 titleID, u8 *data, u32 size)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(size <= 0) return ES_EINVAL;
	if(!data) return ES_EINVAL;
	if(!ISALIGNED(data)) return ES_EALIGN;

	return IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETTMDVIEWS,"qi:d",titleID,size,data,size);
}

s32 ES_GetStoredTMDSize(u64 titleID, u32 *size)
{
	s32 ret;
	u32 tmdsize;

	if(__es_fd<0) return ES_ENOTINIT;
	if(!size) return ES_EINVAL;

	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETSTOREDTMDSIZE,"q:i",titleID,&tmdsize);

	if(ret<0) return ret;
	*size = tmdsize;
	return ret;
}

s32 ES_GetStoredTMD(u64 titleID, signed_blob *stmd, u32 size)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(size <= 0) return ES_EINVAL;
	if(!stmd) return ES_EINVAL;
	if(!ISALIGNED(stmd)) return ES_EALIGN;
	if(size > MAX_SIGNED_TMD_SIZE) return ES_EINVAL;

	return IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETSTOREDTMD,"qi:d",titleID,size,stmd,size);
}

s32 ES_GetNumSharedContents(u32 *cnt)
{
	s32 ret;
	u32 cntct;

	if(__es_fd<0) return ES_ENOTINIT;
	if(!cnt) return ES_EINVAL;

	ret = IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETSHAREDCONTENTCNT,":i",&cntct);

	if(ret<0) return ret;
	*cnt = cntct;
	return ret;
}

s32 ES_GetSharedContents(sha1 *contents, u32 cnt)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(cnt <= 0) return ES_EINVAL;
	if(!contents) return ES_EINVAL;
	if(!ISALIGNED(contents)) return ES_EALIGN;

	return IOS_IoctlvFormat(__es_hid,__es_fd,IOCTL_ES_GETSHAREDCONTENTS,"i:d",cnt,contents,sizeof(sha1)*cnt);
}

signed_blob *ES_NextCert(const signed_blob *certs)
{
	cert_header *cert;
	if(!SIGNATURE_SIZE(certs)) return NULL;
	cert = SIGNATURE_PAYLOAD(certs);
	if(!CERTIFICATE_SIZE(cert)) return NULL;
	return (signed_blob*)(((u8*)cert) + CERTIFICATE_SIZE(cert));
}

s32 __ES_sanity_check_certlist(const signed_blob *certs, u32 certsize)
{
	int count = 0;
	signed_blob *end;

	if(!certs || !certsize) return 0;

	end = (signed_blob*)(((u8*)certs) + certsize);
	while(certs != end) {
		certs = ES_NextCert(certs);
		if(!certs) return 0;
		count++;
	}
	return count;
}

s32 ES_Identify(const signed_blob *certificates, u32 certificates_size, const signed_blob *stmd, u32 tmd_size, const signed_blob *sticket, u32 ticket_size, u32 *keyid)
{

	tmd *p_tmd;
	u8 *hashes;
	s32 ret;
	u32 *keyid_buf;

	if(__es_fd<0) return ES_ENOTINIT;

	if(ticket_size != STD_SIGNED_TIK_SIZE) return ES_EINVAL;
	if(!stmd || !tmd_size || !IS_VALID_SIGNATURE(stmd)) return ES_EINVAL;
	if(!sticket || !IS_VALID_SIGNATURE(sticket)) return ES_EINVAL;
	if(!__ES_sanity_check_certlist(certificates, certificates_size)) return ES_EINVAL;
	if(!ISALIGNED(certificates)) return ES_EALIGN;
	if(!ISALIGNED(stmd)) return ES_EALIGN;
	if(!ISALIGNED(sticket)) return ES_EALIGN;
	if(tmd_size > MAX_SIGNED_TMD_SIZE) return ES_EINVAL;

	p_tmd = SIGNATURE_PAYLOAD(stmd);

	if(!(keyid_buf = iosAlloc(__es_hid, 4))) return ES_ENOMEM;
	if(!(hashes = iosAlloc(__es_hid, p_tmd->num_contents*20))) {
		iosFree(__es_hid, keyid_buf);
		return ES_ENOMEM;
	}

	ret = IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_DIVERIFY, "dddd:id", certificates, certificates_size, 0, 0, sticket, ticket_size, stmd, tmd_size, keyid_buf, hashes, p_tmd->num_contents*20);
	if(ret >= 0 && keyid) *keyid = *keyid_buf;

	iosFree(__es_hid, keyid_buf);
	iosFree(__es_hid, hashes);

	if(ret >= 0) {
		__ES_Close();
		__ES_Init();
	}

	return ret;
}

s32 ES_AddTicket(const signed_blob *stik, u32 stik_size, const signed_blob *certificates, u32 certificates_size, const signed_blob *crl, u32 crl_size)
{
	s32 ret;

	if(__es_fd<0) return ES_ENOTINIT;
	if(stik_size != STD_SIGNED_TIK_SIZE) return ES_EINVAL;
	if(!stik || !IS_VALID_SIGNATURE(stik)) return ES_EINVAL;
	if(crl_size && (!crl || !IS_VALID_SIGNATURE(crl))) return ES_EINVAL;
	if(!__ES_sanity_check_certlist(certificates, certificates_size)) return ES_EINVAL;
	if(!certificates || !ISALIGNED(certificates)) return ES_EALIGN;
	if(!ISALIGNED(stik)) return ES_EALIGN;
	if(!ISALIGNED(certificates)) return ES_EALIGN;
	if(!ISALIGNED(crl)) return ES_EALIGN;

	if(!crl_size) crl=NULL;

	ret = IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_ADDTICKET, "ddd:", stik, stik_size, certificates, certificates_size, crl, crl_size);
	return ret;

}

s32 ES_DeleteTicket(const tikview *view)
{
	s32 ret;

	if(__es_fd<0) return ES_ENOTINIT;
	if(!view) return ES_EINVAL;
	if(!ISALIGNED(view)) return ES_EALIGN;
	ret = IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_DELETETICKET, "d:", view, sizeof(tikview));
	return ret;
}

s32 ES_AddTitleTMD(const signed_blob *stmd, u32 stmd_size)
{
	s32 ret;

	if(__es_fd<0) return ES_ENOTINIT;
	if(!stmd || !IS_VALID_SIGNATURE(stmd)) return ES_EINVAL;
	if(stmd_size != SIGNED_TMD_SIZE(stmd)) return ES_EINVAL;
	if(!ISALIGNED(stmd)) return ES_EALIGN;

	ret = IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_ADDTMD, "d:", stmd, stmd_size);
	return ret;

}

s32 ES_AddTitleStart(const signed_blob *stmd, u32 tmd_size, const signed_blob *certificates, u32 certificates_size, const signed_blob *crl, u32 crl_size)
{
	s32 ret;

	if(__es_fd<0) return ES_ENOTINIT;
	if(!stmd || !IS_VALID_SIGNATURE(stmd)) return ES_EINVAL;
	if(tmd_size != SIGNED_TMD_SIZE(stmd)) return ES_EINVAL;
	if(crl_size && (!crl || !IS_VALID_SIGNATURE(crl))) return ES_EINVAL;
	if(!__ES_sanity_check_certlist(certificates, certificates_size)) return ES_EINVAL;
	if(!certificates || !ISALIGNED(certificates)) return ES_EALIGN;
	if(!ISALIGNED(stmd)) return ES_EALIGN;
	if(!ISALIGNED(certificates)) return ES_EALIGN;
	if(!ISALIGNED(crl)) return ES_EALIGN;

	if(!crl_size) crl=NULL;

	ret = IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_ADDTITLESTART, "dddi:", stmd, tmd_size, certificates, certificates_size, crl, crl_size, 1);
	return ret;
}

s32 ES_AddContentStart(u64 titleID, u32 cid)
{
	if(__es_fd<0) return ES_ENOTINIT;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_ADDCONTENTSTART, "qi:", titleID, cid);
}

s32 ES_AddContentData(s32 cfd, u8 *data, u32 data_size)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(cfd<0) return ES_EINVAL;
	if(!data || !data_size) return ES_EINVAL;
	if(!ISALIGNED(data)) return ES_EALIGN;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_ADDCONTENTDATA, "id:", cfd, data, data_size);
}

s32 ES_AddContentFinish(u32 cid)
{
	if(__es_fd<0) return ES_ENOTINIT;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_ADDCONTENTFINISH, "i:", cid);
}

s32 ES_AddTitleFinish(void)
{
	if(__es_fd<0) return ES_ENOTINIT;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_ADDTITLEFINISH, "");
}

s32 ES_AddTitleCancel(void)
{
	if(__es_fd<0) return ES_ENOTINIT;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_ADDTITLECANCEL, "");
}

s32 ES_ImportBoot(const signed_blob *tik, u32 tik_size,const signed_blob *tik_certs,u32 tik_certs_size,const signed_blob *tmd,u32 tmd_size,const signed_blob *tmd_certs,u32 tmd_certs_size,const u8 *content,u32 content_size)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(!tik || !tik_size) return ES_EINVAL;
	if(!tik_certs || !tik_certs_size) return ES_EINVAL;
	if(!tmd || !tmd_size) return ES_EINVAL;
	if(!tmd_certs || !tmd_certs_size) return ES_EINVAL;
	if(!content || !content_size) return ES_EINVAL;
	if(!ISALIGNED(tik)) return ES_EALIGN;
	if(!ISALIGNED(tmd)) return ES_EALIGN;
	if(!ISALIGNED(tik_certs)) return ES_EALIGN;
	if(!ISALIGNED(tmd_certs)) return ES_EALIGN;
	if(!ISALIGNED(content)) return ES_EALIGN;

	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_IMPORTBOOT, "dddddd:", tik, tik_size, tik_certs, tik_certs_size, tmd, tmd_size, tmd_certs, tmd_certs_size, NULL, 0, content, content_size);
}

s32 ES_OpenContent(u16 index)
{
	if(__es_fd<0) return ES_ENOTINIT;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_OPENCONTENT, "i:", index);
}

s32 ES_OpenTitleContent(u64 titleID, tikview *views, u16 index)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(!ISALIGNED(views)) return ES_EALIGN;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_OPENTITLECONTENT, "qdi:", titleID, views, sizeof(tikview), index);
}

s32 ES_ReadContent(s32 cfd, u8 *data, u32 data_size)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(cfd<0) return ES_EINVAL;
	if(!data || !data_size) return ES_EINVAL;
	if(!ISALIGNED(data)) return ES_EALIGN;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_READCONTENT, "i:d", cfd, data, data_size);
}

s32 ES_SeekContent(s32 cfd, s32 where, s32 whence)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(cfd<0) return ES_EINVAL;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_SEEKCONTENT, "iii:", cfd, where, whence);
}

s32 ES_CloseContent(s32 cfd)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(cfd<0) return ES_EINVAL;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_CLOSECONTENT, "i:", cfd);
}

s32 ES_DeleteTitle(u64 titleID)
{
	if(__es_fd<0) return ES_ENOTINIT;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_DELETETITLE, "q:", titleID);
}

s32 ES_DeleteTitleContent(u64 titleID)
{
	if(__es_fd<0) return ES_ENOTINIT;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_DELETETITLECONTENT, "q:", titleID);
}

s32 ES_Encrypt(u32 keynum, u8 *iv, u8 *source, u32 size, u8 *dest)
{
	if(__es_fd<0) return ES_ENOTINIT;
 	if(!iv || !source || !size || !dest) return ES_EINVAL;
 	if(!ISALIGNED(source) || !ISALIGNED(iv) || !ISALIGNED(dest)) return ES_EALIGN;
 	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_ENCRYPT, "idd:dd", keynum, iv, 0x10, source, size, iv, 0x10, dest, size);
}

s32 ES_Decrypt(u32 keynum, u8 *iv, u8 *source, u32 size, u8 *dest)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(!iv || !source || !size || !dest) return ES_EINVAL;
	if(!ISALIGNED(source) || !ISALIGNED(iv) || !ISALIGNED(dest)) return ES_EALIGN;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_DECRYPT, "idd:dd", keynum, iv, 0x10, source, size, iv, 0x10, dest, size);
}

s32 ES_Sign(u8 *source, u32 size, u8 *sig, u8 *certs)
{
	if(__es_fd<0) return ES_ENOTINIT;
 	if(!source || !size || !sig || !certs) return ES_EINVAL;
 	if(!ISALIGNED(source) || !ISALIGNED(sig) || !ISALIGNED(certs)) return ES_EALIGN;
 	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_SIGN, "d:dd", source, size, sig, 0x3C, certs, 0x180);
}

s32 ES_GetDeviceCert(u8 *outbuf)
{
	if(__es_fd<0) return ES_ENOTINIT;
	if(!outbuf) return ES_EINVAL;
	if(!ISALIGNED(outbuf)) return ES_EALIGN;
	return IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_GETDEVICECERT, ":d", outbuf, 0x180);
}

s32 ES_GetDeviceID(u32 *device_id)
{
	s32 ret;
	u32 _device_id = 0;

	if(__es_fd<0) return ES_ENOTINIT;
	if(!device_id) return ES_EINVAL;

	ret = IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_GETDEVICEID, ":i", &_device_id);
	if (ret>=0) *device_id = _device_id;

	return ret;
}

s32 ES_GetBoot2Version(u32 *version)
{
	s32 ret;
	u32 _version = 0;

	if(__es_fd<0) return ES_ENOTINIT;
	if(!version) return ES_EINVAL;

	ret = IOS_IoctlvFormat(__es_hid, __es_fd, IOCTL_ES_GETBOOT2VERSION, ":i", &_version);
	if (ret>=0) *version = _version;

	return ret;
}

// 64k buffer size for alignment
#define ES_READ_BUF_SIZE 65536

typedef struct {
	s32 cfd;
	u64 titleID;
	tmd_content content;
	void *iobuf;
	mutex_t mutex;
} es_fd;

// valid path formats:
// format            example                          description
// es:%08x           es:00000004                      Content index for current title ID
// es:ID%08x         es:ID00000033                    Content ID for current title ID
// es:%016llx/%08x   es:0000000100000002/00000004     Content index for some title ID (not fully implemented yet)
// es:%016llx/ID%08x es:0000000100000002/ID00000033   Content ID for some title ID (not fully implemented yet)
// leading zeroes may be omitted, 0x prefix allowed. All numbers in hex.

static s32 _ES_decodepath (struct _reent *r, const char *path, u64 *titleID, tmd_content *content)
{
	u64 _tid = 0;
	u32 tmd_size;
	STACK_ALIGN(u8, _tmdbuf, MAX_SIGNED_TMD_SIZE, 32);
	signed_blob *_stmd;
	tmd *_tmd;
	tmd_content *_contents;
	tmd_content _content;
	int is_cid;
	u32 arg;
	char *bad;
	int i;
	u64 mytid;

	// check the device
	if(strncmp(path,"es:",3)) {
		r->_errno = EINVAL;
		return -1;
	}
	path += 3;

	// Get our Title ID
	if(ES_GetTitleID(&mytid) < 0) {
		r->_errno = EIO;
		return -1;
	}

	// Read in Title ID, if this is an absolute path
	if(path[0] == '/') {
		path++;
		if(!path[0]) {
			r->_errno = EINVAL;
			return -1;
		}
		r->_errno = 0;
		_tid = _strtoull_r(r, path, &bad, 16);
		if(r->_errno) return -1;
		if((bad == path) || (bad[0] != '/')) {
			r->_errno = EINVAL;
			return -1;
		}
		path = bad + 1;
	} else {
		_tid = mytid;
	}

	// check if path is a content ID
	if(!strncmp(path,"ID",2)) {
		path += 2;
		is_cid = 1;
	} else {
		is_cid = 0;
	}

	// read in the argument (content ID or content index)
	r->_errno = 0;
	arg = _strtoul_r(r, path, &bad, 16);
	if(r->_errno) return -1;
	if((bad == path) || (bad[0] != 0)) {
		r->_errno = EINVAL;
		return -1;
	}

	// now get the TMD and find the content entry
	if(ES_GetStoredTMDSize(_tid, &tmd_size) < 0) {
		r->_errno = ENOENT;
		return -1;
	}

	_stmd = (signed_blob*)_tmdbuf;
	if(ES_GetStoredTMD(_tid, _stmd, tmd_size) < 0) {
		r->_errno = EIO;
		return -1;
	}
	if(!IS_VALID_SIGNATURE(_stmd)) {
		r->_errno = EIO;
		return -1;
	}
	_tmd = SIGNATURE_PAYLOAD(_stmd);
	_contents = TMD_CONTENTS(_tmd);

	for(i=0;i<_tmd->num_contents;i++) {
		if(is_cid) {
			if(_contents[i].cid == arg) {
				_content = _contents[i];
				break;
			}
		} else {
			if(_contents[i].index == arg) {
				_content = _contents[i];
				break;
			}
		}
	}
	if(i >= _tmd->num_contents) {
		r->_errno = ENOENT;
		return -1;
	}

	if(titleID) {
		if(_tid == mytid) *titleID = 0;
		else *titleID = _tid;
	}
	if(content) *content = _content;
	return 0;
}

static int _ES_open_r (struct _reent *r, void *fileStruct, const char *path, int flags, int mode) {
	es_fd *file = (es_fd *) fileStruct;

	// decode the path
	if(_ES_decodepath(r, path, &file->titleID, &file->content) < 0) {
		return -1;
	}

	// writing not supported
	if ((flags & 0x03) != O_RDONLY) {
		r->_errno = EROFS;
		return -1;
	}

	// open the content
	if(file->titleID == 0)
		file->cfd = ES_OpenContent(file->content.index);
	else
	{
		u32 cnt ATTRIBUTE_ALIGN(32);
		ES_GetNumTicketViews(file->titleID, &cnt);
		tikview *views = (tikview *)memalign( 32, sizeof(tikview)*cnt );
		if(views == NULL)
		{
			return -1;
		}
		ES_GetTicketViews(file->titleID, views, cnt);
		file->cfd = ES_OpenTitleContent(file->titleID, views, file->content.index);
		free(views);
	}

	if(file->cfd<0) {
		r->_errno = EIO;
		return -1;
	}

	file->iobuf = NULL;

	LWP_MutexInit(&file->mutex, false);

	return (int)file;
}

static int _ES_close_r (struct _reent *r, void *fd) {
	es_fd *file = (es_fd *) fd;

	LWP_MutexLock(file->mutex);

	if(ES_CloseContent(file->cfd) < 0) {
		r->_errno = EBADF;
		return -1;
	}
	file->cfd = -1;

	if(file->iobuf) _free_r(r,file->iobuf);

	LWP_MutexUnlock(file->mutex);
	LWP_MutexDestroy(file->mutex);
	return 0;
}

static int _ES_read_r (struct _reent *r, void *fd, char *ptr, size_t len) {
	es_fd *file = (es_fd *) fd;
	int read = 0;
	int res;

	LWP_MutexLock(file->mutex);
	if(file->cfd < 0) {
		LWP_MutexUnlock(file->mutex);
		r->_errno = EBADF;
		return -1;
	}

	// if aligned, just pass through the read
	if(ISALIGNED(ptr))
	{
		res = ES_ReadContent(file->cfd, (u8*)ptr, len);
		if(res < 0) {
			LWP_MutexUnlock(file->mutex);
			// we don't really know what the error codes mean...
			r->_errno = EIO;
			return -1;
		}
		read = res;
	// otherwise read in blocks to an aligned buffer
	} else {
		int chunk;
		if(!file->iobuf) {
			file->iobuf = _memalign_r(r, 32, ES_READ_BUF_SIZE);
			if(!file->iobuf) {
				r->_errno = ENOMEM;
				return -1;
			}
		}
		while(len) {
			if(len > ES_READ_BUF_SIZE) chunk = ES_READ_BUF_SIZE;
			else chunk = len;
			res = ES_ReadContent(file->cfd, file->iobuf, chunk);
			if(res < 0) {
				LWP_MutexUnlock(file->mutex);
				// we don't really know what the error codes mean...
				r->_errno = EIO;
				return -1;
			}
			len -= res;
			read += res;
			memcpy(ptr, file->iobuf, res);
			ptr += res;
			if(res < chunk) break;
		}
	}

	LWP_MutexUnlock(file->mutex);
	return read;
}

static off_t _ES_seek_r (struct _reent *r, void *fd, off_t where, int whence) {
	es_fd *file = (es_fd *) fd;
	s32 res;

	LWP_MutexLock(file->mutex);
	if(file->cfd < 0) {
		LWP_MutexUnlock(file->mutex);
		r->_errno = EBADF;
		return -1;
	}

	res = ES_SeekContent(file->cfd, where, whence);
	LWP_MutexUnlock(file->mutex);

	if(res < 0) {
		r->_errno = EINVAL;
		return -1;
	}
	return res;
}

static void _ES_fillstat(u64 titleID, tmd_content *content, struct stat *st) {
	memset(st, 0, sizeof(*st));
	// the inode is the content ID
	// (pretend each Title ID is a different filesystem)
	st->st_ino = content->cid;
	// st_dev is only a short, so do the best we can and use the middle two letters
	// of the title ID if it's not a system content, otherwise use the low 16 bits
	if((titleID>>32) == 1)
		st->st_dev = titleID & 0xFFFF;
	else
		st->st_dev = (titleID>>8) & 0xFFFF;

	// not necessarily true due to shared contents, but
	// we're not going to implement shared content scan here and now
	st->st_nlink = 1;
	// instead, give the file group read permissions if it's a shared content
	st->st_mode = S_IFREG | S_IRUSR;
	if(content->type & 0x8000)
		st->st_mode |= S_IRGRP;

	// use st_dev as a uid too, see above
	st->st_uid = st->st_dev;
	// no group
	st->st_gid = 0;
	// content size
	st->st_size = content->size;
	st->st_blocks = (st->st_size + 511) / 512;
	// NAND fs cluster size (not like anyone cares, but...)
	st->st_blksize = 16384;
}

static int _ES_fstat_r (struct _reent *r, void *fd, struct stat *st) {
	es_fd *file = (es_fd *) fd;

	LWP_MutexLock(file->mutex);
	if(file->cfd < 0) {
		LWP_MutexUnlock(file->mutex);
		r->_errno = EBADF;
		return -1;
	}

	_ES_fillstat(file->titleID, &file->content, st);
	LWP_MutexUnlock(file->mutex);

	return 0;
}

static int _ES_stat_r (struct _reent *r, const char *path, struct stat *st) {
	tmd_content content;
	u64 titleID;

	if(_ES_decodepath(r, path, &titleID, &content) < 0) {
		return -1;
	}
	_ES_fillstat(titleID, &content, st);
	return 0;
}

static const devoptab_t dotab_es = {
	"es",
	sizeof (es_fd),
	_ES_open_r,
	_ES_close_r,
	NULL,
	_ES_read_r,
	_ES_seek_r,
	_ES_fstat_r,
	_ES_stat_r,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static void __ES_InitFS(void) {
	AddDevice(&dotab_es);
}

static void __ES_DeinitFS(void) {
	RemoveDevice("es:");
}

#endif /* defined(HW_RVL) */
