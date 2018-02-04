#ifndef __USBSTORAGE_H__
#define __USBSTORAGE_H__

#if defined(HW_RVL)

#include <gctypes.h>
#include <ogc/mutex.h>
#include <ogc/disc_io.h>
#include <ogc/system.h>

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

#define	USBSTORAGE_OK			0
#define	USBSTORAGE_ENOINTERFACE		-10000
#define	USBSTORAGE_ESENSE		-10001
#define	USBSTORAGE_ESHORTWRITE		-10002
#define	USBSTORAGE_ESHORTREAD		-10003
#define	USBSTORAGE_ESIGNATURE		-10004
#define	USBSTORAGE_ETAG			-10005
#define	USBSTORAGE_ESTATUS		-10006
#define	USBSTORAGE_EDATARESIDUE		-10007
#define	USBSTORAGE_ETIMEDOUT		-10008
#define	USBSTORAGE_EINIT		-10009
#define USBSTORAGE_PROCESSING	-10010

typedef struct
{
	u8 configuration;
	u32 interface;
	u32 altInterface;
	u8 bInterfaceSubClass;

	u8 ep_in;
	u8 ep_out;

	u8 max_lun;
	u32 *sector_size;

	s32 usb_fd;

	mutex_t lock;
	syswd_t alarm;
	s32 retval;

	u32 tag;
	u8 suspended;

	u8 *buffer;
} usbstorage_handle;

#define B_RAW_DEVICE_DATA_IN 0x01
#define B_RAW_DEVICE_COMMAND 0

typedef struct {
   uint8_t         command[16];
   uint8_t         command_length;
   uint8_t         flags;
   uint8_t         scsi_status;
   void*           data;
   size_t          data_length;
} raw_device_command;

s32 USBStorage_Initialize();

s32 USBStorage_Open(usbstorage_handle *dev, s32 device_id, u16 vid, u16 pid);
s32 USBStorage_Close(usbstorage_handle *dev);
s32 USBStorage_Reset(usbstorage_handle *dev);

s32 USBStorage_GetMaxLUN(usbstorage_handle *dev);
s32 USBStorage_MountLUN(usbstorage_handle *dev, u8 lun);
s32 USBStorage_Suspend(usbstorage_handle *dev);
s32 USBStorage_IsDVD();
s32 USBStorage_ioctl(int request, ...);

s32 USBStorage_ReadCapacity(usbstorage_handle *dev, u8 lun, u32 *sector_size, u32 *n_sectors);
s32 USBStorage_Read(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, u8 *buffer);
s32 USBStorage_Write(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, const u8 *buffer);
s32 USBStorage_StartStop(usbstorage_handle *dev, u8 lun, u8 lo_ej, u8 start, u8 imm);

#define DEVICE_TYPE_WII_USB (('W'<<24)|('U'<<16)|('S'<<8)|'B')

extern DISC_INTERFACE __io_usbstorage;

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif /* HW_RVL */

#endif /* __USBSTORAGE_H__ */
