/* fs/fat.c on the host, over an image file.
 *
 *   fat_test IMAGE check    files mtools put there read back right
 *   fat_test IMAGE write    make a tree for mtools and fsck to check
 *   fat_test IMAGE stress   random file operations against a model
 *   fat_test IMAGE fill     fill the volume, then empty it
 *   fat_test IMAGE unplug   unmount under open files and a directory
 *
 * run.sh drives these over FAT12, FAT16 and FAT32 images. */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../fs/fat.h"

static int failures;

#define CHECK(cond) do { if (!(cond)) { \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
   failures++; } } while (0)

/* ---- platform hooks ---- */

uint32_t fat_now(void)               { return 1700000000u; }
void    *fat_lock_create(void)       { static int lock; return &lock; }
void     fat_lock_destroy(void *lock) { (void)lock; }
void     fat_lock_acquire(void *lock) { (void)lock; }
void     fat_lock_release(void *lock) { (void)lock; }

/* ---- the image ---- */

static unsigned long dev_reads, dev_writes;

static int img_read(gk_blockdev_t *dev, uint64_t lba, uint32_t n, void *buf)
{
   size_t bytes = (size_t)n * dev->sector_size;
   if (lba + n > dev->sectors)
      return -1;
   dev_reads++;
   return pread(*(int*)dev->priv, buf, bytes,
         (off_t)(lba * dev->sector_size)) == (ssize_t)bytes ? 0 : -1;
}

static int img_write(gk_blockdev_t *dev, uint64_t lba, uint32_t n,
      const void *buf)
{
   size_t bytes = (size_t)n * dev->sector_size;
   if (lba + n > dev->sectors)
      return -1;
   dev_writes++;
   return pwrite(*(int*)dev->priv, buf, bytes,
         (off_t)(lba * dev->sector_size)) == (ssize_t)bytes ? 0 : -1;
}

static int           img_fd;
static gk_blockdev_t img;

static fat_vol *mount_image(void)
{
   fat_vol *v = NULL;
   int ret = fat_mount(&v, &img);
   if (ret)
   {
      printf("FAIL mount: %d\n", ret);
      exit(1);
   }
   return v;
}

/* ---- helpers ---- */

static uint8_t pattern(uint32_t seed, uint32_t i)
{
   uint32_t x = (seed + i) * 2654435761u;
   return (uint8_t)(x >> 24);
}

static int write_file(fat_vol *v, const char *path, uint32_t seed,
      uint32_t size)
{
   fat_file *f;
   uint8_t buf[3001];
   uint32_t done = 0, i;
   int ret = fat_open(v, path, O_WRONLY | O_CREAT | O_TRUNC, &f);
   if (ret)
      return ret;
   while (done < size)
   {
      /* Odd chunk sizes so the transfers straddle sectors. */
      uint32_t n = size - done < sizeof(buf) ? size - done : sizeof(buf);
      for (i = 0; i < n; i++)
         buf[i] = pattern(seed, done + i);
      if (fat_write(f, buf, n) != (long)n)
      {
         fat_close(f);
         return -EIO;
      }
      done += n;
   }
   return fat_close(f);
}

/* 0 when the file holds the pattern. */
static int verify_file(fat_vol *v, const char *path, uint32_t seed,
      uint32_t size)
{
   fat_file *f;
   fat_stat st;
   uint8_t *buf;
   uint32_t i;
   long n;
   int ret = fat_open(v, path, O_RDONLY, &f);
   if (ret)
      return ret;
   if (fat_fstat(f, &st) || st.size != size)
   {
      fat_close(f);
      return -EINVAL;
   }
   buf = (uint8_t*)malloc(size + 16);
   n   = fat_read(f, buf, size + 16);
   fat_close(f);
   ret = n == (long)size ? 0 : -EIO;
   for (i = 0; !ret && i < size; i++)
      if (buf[i] != pattern(seed, i))
         ret = -EILSEQ;
   free(buf);
   return ret;
}

static int read_all(fat_vol *v, const char *path, char *buf, size_t size)
{
   fat_file *f;
   long n;
   int ret = fat_open(v, path, O_RDONLY, &f);
   if (ret)
      return ret;
   n = fat_read(f, buf, size - 1);
   fat_close(f);
   if (n < 0)
      return (int)n;
   buf[n] = '\0';
   return 0;
}

static int count_dir(fat_vol *v, const char *path)
{
   fat_dir *d;
   char name[800];
   int n = 0;
   if (fat_opendir(v, path, &d))
      return -1;
   while (fat_readdir(d, name, sizeof(name), NULL) == 1)
      n++;
   fat_closedir(d);
   return n;
}

static int in_dir(fat_vol *v, const char *path, const char *want)
{
   fat_dir *d;
   char name[800];
   int found = 0;
   if (fat_opendir(v, path, &d))
      return 0;
   while (fat_readdir(d, name, sizeof(name), NULL) == 1)
      if (!strcmp(name, want))
         found = 1;
   fat_closedir(d);
   return found;
}

/* ---- check: what mtools wrote ---- */

static void check(fat_vol *v)
{
   char buf[256];
   fat_stat st;

   CHECK(!read_all(v, "/hello.txt", buf, sizeof(buf))
         && !strcmp(buf, "hello\n"));
   CHECK(!read_all(v, "sd:/HELLO.TXT", buf, sizeof(buf))
         && !strcmp(buf, "hello\n"));
   CHECK(!read_all(v, "/A Long File Name.data", buf, sizeof(buf))
         && !strcmp(buf, "long\n"));
   CHECK(!read_all(v, "/a long file name.DATA", buf, sizeof(buf))
         && !strcmp(buf, "long\n"));
   CHECK(!read_all(v, "/dir/sub/deep name.txt", buf, sizeof(buf))
         && !strcmp(buf, "deep\n"));
   CHECK(!read_all(v, "/dir/./sub/../sub/deep name.txt", buf, sizeof(buf))
         && !strcmp(buf, "deep\n"));
   CHECK(!read_all(v, "/caf\xc3\xa9 \xe2\x82\xac.txt", buf, sizeof(buf))
         && !strcmp(buf, "utf\n"));
   CHECK(!verify_file(v, "/big.bin", 7, 1000003));
   CHECK(fat_stat_path(v, "/dir", &st) == 0 && st.is_dir);
   CHECK(fat_stat_path(v, "/", &st) == 0 && st.is_dir);
   CHECK(fat_stat_path(v, "/missing", &st) == -ENOENT);
   CHECK(fat_stat_path(v, "/hello.txt/x", &st) == -ENOTDIR);
   CHECK(fat_stat_path(v, "/hello.txt", &st) == 0 && st.size == 6
         && !st.is_dir && st.mtime > 1000000000u);
   CHECK(in_dir(v, "/", "A Long File Name.data"));
   CHECK(in_dir(v, "/", "hello.txt") || in_dir(v, "/", "HELLO.TXT"));
   CHECK(count_dir(v, "/dir/many") == 300);
   CHECK(in_dir(v, "/dir/many", "file with long name 299.txt"));
}

/* ---- write: a tree for mtools ---- */

static void write_tree(fat_vol *v)
{
   char path[300];
   fat_stat st;
   fat_file *f;
   unsigned i;

   CHECK(fat_mkdir(v, "/new") == 0);
   CHECK(fat_mkdir(v, "/new") == -EEXIST);
   CHECK(fat_mkdir(v, "/new/Mixed Case Directory") == 0);
   CHECK(fat_mkdir(v, "/nope/x") == -ENOENT);
   CHECK(write_file(v, "/new/short.txt", 1, 10) == 0);
   CHECK(write_file(v, "/new/lower.txt", 2, 512) == 0);
   CHECK(write_file(v, "/new/Mixed Case Directory/A file with a long name.bin",
         3, 70000) == 0);
   CHECK(write_file(v, "/new/.dotfile", 4, 3) == 0);
   CHECK(write_file(v, "/new/caf\xc3\xa9.txt", 5, 4) == 0);
   CHECK(write_file(v, "/new/empty", 0, 0) == 0);
   CHECK(write_file(v, "/new/bad?name", 0, 0) == -EINVAL);
   /* Aliases that collide: ~1, ~2, ... */
   for (i = 0; i < 12; i++)
   {
      sprintf(path, "/new/collision number %u.txt", i);
      CHECK(write_file(v, path, 100 + i, 100 + i) == 0);
   }
   /* Enough entries to grow the directory several clusters. */
   CHECK(fat_mkdir(v, "/new/grow") == 0);
   for (i = 0; i < 200; i++)
   {
      sprintf(path, "/new/grow/entry %u with a name long enough", i);
      CHECK(write_file(v, path, i, i % 7) == 0);
   }
   /* Remove some and reuse their slots. */
   for (i = 0; i < 200; i += 3)
   {
      sprintf(path, "/new/grow/entry %u with a name long enough", i);
      CHECK(fat_unlink(v, path) == 0);
   }
   CHECK(write_file(v, "/new/grow/reused", 9, 9) == 0);
   /* Append, rename, truncate. */
   CHECK(fat_open(v, "/new/short.txt", O_WRONLY | O_APPEND, &f) == 0);
   CHECK(fat_write(f, "appended", 8) == 8);
   CHECK(fat_close(f) == 0);
   CHECK(fat_rename(v, "/new/lower.txt", "/new/Mixed Case Directory/moved.txt")
         == 0);
   CHECK(fat_rename(v, "/new/grow", "/new/Mixed Case Directory/grown") == 0);
   CHECK(fat_rename(v, "/new", "/new/Mixed Case Directory/x") == -EINVAL);
   CHECK(write_file(v, "/new/replaced", 11, 5000) == 0);
   CHECK(write_file(v, "/new/replacement", 12, 300) == 0);
   CHECK(fat_rename(v, "/new/replacement", "/new/replaced") == 0);
   CHECK(fat_rename(v, "/new/empty", "/new/EMPTY") == 0);
   CHECK(write_file(v, "/new/trunc", 13, 50000) == 0);
   CHECK(fat_open(v, "/new/trunc", O_RDWR, &f) == 0);
   CHECK(fat_truncate(f, 1000) == 0);
   CHECK(fat_close(f) == 0);
   CHECK(fat_mkdir(v, "/gone") == 0);
   CHECK(write_file(v, "/gone/file", 1, 1) == 0);
   CHECK(fat_rmdir(v, "/gone") == -ENOTEMPTY);
   CHECK(fat_unlink(v, "/gone/file") == 0);
   CHECK(fat_rmdir(v, "/gone") == 0);
   CHECK(fat_stat_path(v, "/gone", &st) == -ENOENT);
   CHECK(fat_unlink(v, "/hello.txt") == 0);
   CHECK(fat_unlink(v, "/dir") == -EISDIR);

   /* An open file follows a rename and cannot be unlinked. */
   CHECK(fat_open(v, "/new/open", O_WRONLY | O_CREAT, &f) == 0);
   CHECK(fat_write(f, "first", 5) == 5);
   CHECK(fat_unlink(v, "/new/open") == -EBUSY);
   CHECK(fat_rename(v, "/new/open", "/new/Mixed Case Directory/was open") == 0);
   CHECK(fat_write(f, " second", 7) == 7);
   CHECK(fat_close(f) == 0);
   CHECK(!read_all(v, "/new/Mixed Case Directory/was open", path,
         sizeof(path)) && !strcmp(path, "first second"));

   /* Read it back here before mtools has a look. */
   CHECK(!verify_file(v, "/new/Mixed Case Directory/moved.txt", 2, 512));
   CHECK(!verify_file(v, "/new/replaced", 12, 300));
   CHECK(count_dir(v, "/new/Mixed Case Directory/grown") == 200 - 67 + 1);
   CHECK(in_dir(v, "/new", "EMPTY") && !in_dir(v, "/new", "empty"));
}

/* ---- stress: random operations against a model ---- */

#define FILES 6
#define MAXSZ (300 * 1024)

static uint32_t rng = 12345;
static uint32_t rnd(uint32_t n)
{
   rng = rng * 1103515245u + 12345u;
   return (rng >> 8) % n;
}

static void stress(fat_vol *v, unsigned rounds)
{
   static uint8_t model[FILES][MAXSZ], buf[MAXSZ];
   uint32_t size[FILES], pos[FILES];
   fat_file *f[FILES];
   unsigned i, r;
   char path[32];

   CHECK(fat_mkdir(v, "/stress") == 0);
   for (i = 0; i < FILES; i++)
   {
      sprintf(path, "/stress/f%u", i);
      CHECK(fat_open(v, path, O_RDWR | O_CREAT, &f[i]) == 0);
      size[i] = pos[i] = 0;
   }
   for (r = 0; r < rounds && !failures; r++)
   {
      unsigned k = rnd(FILES), op = rnd(10);
      uint32_t n;
      if (op < 4)
      {
         /* Write at the position, sometimes past the end. */
         n = rnd(4) ? rnd(5000) : rnd(70000);
         if (pos[k] + n > MAXSZ)
            continue;
         for (i = 0; i < n; i++)
            buf[i] = model[k][pos[k] + i] = (uint8_t)rnd(256);
         if (pos[k] > size[k])
            memset(model[k] + size[k], 0, pos[k] - size[k]);
         CHECK(fat_write(f[k], buf, n) == (long)n);
         pos[k] += n;
         if (pos[k] > size[k])
            size[k] = pos[k];
      }
      else if (op < 7)
      {
         n = rnd(4) ? rnd(5000) : rnd(70000);
         {
            long want = pos[k] >= size[k] ? 0
               : (long)(size[k] - pos[k] < n ? size[k] - pos[k] : n);
            long got  = fat_read(f[k], buf, n);
            CHECK(got == want);
            if (got == want && want > 0)
               CHECK(!memcmp(buf, model[k] + pos[k], (size_t)want));
            if (got > 0)
               pos[k] += (uint32_t)got;
         }
      }
      else if (op < 9)
      {
         pos[k] = rnd(size[k] + 3000);
         CHECK(fat_seek(f[k], pos[k], SEEK_SET) == (int64_t)pos[k]);
      }
      else
      {
         n = rnd(size[k] + 3000);
         if (n > MAXSZ)
            continue;
         if (n > size[k])
            memset(model[k] + size[k], 0, n - size[k]);
         CHECK(fat_truncate(f[k], n) == 0);
         size[k] = n;
      }
   }
   for (i = 0; i < FILES; i++)
   {
      fat_stat st;
      CHECK(fat_fstat(f[i], &st) == 0 && st.size == size[i]);
      CHECK(fat_close(f[i]) == 0);
   }
   /* Through a fresh mount: nothing lived only in the cache. */
   CHECK(fat_unmount(v) == 0);
   v = mount_image();
   for (i = 0; i < FILES; i++)
   {
      fat_file *h;
      sprintf(path, "/stress/f%u", i);
      CHECK(fat_open(v, path, O_RDONLY, &h) == 0);
      CHECK(fat_read(h, buf, MAXSZ) == (long)size[i]);
      CHECK(!memcmp(buf, model[i], size[i]));
      fat_close(h);
   }
   CHECK(fat_unmount(v) == 0);
}

/* ---- fill: run out of space, then give it all back ---- */

static void fill(fat_vol *v)
{
   fat_space before, full, after;
   fat_file *f;
   static uint8_t chunk[65536];
   long n;
   uint64_t total = 0;

   CHECK(fat_statvfs(v, &before) == 0);
   CHECK(fat_open(v, "/filler", O_WRONLY | O_CREAT, &f) == 0);
   memset(chunk, 0xa5, sizeof(chunk));
   while ((n = fat_write(f, chunk, sizeof(chunk))) == (long)sizeof(chunk))
      total += (uint64_t)n;
   CHECK(n == -ENOSPC || (n >= 0 && n < (long)sizeof(chunk)));
   CHECK(fat_close(f) == 0);
   CHECK(fat_statvfs(v, &full) == 0);
   CHECK(full.free_clusters == 0);
   CHECK(write_file(v, "/one more", 1, 1) != 0);
   CHECK(fat_unlink(v, "/one more") == 0);
   CHECK(fat_unlink(v, "/filler") == 0);
   CHECK(fat_statvfs(v, &after) == 0);
   CHECK(after.free_clusters == before.free_clusters);
   printf("filled %lu KiB of %lu clusters\n", (unsigned long)(total >> 10),
         (unsigned long)before.clusters);
}

/* What is still open fails without touching the device, and the last
 * close frees the volume. */
static void unplug(fat_vol *v)
{
   fat_file *r = NULL, *w = NULL;
   fat_dir *d  = NULL;
   unsigned long reads, writes;
   char buf[64];
   fat_stat st;
   CHECK(fat_open(v, "/big.bin", O_RDONLY, &r) == 0);
   CHECK(fat_open(v, "/unplugged", O_WRONLY | O_CREAT, &w) == 0);
   CHECK(fat_write(w, "x", 1) == 1);
   CHECK(fat_opendir(v, "/dir", &d) == 0);
   if (!r || !w || !d)
      return;
   CHECK(fat_unmount(v) == 0);
   reads      = dev_reads;
   writes     = dev_writes;
   img.read   = NULL;
   img.write  = NULL;
   CHECK(fat_read(r, buf, sizeof(buf)) == -EIO);
   CHECK(fat_write(w, "y", 1) == -EIO);
   CHECK(fat_truncate(w, 0) == -EIO);
   CHECK(fat_fsync(w) == -EIO);
   CHECK(fat_readdir(d, buf, sizeof(buf), &st) == -EIO);
   CHECK(fat_close(r) == 0);
   CHECK(fat_close(w) == -EIO);
   fat_closedir(d);
   CHECK(dev_reads == reads && dev_writes == writes);
}

int main(int argc, char **argv)
{
   fat_vol *v;
   if (argc != 3)
   {
      fprintf(stderr,
            "usage: fat_test IMAGE check|write|stress|fill|unplug\n");
      return 2;
   }
   if ((img_fd = open(argv[1], O_RDWR)) < 0)
   {
      perror(argv[1]);
      return 2;
   }
   img.sector_size = 512;
   img.sectors     = (uint64_t)lseek(img_fd, 0, SEEK_END) / 512;
   img.priv        = &img_fd;
   img.read        = img_read;
   img.write       = img_write;

   v = mount_image();
   if (!strcmp(argv[2], "check"))
      check(v);
   else if (!strcmp(argv[2], "write"))
      write_tree(v);
   else if (!strcmp(argv[2], "fill"))
      fill(v);
   else if (!strcmp(argv[2], "stress"))
   {
      stress(v, 4000);
      v = NULL;
   }
   else if (!strcmp(argv[2], "unplug"))
   {
      unplug(v);
      v = NULL;
   }
   if (v)
      CHECK(fat_unmount(v) == 0);
   close(img_fd);
   printf("%s %s: %s (%lu reads, %lu writes)\n", argv[1], argv[2],
         failures ? "FAILED" : "ok", dev_reads, dev_writes);
   return failures ? 1 : 0;
}
