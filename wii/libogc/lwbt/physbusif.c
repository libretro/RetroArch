#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <gccore.h>
#include <usb.h>

#include "hci.h"
#include "btmemb.h"
#include "physbusif.h"

#define STACKSIZE					32768

#define NUM_ACL_BUFS				30
#define NUM_CTRL_BUFS				45

#define ACL_BUF_SIZE				1800
#define CTRL_BUF_SIZE				660

#define ROUNDUP32(v)				(((u32)(v)+0x1f)&~0x1f)
#define ROUNDDOWN32(v)				(((u32)(v)-0x1f)&~0x1f)

struct usbtxbuf
{
	u32 txsize;
	void *rpData;
};

static u32 __ntd_ohci = 0;
static u32 __ntd_ohci_initflag = 0;
static u16 __ntd_vid = 0;
static u16 __ntd_pid = 0;
static u32 __ntd_vid_pid_specified = 0;
static s32 __ntd_usb_fd = -1;
static u32 __wait4hci = 1;
static struct _usb_p __usbdev;

static struct memb_blks ctrlbufs;
static struct memb_blks aclbufs;

static u8 __ppc_btstack1[STACKSIZE] ATTRIBUTE_ALIGN(8);
static u8 __ppc_btstack2[STACKSIZE] ATTRIBUTE_ALIGN(8);

static s32 __issue_bulkread();
static s32 __issue_intrread();

extern u32 __IPC_ClntInit();

static s32 __usb_closeCB(s32 result,void *usrdata)
{
	__usbdev.fd = -1;
	return result;
}

static s32 __writectrlmsgCB(s32 result,void *usrdata)
{
	if(usrdata!=NULL) btmemb_free(&ctrlbufs,usrdata);
	return result;
}

static s32 __writebulkmsgCB(s32 result,void *usrdata)
{
	if(usrdata!=NULL) btmemb_free(&aclbufs,usrdata);
	return result;
}

static s32 __readbulkdataCB(s32 result,void *usrdata)
{
	u8 *ptr;
	u32 len;
	struct pbuf *p,*q;
	struct usbtxbuf *buf = (struct usbtxbuf*)usrdata;

	if(__usbdev.openstate!=0x0002) return 0;

	if(result>0) {
		len = result;
		p = btpbuf_alloc(PBUF_RAW,len,PBUF_POOL);
		if(p!=NULL) {
			ptr = buf->rpData;
			for(q=p;q!=NULL && len>0;q=q->next) {
				memcpy(q->payload,ptr,q->len);
				ptr += q->len;
				len -= q->len;
			}

			SYS_SwitchFiber((u32)p,0,0,0,(u32)hci_acldata_handler,(u32)(&__ppc_btstack2[STACKSIZE]));
			btpbuf_free(p);
		} else
			ERROR("__readbulkdataCB: Could not allocate memory for pbuf.\n");
	}
	btmemb_free(&aclbufs,buf);

	return __issue_bulkread();
}

static s32 __readintrdataCB(s32 result,void *usrdata)
{
	u8 *ptr;
	u32 len;
	struct pbuf *p,*q;
	struct usbtxbuf *buf = (struct usbtxbuf*)usrdata;

	if(__usbdev.openstate!=0x0002) return 0;

	if(result>0) {
		len = result;
		p = btpbuf_alloc(PBUF_RAW,len,PBUF_POOL);
		if(p!=NULL) {
			ptr = buf->rpData;
			for(q=p;q!=NULL && len>0;q=q->next) {
				memcpy(q->payload,ptr,q->len);
				ptr += q->len;
				len -= q->len;
			}

			SYS_SwitchFiber((u32)p,0,0,0,(u32)hci_event_handler,(u32)(&__ppc_btstack1[STACKSIZE]));
			btpbuf_free(p);
		} else
			ERROR("__readintrdataCB: Could not allocate memory for pbuf.\n");
	}
	btmemb_free(&ctrlbufs,buf);

	return __issue_intrread();
}

static s32 __issue_intrread()
{
	s32 ret;
	u32 len;
	u8 *ptr;
	struct usbtxbuf *buf;

	if(__usbdev.openstate!=0x0002) return IPC_OK;

	buf = (struct usbtxbuf*)btmemb_alloc(&ctrlbufs);
	if(buf!=NULL) {
		ptr = (u8*)((u32)buf + sizeof(struct usbtxbuf));
		buf->rpData = (void*)ROUNDUP32(ptr);
		len = (ctrlbufs.size - ((u32)buf->rpData - (u32)buf));
		buf->txsize = ROUNDDOWN32(len);
		ret = USB_ReadIntrMsgAsync(__usbdev.fd,__usbdev.hci_evt,buf->txsize,buf->rpData,__readintrdataCB,buf);
	} else
		ret = IPC_ENOMEM;

	return ret;
}

static s32 __issue_bulkread()
{
	s32 ret;
	u32 len;
	u8 *ptr;
	struct usbtxbuf *buf;

	if(__usbdev.openstate!=0x0002) return IPC_OK;

	buf = (struct usbtxbuf*)btmemb_alloc(&aclbufs);
	if(buf!=NULL) {
		ptr = (u8*)((u32)buf + sizeof(struct usbtxbuf));
		buf->rpData = (void*)ROUNDUP32(ptr);
		len = (aclbufs.size - ((u32)buf->rpData - (u32)buf));
		buf->txsize = ROUNDDOWN32(len);
		ret = USB_ReadBlkMsgAsync(__usbdev.fd,__usbdev.acl_in,buf->txsize,buf->rpData,__readbulkdataCB,buf);
	} else
		ret = IPC_ENOMEM;

	return ret;
}

static s32 __initUsbIOBuffer(struct memb_blks *blk,u32 buf_size,u32 num_bufs)
{
	u32 len;
	u8 *ptr = NULL;

	len = ((MEM_ALIGN_SIZE(buf_size)+sizeof(u32))*num_bufs);
	ptr = (u8*)ROUNDDOWN32(((u32)SYS_GetArena2Hi() - len));
	if((u32)ptr<(u32)SYS_GetArena2Lo()) return -4;

	SYS_SetArena2Hi(ptr);

	blk->size = buf_size;
	blk->num = num_bufs;
	blk->mem = ptr;

	btmemb_init(blk);
	return 0;
}

static s32 __getDeviceId(u16 vid,u16 pid)
{
	s32 ret = 0;

	if(__ntd_ohci_initflag==0x0001) {
		if(__ntd_ohci==0x0000)
			ret = USB_OpenDevice(USB_OH0_DEVICE_ID,vid,pid,&__usbdev.fd);
		else if(__ntd_ohci==0x0001)
			ret = USB_OpenDevice(USB_OH1_DEVICE_ID,vid,pid,&__usbdev.fd);
	} else
		ret = USB_OpenDevice(USB_OH1_DEVICE_ID,vid,pid,&__usbdev.fd);

	//printf("__getDeviceId(%04x,%04x,%d)\n",vid,pid,__usbdev.fd);
	if(ret==0) __ntd_usb_fd = __usbdev.fd;
	return ret;
}

static s32 __usb_register(pbcallback cb)
{
	s32 ret = 0;

	memset(&__usbdev,0,sizeof(struct _usb_p));
	__usbdev.openstate = 5;

	ret = __IPC_ClntInit();
	if(ret<0) return ret;

	ret = USB_Initialize();
	if(ret<0) return ret;

	__usbdev.fd = -1;
	__usbdev.unregcb = cb;
	if(__ntd_vid_pid_specified) {
		__usbdev.vid = __ntd_vid;
		__usbdev.pid = __ntd_pid;
	} else {
		__usbdev.vid = 0x057E;
		__usbdev.pid = 0x0305;
	}

	ret = __getDeviceId(__usbdev.vid,__usbdev.pid);
	if(ret<0) return ret;

	__usbdev.acl_out		= 0x02;
	__usbdev.acl_in			= 0x82;
	__usbdev.hci_evt		= 0x81;
	__usbdev.hci_ctrl		= 0x00;

	__initUsbIOBuffer(&ctrlbufs,CTRL_BUF_SIZE,NUM_CTRL_BUFS);
	__initUsbIOBuffer(&aclbufs,ACL_BUF_SIZE,NUM_ACL_BUFS);

	__usbdev.openstate = 4;
	__wait4hci = 1;

	return ret;
}

static s32 __usb_open(pbcallback cb)
{
	if(__usbdev.openstate!=0x0004) return -1;

	__usbdev.closecb = cb;
	__usbdev.openstate = 2;

	__issue_intrread();
	__issue_bulkread();

	__wait4hci = 0;
	return 0;
}

void __ntd_set_ohci(u8 hci)
{
	if(hci==0x0000) {
		__ntd_ohci = 0;
		__ntd_ohci_initflag = 1;
	} else if(hci==0x0001) {
		__ntd_ohci = 1;
		__ntd_ohci_initflag = 1;
	}
}

void __ntd_set_pid_vid(u16 vid,u16 pid)
{
	__ntd_vid = vid;
	__ntd_pid = pid;
	__ntd_vid_pid_specified = 1;
}

void physbusif_init()
{
	s32 ret;

	ret = __usb_register(NULL);
	if(ret<0) return;

	__usb_open(NULL);
}

void physbusif_close()
{
	if(__usbdev.openstate!=0x0002) return;

	__usbdev.openstate = 4;
	__wait4hci = 1;
}

void physbusif_shutdown()
{
	if(__usbdev.openstate!=0x0004) return;
	USB_CloseDeviceAsync(&__usbdev.fd,__usb_closeCB,NULL);
}

void physbusif_reset_all()
{
	return;
}

void physbusif_output(struct pbuf *p,u16_t len)
{
	u32 pos;
	u8 *ptr;
	struct pbuf *q;
	struct memb_blks *mblks;
	struct usbtxbuf *blkbuf;

	if(__usbdev.openstate!=0x0002) return;

	if(((u8*)p->payload)[0]==HCI_COMMAND_DATA_PACKET) mblks = &ctrlbufs;
	else if(((u8*)p->payload)[0]==HCI_ACL_DATA_PACKET) mblks = &aclbufs;
	else return;

	blkbuf = btmemb_alloc(mblks);
	if(blkbuf!=NULL) {
		blkbuf->txsize = --len;
		blkbuf->rpData = (void*)ROUNDUP32(((u32)blkbuf+sizeof(struct usbtxbuf)));

		ptr = blkbuf->rpData;
		for(q=p,pos=1;q!=NULL && len>0;q=q->next,pos=0) {
			memcpy(ptr,q->payload+pos,(q->len-pos));
			ptr += (q->len-pos);
			len -= (q->len-pos);
		}

		if(((u8*)p->payload)[0]==HCI_COMMAND_DATA_PACKET) {
			USB_WriteCtrlMsgAsync(__usbdev.fd,0x20,0,0,0,blkbuf->txsize,blkbuf->rpData,__writectrlmsgCB,blkbuf);
		} else if(((u8*)p->payload)[0]==HCI_ACL_DATA_PACKET) {
			USB_WriteBlkMsgAsync(__usbdev.fd,__usbdev.acl_out,blkbuf->txsize,blkbuf->rpData,__writebulkmsgCB,blkbuf);
		}
	}
}
