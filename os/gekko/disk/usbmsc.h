/* USB mass storage: bulk-only transport and SCSI block commands, over
 * whatever moves the USB transfers. */

#ifndef GEKKO_DISK_USBMSC_H
#define GEKKO_DISK_USBMSC_H

#include <stdint.h>

#include <gekko/disk.h>

typedef struct msc_bus msc_bus;

struct msc_bus
{
   void *priv;
   /* Bytes moved or a negative errno; a stall is any failure. */
   int (*bulk)(msc_bus *bus, uint8_t ep, void *buf, uint32_t len);
   int (*ctrl)(msc_bus *bus, uint8_t type, uint8_t request, uint16_t value,
         uint16_t index, void *buf, uint16_t len);
   uint32_t (*ms)(msc_bus *bus);
   void     (*sleep_ms)(msc_bus *bus, unsigned ms);
   uint32_t max_transfer;   /* per bulk transfer, a multiple of 512 */
};

typedef struct msc_disk
{
   gk_blockdev_t dev;       /* priv points back here */
   msc_bus      *bus;
   uint32_t      tag;
   uint8_t       ep_in;
   uint8_t       ep_out;
   uint8_t       iface;
   uint8_t       lun;
   uint8_t       long_lba;  /* READ(16) and WRITE(16) */
} msc_disk;

/* The first LUN with a medium; 0 and dev filled in, or a negative
 * errno. */
int msc_init(msc_disk *d, msc_bus *bus, uint8_t iface, uint8_t ep_in,
      uint8_t ep_out);

#endif
