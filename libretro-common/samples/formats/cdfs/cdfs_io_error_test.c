/* Real CDFS sector/cache logic over memory streams, with fault injection
 * at the stream boundary and a sparse large-disc simulator. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <libretro.h>
#include <formats/cdfs.h>
#include <streams/interface_stream.h>

static unsigned failures, reads, seeks;
static int fail_read, fail_seek, short_read, sparse;
static uint64_t sparse_size, last_seek;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); failures++; } } while (0)
static unsigned char pattern(uint64_t pos) { return (unsigned char)(pos % 251); }
static int64_t checked_read(intfstream_t *stream, void *buf, uint64_t len)
{
   uint64_t i;
   reads++;
   if ((int)reads == fail_read)
   {
      if (!short_read) return -1;
      len /= 2;
   }
   if (!sparse) return intfstream_read(stream, buf, len);
   for (i = 0; i < len; i++) ((unsigned char*)buf)[i] = pattern(last_seek + i);
   return (int64_t)len;
}
static int64_t checked_seek(intfstream_t *stream, int64_t offset, int whence)
{
   seeks++;
   if ((int)seeks == fail_seek) return -1;
   last_seek = (uint64_t)offset;
   if (sparse) return 0;
   return intfstream_seek(stream, offset, whence);
}
static int64_t checked_size(intfstream_t *stream)
{
   return sparse ? (int64_t)sparse_size : intfstream_get_size(stream);
}
#define intfstream_read checked_read
#define intfstream_seek checked_seek
#define intfstream_get_size checked_size
#include "../../../formats/cdfs/cdfs.c"
#undef intfstream_read
#undef intfstream_seek
#undef intfstream_get_size

static void reset(void)
{ reads = seeks = 0; fail_read = fail_seek = short_read = 0; }
static void check_pattern(const unsigned char *buf, size_t n, uint64_t start)
{
   size_t i;
   for (i = 0; i < n; i++) CHECK(buf[i] == pattern(start + i));
}
int main(void)
{
   unsigned char *image = (unsigned char*)malloc(20 * 2048);
   unsigned char *buf = (unsigned char*)malloc(4 * 2048);
   cdfs_track_t track;
   static cdfs_file_t file;
   int mode, at;
   size_t i;
   CHECK(image && buf);
   if (!image || !buf) return 1;
   for (i = 0; i < 20 * 2048; i++) image[i] = pattern(i);
   memset(&track, 0, sizeof(track));
   track.stream_sector_size = 2048;
   track.stream = intfstream_open_memory(image, RETRO_VFS_FILE_ACCESS_READ, 0, 20 * 2048);
   CHECK(track.stream != NULL);
   if (!track.stream) return 1;
   for (mode = 0; mode < 2; mode++)
      for (at = 1; at <= 3; at++)
      {
         int64_t got, prefix = (at - 1) * 2048;
         reset();
         CHECK(cdfs_open_file(&file, &track, NULL) == 1);
         fail_read = at;
         short_read = mode;
         got = cdfs_read_file(&file, buf, 3 * 2048);
         CHECK(got == (prefix ? prefix : -1));
         CHECK(cdfs_tell(&file) == prefix && !file.sector_buffer_valid);
         CHECK(file.current_sector == at - 1);
         check_pattern(buf, (size_t)prefix, 0);
         fail_read = 0;
         CHECK(cdfs_read_file(&file, buf, 3 * 2048 - prefix) == 3 * 2048 - prefix);
         check_pattern(buf, (size_t)(3 * 2048 - prefix), (uint64_t)prefix);
      }
   reset();
   CHECK(cdfs_open_file(&file, &track, NULL) == 1);
   CHECK(cdfs_seek(&file, 100, SEEK_SET) == 0);
   fail_read = 1; short_read = 1;
   memset(buf, 0xa5, 100);
   CHECK(cdfs_read_file(&file, buf, 100) == -1);
   CHECK(cdfs_tell(&file) == 100 && !file.sector_buffer_valid && buf[0] == 0xa5);
   fail_read = 0;
   CHECK(cdfs_read_file(&file, buf, 100) == 100);
   check_pattern(buf, 100, 100);
   at = (int)reads;
   CHECK(cdfs_read_file(&file, buf, 100) == 100 && (int)reads == at);
   check_pattern(buf, 100, 200);
   CHECK(cdfs_seek(&file, 2040, SEEK_SET) == 0);
   fail_read = (int)reads + 1;
   CHECK(cdfs_read_file(&file, buf, 64) == 8);
   CHECK(cdfs_tell(&file) == 2048 && file.current_sector == 1 && !file.sector_buffer_valid);
   check_pattern(buf, 8, 2040);
   fail_read = 0;
   CHECK(cdfs_read_file(&file, buf, 56) == 56);
   check_pattern(buf, 56, 2048);
   reset();
   CHECK(cdfs_open_file(&file, &track, NULL) == 1);
   fail_seek = 1;
   CHECK(cdfs_read_file(&file, buf, 2048) == -1 && reads == 0);
   CHECK(cdfs_tell(&file) == 0 && !file.sector_buffer_valid);
   fail_seek = 0;
   CHECK(cdfs_read_file(&file, buf, 2048) == 2048);
   check_pattern(buf, 2048, 0);
   CHECK(cdfs_seek(&file, -1, SEEK_END) == 0);
   CHECK(cdfs_read_file(&file, buf, UINT64_MAX) == 1);
   CHECK(cdfs_read_file(&file, buf, 1) == 0);
   CHECK(cdfs_tell(&file) == 20 * 2048);
   CHECK(cdfs_seek(&file, INT64_MIN, SEEK_CUR) == -1);
   CHECK(cdfs_seek(&file, INT64_MAX, SEEK_END) == -1);
   CHECK(cdfs_tell(&file) == 20 * 2048);

   /* A valid file record in a short directory read must not be published. */
   memset(image, 0, 20 * 2048);
   image[16 * 2048 + 158] = 17;
   image[17 * 2048] = 38;
   image[17 * 2048 + 2] = 18;
   image[17 * 2048 + 10] = 64;
   image[17 * 2048 + 32] = 3;
   memcpy(image + 17 * 2048 + 33, "A;1", 3);
   for (mode = 0; mode < 2; mode++)
      for (at = 1; at <= 2; at++)
      {
         reset(); fail_read = at; short_read = mode;
         CHECK(cdfs_open_file(&file, &track, "A") == 0 && file.first_sector == -1);
      }
   reset();
   CHECK(cdfs_open_file(&file, &track, "A") == 1 && file.size == 64);
   image[17 * 2048 + 13] = 0x80;
   CHECK(cdfs_open_file(&file, &track, "A") == 1 && file.size == UINT64_C(0x80000040));

   /* Cooked/Mode2/Mode1 sparse DVDs, without allocating 8GiB images. */
   for (mode = 0; mode < 3; mode++)
   {
      static const unsigned sizes[3] = {2048, 2336, 2352};
      static const unsigned headers[3] = {0, 8, 16};
      uint64_t expected;
      reset(); sparse = 1;
      track.first_sector_offset = 12345;
      track.stream_sector_size = sizes[mode];
      track.stream_sector_header_size = headers[mode];
      sparse_size = UINT64_C(4194304) * sizes[mode] + 12345;
      CHECK(cdfs_open_file(&file, &track, NULL) == 1);
      CHECK(cdfs_get_size(&file) == INT64_C(8589934592));
      CHECK(cdfs_get_num_sectors(&file) == 4194304);
      {
         cdfs_track_t layout;
         memset(&layout, 0, sizeof(layout));
         layout.stream = track.stream;
         sparse_size = UINT64_C(4194307) * sizes[mode];
         cdfs_determine_sector_size_from_file_size(&layout);
         CHECK(layout.stream_sector_size == sizes[mode]);
         sparse_size = UINT64_C(4194304) * sizes[mode] + 12345;
      }
      CHECK(cdfs_seek(&file, -2048, SEEK_END) == 0);
      CHECK(cdfs_read_file(&file, buf, 2048) == 2048);
      expected = UINT64_C(4194303) * sizes[mode] + headers[mode] + 12345;
      CHECK(last_seek == expected);
      check_pattern(buf, 2048, expected);
      CHECK(cdfs_tell(&file) == INT64_C(8589934592));
      CHECK(cdfs_read_file(&file, buf, 1) == 0);
      cdfs_seek_sector(&file, 3000000);
      CHECK(cdfs_tell(&file) == INT64_C(6144000000));
      CHECK(cdfs_read_file(&file, buf, 7) == 7);
      expected = UINT64_C(3000000) * sizes[mode] + headers[mode] + 12345;
      CHECK(last_seek == expected);
      check_pattern(buf, 7, expected);
   }
   sparse = 0;
   intfstream_close(track.stream);
   free(track.stream);
   free(image); free(buf);
   printf("CDFS I/O fault tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
