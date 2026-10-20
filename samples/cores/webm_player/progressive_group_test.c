/* Real core, callbacks and private prefix-state inspection. */
#define main original_host_main
#define log_cb host_log_cb
#include "webm_player_test.c"
#undef log_cb
#undef main
#include "../../../cores/libretro-media/media_core.c"
#if defined(__linux__) && defined(HAVE_MMAP)
#include <sys/mman.h>
#include <unistd.h>
#endif
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

typedef struct prefix_probe
{
   unsigned char *pages;
   size_t count, page_size;
   int supported;
} prefix_probe_t;

static size_t prefix_resident(prefix_probe_t *probe)
{
   size_t resident = 0, i = 0;
#if defined(__linux__) && defined(HAVE_MMAP)
   if (!probe->pages) return 0;
   if (mincore(media_player.file_buf, probe->count * probe->page_size, probe->pages))
   { check(0, "cannot inspect encoded mapping residency"); return 0; }
   for (i = 0; i < probe->count; i++) if (probe->pages[i] & 1) resident += probe->page_size;
#elif defined(_WIN32)
   uintptr_t at = (uintptr_t)media_player.file_buf;
   uintptr_t end = at + (size_t)media_player.file_len;
   while (at < end)
   {
      MEMORY_BASIC_INFORMATION info;
      uintptr_t next;
      if (VirtualQuery((void*)at, &info, sizeof(info)) != sizeof(info))
      { check(0, "cannot inspect encoded Windows mapping"); return 0; }
      next = (uintptr_t)info.BaseAddress + info.RegionSize;
      if (next <= at) { check(0, "invalid Windows mapping extent"); return 0; }
      if (next > end) next = end;
      if (info.State == MEM_COMMIT) resident += next - at;
      at = next;
   }
   (void)probe; (void)i;
#else
   (void)probe; (void)i;
#endif
   return resident;
}

static int group_run(const char *path, uint32_t *pictures, unsigned *count,
      uint64_t *pcm_count, uint32_t *pcm_hash, int compare)
{
   struct retro_game_info game;
   unsigned ticks, accepted = 0, demand_polls = 0, over_lookahead = 0;
   int64_t shown = 0;
   size_t initial, peak_avail;
   size_t peak_resident = 0, resident_bound;
   prefix_probe_t probe;
   bool sparse = getenv("CORE_SPARSE") != NULL;
   memset(&probe, 0, sizeof(probe));
   current = path;
   clear_capture();
   memset(&game, 0, sizeof(game)); game.path = path;
   if (!retro_load_game(&game)) return 1;
   initial = peak_avail = data_transfer_avail(media_player.dt);
#if defined(__linux__) && defined(HAVE_MMAP)
   probe.page_size = (size_t)sysconf(_SC_PAGESIZE);
   check(probe.page_size && (uintptr_t)media_player.file_buf % probe.page_size == 0,
         "prefix probe requires a page-aligned encoded reservation");
   if (probe.page_size && (uintptr_t)media_player.file_buf % probe.page_size == 0)
   {
      probe.count = ((size_t)media_player.file_len + probe.page_size - 1) / probe.page_size;
      probe.pages = (unsigned char*)calloc(probe.count, 1);
      check(probe.pages != NULL, "cannot allocate residency probe");
      probe.supported = probe.pages != NULL;
      peak_resident = prefix_resident(&probe);
   }
#elif defined(_WIN32)
   probe.supported = data_transfer_reserve_supported();
   if (probe.supported) peak_resident = prefix_resident(&probe);
#endif
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
      {
         size_t resident = prefix_resident(&probe);
         if (resident > peak_resident) peak_resident = resident;
      }
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
      if (!sparse) check(demand_polls && over_lookahead, "fixture did not require fill beyond ordinary lookahead");
   }
   else { *count = accepted; *pcm_count = samples; *pcm_hash = audio_hash; }
   printf("core group: compare=%d pictures=%u polls=%u initial=%lu final_prefix=%lu demand=%u beyond_lookahead=%u samples=%lu\n",
         compare, accepted, ticks, (unsigned long)initial, (unsigned long)peak_avail,
         demand_polls, over_lookahead, (unsigned long)samples);
   if (compare)
   {
      unsigned replay = 0;
      retro_reset();
      clear_capture(); shown = 0;
      for (ticks = 0; ticks < 2000 && !shutdowns; ticks++)
      {
         size_t resident;
         retro_run();
         resident = prefix_resident(&probe);
         if (resident > peak_resident) peak_resident = resident;
         if (media_player.vshown != shown)
         {
            check(replay < *count && pictures[replay] == frame_hash,
                  "backward replay changed a decoded picture");
            replay++; shown = media_player.vshown;
         }
      }
      check(shutdowns && replay == *count, "backward replay did not reach original EOS");
      check(samples == *pcm_count && audio_hash == *pcm_hash,
            "backward replay changed PCM length or samples");
      printf("core backward replay: pictures=%u samples=%lu\n", replay, (unsigned long)samples);
   }
   resident_bound = (sparse ? MEDIA_FILL_LOOKAHEAD : 24 * 1024 * 1024)
      + MEDIA_DISCARD_MARGIN + 4 * 1024 * 1024 + 128 * 1024;
   if (probe.supported)
   {
#ifdef _WIN32
      printf("core encoded mapping: committed_peak=%lu bound=%lu sparse=%u\n",
#else
      printf("core encoded mapping: resident_peak=%lu bound=%lu sparse=%u\n",
#endif
            (unsigned long)peak_resident, (unsigned long)resident_bound, sparse);
      check(peak_resident <= resident_bound, "encoded mapping exceeds residency budget");
   }
   else printf("core encoded mapping: physical residency probe unavailable on this platform\n");
   free(probe.pages);
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
