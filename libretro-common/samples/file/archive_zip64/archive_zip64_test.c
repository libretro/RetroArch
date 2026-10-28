/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (archive_zip64_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* ZIP64 in the zip backend.
 *
 * A small archive written with every ZIP64 field it can carry: the end
 * of the directory found through the ZIP64 record, members whose
 * sizes and offset are in the ZIP64 extra field, beside one without.
 * Each member is listed, read whole, extracted through a transfer (a
 * stored member bigger than the extraction window going through it a
 * window at a time) and read through an entry source. An extra field
 * short of what it must hold, a locator that is not one and a record
 * past the end are refused.
 *
 * Then a real one, sparse: a stored member of 4 GiB and 4 KiB, and a
 * member after it whose local header is past 4 GiB, with the
 * directory after that. The big member's 64-bit size is listed, and
 * the member after it is read every way. A filesystem that will not
 * hold the file skips that part.
 *
 * Build:  make            (SANITIZER=address,undefined for a checked run)
 * Run:    ./archive_zip64_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <file/archive_file.h>
#include <file/file_path.h>
#include <lists/string_list.h>
#include <encodings/crc32.h>
#include <encodings/deflate.h>
#include <streams/file_stream.h>

#if defined(_WIN32)
#include <windows.h>
#include <winioctl.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#define ZIP_PATH "archive_zip64_test.zip"
#define BIG_PATH "archive_zip64_test_big.zip"
#define OUT_PATH "archive_zip64_test.out"

#define STORED_LEN  300000
#define DEFLATE_LEN 70000
#define BIG_LEN     (((uint64_t)1 << 32) + 4096)

static unsigned failures;

/* Linked with --wrap=filestream_read: reads through the stream. */
static unsigned stream_reads;

int64_t __real_filestream_read(RFILE *stream, void *data, int64_t len);
int64_t __wrap_filestream_read(RFILE *stream, void *data, int64_t len)
{
   stream_reads++;
   return __real_filestream_read(stream, data, len);
}

static void fail(const char *what, const char *detail)
{
   printf("[FAIL] %s: %s\n", what, detail);
   failures++;
}

static void put16(uint8_t *p, unsigned v)
{
   p[0] = (uint8_t)v;
   p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
   put16(p, v & 0xffff);
   put16(p + 2, v >> 16);
}

static void put64(uint8_t *p, uint64_t v)
{
   put32(p, (uint32_t)v);
   put32(p + 4, (uint32_t)(v >> 32));
}

static void fill(uint8_t *d, size_t n, uint32_t x, unsigned alpha)
{
   size_t i;
   for (i = 0; i < n; i++)
   {
      x    = x * 1103515245u + 12345u;
      d[i] = (uint8_t)('a' + (x >> 16) % alpha);
   }
}

/* raw DEFLATE of @in into @out; its length */
static size_t deflate_raw(uint8_t *out, size_t cap, const uint8_t *in,
      size_t n)
{
   void  *z = rdeflate_new(6, -15);
   size_t rd = 0, wr = 0, total = 0;
   int    st;
   rdeflate_set_in(z, in, n);
   rdeflate_finish(z);
   do
   {
      rdeflate_set_out(z, out + total, cap - total);
      st     = rdeflate_process(z, &rd, &wr);
      total += wr;
   } while (st == RDEFLATE_PROCESS_NEXT && wr);
   rdeflate_free(z);
   return st == RDEFLATE_PROCESS_END ? total : 0;
}

/* One member of an archive being written: what the headers say */
struct member
{
   const char    *name;
   const uint8_t *data;
   uint64_t       usize;
   uint64_t       csize;
   uint64_t       offset;
   uint32_t       crc;
   unsigned       method;
   bool           big_sizes;   /* sizes in the ZIP64 field */
   bool           big_offset;  /* the offset in it too */
};

/* The ZIP64 extra field for @m at @p; its length */
static size_t zip64_field(uint8_t *p, const struct member *m, bool local)
{
   size_t n = 0;
   if (m->big_sizes || local)
   {
      put64(p + 4 + n, m->usize);
      put64(p + 4 + n + 8, m->csize);
      n += 16;
   }
   if (m->big_offset && !local)
   {
      put64(p + 4 + n, m->offset);
      n += 8;
   }
   if (!n)
      return 0;
   put16(p, 0x0001);
   put16(p + 2, (unsigned)n);
   return 4 + n;
}

static size_t local_header(uint8_t *p, const struct member *m)
{
   size_t nl = strlen(m->name), el;
   memset(p, 0, 30);
   put32(p, 0x04034b50u);
   put16(p + 4, 45);
   put16(p + 8, m->method);
   put32(p + 14, m->crc);
   el = m->big_sizes ? zip64_field(p + 30 + nl, m, true) : 0;
   put32(p + 18, m->big_sizes ? 0xFFFFFFFFu : (uint32_t)m->csize);
   put32(p + 22, m->big_sizes ? 0xFFFFFFFFu : (uint32_t)m->usize);
   put16(p + 26, (unsigned)nl);
   put16(p + 28, (unsigned)el);
   memcpy(p + 30, m->name, nl);
   return 30 + nl + el;
}

static size_t central_header(uint8_t *p, const struct member *m)
{
   size_t nl = strlen(m->name);
   size_t el = zip64_field(p + 46 + nl, m, false);
   memset(p, 0, 46);
   put32(p, 0x02014b50u);
   put16(p + 4, 45);
   put16(p + 6, 45);
   put16(p + 10, m->method);
   put32(p + 16, m->crc);
   put32(p + 20, m->big_sizes ? 0xFFFFFFFFu : (uint32_t)m->csize);
   put32(p + 24, m->big_sizes ? 0xFFFFFFFFu : (uint32_t)m->usize);
   put16(p + 28, (unsigned)nl);
   put16(p + 30, (unsigned)el);
   put32(p + 42, m->big_offset ? 0xFFFFFFFFu : (uint32_t)m->offset);
   memcpy(p + 46, m->name, nl);
   return 46 + nl + el;
}

/* The ZIP64 end record, its locator and the end record, the directory
 * of @entries at @cd, @cd_len long, the record at @at */
static size_t directory_end(uint8_t *p, uint64_t at, uint64_t cd,
      uint64_t cd_len, unsigned entries)
{
   memset(p, 0, 56 + 20 + 22);
   put32(p, 0x06064b50u);
   put64(p + 4, 44);
   put16(p + 12, 45);
   put16(p + 14, 45);
   put64(p + 24, entries);
   put64(p + 32, entries);
   put64(p + 40, cd_len);
   put64(p + 48, cd);
   put32(p + 56, 0x07064b50u);
   put64(p + 56 + 8, at);
   put32(p + 56 + 16, 1);
   put32(p + 76, 0x06054b50u);
   put16(p + 76 + 8, 0xFFFF);
   put16(p + 76 + 10, 0xFFFF);
   put32(p + 76 + 12, 0xFFFFFFFFu);
   put32(p + 76 + 16, 0xFFFFFFFFu);
   return 56 + 20 + 22;
}

static bool write_file(const char *path, const uint8_t *d, size_t n)
{
   FILE *f = fopen(path, "wb");
   bool  ok;
   if (!f)
      return false;
   ok = fwrite(d, 1, n, f) == n;
   fclose(f);
   return ok;
}

/* every member named @only, or all of them when NULL */
struct extract_want
{
   const char *only;
   unsigned    count;
};

static int extract_one_cb(const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint64_t csize, uint64_t size,
      uint32_t checksum, struct archive_extract_userdata *userdata)
{
   struct extract_want *w = (struct extract_want*)userdata->cb_data;
   char out[96];
   if (w->only && strcmp(name, w->only))
      return 1;
   snprintf(out, sizeof(out), "%s.%s", OUT_PATH, path_basename(name));
   if (file_archive_perform_mode(out, valid_exts, cdata, cmode,
            csize, size, checksum, userdata))
      w->count++;
   return 1;
}

static unsigned extract(const char *archive, const char *only)
{
   struct archive_extract_userdata userdata;
   struct extract_want want;
   file_archive_transfer_t state;
   bool returnerr = true;
   memset(&state, 0, sizeof(state));
   memset(&userdata, 0, sizeof(userdata));
   want.only        = only;
   want.count       = 0;
   state.type       = ARCHIVE_TRANSFER_INIT;
   userdata.cb_data = &want;
   while (file_archive_parse_file_iterate(&state, &returnerr, archive,
            NULL, extract_one_cb, &userdata) == 0) { }
   return want.count;
}

/* the member's octets, read every way there is */
static void check_member(const char *what, const char *archive,
      const char *name, const uint8_t *want, size_t n)
{
   char     path[160], out[96];
   void    *buf = NULL;
   int64_t  len = 0, usize = 0, done = 0, got_n;
   uint64_t listed = 0;
   uint8_t *got = (uint8_t*)malloc(n + 1);
   file_archive_entry_source_t *src;

   snprintf(path, sizeof(path), "%s#%s", archive, name);
   if (     !file_archive_compressed_read(path, &buf, NULL, &len)
         || len != (int64_t)n || memcmp(buf, want, n))
      fail(what, "read whole");
   free(buf);
   if (     file_archive_get_file_crc32_and_size(path, &listed)
            != encoding_crc32(0, want, n)
         || listed != n)
      fail(what, "listed CRC and size");

   if (extract(archive, name) != 1)
      fail(what, "not extracted");
   snprintf(out, sizeof(out), "%s.%s", OUT_PATH, name);
   buf = NULL;
   if (     !filestream_read_file(out, &buf, &len)
         || len != (int64_t)n || memcmp(buf, want, n))
      fail(what, "extracted");
   free(buf);
   remove(out);

   if (     !(src = file_archive_entry_source_open(path, &usize))
         || usize != (int64_t)n)
      fail(what, "no entry source");
   else
   {
      while ((got_n = file_archive_entry_source_read(src, got + done,
                  4096)) > 0)
         done += got_n;
      if (done != (int64_t)n || memcmp(got, want, n))
         fail(what, "differs through an entry source");
   }
   if (src)
      file_archive_entry_source_close(src);
   free(got);
}

static bool listed(const char *archive, unsigned n)
{
   struct string_list *list = file_archive_get_file_list(archive, NULL);
   bool ok = list && list->size == n;
   string_list_free(list);
   return ok;
}

/* @archive's first member's CRC and size: @crc and @n, and, mapped,
 * taken from the archive where it lies, with no read through the
 * stream for the end record, the ZIP64 records or the directory. */
static void check_in_place(const char *what, const char *archive,
      uint32_t crc, uint64_t n)
{
   uint64_t size = 0;
   uint32_t got;

   stream_reads = 0;
   got          = file_archive_get_file_crc32_and_size(archive, &size);
   if (got != crc || size != n)
      fail(what, "first member's CRC and size");
#ifdef HAVE_MMAP
   else if (stream_reads)
      fail(what, "read through the stream while mapped");
#endif
}

/* One stored member and an archive comment: the end record is found
 * past the comment. */
static void test_plain_in_place(void)
{
   static const uint8_t note[] = "no ZIP64 here\n";
   static uint8_t zip[512];
   struct member m;
   size_t at, cd;

   m.name   = "plain.txt";
   m.data   = note;
   m.method = 0;
   m.usize  = m.csize = sizeof(note) - 1;
   m.big_sizes  = false;
   m.big_offset = false;
   m.offset = 0;
   m.crc    = encoding_crc32(0, note, sizeof(note) - 1);

   at  = local_header(zip, &m);
   memcpy(zip + at, note, sizeof(note) - 1);
   at += sizeof(note) - 1;
   cd  = at;
   at += central_header(zip + at, &m);
   memset(zip + at, 0, 22);
   put32(zip + at, 0x06054b50u);
   put16(zip + at + 8, 1);
   put16(zip + at + 10, 1);
   put32(zip + at + 12, (uint32_t)(at - cd));
   put32(zip + at + 16, (uint32_t)cd);
   put16(zip + at + 20, 3);
   memcpy(zip + at + 22, "PK!", 3);
   at += 22 + 3;

   if (!write_file(ZIP_PATH, zip, at))
      fail("plain", "cannot write");
   else
      check_in_place("plain, first member in place", ZIP_PATH, m.crc,
            m.usize);
   remove(ZIP_PATH);
}

static void test_small(void)
{
   uint8_t *stored  = (uint8_t*)malloc(STORED_LEN);
   uint8_t *plain   = (uint8_t*)malloc(DEFLATE_LEN);
   uint8_t *packed  = (uint8_t*)malloc(DEFLATE_LEN + 1024);
   uint8_t *zip     = (uint8_t*)malloc(STORED_LEN + DEFLATE_LEN + 4096);
   uint8_t *bad     = (uint8_t*)malloc(STORED_LEN + DEFLATE_LEN + 4096);
   static const uint8_t note[] = "a member with nothing of ZIP64 about it\n";
   struct member m[3];
   size_t   at = 0, cd, cd_len, plen, n, ext1 = 0, rec;
   unsigned i;

   fill(stored, STORED_LEN, 1, 7);
   fill(plain, DEFLATE_LEN, 2, 5);
   plen = deflate_raw(packed, DEFLATE_LEN + 1024, plain, DEFLATE_LEN);
   if (!plen)
      fail("ZIP64", "cannot deflate");

   m[0].name = "stored.bin";  m[0].data = stored; m[0].method = 0;
   m[0].usize = m[0].csize = STORED_LEN;
   m[0].big_sizes = true;  m[0].big_offset = false;
   m[1].name = "deflate.bin"; m[1].data = packed; m[1].method = 8;
   m[1].usize = DEFLATE_LEN; m[1].csize = plen;
   m[1].big_sizes = true;  m[1].big_offset = true;
   m[2].name = "plain.txt";   m[2].data = note;   m[2].method = 0;
   m[2].usize = m[2].csize = sizeof(note) - 1;
   m[2].big_sizes = false; m[2].big_offset = false;
   m[0].crc = encoding_crc32(0, stored, STORED_LEN);
   m[1].crc = encoding_crc32(0, plain, DEFLATE_LEN);
   m[2].crc = encoding_crc32(0, note, sizeof(note) - 1);

   for (i = 0; i < 3; i++)
   {
      m[i].offset = at;
      at += local_header(zip + at, &m[i]);
      memcpy(zip + at, m[i].data, (size_t)m[i].csize);
      at += (size_t)m[i].csize;
   }
   cd = at;
   for (i = 0; i < 3; i++)
   {
      if (i == 1)
         ext1 = at + 46 + strlen(m[1].name);
      at += central_header(zip + at, &m[i]);
   }
   cd_len = at - cd;
   rec    = at;
   at    += directory_end(zip + at, rec, cd, cd_len, 3);
   n      = at;

   if (!write_file(ZIP_PATH, zip, n))
      fail("ZIP64", "cannot write");
   if (!listed(ZIP_PATH, 3))
      fail("ZIP64", "listed");
   check_in_place("ZIP64, first member in place", ZIP_PATH, m[0].crc,
         STORED_LEN);
   check_member("ZIP64, stored", ZIP_PATH, "stored.bin", stored, STORED_LEN);
   check_member("ZIP64, deflated", ZIP_PATH, "deflate.bin", plain,
         DEFLATE_LEN);
   check_member("ZIP64, neither", ZIP_PATH, "plain.txt", note,
         sizeof(note) - 1);

   /* deflate.bin's field holds two of the three values it must */
   memcpy(bad, zip, n);
   put16(bad + ext1 + 2, 16);
   if (write_file(ZIP_PATH, bad, n))
   {
      void   *buf = NULL;
      int64_t len = 0;
      if (file_archive_compressed_read(ZIP_PATH "#deflate.bin", &buf, NULL,
               &len))
         fail("ZIP64, extra field short", "read");
      free(buf);
   }

   /* the locator, not one */
   memcpy(bad, zip, n);
   bad[rec + 56] ^= 0xFF;
   if (write_file(ZIP_PATH, bad, n) && listed(ZIP_PATH, 3))
      fail("ZIP64, no locator", "listed");

   /* the record it points at, past the end */
   memcpy(bad, zip, n);
   put64(bad + rec + 56 + 8, (uint64_t)n + 100);
   if (write_file(ZIP_PATH, bad, n) && listed(ZIP_PATH, 3))
      fail("ZIP64, record past the end", "listed");

   remove(ZIP_PATH);
   free(stored);
   free(plain);
   free(packed);
   free(zip);
   free(bad);
}

/* A sparse file of @size with @n spans of bytes written into it, made
 * without the code under test. false where it cannot be had. */
struct span
{
   uint64_t       at;
   const uint8_t *data;
   size_t         len;
};

static bool make_sparse(const char *path, uint64_t size,
      const struct span *spans, unsigned n)
{
   unsigned i;
#if defined(_WIN32)
   LARGE_INTEGER pos;
   DWORD         written, junk;
   HANDLE        h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0,
         NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
   if (h == INVALID_HANDLE_VALUE)
      return false;
   DeviceIoControl(h, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &junk, NULL);
   pos.QuadPart = (LONGLONG)size;
   if (!SetFilePointerEx(h, pos, NULL, FILE_BEGIN) || !SetEndOfFile(h))
   {
      CloseHandle(h);
      DeleteFileA(path);
      return false;
   }
   for (i = 0; i < n; i++)
   {
      pos.QuadPart = (LONGLONG)spans[i].at;
      if (     !SetFilePointerEx(h, pos, NULL, FILE_BEGIN)
            || !WriteFile(h, spans[i].data, (DWORD)spans[i].len, &written,
               NULL)
            || written != spans[i].len)
      {
         CloseHandle(h);
         DeleteFileA(path);
         return false;
      }
   }
   CloseHandle(h);
   return true;
#else
   int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
   if (fd < 0)
      return false;
   if (sizeof(off_t) < sizeof(uint64_t) || ftruncate(fd, (off_t)size) != 0)
   {
      close(fd);
      remove(path);
      return false;
   }
   for (i = 0; i < n; i++)
      if (     lseek(fd, (off_t)spans[i].at, SEEK_SET) != (off_t)spans[i].at
            || write(fd, spans[i].data, spans[i].len)
               != (ssize_t)spans[i].len)
      {
         close(fd);
         remove(path);
         return false;
      }
   close(fd);
   return true;
#endif
}

static void test_big(void)
{
   static const uint8_t after[] = "the member after four gibibytes\n";
   uint8_t  head[128], tail[512];
   struct member m[2];
   struct span   sp[2];
   uint64_t at, cd, size;
   size_t   n0, t = 0;
   unsigned i;

   m[0].name = "big.iso";   m[0].data = NULL;  m[0].method = 0;
   m[0].usize = m[0].csize = BIG_LEN;
   m[0].crc = 0x12345678u;
   m[0].big_sizes = true;   m[0].big_offset = false;
   m[0].offset = 0;
   m[1].name = "after.txt"; m[1].data = after; m[1].method = 0;
   m[1].usize = m[1].csize = sizeof(after) - 1;
   m[1].crc = encoding_crc32(0, after, sizeof(after) - 1);
   m[1].big_sizes = false;  m[1].big_offset = true;

   n0          = local_header(head, &m[0]);
   m[1].offset = n0 + BIG_LEN;
   t          += local_header(tail, &m[1]);
   memcpy(tail + t, after, sizeof(after) - 1);
   t          += sizeof(after) - 1;
   cd          = m[1].offset + t;
   at          = 0;
   for (i = 0; i < 2; i++)
      at += central_header(tail + t + (size_t)at, &m[i]);
   t   += directory_end(tail + t + (size_t)at, cd + at, cd, at, 2);
   t   += (size_t)at;
   size = m[1].offset + t;

   sp[0].at = 0;           sp[0].data = head; sp[0].len = n0;
   sp[1].at = m[1].offset; sp[1].data = tail; sp[1].len = t;
   if (!make_sparse(BIG_PATH, size, sp, 2))
   {
      printf("skip: no room for a sparse archive of %llu octets\n",
            (unsigned long long)size);
      return;
   }

   if (!listed(BIG_PATH, 2))
      fail("past 4 GiB", "listed");
   {
      uint64_t len = 0;
      if (     file_archive_get_file_crc32_and_size(BIG_PATH "#big.iso", &len)
               != 0x12345678u
            || len != BIG_LEN)
         fail("past 4 GiB", "the big member's size");
   }
   {
      int64_t usize = 0;
      file_archive_entry_source_t *src =
            file_archive_entry_source_open(BIG_PATH "#big.iso", &usize);
      if (!src || usize != (int64_t)BIG_LEN)
         fail("past 4 GiB", "the big member's entry source");
      if (src)
         file_archive_entry_source_close(src);
   }
   check_member("past 4 GiB, the member after", BIG_PATH, "after.txt",
         after, sizeof(after) - 1);
   remove(BIG_PATH);
}

int main(void)
{
   test_plain_in_place();
   test_small();
   test_big();
   if (failures)
   {
      printf("%u check(s) failed\n", failures);
      return 1;
   }
   printf("archive_zip64_test: ZIP64 members listed, read and extracted\n");
   return 0;
}
