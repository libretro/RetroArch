/* SD cards in SPI mode, over whatever moves the bytes. */

#ifndef GEKKO_DISK_SDSPI_H
#define GEKKO_DISK_SDSPI_H

#include <stdint.h>

#include <gekko/disk.h>

typedef struct sdspi_bus sdspi_bus;

struct sdspi_bus
{
   void *priv;
   /* Chip select on, at the slow (initialisation) or the fast clock. */
   void (*select)(sdspi_bus *bus, int fast);
   void (*deselect)(sdspi_bus *bus);
   /* Clocks with the card not selected, where the bus can. */
   void (*idle_clocks)(sdspi_bus *bus, unsigned bytes);
   /* Both ways at once: out NULL sends 0xff, in NULL drops. */
   void (*xfer)(sdspi_bus *bus, const uint8_t *out, uint8_t *in,
         uint32_t len);
   /* Whole data blocks, one way; 0 on success. */
   int  (*read)(sdspi_bus *bus, uint8_t *buf, uint32_t len);
   int  (*write)(sdspi_bus *bus, const uint8_t *buf, uint32_t len);
   /* A millisecond clock for the timeouts. */
   uint32_t (*ms)(sdspi_bus *bus);
};

typedef struct sdspi_card
{
   gk_blockdev_t dev;       /* priv points back here */
   sdspi_bus    *bus;
   uint8_t       blocks;    /* addresses are block numbers (SDHC) */
} sdspi_card;

/* Bring the card up; 0 and dev filled in, or a negative errno. */
int sdspi_init(sdspi_card *card, sdspi_bus *bus);

#endif
