/*-------------------------------------------------------------

usb.c -- USB lowlevel

Copyright (C) 2008
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)
tueidj

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

/*  Note: There are 3 types of USB interfaces here, the early ones
 *  (V0: /dev/usb/oh0 and /dev/usb/oh1) and two later ones (V5: /dev/usb/ven
 *  and /dev/usb/hid) which are similar but have some small
 *  differences. There is also an earlier version of /dev/usb/hid (V4)
 *  found in IOSes 37,61,56,etc. and /dev/usb/msc found in IOS 57.
 *  These interfaces aren't implemented here and you may find some
 *  devices don't show up if you're running under those IOSes.
 */

#if defined(HW_RVL)
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <gcutil.h>
#include <ogcsys.h>
#include <ipc.h>
#include <asm.h>
#include <processor.h>

#include "usb.h"

#define USB_HEAPSIZE                         16384

#define USBV0_IOCTL_CTRLMSG                      0
#define USBV0_IOCTL_BLKMSG                       1
#define USBV0_IOCTL_INTRMSG                      2
#define USBV0_IOCTL_SUSPENDDEV                   5
#define USBV0_IOCTL_RESUMEDEV                    6
#define USBV0_IOCTL_ISOMSG                       9
#define USBV0_IOCTL_GETDEVLIST                  12
#define USBV0_IOCTL_DEVREMOVALHOOK              26
#define USBV0_IOCTL_DEVINSERTHOOK               27
#define USBV0_IOCTL_DEVICECLASSCHANGE           28

#define USBV4_IOCTL_GETVERSION                   6 // returns 0x40001

#define USBV5_IOCTL_GETVERSION                   0 // should return 0x50001
#define USBV5_IOCTL_GETDEVICECHANGE              1
#define USBV5_IOCTL_SHUTDOWN                     2
#define USBV5_IOCTL_GETDEVPARAMS                 3
#define USBV5_IOCTL_ATTACHFINISH                 6
#define USBV5_IOCTL_SETALTERNATE                 7
#define USBV5_IOCTL_SUSPEND_RESUME              16
#define USBV5_IOCTL_CANCELENDPOINT              17
#define USBV5_IOCTL_CTRLMSG                     18
#define USBV5_IOCTL_INTRMSG                     19
#define USBV5_IOCTL_ISOMSG                      20
#define USBV5_IOCTL_BULKMSG                     21
#define USBV5_IOCTL_MSC_READWRITE_ASYNC         32 /* unimplemented */
#define USBV5_IOCTL_MSC_READ_ASYNC              33 /* unimplemented */
#define USBV5_IOCTL_MSC_WRITE_ASYNC             34 /* unimplemented */
#define USBV5_IOCTL_MSC_READWRITE               35 /* unimplemented */
#define USBV5_IOCTL_MSC_RESET                   36 /* unimplemented */

#define USB_MAX_DEVICES                         32

static s32 hId = -1;
static const char __oh0_path[] ATTRIBUTE_ALIGN(32) = "/dev/usb/oh0";
static const char __ven_path[] ATTRIBUTE_ALIGN(32) = "/dev/usb/ven";
static const char __hid_path[] ATTRIBUTE_ALIGN(32) = "/dev/usb/hid";

typedef struct _usb_cb_list {
	usbcallback cb;
	void *userdata;
	union {
		s32 device_id;
		struct _usb_cb_list *next;
	};
} _usb_cb_list;

struct _usbv5_host {
	usb_device_entry attached_devices[USB_MAX_DEVICES];
	_usb_cb_list remove_cb[USB_MAX_DEVICES];
	s32 fd;
	_usb_cb_list *device_change_notify;
};

static struct _usbv5_host* ven_host = NULL;
static struct _usbv5_host* hid_host = NULL;

struct _usb_msg {
	s32 fd;
	u32 heap_buffers;
	union {
		struct {
			u8 bmRequestType;
			u8 bmRequest;
			u16 wValue;
			u16 wIndex;
			u16 wLength;
			void *rpData;
		} ctrl;

		struct {
			void *rpData;
			u16 wLength;
			u8 pad[4];
			u8 bEndpoint;
		} bulk;

		struct {
			void *rpData;
			u16 wLength;
			u8 bEndpoint;
		} intr;

		struct {
			void *rpData;
			void *rpPacketSizes;
			u8 bPackets;
			u8 bEndpoint;
		} iso;

		struct {
			u16 pid;
			u16 vid;
		} notify;

		u8 class;
		u32 hid_intr_dir;

		u32 align_pad[4]; // pad to 24 bytes
	};
	usbcallback cb;
	void *userdata;
	ioctlv vec[7];
};

static s32 __usbv5_devicechangeCB(s32 result, void *p);

static s32 __usbv5_attachfinishCB(s32 result, void *p)
{
	_usb_cb_list *list;
	struct _usbv5_host* host = (struct _usbv5_host*)p;
	if(host==NULL) return IPC_EINVAL;

	/* the callback functions may attempt to set a new notify func,
	 * device_change_notify is set to NULL *before* calling it to
	 * avoid wiping out the new functions
	 */
	list = host->device_change_notify;
	host->device_change_notify = NULL;
	while (list) {
		_usb_cb_list *next = list->next;
		list->cb(result, list->userdata);
		iosFree(hId, list);
		list = next;
	}

	if (result==0)
		IOS_IoctlAsync(host->fd, USBV5_IOCTL_GETDEVICECHANGE, NULL, 0, host->attached_devices, 0x180, __usbv5_devicechangeCB, host);

	return IPC_OK;
}

static s32 __usbv5_devicechangeCB(s32 result, void *p)
{
	int i, j;
	struct _usbv5_host* host = (struct _usbv5_host*)p;

	if(host==NULL) return IPC_EINVAL;

	if (result>=0) {
		// can't check the remove callbacks only if the number of devices has decreased,
		// because devices may have been inserted as well as removed
		for (i=0; i<USB_MAX_DEVICES; i++) {
			if (host->remove_cb[i].cb==NULL)
				continue;
			for (j=0; j<result; j++) {
				if (host->remove_cb[i].device_id == host->attached_devices[j].device_id)
					break;
			}

			if (j==result) { // execute callback and remove it
				host->remove_cb[i].cb(0, host->remove_cb[i].userdata);
				host->remove_cb[i].cb = NULL;
			}
		}
		// wipe unused device entries
		memset(host->attached_devices+result, 0, sizeof(usb_device_entry)*(32-result));

		IOS_IoctlAsync(host->fd, USBV5_IOCTL_ATTACHFINISH, NULL, 0, NULL, 0, __usbv5_attachfinishCB, host);
	}

	return IPC_OK;
}

static s32 add_devicechange_cb(_usb_cb_list **list, usbcallback cb, void *userdata)
{
	_usb_cb_list *new_cb = (_usb_cb_list*)iosAlloc(hId, sizeof(_usb_cb_list));
	if (new_cb==NULL)
		return IPC_ENOMEM;

	new_cb->cb = cb;
	new_cb->userdata = userdata;
	new_cb->next = *list;
	*list = new_cb;

	return IPC_OK;
}

static s32 __usbv5_messageCB(s32 result,void *_msg)
{
	struct _usb_msg *msg = (struct _usb_msg*)_msg;

	if(msg==NULL) return IPC_EINVAL;

	if(msg->cb!=NULL) msg->cb(result, msg->userdata);

	iosFree(hId,msg);

	return IPC_OK;
}

static s32 __usbv0_messageCB(s32 result,void *usrdata)
{
	u32 i;
	struct _usb_msg *msg = (struct _usb_msg*)usrdata;

	if(msg==NULL) return IPC_EINVAL;

	if(msg->cb!=NULL) msg->cb(result, msg->userdata);

	for(i=0; i<msg->heap_buffers; i++) {
		if(msg->vec[i].data!=NULL)
			iosFree(hId,msg->vec[i].data);
	}

	iosFree(hId,msg);

	return IPC_OK;
}

static s32 __find_device_on_host(struct _usbv5_host *host, s32 device_id)
{
	int i;
	if (host==NULL) return -1;

	for (i=0; host->attached_devices[i].device_id; i++) {
		if (host->attached_devices[i].device_id == device_id)
			return i;
	}

	return -1;
}

static s32 __usb_isochronous_message(s32 device_id,u8 bEndpoint,u8 bPackets,u16* rpPacketSizes,void* rpData,usbcallback cb,void *userdata)
{
	s32 ret = IPC_ENOMEM;
	struct _usb_msg *msg;
	u16 wLength=0;
	u8 i;

	for (i=0; i<bPackets; i++)
		wLength += rpPacketSizes[i];

	if(wLength==0) return IPC_EINVAL;
	if(((u32)rpData%32)!=0) return IPC_EINVAL;
	if(((u32)rpPacketSizes%32)!=0) return IPC_EINVAL;
	if(bPackets==0) return IPC_EINVAL;
	if(rpPacketSizes==NULL || rpData==NULL) return IPC_EINVAL;

	msg = (struct _usb_msg*)iosAlloc(hId,sizeof(struct _usb_msg));
	if(msg==NULL) return IPC_ENOMEM;

	memset(msg, 0, sizeof(struct _usb_msg));

	msg->fd = device_id;
	msg->cb = cb;
	msg->userdata = userdata;

	if (device_id>=0 && device_id<0x20) {
		u8 *pEndp=NULL;
		u16 *pLength=NULL;
		u8 *pPackets=NULL;

		pEndp = (u8*)iosAlloc(hId,32);
		if(pEndp==NULL) goto done;
		*pEndp = bEndpoint;

		pLength = (u16*)iosAlloc(hId,32);
		if(pLength==NULL) goto done;
		// NOT byteswapped!
		*pLength = wLength;

		pPackets = (u8*)iosAlloc(hId,32);
		if(pPackets==NULL) goto done;
		*pPackets = bPackets;

		msg->heap_buffers = 3;

		msg->vec[0].data = pEndp;
		msg->vec[0].len = sizeof(u8);
		msg->vec[1].data = pLength;
		msg->vec[1].len = sizeof(u16);
		msg->vec[2].data = pPackets;
		msg->vec[2].len = sizeof(u8);
		msg->vec[3].data = rpPacketSizes;
		msg->vec[3].len = sizeof(u16)*bPackets;
		msg->vec[4].data = rpData;
		msg->vec[4].len = wLength;

		if (cb==NULL)
			ret = IOS_Ioctlv(device_id, USBV0_IOCTL_ISOMSG, 3, 2, msg->vec);
		else
			return IOS_IoctlvAsync(device_id, USBV0_IOCTL_ISOMSG, 3, 2, msg->vec, __usbv0_messageCB, msg);

done:
		if(pEndp) iosFree(hId,pEndp);
		if(pLength) iosFree(hId,pLength);
		if(pPackets) iosFree(hId,pPackets);
	} else {
		u8 endpoint_dir = !!(bEndpoint&USB_ENDPOINT_IN);
		s32 fd = (device_id<0) ? ven_host->fd : hid_host->fd;

		msg->iso.rpData = rpData;
		msg->iso.rpPacketSizes = rpPacketSizes;
		msg->iso.bEndpoint = bEndpoint;
		msg->iso.bPackets = bPackets;

		msg->vec[0].data = msg;
		msg->vec[0].len = 64;
		// block counts are used for both input and output
		msg->vec[1].data = msg->vec[3].data = rpPacketSizes;
		msg->vec[1].len = msg->vec[3].len = sizeof(u16)*bPackets;
		msg->vec[2].data = rpData;
		msg->vec[2].len = wLength;

		if (cb==NULL)
			ret = IOS_Ioctlv(fd, USBV5_IOCTL_ISOMSG, 2-endpoint_dir, 2+endpoint_dir, msg->vec);
		else
			return IOS_IoctlvAsync(fd, USBV5_IOCTL_ISOMSG, 2-endpoint_dir, 2+endpoint_dir, msg->vec, __usbv5_messageCB, msg);
	}

	if (msg!=NULL) iosFree(hId,msg);

	return ret;
}

static s32 __usb_control_message(s32 device_id,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData,usbcallback cb,void *userdata)
{
	s32 ret = IPC_ENOMEM;
	struct _usb_msg *msg;

	if(((s32)rpData%32)!=0) return IPC_EINVAL;
	if(wLength && !rpData) return IPC_EINVAL;
	if(!wLength && rpData) return IPC_EINVAL;

	msg = (struct _usb_msg*)iosAlloc(hId,sizeof(struct _usb_msg));
	if(msg==NULL) return IPC_ENOMEM;

	memset(msg, 0, sizeof(struct _usb_msg));

	msg->fd = device_id;
	msg->cb = cb;
	msg->userdata = userdata;

	if (device_id>=0 && device_id<0x20) {
		u8 *pRqType = NULL,*pRq = NULL,*pNull = NULL;
		u16 *pValue = NULL,*pIndex = NULL,*pLength = NULL;

		pRqType = (u8*)iosAlloc(hId,32);
		if(pRqType==NULL) goto done;
		*pRqType  = bmRequestType;

		pRq = (u8*)iosAlloc(hId,32);
		if(pRq==NULL) goto done;
		*pRq = bmRequest;

		pValue = (u16*)iosAlloc(hId,32);
		if(pValue==NULL) goto done;
		*pValue = bswap16(wValue);

		pIndex = (u16*)iosAlloc(hId,32);
		if(pIndex==NULL) goto done;
		*pIndex = bswap16(wIndex);

		pLength = (u16*)iosAlloc(hId,32);
		if(pLength==NULL) goto done;
		*pLength = bswap16(wLength);

		pNull = (u8*)iosAlloc(hId,32);
		if(pNull==NULL) goto done;
		*pNull = 0;

		msg->heap_buffers = 6;

		msg->vec[0].data = pRqType;
		msg->vec[0].len = sizeof(u8);
		msg->vec[1].data = pRq;
		msg->vec[1].len = sizeof(u8);
		msg->vec[2].data = pValue;
		msg->vec[2].len = sizeof(u16);
		msg->vec[3].data = pIndex;
		msg->vec[3].len = sizeof(u16);
		msg->vec[4].data = pLength;
		msg->vec[4].len = sizeof(u16);
		msg->vec[5].data = pNull;
		msg->vec[5].len = sizeof(u8);
		msg->vec[6].data = rpData;
		msg->vec[6].len = wLength;

		if (cb==NULL)
			ret = IOS_Ioctlv(device_id, USBV0_IOCTL_CTRLMSG, 6, 1, msg->vec);
		else
			return IOS_IoctlvAsync(device_id, USBV0_IOCTL_CTRLMSG, 6, 1, msg->vec, __usbv0_messageCB, msg);

done:
		if(pRqType!=NULL) iosFree(hId,pRqType);
		if(pRq!=NULL) iosFree(hId,pRq);
		if(pValue!=NULL) iosFree(hId,pValue);
		if(pIndex!=NULL) iosFree(hId,pIndex);
		if(pLength!=NULL) iosFree(hId,pLength);
		if(pNull!=NULL) iosFree(hId,pNull);

	} else {
		u8 request_dir = !!(bmRequestType&USB_CTRLTYPE_DIR_DEVICE2HOST);
		s32 fd = (device_id<0) ? ven_host->fd : hid_host->fd;

		msg->ctrl.bmRequestType = bmRequestType;
		msg->ctrl.bmRequest = bmRequest;
		msg->ctrl.wValue = wValue;
		msg->ctrl.wIndex = wIndex;
		msg->ctrl.wLength = wLength;
		msg->ctrl.rpData = rpData;

		msg->vec[0].data = msg;
		msg->vec[0].len = 64;
		msg->vec[1].data = rpData;
		msg->vec[1].len = wLength;

		if (cb==NULL)
			ret = IOS_Ioctlv(fd, USBV5_IOCTL_CTRLMSG, 2-request_dir, request_dir, msg->vec);
		else
			return IOS_IoctlvAsync(fd, USBV5_IOCTL_CTRLMSG, 2-request_dir, request_dir, msg->vec, __usbv5_messageCB, msg);
	}

	if(msg!=NULL) iosFree(hId,msg);

	return ret;
}

static inline s32 __usb_interrupt_bulk_message(s32 device_id,u8 ioctl,u8 bEndpoint,u16 wLength,void *rpData,usbcallback cb,void *userdata)
{
	s32 ret = IPC_ENOMEM;
	struct _usb_msg *msg;

	if(((s32)rpData%32)!=0) return IPC_EINVAL;
	if(wLength && !rpData) return IPC_EINVAL;
	if(!wLength && rpData) return IPC_EINVAL;

	msg = (struct _usb_msg*)iosAlloc(hId,sizeof(struct _usb_msg));
	if(msg==NULL) return IPC_ENOMEM;

	memset(msg, 0, sizeof(struct _usb_msg));

	msg->fd = device_id;
	msg->cb = cb;
	msg->userdata = userdata;

	if (device_id>=0 && device_id<0x20) {
		u8 *pEndP = NULL;
		u16 *pLength = NULL;

		pEndP = (u8*)iosAlloc(hId,32);
		if(pEndP==NULL) goto done;
		*pEndP = bEndpoint;

		pLength = (u16*)iosAlloc(hId,32);
		if(pLength==NULL) goto done;
		*pLength = wLength;

		msg->vec[0].data = pEndP;
		msg->vec[0].len = sizeof(u8);
		msg->vec[1].data = pLength;
		msg->vec[1].len = sizeof(u16);
		msg->vec[2].data = rpData;
		msg->vec[2].len = wLength;

		msg->heap_buffers = 2;

		if (cb==NULL)
			ret = IOS_Ioctlv(device_id,ioctl,2,1,msg->vec);
		else
			return IOS_IoctlvAsync(device_id,ioctl,2,1,msg->vec,__usbv0_messageCB,msg);

done:
		if(pEndP!=NULL) iosFree(hId,pEndP);
		if(pLength!=NULL) iosFree(hId,pLength);

	} else {
		u8 endpoint_dir = !!(bEndpoint&USB_ENDPOINT_IN);
		s32 fd = (device_id<0) ? ven_host->fd : hid_host->fd;

		if (ioctl == USBV0_IOCTL_INTRMSG) {
			// HID does this a little bit differently
			if (device_id>=0)
				msg->hid_intr_dir = !endpoint_dir;
			else {
				msg->intr.rpData = rpData;
				msg->intr.wLength = wLength;
				msg->intr.bEndpoint = bEndpoint;
			}
			ioctl = USBV5_IOCTL_INTRMSG;
		} else {
			msg->bulk.rpData = rpData;
			msg->bulk.wLength = wLength;
			msg->bulk.bEndpoint = bEndpoint;
			ioctl = USBV5_IOCTL_BULKMSG;
		}

		msg->vec[0].data = msg;
		msg->vec[0].len = 64;
		msg->vec[1].data = rpData;
		msg->vec[1].len = wLength;

		if (cb==NULL)
			ret = IOS_Ioctlv(fd, ioctl, 2-endpoint_dir, endpoint_dir, msg->vec);
		else
			return IOS_IoctlvAsync(fd, ioctl, 2-endpoint_dir, endpoint_dir, msg->vec, __usbv5_messageCB, msg);
	}

	if(msg!=NULL) iosFree(hId,msg);

	return ret;
}

static inline s32 __usb_getdesc(s32 fd, u8 *buffer, u8 valuehi, u8 valuelo, u16 index, u16 size)
{
	u8 requestType = USB_CTRLTYPE_DIR_DEVICE2HOST;

	if (valuehi==USB_DT_HID || valuehi==USB_DT_REPORT || valuehi==USB_DT_PHYSICAL)
		requestType |= USB_CTRLTYPE_REC_INTERFACE;

	return __usb_control_message(fd, requestType, USB_REQ_GETDESCRIPTOR, (valuehi << 8) | valuelo, index, size, buffer, NULL, NULL);
}

static u32 __find_next_endpoint(u8 *buffer,s32 size,u8 align)
{
	u8 *ptr = buffer;

	while(size>2 && buffer[0]) { // abort if buffer[0]==0 to avoid getting stuck
		if(buffer[1]==USB_DT_ENDPOINT || buffer[1]==USB_DT_INTERFACE)
			break;

		size -= (buffer[0]+align)&~align;
		buffer += (buffer[0]+align)&~align;
	}

	return (buffer - ptr);
}

s32 USB_Initialize()
{
	if(hId==-1) hId = iosCreateHeap(USB_HEAPSIZE);
	if(hId<0) return IPC_ENOMEM;

	if (ven_host==NULL) {
		s32 ven_fd = IOS_Open(__ven_path, IPC_OPEN_NONE);
		if (ven_fd>=0) {
			ven_host = (struct _usbv5_host*)iosAlloc(hId, sizeof(*ven_host));
			if (ven_host==NULL) {
				IOS_Close(ven_fd);
				return IPC_ENOMEM;
			}
			memset(ven_host, 0, sizeof(*ven_host));
			ven_host->fd = ven_fd;

			u32 *ven_ver = (u32*)iosAlloc(hId, 0x20);
			if (ven_ver==NULL) goto mem_error;
			if (IOS_Ioctl(ven_fd, USBV5_IOCTL_GETVERSION, NULL, 0, ven_ver, 0x20)==0 && ven_ver[0]==0x50001)
				IOS_IoctlAsync(ven_fd, USBV5_IOCTL_GETDEVICECHANGE, NULL, 0, ven_host->attached_devices, 0x180, __usbv5_devicechangeCB, ven_host);
			else {
				// wrong ven version
				IOS_Close(ven_fd);
				iosFree(hId, ven_host);
				ven_host = NULL;
			}

			iosFree(hId, ven_ver);
		}
	}

	if (hid_host==NULL) {
		s32 hid_fd = IOS_Open(__hid_path, IPC_OPEN_NONE);
		if (hid_fd>=0) {
			hid_host = (struct _usbv5_host*)iosAlloc(hId, sizeof(*hid_host));
			if (hid_host==NULL) {
				IOS_Close(hid_fd);
				goto mem_error;
			}
			memset(hid_host, 0, sizeof(*hid_host));
			hid_host->fd = hid_fd;

			u32 *hid_ver = (u32*)iosAlloc(hId, 0x20);
			if (hid_ver==NULL) goto mem_error;
			// have to call the USB4 version first, to be safe
			if (IOS_Ioctl(hid_fd, USBV4_IOCTL_GETVERSION, NULL, 0, NULL, 0)==0x40001  || \
					IOS_Ioctl(hid_fd, USBV5_IOCTL_GETVERSION, NULL, 0, hid_ver, 0x20) || hid_ver[0]!=0x50001) {
				// wrong hid version
				IOS_Close(hid_fd);
				iosFree(hId, hid_host);
				hid_host = NULL;
			} else
				IOS_IoctlAsync(hid_fd, USBV5_IOCTL_GETDEVICECHANGE, NULL, 0, hid_host->attached_devices, 0x180, __usbv5_devicechangeCB, hid_host);

			iosFree(hId, hid_ver);
		}
	}

	return IPC_OK;

mem_error:
	USB_Deinitialize();
	return IPC_ENOMEM;
}

s32 USB_Deinitialize()
{
	if (hid_host) {
		if (hid_host->fd>=0) {
			IOS_Ioctl(hid_host->fd, USBV5_IOCTL_SHUTDOWN, NULL, 0, NULL, 0);
			IOS_Close(hid_host->fd);
		}
		iosFree(hId, hid_host);
		hid_host = NULL;
	}

	if (ven_host) {
		if (ven_host->fd>=0) {
			IOS_Ioctl(ven_host->fd, USBV5_IOCTL_SHUTDOWN, NULL, 0, NULL, 0);
			IOS_Close(ven_host->fd);
		}
		iosFree(hId, ven_host);
		ven_host = NULL;
	}

	return IPC_OK;
}

s32 USB_OpenDevice(s32 device_id,u16 vid,u16 pid,s32 *fd)
{
	s32 ret = USB_OK;
	char *devicepath = NULL;
	*fd = -1;

	if (device_id && device_id!=USB_OH1_DEVICE_ID) {
		int i;

		i = __find_device_on_host(ven_host, device_id);
		if (i>=0)
			USB_ResumeDevice(device_id);
		else {
			// HID V5 devices need their descriptors read before being used
			usb_devdesc desc;
			i = __find_device_on_host(hid_host, device_id);
			if (i>=0) {
				USB_ResumeDevice(device_id);
				i = USB_GetDescriptors(device_id, &desc);
				if (i>=0)
					USB_FreeDescriptors(&desc);
				else {
					USB_SuspendDevice(device_id);
					return i;
				}
			}
		}
		if (i>=0) {
			*fd = device_id;
			return 0;
		}
	}

	devicepath = iosAlloc(hId,USB_MAXPATH);
	if(devicepath==NULL) return IPC_ENOMEM;

	if (device_id==USB_OH1_DEVICE_ID)
		snprintf(devicepath,USB_MAXPATH,"/dev/usb/oh1/%x/%x",vid,pid);
	else
		snprintf(devicepath,USB_MAXPATH,"/dev/usb/oh0/%x/%x",vid,pid);

	*fd = IOS_Open(devicepath,0);
	if(*fd<0) ret = *fd;

	if (devicepath!=NULL) iosFree(hId,devicepath);
	return ret;
}

s32 USBV5_CloseDevice(s32 device_id)
{
	int i;
	struct _usbv5_host* host;

	if (__find_device_on_host(ven_host, device_id)>=0)
		host = ven_host;
	else if (__find_device_on_host(hid_host, device_id)>=0)
		host = hid_host;
	else
		return IPC_EINVAL;

	for (i=0; i < USB_MAX_DEVICES; i++) {
		if (host->remove_cb[i].cb==NULL)
			continue;

		if (host->remove_cb[i].device_id==device_id) {
			host->remove_cb[i].cb(0, host->remove_cb[i].userdata);
			host->remove_cb[i].cb = NULL;
			break;
		}
	}
	//return USB_SuspendDevice(device_id);
	return 0;
}

s32 USB_CloseDevice(s32 *fd)
{
	s32 ret = IPC_EINVAL;
	if (fd) {
		ret = USBV5_CloseDevice(*fd);
		if (ret==IPC_EINVAL && *fd>0)
			ret = IOS_Close(*fd);
		if (ret>=0) *fd = -1;
	}

	return ret;
}

s32 USB_CloseDeviceAsync(s32 *fd,usbcallback cb,void *userdata)
{
	s32 ret = IPC_EINVAL;
	if(fd) {
		ret = USBV5_CloseDevice(*fd);
		if (ret!=IPC_EINVAL) {
			if (cb)
				return cb(ret, userdata);
			else
				return ret;
		}
		if (*fd>0)
			return IOS_CloseAsync(*fd,cb,userdata);
	}

	return ret;
}

s32 USB_GetDeviceDescription(s32 fd,usb_devdesc *devdesc)
{
	s32 ret;
	usb_devdesc *p;

	p = iosAlloc(hId,USB_DT_DEVICE_SIZE);
	if(p==NULL) return IPC_ENOMEM;

	ret = __usb_control_message(fd,USB_CTRLTYPE_DIR_DEVICE2HOST,USB_REQ_GETDESCRIPTOR,(USB_DT_DEVICE<<8),0,USB_DT_DEVICE_SIZE,p,NULL,NULL);
	if(ret>=0) memcpy(devdesc,p,USB_DT_DEVICE_SIZE);
	devdesc->configurations = NULL;

	if(p!=NULL) iosFree(hId,p);
	return ret;
}

static s32 USBV5_GetDescriptors(s32 device_id, usb_devdesc *udd)
{
	s32 retval = IPC_ENOMEM;
	u32 *io_buffer = NULL;
	u8 *buffer = NULL;
	usb_configurationdesc *ucd = NULL;
	usb_interfacedesc *uid = NULL;
	usb_endpointdesc *ued = NULL;
	u32 iConf, iEndpoint;
	s32 fd;
	u32 desc_out_size, desc_start_offset;

	if (__find_device_on_host(ven_host, device_id)>=0) {
		fd = ven_host->fd;
		desc_out_size = 0xC0;
		desc_start_offset = 20;
	} else if (__find_device_on_host(hid_host, device_id)>=0) {
		fd = hid_host->fd;
		desc_out_size = 0x60;
		desc_start_offset = 36;
	} else
		return IPC_EINVAL;

	io_buffer = (u32*)iosAlloc(hId, 0x20);
	buffer = (u8*)iosAlloc(hId, desc_out_size);
	if (io_buffer==NULL || buffer==NULL) goto free_bufs;

	io_buffer[0] = device_id;
	io_buffer[2] = 0;
	memset(buffer, 0, desc_out_size);
	retval = IOS_Ioctl(fd, USBV5_IOCTL_GETDEVPARAMS, io_buffer, 0x20, buffer, desc_out_size);
	if (retval==IPC_OK) {
		u8 *next = buffer+desc_start_offset;

		memcpy(udd, next, sizeof(*udd));
		udd->configurations = calloc(udd->bNumConfigurations, sizeof(*udd->configurations));
		if(udd->configurations == NULL)	goto free_bufs;

		next += (udd->bLength+3)&~3;
		for (iConf = 0; iConf < udd->bNumConfigurations; iConf++)
		{
			ucd = &udd->configurations[iConf];
			memcpy(ucd, next, USB_DT_CONFIG_SIZE);
			next += (USB_DT_CONFIG_SIZE+3)&~3;

			// ignore the actual value of bNumInterfaces; IOS presents each interface as a different device
			// alternate settings will not show up here, if you want them you must explicitly request
			// the interface descriptor
			if (ucd->bNumInterfaces==0)
				continue;

			ucd->bNumInterfaces = 1;
			ucd->interfaces = calloc(1, sizeof(*ucd->interfaces));
			if (ucd->interfaces == NULL) goto free_bufs;

			uid = ucd->interfaces;
			memcpy(uid, next, USB_DT_INTERFACE_SIZE);
			next += (uid->bLength+3)&~3;

			/* This skips vendor and class specific descriptors */
			uid->extra_size = __find_next_endpoint(next, buffer+desc_out_size-next, 3);
			if(uid->extra_size>0)
			{
				uid->extra = malloc(uid->extra_size);
				if(uid->extra == NULL)
					goto free_bufs;
				memcpy(uid->extra, next, uid->extra_size);
				// already rounded
				next += uid->extra_size;
			}

			if (uid->bNumEndpoints) {
				uid->endpoints = calloc(uid->bNumEndpoints, sizeof(*uid->endpoints));
				if (uid->endpoints == NULL)	goto free_bufs;

				for(iEndpoint = 0; iEndpoint < uid->bNumEndpoints; iEndpoint++)
				{
					ued = &uid->endpoints[iEndpoint];
					memcpy(ued, next, USB_DT_ENDPOINT_SIZE);
					next += (ued->bLength+3)&~3;
				}
			}

		}

		retval = IPC_OK;
	}

free_bufs:
	if (io_buffer!=NULL)
		iosFree(hId, io_buffer);
	if (buffer!=NULL)
		iosFree(hId, buffer);
	if (retval<0)
		USB_FreeDescriptors(udd);
	return retval;
}

s32 USB_GetDescriptors(s32 fd, usb_devdesc *udd)
{
	u8 *buffer = NULL;
	u8 *ptr = NULL;
	usb_configurationdesc *ucd = NULL;
	usb_interfacedesc *uid = NULL;
	usb_endpointdesc *ued = NULL;
	s32 retval = 0;
	u32 size;
	u32 iConf, iInterface, iEndpoint;

	if (udd==NULL)
		return IPC_EINVAL;
	memset(udd, 0, sizeof(*udd));

	if (fd>=0x20 || fd<-1)
		return USBV5_GetDescriptors(fd, udd);

	buffer = iosAlloc(hId, sizeof(*udd));
	if(buffer == NULL)
	{
		retval = IPC_ENOHEAP;
		goto free_and_error;
	}

	retval = __usb_getdesc(fd, buffer, USB_DT_DEVICE, 0, 0, USB_DT_DEVICE_SIZE);
	if(retval < 0)
		goto free_and_error;
	memcpy(udd, buffer, USB_DT_DEVICE_SIZE);
	iosFree(hId, buffer);

	udd->bcdUSB = bswap16(udd->bcdUSB);
	udd->idVendor = bswap16(udd->idVendor);
	udd->idProduct = bswap16(udd->idProduct);
	udd->bcdDevice = bswap16(udd->bcdDevice);

	udd->configurations = calloc(udd->bNumConfigurations, sizeof(*udd->configurations));
	if(udd->configurations == NULL)
	{
		retval = IPC_ENOMEM;
		goto free_and_error;
	}
	for(iConf = 0; iConf < udd->bNumConfigurations; iConf++)
	{
		buffer = iosAlloc(hId, USB_DT_CONFIG_SIZE);
		if(buffer == NULL)
		{
			retval = IPC_ENOHEAP;
			goto free_and_error;
		}

		retval = __usb_getdesc(fd, buffer, USB_DT_CONFIG, iConf, 0, USB_DT_CONFIG_SIZE);
		if (retval < 0)
			goto free_and_error;

		ucd = &udd->configurations[iConf];
		memcpy(ucd, buffer, USB_DT_CONFIG_SIZE);
		iosFree(hId, buffer);

		ucd->wTotalLength = bswap16(ucd->wTotalLength);
		size = ucd->wTotalLength;
		buffer = iosAlloc(hId, size);
		if(buffer == NULL)
		{
			retval = IPC_ENOHEAP;
			goto free_and_error;
		}

		retval = __usb_getdesc(fd, buffer, USB_DT_CONFIG, iConf, 0, ucd->wTotalLength);
		if(retval < 0)
			goto free_and_error;

		ptr = buffer;
		ptr += ucd->bLength;
		size -= ucd->bLength;

		retval = IPC_ENOMEM;

		ucd->interfaces = calloc(ucd->bNumInterfaces, sizeof(*ucd->interfaces));
		if(ucd->interfaces == NULL)
			goto free_and_error;
		for(iInterface = 0; iInterface < ucd->bNumInterfaces; iInterface++)
		{
			retval = __find_next_endpoint(ptr, size, 0);
			if (retval>0) {
				// FIXME: do something with this data
			}
			ptr += retval;
			size -= retval;

			uid = ucd->interfaces+iInterface;
			memcpy(uid, ptr, USB_DT_INTERFACE_SIZE);
			ptr += uid->bLength;
			size -= uid->bLength;

			/* This skips vendor and class specific descriptors */
			uid->extra_size = __find_next_endpoint(ptr, size, 0);
			if(uid->extra_size>0)
			{
				uid->extra = malloc(uid->extra_size);
				if(uid->extra == NULL)
					goto free_and_error;
				memcpy(uid->extra, ptr, uid->extra_size);
				ptr += uid->extra_size;
				size -= uid->extra_size;
			}

			if (uid->bNumEndpoints) {
				uid->endpoints = calloc(uid->bNumEndpoints, sizeof(*uid->endpoints));
				if(uid->endpoints == NULL)
					goto free_and_error;

				for(iEndpoint = 0; iEndpoint < uid->bNumEndpoints; iEndpoint++)
				{
					ued = &uid->endpoints[iEndpoint];
					memcpy(ued, ptr, USB_DT_ENDPOINT_SIZE);
					ptr += ued->bLength;
					size -= ued->bLength;
					ued->wMaxPacketSize = bswap16(ued->wMaxPacketSize);
				}
			}

			if (iInterface==(ucd->bNumInterfaces-1) && size>2) {
				// we've read all the interfaces but there's data left (probably alternate setting interfaces)
				// see if we can find another interface descriptor
				retval = __find_next_endpoint(ptr, size, 0);
				if (size-retval >= USB_DT_INTERFACE_SIZE && ptr[retval+1] == USB_DT_INTERFACE) {
					// found alternates, make room and loop
					usb_interfacedesc *interfaces = realloc(ucd->interfaces, (iInterface+2)*sizeof(*interfaces));
					if (interfaces == NULL)
						goto free_and_error;
					interfaces[iInterface+1].endpoints = NULL;
					interfaces[iInterface+1].extra = NULL;
					ucd->bNumInterfaces++;
					ucd->interfaces = interfaces;
				}
			}
		}
		iosFree(hId, buffer);
		buffer = NULL;
	}
	retval = IPC_OK;

free_and_error:
	if(buffer != NULL)
		iosFree(hId, buffer);
	if(retval < 0)
		USB_FreeDescriptors(udd);
	return retval;
}

s32 USB_GetGenericDescriptor(s32 fd,u8 type,u8 index,u8 interface,void *data,u32 size) {
	u8 *buffer;
	s32 retval;

	buffer = iosAlloc(hId,size);
	if(buffer==NULL) {
		retval = IPC_ENOMEM;
		goto free_and_error;
	}

	retval = __usb_getdesc(fd,buffer,type,index,interface,size);
	if(retval<0) goto free_and_error;

	memcpy(data,buffer,size);
	retval = IPC_OK;

free_and_error:
	if(buffer!=NULL) iosFree(hId,buffer);
	return retval;
}

s32 USB_GetHIDDescriptor(s32 fd,u8 interface,usb_hiddesc *uhd,u32 size)
{
	int i;
	s32 retval;

	if (size < USB_DT_HID_SIZE)
		return IPC_EINVAL;

	retval = USB_GetGenericDescriptor(fd, USB_DT_HID, 0, interface, uhd, size);
	if (retval != IPC_OK)
		return retval;

	uhd->bcdHID = bswap16(uhd->bcdHID);
	uhd->descr[0].wDescriptorLength = bswap16(uhd->descr[0].wDescriptorLength);
	size -= USB_DT_HID_SIZE;
	for (i=1; i<uhd->bNumDescriptors && size>=3; size -=3, i++)
		uhd->descr[i].wDescriptorLength = bswap16(uhd->descr[i].wDescriptorLength);

	return retval;
}

void USB_FreeDescriptors(usb_devdesc *udd)
{
	int iConf, iInterface;
	usb_configurationdesc *ucd;
	usb_interfacedesc *uid;
	if(udd->configurations != NULL)
	{
		for(iConf = 0; iConf < udd->bNumConfigurations; iConf++)
		{
			ucd = &udd->configurations[iConf];
			if(ucd->interfaces != NULL)
			{
				for(iInterface = 0; iInterface < ucd->bNumInterfaces; iInterface++)
				{
					uid = ucd->interfaces+iInterface;
					free(uid->endpoints);
					free(uid->extra);
				}
				free(ucd->interfaces);
			}
		}
		free(udd->configurations);
	}
}

s32 USB_GetAsciiString(s32 fd,u8 bIndex,u16 wLangID,u16 wLength,void *rpData)
{
	s32 ret;
	u8 bo, ro;
	u8 *buf;
	u8 *rp = (u8 *)rpData;

	if(wLength > 255)
		wLength = 255;

	buf = iosAlloc(hId, 255); /* 255 is the highest possible length of a descriptor */
	if(buf == NULL)
		return IPC_ENOMEM;

	ret = __usb_getdesc(fd, buf, USB_DT_STRING, bIndex, wLangID, 255);

	/* index 0 gets a list of supported languages */
	if(bIndex == 0)
	{
		if(ret > 0)
			memcpy(rpData, buf, wLength);
		iosFree(hId, buf);
		return ret;
	}

	if(ret > 0)
	{
		bo = 2;
		ro = 0;
		while(ro < (wLength - 1) && bo < buf[0])
		{
			if(buf[bo + 1])
				rp[ro++] = '?';
			else
				rp[ro++] = buf[bo];
			bo += 2;
		}
		rp[ro] = 0;
		ret = ro - 1;
	}

	iosFree(hId, buf);
 	return ret;
}

s32 USB_ReadIsoMsg(s32 fd,u8 bEndpoint,u8 bPackets,u16 *rpPacketSizes,void *rpData)
{
	return __usb_isochronous_message(fd,bEndpoint,bPackets,rpPacketSizes,rpData,NULL,NULL);
}

s32 USB_ReadIsoMsgAsync(s32 fd,u8 bEndpoint,u8 bPackets,u16 *rpPacketSizes,void *rpData,usbcallback cb,void *userdata)
{
	return __usb_isochronous_message(fd,bEndpoint,bPackets,rpPacketSizes,rpData,cb,userdata);
}

s32 USB_WriteIsoMsg(s32 fd,u8 bEndpoint,u8 bPackets,u16 *rpPacketSizes,void *rpData)
{
	return __usb_isochronous_message(fd,bEndpoint,bPackets,rpPacketSizes,rpData,NULL,NULL);
}

s32 USB_WriteIsoMsgAsync(s32 fd,u8 bEndpoint,u8 bPackets,u16 *rpPacketSizes,void *rpData,usbcallback cb,void *userdata)
{
	return __usb_isochronous_message(fd,bEndpoint,bPackets,rpPacketSizes,rpData,cb,userdata);
}

s32 USB_ReadIntrMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData)
{
	return __usb_interrupt_bulk_message(fd,USBV0_IOCTL_INTRMSG,bEndpoint,wLength,rpData,NULL,NULL);
}

s32 USB_ReadIntrMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,usbcallback cb,void *userdata)
{
	return __usb_interrupt_bulk_message(fd,USBV0_IOCTL_INTRMSG,bEndpoint,wLength,rpData,cb,userdata);
}

s32 USB_WriteIntrMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData)
{
	return __usb_interrupt_bulk_message(fd,USBV0_IOCTL_INTRMSG,bEndpoint,wLength,rpData,NULL,NULL);
}

s32 USB_WriteIntrMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,usbcallback cb,void *userdata)
{
	return __usb_interrupt_bulk_message(fd,USBV0_IOCTL_INTRMSG,bEndpoint,wLength,rpData,cb,userdata);
}

s32 USB_ReadBlkMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData)
{
	return __usb_interrupt_bulk_message(fd,USBV0_IOCTL_BLKMSG,bEndpoint,wLength,rpData,NULL,NULL);
}

s32 USB_ReadBlkMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,usbcallback cb,void *userdata)
{
	return __usb_interrupt_bulk_message(fd,USBV0_IOCTL_BLKMSG,bEndpoint,wLength,rpData,cb,userdata);
}

s32 USB_WriteBlkMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData)
{
	return __usb_interrupt_bulk_message(fd,USBV0_IOCTL_BLKMSG,bEndpoint,wLength,rpData,NULL,NULL);
}

s32 USB_WriteBlkMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,usbcallback cb,void *userdata)
{
	return __usb_interrupt_bulk_message(fd,USBV0_IOCTL_BLKMSG,bEndpoint,wLength,rpData,cb,userdata);
}

s32 USB_ReadCtrlMsg(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData)
{
	return __usb_control_message(fd,bmRequestType,bmRequest,wValue,wIndex,wLength,rpData,NULL,NULL);
}

s32 USB_ReadCtrlMsgAsync(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData,usbcallback cb,void *userdata)
{
	return __usb_control_message(fd,bmRequestType,bmRequest,wValue,wIndex,wLength,rpData,cb,userdata);
}

s32 USB_WriteCtrlMsg(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData)
{
	return __usb_control_message(fd,bmRequestType,bmRequest,wValue,wIndex,wLength,rpData,NULL,NULL);
}

s32 USB_WriteCtrlMsgAsync(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData,usbcallback cb,void *userdata)
{
	return __usb_control_message(fd,bmRequestType,bmRequest,wValue,wIndex,wLength,rpData,cb,userdata);
}

static s32 USB5_RegisterDeviceRemoval(struct _usbv5_host *host, s32 device_id, usbcallback cb, void *userdata)
{
	int i;

	// check to make sure the device is present
	if (__find_device_on_host(host, device_id)<0)
		return IPC_ENOENT;

	// now make sure it's not hooked already
	for (i=0; i<USB_MAX_DEVICES; i++) {
		if (host->remove_cb[i].cb && host->remove_cb[i].device_id==device_id)
			return IPC_EINVAL;
	}

	// find a free entry and add it
	for (i=0; i<USB_MAX_DEVICES; i++) {
		if (host->remove_cb[i].cb==NULL) {
			host->remove_cb[i].cb = cb;
			host->remove_cb[i].userdata = userdata;
			host->remove_cb[i].device_id = device_id;
			return IPC_OK;
		}
	}
	return IPC_EINVAL;
}

s32 USB_DeviceRemovalNotifyAsync(s32 fd,usbcallback cb,void *userdata)
{
	s32 ret;
	if (fd>=0 && fd<0x20)
		return IOS_IoctlAsync(fd,USBV0_IOCTL_DEVREMOVALHOOK,NULL,0,NULL,0,cb,userdata);

	ret = USB5_RegisterDeviceRemoval(ven_host, fd, cb, userdata);
	if (ret == IPC_ENOENT)
		ret = USB5_RegisterDeviceRemoval(hid_host, fd, cb, userdata);

	return ret;
}

static s32 USBV5_SuspendResume(s32 device_id, s32 resumed)
{
	s32 ret;
	s32 fd;

	if (__find_device_on_host(ven_host, device_id)>=0)
		fd = ven_host->fd;
	else if (__find_device_on_host(hid_host, device_id)>=0)
		fd = hid_host->fd;
	else
		return IPC_ENOENT;

	s32 *buf = (s32*)iosAlloc(hId, 32);
	if (buf==NULL) return IPC_ENOMEM;

	buf[0] = device_id;
	buf[2] = resumed;
	ret = IOS_Ioctl(fd, USBV5_IOCTL_SUSPEND_RESUME, buf, 32, NULL, 0);
	iosFree(hId, buf);

	return ret;
}

s32 USB_SuspendDevice(s32 fd)
{
	if (fd>=0x20 || fd<-1)
		return USBV5_SuspendResume(fd, 0);

	return IOS_Ioctl(fd,USBV0_IOCTL_SUSPENDDEV,NULL,0,NULL,0);
}

s32 USB_ResumeDevice(s32 fd)
{
	if (fd>=0x20 || fd<-1)
		return USBV5_SuspendResume(fd, 1);

	return IOS_Ioctl(fd,USBV0_IOCTL_RESUMEDEV,NULL,0,NULL,0);
}

s32 USB_DeviceChangeNotifyAsync(u8 interface_class, usbcallback cb, void* userdata)
{
	s32 ret=IPC_ENOENT;

	if (ven_host==NULL) {
		s32 fd;
		struct _usb_msg *msg;

		fd = IOS_Open(__oh0_path,IPC_OPEN_NONE);
		if (fd<0) return fd;

		msg = iosAlloc(hId,sizeof(*msg));
		if (msg==NULL) {
			IOS_Close(fd);
			return IPC_ENOMEM;
		}

		msg->cb = cb;
		msg->userdata = userdata;
		msg->class = interface_class;

		msg->vec[0].data = &msg->class;
		msg->vec[0].len = 1;

		ret = IOS_IoctlvAsync(fd,USBV0_IOCTL_DEVICECLASSCHANGE,1,0,msg->vec,__usbv5_messageCB,msg);
		IOS_Close(fd);

		if (ret<0) iosFree(hId, msg);
	}
	else if (interface_class != USB_CLASS_HID && ven_host)
		ret = add_devicechange_cb(&ven_host->device_change_notify, cb, userdata);

	else if (interface_class==USB_CLASS_HID && hid_host)
		ret = add_devicechange_cb(&hid_host->device_change_notify, cb, userdata);

	return ret;
}

s32 USB_GetDeviceList(usb_device_entry *descr_buffer,u8 num_descr,u8 interface_class,u8 *cnt_descr)
{
	int i;
	u8 cntdevs=0;

	if (ven_host==NULL) {
		s32 fd;
		u32 *buf = (u32*)iosAlloc(hId, num_descr<<3);
		if (buf==NULL) return IPC_ENOMEM;

		fd = IOS_Open(__oh0_path,IPC_OPEN_NONE);
		if (fd<0) {
			iosFree(hId, buf);
			return fd;
		}

		cntdevs = 0;
		i = IOS_IoctlvFormat(hId,fd,USBV0_IOCTL_GETDEVLIST,"bb:dd",num_descr,interface_class,&cntdevs,sizeof(cntdevs),buf,(num_descr<<3));
		if (cnt_descr) *cnt_descr = cntdevs;

		while (cntdevs--) {
			descr_buffer[cntdevs].device_id = 0;
			descr_buffer[cntdevs].vid = (u16)(buf[cntdevs*2+1]>>16);
			descr_buffer[cntdevs].pid = (u16)buf[cntdevs*2+1];
		}

		IOS_Close(fd);
		iosFree(hId, buf);
		return i;
	}

	// for ven_host, we can only exclude usb_hid class devices
	if (interface_class != USB_CLASS_HID && ven_host) {
		i=0;
		while (cntdevs<num_descr && ven_host->attached_devices[i].device_id) {
			descr_buffer[cntdevs++] = ven_host->attached_devices[i++];
			if (i>=32) break;
		}
	}

	if ((!interface_class || interface_class==USB_CLASS_HID) && hid_host) {
		i=0;
		while (cntdevs<num_descr && hid_host->attached_devices[i].device_id) {
			descr_buffer[cntdevs++] = hid_host->attached_devices[i++];
			if (i>32) break;
		}
	}

	if (cnt_descr) *cnt_descr = cntdevs;

	return IPC_OK;
}

s32 USB_SetConfiguration(s32 fd, u8 configuration)
{
	return __usb_control_message(fd, (USB_CTRLTYPE_DIR_HOST2DEVICE | USB_CTRLTYPE_TYPE_STANDARD | USB_CTRLTYPE_REC_DEVICE), USB_REQ_SETCONFIG, configuration, 0, 0, NULL, NULL, NULL);
}

s32 USB_GetConfiguration(s32 fd, u8 *configuration)
{
	u8 *_configuration;
	s32 retval;

	_configuration = iosAlloc(hId, 1);
	if(_configuration == NULL)
		return IPC_ENOMEM;

	retval = __usb_control_message(fd, (USB_CTRLTYPE_DIR_DEVICE2HOST | USB_CTRLTYPE_TYPE_STANDARD | USB_CTRLTYPE_REC_DEVICE), USB_REQ_GETCONFIG, 0, 0, 1, _configuration, NULL, NULL);
	if(retval >= 0)
		*configuration = *_configuration;
	iosFree(hId, _configuration);

	return retval;
}

s32 USB_SetAlternativeInterface(s32 fd, u8 interface, u8 alternateSetting)
{
	return __usb_control_message(fd, (USB_CTRLTYPE_DIR_HOST2DEVICE | USB_CTRLTYPE_TYPE_STANDARD | USB_CTRLTYPE_REC_INTERFACE), USB_REQ_SETINTERFACE, alternateSetting, interface, 0, NULL, NULL, NULL);
}

static s32 USBV5_CancelEndpoint(s32 device_id, u8 endpoint)
{
	s32 ret;
	s32 fd;

	if (__find_device_on_host(ven_host, device_id)>=0)
		fd = ven_host->fd;
	else if (__find_device_on_host(hid_host, device_id)>=0)
		fd = hid_host->fd;
	else
		return IPC_ENOENT;

	s32 *buf = (s32*)iosAlloc(hId, 32);
	if (buf==NULL) return IPC_ENOMEM;

	buf[0] = device_id;
	buf[2] = endpoint;
	ret = IOS_Ioctl(fd, USBV5_IOCTL_CANCELENDPOINT, buf, 32, NULL, 0);
	iosFree(hId, buf);

	return ret;
}

s32 USB_ClearHalt(s32 fd, u8 endpoint)
{
	if (fd>=0x20 || fd<-1)
		return USBV5_CancelEndpoint(fd, endpoint);
	return __usb_control_message(fd, (USB_CTRLTYPE_DIR_HOST2DEVICE | USB_CTRLTYPE_TYPE_STANDARD | USB_CTRLTYPE_REC_ENDPOINT), USB_REQ_CLEARFEATURE, USB_FEATURE_ENDPOINT_HALT, endpoint, 0, NULL, NULL, NULL);
}

#endif /* defined(HW_RVL) */
