/*-------------------------------------------------------------

usbkeyboard.c -- Usb keyboard support(boot protocol)

Copyright (C) 2008, 2009
DAVY Guillaume davyg2@gmail.com
dhewg

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#include <string.h>
#include <malloc.h>

#include <gccore.h>
#include <ogc/usb.h>

#include <wiikeyboard/usbkeyboard.h>

#define	HEAP_SIZE					4096
#define DEVLIST_MAXSIZE				8
#define KEY_ERROR					0x01
#define MAXKEYCODE					6

#define USB_MOD_CTRL_L				0x01
#define USB_MOD_SHIFT_L				0x02
#define USB_MOD_ALT_L				0x04
#define USB_MOD_META_L				0x08
#define USB_MOD_CTRL_R				0x10
#define USB_MOD_SHIFT_R				0x20
#define USB_MOD_ALT_R				0x40
#define USB_MOD_META_R				0x80

struct ukbd_data {
	u16	modifiers;
	u8	keycode[MAXKEYCODE];
} ATTRIBUTE_PACKED;

struct ukbd {
	bool connected;

	s32 fd;

	struct ukbd_data sc_ndata;
	struct ukbd_data sc_odata;

	u8 leds;

	eventcallback cb;

	u8 configuration;
	u32 interface;
	u32 altInterface;

	u8 ep;
	u32 ep_size;
};

static s32 hId = -1;
static struct ukbd *_kbd = NULL;

static u8 _ukbd_mod_map[][2] = {
	{ USB_MOD_CTRL_L, 224 },
	{ USB_MOD_SHIFT_L, 225 },
	{ USB_MOD_ALT_L, 226 },
	{ USB_MOD_META_L, 227 },
	{ USB_MOD_CTRL_R, 228 },
	{ USB_MOD_SHIFT_R, 229 },
	{ USB_MOD_ALT_R, 230 },
	{ USB_MOD_META_R, 231 }
};

#define MODMAPSIZE (sizeof(_ukbd_mod_map)/sizeof(_ukbd_mod_map[0]))

static void _submit(USBKeyboard_eventType type, u8 code)
{
	if (!_kbd->cb)
		return;

	USBKeyboard_event ev;
	ev.type = type;
	ev.keyCode = code;

	_kbd->cb(ev);
}

//Callback when the keyboard is disconnected
static s32 _disconnect(s32 retval, void *data)
{
	(void) data;

	_kbd->connected = false;

	_submit(USBKEYBOARD_DISCONNECTED, 0);

	return 1;
}

//Get the protocol, 0=boot protocol and 1=report protocol
static s32 _get_protocol(void)
{
	s32 protocol;
	u8 *buffer = 0;

	if(!_kbd || _kbd->fd==-1) return -1;

	buffer = iosAlloc(hId, 1);

	if (buffer == NULL)
		return -1;

	USB_WriteCtrlMsg(_kbd->fd, USB_REQTYPE_INTERFACE_GET, USB_REQ_GETPROTOCOL, 0, _kbd->interface, 1, buffer);

	protocol = *buffer;
	iosFree(hId, buffer);

	return protocol;
}

//Modify the protocol, 0=boot protocol and 1=report protocol
static s32 _set_protocol(u8 protocol)
{
	if(!_kbd || _kbd->fd==-1) return -1;
	return USB_WriteCtrlMsg(_kbd->fd, USB_REQTYPE_INTERFACE_SET, USB_REQ_SETPROTOCOL, protocol, _kbd->interface, 0, NULL);
}

//Get an input report from interrupt pipe
static s32 _get_input_report(void)
{
	u8 *buffer = 0;

	if(!_kbd || _kbd->fd==-1) return -1;
	buffer = iosAlloc(hId, 8);

	if (buffer == NULL)
		return -1;

	s32 ret = USB_ReadIntrMsg(_kbd->fd, _kbd->ep, 8, buffer);

	memcpy(&_kbd->sc_ndata, buffer, 8);
	iosFree(hId, buffer);

	_kbd->sc_ndata.modifiers = (_kbd->sc_ndata.modifiers << 8) | (_kbd->sc_ndata.modifiers >> 8);

	return ret;
}

#if 0
//Get an input report from control pipe
static s32 _get_output_report(u8 *leds)
{
	u8 *buffer = 0;
	if(!_kbd || _kbd->fd==-1) return -1;
	buffer = iosAlloc(hId, 1);

	if (buffer == NULL)
		return -1;

	s32 ret = USB_WriteCtrlMsg(_kbd->fd, USB_REQTYPE_INTERFACE_GET, USB_REQ_GETREPORT, USB_REPTYPE_OUTPUT << 8, _kbd->interface, 1, buffer);

	memcpy(leds, buffer, 1);
	iosFree(hId, buffer);

	return ret;
}
#endif

//Set an input report to control pipe
static s32 _set_output_report(void)
{
	u8 *buffer = 0;
	if(!_kbd || _kbd->fd==-1) return -1;
	buffer = iosAlloc(hId, 1);

	if (buffer == NULL)
		return -1;

	memcpy(buffer, &_kbd->leds, 1);
	s32 ret = USB_WriteCtrlMsg(_kbd->fd, USB_REQTYPE_INTERFACE_SET, USB_REQ_SETREPORT, USB_REPTYPE_OUTPUT << 8, _kbd->interface, 1, buffer);

	iosFree(hId, buffer);

	return ret;
}

//init the ioheap
s32 USBKeyboard_Initialize(void)
{
	if (hId > 0)
		return 0;

	hId = iosCreateHeap(HEAP_SIZE);

	if (hId < 0)
		return IPC_ENOHEAP;

	return IPC_OK;
}

s32 USBKeyboard_Deinitialize(void)
{
	return IPC_OK;
}

//Search for a keyboard connected to the wii usb port
//Thanks to Sven Peter usbstorage support
s32 USBKeyboard_Open(const eventcallback cb)
{
	usb_device_entry *buffer;
	u8 device_count, i, conf;
	u16 vid, pid;
	bool found = false;
	u32 iConf, iInterface, iEp;
	usb_devdesc udd;
	usb_configurationdesc *ucd;
	usb_interfacedesc *uid;
	usb_endpointdesc *ued;

	buffer = (usb_device_entry*)iosAlloc(hId, DEVLIST_MAXSIZE * sizeof(usb_device_entry));
	if(buffer == NULL)
		return -1;

	memset(buffer, 0, DEVLIST_MAXSIZE * sizeof(usb_device_entry));

	if (USB_GetDeviceList(buffer, DEVLIST_MAXSIZE, USB_CLASS_HID, &device_count) < 0)
	{
		iosFree(hId, buffer);
		return -2;
	}

	if (_kbd) {
		if (_kbd->fd != -1) USB_CloseDevice(&_kbd->fd);
	} else {
		_kbd = (struct ukbd *) malloc(sizeof(struct ukbd));

		if (!_kbd)
			return -1;
	}

	memset(_kbd, 0, sizeof(struct ukbd));
	_kbd->fd = -1;

	for (i = 0; i < device_count; i++)
	{
		vid = buffer[i].vid;;
		pid = buffer[i].pid;

		if ((vid == 0) || (pid == 0))
			continue;

		s32 fd = 0;
		if (USB_OpenDevice(buffer[i].device_id, vid, pid, &fd) < 0)
			continue;

		if (USB_GetDescriptors(fd, &udd) < 0) {
			USB_CloseDevice(&fd);
			continue;
		}

		for(iConf = 0; iConf < udd.bNumConfigurations; iConf++)
		{
			ucd = &udd.configurations[iConf];

			for(iInterface = 0; iInterface < ucd->bNumInterfaces; iInterface++)
			{
				uid = &ucd->interfaces[iInterface];

				if ((uid->bInterfaceClass == USB_CLASS_HID) &&
					(uid->bInterfaceSubClass == USB_SUBCLASS_BOOT) &&
					(uid->bInterfaceProtocol== USB_PROTOCOL_KEYBOARD))
				{
					for(iEp = 0; iEp < uid->bNumEndpoints; iEp++)
					{
						ued = &uid->endpoints[iEp];

						if (ued->bmAttributes != USB_ENDPOINT_INTERRUPT)
							continue;

						if (!(ued->bEndpointAddress & USB_ENDPOINT_IN))
							continue;

						_kbd->fd = fd;
						_kbd->cb = cb;

						_kbd->configuration = ucd->bConfigurationValue;
						_kbd->interface = uid->bInterfaceNumber;
						_kbd->altInterface = uid->bAlternateSetting;

						_kbd->ep = ued->bEndpointAddress;
						_kbd->ep_size = ued->wMaxPacketSize;

						found = true;

						break;
					}
				}

				if (found)
					break;
			}

			if (found)
				break;
		}

		USB_FreeDescriptors(&udd);

		if (found)
			break;
		else
			USB_CloseDevice(&fd);
	}

	iosFree(hId, buffer);

	if (!found)
		return -3;

	if (USB_GetConfiguration(_kbd->fd, &conf) < 0)
	{
		USBKeyboard_Close();
		return -4;
	}

	if (conf != _kbd->configuration &&
		USB_SetConfiguration(_kbd->fd, _kbd->configuration) < 0)
	{
		USBKeyboard_Close();
		return -5;
	}

	if (_kbd->altInterface != 0 &&
		USB_SetAlternativeInterface(_kbd->fd, _kbd->interface, _kbd->altInterface) < 0)
	{
		USBKeyboard_Close();
		return -6;
	}

	if (_get_protocol() != 0)
	{
		if (_set_protocol(0) < 0)
		{
			USBKeyboard_Close();
			return -6;
		}

		if (_get_protocol() == 1)
		{
			USBKeyboard_Close();
			return -7;
		}
	}

	if (USB_DeviceRemovalNotifyAsync(_kbd->fd, &_disconnect, NULL) < 0)
	{
		USBKeyboard_Close();
		return -8;
	}

	_kbd->connected = true;

	return 1;
}

//Close the device
void USBKeyboard_Close(void)
{
	if (!_kbd)
		return;

	if(_kbd->fd != -1)
		USB_CloseDevice(&_kbd->fd);

	free(_kbd);
	_kbd = NULL;

	return;
}

bool USBKeyboard_IsConnected(void) {
	if (!_kbd)
		return false;

	return _kbd->connected;
}

//Scan for key presses and generate events for the callback function
s32 USBKeyboard_Scan(void)
{
	int i, j, index;

	if (!_kbd)
		return -1;

	if (_get_input_report() < 0)
		return -2;

	if (_kbd->sc_ndata.keycode[0] == KEY_ERROR)
		return 0;

	if (_kbd->sc_ndata.modifiers != _kbd->sc_odata.modifiers) {
		for (i = 0; i < MODMAPSIZE; ++i) {
			if ((_kbd->sc_odata.modifiers & _ukbd_mod_map[i][0])
				&& !(_kbd->sc_ndata.modifiers & _ukbd_mod_map[i][0]))
				_submit(USBKEYBOARD_RELEASED, _ukbd_mod_map[i][1]);
			else if ((_kbd->sc_ndata.modifiers & _ukbd_mod_map[i][0])
				&& !(_kbd->sc_odata.modifiers & _ukbd_mod_map[i][0]))
				_submit(USBKEYBOARD_PRESSED, _ukbd_mod_map[i][1]);
		}
	}

	for (i = 0; i < MAXKEYCODE; i++) {
		if (_kbd->sc_odata.keycode[i] > 3) {
			index = -1;

			for (j = 0; j < MAXKEYCODE; j++) {
				if (_kbd->sc_odata.keycode[i] == _kbd->sc_ndata.keycode[j]) {
					index = j;
					break;
				}
			}

			if (index == -1)
				_submit(USBKEYBOARD_RELEASED, _kbd->sc_odata.keycode[i]);
		}

		if (_kbd->sc_ndata.keycode[i] > 3) {
			index = -1;

			for (j = 0; j < MAXKEYCODE; j++) {
				if (_kbd->sc_ndata.keycode[i] == _kbd->sc_odata.keycode[j]) {
					index = j;
					break;
				}
			}

			if (index == -1)
				_submit(USBKEYBOARD_PRESSED, _kbd->sc_ndata.keycode[i]);
		}
	}

	_kbd->sc_odata = _kbd->sc_ndata;

	return 0;
}

//Turn on/off a led
s32 USBKeyboard_SetLed(const USBKeyboard_led led, bool on)
{
	if (!_kbd)
		return -1;

	if (on)
		_kbd->leds = _kbd->leds | (1 << led );
	else
		_kbd->leds = _kbd->leds & (255 ^ (1 << led));

	if (_set_output_report() < 0)
		return -2;

	return 1;
}

//Toggle a led
s32 USBKeyboard_ToggleLed(const USBKeyboard_led led)
{
	if (!_kbd)
		return -1;

	_kbd->leds = _kbd->leds ^ (1 << led);

	if (_set_output_report() < 0)
		return -2;

	return 1;
}
