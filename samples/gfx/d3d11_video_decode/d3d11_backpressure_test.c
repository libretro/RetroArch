#ifdef _WIN32
#define main reference_main
#include "d3d11_video_decode_test.c"
#undef main
#include <formats/image.h>
#include <formats/video_player.h>

static video_hw_frame_t held_frames[8];
static unsigned held_count;

static int player_start(void *user, unsigned width, unsigned height, unsigned jobs)
{
   (void)user; (void)width; (void)height; (void)jobs;
   return 0;
}

static enum video_player_present player_present(void *user,
      const video_player_frame_t *frame)
{
   (void)user;
   if (!frame->hw || held_count >= 8)
      return VIDEO_PLAYER_PRESENT_FAILED;
   held_frames[held_count++] = *frame->hw;
   return VIDEO_PLAYER_PRESENT_DONE;
}

static void test_backpressure(int free_later, int cancel)
{
   video_player_sink_t sink = {0};
   video_player_t *player;
   void *stream;
   unsigned i, before_count;
   size_t anchor;
   int64_t now = 0;
   int64_t due;
   DWORD started;
   enum video_player_state state = VIDEO_PLAYER_PLAYING;

   held_count = 0;
   stream = image_transfer_anim_stream_new((void*)stream_baseline,
         sizeof(stream_baseline), IMAGE_TYPE_MP4);
   if (!stream || !image_transfer_anim_stream_set_hw(stream, IMAGE_TYPE_MP4, true))
   {
      puts("d3d11_backpressure_test: SKIP (H.264 decoder unavailable)");
      image_transfer_anim_stream_free(stream, IMAGE_TYPE_MP4);
      return;
   }
   sink.start = player_start;
   sink.start_hw = player_start;
   sink.present = player_present;
   player = video_player_new(stream, IMAGE_TYPE_MP4, 1, &sink, VIDEO_PLAYER_INLINE);
   check("inline player opened", stream && player);
   if (!player)
   {
      image_transfer_anim_stream_free(stream, IMAGE_TYPE_MP4);
      return;
   }
   for (i = 0; i < 1000 && held_count < 4 && state == VIDEO_PLAYER_PLAYING; i++)
   {
      state = video_player_poll(player, now += 100000);
      if (held_count < 4)
         Sleep(1);
   }
   check("four output leases held", held_count == 4);
   if (free_later && held_count == 4)
      video_hw_frame_abandon(&held_frames[1]);
   anchor = image_transfer_anim_stream_consumed(stream, IMAGE_TYPE_MP4);
   due = video_player_due(player);
   started = GetTickCount();
   state = video_player_poll(player, now += 100000);
   check("busy poll returns without ending", state == VIDEO_PLAYER_PLAYING
         && GetTickCount() - started < 100);
   if (free_later)
      check("returned non-oldest texture is selected", held_count == 5
            && held_frames[4].image == held_frames[1].image);
   if (!free_later)
   {
      check("pending packet stays inside the window", anchor
            == image_transfer_anim_stream_consumed(stream, IMAGE_TYPE_MP4));
      check("busy output is not published", held_count == 4);
      before_count = held_count;
      state = video_player_poll(player, now += 100000);
      check("repeated busy poll preserves the frame", state == VIDEO_PLAYER_PLAYING
            && held_count == before_count);
      check("busy poll does not advance the clock", video_player_due(player)
            == due);
   }
   if (cancel)
   {
      video_player_free(player);
      image_transfer_anim_stream_free(stream, IMAGE_TYPE_MP4);
   }
   for (i = 0; i < held_count; i++)
      video_hw_frame_abandon(&held_frames[i]);
   if (!cancel)
   {
      before_count = held_count;
      for (i = 0; i < 1000 && held_count == before_count; i++)
      {
         state = video_player_poll(player, now += 100000);
         if (held_count == before_count)
            Sleep(1);
      }
      check("release resumes with one frame", state == VIDEO_PLAYER_PLAYING
            && held_count == before_count + 1);
      if (held_count)
         video_hw_frame_abandon(&held_frames[held_count - 1]);
      video_player_free(player);
      image_transfer_anim_stream_free(stream, IMAGE_TYPE_MP4);
   }
}

int main(void)
{
   D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
   setvbuf(stdout, NULL, _IONBF, 0);
   if (FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
         &level, 1, D3D11_SDK_VERSION, &dev, NULL, &ctx))
         || FAILED(dev->lpVtbl->QueryInterface(dev, &d3d11vd_iid_device5,
               (void**)&dev5))
         || FAILED(ctx->lpVtbl->QueryInterface(ctx, &d3d11vd_iid_context4,
               (void**)&ctx4))
         || !d3d11_video_decode_init(dev))
   {
      D3D11VD_RELEASE(ctx4);
      D3D11VD_RELEASE(dev5);
      D3D11VD_RELEASE(ctx);
      D3D11VD_RELEASE(dev);
      puts("d3d11_backpressure_test: SKIP (no hardware decoder)");
      return 0;
   }
   test_backpressure(0, 0);
   test_backpressure(1, 0);
   test_backpressure(0, 1);
   d3d11_video_decode_deinit();
   D3D11VD_RELEASE(ctx4);
   D3D11VD_RELEASE(dev5);
   D3D11VD_RELEASE(ctx);
   D3D11VD_RELEASE(dev);
   printf("d3d11_backpressure_test: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
   return fails ? 1 : 0;
}
#else
#include <stdio.h>
int main(void)
{
   puts("d3d11_backpressure_test: SKIP (Windows GPU test)");
   return 0;
}
#endif
