#ifndef __PHYSBUSIF_H__
#define __PHYSBUSIF_H__

#include "btpbuf.h"

typedef struct _usb_p usb_p;
typedef s32 (*pbcallback)(s32 state,s32 result,usb_p *usb);

struct _usb_p
{
	s32 fd;
	u8 acl_out;
	u8 acl_in;
	u8 hci_evt;
	u8 hci_ctrl;
	u32 vid;
	u32 pid;
	u8 openstate;
	pbcallback closecb;
	pbcallback unregcb;
};

void physbusif_init();
void physbusif_close();
void physbusif_shutdown();
void physbusif_reset_all();
void physbusif_output(struct pbuf *p,u16_t len);

#endif
