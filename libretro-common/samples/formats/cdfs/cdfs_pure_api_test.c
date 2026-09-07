/* Regression tests for the pure (no path I/O) cdfs API:
 *
 *   - cdfs_parse_cue():        cue-sheet parsing from a memory buffer,
 *                              including quoted names, CRLF endings,
 *                              tab separators, multi-track offset
 *                              arithmetic, audio tracks, truncated
 *                              buffers and missing-newline final lines.
 *   - cdfs_track_from_stream():wrapping an in-memory raw MODE1/2352
 *                              image, ISO-9660 directory walk, file
 *                              read and seek - end to end without a
 *                              single filesystem touch.
 *
 * The whole test runs on memory streams, so it is a pure consumer of
 * the split introduced by the formats I/O removal work and runs clean
 * under ASan/UBSan/LSan.
 *
 * Build (see Makefile):
 *   make -C libretro-common/samples/formats/cdfs cdfs_pure_api_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <formats/cdfs.h>
#include <streams/interface_stream.h>

static int failures = 0;

#define CHECK(cond, name) \
   do { \
      if (cond) \
         printf("[SUCCESS] %s\n", name); \
      else \
      { \
         printf("[ERROR] %s (line %d)\n", name, __LINE__); \
         failures++; \
      } \
   } while (0)

/* --------------------------------------------------------------------
 * cdfs_parse_cue
 * ------------------------------------------------------------------ */

static void test_parse_cue_single_track(void)
{
   cdfs_cue_info_t info;
   static const char cue[] =
      "FILE \"game.bin\" BINARY\n"
      "  TRACK 01 MODE1/2352\n"
      "    INDEX 01 00:00:00\n";

   CHECK(cdfs_parse_cue(cue, sizeof(cue) - 1, 1, &info),
         "parse_cue: single track found");
   CHECK(!strcmp(info.bin_name, "game.bin"),
         "parse_cue: bin name extracted");
   CHECK(info.track_index == 1, "parse_cue: track index");
   CHECK(info.sector_size == 2352, "parse_cue: sector size");
   CHECK(info.first_sector_offset == 0, "parse_cue: offset 0");
   CHECK(info.first_sector_index == 0, "parse_cue: sector index 0");

   /* track_index = 0 selects the first data track */
   CHECK(cdfs_parse_cue(cue, sizeof(cue) - 1, 0, &info)
         && info.track_index == 1,
         "parse_cue: track_index 0 finds first data track");
}

static void test_parse_cue_quoted_name_with_spaces(void)
{
   cdfs_cue_info_t info;
   static const char cue[] =
      "FILE \"my game (disc 1).bin\" BINARY\n"
      "TRACK 01 MODE2/2352\n"
      "INDEX 01 00:00:00\n";

   CHECK(cdfs_parse_cue(cue, sizeof(cue) - 1, 0, &info)
         && !strcmp(info.bin_name, "my game (disc 1).bin"),
         "parse_cue: quoted name with spaces");
   CHECK(info.sector_size == 2352, "parse_cue: MODE2/2352 size");
}

static void test_parse_cue_second_data_track_offset(void)
{
   cdfs_cue_info_t info;
   static const char cue[] =
      "FILE \"disc.bin\" BINARY\n"
      "  TRACK 01 MODE1/2352\n"
      "    INDEX 01 00:00:00\n"
      "  TRACK 02 MODE2/2352\n"
      "    INDEX 01 00:02:00\n";

   CHECK(cdfs_parse_cue(cue, sizeof(cue) - 1, 2, &info),
         "parse_cue: second data track found");
   /* 00:02:00 = 150 sectors of 2352 bytes into the same bin. */
   CHECK(info.first_sector_offset == 150 * 2352,
         "parse_cue: second track byte offset");
   CHECK(info.first_sector_index == 150,
         "parse_cue: second track sector index");
   CHECK(info.sector_size == 2352, "parse_cue: second track size");
}

static void test_parse_cue_audio_pregap_arithmetic(void)
{
   cdfs_cue_info_t info;
   /* Data track behind an audio track with a pregap (INDEX 00):
    * the data track's INDEX 01 sits at 02:00:00 = 9000 sectors. */
   static const char cue[] =
      "FILE \"mixed.bin\" BINARY\n"
      "  TRACK 01 AUDIO\n"
      "    INDEX 01 00:00:00\n"
      "  TRACK 02 MODE1/2352\n"
      "    INDEX 00 01:58:00\n"
      "    INDEX 01 02:00:00\n";

   CHECK(cdfs_parse_cue(cue, sizeof(cue) - 1, 0, &info),
         "parse_cue: data track behind audio found");
   CHECK(info.track_index == 2, "parse_cue: audio track skipped");
   CHECK(info.first_sector_index == 9000,
         "parse_cue: pregap sector arithmetic");
   CHECK(info.first_sector_offset == 9000 * 2352,
         "parse_cue: pregap byte arithmetic");
}

static void test_parse_cue_audio_only_not_found(void)
{
   cdfs_cue_info_t info;
   static const char cue[] =
      "FILE \"cdda.bin\" BINARY\n"
      "TRACK 01 AUDIO\n"
      "INDEX 01 00:00:00\n";

   CHECK(!cdfs_parse_cue(cue, sizeof(cue) - 1, 0, &info),
         "parse_cue: audio-only cue yields no data track");
}

static void test_parse_cue_crlf_tabs_no_trailing_newline(void)
{
   cdfs_cue_info_t info;
   /* CRLF endings, tab separators, and no newline on the last line. */
   static const char cue[] =
      "FILE\t\"crlf.bin\"\tBINARY\r\n"
      "TRACK\t01\tMODE1/2048\r\n"
      "INDEX\t01\t00:00:00";

   CHECK(cdfs_parse_cue(cue, sizeof(cue) - 1, 1, &info),
         "parse_cue: CRLF + tabs + missing final newline");
   CHECK(!strcmp(info.bin_name, "crlf.bin"),
         "parse_cue: CRLF bin name");
   CHECK(info.sector_size == 2048, "parse_cue: MODE1/2048 size");
}

static void test_parse_cue_hostile_inputs(void)
{
   cdfs_cue_info_t info;
   static const char cue[] =
      "FILE \"game.bin\" BINARY\n"
      "TRACK 01 MODE1/2352\n"
      "INDEX 01 00:00:00\n";
   size_t i;

   /* Every truncation of a valid cue must parse without reading past
    * the buffer (ASan-checked) and without crashing.  A heap copy of
    * exactly i bytes gives the sanitizer a real object boundary. */
   for (i = 0; i < sizeof(cue) - 1; i++)
   {
      char *partial = (char*)malloc(i ? i : 1);
      if (!partial)
         continue;
      memcpy(partial, cue, i);
      (void)cdfs_parse_cue(partial, i, 1, &info);
      free(partial);
   }
   printf("[SUCCESS] parse_cue: all truncations handled\n");

   CHECK(!cdfs_parse_cue(NULL, 0, 1, &info), "parse_cue: NULL text");
   CHECK(!cdfs_parse_cue(cue, sizeof(cue) - 1, 1, NULL),
         "parse_cue: NULL out");
   CHECK(!cdfs_parse_cue("", 0, 1, &info), "parse_cue: empty text");
   /* A found track with no preceding FILE line must fail cleanly. */
   {
      static const char nofile[] =
         "TRACK 01 MODE1/2352\nINDEX 01 00:00:00\n";
      CHECK(!cdfs_parse_cue(nofile, sizeof(nofile) - 1, 1, &info),
            "parse_cue: INDEX without FILE rejected");
   }
}

/* --------------------------------------------------------------------
 * cdfs_track_from_stream over an in-memory raw MODE1/2352 image
 * ------------------------------------------------------------------ */

#define TEST_SECTOR_SIZE   2352
#define TEST_HEADER_SIZE   16
#define TEST_NUM_SECTORS   22
#define TEST_ROOT_SECTOR   17
#define TEST_FILE_SECTOR   18
#define TEST_BIG_SECTOR    19
#define TEST_BIG_SIZE      5000 /* spans three 2048-byte sectors */

static const char test_file_content[] =
   "Hello from a purely in-memory ISO-9660 track!\n";

static uint8_t *build_raw_iso(size_t *out_len)
{
   size_t _len  = (size_t)TEST_NUM_SECTORS * TEST_SECTOR_SIZE;
   uint8_t *img = (uint8_t*)calloc(1, _len);
   uint8_t *data;
   uint8_t *rec;
   size_t fn_len;
   uint32_t fsize = (uint32_t)(sizeof(test_file_content) - 1);
   int s;

   if (!img)
      return NULL;

   /* 12-byte sync pattern + 3-byte address + mode byte per sector. */
   for (s = 0; s < TEST_NUM_SECTORS; s++)
   {
      uint8_t *sec = img + (size_t)s * TEST_SECTOR_SIZE;
      sec[0]  = 0;
      memset(sec + 1, 0xFF, 10);
      sec[11] = 0;
      sec[15] = 1; /* MODE1 */
   }

   /* Sector 16: primary volume descriptor.  "CD001" lands 17 bytes into
    * the raw sector (1 byte into the data area), which is the layout
    * cdfs_determine_sector_size probes for a 16-byte-header source. */
   data    = img + 16 * TEST_SECTOR_SIZE + TEST_HEADER_SIZE;
   data[0] = 1;
   memcpy(data + 1, "CD001", 5);

   /* Root directory record at data offset 156; bytes 2..4 hold the
    * root directory's extent sector (little-endian 24-bit). */
   data[156 + 2] = TEST_ROOT_SECTOR & 0xFF;
   data[156 + 3] = (TEST_ROOT_SECTOR >> 8) & 0xFF;
   data[156 + 4] = (TEST_ROOT_SECTOR >> 16) & 0xFF;

   /* Sector 17: root directory with records for README.TXT;1 and the
    * multi-sector BIG.DAT;1 */
   rec    = img + TEST_ROOT_SECTOR * TEST_SECTOR_SIZE + TEST_HEADER_SIZE;
   fn_len = strlen("README.TXT;1");
   rec[0] = (uint8_t)(33 + fn_len + 1); /* record length */
   rec[2] = TEST_FILE_SECTOR & 0xFF;    /* extent sector, LE24 */
   rec[3] = (TEST_FILE_SECTOR >> 8) & 0xFF;
   rec[4] = (TEST_FILE_SECTOR >> 16) & 0xFF;
   rec[10] = (uint8_t)(fsize & 0xFF);   /* data length, LE32 */
   rec[11] = (uint8_t)((fsize >> 8) & 0xFF);
   rec[12] = (uint8_t)((fsize >> 16) & 0xFF);
   rec[13] = (uint8_t)((fsize >> 24) & 0xFF);
   rec[32] = (uint8_t)fn_len;
   memcpy(rec + 33, "README.TXT;1", fn_len);

   rec    = rec + rec[0];
   fn_len = strlen("BIG.DAT;1");
   rec[0] = (uint8_t)(33 + fn_len + 1);
   rec[2] = TEST_BIG_SECTOR & 0xFF;
   rec[3] = (TEST_BIG_SECTOR >> 8) & 0xFF;
   rec[4] = (TEST_BIG_SECTOR >> 16) & 0xFF;
   rec[10] = (uint8_t)(TEST_BIG_SIZE & 0xFF);
   rec[11] = (uint8_t)((TEST_BIG_SIZE >> 8) & 0xFF);
   rec[12] = (uint8_t)((TEST_BIG_SIZE >> 16) & 0xFF);
   rec[13] = (uint8_t)((TEST_BIG_SIZE >> 24) & 0xFF);
   rec[32] = (uint8_t)fn_len;
   memcpy(rec + 33, "BIG.DAT;1", fn_len);

   /* Sector 18: README.TXT contents. */
   memcpy(img + TEST_FILE_SECTOR * TEST_SECTOR_SIZE + TEST_HEADER_SIZE,
         test_file_content, fsize);

   /* Sectors 19-21: BIG.DAT, a deterministic byte pattern spanning
    * three cooked sectors so sector-crossing reads and seeks are
    * exercised. */
   for (s = 0; s < 3; s++)
   {
      uint8_t *dst = img
            + (size_t)(TEST_BIG_SECTOR + s) * TEST_SECTOR_SIZE
            + TEST_HEADER_SIZE;
      size_t remain = TEST_BIG_SIZE - (size_t)s * 2048;
      size_t chunk  = (remain < 2048) ? remain : 2048;
      size_t i;
      for (i = 0; i < chunk; i++)
         dst[i] = (uint8_t)(((size_t)s * 2048 + i) * 31 + 7);
   }

   *out_len = _len;
   return img;
}

static void test_track_from_stream_iso(void)
{
   size_t img_len       = 0;
   uint8_t *img         = build_raw_iso(&img_len);
   intfstream_t *stream = NULL;
   cdfs_track_t *track  = NULL;
   cdfs_file_t file;
   char buf[128];
   int64_t n;

   if (!img)
   {
      printf("[ERROR] ISO image allocation failed\n");
      failures++;
      return;
   }

   stream = intfstream_open_memory(img,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE,
         img_len);
   track  = cdfs_track_from_stream(stream, 0, 0);

   CHECK(track != NULL, "track_from_stream: wrap succeeds");
   if (!track)
   {
      free(img);
      return;
   }

   CHECK(track->stream_sector_size == TEST_SECTOR_SIZE,
         "track_from_stream: sector size probed from CD001");
   CHECK(track->stream_sector_header_size == TEST_HEADER_SIZE,
         "track_from_stream: header size probed from CD001");

   CHECK(cdfs_open_file(&file, track, "README.TXT") != 0,
         "cdfs_open_file: directory walk finds file");
   CHECK(cdfs_get_size(&file) ==
         (int64_t)(sizeof(test_file_content) - 1),
         "cdfs_get_size: matches directory record");

   memset(buf, 0, sizeof(buf));
   n = cdfs_read_file(&file, buf, sizeof(buf));
   CHECK(n == (int64_t)(sizeof(test_file_content) - 1),
         "cdfs_read_file: full read length");
   CHECK(!memcmp(buf, test_file_content, (size_t)n),
         "cdfs_read_file: contents round-trip");

   /* Seek back into the middle and re-read. */
   CHECK(cdfs_seek(&file, 6, SEEK_SET) == 0, "cdfs_seek: SEEK_SET");
   memset(buf, 0, sizeof(buf));
   n = cdfs_read_file(&file, buf, 4);
   CHECK(n == 4 && !memcmp(buf, "from", 4),
         "cdfs_read_file: read after seek");

   cdfs_close_file(&file);

   /* Missing file must not be found. */
   CHECK(cdfs_open_file(&file, track, "MISSING.BIN") == 0,
         "cdfs_open_file: absent file rejected");

   /* NULL path opens the raw cooked view of the track. */
   CHECK(cdfs_open_file(&file, track, NULL) == 1,
         "cdfs_open_file: raw (NULL path) open");
   CHECK(cdfs_get_size(&file) ==
         (int64_t)TEST_NUM_SECTORS * 2048,
         "cdfs_open_file: raw size = sectors * 2048");
   cdfs_close_file(&file);

   cdfs_close_track(track);
   free(img);
   printf("[SUCCESS] track_from_stream: track closed cleanly\n");
}

static void test_multi_sector_file(void)
{
   size_t img_len       = 0;
   uint8_t *img         = build_raw_iso(&img_len);
   intfstream_t *stream = NULL;
   cdfs_track_t *track  = NULL;
   cdfs_file_t file;
   uint8_t *buf         = NULL;
   int64_t n;
   size_t i;
   int ok;

   if (!img)
   {
      printf("[ERROR] ISO image allocation failed\n");
      failures++;
      return;
   }

   stream = intfstream_open_memory(img,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE,
         img_len);
   track  = cdfs_track_from_stream(stream, 0, 0);
   if (!track)
   {
      printf("[ERROR] track wrap failed\n");
      failures++;
      free(img);
      return;
   }

   CHECK(cdfs_open_file(&file, track, "BIG.DAT") != 0,
         "multi-sector: file found");
   CHECK(cdfs_get_size(&file) == TEST_BIG_SIZE,
         "multi-sector: size from record");

   buf = (uint8_t*)malloc(TEST_BIG_SIZE);
   if (buf)
   {
      /* Full sequential read across three sectors. */
      n  = cdfs_read_file(&file, buf, TEST_BIG_SIZE);
      ok = (n == TEST_BIG_SIZE);
      for (i = 0; ok && i < TEST_BIG_SIZE; i++)
         if (buf[i] != (uint8_t)(i * 31 + 7))
            ok = 0;
      CHECK(ok, "multi-sector: sequential read pattern");

      /* Seek into the third sector and read across nothing special;
       * this drives the new_sector != current_sector branch of
       * cdfs_seek with a non-zero first_sector. */
      CHECK(cdfs_seek(&file, 4500, SEEK_SET) == 0,
            "multi-sector: seek into third sector");
      n  = cdfs_read_file(&file, buf, 100);
      ok = (n == 100);
      for (i = 0; ok && i < 100; i++)
         if (buf[i] != (uint8_t)((4500 + i) * 31 + 7))
            ok = 0;
      CHECK(ok, "multi-sector: read after cross-sector seek");

      /* The read above is served entirely from the sector cache;
       * cdfs_tell must still advance (SEEK_CUR below depends on it). */
      CHECK(cdfs_tell(&file) == 4600,
            "multi-sector: tell after cache-hit read");

      /* Seek backwards with SEEK_CUR and read across the sector
       * boundary at 2048. */
      CHECK(cdfs_seek(&file, -(4600 - 2000), SEEK_CUR) == 0,
            "multi-sector: relative seek back");
      n  = cdfs_read_file(&file, buf, 100);
      ok = (n == 100);
      for (i = 0; ok && i < 100; i++)
         if (buf[i] != (uint8_t)((2000 + i) * 31 + 7))
            ok = 0;
      CHECK(ok, "multi-sector: read spanning sector boundary");

      free(buf);
   }

   cdfs_close_file(&file);
   cdfs_close_track(track);
   free(img);
}

static void test_track_from_stream_null(void)
{
   CHECK(cdfs_track_from_stream(NULL, 0, 0) == NULL,
         "track_from_stream: NULL stream rejected");
}

int main(void)
{
   test_parse_cue_single_track();
   test_parse_cue_quoted_name_with_spaces();
   test_parse_cue_second_data_track_offset();
   test_parse_cue_audio_pregap_arithmetic();
   test_parse_cue_audio_only_not_found();
   test_parse_cue_crlf_tabs_no_trailing_newline();
   test_parse_cue_hostile_inputs();

   test_track_from_stream_iso();
   test_multi_sector_file();
   test_track_from_stream_null();

   if (failures)
   {
      printf("%d FAILURE(S)\n", failures);
      return 1;
   }

   printf("ALL TESTS PASSED\n");
   return 0;
}
