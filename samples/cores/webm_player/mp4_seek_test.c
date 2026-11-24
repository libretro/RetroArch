/* Compare resumable seeks with independently decoded sequential frames. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <formats/rmp4.h>
#include <formats/rmp4_video.h>

static int metadata_only;

static int compare_pts(const void *a, const void *b)
{
   int64_t x = *(const int64_t*)a, y = *(const int64_t*)b;
   return x < y ? -1 : x > y;
}

static int check_file(const char *path)
{
   FILE *file = NULL;
   uint8_t *data = NULL;
   uint32_t *reference = NULL;
   int64_t *times = NULL;
   const int64_t *raw_times = NULL;
   const uint32_t *picture;
   rmp4_t *demux = NULL;
   rmp4_video_stream_t *stream = NULL;
   long length;
   size_t size, pixels;
   unsigned width = 0, height = 0;
   uint32_t samples = 0;
   int frames = 0, loops = 0, n, track, duration, failures = 1;
   int probe, probes;
   if (!(file = fopen(path, "rb"))) goto done;
   if (fseek(file, 0, SEEK_END) || (length = ftell(file)) <= 0
         || fseek(file, 0, SEEK_SET)) goto done;
   size = (size_t)length;
   if (!(data = (uint8_t*)malloc(size))
         || fread(data, 1, size, file) != size) goto done;
   fclose(file); file = NULL;
   if (!(demux = rmp4_open_memory(data, size))) goto done;
   for (track = 0; track < rmp4_num_tracks(demux); track++)
   {
      const rmp4_track *info = rmp4_get_track(demux, track);
      if (info && info->type == RMP4_TRACK_VIDEO)
      {
         raw_times = rmp4_track_pts(demux, track, &samples);
         break;
      }
   }
   if (!raw_times || !samples || samples > 16384) goto done;
   {
      rmp4_t *partial = rmp4_open_memory_avail(data, size, size / 2, NULL, NULL, NULL);
      uint32_t partial_samples = 0;
      if (partial) rmp4_track_pts(partial, track, &partial_samples);
      if (partial && partial_samples < samples)
      {
         int need_more = 0;
         stream = rmp4_video_stream_open_avail(data, size, size / 2,
               &need_more, NULL, NULL);
         if (stream || !need_more)
         {
            fprintf(stderr, "FAIL: partial fragment index published as complete movie\n");
            rmp4_close(partial);
            goto done;
         }
      }
      rmp4_close(partial);
   }
   if (!(stream = rmp4_video_stream_open(data, size))) goto done;
   rmp4_video_stream_get_info(stream, &width, &height, &frames, &loops);
   if (!width || !height || (!metadata_only && (width > 512 || height > 512))
         || frames != (int)samples)
   {
      printf("%s: metadata samples %u, stream frames %d\n", path,
            (unsigned)samples, frames);
      goto done;
   }
   if (rmp4_video_stream_duration_ns(stream) != rmp4_duration_ns(demux)) goto done;
   if (metadata_only)
   {
      printf("complete recording timeline: frames=%d duration=%.3f seconds\n",
            frames, (double)rmp4_video_stream_duration_ns(stream) / 1000000000.0);
      if (metadata_only == 2)
      {
         int64_t targets[3];
         int probe_index;
         targets[0] = rmp4_video_stream_duration_ns(stream) * 3 / 4;
         targets[1] = rmp4_video_stream_duration_ns(stream) / 4;
         targets[2] = 1000000000;
         for (probe_index = 0; probe_index < 3; probe_index++)
         {
            int state = 0, steps = 0;
            int64_t position = -1;
            if (!rmp4_video_stream_seek_begin_ns(stream, targets[probe_index])) goto done;
            while (!state && steps++ < 10000)
               state = rmp4_video_stream_seek_step(stream, &position);
            if (state != 1 || position < 0
                  || position > targets[probe_index] / 1000
                  || targets[probe_index] / 1000 - position > 100000) goto done;
            if (!rmp4_video_stream_next(stream, &duration)) goto done;
            printf("recording seek: target=%.3f actual=%.3f PASS\n",
                  (double)targets[probe_index] / 1000000000.0,
                  (double)position / 1000000.0);
         }
      }
      failures = 0;
      goto done;
   }
   pixels = (size_t)width * height;
   if (pixels > (size_t)-1 / sizeof(*reference) / samples) goto done;
   reference = (uint32_t*)malloc(pixels * samples * sizeof(*reference));
   times = (int64_t*)malloc(samples * sizeof(*times));
   if (!reference || !times) goto done;
   memcpy(times, raw_times, samples * sizeof(*times));
   qsort(times, samples, sizeof(*times), compare_pts);
   for (n = 0; n < frames; n++)
   {
      picture = rmp4_video_stream_next(stream, &duration);
      if (!picture) goto done;
      memcpy(reference + n * pixels, picture, pixels * sizeof(*picture));
   }
   if (rmp4_video_stream_next(stream, &duration)) goto done;
   failures = 0;
   probes = frames > 1024 ? 64 : frames;
   for (probe = probes - 1; probe >= 0; probe--)
   {
      int steps = 0, state = 0;
      int64_t position = -1;
      n = probes == frames ? probe : (frames - 1) * probe / (probes - 1);
      /* A newer request replaces a partly prepared one. */
      if (!rmp4_video_stream_seek_begin_ns(stream, times[(n + 5) % frames]))
      { failures++; break; }
      if (rmp4_video_stream_seek_step(stream, &position) < 0)
      { failures++; break; }
      if (!rmp4_video_stream_seek_begin_ns(stream, times[n]))
      { failures++; break; }
      rmp4_video_stream_set_avail(stream, 0);
      while (!state && steps++ < 10000)
      {
         state = rmp4_video_stream_seek_step(stream, &position);
         if (rmp4_video_stream_waiting(stream)) break;
      }
      if (state < 0) { failures++; break; }
      rmp4_video_stream_set_avail(stream, size);
      while (!state && steps++ < 10000)
         state = rmp4_video_stream_seek_step(stream, &position);
      picture = state == 1 ? rmp4_video_stream_next(stream, &duration) : NULL;
      if (!picture || position != times[n] / 1000
            || rmp4_video_stream_frame_index(stream) != n + 1
            || memcmp(picture, reference + n * pixels,
               pixels * sizeof(*picture)))
      {
         printf("%s: target %d state %d steps %d index %d mismatch\n",
               path, n, state, steps, rmp4_video_stream_frame_index(stream));
         failures++;
      }
      position = (times[n] - times[0]
            + rmp4_video_stream_presentation_delay_ns(stream)) / 1000000;
      if (rmp4_video_stream_seek_movie_ms(stream, position) != position)
         failures++;
      picture = rmp4_video_stream_next(stream, &duration);
      if (!picture || rmp4_video_stream_frame_index(stream) != n + 1
            || memcmp(picture, reference + n * pixels,
               pixels * sizeof(*picture)))
      {
         printf("%s: movie target %d mismatch\n", path, n);
         failures++;
      }
   }
   if (rmp4_video_stream_seek_movie_ms(stream, -1) != 0
         || rmp4_video_stream_frame_index(stream) != 0)
      failures++;
   {
      int64_t tail = (times[frames - 1] - times[0]
            + rmp4_video_stream_presentation_delay_ns(stream)) / 1000000 + 1000;
      if (rmp4_video_stream_seek_movie_ms(stream, tail) != tail
            || rmp4_video_stream_frame_index(stream) != frames - 1)
         failures++;
   }
   if (rmp4_video_stream_seek_movie_ms(stream, INT64_MAX) < 0
         || rmp4_video_stream_frame_index(stream) != frames - 1)
      failures++;
   if (rmp4_video_stream_seek_begin_ns(stream, 0))
   {
      rmp4_video_stream_rewind(stream);
      if (rmp4_video_stream_seek_step(stream, NULL) != -1) failures++;
   }
   else failures++;
   printf("%s: %d sequential frames, %d reverse seeks: %s\n",
         path, frames, probes, failures ? "FAIL" : "PASS");
done:
   if (file) fclose(file);
   if (stream) rmp4_video_stream_close(stream);
   if (demux) rmp4_close(demux);
   free(times); free(reference); free(data);
   if (failures) printf("%s: %d seek failure(s)\n", path, failures);
   return failures != 0;
}

int main(int argc, char **argv)
{
   int i, first = 1, failed = argc < 2;
   if (argc > 2 && !strcmp(argv[1], "--metadata"))
   { metadata_only = 1; first = 2; }
   if (argc > 2 && !strcmp(argv[1], "--probe"))
   { metadata_only = 2; first = 2; }
   if (rmp4_video_stream_seek_movie_ms(NULL, 0) != -1) failed = 1;
   for (i = first; i < argc; i++) failed |= check_file(argv[i]);
   return failed;
}
