#ifndef __USBGECKO_H___
#define __USBGECKO_H___

#include <gctypes.h>

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

void usb_flush(s32 chn);
int usb_isgeckoalive(s32 chn);
int usb_recvbuffer(s32 chn,void *buffer,int size);
int usb_sendbuffer(s32 chn,const void *buffer,int size);
int usb_recvbuffer_safe(s32 chn,void *buffer,int size);
int usb_sendbuffer_safe(s32 chn,const void *buffer,int size);
int usb_recvbuffer_ex(s32 chn,void *buffer,int size, int retries);
int usb_sendbuffer_ex(s32 chn,const void *buffer,int size, int retries);
int usb_recvbuffer_safe_ex(s32 chn,void *buffer,int size, int retries);
int usb_sendbuffer_safe_ex(s32 chn,const void *buffer,int size, int retries);
int usb_flashread(s32 chn, u32 offset, void *buffer, size_t length);
int usb_flashwrite(s32 chn, u32 offset, const void *buffer, size_t length);
int usb_flashverify(s32 chn);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
