/* disk/sdspi.c against a model of an SD card in SPI mode.
 *
 * The model follows the Physical Layer Simplified Specification as
 * strictly as a host can check it: CRCs on CMD0 and CMD8, ACMD41 only
 * after CMD55, no reads or writes before initialisation, byte or
 * block addressing by card kind, random response and access delays,
 * busy after writes, CMD12 ending multi-block reads and the stop token
 * ending multi-block writes.  Blocks are read and written at random
 * and checked against a copy, then FAT is mounted over the card.
 *
 *   sdspi_test             the card kinds over random data
 *   sdspi_test IMAGE       an SDHC card holding IMAGE: read
 *                          /hello.txt through FAT, write /spi.txt */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../disk/sdspi.h"
#include "../../fs/fat.h"

static int failures;

#define CHECK(cond) do { if (!(cond)) { \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
   failures++; } } while (0)

uint32_t fat_now(void)               { return 1700000000u; }
void    *fat_lock_create(void)       { static int lock; return &lock; }
void     fat_lock_destroy(void *lock) { (void)lock; }
void     fat_lock_acquire(void *lock) { (void)lock; }
void     fat_lock_release(void *lock) { (void)lock; }

static uint32_t rng = 1;
static uint32_t rnd(uint32_t n)
{
   rng = rng * 1103515245u + 12345u;
   return (rng >> 8) % n;
}

/* ---- the card ---- */

#define QUEUE 1100

enum mode { IDLE, READ_SINGLE, READ_MULTI, WRITE_WAIT, WRITE_DATA };

struct card
{
   uint8_t  *data;
   uint32_t  blocks;
   int       sdhc, v1;
   int       selected, inited, app, init_polls;
   enum mode mode;
   int       multi;
   uint32_t  addr;           /* next block */
   uint8_t   cmd[6];
   unsigned  cmd_len;
   uint8_t   out[QUEUE];
   unsigned  out_head, out_len;
   uint8_t   wbuf[514];
   unsigned  wlen;
   unsigned  busy;           /* bytes of 0x00 still to come */
   unsigned long bytes, cmds, blocks_read, blocks_written;
} card;

static void put(uint8_t b)
{
   if (card.out_len < QUEUE)
      card.out[(card.out_head + card.out_len++) % QUEUE] = b;
}

static void put_delay(unsigned max)
{
   unsigned n = rnd(max) + 1;
   while (n--)
      put(0xff);
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

static void queue_block(void)
{
   unsigned i;
   put_delay(40);
   put(0xfe);
   for (i = 0; i < 512; i++)
      put(card.data[(size_t)card.addr * 512 + i]);
   put(0x12);
   put(0x34);
   card.addr++;
   card.blocks_read++;
}

/* The block a read or write argument names; -1 with R1 set if bad. */
static int64_t target(uint32_t arg, uint8_t *r1)
{
   uint32_t b;
   if (!card.sdhc && (arg & 511))
   {
      *r1 = 0x20;   /* address error */
      return -1;
   }
   b = card.sdhc ? arg : arg >> 9;
   if (b >= card.blocks)
   {
      *r1 = 0x40;   /* parameter error */
      return -1;
   }
   return b;
}

static void do_command(void)
{
   uint8_t idx = card.cmd[0] & 0x3f, r1 = card.inited ? 0 : 1;
   uint32_t arg = ((uint32_t)card.cmd[1] << 24) | (card.cmd[2] << 16)
      | (card.cmd[3] << 8) | card.cmd[4];
   int app = card.app;
   int64_t b;
   card.app = 0;
   card.cmds++;

   if (idx == 12)
   {
      /* Ends a multi-block read: a stuff byte, R1, then busy. */
      if (card.mode == READ_MULTI)
      {
         card.mode    = IDLE;
         card.out_len = 0;
      }
      put((uint8_t)rnd(256));   /* the stuff byte: anything */
      put_delay(4);
      put(r1);
      card.busy = rnd(20);
      return;
   }
   if (card.mode != IDLE)
      return;   /* not listening */
   if ((idx == 0 || idx == 8)
         && (uint8_t)((crc7(card.cmd, 5) << 1) | 1) != card.cmd[5])
   {
      put_delay(8);
      put((uint8_t)(r1 | 0x08));
      return;
   }
   put_delay(8);
   switch (app ? 100 + idx : idx)
   {
      case 0:
         card.inited = 0;
         card.init_polls = 3 + (int)rnd(5);
         put(0x01);
         break;
      case 8:
         if (card.v1)
         {
            put((uint8_t)(r1 | 0x04));
            break;
         }
         put(r1);
         put(0);
         put(0);
         put((uint8_t)((arg >> 8) & 0x0f));
         put((uint8_t)arg);
         break;
      case 55:
         card.app = 1;
         put(r1);
         break;
      case 141:
         if (card.sdhc && !(arg & 0x40000000u))
            card.init_polls = 1 << 30;   /* SDHC without HCS never ends */
         if (card.init_polls > 0)
            card.init_polls--;
         else
            card.inited = 1;
         put(card.inited ? 0 : 1);
         break;
      case 58:
         put(r1);
         put((uint8_t)((card.inited ? 0x80 : 0) | (card.sdhc ? 0x40 : 0)));
         put(0xff);
         put(0x80);
         put(0x00);
         break;
      case 9:
         if (!card.inited)
         {
            put(0x05);
            break;
         }
         put(0);
         put_delay(10);
         put(0xfe);
         if (card.sdhc)
         {
            uint32_t size = card.blocks / 1024 - 1;
            uint8_t csd[16] = { 0x40, 0x0e, 0x00, 0x32, 0x5b, 0x59, 0x00,
               0, 0, 0, 0x7f, 0x80, 0x0a, 0x40, 0x00, 0x01 };
            csd[7] = (uint8_t)((size >> 16) & 0x3f);
            csd[8] = (uint8_t)(size >> 8);
            csd[9] = (uint8_t)size;
            for (b = 0; b < 16; b++)
               put(csd[b]);
         }
         else
         {
            /* 512-byte blocks, C_SIZE_MULT 7: C_SIZE + 1 units of 256. */
            uint32_t size = card.blocks / 512 - 1;
            uint8_t csd[16] = { 0x00, 0x26, 0x00, 0x32, 0x5f, 0x59, 0,
               0, 0, 0, 0, 0x7f, 0x80, 0x0a, 0x40, 0x01 };
            csd[6]  = (uint8_t)(0x80 | ((size >> 10) & 3));
            csd[7]  = (uint8_t)(size >> 2);
            csd[8]  = (uint8_t)((size & 3) << 6);
            csd[9]  = 0x03;               /* C_SIZE_MULT high bits */
            csd[10] = 0x80;               /* C_SIZE_MULT low bit */
            for (b = 0; b < 16; b++)
               put(csd[b]);
         }
         put(0);
         put(0);
         break;
      case 13:
         put(r1);
         put(0);
         break;
      case 16:
         put(card.inited && arg == 512 ? 0 : (uint8_t)(r1 | 0x40));
         break;
      case 17:
      case 18:
      case 24:
      case 25:
         if (!card.inited)
         {
            put(0x05);
            break;
         }
         if ((b = target(arg, &r1)) < 0)
         {
            put(r1);
            break;
         }
         put(0);
         card.addr  = (uint32_t)b;
         card.multi = idx == 18 || idx == 25;
         if (idx == 17 || idx == 18)
         {
            card.mode = idx == 17 ? READ_SINGLE : READ_MULTI;
            queue_block();
            if (idx == 17)
               card.mode = IDLE;
         }
         else
            card.mode = WRITE_WAIT;
         break;
      default:
         put((uint8_t)(r1 | 0x04));
         break;
   }
}

static uint8_t card_byte(uint8_t in)
{
   uint8_t out = 0xff;
   card.bytes++;
   if (!card.selected)
      return 0xff;
   /* What goes out this clock. */
   if (card.out_len)
   {
      out = card.out[card.out_head];
      card.out_head = (card.out_head + 1) % QUEUE;
      card.out_len--;
      if (!card.out_len && card.mode == READ_MULTI)
      {
         if (card.addr < card.blocks)
            queue_block();
      }
   }
   else if (card.busy)
   {
      card.busy--;
      out = 0x00;
   }
   /* What comes in. */
   switch (card.mode)
   {
      case WRITE_WAIT:
         if (in == 0xff)
            break;
         if (in == (card.multi ? 0xfc : 0xfe))
         {
            card.mode = WRITE_DATA;
            card.wlen = 0;
         }
         else if (card.multi && in == 0xfd)
         {
            card.mode = IDLE;
            put(0xff);
            card.busy = 1 + rnd(30);
         }
         else
         {
            printf("card: bad token %02x\n", in);
            failures++;
         }
         break;
      case WRITE_DATA:
         card.wbuf[card.wlen++] = in;
         if (card.wlen == 514)
         {
            if (card.addr >= card.blocks)
               put(0x0d);    /* write error */
            else
            {
               memcpy(card.data + (size_t)card.addr * 512, card.wbuf, 512);
               card.addr++;
               card.blocks_written++;
               put(0x05 | 0xe0);
            }
            card.busy = 1 + rnd(60);
            card.mode = card.multi ? WRITE_WAIT : IDLE;
         }
         break;
      default:
         if (card.cmd_len == 0 && (in & 0xc0) != 0x40)
            break;
         if (card.busy && card.cmd_len == 0)
            break;   /* busy cards do not take commands */
         card.cmd[card.cmd_len++] = in;
         if (card.cmd_len == 6)
         {
            card.cmd_len = 0;
            if (!(card.cmd[5] & 1))
            {
               printf("card: no end bit\n");
               failures++;
            }
            do_command();
         }
         break;
   }
   return out;
}

/* ---- the bus ---- */

static void bus_select(sdspi_bus *bus, int fast)
{
   (void)bus;
   (void)fast;
   if (card.selected)
   {
      printf("bus: selected twice\n");
      failures++;
   }
   card.selected = 1;
}

static void bus_deselect(sdspi_bus *bus)
{
   (void)bus;
   card.selected = 0;
   card.cmd_len  = 0;
   card.out_len  = 0;
   if (card.mode == READ_MULTI || card.mode == WRITE_WAIT
         || card.mode == WRITE_DATA)
   {
      printf("bus: deselected mid-transfer\n");
      failures++;
      card.mode = IDLE;
   }
}

static void bus_idle_clocks(sdspi_bus *bus, unsigned bytes)
{
   (void)bus;
   card.bytes += bytes;
}

static void bus_xfer(sdspi_bus *bus, const uint8_t *out, uint8_t *in,
      uint32_t len)
{
   uint32_t i;
   (void)bus;
   if (!card.selected)
   {
      printf("bus: transfer without a selection\n");
      failures++;
   }
   for (i = 0; i < len; i++)
   {
      uint8_t b = card_byte(out ? out[i] : 0xff);
      if (in)
         in[i] = b;
   }
}

static int bus_read(sdspi_bus *bus, uint8_t *buf, uint32_t len)
{
   bus_xfer(bus, NULL, buf, len);
   return 0;
}

static int bus_write(sdspi_bus *bus, const uint8_t *buf, uint32_t len)
{
   bus_xfer(bus, buf, NULL, len);
   return 0;
}

static uint32_t bus_ms(sdspi_bus *bus)
{
   (void)bus;
   return (uint32_t)(card.bytes / 1000);   /* 1 byte a microsecond */
}

static sdspi_bus bus = { NULL, bus_select, bus_deselect, bus_idle_clocks,
   bus_xfer, bus_read, bus_write, bus_ms };

/* ---- the test ---- */

static void run(int sdhc, int v1, uint32_t blocks)
{
   sdspi_card c;
   uint8_t *copy, *buf;
   unsigned i;
   int ret;

   memset(&card, 0, sizeof(card));
   card.sdhc   = sdhc;
   card.v1     = v1;
   card.blocks = blocks;
   card.data   = (uint8_t*)malloc((size_t)blocks * 512);
   copy        = (uint8_t*)malloc((size_t)blocks * 512);
   buf         = (uint8_t*)malloc(64 * 512);
   for (i = 0; i < blocks * 512; i++)
      card.data[i] = copy[i] = (uint8_t)rnd(256);

   ret = sdspi_init(&c, &bus);
   CHECK(ret == 0);
   if (ret)
      return;
   CHECK(c.dev.sectors == blocks);
   CHECK(c.blocks == sdhc);

   for (i = 0; i < 400 && !failures; i++)
   {
      uint32_t n   = rnd(4) ? 1 + rnd(3) : 1 + rnd(64);
      uint32_t lba = rnd(blocks - n);
      if (rnd(2))
      {
         uint32_t k;
         for (k = 0; k < n * 512; k++)
            buf[k] = (uint8_t)rnd(256);
         CHECK(c.dev.write(&c.dev, lba, n, buf) == 0);
         memcpy(copy + (size_t)lba * 512, buf, (size_t)n * 512);
      }
      else
      {
         CHECK(c.dev.read(&c.dev, lba, n, buf) == 0);
         CHECK(!memcmp(buf, copy + (size_t)lba * 512, (size_t)n * 512));
      }
   }
   CHECK(!memcmp(card.data, copy, (size_t)blocks * 512));
   /* Past the end fails rather than wrapping. */
   CHECK(c.dev.read(&c.dev, blocks, 1, buf) != 0);

   printf("%s%s: %lu commands, %lu blocks read, %lu written, %lu bytes\n",
         sdhc ? "SDHC" : "SDSC", v1 ? " v1" : "", card.cmds,
         card.blocks_read, card.blocks_written, card.bytes);
   free(card.data);
   free(copy);
   free(buf);
}

static void image(const char *path)
{
   sdspi_card c;
   fat_vol *vol;
   fat_file *f;
   char text[16];
   FILE *fp = fopen(path, "r+b");
   long size;
   if (!fp)
   {
      perror(path);
      exit(2);
   }
   fseek(fp, 0, SEEK_END);
   size = ftell(fp);
   rewind(fp);
   memset(&card, 0, sizeof(card));
   card.sdhc   = 1;
   card.blocks = (uint32_t)(size / 512);
   card.data   = (uint8_t*)malloc((size_t)size);
   CHECK(fread(card.data, 1, (size_t)size, fp) == (size_t)size);
   CHECK(sdspi_init(&c, &bus) == 0);
   CHECK(fat_mount(&vol, &c.dev) == 0);
   CHECK(fat_open(vol, "/hello.txt", O_RDONLY, &f) == 0);
   memset(text, 0, sizeof(text));
   CHECK(fat_read(f, text, sizeof(text) - 1) == 6 && !strcmp(text, "hello\n"));
   CHECK(fat_close(f) == 0);
   CHECK(fat_open(vol, "/spi.txt", O_WRONLY | O_CREAT, &f) == 0);
   CHECK(fat_write(f, "over spi\n", 9) == 9);
   CHECK(fat_close(f) == 0);
   CHECK(fat_unmount(vol) == 0);
   rewind(fp);
   CHECK(fwrite(card.data, 1, (size_t)size, fp) == (size_t)size);
   fclose(fp);
   free(card.data);
   printf("%s over spi: %s\n", path, failures ? "FAILED" : "ok");
}

int main(int argc, char **argv)
{
   if (argc > 1)
   {
      image(argv[1]);
      return failures ? 1 : 0;
   }
   run(0, 0, 8192);
   run(0, 1, 4096);
   run(1, 0, 16384);
   printf("sdspi: %s\n", failures ? "FAILED" : "ok");
   return failures ? 1 : 0;
}
