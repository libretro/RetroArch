#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <gccore.h>

#include "geckousb.h"

#define _SHIFTL(v, s, w)	\
	((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
	((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

static struct dbginterface usb_device;

static __inline__ int __send_command(s32 chn,u16 *cmd)
{
	s32 ret;

	ret = 0;
	if(!EXI_Select(chn,EXI_DEVICE_0,EXI_SPEED32MHZ)) ret |= 0x01;
	if(!EXI_Imm(chn,cmd,sizeof(u16),EXI_READWRITE,NULL)) ret |= 0x02;
	if(!EXI_Sync(chn)) ret |= 0x04;
	if(!EXI_Deselect(chn)) ret |= 0x08;

	if(ret) return 0;
	return 1;
}

static int __usb_sendbyte(s32 chn,char ch)
{
	s32 ret;
	u16 val;

	val = (0xB000|_SHIFTL(ch,4,8));
	ret = __send_command(chn,&val);
	if(ret==1 && !(val&0x0400)) ret = 0;

	return ret;
}

static int __usb_recvbyte(s32 chn,char *ch)
{
	s32 ret;
	u16 val;

	*ch = 0;
	val = 0xA000;
	ret = __send_command(chn,&val);
	if(ret==1 && !(val&0x0800)) ret = 0;
	else if(ret==1) *ch = (val&0xff);

	return ret;
}

static int __usb_checksend(s32 chn)
{
	s32 ret;
	u16 val;

	val = 0xC000;
	ret = __send_command(chn,&val);
	if(ret==1 && !(val&0x0400)) ret = 0;

	return ret;
}

static int __usb_checkrecv(s32 chn)
{
	s32 ret;
	u16 val;

	val = 0xD000;
	ret = __send_command(chn,&val);
	if(ret==1 && !(val&0x0400)) ret = 0;

	return ret;
}

static void __usb_flush(s32 chn)
{
	char tmp;

	if(!EXI_Lock(chn,EXI_DEVICE_0,NULL)) return;

	while(__usb_recvbyte(chn,&tmp));

	EXI_Unlock(chn);
}

static int __usb_isgeckoalive(s32 chn)
{
	s32 ret;
	u16 val;

	if(!EXI_Lock(chn,EXI_DEVICE_0,NULL)) return 0;

	val = 0x9000;
	ret = __send_command(chn,&val);
	if(ret==1 && !(val&0x0470)) ret = 0;

	EXI_Unlock(chn);
	return ret;
}

static int __usb_recvbuffer(s32 chn,void *buffer,int size)
{
	s32 ret;
	s32 left = size;
	char *ptr = (char*)buffer;

	if(!EXI_Lock(chn,EXI_DEVICE_0,NULL)) return 0;

	while(left>0) {
		if(__usb_checkrecv(chn)) {
			ret = __usb_recvbyte(chn,ptr);
			if(ret==0) break;

			ptr++;
			left--;
		}
	}

	EXI_Unlock(chn);
	return (size - left);
}

static int __usb_sendbuffer(s32 chn,const void *buffer,int size)
{
	s32 ret;
	s32 left = size;
	char *ptr = (char*)buffer;

	if(!EXI_Lock(chn,EXI_DEVICE_0,NULL)) return 0;

	while(left>0) {
		if(__usb_checksend(chn)) {
			ret = __usb_sendbyte(chn,*ptr);
			if(ret==0) break;

			ptr++;
			left--;
		}
	}

	EXI_Unlock(chn);
	return (size - left);
}

static int usbopen(struct dbginterface *device)
{
	if(!__usb_isgeckoalive(device->fhndl)) {
		return -1;
	}

	return 0;
}

static int usbclose(struct dbginterface *device)
{
	return 0;
}

static int usbwait(struct dbginterface *device)
{
	return 0;
}

static int usbread(struct dbginterface *device,void *buffer,int size)
{
	int ret;
	ret = __usb_recvbuffer(device->fhndl,buffer,size);
	return ret;
}

static int usbwrite(struct dbginterface *device,const void *buffer,int size)
{
	int ret;
	ret = __usb_sendbuffer(device->fhndl,buffer,size);
	return ret;
}

struct dbginterface* usb_init(s32 channel)
{
	usb_device.fhndl = channel;
	if(__usb_isgeckoalive(channel))
		__usb_flush(channel);

	usb_device.open = usbopen;
	usb_device.close = usbclose;
	usb_device.wait = usbwait;
	usb_device.read = usbread;
	usb_device.write = usbwrite;

	return &usb_device;
}
