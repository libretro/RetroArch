/* AV1 builtin core output through the native shared-frame reader. */
#include "../../../cores/libretro-media/media_core.c"

static struct
{
   video_hw_frame_t frame;
   unsigned index, stopped, failed;
} av1_core;

static bool av1_core_sink(void *user, const video_hw_frame_t *frame)
{
   (void)user;
   if (!frame) return true;
   if (av1_core.frame.image) return false;
   av1_core.frame = *frame;
   av1_core.index = (unsigned)media_player.vshown - 1;
   return true;
}

static bool av1_core_environment(unsigned cmd, void *data)
{
   if (cmd == RETRO_ENVIRONMENT_SET_PIXEL_FORMAT)
      return *(enum retro_pixel_format*)data == RETRO_PIXEL_FORMAT_XRGB8888;
   if (cmd == RETRO_ENVIRONMENT_SHUTDOWN)
   {
      av1_core.stopped = 1;
      return true;
   }
   return false;
}

static void av1_core_video(const void *data, unsigned w, unsigned h, size_t pitch)
{
   if (data || pitch || w != media_player.width || h != media_player.height)
      av1_core.failed = 1;
}

static void av1_core_poll(void) { }
static int16_t av1_core_input(unsigned port, unsigned device, unsigned index, unsigned id)
{
   (void)port; (void)device; (void)index; (void)id;
   return 0;
}

static size_t av1_core_audio(const int16_t *data, size_t count)
{
   (void)data;
   return count;
}

static int read_av1_core(const char *path, frames_t *hw, opened_t *opened, unsigned *bps)
{
   struct retro_game_info game;
   unsigned polls, last = UINT32_MAX;
   int ok = 0;
   memset(&av1_core, 0, sizeof(av1_core));
   memset(&game, 0, sizeof(game));
   game.path = path;
   retro_set_environment(av1_core_environment);
   retro_set_video_refresh(av1_core_video);
   retro_set_input_poll(av1_core_poll);
   retro_set_input_state(av1_core_input);
   retro_set_audio_sample_batch(av1_core_audio);
   if (!media_set_video_hw_refresh(av1_core_sink, NULL) || !retro_load_game(&game))
   { puts("FAIL AV1 core load"); goto done; }
   if ((!media_player.mp4_hw_required && !media_player.webm_hw_required) || media_player.fb)
   { puts("FAIL AV1 core hardware admission"); goto done; }
   for (polls = 0; polls < 8192 && !av1_core.stopped; polls++)
   {
      retro_run();
      if (av1_core.failed || media_player.fb)
      { puts("FAIL AV1 core video callback"); goto done; }
      if (av1_core.frame.image)
      {
         video_hw_frame_t frame = av1_core.frame;
         unsigned index = av1_core.index;
         memset(&av1_core.frame, 0, sizeof(av1_core.frame));
         if (!av1_color_matches(&frame)) { video_hw_frame_abandon(&frame); goto done; }
         *bps = frame.pixfmt == IMAGE_PIXFMT_P010 ? 2 : 1;
         if ((last != UINT32_MAX && index != last + 1) || (last == UINT32_MAX && index))
         {
            printf("FAIL AV1 core display order: previous=%u current=%u\n", last, index);
            video_hw_frame_abandon(&frame);
            goto done;
         }
         if (!read_av1_frame(hw, opened, &frame, index))
         {
            puts("FAIL AV1 core shared-frame read");
            video_hw_frame_abandon(&frame);
            goto done;
         }
         last = index;
      }
      else Sleep(1);
   }
   ok = av1_core.stopped && hw->n;
   printf("AV1 builtin core: %s (%u displayed frames)\n", ok ? "PASS" : "FAIL", (unsigned)hw->n);
done:
   if (av1_core.frame.image) video_hw_frame_abandon(&av1_core.frame);
   retro_unload_game();
   media_set_video_hw_refresh(NULL, NULL);
   return ok;
}
