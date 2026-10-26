/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------
 * The following license statement only applies to this file (subtitle_load_test.c).
 * ---------------------------------------------------------------------
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
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <retro_atomic.h>
#include <streams/file_stream.h>
#include <string.h>
#ifdef _WIN32
#include <process.h>
#define sample_getpid _getpid
#else
#include <unistd.h>
#define sample_getpid getpid
#endif
#include <features/features_cpu.h>
#include <queues/task_queue.h>
#include <retro_timers.h>
#include "../../../tasks/task_subtitle.h"
#include <formats/rsubtitle_vtt.h>

static retro_atomic_int_t embedded_read_bytes;
int64_t subtitle_test_filestream_read_real(RFILE *stream, void *data, int64_t length);
int64_t filestream_read(RFILE *stream, void *data, int64_t length)
{
   int64_t got = subtitle_test_filestream_read_real(stream, data, length);
   if (got > 0 && got < 16777216)
      retro_atomic_fetch_add_int(&embedded_read_bytes, (int)got);
   return got;
}

static char fixture[64];

static bool any_task(retro_task_t *task, void *data)
{
   (void)task;
   (void)data;
   return true;
}

static int busy(void)
{
   task_finder_data_t finder;
   memset(&finder, 0, sizeof(finder));
   finder.func = any_task;
   return task_queue_find(&finder);
}

static void drain(void)
{
   int64_t deadline = cpu_features_get_time_usec() + 10000000;
   do
   {
      task_queue_check();
      if (!busy()) return;
      retro_sleep(1);
   } while (cpu_features_get_time_usec() < deadline);
   assert(!busy());
}

static void write_valid(void)
{
   unsigned i;
   FILE *file = fopen(fixture, "wb");
   assert(file);
   for (i = 0; i < 4000; i++)
   {
      unsigned ms = (i * 1709) % 50000;
      unsigned end = ms + 300;
      assert(fprintf(file,
            "%u\r\n00:00:%02u,%03u --> 00:00:%02u,%03u\r\n"
            "cue %u \303\251\r\n\r\n", i, ms / 1000, ms % 1000,
            end / 1000, end % 1000, i) > 0);
   }
   assert(!fclose(file));
}

static unsigned await(task_subtitle_request_t *request)
{
   int64_t deadline = cpu_features_get_time_usec() + 10000000;
   unsigned ticks = 0;
   while (!task_subtitle_status(request)
         && cpu_features_get_time_usec() < deadline)
   {
      task_queue_check();
      ticks++;
      retro_sleep(1);
   }
   assert(task_subtitle_status(request));
   return ticks;
}

static void run_mode(bool threaded)
{
   task_subtitle_request_t *request;
   rsubtitle_srt_t *track;
   unsigned i, ticks;
   char missing[96];
   FILE *file;
   if (threaded) task_queue_set_threaded();
   else task_queue_unset_threaded();
   task_queue_init(threaded, NULL);
   write_valid();
   request = task_subtitle_load(fixture);
   assert(request);
   if (!threaded)
   {
      assert(!task_subtitle_status(request));
      assert(!task_subtitle_take(request));
   }
   ticks = await(request);
   assert(task_subtitle_status(request) == 1);
   if (!threaded) assert(ticks > 2);
   track = task_subtitle_take(request);
   assert(track && rsubtitle_srt_count(track) == 4000);
   assert(task_subtitle_status(request) == 2 && !task_subtitle_take(request));
   assert(strstr(rsubtitle_srt_cue(track, 0)->text, "cue "));
   task_subtitle_release(request);
   rsubtitle_srt_free(track);
   drain();

   sprintf(missing, "%s.missing", fixture);
   request = task_subtitle_load(missing);
   assert(request);
   await(request);
   assert(task_subtitle_status(request) == -1 && !task_subtitle_take(request));
   task_subtitle_release(request);
   drain();

   file = fopen(fixture, "wb");
   assert(file && fputs("invalid subtitle\377", file) >= 0 && !fclose(file));
   request = task_subtitle_load(fixture);
   assert(request);
   await(request);
   assert(task_subtitle_status(request) == -1 && !task_subtitle_take(request));
   task_subtitle_release(request);
   drain();

   file = fopen(fixture, "wb");
   assert(file && !fseek(file, (long)RSUBTITLE_SRT_MAX_INPUT, SEEK_SET));
   assert(fputc('x', file) != EOF && !fclose(file));
   request = task_subtitle_load(fixture);
   assert(request);
   await(request);
   assert(task_subtitle_status(request) == -1);
   task_subtitle_release(request);
   drain();

   write_valid();
   if (!threaded)
   {
      request = task_subtitle_load(fixture);
      assert(request && !remove(fixture));
      await(request);
      assert(task_subtitle_status(request) == -1);
      task_subtitle_release(request);
      drain();
      write_valid();
      request = task_subtitle_load(fixture);
      assert(request);
      task_queue_check();
      file = fopen(fixture, "wb");
      assert(file && fputs("short", file) >= 0 && !fclose(file));
      await(request);
      assert(task_subtitle_status(request) == -1);
      task_subtitle_release(request);
      drain();
      write_valid();
   }
   for (i = 0; i < 64; i++)
   {
      request = task_subtitle_load(fixture);
      assert(request);
      if (!threaded && (i & 1)) task_queue_check();
      task_subtitle_release(request);
   }
   drain();
   request = task_subtitle_load(fixture);
   assert(request);
   task_queue_reset();
   drain();
   if (task_subtitle_status(request) == 1)
   {
      track = task_subtitle_take(request);
      assert(threaded && track);
      rsubtitle_srt_free(track);
   }
   else
      assert(task_subtitle_status(request) == -1);
   task_subtitle_release(request);
   task_queue_deinit();
   assert(!remove(fixture));
   printf("PASS: subtitle task load, bounded progress, retirement and cancellation (%s)\n",
         threaded ? "threaded" : "regular");
}

#ifdef HAVE_RMP4
static void embedded(const char *path, bool threaded)
{
   task_subtitle_request_t *request;
   rsubtitle_srt_t *track;
   const rsubtitle_srt_cue_t *cue;
   unsigned i;
   task_subtitle_catalog_t *catalog;
   const task_subtitle_entry_t *entry;
   task_subtitle_entry_t selected;
   bool delayed = getenv("SUBTITLE_EXPECT_VTT_DELAY") != NULL;
   int64_t shift_us = delayed ? 500001 : 0;
   if (threaded) task_queue_set_threaded();
   else task_queue_unset_threaded();
   task_queue_init(threaded, NULL);
   retro_atomic_store_release_int(&embedded_read_bytes, 0);
   catalog = task_subtitle_discover(path);
   assert(catalog);
   drain();
   assert(task_subtitle_catalog_status(catalog) == 1);
   assert(task_subtitle_catalog_count(catalog) == 2);
   entry = task_subtitle_catalog_entry(catalog, 0);
   assert(entry && entry->source == (getenv("SUBTITLE_EXPECT_WEBM")
            ? TASK_SUBTITLE_WEBM : TASK_SUBTITLE_MP4) && entry->track == 0);
   selected = *entry;
   selected.path = path;
   assert(!strcmp(entry->path, path));
   request = task_subtitle_load_entry(entry);
   task_subtitle_catalog_release(catalog);
   assert(request && !task_subtitle_status(request));
   await(request);
   if (getenv("SUBTITLE_EXPECT_FAILURE"))
   {
      assert(task_subtitle_status(request) == -1 && !task_subtitle_take(request));
      assert(retro_atomic_load_acquire_int(&embedded_read_bytes) < 262144);
      task_subtitle_release(request);
      drain();
      task_queue_deinit();
      puts("PASS: malformed embedded text refused without publishing cues");
      return;
   }
   assert(task_subtitle_status(request) == 1);
   assert(retro_atomic_load_acquire_int(&embedded_read_bytes) > 0);
   assert(retro_atomic_load_acquire_int(&embedded_read_bytes) < 262144);
   track = task_subtitle_take(request);
   assert(track && rsubtitle_srt_count(track) == 2);
   if (getenv("SUBTITLE_EXPECT_LANGUAGE_SET"))
   {
      const char *expected = getenv("SUBTITLE_EXPECT_LANGUAGE");
      const char *expected_key = getenv("SUBTITLE_EXPECT_LANGUAGE_KEY");
      const char *value;
      size_t length;
      if (!expected) expected = "";
      if (!expected_key) expected_key = expected;
      value = rsubtitle_vtt_track_language(track, &length);
      assert(value && length == strlen(expected) && !memcmp(value, expected, length));
      value = rsubtitle_vtt_track_language_key(track, &length);
      if (!value || length != strlen(expected_key) || memcmp(value, expected_key, length))
         fprintf(stderr, "language key: expected [%s], got [%.*s]\n",
               expected_key, (int)length, value ? value : "");
      assert(value && length == strlen(expected_key) && !memcmp(value, expected_key, length));
   }
   cue = rsubtitle_srt_cue(track, 0);
   assert(cue && cue->start_us == (delayed ? 0 : 100000)
         && cue->end_us == 1200000 - shift_us);
   assert(!strcmp(cue->text, getenv("SUBTITLE_EXPECT_UTF16")
            ? "Z\360\237\230\200" : "first caf\303\251"));
   if (getenv("SUBTITLE_EXPECT_WEBVTT"))
   {
      const rsubtitle_vtt_cue_t *info = rsubtitle_vtt_cue_info(track, 0);
      const rsubtitle_vtt_block_t *block = rsubtitle_vtt_block(track, 0);
      bool empty = getenv("SUBTITLE_EXPECT_VTT_EMPTY") != NULL;
      assert(info && !strcmp(info->settings, empty ? "" : "align:start"));
      assert(info->flags == RSUBTITLE_VTT_CUE_RELATIVE_TIMESTAMPS
            && info->timestamp_origin_ns == (delayed ? -400000123 : 100000000));
      assert(empty ? !info->identifier : !strcmp(info->identifier, "first-id"));
      assert(rsubtitle_vtt_block_count(track) == 2 && block
            && block->kind == RSUBTITLE_VTT_STYLE
            && !strcmp(block->text, "::cue { color: lime; }"));
      block = rsubtitle_vtt_block(track, 1);
      assert(block && block->kind == RSUBTITLE_VTT_REGION
            && !strcmp(block->text, "id:lower"));
      info = rsubtitle_vtt_cue_info(track, 1);
      assert(info && !strcmp(info->settings, empty ? "" : "align:end"));
      assert(info->flags == RSUBTITLE_VTT_CUE_RELATIVE_TIMESTAMPS
            && info->timestamp_origin_ns == (delayed ? 1499999877 : 2000000000));
      assert(empty ? !info->identifier : !strcmp(info->identifier, "second-id"));
   }
   cue = rsubtitle_srt_cue(track, 1);
   assert(cue && cue->start_us == 2000000 - shift_us
         && cue->end_us == (getenv("SUBTITLE_EXPECT_DEFAULT") ? 3100000 : 3500000) - shift_us);
   assert(!strcmp(cue->text, "second \360\237\230\200"));
   rsubtitle_srt_free(track);
   task_subtitle_release(request);
   drain();
   selected.track = 9;
   request = task_subtitle_load_entry(&selected);
   assert(request);
   await(request);
   assert(task_subtitle_status(request) == -1);
   task_subtitle_release(request);
   drain();
   for (i = 0; i < 32; i++)
   {
      selected.track = 0;
      request = task_subtitle_load_entry(&selected);
      assert(request);
      if (!threaded)
      {
         unsigned step;
         for (step = 0; step < i % 12; step++) task_queue_check();
      }
      task_subtitle_release(request);
   }
   drain();
   task_queue_deinit();
   puts("PASS: embedded text, container intervals, invalid track and cancellation");
}
#endif

static void vtt_mode(bool threaded)
{
   static const char vtt[] = "WEBVTT\n\nSTYLE\n::cue { color: lime; }\n\n"
      "id\n00:00.100 --> 00:01.200 align:center\nWebVTT \303\251\n";
   task_subtitle_catalog_t *catalog;
   task_subtitle_request_t *request;
   rsubtitle_srt_t *track;
   char movie[96], paths[3][96], decoy[96];
   FILE *file;
   unsigned i;
   unsigned long pid = (unsigned long)sample_getpid();
   if (threaded) task_queue_set_threaded();
   else task_queue_unset_threaded();
   task_queue_init(threaded, NULL);
   sprintf(movie, "subtitle-vtt-%lu.bin", pid);
   sprintf(paths[0], "subtitle-vtt-%lu.en.VTT", pid);
   sprintf(paths[1], "subtitle-vtt-%lu.srt", pid);
   sprintf(paths[2], "subtitle-vtt-%lu.vtt", pid);
   sprintf(decoy, "subtitle-vtt-%lu-other.vtt", pid);
   for (i = 0; i < 3; i++)
   {
      file = fopen(paths[i], "wb");
      assert(file && fputs(i == 1
               ? "1\n00:00:00,100 --> 00:00:01,200\nSubRip\n" : vtt, file) >= 0);
      assert(!fclose(file));
   }
   file = fopen(decoy, "wb");
   assert(file && fputs(vtt, file) >= 0 && !fclose(file));
   catalog = task_subtitle_discover(movie);
   assert(catalog);
   for (i = 0; i < 10000 && !task_subtitle_catalog_status(catalog); i++)
   {
      task_queue_check();
      retro_sleep(1);
   }
   assert(task_subtitle_catalog_status(catalog) == 1
         && task_subtitle_catalog_count(catalog) == 3);
   for (i = 0; i < 3; i++)
   {
      const task_subtitle_entry_t *entry = task_subtitle_catalog_entry(catalog, i);
      const rsubtitle_vtt_cue_t *info;
      assert(entry && entry->source == TASK_SUBTITLE_SIDECAR);
      assert(strstr(entry->path, paths[i]));
      request = task_subtitle_load_entry(entry);
      assert(request);
      await(request);
      assert(task_subtitle_status(request) == 1);
      track = task_subtitle_take(request);
      assert(track && rsubtitle_srt_count(track) == 1);
      assert(rsubtitle_srt_cue(track, 0)->start_us == 100000
            && rsubtitle_srt_cue(track, 0)->end_us == 1200000);
      info = rsubtitle_vtt_cue_info(track, 0);
      if (i == 1)
         assert(!info && !strcmp(rsubtitle_srt_cue(track, 0)->text, "SubRip"));
      else
      {
         assert(info && !strcmp(info->identifier, "id"));
         assert(!strcmp(info->settings, "align:center"));
         assert(!strcmp(rsubtitle_srt_cue(track, 0)->text, "WebVTT \303\251"));
         assert(rsubtitle_vtt_block_count(track) == 1);
      }
      rsubtitle_srt_free(track);
      task_subtitle_release(request);
      drain();
   }
   task_subtitle_catalog_release(catalog);
   for (i = 0; i < 16; i++)
   {
      request = task_subtitle_load(paths[2]);
      assert(request);
      if (!threaded)
      {
         unsigned step;
         for (step = 0; step < i % 5; step++) task_queue_check();
      }
      task_subtitle_release(request);
   }
   drain();
   task_queue_deinit();
   for (i = 0; i < 3; i++) assert(!remove(paths[i]));
   assert(!remove(decoy));
   puts("PASS: VTT/SRT sidecar discovery, worker metadata and cancellation");
}

int main(int argc, char **argv)
{
#ifdef HAVE_RMP4
   if (argc == 2)
   {
      embedded(argv[1], false);
#ifdef HAVE_THREADS
      embedded(argv[1], true);
#endif
      return 0;
   }
#else
   (void)argc; (void)argv;
#endif
   sprintf(fixture, "subtitle-load-%lu.srt", (unsigned long)sample_getpid());
   assert(!task_subtitle_load(NULL) && !task_subtitle_load(""));
   assert(task_subtitle_status(NULL) == -1 && !task_subtitle_take(NULL));
   task_subtitle_release(NULL);
   run_mode(false);
   vtt_mode(false);
#ifdef HAVE_THREADS
   run_mode(true);
   vtt_mode(true);
#endif
   return 0;
}
