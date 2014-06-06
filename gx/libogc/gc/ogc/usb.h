#ifndef __USB_H__
#define __USB_H__

#if defined(HW_RVL)

#include <gcutil.h>
#include <gctypes.h>

#define USB_MAXPATH						IPC_MAXPATH_LEN

#define USB_OK							0
#define USB_FAILED						1

#define USB_CLASS_HID					0x03
#define USB_SUBCLASS_BOOT				0x01
#define USB_PROTOCOL_KEYBOARD			0x01
#define USB_PROTOCOL_MOUSE				0x02

#define USB_REPTYPE_INPUT				0x01
#define USB_REPTYPE_OUTPUT				0x02
#define USB_REPTYPE_FEATURE				0x03

/* Descriptor types */
#define USB_DT_DEVICE					0x01
#define USB_DT_CONFIG					0x02
#define USB_DT_STRING					0x03
#define USB_DT_INTERFACE				0x04
#define USB_DT_ENDPOINT					0x05
#define USB_DT_DEVICE_QUALIFIER         0x06
#define USB_DT_OTHER_SPEED_CONFIG       0x07
#define USB_DT_INTERFACE_POWER          0x08
#define USB_DT_OTG                      0x09
#define USB_DT_DEBUG                    0x10
#define USB_DT_INTERFACE_ASSOCIATION    0x11
#define USB_DT_HID						0x21
#define USB_DT_REPORT					0x22
#define USB_DT_PHYSICAL					0x23
#define USB_DT_CLASS_SPECIFIC_INTERFACE 0x24
#define USB_DT_CLASS_SPECIFIC_ENDPOINT  0x25
#define USB_DT_HUB                      0x29

/* Standard requests */
#define USB_REQ_GETSTATUS				0x00
#define USB_REQ_CLEARFEATURE			0x01
#define USB_REQ_SETFEATURE				0x03
#define USB_REQ_SETADDRESS				0x05
#define USB_REQ_GETDESCRIPTOR			0x06
#define USB_REQ_SETDESCRIPTOR			0x07
#define USB_REQ_GETCONFIG				0x08
#define USB_REQ_SETCONFIG				0x09
#define USB_REQ_GETINTERFACE			0x0A
#define USB_REQ_SETINTERFACE			0x0B
#define USB_REQ_SYNCFRAME				0x0C

#define USB_REQ_GETREPORT				0x01
#define USB_REQ_GETIDLE					0x02
#define USB_REQ_GETPROTOCOL				0x03
#define USB_REQ_SETREPORT				0x09
#define USB_REQ_SETIDLE					0x0A
#define USB_REQ_SETPROTOCOL				0x0B

/* Descriptor sizes per descriptor type */
#define USB_DT_DEVICE_SIZE				18
#define USB_DT_CONFIG_SIZE				9
#define USB_DT_INTERFACE_SIZE			9
#define USB_DT_ENDPOINT_SIZE			7
#define USB_DT_ENDPOINT_AUDIO_SIZE		9	/* Audio extension */
#define USB_DT_HID_SIZE					9
#define USB_DT_HUB_NONVAR_SIZE			7

/* control message request type bitmask */
#define USB_CTRLTYPE_DIR_HOST2DEVICE	(0<<7)
#define USB_CTRLTYPE_DIR_DEVICE2HOST	(1<<7)
#define USB_CTRLTYPE_TYPE_STANDARD		(0<<5)
#define USB_CTRLTYPE_TYPE_CLASS			(1<<5)
#define USB_CTRLTYPE_TYPE_VENDOR		(2<<5)
#define USB_CTRLTYPE_TYPE_RESERVED		(3<<5)
#define USB_CTRLTYPE_REC_DEVICE			0
#define USB_CTRLTYPE_REC_INTERFACE		1
#define USB_CTRLTYPE_REC_ENDPOINT		2
#define USB_CTRLTYPE_REC_OTHER			3

#define USB_REQTYPE_INTERFACE_GET		(USB_CTRLTYPE_DIR_DEVICE2HOST|USB_CTRLTYPE_TYPE_CLASS|USB_CTRLTYPE_REC_INTERFACE)
#define USB_REQTYPE_INTERFACE_SET		(USB_CTRLTYPE_DIR_HOST2DEVICE|USB_CTRLTYPE_TYPE_CLASS|USB_CTRLTYPE_REC_INTERFACE)
#define USB_REQTYPE_ENDPOINT_GET		(USB_CTRLTYPE_DIR_DEVICE2HOST|USB_CTRLTYPE_TYPE_CLASS|USB_CTRLTYPE_REC_ENDPOINT)
#define USB_REQTYPE_ENDPOINT_SET		(USB_CTRLTYPE_DIR_HOST2DEVICE|USB_CTRLTYPE_TYPE_CLASS|USB_CTRLTYPE_REC_ENDPOINT)

#define USB_FEATURE_ENDPOINT_HALT		0

#define USB_ENDPOINT_INTERRUPT			0x03
#define USB_ENDPOINT_IN					0x80
#define USB_ENDPOINT_OUT				0x00

#define USB_OH0_DEVICE_ID				0x00000000				// for completion
#define USB_OH1_DEVICE_ID				0x00200000

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct _usbendpointdesc
{
	u8 bLength;
	u8 bDescriptorType;
	u8 bEndpointAddress;
	u8 bmAttributes;
	u16 wMaxPacketSize;
	u8 bInterval;
} ATTRIBUTE_PACKED usb_endpointdesc;

typedef struct _usbinterfacedesc
{
	u8 bLength;
	u8 bDescriptorType;
	u8 bInterfaceNumber;
	u8 bAlternateSetting;
	u8 bNumEndpoints;
	u8 bInterfaceClass;
	u8 bInterfaceSubClass;
	u8 bInterfaceProtocol;
	u8 iInterface;
	u8 *extra;
	u16 extra_size;
	struct _usbendpointdesc *endpoints;
} ATTRIBUTE_PACKED usb_interfacedesc;

typedef struct _usbconfdesc
{
	u8 bLength;
	u8 bDescriptorType;
	u16 wTotalLength;
	u8 bNumInterfaces;
	u8 bConfigurationValue;
	u8 iConfiguration;
	u8 bmAttributes;
	u8 bMaxPower;
	struct _usbinterfacedesc *interfaces;
} ATTRIBUTE_PACKED usb_configurationdesc;

typedef struct _usbdevdesc
{
	u8  bLength;
	u8  bDescriptorType;
	u16 bcdUSB;
	u8  bDeviceClass;
	u8  bDeviceSubClass;
	u8  bDeviceProtocol;
	u8  bMaxPacketSize0;
	u16 idVendor;
	u16 idProduct;
	u16 bcdDevice;
	u8  iManufacturer;
	u8  iProduct;
	u8  iSerialNumber;
	u8  bNumConfigurations;
	struct _usbconfdesc *configurations;
} ATTRIBUTE_PACKED usb_devdesc;

typedef struct _usbhiddesc
{
	u8 bLength;
	u8 bDescriptorType;
	u16 bcdHID;
	u8 bCountryCode;
	u8 bNumDescriptors;
	struct {
		u8 bDescriptorType;
		u16 wDescriptorLength;
	} ATTRIBUTE_PACKED descr[1];
} ATTRIBUTE_PACKED usb_hiddesc;

typedef struct _usb_device_entry {
	s32 device_id;
	u16 vid;
	u16 pid;
	u32 token;
} usb_device_entry;

typedef s32 (*usbcallback)(s32 result,void *usrdata);

s32 USB_Initialize();
s32 USB_Deinitialize();

s32 USB_OpenDevice(s32 device_id,u16 vid,u16 pid,s32 *fd);
s32 USB_CloseDevice(s32 *fd);
s32 USB_CloseDeviceAsync(s32 *fd,usbcallback cb,void *usrdata);

s32 USB_GetDescriptors(s32 fd, usb_devdesc *udd);
void USB_FreeDescriptors(usb_devdesc *udd);

s32 USB_GetGenericDescriptor(s32 fd,u8 type,u8 index,u8 interface,void *data,u32 size);
s32 USB_GetHIDDescriptor(s32 fd,u8 interface,usb_hiddesc *uhd,u32 size);

s32 USB_GetDeviceDescription(s32 fd,usb_devdesc *devdesc);
s32 USB_DeviceRemovalNotifyAsync(s32 fd,usbcallback cb,void *userdata);
s32 USB_DeviceChangeNotifyAsync(u8 interface_class,usbcallback cb,void *userdata);

s32 USB_SuspendDevice(s32 fd);
s32 USB_ResumeDevice(s32 fd);

s32 USB_ReadIsoMsg(s32 fd,u8 bEndpoint,u8 bPackets,u16 *rpPacketSizes,void *rpData);
s32 USB_ReadIsoMsgAsync(s32 fd,u8 bEndpoint,u8 bPackets,u16 *rpPacketSizes,void *rpData,usbcallback cb,void *userdata);

s32 USB_ReadIntrMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData);
s32 USB_ReadIntrMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,usbcallback cb,void *usrdata);

s32 USB_ReadBlkMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData);
s32 USB_ReadBlkMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,usbcallback cb,void *usrdata);

s32 USB_ReadCtrlMsg(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData);
s32 USB_ReadCtrlMsgAsync(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData,usbcallback cb,void *usrdata);

s32 USB_WriteIsoMsg(s32 fd,u8 bEndpoint,u8 bPackets,u16 *rpPacketSizes,void *rpData);
s32 USB_WriteIsoMsgAsync(s32 fd,u8 bEndpoint,u8 bPackets,u16 *rpPacketSizes,void *rpData,usbcallback cb,void *userdata);

s32 USB_WriteIntrMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData);
s32 USB_WriteIntrMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,usbcallback cb,void *usrdata);

s32 USB_WriteBlkMsg(s32 fd,u8 bEndpoint,u16 wLength,void *rpData);
s32 USB_WriteBlkMsgAsync(s32 fd,u8 bEndpoint,u16 wLength,void *rpData,usbcallback cb,void *usrdata);

s32 USB_WriteCtrlMsg(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData);
s32 USB_WriteCtrlMsgAsync(s32 fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData,usbcallback cb,void *usrdata);

s32 USB_GetConfiguration(s32 fd, u8 *configuration);
s32 USB_SetConfiguration(s32 fd, u8 configuration);
s32 USB_SetAlternativeInterface(s32 fd, u8 interface, u8 alternateSetting);
s32 USB_ClearHalt(s32 fd, u8 endpointAddress);
s32 USB_GetDeviceList(usb_device_entry *descr_buffer,u8 num_descr,u8 interface_class,u8 *cnt_descr);

s32 USB_GetAsciiString(s32 fd,u8 bIndex,u16 wLangID,u16 wLength,void *rpData);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif /* defined(HW_RVL) */

#endif
