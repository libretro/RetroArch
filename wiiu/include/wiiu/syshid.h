#pragma once
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct
{
    u32 handle;
    u32 physical_device_inst;
    u16 vid;
    u16 pid;
    u8 interface_index;
    u8 sub_class;
    u8 protocol;

    u16 max_packet_size_rx;
    u16 max_packet_size_tx;

} HIDDevice;

#define HID_DEVICE_DETACH   0
#define HID_DEVICE_ATTACH   1

typedef struct _HIDClient HIDClient;

typedef s32 (*HIDAttachCallback)(HIDClient *p_hc,HIDDevice *p_hd,u32 attach);

struct _HIDClient
{
    HIDClient *next;
    HIDAttachCallback attach_cb;
};

typedef void  HIDCallback(u32 handle,s32 error,u8 *p_buffer,u32 bytes_transferred,void *p_user);

s32 HIDSetup(void);
s32 HIDTeardown(void);

s32 HIDAddClient(HIDClient *p_client, HIDAttachCallback attach_callback);
s32 HIDDelClient(HIDClient *p_client);

s32 HIDGetDescriptor(u32 handle,u8 descriptor_type,u8 descriptor_index, u16 language_id, u8 *p_buffer, u32 buffer_length, HIDCallback hc, void *p_user);
s32 HIDSetDescriptor(u32 handle,u8 descriptor_type,u8 descriptor_index, u16 language_id, u8 *p_buffer, u32 buffer_length, HIDCallback hc, void *p_user);

s32 HIDGetReport(u32 handle, u8 report_type, u8 report_id, u8 *p_buffer, u32 buffer_length, HIDCallback hc, void *p_user);
s32 HIDSetReport(u32 handle, u8 report_type, u8 report_id, u8 *p_buffer, u32 buffer_length, HIDCallback hc, void *p_user);

s32 HIDSetIdle(u32 handle, u8 interface_index,u8 duration, HIDCallback hc, void *p_user);

s32 HIDSetProtocol(u32 handle,u8 interface_index,u8 protocol, HIDCallback hc, void *p_user);
s32 HIDGetProtocol(u32 handle,u8 interface_index,u8 * protocol, HIDCallback hc, void *p_user);

s32 HIDRead(u32 handle, u8 *p_buffer, u32 buffer_length, HIDCallback hc, void *p_user);
s32 HIDWrite(u32 handle, u8 *p_buffer, u32 buffer_length, HIDCallback hc, void *p_user);

#ifdef __cplusplus
}
#endif
