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

int main(void)
{
   sprintf(fixture, "subtitle-load-%lu.srt", (unsigned long)sample_getpid());
   assert(!task_subtitle_load(NULL) && !task_subtitle_load(""));
   assert(task_subtitle_status(NULL) == -1 && !task_subtitle_take(NULL));
   task_subtitle_release(NULL);
   run_mode(false);
#ifdef HAVE_THREADS
   run_mode(true);
#endif
   return 0;
}
