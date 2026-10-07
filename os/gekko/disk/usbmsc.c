/* USB mass storage after the USB Mass Storage Class Bulk-Only
 * Transport specification (1.0): a command block wrapper out, the data,
 * a status wrapper in, and reset recovery when the exchange goes
 * wrong; the commands are SCSI's block commands. */

#include <errno.h>
#include <string.h>

#include "usbmsc.h"

#define CBW_SIG       0x43425355u
#define CSW_SIG       0x53425355u
#define CBW_LEN       31
#define CSW_LEN       13

#define CSW_PASSED    0
#define CSW_FAILED    1

#define REQ_RESET     0xff
#define REQ_MAX_LUN   0xfe

#define SCSI_TUR      0x00
#define SCSI_SENSE    0x03
#define SCSI_INQUIRY  0x12
#define SCSI_CAP10    0x25
#define SCSI_READ10   0x28
#define SCSI_WRITE10  0x2a
#define SCSI_READ16   0x88
#define SCSI_WRITE16  0x8a
#define SCSI_CAP16    0x9e

#define SENSE_NOT_READY  0x02
#define ASC_NO_MEDIUM    0x3a

#define READY_MS      10000
#define CMD_BYTES     (128 * 1024)

static void wr32le(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)v;
   p[1] = (uint8_t)(v >> 8);
   p[2] = (uint8_t)(v >> 16);
   p[3] = (uint8_t)(v >> 24);
}

static uint32_t rd32le(const uint8_t *p)
{
   return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t rd32be(const uint8_t *p)
{
   return ((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static void clear_halt(msc_disk *d, uint8_t ep)
{
   /* CLEAR_FEATURE(ENDPOINT_HALT) to the endpoint. */
   d->bus->ctrl(d->bus, 0x02, 0x01, 0, ep, NULL, 0);
}

static void reset_recovery(msc_disk *d)
{
   d->bus->ctrl(d->bus, 0x21, REQ_RESET, 0, d->iface, NULL, 0);
   clear_halt(d, d->ep_in);
   clear_halt(d, d->ep_out);
}

/* One command.  0 passed, 1 failed (the sense says why), or a negative
 * errno after the transport was reset.  *moved: data bytes moved. */
static int command(msc_disk *d, const uint8_t *cdb, unsigned cdb_len,
      void *buf, uint32_t len, int to_host, uint32_t *moved)
{
   msc_bus *bus = d->bus;
   uint8_t cbw[CBW_LEN], csw[CSW_LEN];
   uint32_t done = 0, tag = ++d->tag;
   uint8_t ep = to_host ? d->ep_in : d->ep_out;
   int n, tries;

   memset(cbw, 0, sizeof(cbw));
   wr32le(cbw, CBW_SIG);
   wr32le(cbw + 4, tag);
   wr32le(cbw + 8, len);
   cbw[12] = to_host ? 0x80 : 0x00;
   cbw[13] = d->lun;
   cbw[14] = (uint8_t)cdb_len;
   memcpy(cbw + 15, cdb, cdb_len);
   if (moved)
      *moved = 0;
   if (bus->bulk(bus, d->ep_out, cbw, CBW_LEN) != CBW_LEN)
   {
      reset_recovery(d);
      return -EIO;
   }

   while (done < len)
   {
      uint32_t chunk = len - done < bus->max_transfer ? len - done
         : bus->max_transfer;
      n = bus->bulk(bus, ep, (uint8_t*)buf + done, chunk);
      if (n < 0)
      {
         /* A stalled data stage still ends with the status. */
         clear_halt(d, ep);
         break;
      }
      done += (uint32_t)n;
      if ((uint32_t)n < chunk)
         break;
   }

   for (tries = 0; tries < 2; tries++)
   {
      n = bus->bulk(bus, d->ep_in, csw, CSW_LEN);
      if (n >= 0)
         break;
      clear_halt(d, d->ep_in);
   }
   if (n != CSW_LEN || rd32le(csw) != CSW_SIG || rd32le(csw + 4) != tag
         || csw[12] > CSW_FAILED)
   {
      reset_recovery(d);
      return -EIO;
   }
   if (moved)
      *moved = done;
   return csw[12] == CSW_PASSED ? 0 : 1;
}

/* A command retried while the exchange itself breaks. */
static int command3(msc_disk *d, const uint8_t *cdb, unsigned cdb_len,
      void *buf, uint32_t len)
{
   int ret = -EIO, tries;
   for (tries = 0; tries < 3 && ret < 0; tries++)
      ret = command(d, cdb, cdb_len, buf, len, 1, NULL);
   return ret;
}

static int request_sense(msc_disk *d, uint8_t *key, uint8_t *asc)
{
   uint8_t cdb[6] = { SCSI_SENSE, 0, 0, 0, 18, 0 }, s[18];
   memset(s, 0, sizeof(s));
   if (command3(d, cdb, 6, s, 18))
      return -EIO;
   *key = s[2] & 0x0f;
   *asc = s[12];
   return 0;
}

/* Until the unit is ready: 0, -ENXIO (no medium), or -ETIMEDOUT. */
static int wait_ready(msc_disk *d)
{
   static const uint8_t tur[6] = { SCSI_TUR, 0, 0, 0, 0, 0 };
   uint32_t start = d->bus->ms(d->bus);
   for (;;)
   {
      uint8_t key = 0, asc = 0;
      int ret = command(d, tur, 6, NULL, 0, 0, NULL);
      if (!ret)
         return 0;
      if (ret == 1 && !request_sense(d, &key, &asc)
            && key == SENSE_NOT_READY && asc == ASC_NO_MEDIUM)
         return -ENXIO;
      if (d->bus->ms(d->bus) - start > READY_MS)
         return -ETIMEDOUT;
      d->bus->sleep_ms(d->bus, 50);
   }
}

static int read_capacity(msc_disk *d)
{
   uint8_t cdb[16], b[32];
   uint64_t last;
   uint32_t bs;
   memset(cdb, 0, sizeof(cdb));
   cdb[0] = SCSI_CAP10;
   if (command3(d, cdb, 10, b, 8))
      return -EIO;
   last = rd32be(b);
   bs   = rd32be(b + 4);
   if (last == 0xffffffffu)
   {
      /* Past 2^32 blocks: the 16-byte form. */
      memset(cdb, 0, sizeof(cdb));
      cdb[0]  = SCSI_CAP16;
      cdb[1]  = 0x10;
      cdb[13] = 32;
      if (command3(d, cdb, 16, b, 32))
         return -EIO;
      last = ((uint64_t)rd32be(b) << 32) | rd32be(b + 4);
      bs   = rd32be(b + 8);
      d->long_lba = 1;
   }
   if (bs != 512 && bs != 1024 && bs != 2048 && bs != 4096)
      return -EINVAL;
   d->dev.sectors     = last + 1;
   d->dev.sector_size = bs;
   return 0;
}

static int rw(msc_disk *d, uint64_t lba, uint32_t count, uint8_t *buf,
      int writing)
{
   uint32_t bs = d->dev.sector_size, per = CMD_BYTES / bs;
   while (count)
   {
      uint32_t n = count < per ? count : per, moved;
      uint8_t cdb[16];
      int ret = -EIO, tries;
      memset(cdb, 0, sizeof(cdb));
      if (d->long_lba)
      {
         cdb[0]  = writing ? SCSI_WRITE16 : SCSI_READ16;
         cdb[2]  = (uint8_t)(lba >> 56);
         cdb[3]  = (uint8_t)(lba >> 48);
         cdb[4]  = (uint8_t)(lba >> 40);
         cdb[5]  = (uint8_t)(lba >> 32);
         cdb[6]  = (uint8_t)(lba >> 24);
         cdb[7]  = (uint8_t)(lba >> 16);
         cdb[8]  = (uint8_t)(lba >> 8);
         cdb[9]  = (uint8_t)lba;
         cdb[12] = (uint8_t)(n >> 8);
         cdb[13] = (uint8_t)n;
      }
      else
      {
         cdb[0] = writing ? SCSI_WRITE10 : SCSI_READ10;
         cdb[2] = (uint8_t)(lba >> 24);
         cdb[3] = (uint8_t)(lba >> 16);
         cdb[4] = (uint8_t)(lba >> 8);
         cdb[5] = (uint8_t)lba;
         cdb[7] = (uint8_t)(n >> 8);
         cdb[8] = (uint8_t)n;
      }
      /* A failed command is retried after its sense is read (a unit
       * attention after a reset, say); a broken exchange after the
       * reset recovery. */
      for (tries = 0; tries < 3 && ret; tries++)
      {
         ret = command(d, cdb, d->long_lba ? 16 : 10, buf, n * bs,
               !writing, &moved);
         if (ret == 1)
         {
            uint8_t key, asc;
            request_sense(d, &key, &asc);
         }
         if (!ret && moved != n * bs)
            ret = -EIO;
      }
      if (ret)
         return -1;
      buf   += (size_t)n * bs;
      lba   += n;
      count -= n;
   }
   return 0;
}

static int dev_read(gk_blockdev_t *dev, uint64_t lba, uint32_t count,
      void *buf)
{
   return rw((msc_disk*)dev->priv, lba, count, (uint8_t*)buf, 0);
}

static int dev_write(gk_blockdev_t *dev, uint64_t lba, uint32_t count,
      const void *buf)
{
   return rw((msc_disk*)dev->priv, lba, count, (uint8_t*)buf, 1);
}

int msc_init(msc_disk *d, msc_bus *bus, uint8_t iface, uint8_t ep_in,
      uint8_t ep_out)
{
   uint8_t max_lun = 0, lun;
   int ret = -ENODEV;

   memset(d, 0, sizeof(*d));
   d->bus    = bus;
   d->iface  = iface;
   d->ep_in  = ep_in;
   d->ep_out = ep_out;

   /* Devices with one LUN may stall this. */
   if (bus->ctrl(bus, 0xa1, REQ_MAX_LUN, 0, iface, &max_lun, 1) != 1
         || max_lun > 15)
      max_lun = 0;

   for (lun = 0; lun <= max_lun; lun++)
   {
      uint8_t cdb[6] = { SCSI_INQUIRY, 0, 0, 0, 36, 0 }, inq[36];
      unsigned type;
      d->lun      = lun;
      d->long_lba = 0;
      memset(inq, 0, sizeof(inq));
      if (command3(d, cdb, 6, inq, 36))
         continue;
      /* Direct access, or the simplified (RBC) kind. */
      type = inq[0] & 0x1f;
      if ((inq[0] >> 5) || (type != 0x00 && type != 0x0e))
         continue;
      if ((ret = wait_ready(d)) || (ret = read_capacity(d)))
         continue;
      d->dev.priv  = d;
      d->dev.read  = dev_read;
      d->dev.write = dev_write;
      return 0;
   }
   return ret;
}
