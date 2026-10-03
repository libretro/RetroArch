/* SD cards in SPI mode, after the SD Association's Physical Layer
 * Simplified Specification: the initialisation of section 7.2.1, the
 * command and response formats of 7.3 and the data tokens of 7.3.3. */

#include <errno.h>
#include <string.h>

#include "sdspi.h"

#define CMD_GO_IDLE        0
#define CMD_SEND_IF_COND   8
#define CMD_SEND_CSD       9
#define CMD_STOP           12
#define CMD_STATUS         13
#define CMD_BLOCKLEN       16
#define CMD_READ_SINGLE    17
#define CMD_READ_MULTI     18
#define CMD_WRITE_SINGLE   24
#define CMD_WRITE_MULTI    25
#define CMD_APP            55
#define CMD_READ_OCR       58
#define ACMD_OP_COND       41

#define R1_IDLE            0x01
#define R1_ILLEGAL         0x04

#define TOKEN_START        0xfe
#define TOKEN_MULTI_WRITE  0xfc
#define TOKEN_STOP         0xfd

#define OCR_CCS            0x40000000u

#define INIT_MS            1000
#define READ_MS            200
#define BUSY_MS            600

/* ---- bytes ---- */

static uint8_t byte_in(sdspi_bus *bus)
{
   uint8_t b;
   bus->xfer(bus, NULL, &b, 1);
   return b;
}

static void byte_out(sdspi_bus *bus, uint8_t b)
{
   bus->xfer(bus, &b, NULL, 1);
}

/* Until the card stops holding the line low.  0 or -ETIMEDOUT. */
static int wait_ready(sdspi_bus *bus, uint32_t ms)
{
   uint32_t start = bus->ms(bus);
   uint8_t b[4];
   for (;;)
   {
      bus->xfer(bus, NULL, b, sizeof(b));
      if (b[3] == 0xff)
         return 0;
      if (bus->ms(bus) - start > ms)
         return -ETIMEDOUT;
   }
}

static uint8_t crc7(const uint8_t *p, unsigned n)
{
   unsigned crc = 0, i, k;
   for (i = 0; i < n; i++)
      for (k = 0; k < 8; k++)
      {
         unsigned bit = ((p[i] >> (7 - k)) & 1) ^ ((crc >> 6) & 1);
         crc = (crc << 1) & 0x7f;
         if (bit)
            crc ^= 0x09;
      }
   return (uint8_t)crc;
}

/* A command and its R1; 0xff if the card did not answer. */
static uint8_t command(sdspi_bus *bus, uint8_t cmd, uint32_t arg)
{
   uint8_t pkt[6], r1 = 0xff;
   unsigned i;
   if (cmd != CMD_GO_IDLE && cmd != CMD_STOP && wait_ready(bus, BUSY_MS))
      return 0xff;
   pkt[0] = (uint8_t)(0x40 | cmd);
   pkt[1] = (uint8_t)(arg >> 24);
   pkt[2] = (uint8_t)(arg >> 16);
   pkt[3] = (uint8_t)(arg >> 8);
   pkt[4] = (uint8_t)arg;
   pkt[5] = (uint8_t)((crc7(pkt, 5) << 1) | 1);
   bus->xfer(bus, pkt, NULL, sizeof(pkt));
   if (cmd == CMD_STOP)
      byte_in(bus);   /* the stuff byte */
   for (i = 0; i < 10 && (r1 & 0x80); i++)
      r1 = byte_in(bus);
   return r1;
}

/* One command with the card selected around it. */
static uint8_t command1(sdspi_card *c, int fast, uint8_t cmd, uint32_t arg,
      uint8_t *extra, unsigned extra_len)
{
   uint8_t r1;
   c->bus->select(c->bus, fast);
   r1 = command(c->bus, cmd, arg);
   if (extra_len)
      c->bus->xfer(c->bus, NULL, extra, extra_len);
   c->bus->deselect(c->bus);
   return r1;
}

/* The start of a data block: 0, or -EIO on an error token. */
static int wait_token(sdspi_bus *bus)
{
   uint32_t start = bus->ms(bus);
   for (;;)
   {
      uint8_t b = byte_in(bus);
      if (b == TOKEN_START)
         return 0;
      if (b != 0xff)
         return -EIO;
      if (bus->ms(bus) - start > READ_MS)
         return -ETIMEDOUT;
   }
}

/* ---- blocks ---- */

static int read_blocks(sdspi_card *c, uint64_t lba, uint32_t count,
      uint8_t *buf)
{
   sdspi_bus *bus = c->bus;
   uint32_t arg = c->blocks ? (uint32_t)lba : (uint32_t)(lba << 9);
   int multi = count > 1, ret = 0;
   bus->select(bus, 1);
   if (command(bus, multi ? CMD_READ_MULTI : CMD_READ_SINGLE, arg))
      ret = -EIO;
   while (!ret && count--)
   {
      uint8_t crc[2];
      if (!(ret = wait_token(bus)))
      {
         ret = bus->read(bus, buf, 512) ? -EIO : 0;
         bus->xfer(bus, NULL, crc, 2);
      }
      buf += 512;
   }
   if (multi && command(bus, CMD_STOP, 0) & 0xfe)
      ret = ret ? ret : -EIO;
   if (multi && wait_ready(bus, BUSY_MS))
      ret = ret ? ret : -EIO;
   bus->deselect(bus);
   return ret;
}

static int write_blocks(sdspi_card *c, uint64_t lba, uint32_t count,
      const uint8_t *buf)
{
   static const uint8_t no_crc[2] = { 0xff, 0xff };
   sdspi_bus *bus = c->bus;
   uint32_t arg = c->blocks ? (uint32_t)lba : (uint32_t)(lba << 9);
   int multi = count > 1, ret = 0;
   bus->select(bus, 1);
   if (command(bus, multi ? CMD_WRITE_MULTI : CMD_WRITE_SINGLE, arg))
      ret = -EIO;
   while (!ret && count--)
   {
      byte_out(bus, 0xff);
      byte_out(bus, multi ? TOKEN_MULTI_WRITE : TOKEN_START);
      if (bus->write(bus, buf, 512))
         ret = -EIO;
      bus->xfer(bus, no_crc, NULL, 2);
      /* The data response, xxx0sss1: 010 is accepted. */
      if (!ret && (byte_in(bus) & 0x1f) != 0x05)
         ret = -EIO;
      if (!ret)
         ret = wait_ready(bus, BUSY_MS);
      buf += 512;
   }
   if (multi)
   {
      byte_out(bus, TOKEN_STOP);
      byte_in(bus);
      if (wait_ready(bus, BUSY_MS))
         ret = ret ? ret : -EIO;
   }
   bus->deselect(bus);
   if (!ret && !multi)
   {
      /* A single write reports programming errors in the status. */
      uint8_t r2;
      if (command1(c, 1, CMD_STATUS, 0, &r2, 1) || r2)
         ret = -EIO;
   }
   return ret;
}

static int dev_read(gk_blockdev_t *dev, uint64_t lba, uint32_t count,
      void *buf)
{
   sdspi_card *c = (sdspi_card*)dev->priv;
   /* One retry: a transfer can fail on a glitch the next survives. */
   return read_blocks(c, lba, count, (uint8_t*)buf)
      && read_blocks(c, lba, count, (uint8_t*)buf) ? -1 : 0;
}

static int dev_write(gk_blockdev_t *dev, uint64_t lba, uint32_t count,
      const void *buf)
{
   sdspi_card *c = (sdspi_card*)dev->priv;
   return write_blocks(c, lba, count, (const uint8_t*)buf)
      && write_blocks(c, lba, count, (const uint8_t*)buf) ? -1 : 0;
}

/* ---- bringing it up ---- */

static uint64_t csd_sectors(const uint8_t *csd)
{
   if ((csd[0] >> 6) == 1)
   {
      uint32_t size = ((uint32_t)(csd[7] & 0x3f) << 16) | (csd[8] << 8)
         | csd[9];
      return ((uint64_t)size + 1) << 10;
   }
   {
      uint32_t bl_len = csd[5] & 0x0f;
      uint32_t size   = ((uint32_t)(csd[6] & 0x03) << 10) | (csd[7] << 2)
         | (csd[8] >> 6);
      uint32_t mult   = ((csd[9] & 0x03) << 1) | (csd[10] >> 7);
      return ((uint64_t)size + 1) << (mult + 2 + bl_len - 9);
   }
}

int sdspi_init(sdspi_card *c, sdspi_bus *bus)
{
   uint8_t r1, r7[4], csd[16], crc[2];
   uint32_t start;
   unsigned i;
   int v2;

   memset(c, 0, sizeof(*c));
   c->bus = bus;

   /* At least 74 clocks, then CMD0 with chip select puts it in SPI
    * mode. */
   bus->idle_clocks(bus, 10);
   for (i = 0, r1 = 0xff; i < 10 && r1 != R1_IDLE; i++)
      r1 = command1(c, 0, CMD_GO_IDLE, 0, NULL, 0);
   if (r1 != R1_IDLE)
      return -ENODEV;

   /* Version 2 cards echo the check pattern; version 1 does not know
    * the command. */
   r1 = command1(c, 0, CMD_SEND_IF_COND, 0x1aa, r7, 4);
   if (r1 & R1_ILLEGAL)
      v2 = 0;
   else if (r1 == R1_IDLE && (r7[2] & 0x0f) == 0x01 && r7[3] == 0xaa)
      v2 = 1;
   else
      return -ENODEV;

   start = bus->ms(bus);
   do
   {
      r1 = command1(c, 0, CMD_APP, 0, NULL, 0);
      if (!(r1 & ~R1_IDLE))
         r1 = command1(c, 0, ACMD_OP_COND, v2 ? 0x40000000u : 0, NULL, 0);
      if (r1 & ~R1_IDLE)
         return -ENODEV;
   } while (r1 && bus->ms(bus) - start < INIT_MS);
   if (r1)
      return -ETIMEDOUT;

   if (v2)
   {
      uint8_t ocr[4];
      if (command1(c, 0, CMD_READ_OCR, 0, ocr, 4))
         return -EIO;
      c->blocks = (ocr[0] & (OCR_CCS >> 24)) != 0;
   }
   if (!c->blocks && command1(c, 0, CMD_BLOCKLEN, 512, NULL, 0))
      return -EIO;

   bus->select(bus, 1);
   r1 = command(bus, CMD_SEND_CSD, 0);
   if (!r1 && !wait_token(bus))
   {
      bus->xfer(bus, NULL, csd, sizeof(csd));
      bus->xfer(bus, NULL, crc, sizeof(crc));
   }
   else
      r1 = 0xff;
   bus->deselect(bus);
   if (r1)
      return -EIO;

   c->dev.sectors     = csd_sectors(csd);
   c->dev.sector_size = 512;
   c->dev.priv        = c;
   c->dev.read        = dev_read;
   c->dev.write       = dev_write;
   return 0;
}
