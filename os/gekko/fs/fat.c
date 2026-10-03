/* FAT12/16/32 with long file names, written from Microsoft's FAT
 * specification (fatgen103).
 *
 * Sectors pass through a small write-back cache; FAT sectors are
 * written to every FAT copy when they leave it.  Whole sectors of
 * file data bypass the cache and go to the device in runs as long as
 * the cluster chain stays contiguous. */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fat.h"

#define CACHE_SECTORS 64
#define MAX_NAME      255
#define DIRENT        32

#define ATTR_RO      0x01
#define ATTR_HIDDEN  0x02
#define ATTR_SYSTEM  0x04
#define ATTR_VOLUME  0x08
#define ATTR_DIR     0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LFN     0x0f

#define NT_LOWER_BASE 0x08
#define NT_LOWER_EXT  0x10

#define FREE_UNKNOWN 0xffffffffu

struct cache_ent
{
   uint64_t lba;
   uint8_t *data;
   uint32_t age;
   uint8_t  valid;
   uint8_t  dirty;
};

struct fat_vol
{
   gk_blockdev_t   *dev;            /* NULL once unmounted */
   uint32_t         handles;        /* open files and directories */
   fat_file        *open;           /* open files, linked by next */
   void            *lock;
   uint8_t         *pool;
   uint64_t         part_lba;
   uint64_t         fat_lba;
   uint64_t         root_lba;       /* FAT12/16 fixed root */
   uint64_t         data_lba;       /* cluster 2 */
   uint64_t         fsinfo_lba;     /* FAT32, 0 if none */
   struct cache_ent cache[CACHE_SECTORS];
   uint32_t         tick;
   uint32_t         bps;
   uint32_t         spc;            /* sectors per cluster */
   uint32_t         clus_size;
   uint32_t         fat_sectors;
   uint32_t         root_entries;
   uint32_t         clusters;       /* data clusters, numbered 2.. */
   uint32_t         root_clus;      /* FAT32 */
   uint32_t         next_free;
   uint32_t         free_count;
   uint32_t         nfats;
   /* The last directory walk, so sequential entries cost one step. */
   uint32_t         walk_dir, walk_n, walk_clus;
   uint8_t          type;           /* 12, 16 or 32 */
   uint8_t          fsinfo_dirty;
};

struct fat_file
{
   fat_vol  *v;
   uint64_t  pos;
   uint32_t  size;
   uint32_t  first;          /* first cluster, 0 if empty */
   uint32_t  clus;           /* cluster holding pos (when known) */
   uint32_t  clus_n;         /* its index in the chain */
   uint32_t  dir;            /* directory holding the entry */
   uint32_t  entry;          /* index of the short entry */
   int       flags;
   uint8_t   dirty;
   fat_file *next;
};

struct fat_dir
{
   fat_vol  *v;
   uint32_t  clus;           /* 0: the FAT12/16 root */
   uint32_t  n;
};

/* ---- little-endian fields ---- */

static uint32_t rd16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static uint32_t rd32(const uint8_t *p)
{
   return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void wr16(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)v;
   p[1] = (uint8_t)(v >> 8);
}
static void wr32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)v;
   p[1] = (uint8_t)(v >> 8);
   p[2] = (uint8_t)(v >> 16);
   p[3] = (uint8_t)(v >> 24);
}

/* ---- sector cache ---- */

static int dev_read(fat_vol *v, uint64_t lba, uint32_t n, void *buf)
{
   if (!v->dev)
      return -EIO;
   return v->dev->read(v->dev, lba, n, buf) ? -EIO : 0;
}

static int dev_write(fat_vol *v, uint64_t lba, uint32_t n, const void *buf)
{
   if (!v->dev)
      return -EIO;
   return v->dev->write(v->dev, lba, n, buf) ? -EIO : 0;
}

static int cache_writeback(fat_vol *v, struct cache_ent *e)
{
   uint32_t k;
   if (!e->valid || !e->dirty)
      return 0;
   if (dev_write(v, e->lba, 1, e->data))
      return -EIO;
   /* FAT sectors go to every copy. */
   if (e->lba >= v->fat_lba && e->lba < v->fat_lba + v->fat_sectors)
      for (k = 1; k < v->nfats; k++)
         if (dev_write(v, e->lba + (uint64_t)k * v->fat_sectors, 1, e->data))
            return -EIO;
   e->dirty = 0;
   return 0;
}

/* The cached copy of a sector, loading it unless `fresh` (the caller
 * overwrites all of it). */
static uint8_t *cache_get(fat_vol *v, uint64_t lba, int fresh)
{
   struct cache_ent *victim = NULL;
   unsigned i;
   v->tick++;
   for (i = 0; i < CACHE_SECTORS; i++)
   {
      struct cache_ent *e = &v->cache[i];
      if (e->valid && e->lba == lba)
      {
         e->age = v->tick;
         return e->data;
      }
      if (!victim || !e->valid || (victim->valid && e->age < victim->age))
         victim = e;
   }
   if (cache_writeback(v, victim))
      return NULL;
   victim->valid = 0;
   if (!fresh && dev_read(v, lba, 1, victim->data))
      return NULL;
   if (fresh)
      memset(victim->data, 0, v->bps);
   victim->lba   = lba;
   victim->valid = 1;
   victim->dirty = 0;
   victim->age   = v->tick;
   return victim->data;
}

static void cache_dirty(fat_vol *v, uint64_t lba)
{
   unsigned i;
   for (i = 0; i < CACHE_SECTORS; i++)
      if (v->cache[i].valid && v->cache[i].lba == lba)
      {
         v->cache[i].dirty = 1;
         return;
      }
}

/* Before a direct transfer over [lba, lba + n): write back what is
 * dirty there; for a write, drop the cached copies it replaces. */
static int cache_range(fat_vol *v, uint64_t lba, uint32_t n, int writing)
{
   unsigned i;
   for (i = 0; i < CACHE_SECTORS; i++)
   {
      struct cache_ent *e = &v->cache[i];
      if (!e->valid || e->lba < lba || e->lba >= lba + n)
         continue;
      if (writing)
         e->valid = e->dirty = 0;
      else if (cache_writeback(v, e))
         return -EIO;
   }
   return 0;
}

static int cache_flush(fat_vol *v)
{
   unsigned i;
   for (i = 0; i < CACHE_SECTORS; i++)
      if (cache_writeback(v, &v->cache[i]))
         return -EIO;
   return 0;
}

/* ---- the allocation table ---- */

static uint32_t eoc(fat_vol *v)
{
   return v->type == 12 ? 0xfff : v->type == 16 ? 0xffff : 0x0fffffff;
}

static int is_eoc(fat_vol *v, uint32_t c)
{
   return c >= (v->type == 12 ? 0xff8u : v->type == 16 ? 0xfff8u
         : 0x0ffffff8u);
}

static int fat_byte(fat_vol *v, uint32_t off, uint8_t **p)
{
   uint8_t *s = cache_get(v, v->fat_lba + off / v->bps, 0);
   if (!s)
      return -EIO;
   *p = s + off % v->bps;
   return 0;
}

/* Next cluster in a chain; 0xffffffff on an I/O error. */
static uint32_t fat_get(fat_vol *v, uint32_t c)
{
   uint8_t *p, *q;
   uint32_t off, val;
   switch (v->type)
   {
      case 32:
         if (fat_byte(v, c * 4, &p))
            return 0xffffffffu;
         return rd32(p) & 0x0fffffff;
      case 16:
         if (fat_byte(v, c * 2, &p))
            return 0xffffffffu;
         return rd16(p);
   }
   off = c + c / 2;
   if (fat_byte(v, off, &p))
      return 0xffffffffu;
   val = *p;
   if (fat_byte(v, off + 1, &q))
      return 0xffffffffu;
   val |= (uint32_t)*q << 8;
   return (c & 1) ? val >> 4 : val & 0xfff;
}

static int fat_set(fat_vol *v, uint32_t c, uint32_t val)
{
   uint8_t *p, *q;
   uint32_t off;
   switch (v->type)
   {
      case 32:
         off = c * 4;
         if (fat_byte(v, off, &p))
            return -EIO;
         wr32(p, (rd32(p) & 0xf0000000u) | (val & 0x0fffffff));
         cache_dirty(v, v->fat_lba + off / v->bps);
         return 0;
      case 16:
         off = c * 2;
         if (fat_byte(v, off, &p))
            return -EIO;
         wr16(p, val);
         cache_dirty(v, v->fat_lba + off / v->bps);
         return 0;
   }
   off = c + c / 2;
   if (fat_byte(v, off, &p))
      return -EIO;
   if (c & 1)
      *p = (uint8_t)((*p & 0x0f) | ((val << 4) & 0xf0));
   else
      *p = (uint8_t)val;
   cache_dirty(v, v->fat_lba + off / v->bps);
   if (fat_byte(v, off + 1, &q))
      return -EIO;
   if (c & 1)
      *q = (uint8_t)(val >> 4);
   else
      *q = (uint8_t)((*q & 0xf0) | ((val >> 8) & 0x0f));
   cache_dirty(v, v->fat_lba + (off + 1) / v->bps);
   return 0;
}

/* A free cluster, marked end of chain and linked after prev (if any).
 * 0 when the volume is full. */
static uint32_t alloc_cluster(fat_vol *v, uint32_t prev)
{
   uint32_t i, c = (prev && prev + 1 < v->clusters + 2) ? prev + 1
      : v->next_free;
   for (i = 0; i < v->clusters; i++, c++)
   {
      uint32_t val;
      if (c < 2 || c >= v->clusters + 2)
         c = 2;
      if ((val = fat_get(v, c)) == 0xffffffffu)
         return 0;
      if (val)
         continue;
      if (fat_set(v, c, eoc(v)) || (prev && fat_set(v, prev, c)))
         return 0;
      v->next_free = c + 1;
      if (v->free_count != FREE_UNKNOWN)
         v->free_count--;
      v->fsinfo_dirty = 1;
      return c;
   }
   return 0;
}

static int free_chain(fat_vol *v, uint32_t c)
{
   while (c >= 2 && c < v->clusters + 2)
   {
      uint32_t next = fat_get(v, c);
      if (next == 0xffffffffu || fat_set(v, c, 0))
         return -EIO;
      if (v->free_count != FREE_UNKNOWN)
         v->free_count++;
      v->fsinfo_dirty = 1;
      if (is_eoc(v, next))
         break;
      c = next;
   }
   return 0;
}

static uint64_t clus_lba(fat_vol *v, uint32_t c)
{
   return v->data_lba + (uint64_t)(c - 2) * v->spc;
}

static int zero_cluster(fat_vol *v, uint32_t c)
{
   static const uint8_t zeros[4096];
   uint64_t lba = clus_lba(v, c);
   uint32_t i;
   if (cache_range(v, lba, v->spc, 1))
      return -EIO;
   for (i = 0; i < v->spc; i++)
      if (dev_write(v, lba + i, 1, zeros))
         return -EIO;
   return 0;
}

/* ---- mounting ---- */

static int bpb_plausible(const uint8_t *s)
{
   uint32_t bps = rd16(s + 11), spc = s[13];
   return (s[0] == 0xeb || s[0] == 0xe9)
      && (bps == 512 || bps == 1024 || bps == 2048 || bps == 4096)
      && spc && !(spc & (spc - 1)) && rd16(s + 14) && s[16] >= 1
      && (s[21] == 0xf0 || s[21] >= 0xf8)
      && (rd16(s + 19) || rd32(s + 32)) && (rd16(s + 22) || rd32(s + 36));
}

static int read_bpb(fat_vol *v, const uint8_t *s)
{
   uint32_t total, fat_sz, root_secs, data_secs;
   v->bps          = rd16(s + 11);
   v->spc          = s[13];
   v->nfats        = s[16];
   v->root_entries = rd16(s + 17);
   total           = rd16(s + 19) ? rd16(s + 19) : rd32(s + 32);
   fat_sz          = rd16(s + 22) ? rd16(s + 22) : rd32(s + 36);
   if (v->bps != v->dev->sector_size || !fat_sz || !total)
      return -EINVAL;
   root_secs       = (v->root_entries * DIRENT + v->bps - 1) / v->bps;
   v->fat_sectors  = fat_sz;
   v->clus_size    = v->bps * v->spc;
   v->fat_lba      = v->part_lba + rd16(s + 14);
   v->root_lba     = v->fat_lba + (uint64_t)v->nfats * fat_sz;
   v->data_lba     = v->root_lba + root_secs;
   data_secs       = total - (uint32_t)(v->data_lba - v->part_lba);
   v->clusters     = data_secs / v->spc;
   v->type         = v->clusters < 4085 ? 12 : v->clusters < 65525 ? 16 : 32;
   v->free_count   = FREE_UNKNOWN;
   v->next_free    = 2;
   if (v->type == 32)
   {
      v->root_clus = rd32(s + 44);
      if (rd16(s + 48) && rd16(s + 48) != 0xffff)
         v->fsinfo_lba = v->part_lba + rd16(s + 48);
   }
   return 0;
}

static void read_fsinfo(fat_vol *v)
{
   uint8_t *s;
   if (!v->fsinfo_lba || !(s = cache_get(v, v->fsinfo_lba, 0)))
      return;
   if (rd32(s) != 0x41615252u || rd32(s + 484) != 0x61417272u)
   {
      v->fsinfo_lba = 0;
      return;
   }
   if (rd32(s + 488) <= v->clusters)
      v->free_count = rd32(s + 488);
   if (rd32(s + 492) >= 2 && rd32(s + 492) < v->clusters + 2)
      v->next_free = rd32(s + 492);
}

int fat_mount(fat_vol **out, gk_blockdev_t *dev)
{
   fat_vol *v;
   uint8_t *s;
   unsigned i;
   int ret;

   if (!dev || dev->sector_size < 512 || dev->sector_size > 4096)
      return -EINVAL;
   if (!(v = (fat_vol*)calloc(1, sizeof(*v))))
      return -ENOMEM;
   v->dev = dev;
   v->bps = dev->sector_size;
   if (!(v->pool = (uint8_t*)malloc((size_t)CACHE_SECTORS * dev->sector_size)))
   {
      free(v);
      return -ENOMEM;
   }
   for (i = 0; i < CACHE_SECTORS; i++)
      v->cache[i].data = v->pool + (size_t)i * dev->sector_size;

   ret = -EINVAL;
   if (!(s = cache_get(v, 0, 0)) || s[510] != 0x55 || s[511] != 0xaa)
      goto fail;
   if (!bpb_plausible(s))
   {
      /* A partition table: the first FAT partition. */
      static const uint8_t types[] = { 0x01, 0x04, 0x06, 0x0b, 0x0c, 0x0e };
      uint8_t pe[64];
      memcpy(pe, s + 446, 64);
      for (i = 0; i < 4 && !v->part_lba; i++)
         if (memchr(types, pe[i * 16 + 4], sizeof(types)) && rd32(pe + i * 16 + 8))
            v->part_lba = rd32(pe + i * 16 + 8);
      if (!v->part_lba || !(s = cache_get(v, v->part_lba, 0))
            || !bpb_plausible(s))
         goto fail;
   }
   if ((ret = read_bpb(v, s)))
      goto fail;
   read_fsinfo(v);
   v->lock = fat_lock_create();
   *out = v;
   return 0;

fail:
   free(v->pool);
   free(v);
   return ret;
}

static int write_fsinfo(fat_vol *v)
{
   uint8_t *s;
   if (!v->fsinfo_lba || !v->fsinfo_dirty)
      return 0;
   if (!(s = cache_get(v, v->fsinfo_lba, 0)))
      return -EIO;
   wr32(s + 488, v->free_count);
   wr32(s + 492, v->next_free);
   cache_dirty(v, v->fsinfo_lba);
   v->fsinfo_dirty = 0;
   return 0;
}

int fat_sync(fat_vol *v)
{
   int ret;
   fat_lock_acquire(v->lock);
   ret = write_fsinfo(v);
   if (!ret)
      ret = cache_flush(v);
   fat_lock_release(v->lock);
   return ret;
}

static void destroy(fat_vol *v)
{
   fat_lock_destroy(v->lock);
   free(v->pool);
   free(v);
}

/* Drops a handle; the last one of an unmounted volume frees it. */
static void put_handle(fat_vol *v)
{
   int last;
   fat_lock_acquire(v->lock);
   last = !--v->handles && !v->dev;
   fat_lock_release(v->lock);
   if (last)
      destroy(v);
}

/* Handles still open fail from here on, and the volume goes with the
 * last of them. */
int fat_unmount(fat_vol *v)
{
   int ret, last;
   fat_lock_acquire(v->lock);
   if (!(ret = write_fsinfo(v)))
      ret = cache_flush(v);
   v->dev = NULL;
   last   = !v->handles;
   fat_lock_release(v->lock);
   if (last)
      destroy(v);
   return ret;
}

/* ---- directories as arrays of 32-byte entries ---- */

/* The sector holding entry n of directory dir (cluster, or 0 for the
 * FAT12/16 root) and the entry's offset in it.  With grow, a missing
 * cluster is added (zeroed); -ENOSPC past the fixed root. */
static int dir_locate(fat_vol *v, uint32_t dir, uint32_t n, int grow,
      uint64_t *lba, uint32_t *off)
{
   uint32_t per_clus = v->clus_size / DIRENT, want, c, at;
   if (!dir && v->type != 32)
   {
      if (n >= v->root_entries)
         return grow ? -ENOSPC : -ENOENT;
      *lba = v->root_lba + (uint64_t)n * DIRENT / v->bps;
      *off = (n * DIRENT) % v->bps;
      return 0;
   }
   if (!dir)
      dir = v->root_clus;
   want = n / per_clus;
   if (v->walk_dir == dir && v->walk_n <= want)
   {
      c  = v->walk_clus;
      at = v->walk_n;
   }
   else
   {
      c  = dir;
      at = 0;
   }
   while (at < want)
   {
      uint32_t next = fat_get(v, c);
      if (next == 0xffffffffu)
         return -EIO;
      if (is_eoc(v, next) || next < 2)
      {
         if (!grow)
            return -ENOENT;
         if (!(next = alloc_cluster(v, c)))
            return -ENOSPC;
         if (zero_cluster(v, next))
            return -EIO;
      }
      c = next;
      at++;
   }
   v->walk_dir  = dir;
   v->walk_n    = at;
   v->walk_clus = c;
   *lba = clus_lba(v, c) + (uint64_t)(n % per_clus) * DIRENT / v->bps;
   *off = ((n % per_clus) * DIRENT) % v->bps;
   return 0;
}

static int dir_read(fat_vol *v, uint32_t dir, uint32_t n, uint8_t *e)
{
   uint64_t lba;
   uint32_t off;
   uint8_t *s;
   int ret = dir_locate(v, dir, n, 0, &lba, &off);
   if (ret)
      return ret;
   if (!(s = cache_get(v, lba, 0)))
      return -EIO;
   memcpy(e, s + off, DIRENT);
   return 0;
}

static int dir_write(fat_vol *v, uint32_t dir, uint32_t n, const uint8_t *e)
{
   uint64_t lba;
   uint32_t off;
   uint8_t *s;
   int ret = dir_locate(v, dir, n, 1, &lba, &off);
   if (ret)
      return ret;
   if (!(s = cache_get(v, lba, 0)))
      return -EIO;
   memcpy(s + off, e, DIRENT);
   cache_dirty(v, lba);
   return 0;
}

static uint32_t ent_cluster(fat_vol *v, const uint8_t *e)
{
   return rd16(e + 26) | (v->type == 32 ? rd16(e + 20) << 16 : 0);
}

static void ent_set_cluster(uint8_t *e, uint32_t c)
{
   wr16(e + 26, c & 0xffff);
   wr16(e + 20, c >> 16);
}

/* ---- names ---- */

static unsigned char lfn_sum(const uint8_t *short_name)
{
   unsigned char sum = 0;
   unsigned i;
   for (i = 0; i < 11; i++)
      sum = (unsigned char)(((sum & 1) << 7) + (sum >> 1) + short_name[i]);
   return sum;
}

/* UTF-8 to UTF-16; returns units written, -1 if invalid or too long. */
static int utf8_to_16(const char *s, size_t len, uint16_t *out, int max)
{
   int n = 0;
   size_t i = 0;
   while (i < len)
   {
      uint32_t c = (unsigned char)s[i];
      unsigned extra = c < 0x80 ? 0 : c < 0xe0 ? 1 : c < 0xf0 ? 2 : 3;
      if (c >= 0x80 && c < 0xc0)
         return -1;
      if (c >= 0xf8)
         return -1;
      if (extra)
         c &= 0x3f >> extra;
      i++;
      while (extra--)
      {
         if (i >= len || ((unsigned char)s[i] & 0xc0) != 0x80)
            return -1;
         c = (c << 6) | ((unsigned char)s[i++] & 0x3f);
      }
      if (c >= 0x10000)
      {
         if (n + 2 > max)
            return -1;
         c -= 0x10000;
         out[n++] = (uint16_t)(0xd800 | (c >> 10));
         out[n++] = (uint16_t)(0xdc00 | (c & 0x3ff));
      }
      else
      {
         if (n + 1 > max)
            return -1;
         out[n++] = (uint16_t)c;
      }
   }
   return n;
}

static size_t utf16_to_8(const uint16_t *in, int n, char *out, size_t size)
{
   size_t o = 0;
   int i;
   for (i = 0; i < n; i++)
   {
      uint32_t c = in[i];
      if (c >= 0xd800 && c < 0xdc00 && i + 1 < n
            && in[i + 1] >= 0xdc00 && in[i + 1] < 0xe000)
      {
         c = 0x10000 + ((c - 0xd800) << 10) + (in[i + 1] - 0xdc00);
         i++;
      }
      if (c < 0x80 && o + 1 < size)
         out[o++] = (char)c;
      else if (c < 0x800 && o + 2 < size)
      {
         out[o++] = (char)(0xc0 | (c >> 6));
         out[o++] = (char)(0x80 | (c & 0x3f));
      }
      else if (c < 0x10000 && o + 3 < size)
      {
         out[o++] = (char)(0xe0 | (c >> 12));
         out[o++] = (char)(0x80 | ((c >> 6) & 0x3f));
         out[o++] = (char)(0x80 | (c & 0x3f));
      }
      else if (o + 4 < size)
      {
         out[o++] = (char)(0xf0 | (c >> 18));
         out[o++] = (char)(0x80 | ((c >> 12) & 0x3f));
         out[o++] = (char)(0x80 | ((c >> 6) & 0x3f));
         out[o++] = (char)(0x80 | (c & 0x3f));
      }
   }
   out[o] = '\0';
   return o;
}

static uint16_t fold(uint16_t c)
{
   return (c >= 'a' && c <= 'z') ? (uint16_t)(c - 32) : c;
}

static int name_eq(const uint16_t *a, int an, const uint16_t *b, int bn)
{
   int i;
   if (an != bn)
      return 0;
   for (i = 0; i < an; i++)
      if (fold(a[i]) != fold(b[i]))
         return 0;
   return 1;
}

/* The display name of a short entry, honouring the lower-case bits. */
static int short_name16(const uint8_t *e, uint16_t *out)
{
   int n = 0, i, end;
   for (end = 8; end > 0 && e[end - 1] == ' '; end--)
      ;
   for (i = 0; i < end; i++)
   {
      uint8_t c = (i == 0 && e[0] == 0x05) ? 0xe5 : e[i];
      out[n++] = (e[12] & NT_LOWER_BASE && c >= 'A' && c <= 'Z')
         ? (uint16_t)(c + 32) : c;
   }
   for (end = 11; end > 8 && e[end - 1] == ' '; end--)
      ;
   if (end > 8)
   {
      out[n++] = '.';
      for (i = 8; i < end; i++)
         out[n++] = (e[12] & NT_LOWER_EXT && e[i] >= 'A' && e[i] <= 'Z')
            ? (uint16_t)(e[i] + 32) : e[i];
   }
   return n;
}

/* ---- directory search ---- */

struct found
{
   uint8_t  e[DIRENT];     /* the short entry */
   uint32_t index;         /* where it is */
   uint32_t first;         /* first entry of its long name, or index */
   uint16_t name[MAX_NAME + 1];
   int      name_len;
};

/* Entry n onwards: the next live entry with its long name.  1 if one
 * was found, 0 at the end. */
static const uint8_t lfn_pos[13] = { 1, 3, 5, 7, 9, 14, 16, 18, 20, 22,
   24, 28, 30 };

static int dir_next(fat_vol *v, uint32_t dir, uint32_t *n, struct found *f)
{
   uint8_t e[DIRENT];
   uint16_t lfn[20 * 13];
   unsigned lfn_total = 0, expect = 0, k;
   unsigned char lfn_check = 0;
   uint32_t lfn_first = 0;

   for (;; (*n)++)
   {
      int ret = dir_read(v, dir, *n, e);
      if (ret == -ENOENT)
         return 0;
      if (ret)
         return ret;
      if (e[0] == 0x00)
         return 0;
      if (e[0] == 0xe5)
      {
         lfn_total = 0;
         continue;
      }
      if ((e[11] & 0x3f) == ATTR_LFN)
      {
         unsigned ord = e[0] & 0x3f;
         if (e[0] & 0x40)
         {
            /* The last piece of a name comes first. */
            lfn_total = (ord >= 1 && ord <= 20) ? ord : 0;
            expect    = ord;
            lfn_check = e[13];
            lfn_first = *n;
            if (lfn_total)
               memset(lfn, 0xff, sizeof(lfn));
         }
         if (!lfn_total || ord != expect || e[13] != lfn_check)
         {
            lfn_total = 0;
            continue;
         }
         for (k = 0; k < 13; k++)
            lfn[(ord - 1) * 13 + k] = (uint16_t)rd16(e + lfn_pos[k]);
         expect--;
         continue;
      }
      if (e[11] & ATTR_VOLUME)
      {
         lfn_total = 0;
         continue;
      }
      memcpy(f->e, e, DIRENT);
      f->index    = *n;
      f->first    = *n;
      f->name_len = 0;
      if (lfn_total && !expect && lfn_sum(e) == lfn_check)
      {
         int len = 0;
         while (len < (int)lfn_total * 13 && len < MAX_NAME
               && lfn[len] != 0 && lfn[len] != 0xffff)
            len++;
         memcpy(f->name, lfn, (size_t)len * 2);
         f->name_len = len;
         f->first    = lfn_first;
      }
      if (!f->name_len)
         f->name_len = short_name16(e, f->name);
      (*n)++;
      return 1;
   }
}

static int dir_find(fat_vol *v, uint32_t dir, const uint16_t *name, int len,
      struct found *f)
{
   uint32_t n = 0;
   int ret;
   uint16_t sn[13];
   while ((ret = dir_next(v, dir, &n, f)) == 1)
   {
      int sl;
      if (name_eq(f->name, f->name_len, name, len))
         return 0;
      sl = short_name16(f->e, sn);
      if (name_eq(sn, sl, name, len))
         return 0;
   }
   return ret ? ret : -ENOENT;
}

/* ---- paths ---- */

/* Walk to the directory holding the last component; returns that
 * directory and the component.  dir 0 means the root. */
static int walk(fat_vol *v, const char *path, uint32_t *dir,
      uint16_t *leaf, int *leaf_len)
{
   const char *p = path, *end;
   const char *colon = strchr(path, ':');
   uint32_t d = 0;
   if (colon)
      p = colon + 1;
   *leaf_len = 0;
   for (;;)
   {
      size_t len;
      while (*p == '/')
         p++;
      if (!*p)
      {
         *dir = d;
         return 0;
      }
      end = p;
      while (*end && *end != '/')
         end++;
      len = (size_t)(end - p);
      {
         const char *rest = end;
         while (*rest == '/')
            rest++;
         if (!*rest)
         {
            /* The last component. */
            if ((*leaf_len = utf8_to_16(p, len, leaf, MAX_NAME)) <= 0)
               return -ENAMETOOLONG;
            *dir = d;
            return 0;
         }
      }
      if (len == 1 && p[0] == '.')
         ;
      else
      {
         struct found f;
         uint16_t name[MAX_NAME];
         int nl = utf8_to_16(p, len, name, MAX_NAME), ret;
         if (nl <= 0)
            return -ENAMETOOLONG;
         if ((ret = dir_find(v, d, name, nl, &f)))
            return ret;
         if (!(f.e[11] & ATTR_DIR))
            return -ENOTDIR;
         d = ent_cluster(v, f.e);
         if (d == v->root_clus && v->type == 32)
            d = 0;
      }
      p = end;
   }
}

/* Resolve a whole path to its entry.  The root has no entry: 1. */
static int lookup(fat_vol *v, const char *path, uint32_t *dir,
      struct found *f)
{
   uint16_t leaf[MAX_NAME];
   int len, ret;
   if ((ret = walk(v, path, dir, leaf, &len)))
      return ret;
   if (!len)
      return 1;
   if (len == 1 && leaf[0] == '.')
      return -EINVAL;
   return dir_find(v, *dir, leaf, len, f);
}

/* ---- creating entries ---- */

static int valid_short_char(uint16_t c)
{
   if (c >= 'A' && c <= 'Z')
      return 1;
   if (c >= '0' && c <= '9')
      return 1;
   return c > 127 ? 0 : strchr("!#$%&'()-@^_`{}~", (int)c) != NULL && c;
}

/* An 8.3 form of the name; 1 if it stands for the name exactly
 * (with the case flags in *nt), 0 if a ~N alias is needed. */
static int make_short(const uint16_t *name, int len, uint8_t *out,
      uint8_t *nt)
{
   int i, dot = -1, b = 0, x = 0, exact = 1, lower_b = 0, upper_b = 0;
   int lower_x = 0, upper_x = 0;
   memset(out, ' ', 11);
   *nt = 0;
   for (i = len - 1; i >= 0; i--)
      if (name[i] == '.')
      {
         dot = i;
         break;
      }
   if (dot == 0)
      dot = -1;     /* ".name" has no extension */
   if (len > 12)
      exact = 0;
   for (i = 0; i < len; i++)
   {
      uint16_t c = name[i];
      int ext = dot >= 0 && i > dot;
      if (i == dot)
         continue;
      if (c == '.' || c == ' ')
      {
         exact = 0;
         continue;
      }
      if (c >= 'a' && c <= 'z')
      {
         if (ext)
            lower_x = 1;
         else
            lower_b = 1;
         c = (uint16_t)(c - 32);
      }
      else if (c >= 'A' && c <= 'Z')
      {
         if (ext)
            upper_x = 1;
         else
            upper_b = 1;
      }
      if (!valid_short_char(c))
      {
         exact = 0;
         c = '_';
      }
      if (ext)
      {
         if (x < 3)
            out[8 + x++] = (uint8_t)c;
         else
            exact = 0;
      }
      else if (b < 8)
         out[b++] = (uint8_t)c;
      else
         exact = 0;
   }
   if (!b)
   {
      exact = 0;
      out[0] = '_';
   }
   if ((lower_b && upper_b) || (lower_x && upper_x))
      exact = 0;
   if (exact)
      *nt = (uint8_t)((lower_b ? NT_LOWER_BASE : 0)
            | (lower_x ? NT_LOWER_EXT : 0));
   if (out[0] == 0xe5)
      out[0] = 0x05;
   return exact;
}

static int short_exists(fat_vol *v, uint32_t dir, const uint8_t *sn)
{
   uint32_t n = 0;
   struct found f;
   int ret;
   while ((ret = dir_next(v, dir, &n, &f)) == 1)
      if (!memcmp(f.e, sn, 11))
         return 1;
   return ret < 0 ? ret : 0;
}

/* Add the ~N tail that makes the alias unique in dir. */
static int unique_short(fat_vol *v, uint32_t dir, uint8_t *sn)
{
   uint8_t base[8];
   unsigned k;
   int b, ret;
   memcpy(base, sn, 8);
   for (b = 0; b < 8 && base[b] != ' '; b++)
      ;
   for (k = 1; k < 1000000; k++)
   {
      char tail[8];
      int tl = 0, keep, i;
      unsigned t = k;
      char digits[8];
      int nd = 0;
      while (t)
      {
         digits[nd++] = (char)('0' + t % 10);
         t /= 10;
      }
      tail[tl++] = '~';
      while (nd)
         tail[tl++] = digits[--nd];
      keep = b < 8 - tl ? b : 8 - tl;
      memset(sn, ' ', 8);
      memcpy(sn, base, (size_t)keep);
      for (i = 0; i < tl; i++)
         sn[keep + i] = (uint8_t)tail[i];
      if ((ret = short_exists(v, dir, sn)) <= 0)
         return ret;
   }
   return -EEXIST;
}

/* n consecutive free entries in dir (growing it if it must). */
static int find_free(fat_vol *v, uint32_t dir, unsigned need, uint32_t *at)
{
   uint8_t e[DIRENT];
   uint32_t n = 0, run = 0, start = 0;
   for (;; n++)
   {
      int ret = dir_read(v, dir, n, e);
      if (ret == -ENOENT || (!ret && e[0] == 0x00))
      {
         /* From here on everything is free (or yet to exist). */
         if (!run)
            start = n;
         *at = start;
         return 0;
      }
      if (ret)
         return ret;
      if (e[0] == 0xe5)
      {
         if (!run++)
            start = n;
         if (run == need)
         {
            *at = start;
            return 0;
         }
      }
      else
         run = 0;
   }
}

static void fat_time(uint32_t t, uint16_t *date, uint16_t *time_)
{
   /* Days since 1970 to a civil date. */
   uint32_t days = t / 86400, secs = t % 86400;
   uint32_t z = days + 719468, era = z / 146097;
   uint32_t doe = z - era * 146097;
   uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
   uint32_t y = yoe + era * 400, doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
   uint32_t mp = (5 * doy + 2) / 153, d = doy - (153 * mp + 2) / 5 + 1;
   uint32_t m = mp < 10 ? mp + 3 : mp - 9;
   if (m <= 2)
      y++;
   if (y < 1980)
   {
      y = 1980;
      m = d = 1;
      secs = 0;
   }
   *date  = (uint16_t)(((y - 1980) << 9) | (m << 5) | d);
   *time_ = (uint16_t)(((secs / 3600) << 11) | (((secs / 60) % 60) << 5)
         | ((secs % 60) / 2));
}

static uint32_t unix_time(uint16_t date, uint16_t time_)
{
   uint32_t y = 1980 + (date >> 9), m = (date >> 5) & 15, d = date & 31;
   uint32_t era, yoe, doy, doe, days;
   if (m < 1 || m > 12 || !d)
      return 0;
   if (m <= 2)
      y--;
   era  = y / 400;
   yoe  = y - era * 400;
   doy  = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
   doe  = yoe * 365 + yoe / 4 - yoe / 100 + doy;
   days = era * 146097 + doe - 719468;
   return days * 86400 + (time_ >> 11) * 3600 + ((time_ >> 5) & 63) * 60
      + (time_ & 31) * 2;
}

static void stamp(uint8_t *e, int created)
{
   uint16_t d, t;
   fat_time(fat_now(), &d, &t);
   if (created)
   {
      e[13] = 0;
      wr16(e + 14, t);
      wr16(e + 16, d);
   }
   wr16(e + 18, d);
   wr16(e + 22, t);
   wr16(e + 24, d);
}

/* Write a new entry (long name entries, then e) for name into dir;
 * e[0..10] and e[12] are filled in here.  Returns the short entry's
 * index in *index. */
static int add_entry(fat_vol *v, uint32_t dir, const uint16_t *name,
      int len, uint8_t *e, uint32_t *index)
{
   uint8_t nt, sum;
   int exact = make_short(name, len, e, &nt), ret;
   unsigned lfn_n = 0, i, k;
   uint32_t at;

   for (i = 0; i < (unsigned)len; i++)
      if (name[i] < 0x20 || (name[i] < 0x80 && strchr("\\/:*?\"<>|", name[i])))
         return -EINVAL;
   if (!exact)
   {
      if ((ret = unique_short(v, dir, e)))
         return ret;
      lfn_n = (unsigned)(len + 12) / 13;
   }
   else if ((ret = short_exists(v, dir, e)) != 0)
      return ret < 0 ? ret : -EEXIST;
   e[12] = exact ? nt : 0;
   if ((ret = find_free(v, dir, lfn_n + 1, &at)))
      return ret;

   sum = lfn_sum(e);
   for (i = 0; i < lfn_n; i++)
   {
      unsigned ord = lfn_n - i;
      uint8_t l[DIRENT];
      memset(l, 0, sizeof(l));
      l[0]  = (uint8_t)(ord | (i == 0 ? 0x40 : 0));
      l[11] = ATTR_LFN;
      l[13] = sum;
      for (k = 0; k < 13; k++)
      {
         int c = (int)((ord - 1) * 13 + k);
         uint16_t u = c < len ? name[c] : c == len ? 0 : 0xffff;
         wr16(l + lfn_pos[k], u);
      }
      if ((ret = dir_write(v, dir, at + i, l)))
         return ret;
   }
   if ((ret = dir_write(v, dir, at + lfn_n, e)))
      return ret;
   *index = at + lfn_n;
   return 0;
}

static int remove_entry(fat_vol *v, uint32_t dir, const struct found *f)
{
   uint8_t e[DIRENT];
   uint32_t n;
   int ret;
   for (n = f->first; n <= f->index; n++)
   {
      if ((ret = dir_read(v, dir, n, e)))
         return ret;
      e[0] = 0xe5;
      if ((ret = dir_write(v, dir, n, e)))
         return ret;
   }
   return 0;
}

/* ---- files ---- */

static void fill_stat(fat_vol *v, const uint8_t *e, fat_stat *st)
{
   st->size      = rd32(e + 28);
   st->cluster   = ent_cluster(v, e);
   st->mtime     = unix_time((uint16_t)rd16(e + 24), (uint16_t)rd16(e + 22));
   st->is_dir    = (e[11] & ATTR_DIR) ? 1 : 0;
   st->read_only = (e[11] & ATTR_RO) ? 1 : 0;
}

int fat_open(fat_vol *v, const char *path, int flags, fat_file **out)
{
   struct found f;
   uint32_t dir;
   fat_file *h;
   int ret;
   int acc = flags & O_ACCMODE;

   fat_lock_acquire(v->lock);
   ret = lookup(v, path, &dir, &f);
   if (ret == 1)
      ret = -EISDIR;
   else if (ret == -ENOENT && (flags & O_CREAT))
   {
      uint16_t leaf[MAX_NAME];
      int len;
      if (!(ret = walk(v, path, &dir, leaf, &len)))
      {
         memset(f.e, 0, DIRENT);
         f.e[11] = ATTR_ARCHIVE;
         stamp(f.e, 1);
         ret = add_entry(v, dir, leaf, len, f.e, &f.index);
      }
   }
   else if (!ret && (flags & O_CREAT) && (flags & O_EXCL))
      ret = -EEXIST;
   else if (!ret && (f.e[11] & ATTR_DIR))
      ret = -EISDIR;
   else if (!ret && acc != O_RDONLY && (f.e[11] & ATTR_RO))
      ret = -EACCES;
   if (ret)
   {
      fat_lock_release(v->lock);
      return ret;
   }
   if (!(h = (fat_file*)calloc(1, sizeof(*h))))
   {
      fat_lock_release(v->lock);
      return -ENOMEM;
   }
   h->v     = v;
   h->next  = v->open;
   v->open  = h;
   v->handles++;
   h->dir   = dir;
   h->entry = f.index;
   h->flags = flags;
   h->first = ent_cluster(v, f.e);
   h->size  = rd32(f.e + 28);
   if ((flags & O_TRUNC) && acc != O_RDONLY && (h->size || h->first))
   {
      if ((ret = free_chain(v, h->first)))
      {
         v->open = h->next;
         v->handles--;
         free(h);
         fat_lock_release(v->lock);
         return ret;
      }
      h->first = 0;
      h->size  = 0;
      h->dirty = 1;
   }
   fat_lock_release(v->lock);
   *out = h;
   return 0;
}

static int update_entry(fat_file *h)
{
   uint8_t e[DIRENT];
   int ret;
   if (!h->dirty)
      return 0;
   if ((ret = dir_read(h->v, h->dir, h->entry, e)))
      return ret;
   ent_set_cluster(e, h->first);
   wr32(e + 28, h->size);
   e[11] |= ATTR_ARCHIVE;
   stamp(e, 0);
   if ((ret = dir_write(h->v, h->dir, h->entry, e)))
      return ret;
   h->dirty = 0;
   return 0;
}

int fat_fsync(fat_file *h)
{
   int ret;
   fat_lock_acquire(h->v->lock);
   ret = h->v->dev ? update_entry(h) : -EIO;
   if (!ret)
      ret = write_fsinfo(h->v);
   if (!ret)
      ret = cache_flush(h->v);
   fat_lock_release(h->v->lock);
   return ret;
}

int fat_close(fat_file *h)
{
   fat_file **p;
   int ret;
   fat_lock_acquire(h->v->lock);
   for (p = &h->v->open; *p; p = &(*p)->next)
      if (*p == h)
      {
         *p = h->next;
         break;
      }
   if (!h->v->dev)
      ret = h->dirty ? -EIO : 0;
   else if (!(ret = update_entry(h)) && !(ret = write_fsinfo(h->v))
         && (h->flags & O_ACCMODE) != O_RDONLY)
      ret = cache_flush(h->v);
   fat_lock_release(h->v->lock);
   put_handle(h->v);
   free(h);
   return ret;
}

/* Make h->clus the cluster with index idx in the chain, allocating
 * the missing ones when grow.  0, -ENOENT past the end, or errors. */
static int seek_cluster(fat_file *h, uint32_t idx, int grow)
{
   fat_vol *v = h->v;
   uint32_t c, at;
   if (!h->first)
   {
      if (!grow)
         return -ENOENT;
      if (!(h->first = alloc_cluster(v, 0)))
         return -ENOSPC;
      h->dirty  = 1;
      h->clus   = h->first;
      h->clus_n = 0;
   }
   if (h->clus && h->clus_n <= idx)
   {
      c  = h->clus;
      at = h->clus_n;
   }
   else
   {
      c  = h->first;
      at = 0;
   }
   while (at < idx)
   {
      uint32_t next = fat_get(v, c);
      if (next == 0xffffffffu)
         return -EIO;
      if (is_eoc(v, next) || next < 2)
      {
         if (!grow)
            return -ENOENT;
         if (!(next = alloc_cluster(v, c)))
            return -ENOSPC;
      }
      c = next;
      at++;
   }
   h->clus   = c;
   h->clus_n = at;
   return 0;
}

/* How many clusters from h->clus on are contiguous (up to max). */
static uint32_t run_length(fat_file *h, uint32_t max)
{
   uint32_t n = 1, c = h->clus;
   while (n < max)
   {
      uint32_t next = fat_get(h->v, c);
      if (next != c + 1)
         break;
      c = next;
      n++;
   }
   return n;
}

/* Move data between buf and the file at h->pos; the file already
 * covers [pos, pos + len) when writing. */
static long transfer(fat_file *h, uint8_t *buf, size_t len, int writing)
{
   fat_vol *v = h->v;
   size_t done = 0;
   while (done < len)
   {
      uint32_t idx = (uint32_t)(h->pos / v->clus_size);
      uint32_t in_clus = (uint32_t)(h->pos % v->clus_size);
      uint32_t sec = in_clus / v->bps, sec_off = in_clus % v->bps;
      uint64_t lba;
      int ret;
      if ((ret = seek_cluster(h, idx, writing)))
         return done ? (long)done : ret;
      lba = clus_lba(v, h->clus) + sec;
      if (!sec_off && len - done >= v->bps)
      {
         /* Whole sectors: as many as the contiguous clusters hold. */
         uint64_t want = (len - done) / v->bps;
         uint32_t clus_left = (uint32_t)((want + sec + v->spc - 1) / v->spc);
         uint32_t run = run_length(h, clus_left ? clus_left : 1);
         uint64_t avail = (uint64_t)run * v->spc - sec;
         uint32_t n = (uint32_t)(want < avail ? want : avail);
         if (n > 0x10000)
            n = 0x10000;
         if ((ret = cache_range(v, lba, n, writing)))
            return done ? (long)done : ret;
         ret = writing ? dev_write(v, lba, n, buf + done)
            : dev_read(v, lba, n, buf + done);
         if (ret)
            return done ? (long)done : ret;
         done   += (size_t)n * v->bps;
         h->pos += (uint64_t)n * v->bps;
         /* Leave clus at the cluster holding the last sector moved. */
         {
            uint32_t last = (uint32_t)((h->pos - 1) / v->clus_size);
            h->clus  += last - h->clus_n;
            h->clus_n = last;
         }
      }
      else
      {
         uint32_t n = v->bps - sec_off;
         uint8_t *s = cache_get(v, lba, 0);
         if (n > len - done)
            n = (uint32_t)(len - done);
         if (!s)
            return done ? (long)done : -EIO;
         if (writing)
         {
            memcpy(s + sec_off, buf + done, n);
            cache_dirty(v, lba);
         }
         else
            memcpy(buf + done, s + sec_off, n);
         done   += n;
         h->pos += n;
      }
   }
   return (long)done;
}

long fat_read(fat_file *h, void *buf, size_t len)
{
   long ret;
   if ((h->flags & O_ACCMODE) == O_WRONLY)
      return -EBADF;
   fat_lock_acquire(h->v->lock);
   if (!h->v->dev)
      ret = -EIO;
   else if (h->pos >= h->size)
      ret = 0;
   else
   {
      if (len > h->size - h->pos)
         len = (size_t)(h->size - h->pos);
      ret = transfer(h, (uint8_t*)buf, len, 0);
   }
   fat_lock_release(h->v->lock);
   return ret;
}

static long zero_fill(fat_file *h, uint64_t to)
{
   static const uint8_t zeros[512];
   while (h->pos < to)
   {
      size_t n = to - h->pos > sizeof(zeros) ? sizeof(zeros)
         : (size_t)(to - h->pos);
      long r = transfer(h, (uint8_t*)zeros, n, 1);
      if (r <= 0)
         return r ? r : -EIO;
      if (h->pos > h->size)
      {
         h->size  = (uint32_t)h->pos;
         h->dirty = 1;
      }
   }
   return 0;
}

long fat_write(fat_file *h, const void *buf, size_t len)
{
   long ret;
   uint64_t end;
   if ((h->flags & O_ACCMODE) == O_RDONLY)
      return -EBADF;
   fat_lock_acquire(h->v->lock);
   if (!h->v->dev)
   {
      fat_lock_release(h->v->lock);
      return -EIO;
   }
   if (h->flags & O_APPEND)
      h->pos = h->size;
   end = h->pos + len;
   if (end > 0xffffffffu)
   {
      fat_lock_release(h->v->lock);
      return -EFBIG;
   }
   if (h->pos > h->size)
   {
      uint64_t target = h->pos;
      h->pos = h->size;
      if ((ret = zero_fill(h, target)) < 0)
      {
         fat_lock_release(h->v->lock);
         return ret;
      }
   }
   if (len && end > h->size)
   {
      /* Allocate the clusters up front so they come out contiguous
       * and the data goes in long runs. */
      uint32_t clus = h->clus, clus_n = h->clus_n;
      ret = seek_cluster(h, (uint32_t)((end - 1) / h->v->clus_size), 1);
      if (h->first && clus)
      {
         h->clus   = clus;
         h->clus_n = clus_n;
      }
      if (ret)
      {
         h->dirty = 1;
         fat_lock_release(h->v->lock);
         return ret;
      }
   }
   ret = transfer(h, (uint8_t*)buf, len, 1);
   if (ret > 0 && h->pos > h->size)
      h->size = (uint32_t)h->pos;
   if (ret > 0)
      h->dirty = 1;
   fat_lock_release(h->v->lock);
   return ret;
}

int64_t fat_seek(fat_file *h, int64_t off, int whence)
{
   int64_t base = whence == SEEK_SET ? 0
      : whence == SEEK_CUR ? (int64_t)h->pos : (int64_t)h->size;
   if (base + off < 0)
      return -EINVAL;
   h->pos = (uint64_t)(base + off);
   return (int64_t)h->pos;
}

int fat_truncate(fat_file *h, uint64_t len)
{
   fat_vol *v = h->v;
   int ret = 0;
   if ((h->flags & O_ACCMODE) == O_RDONLY)
      return -EBADF;
   if (len > 0xffffffffu)
      return -EFBIG;
   fat_lock_acquire(v->lock);
   if (!v->dev)
      ret = -EIO;
   else if (len > h->size)
   {
      uint64_t pos = h->pos;
      h->pos = h->size;
      ret = (int)zero_fill(h, len);
      h->pos = pos;
   }
   else if (len < h->size)
   {
      if (!len)
      {
         ret = free_chain(v, h->first);
         h->first = 0;
      }
      else
      {
         uint32_t last = (uint32_t)((len - 1) / v->clus_size);
         if (!(ret = seek_cluster(h, last, 0)))
         {
            uint32_t next = fat_get(v, h->clus);
            if (next == 0xffffffffu)
               ret = -EIO;
            else if (!is_eoc(v, next) && next >= 2)
            {
               ret = fat_set(v, h->clus, eoc(v));
               if (!ret)
                  ret = free_chain(v, next);
            }
         }
      }
      h->clus = h->clus_n = 0;
      h->size  = (uint32_t)len;
      h->dirty = 1;
   }
   fat_lock_release(v->lock);
   return ret;
}

int fat_fstat(fat_file *h, fat_stat *st)
{
   uint8_t e[DIRENT];
   int ret;
   memset(st, 0, sizeof(*st));
   fat_lock_acquire(h->v->lock);
   if (!(ret = dir_read(h->v, h->dir, h->entry, e)))
      fill_stat(h->v, e, st);
   fat_lock_release(h->v->lock);
   st->size    = h->size;
   st->cluster = h->first;
   if (h->dirty)
      st->mtime = fat_now();
   return ret;
}

int fat_stat_path(fat_vol *v, const char *path, fat_stat *st)
{
   struct found f;
   uint32_t dir;
   int ret;
   fat_lock_acquire(v->lock);
   ret = lookup(v, path, &dir, &f);
   memset(st, 0, sizeof(*st));
   if (ret == 1)
   {
      st->is_dir  = 1;
      st->cluster = v->type == 32 ? v->root_clus : 0;
      ret = 0;
   }
   else if (!ret)
      fill_stat(v, f.e, st);
   fat_lock_release(v->lock);
   return ret;
}

/* ---- namespace changes ---- */

static fat_file *open_at(fat_vol *v, uint32_t dir, uint32_t entry)
{
   fat_file *h;
   for (h = v->open; h; h = h->next)
      if (h->dir == dir && h->entry == entry)
         return h;
   return NULL;
}

static int dir_is_empty(fat_vol *v, uint32_t dir)
{
   uint32_t n = 0;
   struct found f;
   int ret;
   while ((ret = dir_next(v, dir, &n, &f)) == 1)
      if (!(f.name_len == 1 && f.name[0] == '.')
            && !(f.name_len == 2 && f.name[0] == '.' && f.name[1] == '.'))
         return 0;
   return ret < 0 ? ret : 1;
}

int fat_unlink(fat_vol *v, const char *path)
{
   struct found f;
   uint32_t dir;
   int ret;
   fat_lock_acquire(v->lock);
   ret = lookup(v, path, &dir, &f);
   if (ret == 1 || (!ret && (f.e[11] & ATTR_DIR)))
      ret = -EISDIR;
   else if (!ret && (f.e[11] & ATTR_RO))
      ret = -EACCES;
   else if (!ret && open_at(v, dir, f.index))
      ret = -EBUSY;   /* its entry would be written back on close */
   if (!ret)
      ret = remove_entry(v, dir, &f);
   if (!ret)
      ret = free_chain(v, ent_cluster(v, f.e));
   fat_lock_release(v->lock);
   return ret;
}

int fat_rmdir(fat_vol *v, const char *path)
{
   struct found f;
   uint32_t dir, c;
   int ret;
   fat_lock_acquire(v->lock);
   ret = lookup(v, path, &dir, &f);
   if (ret == 1)
      ret = -EBUSY;
   else if (!ret && !(f.e[11] & ATTR_DIR))
      ret = -ENOTDIR;
   if (!ret)
   {
      c = ent_cluster(v, f.e);
      if ((ret = dir_is_empty(v, c)) == 1)
      {
         ret = remove_entry(v, dir, &f);
         if (!ret)
            ret = free_chain(v, c);
         v->walk_dir = 0;
      }
      else if (!ret)
         ret = -ENOTEMPTY;
   }
   fat_lock_release(v->lock);
   return ret;
}

int fat_mkdir(fat_vol *v, const char *path)
{
   uint16_t leaf[MAX_NAME];
   struct found f;
   uint8_t e[DIRENT], dot[DIRENT];
   uint32_t dir, c, index;
   int len, ret;
   fat_lock_acquire(v->lock);
   ret = lookup(v, path, &dir, &f);
   if (ret == 0 || ret == 1)
      ret = -EEXIST;
   else if (ret == -ENOENT && !(ret = walk(v, path, &dir, leaf, &len)))
   {
      if (!(c = alloc_cluster(v, 0)))
         ret = -ENOSPC;
      else if ((ret = zero_cluster(v, c)))
         free_chain(v, c);
      else
      {
         memset(e, 0, DIRENT);
         e[11] = ATTR_DIR;
         stamp(e, 1);
         ent_set_cluster(e, c);
         if ((ret = add_entry(v, dir, leaf, len, e, &index)))
            free_chain(v, c);
         else
         {
            memcpy(dot, e, DIRENT);
            memset(dot, ' ', 11);
            dot[0] = '.';
            dot[12] = 0;
            ret = dir_write(v, c, 0, dot);
            if (!ret)
            {
               dot[1] = '.';
               ent_set_cluster(dot, dir);
               ret = dir_write(v, c, 1, dot);
            }
         }
      }
   }
   fat_lock_release(v->lock);
   return ret;
}

/* Open files follow their entry to where it moved. */
static void retarget(fat_vol *v, uint32_t dir, uint32_t entry,
      uint32_t new_dir, uint32_t new_entry)
{
   fat_file *h;
   for (h = v->open; h; h = h->next)
      if (h->dir == dir && h->entry == entry)
      {
         h->dir   = new_dir;
         h->entry = new_entry;
      }
}

int fat_rename(fat_vol *v, const char *from, const char *to)
{
   struct found f, g;
   uint16_t leaf[MAX_NAME];
   uint8_t e[DIRENT];
   uint32_t dir_from, dir_to, index, c;
   int len, ret, is_dir;
   fat_lock_acquire(v->lock);
   if ((ret = lookup(v, from, &dir_from, &f)) == 1)
      ret = -EBUSY;
   if (ret)
      goto out;
   is_dir = (f.e[11] & ATTR_DIR) != 0;
   c      = ent_cluster(v, f.e);
   if ((ret = walk(v, to, &dir_to, leaf, &len)))
      goto out;
   if (!len)
   {
      ret = -EBUSY;
      goto out;
   }
   ret = dir_find(v, dir_to, leaf, len, &g);
   if (!ret)
   {
      /* An existing target is replaced, as POSIX has it. */
      uint32_t gc = ent_cluster(v, g.e);
      if (dir_to == dir_from && g.index == f.index)
      {
         /* The same entry: only the spelling may change. */
         if ((ret = remove_entry(v, dir_from, &f)))
            goto out;
         memcpy(e, f.e, DIRENT);
         if (!(ret = add_entry(v, dir_to, leaf, len, e, &index)))
            retarget(v, dir_from, f.index, dir_to, index);
         goto out;
      }
      if (g.e[11] & ATTR_DIR)
      {
         if (!is_dir)
            ret = -EISDIR;
         else if ((ret = dir_is_empty(v, gc)) == 1)
            ret = 0;
         else if (!ret)
            ret = -ENOTEMPTY;
      }
      else if (is_dir)
         ret = -ENOTDIR;
      else if (open_at(v, dir_to, g.index))
         ret = -EBUSY;
      if (!ret)
         ret = remove_entry(v, dir_to, &g);
      if (!ret)
         ret = free_chain(v, gc);
      v->walk_dir = 0;
      if (ret)
         goto out;
   }
   else if (ret != -ENOENT)
      goto out;
   if (is_dir)
   {
      /* Not into itself or below it. */
      uint32_t d = dir_to;
      ret = 0;
      while (d)
      {
         uint8_t up[DIRENT];
         if (d == c)
         {
            ret = -EINVAL;
            goto out;
         }
         if (dir_read(v, d, 1, up))
            break;
         d = ent_cluster(v, up);
      }
   }
   memcpy(e, f.e, DIRENT);
   /* Entries are only added in free slots, so the old ones stay put. */
   if ((ret = add_entry(v, dir_to, leaf, len, e, &index)))
      goto out;
   if ((ret = remove_entry(v, dir_from, &f)))
      goto out;
   retarget(v, dir_from, f.index, dir_to, index);
   if (is_dir && dir_to != dir_from)
   {
      uint8_t dd[DIRENT];
      if (!(ret = dir_read(v, c, 1, dd)) && dd[0] == '.' && dd[1] == '.')
      {
         ent_set_cluster(dd, dir_to);
         ret = dir_write(v, c, 1, dd);
      }
   }
out:
   fat_lock_release(v->lock);
   return ret;
}

int fat_statvfs(fat_vol *v, fat_space *st)
{
   fat_lock_acquire(v->lock);
   if (v->free_count == FREE_UNKNOWN)
   {
      uint32_t c, n = 0;
      if (v->type == 12)
      {
         for (c = 2; c < v->clusters + 2; c++)
         {
            uint32_t val = fat_get(v, c);
            if (val == 0xffffffffu)
               goto io_error;
            n += !val;
         }
      }
      else
      {
         /* Whole sectors at a time, straight from the device. */
         uint32_t per = v->bps / (v->type / 8), sec, i;
         uint8_t *buf = (uint8_t*)malloc(v->bps);
         if (!buf)
         {
            fat_lock_release(v->lock);
            return -ENOMEM;
         }
         if (cache_flush(v))
         {
            free(buf);
            goto io_error;
         }
         for (c = 0, sec = 0; c < v->clusters + 2; sec++)
         {
            if (dev_read(v, v->fat_lba + sec, 1, buf))
            {
               free(buf);
               goto io_error;
            }
            for (i = 0; i < per && c < v->clusters + 2; i++, c++)
               if (c >= 2 && !(v->type == 32
                     ? rd32(buf + i * 4) & 0x0fffffff : rd16(buf + i * 2)))
                  n++;
         }
         free(buf);
      }
      v->free_count   = n;
      v->fsinfo_dirty = 1;
   }
   st->clusters      = v->clusters;
   st->free_clusters = v->free_count;
   st->cluster_size  = v->clus_size;
   fat_lock_release(v->lock);
   return 0;

io_error:
   fat_lock_release(v->lock);
   return -EIO;
}

/* ---- listing ---- */

int fat_opendir(fat_vol *v, const char *path, fat_dir **out)
{
   struct found f;
   uint32_t dir;
   fat_dir *d;
   int ret;
   fat_lock_acquire(v->lock);
   ret = lookup(v, path, &dir, &f);
   if (ret == 1)
   {
      dir = 0;
      ret = 0;
   }
   else if (!ret)
   {
      if (!(f.e[11] & ATTR_DIR))
         ret = -ENOTDIR;
      else
      {
         dir = ent_cluster(v, f.e);
         if (dir == v->root_clus && v->type == 32)
            dir = 0;
      }
   }
   if (!ret && !(d = (fat_dir*)calloc(1, sizeof(*d))))
      ret = -ENOMEM;
   if (!ret)
   {
      d->v    = v;
      d->clus = dir;
      *out    = d;
      v->handles++;
   }
   fat_lock_release(v->lock);
   return ret;
}

int fat_readdir(fat_dir *d, char *name, size_t size, fat_stat *st)
{
   struct found f;
   int ret;
   fat_lock_acquire(d->v->lock);
   for (;;)
   {
      if (!d->v->dev)
      {
         ret = -EIO;
         break;
      }
      ret = dir_next(d->v, d->clus, &d->n, &f);
      if (ret != 1)
         break;
      if (!(f.name_len == 1 && f.name[0] == '.')
            && !(f.name_len == 2 && f.name[0] == '.' && f.name[1] == '.'))
         break;
   }
   if (ret == 1)
   {
      utf16_to_8(f.name, f.name_len, name, size);
      if (st)
         fill_stat(d->v, f.e, st);
   }
   fat_lock_release(d->v->lock);
   return ret;
}

void fat_rewinddir(fat_dir *d)
{
   d->n = 0;
}

void fat_closedir(fat_dir *d)
{
   put_handle(d->v);
   free(d);
}
