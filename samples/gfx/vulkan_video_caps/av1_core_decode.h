/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "../../../cores/libretro-media/media_core.c"

/* Expectations come from the independent fixture-generation command. */
static int av1_static_hdr_matches(const video_hw_frame_t *frame)
{
   static const uint16_t xy[8] = {35400,14600,8500,39850,6550,2300,15635,16450};
   if (!getenv("AV1_TEST_STATIC_HDR")) return 1;
   return frame->color.max_cll == 1000 && frame->color.max_fall == 400 &&
      frame->color.mastering.present == VIDEO_MASTERING_DISPLAY_COMPLETE &&
      frame->color.mastering.luminance_max == 10000000 &&
      frame->color.mastering.luminance_min == 50 &&
      !memcmp(frame->color.mastering.chromaticity, xy, sizeof(xy));
}

static int av1_hdr_frame_matches(const char *path, video_hw_frame_t *frame)
{
   unsigned transfer = strstr(path, "smpte2084") ? 16 :
      (strstr(path, "arib-std-b67") ? 18 : 0);
   unsigned range = strstr(path, "-pc.") ? 2 : 1;
   if (!transfer) return 1;
   if (frame->color.transfer == transfer && frame->color.primaries == 9 &&
       frame->color.matrix == 9 && frame->color.range == range &&
       frame->pixfmt == IMAGE_PIXFMT_P010 && av1_static_hdr_matches(frame)) return 1;
   puts("FAIL AV1 Vulkan HDR shared-frame source colour");
   video_hw_frame_abandon(frame);
   memset(frame, 0, sizeof(*frame));
   return 0;
}

static struct
{
   video_hw_frame_t frame;
   unsigned index, displayed, stopped, failed;
} av1_core;

static bool av1_core_sink(void *user, const video_hw_frame_t *frame)
{
   (void)user;
   if (!frame)
      return true;
   if (av1_core.frame.image)
      return false;
   av1_core.frame = *frame;
   av1_core.index = (unsigned)media_player.vshown - 1;
   return true;
}

static bool av1_core_environment(unsigned cmd, void *data)
{
   if (cmd == RETRO_ENVIRONMENT_SET_PIXEL_FORMAT)
      return *(enum retro_pixel_format *)data == RETRO_PIXEL_FORMAT_XRGB8888;
   if (cmd == RETRO_ENVIRONMENT_SHUTDOWN)
   {
      av1_core.stopped = 1;
      return true;
   }
   return false;
}

static void av1_core_video(const void *data, unsigned w, unsigned h,
                           size_t pitch)
{
   if (data || pitch || w != media_player.width || h != media_player.height)
      av1_core.failed = 1;
}

static void av1_core_poll(void)
{
}
static int16_t av1_core_input(unsigned port, unsigned device, unsigned index,
                              unsigned id)
{
   (void)port;
   (void)device;
   (void)index;
   (void)id;
   return 0;
}
static size_t av1_core_audio(const int16_t *data, size_t count)
{
   (void)data;
   return count;
}

static int av1_core_decode_fixture(const char *path, oracle_readback_t *reader,
                                   const hevc_fixture_t *fixture,
                                   unsigned *count)
{
   struct retro_game_info game;
   unsigned i, index, last = UINT32_MAX;
   int ok = 0;
   memset(&av1_core, 0, sizeof(av1_core));
   memset(&game, 0, sizeof(game));
   game.path = path;
   retro_set_environment(av1_core_environment);
   retro_set_video_refresh(av1_core_video);
   retro_set_input_poll(av1_core_poll);
   retro_set_input_state(av1_core_input);
   retro_set_audio_sample_batch(av1_core_audio);
   if (!media_set_video_hw_refresh(av1_core_sink, NULL) ||
       !retro_load_game(&game))
      goto done;
   if ((!media_player.mp4_hw_required && !media_player.webm_hw_required)
         || media_player.fb)
      goto done;
   for (i = 0; i < 8192 && !av1_core.stopped; i++)
   {
      retro_run();
      if (av1_core.failed || media_player.fb)
         goto done;
      if (av1_core.frame.image)
      {
         index = av1_core.index;
         if (index >= fixture->frames ||
             (last == UINT32_MAX ? index != 0 : index != last + 1))
            goto done;
         oracle_release(reader);
         if (!av1_hdr_frame_matches(path, &av1_core.frame)) goto done;
         if (!oracle_read_frame(reader, av1_core.frame, fixture, &index))
         {
            memset(&av1_core.frame, 0, sizeof(av1_core.frame));
            goto done;
         }
         memset(&av1_core.frame, 0, sizeof(av1_core.frame));
         last = av1_core.index;
         av1_core.displayed++;
      }
      else
         vkDeviceWaitIdle(vvd.device);
   }
   ok = av1_core.stopped && av1_core.displayed && last == fixture->frames - 1;
done:
   if (av1_core.frame.image)
      video_hw_frame_abandon(&av1_core.frame);
   retro_unload_game();
   media_set_video_hw_refresh(NULL, NULL);
   if (ok && reader->held_count)
   {
      video_hw_frame_t frame = reader->held[--reader->held_count];
      index = last;
      ok = oracle_read_frame(reader, frame, fixture, &index);
   }
   *count = av1_core.displayed;
   printf("AV1 builtin core: %s (%u displayed of %u frames)\n",
          ok ? "PASS" : "FAIL", *count, fixture->frames);
   return ok;
}
