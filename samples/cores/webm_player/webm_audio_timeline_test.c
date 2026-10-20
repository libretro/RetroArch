/* Real core: audio must retain its container presentation offset. */
#include <stdlib.h>
static unsigned timeline_fail_allocation, timeline_allocation_count;
static void *timeline_malloc(size_t size)
{
   if (timeline_fail_allocation && ++timeline_allocation_count == timeline_fail_allocation)
      return NULL;
   return malloc(size);
}
#define malloc timeline_malloc
#include "../../../cores/libretro-media/media_core.c"
#undef malloc
static size_t timeline_samples, first_signal = (size_t)-1;
static unsigned timeline_shutdowns;
static const char *timeline_path;
static size_t gap_lo, gap_hi, gap_signal, gap_observed;
static uint32_t timeline_hash = 2166136261u;
static int16_t reference_head[8192], actual_head[8192];
static size_t reference_head_count, actual_head_count;
static bool timeline_environment(unsigned cmd, void *data)
{
   if (cmd == RETRO_ENVIRONMENT_SET_PIXEL_FORMAT)
      return *(enum retro_pixel_format*)data == RETRO_PIXEL_FORMAT_XRGB8888;
   if (cmd == RETRO_ENVIRONMENT_SHUTDOWN) { timeline_shutdowns++; return true; }
   return false;
}
static void timeline_video(const void *data, unsigned w, unsigned h, size_t pitch)
{ (void)data; (void)w; (void)h; (void)pitch; }
static size_t timeline_audio(const int16_t *data, size_t count)
{
   size_t i;
   for (i = 0; i < count; i++)
   {
      if (actual_head_count < 4096)
      {
         actual_head[actual_head_count*2] = data[2*i];
         actual_head[actual_head_count*2+1] = data[2*i+1];
         actual_head_count++;
      }
      timeline_hash = (timeline_hash ^ (uint16_t)data[2*i]) * 16777619u;
      timeline_hash = (timeline_hash ^ (uint16_t)data[2*i+1]) * 16777619u;
      if (gap_hi && timeline_samples + i >= gap_lo
            && timeline_samples + i < gap_hi)
      {
         gap_observed++;
         if (data[2*i] > 64 || data[2*i] < -64
               || data[2*i+1] > 64 || data[2*i+1] < -64) gap_signal++;
      }
      if (first_signal == (size_t)-1 && (data[2*i] > 64 || data[2*i] < -64
               || data[2*i+1] > 64 || data[2*i+1] < -64))
         first_signal = timeline_samples + i;
   }
   timeline_samples += count;
   return count;
}
static void timeline_poll(void) { }
static int16_t timeline_input(unsigned p, unsigned d, unsigned i, unsigned id)
{ (void)p; (void)d; (void)i; (void)id; return 0; }
/* Failure during metadata growth must retain both old tables and stop
 * further decoding, rather than leave a borrowed freed pointer behind. */
static int timeline_allocation_failure(void)
{
   media_player_t p;
   uint32_t *sizes;
   struct media_audio_time *times;
   int16_t output[2];
   size_t got = 99;
   int failed = 0;
   memset(&p, 0, sizeof(p));
   sizes = (uint32_t*)malloc(sizeof(*sizes));
   times = (struct media_audio_time*)malloc(sizeof(*times));
   if (!sizes || !times) { free(sizes); free(times); return 1; }
   *sizes = 7; memset(times, 0, sizeof(*times)); times->pts = 123;
   p.asizes = sizes; p.atimes = times; p.anpkts = p.anpkts_cap = 1;
   timeline_fail_allocation = 2; timeline_allocation_count = 0;
   if (media_audio_packet_reserve(&p) || !p.atime_failed || p.asizes != sizes
         || p.atimes != times || *sizes != 7 || times->pts != 123) failed = 1;
   timeline_fail_allocation = 0;
   if (media_audio_read(&p, output, 1, &got) != AUDIO_PROCESS_ERROR_END || got) failed = 1;
   free(sizes); free(times);
   printf("Metadata allocation failure: retained tables and stopped reader %s\n", failed ? "FAIL" : "PASS");
   return failed;
}

/* Measure against an independently started decoder. Header queries must
 * agree with actual PCM length and leave mid-stream reads unchanged. */
static int timeline_packet_measure(void)
{
   media_player_t *p = &media_player;
   const void *setup;
   size_t setup_size, i, offset = 0;
   uint64_t predicted = 0, reference = 0;
   uint32_t reference_hash = 0;
   unsigned pass;
   uint64_t source_trim = 0;
   if (getenv("EXPECT_START_TRIM"))
   {
      char path[1024];
      unsigned long skip;
      FILE *f;
      int n = snprintf(path, sizeof(path), "%s.slice", timeline_path);
      /* The sidecar records edit-free decoder sample coordinates. */
      f = n > 0 && (size_t)n < sizeof(path) ? fopen(path, "r") : NULL;
      if (!f) return 1;
      if (fscanf(f, "%lu", &skip) != 1) { fclose(f); return 1; }
      fclose(f); source_trim = (uint64_t)skip;
   }
   if (!p->actx || !data_transfer_complete(p->dt)) return 1;
   if (p->wgather)
   {
      const rwebm_track *track = rwebm_get_track(p->wgather, p->atrack);
      setup = track->codec_private; setup_size = track->codec_private_size;
   }
#ifdef HAVE_RMP4
   else if (p->agather)
   {
      const rmp4_track *track = rmp4_get_track(p->agather, p->agtrack);
      setup = track->codec_private; setup_size = track->codec_private_size;
   }
#endif
   else return 1;
   for (pass = 0; pass < 2; pass++)
   {
      void *decoder = audio_transfer_new(p->atype);
      uint64_t count = 0;
      uint32_t hash = 2166136261u;
      int16_t pcm[512];
      int status = AUDIO_PROCESS_NEXT, queried = 0;
      if (!decoder || !audio_transfer_set_demuxed_ptr(decoder, p->atype,
               setup, setup_size, p->apkts, p->apkts_used, p->asizes, p->anpkts)
            || !audio_transfer_start(decoder, p->atype))
      {
         if (decoder) audio_transfer_free(decoder, p->atype);
         return 1;
      }
      while (status == AUDIO_PROCESS_NEXT)
      {
         size_t got = 0;
         status = audio_transfer_read_s16(decoder, p->atype, pcm, 256, &got);
         for (i = 0; i < got * p->ach; i++) hash = (hash ^ (uint16_t)pcm[i]) * 16777619u;
         if (!pass && getenv("EXPECT_START_TRIM"))
         {
            uint64_t trim = source_trim;
            for (i = 0; i < got; i++)
               if (count + i >= trim && reference_head_count < 4096)
               {
                  reference_head[reference_head_count*2] = pcm[i*p->ach];
                  reference_head[reference_head_count*2+1] = pcm[i*p->ach+(p->ach>1)];
                  reference_head_count++;
               }
         }
         count += got;
         if (pass && !queried)
         {
            offset = 0;
            for (i = 0; i < p->anpkts; i++)
            {
               int frames = audio_transfer_demuxed_packet_frames(decoder, p->atype,
                     p->apkts + offset, p->asizes[i], i);
               if (frames < 0) { audio_transfer_free(decoder, p->atype); return 1; }
               predicted += (unsigned)frames;
               offset += p->asizes[i];
            }
            queried = 1;
         }
         if (!got && status == AUDIO_PROCESS_NEXT)
         { audio_transfer_free(decoder, p->atype); return 1; }
      }
      audio_transfer_free(decoder, p->atype);
      if (status != AUDIO_PROCESS_END) return 1;
      if (!pass) { reference = count; reference_hash = hash; }
      else if (reference != count || reference_hash != hash) return 1;
   }
   if (p->atype == AUDIO_TYPE_OPUS)
   {
      if (predicted < (uint64_t)p->apreskip) return 1;
      predicted -= (uint64_t)p->apreskip;
   }
   printf("Packet sample measure: predicted=%lu decoded=%lu; mid-read query PCM unchanged\n",
         (unsigned long)predicted, (unsigned long)reference);
   return predicted != reference;
}

int main(int argc, char **argv)
{
   struct retro_game_info game;
   unsigned i;
   size_t expected;
   int failed;
   if (argc != 3 && argc != 5) return 2;
   if (argc == 5)
   {
      gap_lo = (size_t)strtoul(argv[3], NULL, 10);
      gap_hi = (size_t)strtoul(argv[4], NULL, 10);
      if (gap_hi <= gap_lo) return 2;
   }
   timeline_path = argv[1];
   expected = (size_t)strtoul(argv[2], NULL, 10);
   memset(&game, 0, sizeof(game)); game.path = argv[1];
   retro_set_environment(timeline_environment); retro_set_video_refresh(timeline_video);
   retro_set_audio_sample_batch(timeline_audio); retro_set_input_poll(timeline_poll);
   retro_set_input_state(timeline_input); retro_init();
   if (!retro_load_game(&game)) { retro_deinit(); return 2; }
   failed = timeline_allocation_failure();
   /* Force a real table replacement while a decoder borrows its sizes. */
   media_player.anpkts_cap = media_player.anpkts;
   if (!media_audio_packet_reserve(&media_player)) failed = 1;
   failed |= timeline_packet_measure();
   for (i = 0; i < 256 && !timeline_shutdowns; i++) retro_run();
   printf("Audio presentation: first_signal=%lu expected=%lu frames=%lu EOS=%u\n",
         (unsigned long)first_signal, (unsigned long)expected,
         (unsigned long)timeline_samples, timeline_shutdowns);
   failed |= !timeline_shutdowns || first_signal == (size_t)-1
      || first_signal + 2048 < expected || first_signal > expected + 2048;
   if (getenv("EXPECT_START_TRIM"))
   {
      int match = reference_head_count == 4096 && actual_head_count == 4096
         && !memcmp(reference_head, actual_head, sizeof(reference_head));
      printf("Edit-list trim: 4096 PCM frames match independent source slice %s\n", match ? "PASS" : "FAIL");
      if (!match) failed = 1;
   }
   if (getenv("EXPECT_OVERLAP_FRAMES"))
   {
      int64_t overlap = (int64_t)strtol(getenv("EXPECT_OVERLAP_FRAMES"), NULL, 10);
      int64_t removed = media_player.atotal - media_player.apos;
      printf("Audio overlap: removed=%ld expected=%ld\n", (long)removed, (long)overlap);
      if (removed < overlap - 2048 || removed > overlap + 2048) failed = 1;
   }
   if (gap_hi)
   {
      printf("Audio gap: observed=%lu expected=%lu signal=%lu\n",
            (unsigned long)gap_observed, (unsigned long)(gap_hi-gap_lo),
            (unsigned long)gap_signal);
      if (gap_observed != gap_hi-gap_lo || gap_signal) failed = 1;
   }
   {
      char reference_path[1024];
      unsigned long reference_onset = 0;
      FILE *reference;
      int n = snprintf(reference_path, sizeof(reference_path), "%s.onset", argv[1]);
      reference = n > 0 && (size_t)n < sizeof(reference_path)
         ? fopen(reference_path, "r") : NULL;
      if (!reference || fscanf(reference, "%lu", &reference_onset) != 1) failed = 1;
      else
      {
         if (first_signal + 2 < (size_t)reference_onset
               || first_signal > (size_t)reference_onset + 2) failed = 1;
         printf("Independent timestamp-preserving decoder onset: actual=%lu reference=%lu\n",
               (unsigned long)first_signal, reference_onset);
      }
      if (reference) fclose(reference);
      if (getenv("EXPECT_START_TRIM"))
      {
         unsigned peak = 0;
         uint64_t error = 0;
         n = snprintf(reference_path, sizeof(reference_path), "%s.head", argv[1]);
         reference = n > 0 && (size_t)n < sizeof(reference_path) ? fopen(reference_path, "rb") : NULL;
         if (!reference) failed = 1;
         else
         {
            size_t j;
            for (j = 0; j < 8192; j++)
            {
               int lo = fgetc(reference), hi = fgetc(reference), delta;
               if (lo == EOF || hi == EOF) { failed = 1; break; }
               delta = (int)actual_head[j] - (int)(int16_t)((unsigned)lo | ((unsigned)hi << 8));
               if (delta < 0) delta = -delta;
               if ((unsigned)delta > peak) peak = (unsigned)delta;
               error += (uint64_t)delta * (unsigned)delta;
            }
            fclose(reference);
            printf("Independent trimmed PCM: peak=%u MSE=%lu\n", peak, (unsigned long)(error/8192));
            if (peak > 16) failed = 1;
         }
      }
   }
   {
      size_t original_first = first_signal, original_samples = timeline_samples;
      uint32_t original_hash = timeline_hash;
      if (gap_hi)
      {
         int16_t pcm[512];
         size_t got = 0, j;
         int64_t middle = (int64_t)(gap_lo + (gap_hi-gap_lo)/2);
         if (!media_seek_audio(&media_player, middle)
               || media_audio_read(&media_player, pcm, 256, &got) != AUDIO_PROCESS_NEXT
               || got != 256) failed = 1;
         for (j = 0; j < got * media_player.ach; j++) if (pcm[j]) failed = 1;
         printf("Seek into timestamp gap: frames=%lu silent=%s\n",
               (unsigned long)got, failed ? "FAIL" : "PASS");
      }
      retro_reset();
      timeline_samples = 0; first_signal = (size_t)-1; timeline_shutdowns = 0;
      gap_signal = gap_observed = 0; timeline_hash = 2166136261u;
      for (i = 0; i < 256 && !timeline_shutdowns; i++) retro_run();
      if (!timeline_shutdowns || original_first != first_signal
            || original_samples != timeline_samples || original_hash != timeline_hash)
         failed = 1;
      printf("Timeline reset replay: first=%lu frames=%lu PCM=%s\n",
            (unsigned long)first_signal, (unsigned long)timeline_samples,
            original_hash == timeline_hash ? "identical" : "FAIL");
   }
   {
      uint8_t state[40];
      size_t saved_count;
      uint32_t saved_hash;
      retro_reset(); timeline_shutdowns = 0;
      for (i = 0; i < 18; i++) retro_run();
      if (retro_serialize_size() > sizeof(state) || !retro_serialize(state, sizeof(state)))
         failed = 1;
      else
      {
         timeline_samples = 0; timeline_hash = 2166136261u;
         retro_run(); retro_run();
         saved_count = timeline_samples; saved_hash = timeline_hash;
         if (!retro_unserialize(state, sizeof(state))) failed = 1;
         timeline_samples = 0; timeline_hash = 2166136261u;
         retro_run(); retro_run();
         if (saved_count != timeline_samples || saved_hash != timeline_hash) failed = 1;
         printf("Timeline state restore: frames=%lu/%lu PCM=%s\n",
               (unsigned long)saved_count, (unsigned long)timeline_samples,
               saved_hash == timeline_hash ? "identical" : "FAIL");
      }
   }
   retro_unload_game(); retro_deinit();
   return failed;
}
