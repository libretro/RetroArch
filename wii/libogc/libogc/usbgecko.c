#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <gccore.h>
#include <machine/processor.h>

#include "usbgecko.h"

#define _SHIFTL(v, s, w)	\
	((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
	((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

static u32 usbgecko_inited = 0;
static lwpq_t wait_exi_queue[2];

static s32 __usbgecko_exi_unlock(s32 chan,s32 dev)
{
	LWP_ThreadBroadcast(wait_exi_queue[chan]);
	return 1;
}

static void __usbgecko_init()
{
	u32 i;

	for(i=0;i<EXI_CHANNEL_2;i++) {
		LWP_InitQueue(&wait_exi_queue[i]);
	}
	usbgecko_inited = 1;
}

static __inline__ void __usbgecko_exi_wait(s32 chn)
{
	u32 level;

	_CPU_ISR_Disable(level);
	if(!usbgecko_inited) __usbgecko_init();
	while(EXI_Lock(chn,EXI_DEVICE_0,__usbgecko_exi_unlock)==0) {
		LWP_ThreadSleep(wait_exi_queue[chn]);
	}
	_CPU_ISR_Restore(level);
}

static __inline__ int __send_command(s32 chn,u16 *cmd)
{
	s32 ret = 0;

	if(!EXI_Select(chn,EXI_DEVICE_0,EXI_SPEED32MHZ)) ret |= 0x01;
	if(!EXI_Imm(chn,cmd,sizeof(u16),EXI_READWRITE,NULL)) ret |= 0x02;
	if(!EXI_Sync(chn)) ret |= 0x04;
	if(!EXI_Deselect(chn)) ret |= 0x08;

	if(ret) return 0;
	return 1;
}

static __inline__ int __flashwritecommand(s32 chn, u32 flashaddress, u8 flashdata)
{
	s32 ret = 0;
	u32 val = 0xE0000000|(flashaddress<<9)|(flashdata<<1);

	if (flashaddress > 0x7FFFF)
		return 0;

	if(!EXI_Select(chn,EXI_DEVICE_0,EXI_SPEED16MHZ)) ret |= 0x01;
	if(!EXI_ImmEx(chn,&val,sizeof(u32),EXI_WRITE)) ret |= 0x02;
	if(!EXI_ImmEx(chn,&val,sizeof(u32),EXI_WRITE)) ret |= 0x04;
	if(!EXI_Deselect(chn)) ret |= 0x08;

	if(ret) return 0;
	return 1;
}

static __inline__ int __flashreadcommand(s32 chn, u32 flashaddress, u8 *flashdata)
{
	s32 ret = 0;
	u32 val = 0xF0000000|(flashaddress<<9);

	if (flashaddress > 0x7FFFF)
		return 0;

	if(!EXI_Select(chn,EXI_DEVICE_0,EXI_SPEED16MHZ)) ret |= 0x01;
	if(!EXI_ImmEx(chn,&val,sizeof(u32),EXI_WRITE)) ret |= 0x02;
	if(!EXI_ImmEx(chn,&val,sizeof(u32),EXI_READ)) ret |= 0x04;
	if(!EXI_Deselect(chn)) ret |= 0x08;

	if(ret) return 0;
	*flashdata = val>>23;
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

int __usb_checksend(s32 chn)
{
	s32 ret;
	u16 val;

	val = 0xC000;
	ret = __send_command(chn,&val);
	if(ret==1 && !(val&0x0400)) ret = 0;

	return ret;
}

int __usb_checkrecv(s32 chn)
{
	s32 ret;
	u16 val;

	val = 0xD000;
	ret = __send_command(chn,&val);
	if(ret==1 && !(val&0x0400)) ret = 0;

	return ret;
}

void usb_flush(s32 chn)
{
	char tmp;

	__usbgecko_exi_wait(chn);
	while(__usb_recvbyte(chn,&tmp));
	EXI_Unlock(chn);
}

int usb_isgeckoalive(s32 chn)
{
	u32 id = 0;
	s32 ret;
	u16 val;

	if (EXI_GetID(chn, EXI_DEVICE_0, &id) == 0)
		return 0;

	if (id != 0)
		return 0;

	__usbgecko_exi_wait(chn);

	val = 0x9000;
	ret = __send_command(chn,&val);
	if(ret==1 && !(val&0x0470)) ret = 0;

	EXI_Unlock(chn);

	return ret;
}

int usb_recvbuffer_ex(s32 chn,void *buffer,int size, int retries)
{
	s32 ret;
	s32 left = size;
	char *ptr = (char*)buffer;

	__usbgecko_exi_wait(chn);
	while(left>0) {
		ret = __usb_recvbyte(chn,ptr);
		if(ret==0) break;

		ptr++;
		left--;

		if (retries >= 0) {
			retries--;
			if (retries == 0)
				break;
		}
	}
	EXI_Unlock(chn);

	return (size - left);
}

int usb_recvbuffer(s32 chn,void *buffer,int size) {
	return usb_recvbuffer_ex(chn, buffer, size, -1);
}

int usb_sendbuffer_ex(s32 chn,const void *buffer,int size, int retries)
{
	s32 ret;
	s32 left = size;
	char *ptr = (char*)buffer;

	__usbgecko_exi_wait(chn);
	while(left>0) {
		ret = __usb_sendbyte(chn,*ptr);
		if(ret==0) break;

		ptr++;
		left--;

		if (retries >= 0) {
			retries--;
			if (retries == 0)
				break;
		}
	}
	EXI_Unlock(chn);

	return (size - left);
}

int usb_sendbuffer(s32 chn,const void *buffer,int size) {
	return usb_sendbuffer_ex(chn, buffer, size, -1);
}

int usb_recvbuffer_safe_ex(s32 chn,void *buffer,int size, int retries)
{
	s32 ret;
	s32 left = size;
	char *ptr = (char*)buffer;

	__usbgecko_exi_wait(chn);
	while(left>0) {
		if(__usb_checkrecv(chn)) {
			ret = __usb_recvbyte(chn,ptr);
			if(ret==0) break;

			ptr++;
			left--;
		}

		if (retries >= 0) {
			retries--;
			if (retries == 0)
				break;
		}
	}
	EXI_Unlock(chn);

	return (size - left);
}

int usb_recvbuffer_safe(s32 chn,void *buffer,int size) {
	return usb_recvbuffer_safe_ex(chn, buffer, size, -1);
}

int usb_sendbuffer_safe_ex(s32 chn,const void *buffer,int size, int retries)
{
	s32 ret;
	s32 left = size;
	char *ptr = (char*)buffer;

	__usbgecko_exi_wait(chn);
	while(left>0) {
		if(__usb_checksend(chn)) {
			ret = __usb_sendbyte(chn,*ptr);
			if(ret==0) break;

			ptr++;
			left--;
		}

		if (retries >= 0) {
			retries--;
			if (retries == 0)
				break;
		}
	}
	EXI_Unlock(chn);

	return (size - left);
}

int usb_sendbuffer_safe(s32 chn,const void *buffer,int size) {
	return usb_sendbuffer_safe_ex(chn, buffer, size, -1);
}

static int __flashsoftwareid_entry(s32 chn)
{
	s32 ret=0;

	if (__flashwritecommand(chn, 0x5555, 0xAA) && __flashwritecommand(chn, 0x2AAA, 0x55) &&
	    __flashwritecommand(chn, 0x5555, 0x90))
		ret = 1;

	return ret;
}

static int __flashsoftwareid_exit(s32 chn)
{
	s32 ret=0;

	if (__flashwritecommand(chn, 0x5555, 0xAA) && __flashwritecommand(chn, 0x2AAA, 0x55) &&
	    __flashwritecommand(chn, 0x5555, 0xF0))
		ret = 1;

	return ret;
}

int usb_flashread(s32 chn, u32 offset, void *buffer, size_t length)
{
	s32 ret=1;
	u8 *data = (u8*)buffer;

	__usbgecko_exi_wait(chn);
	while (ret && length--)
		ret = __flashreadcommand(chn, offset++, data++);

	EXI_Unlock(chn);

	return ret;
}

int usb_flashwrite(s32 chn, u32 offset, const void *buffer, size_t length)
{
	s32 ret=1;
	const u8 *data = (const u8*)buffer;
	u8 verify;

	__usbgecko_exi_wait(chn);
	while (ret && length--)
	{
		if (!__flashwritecommand(chn, 0x5555, 0xAA) || !__flashwritecommand(chn, 0x2AAA, 0x55) ||
		    !__flashwritecommand(chn, 0x5555, 0xA0) || !__flashwritecommand(chn, offset, *data))
			ret = 0;
		else
		{
			usleep(20);
			if (!__flashreadcommand(chn, offset++, &verify) || verify != *data++)
				ret = 0;
		}
	}
	EXI_Unlock(chn);

	return ret;
}

int usb_flashverify(s32 chn)
{
	u8 id[2];
	s32 ret=0;

	__usbgecko_exi_wait(chn);

	if (__flashsoftwareid_entry(chn) &&__flashreadcommand(chn, 0, id+0) &&
	    __flashreadcommand(chn, 1, id+1) &&	id[0] == 0xBF && id[1] == 0xD7 &&
	    __flashsoftwareid_exit(chn))
		ret = 1;

	EXI_Unlock(chn);

	return ret;
}
