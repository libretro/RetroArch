#include "sicksaxis.h"
#include <gccore.h>
#include <stdio.h>
#include <string.h>

static uint8_t ATTRIBUTE_ALIGN(32) _ss_attributes_payload[] =
{
    0x52,
    0x00, 0x00, 0x00, 0x00, //Rumble
    0xff, 0x80, //Gyro
    0x00, 0x00,
    0x00, //* LED_1 = 0x02, LED_2 = 0x04, ... */
    0xff, 0x27, 0x10, 0x00, 0x32, /* LED_4 */
    0xff, 0x27, 0x10, 0x00, 0x32, /* LED_3 */
    0xff, 0x27, 0x10, 0x00, 0x32, /* LED_2 */
    0xff, 0x27, 0x10, 0x00, 0x32, /* LED_1 */
};

static const uint8_t _ss_led_pattern[] = {0x0, 0x02, 0x04, 0x08, 0x10, 0x12, 0x14, 0x18};

static int _ss_heap_id = -1;
static int _ss_inited = 0;
static int _ss_dev_number = 1;
static int _ss_dev_id_list[SS_MAX_DEV] = {0};

static int _ss_dev_id_list_exists(int id);
static int _ss_dev_id_list_add(int id);
static int _ss_dev_id_list_remove(int id);
static int _ss_removal_cb(int result, void *usrdata);
static int _ss_read_cb(int result, void *usrdata);
static int _ss_operational_cb(int result, void *usrdata);
static int _ss_read(struct ss_device *dev);
static int _ss_set_operational(struct ss_device *dev);
static int _ss_build_attributes_payload(struct ss_device *dev);
static int _ss_send_attributes_payload(struct ss_device *dev);

int ss_init()
{
    if (!_ss_inited) {
        _ss_heap_id = iosCreateHeap(SS_HEAP_SIZE);
        _ss_inited = 1;
    }
    return 1;
}


int ss_initialize(struct ss_device *dev)
{
    dev->device_id = -1;
    dev->fd        = -1;
    dev->connected = 0;
    dev->enabled   = 0;
    dev->reading   = 0;
    dev->removal_callback = NULL;
    dev->removal_usrdata  = NULL;
    dev->read_callback    = NULL;
    dev->read_usrdata     = NULL;
    memset(&dev->pad, 0x0, sizeof(struct SS_GAMEPAD));
    memset(&dev->attributes, 0x0, sizeof(struct SS_ATTRIBUTES));
    return 1;
}

int ss_open(struct ss_device *dev)
{
    if (!_ss_inited) return -1;
    if (dev->connected) ss_close(dev);
    
    usb_device_entry dev_entry[8];
    unsigned char dev_count;
    if (USB_GetDeviceList(dev_entry, 8, USB_CLASS_HID, &dev_count) < 0) {
        return -2;
    }
        
    int i;
    for (i = 0; i < dev_count; ++i) {
        
        if ((dev_entry[i].vid == SS_VENDOR_ID) && (dev_entry[i].pid == SS_PRODUCT_ID)) {
            
            if (!_ss_dev_id_list_exists(dev_entry[i].device_id)) {
                if (USB_OpenDevice(dev_entry[i].device_id, SS_VENDOR_ID, SS_PRODUCT_ID, &dev->fd) < 0) {
                    return -3;
                }
                
                dev->device_id = dev_entry[i].device_id;
                dev->connected = 1;
                dev->enabled = 0;
                dev->reading = 0;
                
                _ss_set_operational(dev);
                ss_set_led(dev, _ss_dev_number);
                
                _ss_dev_id_list_add(dev_entry[i].device_id);
                _ss_dev_number++;
                
                USB_DeviceRemovalNotifyAsync(dev->fd, &_ss_removal_cb, dev);
                return 1;
            }
        }
    }
    return -4;
}

int ss_close(struct ss_device *dev)
{
    if (dev && dev->fd > 0) {
        USB_CloseDevice(&dev->fd);
    }
    return 1;
}

int ss_is_connected(struct ss_device *dev)
{
    return dev->connected;
}

int ss_set_read_cb(struct ss_device *dev,ss_usb_callback cb, void *usrdata)
{
    dev->read_callback = cb;
    dev->read_usrdata  = usrdata;
    return 1;
}

int ss_set_removal_cb(struct ss_device *dev, ss_usb_callback cb, void *usrdata)
{
    dev->removal_callback = cb;
    dev->removal_usrdata  = usrdata;
    return 1;
}

int ss_start_reading(struct ss_device *dev)
{
    if (dev) {
        dev->reading = 1;
        if (dev->enabled) {
            _ss_read(dev);
        }
        return 1;
    }
    return 0;
}


int ss_stop_reading(struct ss_device *dev)
{
    if (dev) {
        dev->reading = 0;
        return 1;
    }
    return 0;
}

static int _ss_build_attributes_payload(struct ss_device *dev)
{
    _ss_attributes_payload[1] = dev->attributes.rumble.duration_right;
    _ss_attributes_payload[2] = dev->attributes.rumble.power_right;
    _ss_attributes_payload[3] = dev->attributes.rumble.duration_left;
    _ss_attributes_payload[4] = dev->attributes.rumble.power_left;
    _ss_attributes_payload[9] = _ss_led_pattern[dev->attributes.led];
    return 1;   
}

static int _ss_send_attributes_payload(struct ss_device *dev)
{
    if (!dev->connected) return 0;
    _ss_build_attributes_payload(dev);
    return USB_WriteCtrlMsgAsync(dev->fd,
        USB_REQTYPE_INTERFACE_SET,
        USB_REQ_SETREPORT,
        (USB_REPTYPE_OUTPUT<<8) | 0x01,
        0x0,
        sizeof(_ss_attributes_payload),
        _ss_attributes_payload,
        NULL, NULL);  
}

inline int ss_set_led(struct ss_device *dev, int led)
{
    dev->attributes.led = led;
    return _ss_send_attributes_payload(dev);																
}

inline int ss_set_rumble(struct ss_device *dev, uint8_t duration_right, uint8_t power_right, uint8_t duration_left, uint8_t power_left)
{
    dev->attributes.rumble.duration_right = duration_right;
    dev->attributes.rumble.power_right    = power_right;
    dev->attributes.rumble.duration_left  = duration_left;
    dev->attributes.rumble.power_left     = power_left;
    return _ss_send_attributes_payload(dev);
}

int ss_get_bd_address(struct ss_device *dev, uint8_t *mac)
{
    uint8_t ATTRIBUTE_ALIGN(32) msg[17];
    int ret = USB_WriteCtrlMsgAsync(dev->fd,
                USB_REQTYPE_INTERFACE_GET,
                USB_REQ_GETREPORT,
                (USB_REPTYPE_FEATURE<<8) | 0xf2,
                0,
                sizeof(msg),
                msg,
                NULL, NULL);

    mac[0] = msg[4];
    mac[1] = msg[5];
    mac[2] = msg[6];
    mac[3] = msg[7];
    mac[4] = msg[8];
    mac[5] = msg[9];
    return ret;
}

int ss_get_mac(struct ss_device *dev, uint8_t *mac)
{
    uint8_t ATTRIBUTE_ALIGN(32) msg[8];
    int ret = USB_WriteCtrlMsgAsync(dev->fd,
                USB_REQTYPE_INTERFACE_GET,
                USB_REQ_GETREPORT,
                (USB_REPTYPE_FEATURE<<8) | 0xf5,
                0,
                sizeof(msg),
                msg,
                NULL, NULL);

    mac[0] = msg[2];
    mac[1] = msg[3];
    mac[2] = msg[4];
    mac[3] = msg[5];
    mac[4] = msg[6];
    mac[5] = msg[7];
    return ret;
}

int ss_set_mac(struct ss_device *dev, const uint8_t *mac)
{
    uint8_t ATTRIBUTE_ALIGN(32) msg[] = {0x01, 0x00, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]};
    int ret = USB_WriteCtrlMsgAsync(dev->fd,
                USB_REQTYPE_INTERFACE_SET,
                USB_REQ_SETREPORT,
                (USB_REPTYPE_FEATURE<<8) | 0xf5,
                0,
                sizeof(msg),
                msg,
                NULL, NULL);
    return ret;
}


static int _ss_read(struct ss_device *dev)
{
    return USB_WriteCtrlMsgAsync(
            dev->fd,
            USB_REQTYPE_INTERFACE_GET,
            USB_REQ_GETREPORT,
            (USB_REPTYPE_INPUT<<8) | 0x01,
            0x0,
            SS_PAYLOAD_SIZE,
            &dev->pad,
            &_ss_read_cb,
            dev);
}

static int _ss_removal_cb(int result, void *usrdata)
{
    struct ss_device *dev = (struct ss_device*)usrdata;
    if (dev->device_id > 0) {
        _ss_dev_id_list_remove(dev->device_id);
        _ss_dev_number--;
        if (dev->removal_callback)
            dev->removal_callback(dev->removal_usrdata);
        ss_initialize(dev);
        return 1;
    }
    return 0;
}

static int _ss_set_operational(struct ss_device *dev)
{
    uint8_t ATTRIBUTE_ALIGN(32) buf[17];
    return USB_WriteCtrlMsgAsync(
            dev->fd,
            USB_REQTYPE_INTERFACE_GET,
            USB_REQ_GETREPORT,
            (USB_REPTYPE_FEATURE<<8) | 0xf2,
            0x0,
            17,
            buf,
            &_ss_operational_cb,
            dev);
}

static int _ss_read_cb(int result, void *usrdata)
{
    if (usrdata) {
        struct ss_device *dev = (struct ss_device*)usrdata;
        if (dev->reading) {
            _ss_read(dev);
        if (dev->read_callback)
            dev->read_callback(dev->read_usrdata);
        }
    }
    return 1;
}

static int _ss_operational_cb(int result, void *usrdata)
{
    struct ss_device *dev = (struct ss_device*)usrdata;
    dev->enabled = 1;
    if (dev->reading) {
        _ss_read(dev);   
    }
    return 1;
}


static int _ss_dev_id_list_exists(int id)
{
    int i;
    for (i = 0; i < SS_MAX_DEV; ++i) {
        if (_ss_dev_id_list[i] == id) return 1;
    }
    return 0;
}

static int _ss_dev_id_list_add(int id)
{
    int i;
    for (i = 0; i < SS_MAX_DEV; ++i) {
        if (_ss_dev_id_list[i] == 0) {
           _ss_dev_id_list[i] = id;
           return 1; 
        }
    }
    return 0; 
}

static int _ss_dev_id_list_remove(int id)
{
    int i;
    for (i = 0; i < SS_MAX_DEV; ++i) {
        if (_ss_dev_id_list[i] == id) {
           _ss_dev_id_list[i] = 0;
           return 1; 
        }
    }
    return 0; 
}
