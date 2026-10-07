/* Wii: USB devices through IOS 57 and later: HID-class interfaces
 * through /dev/usb/hid, the rest through /dev/usb/ven.
 *
 * IOS lists each interface of a device as a device of its own, by an
 * id; the calls below take ids from either list.  Transfers block
 * until they finish; their data can be anywhere (it is copied through
 * memory IOS can reach), at most 65535 bytes. */

#ifndef GEKKO_USB_H
#define GEKKO_USB_H

#include <gekko/disk.h>
#include <gekko/gekko.h>

#define GK_USB_MAX_DEVICES   32
#define GK_USB_MAX_ENDPOINTS 16

typedef struct gk_usb_dev
{
   uint32_t id;
   uint16_t vid;
   uint16_t pid;
   uint8_t  interface;
   uint8_t  alt_settings;
} gk_usb_dev_t;

typedef struct gk_usb_ep
{
   uint16_t max_packet;
   uint8_t  address;        /* bit 7 set: device to host */
   uint8_t  attributes;     /* bits 0-1: 0 control, 1 iso, 2 bulk, 3 intr */
   uint8_t  interval;
} gk_usb_ep_t;

typedef struct gk_usb_info
{
   gk_usb_ep_t ep[GK_USB_MAX_ENDPOINTS];
   uint16_t    vid;
   uint16_t    pid;
   uint8_t     dev_class;
   uint8_t     dev_subclass;
   uint8_t     dev_protocol;
   uint8_t     if_number;
   uint8_t     if_class;
   uint8_t     if_subclass;
   uint8_t     if_protocol;
   uint8_t     num_ep;
} gk_usb_info_t;

/* Whether the running IOS has the USB interfaces these calls use. */
int  gk_usb_supported(void);

/* The devices plugged in now; how many were written. */
int  gk_usb_list(gk_usb_dev_t *out, int max);
/* The HID-class ones: their description has the interrupt IN and OUT
 * endpoints only, and only control and interrupt transfers reach them
 * (no alternate settings). */
int  gk_usb_hid_list(gk_usb_dev_t *out, int max);
/* Wake a device and describe the interface; 0 or a negative errno. */
int  gk_usb_open(uint32_t id, gk_usb_info_t *info);
void gk_usb_close(uint32_t id);
/* Switch the interface to an alternate setting and describe that. */
int  gk_usb_set_alt(uint32_t id, uint8_t alt, gk_usb_info_t *info);

/* Bytes moved, or a negative errno (-EPIPE: the endpoint stalled or
 * the transfer failed on the wire). */
int  gk_usb_ctrl(uint32_t id, uint8_t type, uint8_t request, uint16_t value,
      uint16_t index, void *data, uint16_t len);
int  gk_usb_bulk(uint32_t id, uint8_t ep, void *data, uint32_t len);
int  gk_usb_intr(uint32_t id, uint8_t ep, void *data, uint32_t len);
/* Make transfers waiting on an endpoint return. */
int  gk_usb_cancel(uint32_t id, uint8_t ep);

/* Mass storage: the first USB drive's first LUN with a medium, or
 * NULL.  gk_usbstorage_inserted: the drive open is still plugged in. */
gk_blockdev_t *gk_usbstorage_open(void);
void           gk_usbstorage_close(void);
int            gk_usbstorage_inserted(void);

#endif
