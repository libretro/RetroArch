/****************************************************************************
 * Copyright (C) 2015
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/
#ifndef __SYSHID_FUNCTIONS_H_
#define __SYSHID_FUNCTIONS_H_

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned int syshid_handle;

typedef struct
{
	unsigned int handle;
	unsigned int physical_device_inst;
	unsigned short vid;
	unsigned short pid;
	unsigned char interface_index;
	unsigned char sub_class;
	unsigned char protocol;

	unsigned short max_packet_size_rx;
	unsigned short max_packet_size_tx;

} HIDDevice;

typedef struct _HIDClient HIDClient;

#define HID_DEVICE_DETACH	0
#define HID_DEVICE_ATTACH	1

typedef int (*HIDAttachCallback)(HIDClient *p_hc,HIDDevice *p_hd,unsigned int attach);

struct _HIDClient
{
	HIDClient *next;
	HIDAttachCallback attach_cb;
};

typedef void (*HIDCallback)(unsigned int handle,int error,unsigned char *p_buffer,unsigned int bytes_transferred,void *p_user);

void InitSysHIDFunctionPointers(void);
void InitAcquireSysHID(void);

extern int(*HIDSetup)(void);
extern int(*HIDTeardown)(void);

extern int(*HIDAddClient)(HIDClient *p_client, HIDAttachCallback attach_callback);
extern int(*HIDDelClient)(HIDClient *p_client);

extern int(*HIDGetDescriptor)(unsigned int handle,u8 descriptor_type,u8 descriptor_index, u16 language_id, unsigned char *p_buffer, unsigned int buffer_length, HIDCallback hc, void *p_user);
extern int(*HIDSetDescriptor)(unsigned int handle,u8 descriptor_type,u8 descriptor_index, u16 language_id, unsigned char *p_buffer, unsigned int buffer_length, HIDCallback hc, void *p_user);

extern int(*HIDGetReport)(u32 handle, u8 report_type, u8 report_id, u8 *p_buffer, u32 buffer_length, HIDCallback hc, void *p_user);
extern int(*HIDSetReport)(u32 handle, u8 report_type, u8 report_id, u8 *p_buffer, u32 buffer_length, HIDCallback hc, void *p_user);

extern int(*HIDSetIdle)(unsigned int handle, u8 interface_index,u8 duration, HIDCallback hc, void *p_user);

extern int(* HIDSetProtocol)(unsigned int handle,u8 interface_index,u8 protocol, HIDCallback hc, void *p_user);
extern int(* HIDGetProtocol)(unsigned int handle,u8 interface_index,u8 * protocol, HIDCallback hc, void *p_user);

extern int(*HIDRead)(unsigned int handle, unsigned char *p_buffer, unsigned int buffer_length, HIDCallback hc, void *p_user);
extern int(*HIDWrite)(unsigned int handle, unsigned char *p_buffer, unsigned int buffer_length, HIDCallback hc, void *p_user);

#ifdef __cplusplus
}
#endif

#endif // __SYSHID_FUNCTIONS_H_
