#ifndef __DEBUG_IF_H__
#define __DEBUG_IF_H__

#include <gctypes.h>

#define GDBSTUB_DEVICE_USB		0
#define GDBSTUB_DEVICE_TCP		1

struct dbginterface
{
	s32 fhndl;

	int (*open)(struct dbginterface *device);
	int (*close)(struct dbginterface *device);
	int (*wait)(struct dbginterface *device);
	int (*read)(struct dbginterface *device,void *buffer,int size);
	int (*write)(struct dbginterface *devicec,const void *buffer,int size);
};

#endif
