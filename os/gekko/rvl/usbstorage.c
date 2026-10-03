/* Wii: USB drives, the mass storage class over /dev/usb/ven. */

#include <errno.h>

#include <gekko/thread.h>
#include <gekko/usb.h>

#include "../disk/usbmsc.h"

#define CLASS_MASS_STORAGE 0x08
#define SUBCLASS_SCSI      0x06
#define PROTOCOL_BULK_ONLY 0x50

static struct
{
   msc_disk   disk;
   msc_bus    bus;
   gk_mutex_t lock;
   uint32_t   id;
   int        open;
   /* Devices looked at that are no drive, while plugged in. */
   uint32_t   skip[GK_USB_MAX_DEVICES];
   int        num_skip;
} drive;

static int bus_bulk(msc_bus *bus, uint8_t ep, void *buf, uint32_t len)
{
   (void)bus;
   return gk_usb_bulk(drive.id, ep, buf, len);
}

static int bus_ctrl(msc_bus *bus, uint8_t type, uint8_t request,
      uint16_t value, uint16_t index, void *buf, uint16_t len)
{
   (void)bus;
   return gk_usb_ctrl(drive.id, type, request, value, index, buf, len);
}

static uint32_t bus_ms(msc_bus *bus)
{
   (void)bus;
   return (uint32_t)(gk_ticks() / (gk_tb_hz / 1000u));
}

static void bus_sleep_ms(msc_bus *bus, unsigned ms)
{
   (void)bus;
   gk_sleep_us((uint64_t)ms * 1000u);
}

/* The bulk-only SCSI interface's endpoints; 0 if it is not one. */
static int endpoints(const gk_usb_info_t *info, uint8_t *in, uint8_t *out)
{
   unsigned i;
   if (info->if_class != CLASS_MASS_STORAGE
         || info->if_subclass != SUBCLASS_SCSI
         || info->if_protocol != PROTOCOL_BULK_ONLY)
      return 0;
   *in = *out = 0;
   for (i = 0; i < info->num_ep; i++)
      if ((info->ep[i].attributes & 3) == 2)
      {
         if (info->ep[i].address & 0x80)
            *in = info->ep[i].address;
         else
            *out = info->ep[i].address;
      }
   return *in && *out;
}

/* Forgets the devices unplugged; an id can come back as another. */
static void prune(const gk_usb_dev_t *devs, int n)
{
   int i, j;
   for (i = 0; i < drive.num_skip; i++)
   {
      for (j = 0; j < n && devs[j].id != drive.skip[i]; j++)
         ;
      if (j == n)
         drive.skip[i--] = drive.skip[--drive.num_skip];
   }
}

static int skipped(uint32_t id)
{
   int i;
   for (i = 0; i < drive.num_skip; i++)
      if (drive.skip[i] == id)
         return 1;
   return 0;
}

static void skip(uint32_t id)
{
   if (drive.num_skip < GK_USB_MAX_DEVICES)
      drive.skip[drive.num_skip++] = id;
}

/* Each device is looked at once while it stays plugged in, so this
 * can be called again to find a drive plugged in later. */
gk_blockdev_t *gk_usbstorage_open(void)
{
   gk_usb_dev_t devs[GK_USB_MAX_DEVICES];
   gk_blockdev_t *dev = NULL;
   int n, i;

   gk_mutex_lock(&drive.lock);
   if (drive.open)
   {
      dev = &drive.disk.dev;
      goto out;
   }
   n = gk_usb_list(devs, GK_USB_MAX_DEVICES);
   prune(devs, n);
   for (i = 0; i < n && !dev; i++)
   {
      gk_usb_info_t info;
      uint8_t in, out;
      if (skipped(devs[i].id))
         continue;
      skip(devs[i].id);
      if (gk_usb_open(devs[i].id, &info))
         continue;
      if (!endpoints(&info, &in, &out))
      {
         gk_usb_close(devs[i].id);
         continue;
      }
      drive.id               = devs[i].id;
      drive.bus.priv         = &drive;
      drive.bus.bulk         = bus_bulk;
      drive.bus.ctrl         = bus_ctrl;
      drive.bus.ms           = bus_ms;
      drive.bus.sleep_ms     = bus_sleep_ms;
      drive.bus.max_transfer = 16 * 1024;
      if (msc_init(&drive.disk, &drive.bus, info.if_number, in, out))
      {
         gk_usb_close(devs[i].id);
         continue;
      }
      drive.num_skip--;
      drive.open = 1;
      dev = &drive.disk.dev;
   }
out:
   gk_mutex_unlock(&drive.lock);
   return dev;
}

int gk_usbstorage_inserted(void)
{
   gk_usb_dev_t devs[GK_USB_MAX_DEVICES];
   int n, i, in = 0;
   gk_mutex_lock(&drive.lock);
   if (drive.open)
   {
      n = gk_usb_list(devs, GK_USB_MAX_DEVICES);
      for (i = 0; i < n && !in; i++)
         in = devs[i].id == drive.id;
   }
   gk_mutex_unlock(&drive.lock);
   return in;
}

void gk_usbstorage_close(void)
{
   gk_mutex_lock(&drive.lock);
   if (drive.open)
      gk_usb_close(drive.id);
   drive.open = 0;
   gk_mutex_unlock(&drive.lock);
}
