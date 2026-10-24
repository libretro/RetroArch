/* Compare bounded WebM seeks with independent sequential decode. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rwebm.h>
#include <formats/rwebm_video.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", path, __LINE__, #x); goto done; } } while (0)
static int check_file(const char *path)
{
   FILE *f = NULL;
   uint8_t *data = NULL;
   uint32_t *reference = NULL;
   int64_t times[256], position;
   const uint32_t *picture;
   rwebm_t *demux = NULL;
   rwebm_video_stream_t *stream = NULL;
   rwebm_packet packet;
   unsigned width = 0, height = 0;
   size_t pixels, size, second_end = 0;
   long length;
   int count = 0, hidden = 0, i, probe, ret, steps, duration, status = 1, track = -1;
   int forward_steps = 0;
   CHECK((f = fopen(path, "rb")) != NULL);
   CHECK(!fseek(f, 0, SEEK_END) && (length = ftell(f)) > 0 && !fseek(f, 0, SEEK_SET));
   size = (size_t)length;
   CHECK((data = (uint8_t*)malloc(size)) != NULL);
   CHECK(fread(data, 1, size, f) == size);
   fclose(f); f = NULL;
   CHECK((demux = rwebm_open_memory(data, size)) != NULL);
   for (i = 0; i < rwebm_num_tracks(demux); i++)
      if (rwebm_get_track(demux, i)->type == RWEBM_TRACK_VIDEO) { track = i; break; }
   CHECK(track >= 0);
   while (rwebm_read_packet(demux, &packet) == 1)
      if (packet.track == track)
      {
         if (rwebm_get_track(demux, track)->codec == RWEBM_CODEC_VP8
               && packet.size && !(packet.data[0] & 16))
         {
            hidden++;
            continue;
         }
         CHECK(count < 256);
         if (count == 1) second_end = (size_t)(packet.data - data) + packet.size;
         times[count++] = packet.timestamp;
      }
   CHECK(count > 2);
   {
      size_t before = rwebm_consumed(demux);
      int64_t cue = rwebm_cue_time_ns(demux, times[count / 2]);
      CHECK(rwebm_consumed(demux) == before);
      rwebm_set_avail(demux, 0);
      CHECK(rwebm_cue_time_ns(demux, times[count / 2]) == cue);
      CHECK(rwebm_consumed(demux) == before);
      rwebm_set_avail(demux, size);
   }
   {
      int cues = rwebm_num_cues(demux);
      rwebm_t *cursor = rwebm_cursor_clone(demux);
      CHECK(cursor && !rwebm_num_cues(cursor));
      rwebm_cursor_copy(demux, cursor);
      CHECK(rwebm_num_cues(demux) == cues);
      rwebm_close(cursor);
   }

   CHECK((stream = rwebm_video_stream_open(data, size)) != NULL);
   rwebm_video_stream_get_info(stream, &width, &height, NULL, NULL);
   CHECK(width && height && width <= 512 && height <= 512);
   pixels = (size_t)width * height;
   CHECK(pixels <= (64u * 1024 * 1024) / sizeof(uint32_t) / (unsigned)count);
   CHECK((reference = (uint32_t*)malloc(pixels * count * sizeof(uint32_t))) != NULL);
   for (i = 0; i < count; i++)
   {
      CHECK((picture = rwebm_video_stream_next(stream, &duration)) != NULL);
      memcpy(reference + i * pixels, picture, pixels * sizeof(uint32_t));
   }
   for (probe = 0; probe < count * 3; probe++)
   {
      i = (probe * 17) % count;
      /* Replace an unfinished request, then withdraw all media bytes.
       * Metadata selection must still succeed and report a feed frontier. */
      CHECK(rwebm_video_stream_seek_begin_ns(stream, times[count - 1]));
      rwebm_video_stream_set_avail(stream, 0);
      CHECK(rwebm_video_stream_seek_begin_ns(stream, times[i]));
      ret = rwebm_video_stream_seek_step(stream, &position);
      CHECK(ret == 0 && rwebm_video_stream_waiting(stream));
      CHECK(rwebm_video_stream_consumed(stream) < size);
      rwebm_video_stream_set_avail(stream, size);
      for (steps = 0; steps < count + 100 && ret == 0; steps++)
         ret = rwebm_video_stream_seek_step(stream, &position);
      CHECK(ret == 1 && position == times[i] / 1000);
      CHECK(rwebm_video_stream_seek_step(stream, &position) == 1);
      CHECK((picture = rwebm_video_stream_next(stream, &duration)) != NULL);
      CHECK(!memcmp(reference + i * pixels, picture, pixels * sizeof(uint32_t)));
   }
   rwebm_video_stream_rewind(stream);
   CHECK(rwebm_video_stream_next(stream, NULL));
   for (i = 1; i < count; i++)
   {
      CHECK(rwebm_video_stream_seek_begin_ns(stream, times[i]));
      ret = 0;
      for (steps = 0; steps < count + 100 && ret == 0; steps++)
         ret = rwebm_video_stream_seek_step(stream, &position);
      CHECK(ret == 1 && position == times[i] / 1000 && (hidden || steps <= 2));
      forward_steps += steps;
      CHECK((picture = rwebm_video_stream_next(stream, &duration)) != NULL);
      CHECK(!memcmp(reference + i * pixels, picture, pixels * sizeof(uint32_t)));
   }
   CHECK(forward_steps <= count + hidden + 4);
   /* Cancel a duration lookahead that still owns an undecoded picture. */
   rwebm_video_stream_rewind(stream);
   CHECK(rwebm_video_stream_next(stream, NULL));
   rwebm_video_stream_set_avail(stream, second_end);
   picture = rwebm_video_stream_next(stream, NULL);
   CHECK(picture || rwebm_video_stream_waiting(stream));
   rwebm_video_stream_set_avail(stream, size);
   CHECK(rwebm_video_stream_seek_begin_ns(stream, times[2]));
   ret = 0;
   for (steps = 0; steps < count + 100 && ret == 0; steps++)
      ret = rwebm_video_stream_seek_step(stream, &position);
   CHECK(ret == 1 && position == times[2] / 1000);
   CHECK((picture = rwebm_video_stream_next(stream, NULL)) != NULL);
   CHECK(!memcmp(reference + 2 * pixels, picture, pixels * sizeof(uint32_t)));

   /* A replacement behind completed preroll must restart the decoder. */
   CHECK(rwebm_video_stream_seek_begin_ns(stream, times[count - 1]));
   for (steps = 0; steps < count / 2; steps++)
      if (rwebm_video_stream_seek_step(stream, &position) != 0) break;
   CHECK(rwebm_video_stream_seek_begin_ns(stream, times[1]));
   ret = 0;
   for (steps = 0; steps < count + 100 && ret == 0; steps++)
      ret = rwebm_video_stream_seek_step(stream, &position);
   CHECK(ret == 1 && position == times[1] / 1000);
   CHECK((picture = rwebm_video_stream_next(stream, NULL)) != NULL);
   CHECK(!memcmp(reference + pixels, picture, pixels * sizeof(uint32_t)));
   /* Retarget a held, undecoded picture without dropping its references. */
   rwebm_video_stream_rewind(stream);
   CHECK(rwebm_video_stream_next(stream, NULL));
   for (i = 2; i < count; i += 2)
   {
      CHECK(rwebm_video_stream_seek_begin_ns(stream, times[i - 1]));
      ret = 0;
      for (steps = 0; steps < count + 100 && ret == 0; steps++)
         ret = rwebm_video_stream_seek_step(stream, &position);
      CHECK(ret == 1);
      CHECK(rwebm_video_stream_seek_begin_ns(stream, times[i]));
      ret = 0;
      for (steps = 0; steps < count + 100 && ret == 0; steps++)
         ret = rwebm_video_stream_seek_step(stream, &position);
      CHECK(ret == 1 && position == times[i] / 1000 && (hidden || steps <= 3));
      CHECK((picture = rwebm_video_stream_next(stream, &duration)) != NULL);
      CHECK(!memcmp(reference + i * pixels, picture, pixels * sizeof(uint32_t)));
   }
   /* Sub-frame requests select the next display timestamp. */
   for (i = 0; i + 1 < count; i++)
   {
      CHECK(rwebm_video_stream_seek_begin_ns(stream, times[i] + 1));
      ret = 0;
      for (steps = 0; steps < count + 100 && ret == 0; steps++)
         ret = rwebm_video_stream_seek_step(stream, &position);
      CHECK(ret == 1 && position == times[i + 1] / 1000);
      CHECK((picture = rwebm_video_stream_next(stream, &duration)) != NULL);
      CHECK(!memcmp(reference + (i + 1) * pixels, picture, pixels * sizeof(uint32_t)));
   }
   CHECK(rwebm_video_stream_seek_begin_ns(stream, times[count - 1] + 1000000000));
   ret = 0;
   for (steps = 0; steps < count + 100 && ret == 0; steps++)
      ret = rwebm_video_stream_seek_step(stream, &position);
   CHECK(ret == 1 && position == times[count - 1] / 1000);
   CHECK((picture = rwebm_video_stream_next(stream, &duration)) != NULL);
   CHECK(!memcmp(reference + (count - 1) * pixels, picture, pixels * sizeof(uint32_t)));
   CHECK(rwebm_video_stream_seek_begin_ns(stream, times[count - 1]));
   rwebm_video_stream_rewind(stream);
   CHECK(rwebm_video_stream_seek_step(stream, &position) == -1);
   CHECK((picture = rwebm_video_stream_next(stream, &duration)) != NULL);
   CHECK(!memcmp(reference, picture, pixels * sizeof(uint32_t)));
   printf("%s: %d bounded seeks match sequential pixels\n", path, count * 3);
   status = 0;
done:
   if (f) fclose(f);
   rwebm_video_stream_close(stream); rwebm_close(demux);
   free(reference); free(data);
   return status;
}
int main(int argc, char **argv)
{
   int i;
   if (argc < 2) return 2;
   for (i = 1; i < argc; i++) if (check_file(argv[i])) return 1;
   return 0;
}
