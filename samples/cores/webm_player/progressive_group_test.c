/* Real core, callbacks and private prefix-state inspection. */
#define main original_host_main
#define log_cb host_log_cb
#include "webm_player_test.c"
#undef log_cb
#undef main
#include "../../../cores/libretro-media/media_core.c"

static int group_run(const char *path, uint32_t *pictures, unsigned *count,
      uint64_t *pcm_count, uint32_t *pcm_hash, int compare)
{
   struct retro_game_info game;
   unsigned ticks, accepted = 0, demand_polls = 0, over_lookahead = 0;
   int64_t shown = 0;
   size_t initial, peak_avail;
   current = path;
   clear_capture();
   memset(&game, 0, sizeof(game)); game.path = path;
   if (!retro_load_game(&game)) return 1;
   initial = peak_avail = data_transfer_avail(media_player.dt);
   if (compare) check(!data_transfer_complete(media_player.dt), "group was loaded in full before playback");
   for (ticks = 0; ticks < 2000 && !shutdowns; ticks++)
   {
      size_t avail = data_transfer_avail(media_player.dt);
      size_t needed = media_native_read_end(&media_player);
      size_t frontier = media_player.webmvs
         ? rwebm_video_stream_feed_frontier(media_player.webmvs)
         : rwebm_tell(media_player.webm);
      if (needed > avail)
      {
         demand_polls++;
         if (avail >= frontier && avail - frontier >= MEDIA_FILL_LOOKAHEAD) over_lookahead++;
      }
      retro_run();
      avail = data_transfer_avail(media_player.dt);
      if (avail > peak_avail) peak_avail = avail;
      if (media_player.vshown != shown)
      {
         check(media_player.vshown == shown + 1, "presentation skipped an indexed picture");
         if (accepted >= 4096) break;
         if (compare) check(accepted < *count && pictures[accepted] == frame_hash,
               "progressive group changed decoded picture");
         else pictures[accepted] = frame_hash;
         accepted++;
         shown = media_player.vshown;
      }
   }
   check(shutdowns != 0, "progressive group did not reach EOS");
   if (compare)
   {
      check(accepted == *count, "group changed picture count");
      check(samples == *pcm_count && audio_hash == *pcm_hash, "group changed PCM length or samples");
      check(demand_polls && over_lookahead, "fixture did not require fill beyond ordinary lookahead");
   }
   else { *count = accepted; *pcm_count = samples; *pcm_hash = audio_hash; }
   printf("core group: compare=%d pictures=%u polls=%u initial=%lu final_prefix=%lu demand=%u beyond_lookahead=%u samples=%lu\n",
         compare, accepted, ticks, (unsigned long)initial, (unsigned long)peak_avail,
         demand_polls, over_lookahead, (unsigned long)samples);
   retro_unload_game();
   return 0;
}

int main(int argc, char **argv)
{
   uint32_t *pictures = (uint32_t*)calloc(4096, sizeof(uint32_t));
   unsigned count = 0;
   uint64_t pcm_count = 0;
   uint32_t pcm_hash = 0;
   int result;
   if (argc != 3 || !pictures) { free(pictures); return 1; }
   retro_set_environment(environment); retro_set_video_refresh(video);
   retro_set_audio_sample(audio_sample); retro_set_audio_sample_batch(audio);
   retro_set_input_poll(input_poll); retro_set_input_state(input_state);
   retro_init();
   result = group_run(argv[1], pictures, &count, &pcm_count, &pcm_hash, 0);
   if (!result) result = group_run(argv[2], pictures, &count, &pcm_count, &pcm_hash, 1);
   retro_deinit(); free(pictures);
   return result || failures;
}
