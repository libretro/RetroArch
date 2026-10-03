/* Block devices and the FAT volumes mounted on them. */

#ifndef GEKKO_DISK_H
#define GEKKO_DISK_H

#include <stddef.h>
#include <stdint.h>

typedef struct gk_blockdev gk_blockdev_t;

struct gk_blockdev
{
   uint64_t sectors;
   void    *priv;
   /* Whole sectors; buf is any alignment.  0 on success. */
   int (*read)(gk_blockdev_t *dev, uint64_t lba, uint32_t count, void *buf);
   int (*write)(gk_blockdev_t *dev, uint64_t lba, uint32_t count,
         const void *buf);
   uint32_t sector_size;
};

/* Mount the first FAT partition (or the whole device) as "name:" for
 * the C library's file calls.  0 on success, a negative errno else. */
int  gk_fat_mount(const char *name, gk_blockdev_t *dev);
void gk_fat_unmount(const char *name);
/* Write everything cached to the device. */
int  gk_fat_sync(const char *name);

/* An SD card adapter on the external interface: memory card slot A
 * (0) or B (1), or serial port 2 (2).  NULL without a card. */
gk_blockdev_t *gk_sdgecko_open(unsigned ch);
void           gk_sdgecko_close(unsigned ch);

#ifdef HW_RVL
/* The front SD slot; NULL without a card.  gk_sd_inserted: the card
 * open is still in the slot. */
gk_blockdev_t *gk_sd_open(void);
void           gk_sd_close(void);
int            gk_sd_inserted(void);
#endif

#endif
