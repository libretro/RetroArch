/* Real core/MP4/HEVC ownership, with a portable GPU backend and sink. */
#define main hevc_stream_checks_main
#include "../../gfx/d3d11_video_decode/rh265_hw_stream_test.c"
#undef main
#include "../../../cores/libretro-media/media_core.c"
static unsigned sink_calls, sink_busy, sink_taken, ticks, stopped;
static unsigned cpu_ticks, expect_software, sink_ready = 1;
static video_hw_frame_t held;
static bool sink(void *user, const video_hw_frame_t *frame)
{
   if (!frame) return sink_ready != 0;
   CHECK(user == &sink_taken && frame->image && frame->width == 64 && frame->height == 48
         && frame->pixfmt == IMAGE_PIXFMT_P010);
   sink_calls++;
   if (held.image) CHECK(!memcmp(&held, frame, sizeof(held)));
   if (sink_busy)
   {
      held = *frame;
      sink_busy--;
      return false;
   }
   memset(&held, 0, sizeof(held));
   video_hw_frame_abandon(frame);
   sink_taken++;
   return true;
}
static bool env(unsigned cmd, void *data)
{
   if (cmd == RETRO_ENVIRONMENT_SHUTDOWN)
   {
      stopped++;
      return true;
   }
   if (cmd == RETRO_ENVIRONMENT_SET_PIXEL_FORMAT)
      return *(enum retro_pixel_format*)data == RETRO_PIXEL_FORMAT_XRGB8888;
   return false;
}
static void video_tick(const void *data, unsigned w, unsigned h, size_t pitch)
{
   CHECK(w == 64 && h == 48);
   if (expect_software)
   {
      CHECK(data && pitch == 64 * sizeof(uint32_t));
      cpu_ticks++;
   }
   else CHECK(!data && pitch == 0);
   ticks++;
}
static void input_tick(void) { }
static int16_t input_none(unsigned port, unsigned device, unsigned index, unsigned id)
{ (void)port; (void)device; (void)index; (void)id; return 0; }
static size_t audio_tick(const int16_t *samples, size_t count)
{ (void)samples; return count; }
static bool start_core(int hardware)
{
   struct retro_game_info game;
   unsigned before = opened;
   memset(&held, 0, sizeof(held));
   memset(&game, 0, sizeof(game));
   game.path = "../../formats/video_hdr_fp16/fixtures/sdr_hevc.mp4";
   CHECK(retro_load_game(&game) && opened == before);
   if (!media_player.mp4vs) return false;
   CHECK(!media_player.hw_started && !media_player.hw_video);
   (void)hardware;
   CHECK(!media_player.fb);
   CHECK(!media_set_video_hw_refresh(NULL, NULL));
   return true;
}

int main(void)
{
   unsigned i, before, decoded;
   uint8_t state[40];
   hevc_stream_checks_main();
   rh265_set_hw_backend(&backend);
   CHECK(media_set_video_hw_refresh(sink, &sink_taken));
   retro_set_environment(env); retro_set_video_refresh(video_tick);
   retro_set_input_poll(input_tick); retro_set_input_state(input_none);
   retro_set_audio_sample_batch(audio_tick);
   if (!start_core(1)) return 1;
   media_video_hw_ready();
   CHECK(media_player.hw_video && media_player.hw_started);
   decode_busy = output_busy = 1; sink_busy = 2;
   retro_run();
   CHECK(!media_player.eof && !sink_calls
         && rmp4_video_stream_wait_reason(media_player.mp4vs) == RMP4_VIDEO_WAIT_DECODER);
   for (i = 0; i < 30 && !stopped; i++)
   {
      decoded = accepted;
      before = media_player.hw_frame.image != 0;
      retro_run();
      if (before) CHECK(accepted == decoded);
   }
   CHECK(stopped && sink_taken && ticks && !media_player.hw_frame.image);
   CHECK(!media_player.fb);
   CHECK(retro_serialize(state, sizeof(state)));
   retro_unload_game();
   CHECK(opened == closed && exported == abandoned);
   refuse = 1;
   expect_software = 1;
   CHECK(start_core(0));
   CHECK(retro_unserialize(state, sizeof(state)));
   CHECK(media_player.fb && media_player.fb[0] != 0);
   retro_run();
   CHECK(!media_player.hw_video && cpu_ticks);
   retro_unload_game();
   refuse = 0;
   expect_software = 0;
   CHECK(start_core(1)); sink_busy = 100;
   for (i = 0; i < 20 && !media_player.hw_frame.image; i++) retro_run();
   CHECK(media_player.hw_frame.image);
   before = abandoned;
   refuse = 1;
   retro_reset();
   CHECK(!media_player.hw_frame.image && abandoned > before && !media_player.hw_video);
   memset(&held, 0, sizeof(held));
   expect_software = 1;
   before = sink_calls;
   for (i = 0; i < 5; i++) retro_run();
   CHECK(cpu_ticks && sink_calls == before && media_player.fb && media_player.fb[0] != 0);
   expect_software = 0;
   refuse = 0;
   retro_reset();
   CHECK(media_player.hw_video);
   for (i = 0; i < 20 && !media_player.hw_frame.image; i++) retro_run();
   CHECK(media_player.hw_frame.image);
   before = abandoned;
   media_seek(&media_player, 0);
   CHECK(!media_player.hw_frame.image && abandoned > before);
   memset(&held, 0, sizeof(held));
   for (i = 0; i < 20 && !media_player.hw_frame.image; i++) retro_run();
   CHECK(media_player.hw_frame.image);
   CHECK(retro_serialize_size() == sizeof(state) && retro_serialize(state, sizeof(state)));
   before = abandoned;
   CHECK(retro_unserialize(state, sizeof(state)));
   CHECK(abandoned > before && media_player.hw_frame.image);
   retro_unload_game();
   CHECK(opened == closed && exported == abandoned);
   refuse = 1;
   CHECK(start_core(0));
   expect_software = 1;
   before = sink_calls;
   for (i = 0; i < 5; i++) retro_run();
   CHECK(sink_calls == before && media_player.fb && media_player.fb[0] != 0);
   retro_unload_game();
   CHECK(opened == closed && exported == abandoned);
   refuse = 0;
   expect_software = 0;
   CHECK(start_core(1));
   sink_busy = 100;
   sink_ready = 1;
   for (i = 0; i < 20 && !media_player.hw_frame.image; i++) retro_run();
   CHECK(media_player.hw_video && media_player.hw_frame.image);
   before = closed;
   media_suspend_video_hw();
   CHECK(!media_player.hw_video && !media_player.hw_frame.image && closed > before);
   CHECK(opened == closed && exported == abandoned);
   memset(&held, 0, sizeof(held));
   rh265_set_hw_backend(NULL);
   rh265_set_hw_backend(&backend);
   retro_run();
   CHECK(media_player.hw_video && !media_player.hw_restart && opened == closed + 1);
   media_suspend_video_hw();
   CHECK(opened == closed && exported == abandoned);
   memset(&held, 0, sizeof(held));
   sink_ready = 0;
   expect_software = 1;
   retro_run();
   CHECK(!media_player.hw_video && media_player.hw_started);
   media_suspend_video_hw();
   sink_ready = 1;
   media_video_hw_ready();
   CHECK(media_player.hw_video && media_player.hw_started);
   retro_unload_game();
   CHECK(media_set_video_hw_refresh(NULL, NULL));
   rh265_set_hw_backend(NULL);
   printf("Movie GPU lease handoff: %u failures\n", failures);
   return failures ? 1 : 0;
}
