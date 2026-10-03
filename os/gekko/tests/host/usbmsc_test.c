/* disk/usbmsc.c against a model USB drive.
 *
 * The model keeps to the Bulk-Only Transport rules a host can get
 * wrong: a status wrapper only after the data stage, stalls on short
 * data-in it cannot fill (or short packets instead), endpoints that
 * stay halted until CLEAR_FEATURE, an invalid command block wrapper
 * stalling both endpoints until a reset recovery, and phase errors.
 * Its SCSI side has a LUN without a medium ahead of the one with it,
 * a unit that takes a while to become ready, unit attentions, and
 * READ(16)/WRITE(16) past 2^32 blocks.
 *
 *   usbmsc_test           the drive kinds over random data
 *   usbmsc_test IMAGE     a drive holding IMAGE: read /hello.txt
 *                         through FAT, write /usb.txt */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../disk/usbmsc.h"
#include "../../fs/fat.h"

static int failures;

#define CHECK(cond) do { if (!(cond)) { \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
   failures++; } } while (0)

#define PROTO(msg) do { printf("drive: %s\n", msg); failures++; } while (0)

uint32_t fat_now(void)               { return 1700000000u; }
void    *fat_lock_create(void)       { static int lock; return &lock; }
void     fat_lock_destroy(void *lock) { (void)lock; }
void     fat_lock_acquire(void *lock) { (void)lock; }
void     fat_lock_release(void *lock) { (void)lock; }

static uint32_t rng = 7;
static uint32_t rnd(uint32_t n)
{
   rng = rng * 1103515245u + 12345u;
   return (rng >> 8) % n;
}

/* ---- blocks: written ones in a table, the rest a pattern ---- */

#define SLOTS 65536

struct block
{
   uint64_t lba;
   uint8_t *data;
};

struct store
{
   struct block slot[SLOTS];
   uint32_t     bs;
   uint8_t     *image;     /* or a whole image */
};

static uint8_t fill_byte(uint64_t lba, uint32_t i)
{
   return (uint8_t)((lba * 2654435761u + i * 40503u) >> 7);
}

static struct block *find(struct store *s, uint64_t lba, int add)
{
   uint32_t h = (uint32_t)(lba * 2654435761u) % SLOTS, k;
   for (k = 0; k < SLOTS; k++, h = (h + 1) % SLOTS)
   {
      struct block *b = &s->slot[h];
      if (b->data && b->lba == lba)
         return b;
      if (!b->data)
      {
         if (!add)
            return NULL;
         b->lba  = lba;
         b->data = (uint8_t*)malloc(s->bs);
         return b;
      }
   }
   return NULL;
}

static void store_read(struct store *s, uint64_t lba, uint8_t *out)
{
   struct block *b;
   uint32_t i;
   if (s->image)
   {
      memcpy(out, s->image + lba * s->bs, s->bs);
      return;
   }
   if ((b = find(s, lba, 0)))
      memcpy(out, b->data, s->bs);
   else
      for (i = 0; i < s->bs; i++)
         out[i] = fill_byte(lba, i);
}

static void store_write(struct store *s, uint64_t lba, const uint8_t *in)
{
   if (s->image)
      memcpy(s->image + lba * s->bs, in, s->bs);
   else
      memcpy(find(s, lba, 1)->data, in, s->bs);
}

static void store_free(struct store *s)
{
   unsigned i;
   for (i = 0; i < SLOTS; i++)
      free(s->slot[i].data);
   memset(s, 0, sizeof(*s));
}

/* ---- the drive ---- */

#define EP_IN  0x81
#define EP_OUT 0x02
#define BUF    (512 * 1024)

enum phase { CBW, DATA_IN, DATA_OUT, CSW };

static struct drive
{
   struct store *store;     /* LUN 1; LUN 0 has no medium */
   uint64_t  blocks;
   uint32_t  bs;
   int       luns;          /* 1 or 2 */
   int       not_ready;     /* TURs before the unit is ready */
   int       stall_lun;     /* GET MAX LUN stalls */
   enum phase phase;
   int       halt_in, halt_out, need_reset;
   uint8_t   lun;
   uint32_t  tag, last_tag, expect;   /* expect: the CBW's length */
   uint32_t  residue, done;
   uint8_t   status;
   uint8_t   data[BUF];
   uint32_t  data_len;
   uint64_t  w_lba;         /* for a write's data */
   uint8_t   sense_key, asc;
   unsigned long cmds, stalls, resets, attentions, phase_errors;
} dr;

static uint64_t be(const uint8_t *p, unsigned n)
{
   uint64_t v = 0;
   while (n--)
      v = (v << 8) | *p++;
   return v;
}

static void put_be(uint8_t *p, uint64_t v, unsigned n)
{
   while (n--)
   {
      p[n] = (uint8_t)v;
      v >>= 8;
   }
}

static void fail_cmd(uint8_t key, uint8_t asc)
{
   dr.status    = 1;
   dr.sense_key = key;
   dr.asc       = asc;
   dr.data_len  = 0;
}

/* Run a command: data_len bytes of data-in, or set up for data-out. */
static void scsi(const uint8_t *cb, int to_host)
{
   int has_medium = dr.lun == dr.luns - 1;
   uint64_t lba, n;
   dr.status   = 0;
   dr.data_len = 0;
   switch (cb[0])
   {
      case 0x00:   /* TEST UNIT READY */
         if (!has_medium)
            fail_cmd(0x02, 0x3a);
         else if (dr.not_ready > 0)
         {
            dr.not_ready--;
            fail_cmd(0x02, 0x04);
         }
         break;
      case 0x03:   /* REQUEST SENSE */
         memset(dr.data, 0, 18);
         dr.data[0]  = 0x70;
         dr.data[2]  = dr.sense_key;
         dr.data[7]  = 10;
         dr.data[12] = dr.asc;
         dr.data_len = cb[4] < 18 ? cb[4] : 18;
         dr.sense_key = dr.asc = 0;
         break;
      case 0x12:   /* INQUIRY */
         memset(dr.data, 0, 36);
         dr.data[4] = 31;
         memcpy(dr.data + 8, "MODEL   USB DRIVE       0001", 28);
         dr.data_len = cb[4] < 36 ? cb[4] : 36;
         break;
      case 0x25:   /* READ CAPACITY(10) */
         if (!has_medium || dr.not_ready)
         {
            fail_cmd(0x02, has_medium ? 0x04 : 0x3a);
            break;
         }
         put_be(dr.data, dr.blocks - 1 > 0xffffffffu ? 0xffffffffu
               : dr.blocks - 1, 4);
         put_be(dr.data + 4, dr.bs, 4);
         dr.data_len = 8;
         break;
      case 0x9e:   /* READ CAPACITY(16) */
         if (cb[1] != 0x10)
         {
            fail_cmd(0x05, 0x24);
            break;
         }
         memset(dr.data, 0, 32);
         put_be(dr.data, dr.blocks - 1, 8);
         put_be(dr.data + 8, dr.bs, 4);
         dr.data_len = 32;
         break;
      case 0x28: case 0x2a: case 0x88: case 0x8a:
      {
         int is16 = cb[0] == 0x88 || cb[0] == 0x8a, write = cb[0] & 2;
         uint64_t i;
         lba = is16 ? be(cb + 2, 8) : be(cb + 2, 4);
         n   = is16 ? be(cb + 10, 4) : be(cb + 7, 2);
         if (!is16 && dr.blocks > 0xffffffffu && lba + n > 0xffffffffu)
            PROTO("10-byte command where 16 is needed");
         if (!has_medium || lba + n > dr.blocks || n * dr.bs > BUF
               || to_host == write)
         {
            fail_cmd(0x05, 0x21);
            break;
         }
         if (!rnd(40))
         {
            /* Something happened since the last command. */
            dr.attentions++;
            fail_cmd(0x06, 0x29);
            break;
         }
         if (write)
         {
            dr.w_lba    = lba;
            dr.data_len = (uint32_t)(n * dr.bs);
         }
         else
         {
            for (i = 0; i < n; i++)
               store_read(dr.store, lba + i, dr.data + i * dr.bs);
            dr.data_len = (uint32_t)(n * dr.bs);
         }
         break;
      }
      default:
         fail_cmd(0x05, 0x20);
         break;
   }
}

static void make_csw(uint8_t *p)
{
   p[0] = 'U'; p[1] = 'S'; p[2] = 'B'; p[3] = 'S';
   p[4] = (uint8_t)dr.tag;
   p[5] = (uint8_t)(dr.tag >> 8);
   p[6] = (uint8_t)(dr.tag >> 16);
   p[7] = (uint8_t)(dr.tag >> 24);
   p[8] = (uint8_t)dr.residue;
   p[9] = (uint8_t)(dr.residue >> 8);
   p[10] = (uint8_t)(dr.residue >> 16);
   p[11] = (uint8_t)(dr.residue >> 24);
   p[12] = dr.status;
}

static int bus_bulk(msc_bus *bus, uint8_t ep, void *buf, uint32_t len)
{
   uint8_t *p = (uint8_t*)buf;
   (void)bus;
   if (len > bus->max_transfer)
      PROTO("transfer longer than the bus allows");
   if (ep == EP_OUT)
   {
      if (dr.halt_out)
      {
         dr.stalls++;
         return -EPIPE;
      }
      if (dr.phase == CBW)
      {
         uint32_t sig = p[0] | (p[1] << 8) | (p[2] << 16)
            | ((uint32_t)p[3] << 24);
         if (len != 31 || sig != 0x43425355u || (p[12] & 0x7f)
               || p[14] < 1 || p[14] > 16 || p[13] >= dr.luns)
         {
            PROTO("invalid command block wrapper");
            dr.halt_in = dr.halt_out = dr.need_reset = 1;
            return -EPIPE;
         }
         dr.cmds++;
         dr.tag    = p[4] | (p[5] << 8) | (p[6] << 16)
            | ((uint32_t)p[7] << 24);
         if (dr.tag == dr.last_tag)
            PROTO("tag reused");
         dr.last_tag = dr.tag;
         dr.expect = p[8] | (p[9] << 8) | (p[10] << 16)
            | ((uint32_t)p[11] << 24);
         dr.lun    = p[13];
         dr.done   = 0;
         scsi(p + 15, (p[12] & 0x80) != 0);
         if (!rnd(60))
         {
            /* Something went wrong inside the drive. */
            dr.phase_errors++;
            dr.status   = 2;
            dr.data_len = 0;
         }
         if (!dr.expect)
            dr.phase = CSW;
         else if (p[12] & 0x80)
            dr.phase = DATA_IN;
         else
            dr.phase = DATA_OUT;
         dr.residue = dr.expect;
         if (dr.phase == DATA_IN && dr.data_len > dr.expect)
            dr.data_len = dr.expect;
         return 31;
      }
      if (dr.phase == DATA_OUT)
      {
         uint32_t take = len < dr.expect - dr.done ? len : dr.expect - dr.done;
         if (len > dr.expect - dr.done)
            PROTO("more data out than the wrapper said");
         if (dr.status == 0 && dr.data_len)
         {
            uint32_t at = dr.done, k;
            for (k = 0; k < take; k++)
               dr.data[at + k] = p[k];
         }
         dr.done += take;
         if (dr.done == dr.expect)
         {
            if (dr.status == 0 && dr.data_len)
            {
               uint32_t k;
               for (k = 0; k < dr.data_len / dr.bs; k++)
                  store_write(dr.store, dr.w_lba + k, dr.data + k * dr.bs);
            }
            dr.residue = dr.expect - (dr.status ? 0 : dr.data_len);
            dr.phase   = CSW;
         }
         return (int)take;
      }
      PROTO("bulk out in the wrong phase");
      return -EPIPE;
   }
   if (ep != EP_IN)
   {
      PROTO("unknown endpoint");
      return -EPIPE;
   }
   if (dr.halt_in)
   {
      dr.stalls++;
      return -EPIPE;
   }
   if (dr.phase == DATA_IN)
   {
      uint32_t left = dr.data_len - dr.done, n = len < left ? len : left;
      if (!left)
      {
         /* Nothing (more) to send: stall, or a short (empty) packet. */
         dr.phase   = CSW;
         dr.residue = dr.expect - dr.data_len;
         if (rnd(2))
         {
            dr.halt_in = 1;
            dr.stalls++;
            return -EPIPE;
         }
         return 0;
      }
      memcpy(p, dr.data + dr.done, n);
      dr.done += n;
      if (dr.done == dr.data_len)
      {
         dr.residue = dr.expect - dr.data_len;
         if (dr.data_len == dr.expect)
            dr.phase = CSW;
         else if (n < len)
            dr.phase = CSW;   /* the short packet ended it */
      }
      return (int)n;
   }
   if (dr.phase == CSW)
   {
      if (len < 13)
         PROTO("status read shorter than 13 bytes");
      if (!rnd(50))
      {
         /* A status stage that stalls once. */
         dr.halt_in = 1;
         dr.stalls++;
         return -EPIPE;
      }
      make_csw(p);
      dr.phase = CBW;
      return 13;
   }
   PROTO("bulk in while the drive waits for a command");
   return -EPIPE;
}

static int bus_ctrl(msc_bus *bus, uint8_t type, uint8_t request,
      uint16_t value, uint16_t index, void *buf, uint16_t len)
{
   (void)bus;
   if (type == 0x02 && request == 0x01 && value == 0)
   {
      if (dr.need_reset)
         return 0;   /* halts stay until the reset */
      if (index == EP_IN)
         dr.halt_in = 0;
      else if (index == EP_OUT)
         dr.halt_out = 0;
      else
         PROTO("CLEAR_FEATURE on an unknown endpoint");
      return 0;
   }
   if (type == 0x21 && request == 0xff)
   {
      dr.resets++;
      dr.need_reset = 0;
      dr.phase      = CBW;
      return 0;
   }
   if (type == 0xa1 && request == 0xfe)
   {
      if (dr.stall_lun)
         return -EPIPE;
      if (len != 1)
         PROTO("GET MAX LUN length");
      *(uint8_t*)buf = (uint8_t)(dr.luns - 1);
      return 1;
   }
   PROTO("unknown control request");
   return -EPIPE;
}

static uint32_t clock_ms;
static uint32_t bus_ms(msc_bus *bus)       { (void)bus; return clock_ms++; }
static void bus_sleep(msc_bus *bus, unsigned ms) { (void)bus; clock_ms += ms; }

static msc_bus bus = { NULL, bus_bulk, bus_ctrl, bus_ms, bus_sleep, 16384 };

/* ---- the test ---- */

static void run(uint64_t blocks, uint32_t bs, int luns, int stall_lun)
{
   static struct store store, shadow;
   static uint8_t buf[300 * 4096], want[4096];
   msc_disk d;
   unsigned i;
   int ret;

   memset(&dr, 0, sizeof(dr));
   store.bs = shadow.bs = bs;
   dr.store     = &store;
   dr.blocks    = blocks;
   dr.bs        = bs;
   dr.luns      = luns;
   dr.not_ready = 5;
   dr.stall_lun = stall_lun;

   ret = msc_init(&d, &bus, 0, EP_IN, EP_OUT);
   CHECK(ret == 0);
   if (ret)
      return;
   CHECK(d.dev.sectors == blocks && d.dev.sector_size == bs);
   CHECK(d.lun == luns - 1);
   CHECK(d.long_lba == (blocks > 0xffffffffu));

   for (i = 0; i < 300 && !failures; i++)
   {
      uint32_t n   = rnd(3) ? 1 + rnd(8) : 1 + rnd(300 * 4096 / bs);
      uint64_t lba = rnd(2) ? rnd(1000)
         : blocks - n - rnd(1000);
      uint32_t k, j;
      if (rnd(2))
      {
         for (k = 0; k < n * bs; k++)
            buf[k] = (uint8_t)rnd(256);
         CHECK(d.dev.write(&d.dev, lba, n, buf) == 0);
         for (k = 0; k < n; k++)
            store_write(&shadow, lba + k, buf + (size_t)k * bs);
      }
      else
      {
         CHECK(d.dev.read(&d.dev, lba, n, buf) == 0);
         for (k = 0; k < n; k++)
         {
            store_read(&shadow, lba + k, want);
            for (j = 0; j < bs && buf[(size_t)k * bs + j] == want[j]; j++)
               ;
            CHECK(j == bs);
         }
      }
   }
   CHECK(d.dev.read(&d.dev, blocks, 1, buf) != 0);
   /* Stalls are cleared one by one; only a phase error resets. */
   CHECK(dr.resets <= dr.phase_errors);
   printf("%.0f x %u, %d LUN(s): %lu commands, %lu stalls, %lu resets, "
         "%lu attentions, %lu phase errors\n", (double)blocks,
         bs, luns, dr.cmds, dr.stalls, dr.resets, dr.attentions,
         dr.phase_errors);
   store_free(&store);
   store_free(&shadow);
}

static void image(const char *path)
{
   static struct store store;
   msc_disk d;
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
   memset(&dr, 0, sizeof(dr));
   store.bs    = 512;
   store.image = (uint8_t*)malloc((size_t)size);
   CHECK(fread(store.image, 1, (size_t)size, fp) == (size_t)size);
   dr.store  = &store;
   dr.blocks = (uint64_t)size / 512;
   dr.bs     = 512;
   dr.luns   = 1;
   CHECK(msc_init(&d, &bus, 0, EP_IN, EP_OUT) == 0);
   CHECK(fat_mount(&vol, &d.dev) == 0);
   CHECK(fat_open(vol, "/hello.txt", O_RDONLY, &f) == 0);
   memset(text, 0, sizeof(text));
   CHECK(fat_read(f, text, sizeof(text) - 1) == 6 && !strcmp(text, "hello\n"));
   CHECK(fat_close(f) == 0);
   CHECK(fat_open(vol, "/usb.txt", O_WRONLY | O_CREAT, &f) == 0);
   CHECK(fat_write(f, "over usb\n", 9) == 9);
   CHECK(fat_close(f) == 0);
   CHECK(fat_unmount(vol) == 0);
   rewind(fp);
   CHECK(fwrite(store.image, 1, (size_t)size, fp) == (size_t)size);
   fclose(fp);
   free(store.image);
   printf("%s over usb: %s\n", path, failures ? "FAILED" : "ok");
}

int main(int argc, char **argv)
{
   if (argc > 1)
   {
      image(argv[1]);
      return failures ? 1 : 0;
   }
   /* Only single-LUN drives may stall GET MAX LUN. */
   run(1000000, 512, 1, 1);
   run(250000, 4096, 2, 0);
   run(0x180000000ull, 512, 2, 0);   /* 3 TiB: the 16-byte commands */
   printf("usbmsc: %s\n", failures ? "FAILED" : "ok");
   return failures ? 1 : 0;
}
