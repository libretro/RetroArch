/* Wii: the front SD slot, through IOS's /dev/sdio/slot0.
 *
 * IOS brings the card up to the stand-by state on a reset; from there
 * it is ordinary SD: read the CSD for the size, select the card, set
 * 512-byte blocks and the 4-bit bus.  Data moves with multi-block
 * commands that IOS DMAs to a physical address; SDHC cards take block
 * numbers, standard ones byte addresses. */

#include <errno.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

#include <gekko/disk.h>
#include <gekko/ios.h>
#include <gekko/thread.h>

#define SDIO_WRITE_HCR  0x01
#define SDIO_READ_HCR   0x02
#define SDIO_RESET      0x04
#define SDIO_SET_CLOCK  0x06
#define SDIO_COMMAND    0x07
#define SDIO_STATUS     0x0b

#define STATUS_INSERTED    0x00000001u
#define STATUS_INITIALIZED 0x00010000u
#define STATUS_SDHC        0x00100000u

/* Command and response kinds as IOS numbers them. */
#define TYPE_AC  3
#define RESP_R1  1
#define RESP_R1B 2
#define RESP_R2  3

#define HCR_HOST_CONTROL 0x28
#define HC_4BIT          0x02

#define CMD_SELECT       7
#define CMD_SEND_CSD     9
#define CMD_SET_BLOCKLEN 16
#define CMD_READ_MULTI   18
#define CMD_WRITE_MULTI  25
#define CMD_APP          55
#define ACMD_BUS_WIDTH   6

#define BOUNCE_SECTORS 128

struct sd_request
{
   uint32_t cmd;
   uint32_t type;
   uint32_t resp;
   uint32_t arg;
   uint32_t blocks;
   uint32_t block_size;
   uint32_t addr;
   uint32_t dma;
   uint32_t pad;
};

static struct
{
   gk_blockdev_t dev;
   gk_mutex_t    lock;
   int32_t       fd;
   uint32_t      rca;
   int           sdhc;
   uint8_t      *bounce;
} sd;

static struct sd_request req   __attribute__((aligned(32)));
static uint32_t          reply[8] __attribute__((aligned(32)));
static uint32_t          word[8]  __attribute__((aligned(32)));
static uint32_t          hcr[8]   __attribute__((aligned(32)));

static int command(uint32_t cmd, uint32_t resp, uint32_t arg, void *buf,
      uint32_t blocks)
{
   int32_t ret;
   memset(&req, 0, sizeof(req));
   req.cmd  = cmd;
   req.type = TYPE_AC;
   req.resp = resp;
   req.arg  = arg;
   if (buf)
   {
      gk_ios_vec_t vec[3];
      req.blocks     = blocks;
      req.block_size = 512;
      req.addr       = GK_PHYS(buf);
      req.dma        = 1;
      vec[0].data = &req;
      vec[0].len  = sizeof(req);
      vec[1].data = buf;
      vec[1].len  = blocks * 512;
      vec[2].data = reply;
      vec[2].len  = 16;
      ret = gk_ios_ioctlv(sd.fd, SDIO_COMMAND, 2, 1, vec);
   }
   else
      ret = gk_ios_ioctl(sd.fd, SDIO_COMMAND, &req, sizeof(req), reply, 16);
   return ret < 0 ? -EIO : 0;
}

static int hcr_access(uint32_t ioctl, uint32_t reg, uint32_t val,
      uint32_t *out)
{
   memset(hcr, 0, sizeof(hcr));
   hcr[0] = reg;
   hcr[3] = 1;        /* register width in bytes */
   hcr[4] = val;
   return gk_ios_ioctl(sd.fd, ioctl, hcr, 24, out, out ? 4 : 0) < 0
      ? -EIO : 0;
}

/* Bits hi..lo of the CSD as the response holds it: the 128-bit
 * register less its CRC byte, least significant word first. */
static uint32_t csd_bits(unsigned hi, unsigned lo)
{
   uint32_t v = 0;
   unsigned b;
   for (b = hi + 1; b-- > lo;)
      v = (v << 1) | ((reply[b / 32] >> (b % 32)) & 1);
   return v;
}

static uint64_t card_sectors(void)
{
   if (command(CMD_SEND_CSD, RESP_R2, sd.rca, NULL, 0))
      return 0;
   if (csd_bits(119, 118) == 1)
      return ((uint64_t)csd_bits(61, 40) + 1) * 1024;
   /* Version 1: (C_SIZE + 1) << (C_SIZE_MULT + 2 + READ_BL_LEN) bytes. */
   return ((uint64_t)csd_bits(65, 54) + 1)
      << (csd_bits(41, 39) + 2 + csd_bits(75, 72) - 9);
}

static int transfer(gk_blockdev_t *dev, uint64_t lba, uint32_t count,
      void *buf, int writing)
{
   int ret = 0;
   (void)dev;
   gk_mutex_lock(&sd.lock);
   while (count && !ret)
   {
      uint32_t n = count < BOUNCE_SECTORS ? count : BOUNCE_SECTORS;
      /* The DMA needs 32-byte alignment; whole sectors keep the end
       * aligned too. */
      int direct = !((uintptr_t)buf & 31);
      uint8_t *p = direct ? (uint8_t*)buf : sd.bounce;
      uint32_t arg = sd.sdhc ? (uint32_t)lba : (uint32_t)(lba * 512);
      if (writing && !direct)
         memcpy(p, buf, n * 512);
      ret = command(writing ? CMD_WRITE_MULTI : CMD_READ_MULTI, RESP_R1,
            arg, p, n);
      if (!writing)
      {
         /* IOS wrote memory behind the cache. */
         gk_dcache_invalidate(p, n * 512);
         if (!direct && !ret)
            memcpy(buf, p, n * 512);
      }
      buf    = (uint8_t*)buf + n * 512;
      lba   += n;
      count -= n;
   }
   gk_mutex_unlock(&sd.lock);
   return ret;
}

static int sd_read(gk_blockdev_t *dev, uint64_t lba, uint32_t count,
      void *buf)
{
   return transfer(dev, lba, count, buf, 0);
}

static int sd_write(gk_blockdev_t *dev, uint64_t lba, uint32_t count,
      const void *buf)
{
   return transfer(dev, lba, count, (void*)buf, 1);
}

void gk_sd_close(void)
{
   gk_mutex_lock(&sd.lock);
   if (sd.dev.read)
   {
      command(CMD_SELECT, RESP_R1B, 0, NULL, 0);
      gk_ios_close(sd.fd);
      sd.dev.read = NULL;
   }
   gk_mutex_unlock(&sd.lock);
}

int gk_sd_inserted(void)
{
   int in = 0;
   gk_mutex_lock(&sd.lock);
   if (sd.dev.read
         && gk_ios_ioctl(sd.fd, SDIO_STATUS, NULL, 0, word, 4) >= 0)
      in = (word[0] & STATUS_INSERTED) != 0;
   gk_mutex_unlock(&sd.lock);
   return in;
}

static gk_blockdev_t *open_card(void)
{
   uint32_t status, ctl;

   if (sd.dev.read)
      return &sd.dev;
   if (!sd.bounce && !(sd.bounce = (uint8_t*)memalign(32,
               BOUNCE_SECTORS * 512)))
      return NULL;
   if ((sd.fd = gk_ios_open("/dev/sdio/slot0", 0)) < 0)
      return NULL;
   if (gk_ios_ioctl(sd.fd, SDIO_RESET, NULL, 0, word, 4) < 0)
      goto fail;
   sd.rca = word[0] & 0xffff0000u;
   if (gk_ios_ioctl(sd.fd, SDIO_STATUS, NULL, 0, word, 4) < 0)
      goto fail;
   status = word[0];
   if (!(status & STATUS_INSERTED) || !(status & STATUS_INITIALIZED))
      goto fail;
   sd.sdhc = (status & STATUS_SDHC) != 0;

   sd.dev.sectors     = card_sectors();
   sd.dev.sector_size = 512;
   sd.dev.write       = sd_write;
   sd.dev.priv        = &sd;

   if (command(CMD_SELECT, RESP_R1B, sd.rca, NULL, 0)
         || command(CMD_SET_BLOCKLEN, RESP_R1, 512, NULL, 0)
         || command(CMD_APP, RESP_R1, sd.rca, NULL, 0)
         || command(ACMD_BUS_WIDTH, RESP_R1, 2, NULL, 0)
         || hcr_access(SDIO_READ_HCR, HCR_HOST_CONTROL, 0, word))
      goto fail;
   ctl = (word[0] & 0xff) | HC_4BIT;
   word[0] = 1;   /* half the SD clock divisor */
   if (hcr_access(SDIO_WRITE_HCR, HCR_HOST_CONTROL, ctl, NULL)
         || gk_ios_ioctl(sd.fd, SDIO_SET_CLOCK, word, 4, NULL, 0) < 0)
      goto fail;
   sd.dev.read = sd_read;   /* open from here on */
   return &sd.dev;

fail:
   gk_ios_close(sd.fd);
   return NULL;
}

gk_blockdev_t *gk_sd_open(void)
{
   gk_blockdev_t *dev;
   gk_mutex_lock(&sd.lock);
   dev = open_card();
   gk_mutex_unlock(&sd.lock);
   return dev;
}
