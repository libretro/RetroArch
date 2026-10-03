/* SD card adapters on the external interface: the memory card slots
 * (channels 0 and 1) and serial port 2 (channel 2), device 0, as an
 * SPI bus.  Blocks move by DMA, through a buffer in MEM1 when the
 * caller's is not aligned or not in MEM1. */

#include <errno.h>
#include <string.h>

#include <gekko/disk.h>
#include <gekko/exi.h>
#include <gekko/thread.h>
#include <gekko/usbgecko.h>

#include "sdspi.h"

#define MEM1_LIMIT 0x01800000u

struct slot
{
   uint8_t    bounce[512] __attribute__((aligned(32)));
   sdspi_card card;
   sdspi_bus  bus;
   unsigned   ch;
   int        open;
};

static struct slot slots[GK_EXI_CHANNELS];

static void bus_select(sdspi_bus *bus, int fast)
{
   struct slot *s = (struct slot*)bus->priv;
   gk_exi_lock(s->ch);
   /* 16 MHz is as fast as the adapters are known to run. */
   gk_exi_select(s->ch, 0, fast ? GK_EXI_16MHZ : GK_EXI_1MHZ);
}

static void bus_deselect(sdspi_bus *bus)
{
   struct slot *s = (struct slot*)bus->priv;
   gk_exi_deselect(s->ch);
   gk_exi_unlock(s->ch);
}

static void bus_idle_clocks(sdspi_bus *bus, unsigned bytes)
{
   struct slot *s = (struct slot*)bus->priv;
   gk_exi_lock(s->ch);
   /* Slots 1 and 2 wire only device 0, so device 2 clocks with the
    * card's select high; slot 0's other devices are the clock chip and
    * serial port 1, so there the card is selected for them. */
   gk_exi_select(s->ch, s->ch ? 2 : 0, GK_EXI_1MHZ);
   gk_exi_imm_bytes(s->ch, NULL, NULL, bytes);
   gk_exi_deselect(s->ch);
   gk_exi_unlock(s->ch);
}

static void bus_xfer(sdspi_bus *bus, const uint8_t *out, uint8_t *in,
      uint32_t len)
{
   gk_exi_imm_bytes(((struct slot*)bus->priv)->ch, out, in, len);
}

static int direct(const void *buf)
{
   return !((uintptr_t)buf & 31) && GK_PHYS(buf) < MEM1_LIMIT;
}

static int bus_read(sdspi_bus *bus, uint8_t *buf, uint32_t len)
{
   struct slot *s = (struct slot*)bus->priv;
   if (direct(buf))
      return gk_exi_dma(s->ch, buf, len, GK_EXI_READ);
   if (gk_exi_dma(s->ch, s->bounce, len, GK_EXI_READ))
      return -1;
   memcpy(buf, s->bounce, len);
   return 0;
}

static int bus_write(sdspi_bus *bus, const uint8_t *buf, uint32_t len)
{
   struct slot *s = (struct slot*)bus->priv;
   if (direct(buf))
      return gk_exi_dma(s->ch, (void*)buf, len, GK_EXI_WRITE);
   memcpy(s->bounce, buf, len);
   return gk_exi_dma(s->ch, s->bounce, len, GK_EXI_WRITE);
}

static uint32_t bus_ms(sdspi_bus *bus)
{
   (void)bus;
   return (uint32_t)(gk_ticks() / (gk_tb_hz / 1000u));
}

gk_blockdev_t *gk_sdgecko_open(unsigned ch)
{
   struct slot *s;
   if (ch >= GK_EXI_CHANNELS)
      return NULL;
   s = &slots[ch];
   if (s->open)
      return &s->card.dev;
   if (ch < 2 && (!gk_exi_attached(ch)
            || (int)ch == gk_usbgecko_channel()))
      return NULL;
   s->ch              = ch;
   s->bus.priv        = s;
   s->bus.select      = bus_select;
   s->bus.deselect    = bus_deselect;
   s->bus.idle_clocks = bus_idle_clocks;
   s->bus.xfer        = bus_xfer;
   s->bus.read        = bus_read;
   s->bus.write       = bus_write;
   s->bus.ms          = bus_ms;
   if (sdspi_init(&s->card, &s->bus))
      return NULL;
   s->open = 1;
   return &s->card.dev;
}

void gk_sdgecko_close(unsigned ch)
{
   if (ch < GK_EXI_CHANNELS)
      slots[ch].open = 0;
}
