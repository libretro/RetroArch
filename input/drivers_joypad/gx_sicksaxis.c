#include <gccore.h>
#include <string.h>

#include "gx_sicksaxis.h"

static uint8_t ATTRIBUTE_ALIGN(32) _ss_attributes[] =
{
    0x00,
	0x00, 0x00, 0x00, 0x00, //Rumble
	0x00, 0x00, //Gyro
	0x00, 0x00,
	0x00, //* LED_1 = 0x02, LED_2 = 0x04, ... */
	0xFF, 0x27, 0x10, 0x00, 0x32, /* LED_4 */
    0xFF, 0x27, 0x10, 0x00, 0x32, /* LED_3 */
	0xFF, 0x27, 0x10, 0x00, 0x32, /* LED_2 */
	0xFF, 0x27, 0x10, 0x00, 0x32, /* LED_1 */
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00,
};

static const uint8_t _ss_led_pattern[] = {0x0, 0x02, 0x04, 0x08, 0x10, 0x12, 0x14, 0x18};

static int _ss_inited = 0;
static int _dev_detected = 0;
static int _slots = 0;
static int _ss_rem_cb = 0; /* helps to know if it has just removed a device from the usb iface. */
struct ss_device *_ss_dev_list = NULL; /* just hold a pointer to the dev list. */

static int _ss_initialize(struct ss_device *dev);
int _ss_open(struct ss_device *dev);
int _ss_close(struct ss_device *dev);
static int _ss_dev_id_list_exists(int id);
static int _ss_removal_cb(int result, void *usrdata);
static int _ss_change_cb(int result, void *usrdata);
static int _ss_send_attributes_payload(struct ss_device *dev);
static int _ss_set_operational(struct ss_device *dev);

int ss_init(struct ss_device *dev_list, int slots) {
	if (!_ss_inited) {
		_ss_dev_list = dev_list;
		_slots = slots;

		int i;
		for (i = 0;i < _slots; i++) {
			_ss_initialize(&_ss_dev_list[i]);
		}

		USB_DeviceChangeNotifyAsync(USB_CLASS_HID, _ss_change_cb, NULL);
		_dev_detected = 1; /* try open any existing sixasis device */
		_ss_inited = 1;
    }
    return _ss_inited;
}

int ss_shutdown() {
	int i;
	for (i = 0;i < _slots; i++)	{
		_ss_close(&_ss_dev_list[i]);
	}

	_ss_inited = 0;

	return 1;
}

static int _ss_initialize(struct ss_device *dev) {
    dev->device_id = -1;
    dev->connected = 0;
    dev->enabled   = 0;
    memset(&dev->pad, 0x0, sizeof(struct SS_GAMEPAD));
    memset(&dev->attributes, 0x0, sizeof(struct SS_ATTRIBUTES));
    return 1;
}

static int _ss_change_cb(int result, void *usrdata) {
	if (!_ss_rem_cb) {
		/* As it's not coming from the removal callback
		then we	detected a new device being inserted */
		_dev_detected = 1;
	}
	else {
		_ss_rem_cb = 0;
	}

	/* Re-apply the callback notification for future connections changes*/
	USB_DeviceChangeNotifyAsync(USB_CLASS_HID, _ss_change_cb, NULL);

	return 1;
}

int _ss_open(struct ss_device *dev) {
	/* always try to close the device first */
	_ss_close(dev);

    usb_device_entry dev_entry[SS_MAX_DEV];
    unsigned char dev_count = 0;
	if (USB_GetDeviceList(dev_entry, SS_MAX_DEV, USB_CLASS_HID, &dev_count) < 0) {
		return -2;
	}

    int i;
	for (i = 0; i < dev_count; ++i)	{
        if ((dev_entry[i].vid == SS_VENDOR_ID) && (dev_entry[i].pid == SS_PRODUCT_ID)) {
			if (!_ss_dev_id_list_exists(dev_entry[i].device_id)) {
				int fd;
				if (USB_OpenDevice(dev_entry[i].device_id, SS_VENDOR_ID, SS_PRODUCT_ID, &fd) < 0) {
					return -3;
                }

                dev->device_id = dev_entry[i].device_id;
                dev->connected = 1;
				dev->fd = fd;

                _ss_set_operational(dev);
                USB_DeviceRemovalNotifyAsync(dev->fd, &_ss_removal_cb, dev);
                return 1;
            }
        }
    }
    return -4;
}

int _ss_close(struct ss_device *dev) {
    if (dev && dev->fd > 0)	{
        USB_CloseDevice(&dev->fd);
		dev->fd = -1; /* Clear its descriptor */
    }
    return 1;
}

int ss_is_ready(struct ss_device *dev) {

	/* if a device is detected, try to connect it. */
	if (_dev_detected) {
		/* As we are processing the detected device, we turn off the flag. */
		_dev_detected = 0;
		int i;
		for(i = 0; i < _slots; i++)	{
			if (_ss_dev_list[i].device_id < 0) { /* found an empty slot? */
				if (_ss_open(&_ss_dev_list[i]) > 0) {
					ss_set_led(&_ss_dev_list[i], i+1);
				}
				break;
			}
		}
	}

	if (dev->connected && dev->enabled)
		return 1;

	return 0;
}

static int _ss_send_attributes_payload(struct ss_device *dev) {
	_ss_attributes[1] = dev->attributes.rumble.duration_right;
    _ss_attributes[2] = dev->attributes.rumble.power_right;
    _ss_attributes[3] = dev->attributes.rumble.duration_left;
    _ss_attributes[4] = dev->attributes.rumble.power_left;
    _ss_attributes[9] = _ss_led_pattern[dev->attributes.led];

    return USB_WriteCtrlMsg(
				dev->fd,
				USB_REQTYPE_INTERFACE_SET,
				USB_REQ_SETREPORT,
				(USB_REPTYPE_OUTPUT<<8) | 0x01,
				0x0,
				sizeof(_ss_attributes),
				_ss_attributes
			);
}

int ss_set_led(struct ss_device *dev, int led) {
    /* Need to clear the data for rumble */
	dev->attributes.rumble.duration_right = 0;
    dev->attributes.rumble.power_right    = 0;
    dev->attributes.rumble.duration_left  = 0;
    dev->attributes.rumble.power_left     = 0;

	dev->attributes.led = led;

	return _ss_send_attributes_payload(dev);
}

int ss_set_rumble(struct ss_device *dev, uint8_t duration_right, uint8_t power_right, uint8_t duration_left, uint8_t power_left) {
    dev->attributes.rumble.duration_right = duration_right;
    dev->attributes.rumble.power_right    = power_right;
    dev->attributes.rumble.duration_left  = duration_left;
    dev->attributes.rumble.power_left     = power_left;
    return _ss_send_attributes_payload(dev);
}

inline int ss_read_pad(struct ss_device *dev) {
	return USB_ReadIntrMsg(dev->fd, 0x81, SS_PAYLOAD_SIZE, (u8 *)&dev->pad);
}

static int _ss_removal_cb(int result, void *usrdata) {
    struct ss_device *dev = (struct ss_device*)usrdata;
    if (dev->device_id > 0)	{
		_ss_initialize(dev);
		_ss_rem_cb = 1; /* inform we already pass thru the removal callback */
    }

	return 1;
}

static int _ss_set_operational(struct ss_device *dev) {
	int r;
	uint8_t ATTRIBUTE_ALIGN(32) buf[4] = {0x42, 0x0c, 0x00, 0x00}; /* Special command to enable Sixaxis */
	/* Sometimes it fails so we should keep trying until success */
	do {
		r = USB_WriteCtrlMsg(
				dev->fd,
				USB_REQTYPE_INTERFACE_SET,
				USB_REQ_SETREPORT,
				(USB_REPTYPE_FEATURE<<8) | 0xf4,
				0x0,
				sizeof(buf),
				buf
			);
	} while (r < 0);

	dev->enabled = 1;
	return 1;
}

static int _ss_dev_id_list_exists(int id) {
    int i;
	for (i = 0; i < _slots; ++i) {
        if (_ss_dev_list[i].device_id == id) return 1;
    }
    return 0;
}
