#ifndef _SICKSAXIS_H_
#define _SICKSAXIS_H_

#include <gccore.h>


#define SS_HEAP_SIZE    4096
#define SS_MAX_DEV      8
#define SS_VENDOR_ID    0x054C
#define SS_PRODUCT_ID   0x0268
#define SS_PAYLOAD_SIZE 49

struct SS_BUTTONS
{
    uint8_t left     : 1;
    uint8_t down     : 1;
    uint8_t right    : 1;
    uint8_t up       : 1;
    uint8_t start    : 1;
    uint8_t R3       : 1;
    uint8_t L3       : 1;
    uint8_t select   : 1;

    uint8_t square   : 1;
    uint8_t cross    : 1;
    uint8_t circle   : 1;
    uint8_t triangle : 1;
    uint8_t R1       : 1;
    uint8_t L1       : 1;
    uint8_t R2       : 1;
    uint8_t L2       : 1;

    uint8_t not_used : 7;
    uint8_t PS       : 1;
};

struct SS_ANALOG
{
    uint8_t x;
    uint8_t y;
};

struct SS_DPAD_SENSITIVE
{
    uint8_t up;
    uint8_t right;
    uint8_t down;
    uint8_t left;
};

struct SS_SHOULDER_SENSITIVE
{
    uint8_t L2;
    uint8_t R2;
    uint8_t L1;
    uint8_t R1;
};

struct SS_BUTTON_SENSITIVE
{
    uint8_t triangle;
    uint8_t circle;
    uint8_t cross;
    uint8_t square;
};

struct SS_MOTION
{
    uint16_t acc_x;
    uint16_t acc_y;
    uint16_t acc_z;
    uint16_t z_gyro;
};

struct SS_GAMEPAD
{
    uint8_t                        hid_data;
    uint8_t                        unk0;
    struct SS_BUTTONS              buttons;
    uint8_t                        unk1;
    struct SS_ANALOG               left_analog;
    struct SS_ANALOG               right_analog;
    uint32_t                       unk2;
    struct SS_DPAD_SENSITIVE       dpad_sens;
    struct SS_SHOULDER_SENSITIVE   shoulder_sens;
    struct SS_BUTTON_SENSITIVE     button_sens;
    uint16_t                       unk3;
    uint8_t                        unk4;
    uint8_t                        status;
    uint8_t                        power_rating;
    uint8_t                        comm_status;
    uint32_t                       unk5;
    uint32_t                       unk6;
    uint8_t                        unk7;
    struct SS_MOTION               motion;
}__attribute__((packed));

struct SS_ATTRIBUTE_RUMBLE
{
    uint8_t duration_right;
    uint8_t power_right;
    uint8_t duration_left;
    uint8_t power_left;
};

struct SS_ATTRIBUTES
{
    struct SS_ATTRIBUTE_RUMBLE rumble;
    int led;
};

typedef void (*ss_usb_callback)(void *usrdata);

struct ss_device {
    struct SS_GAMEPAD pad;
    struct SS_ATTRIBUTES attributes;
    int device_id, fd;
    int connected, enabled, reading;
    ss_usb_callback read_callback;
    ss_usb_callback removal_callback;
    void *read_usrdata, *removal_usrdata;
}__attribute__((aligned(32)));


int ss_init();
int ss_initialize(struct ss_device *dev);
int ss_open(struct ss_device *dev);
int ss_close(struct ss_device *dev);
int ss_is_connected(struct ss_device *dev);

int ss_set_read_cb(struct ss_device *dev, ss_usb_callback cb, void *usrdata);
int ss_set_removal_cb(struct ss_device *dev, ss_usb_callback cb, void *usrdata);

int ss_start_reading(struct ss_device *dev);
int ss_stop_reading(struct ss_device *dev);
int ss_set_led(struct ss_device *dev, int led);
int ss_set_rumble(struct ss_device *dev, uint8_t duration_right, uint8_t power_right, uint8_t duration_left, uint8_t power_left);
int ss_get_bd_address(struct ss_device *dev, uint8_t *mac);
int ss_get_paired_mac(struct ss_device *dev, uint8_t *mac);
int ss_set_paired_mac(struct ss_device *dev, const uint8_t *mac);


#endif
