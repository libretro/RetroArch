/* Full packet timeline oracle, including timestamps beyond the prefix. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rwebm.h>
#include <formats/rwebm_video.h>
int main(int argc, char **argv)
{
   FILE *f = NULL;
   uint8_t *bytes = NULL;
   int64_t *times = NULL;
   size_t *ends = NULL;
   rwebm_t *demux = NULL;
   rwebm_video_stream_t *stream = NULL;
   rwebm_packet packet;
   const uint32_t *picture;
   uint32_t *last = NULL;
   unsigned width = 0, height = 0;
   int ret, steps;
   int64_t position;
   size_t size;
   long length;
   int track = -1, count = 0, i, duration, mismatch = 0, status = 1;
   int64_t elapsed = 0, expected;
   if (argc != 2 || !(f = fopen(argv[1], "rb"))) goto done;
   if (fseek(f, 0, SEEK_END) || (length = ftell(f)) <= 0 || fseek(f, 0, SEEK_SET)) goto done;
   size = (size_t)length;
   if (!(bytes = (uint8_t*)malloc(size)) || fread(bytes, 1, size, f) != size) goto done;
   if (!(demux = rwebm_open_memory(bytes, size))) goto done;
   for (i = 0; i < rwebm_num_tracks(demux); i++)
      if (rwebm_get_track(demux, i)->type == RWEBM_TRACK_VIDEO) { track = i; break; }
   if (track < 0 || !(times = (int64_t*)malloc(10000 * sizeof(*times)))) goto done;
   if (!(ends = (size_t*)malloc(10000 * sizeof(*ends)))) goto done;
   while (rwebm_read_packet(demux, &packet) == 1)
      if (packet.track == track)
      {
         if (count == 10000) goto done;
         ends[count] = (size_t)(packet.data - bytes) + packet.size;
         times[count++] = packet.timestamp;
      }
   if (count <= 8192 || !(stream = rwebm_video_stream_open(bytes, size))) goto done;
   for (i = 0; i + 1 < count; i++)
   {
      if (i == 8191 || i == 8390)
      {
         rwebm_video_stream_set_avail(stream, ends[i]);
         if (rwebm_video_stream_next(stream, &duration)
               || !rwebm_video_stream_waiting(stream)
               || rwebm_video_stream_consumed(stream) > ends[i]) goto done;
         rwebm_video_stream_set_avail(stream, size);
      }
      if (!rwebm_video_stream_next(stream, &duration)) goto done;
      expected = times[i + 1] / 1000000 - times[i] / 1000000;
      if (duration != expected) mismatch++;
      elapsed += duration;
   }
   rwebm_video_stream_get_info(stream, &width, &height, NULL, NULL);
   if (!width || !height || width > 256 || height > 256) goto done;
   picture = rwebm_video_stream_next(stream, &duration);
   if (!picture || !(last = (uint32_t*)malloc((size_t)width * height * sizeof(*last)))) goto done;
   memcpy(last, picture, (size_t)width * height * sizeof(*last));
   /* The prefix did not establish EOF. Discover the endpoint through
    * bounded steps, including replacement of an unfinished discovery. */
   if (!rwebm_video_stream_seek_begin_ns(stream, times[count - 1] + 1000000000)) goto done;
   if (rwebm_video_stream_seek_step(stream, &position) != 0) goto done;
   rwebm_video_stream_set_avail(stream, ends[8192]);
   if (!rwebm_video_stream_seek_begin_ns(stream, times[count - 1] + 2000000000)) goto done;
   ret = 0;
   for (steps = 0; steps < count + 100 && ret == 0; steps++)
   {
      ret = rwebm_video_stream_seek_step(stream, &position);
      if (rwebm_video_stream_waiting(stream)) rwebm_video_stream_set_avail(stream, size);
   }
   if (ret != 1 || position != times[count - 1] / 1000) goto done;
   picture = rwebm_video_stream_next(stream, &duration);
   if (!picture || memcmp(last, picture, (size_t)width * height * sizeof(*last))) goto done;
   expected = times[count - 1] / 1000000 - times[0] / 1000000;
   printf("WebM timeline: packets=%d differing_intervals=%d elapsed_ms=%ld expected_ms=%ld\n",
         count, mismatch, (long)elapsed, (long)expected);
   status = mismatch || elapsed != expected;
done:
   if (f) fclose(f);
   rwebm_video_stream_close(stream); rwebm_close(demux);
   free(last); free(ends); free(times); free(bytes); return status;
}
