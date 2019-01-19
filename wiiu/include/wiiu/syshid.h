#pragma once
#include <wiiu/types.h>

typedef struct
{
    uint32_t handle;
    uint32_t physical_device_inst;
    uint16_t vid;
    uint16_t pid;
    uint8_t interface_index;
    uint8_t sub_class;
    uint8_t protocol;

    uint16_t max_packet_size_rx;
    uint16_t max_packet_size_tx;

} HIDDevice;

typedef struct _HIDClient HIDClient;

#define HID_DEVICE_DETACH   0
#define HID_DEVICE_ATTACH   1

typedef int32_t (*HIDAttachCallback)(HIDClient *p_hc,HIDDevice *p_hd,uint32_t attach);

struct _HIDClient
{
    HIDClient *next;
    HIDAttachCallback attach_cb;
};

typedef void (*HIDCallback)(uint32_t handle,int32_t error,uint8_t *p_buffer,uint32_t bytes_transferred,void *p_user);

#ifdef __cplusplus
extern "C" {
#endif

int32_t
HIDSetup(void);

int32_t
HIDTeardown(void);

int32_t
HIDAddClient(HIDClient *p_client,
             HIDAttachCallback attach_callback);
int32_t
HIDDelClient(HIDClient *p_client);

int32_t
HIDGetDescriptor(uint32_t handle,
                 uint8_t descriptor_type,
                 uint8_t descriptor_index,
                 uint16_t language_id,
                 uint8_t *p_buffer,
                 uint32_t buffer_length,
                 HIDCallback hc,
                 void *p_user);

int32_t
HIDSetDescriptor(uint32_t handle,
                 uint8_t descriptor_type,
                 uint8_t descriptor_index,
                 uint16_t language_id,
                 uint8_t *p_buffer,
                 uint32_t buffer_length,
                 HIDCallback hc,
                 void *p_user);

int32_t
HIDGetReport(uint32_t handle,
             uint8_t report_type,
             uint8_t report_id,
             uint8_t *p_buffer,
             uint32_t buffer_length,
             HIDCallback hc,
             void *p_user);

int32_t
HIDSetReport(uint32_t handle,
             uint8_t report_type,
             uint8_t report_id,
             uint8_t *p_buffer,
             uint32_t buffer_length,
             HIDCallback hc,
             void *p_user);

int32_t
HIDSetIdle(uint32_t handle,
           uint8_t interface_index,
           uint8_t duration,
           HIDCallback hc,
           void *p_user);

int32_t
HIDSetProtocol(uint32_t handle,
               uint8_t interface_index,
               uint8_t protocol,
               HIDCallback hc,
               void *p_user);

int32_t
HIDGetProtocol(uint32_t handle,
               uint8_t interface_index,
               uint8_t * protocol,
               HIDCallback hc,
               void *p_user);

int32_t
HIDRead(uint32_t handle,
        uint8_t *p_buffer,
        uint32_t buffer_length,
        HIDCallback hc,
        void *p_user);

int32_t
HIDWrite(uint32_t handle,
         uint8_t *p_buffer,
         uint32_t buffer_length,
         HIDCallback hc,
         void *p_user);

#ifdef __cplusplus
}
#endif
