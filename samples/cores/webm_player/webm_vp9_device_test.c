#if !defined(_WIN32) || !defined(HAVE_RMP4)
#include <stdio.h>
int main(void)
{
   puts("Movie core VP9 GPU oracle: SKIP (requires Windows and MP4 test readers)");
   return 0;
}
#else
/* SPDX-License-Identifier: MIT */
/* Optional native GPU oracle; waits and pixel readbacks are test-only. */
#define main decoder_device_main
#include "../../gfx/d3d11_video_decode/d3d11_video_decode_test.c"
#undef main
#include "../../../cores/libretro-media/media_core.c"

static frames_t core_frames;
static opened_t core_reader;
static unsigned core_done, core_format, core_retained;
static video_hw_frame_t core_held;
static bool core_sink(void *user, const video_hw_frame_t *frame)
{
   (void)user;
   if (!frame) return true;
   core_format = frame->pixfmt;
   if (getenv("D3D11_TEST_SHARED_DPB"))
   {
      d3d11vd_dec_t *d = (d3d11vd_dec_t*)frame->owner;
      check("movie core: direct DPB without output textures", d->direct_dpb && !d->out[0]
            && frame->image == (uint64_t)(uintptr_t)d->dpb_handle);
      if (!core_retained)
      {
         unsigned slot;
         for (slot = 0; slot < d->slots; slot++)
            if (d->slot_map[slot] == frame->array_slice) break;
         check("movie core: retain a direct DPB lease", slot < d->slots
               && !d3d11vd_output(d, slot, &core_held));
         core_retained = 1;
      }
   }
   check("movie core: GPU frame read", use_d3d12
         ? take_hw12(&core_frames, frame) : take_hw(&core_frames, &core_reader, frame));
   return true;
}
static bool core_env(unsigned cmd, void *data)
{
   if (cmd == RETRO_ENVIRONMENT_SET_PIXEL_FORMAT)
      return *(enum retro_pixel_format*)data == RETRO_PIXEL_FORMAT_XRGB8888;
   if (cmd == RETRO_ENVIRONMENT_SHUTDOWN) { core_done = 1; return true; }
   return false;
}
static void core_video(const void *data, unsigned w, unsigned h, size_t pitch)
{
   check("movie core: GPU refresh without CPU pixels", !data && !pitch
         && w == media_player.width && h == media_player.height);
}
static void core_poll(void) { }
static int16_t core_input(unsigned port, unsigned device, unsigned index, unsigned id)
{ (void)port; (void)device; (void)index; (void)id; return 0; }
static size_t core_audio(const int16_t *data, size_t count)
{ (void)data; return count; }

static void core_run_fixture(void)
{
   struct retro_game_info game;
   uint8_t *raw;
   size_t raw_size = 0, y_size, frame_size, i;
   unsigned tries;
   int matches;
   game.data = NULL; game.size = 0; game.meta = NULL;
   game.path = getenv("VP9_TEST_WEBM");
   raw = read_fixture_file(getenv("VP9_TEST_RAW"), &raw_size);
   if (!raw) { check("movie core: oracle opens", 0); return; }
   core_done = core_retained = 0;
   memset(&core_held, 0, sizeof(core_held));
   check("movie core: opens WebM", retro_load_game(&game));
   media_video_hw_ready();
   if (!media_player.webmvs)
   {
      check("movie core: hardware selected", 0);
      goto done;
   }
   memset(media_player.fb, 0xa5, (size_t)media_player.width * media_player.height * 4);
   for (tries = 0; tries < 2000 && !core_done; tries++)
   {
      /* Keep each slot due across BUSY so the oracle observes every frame. */
      int slot = core_frames.n < media_player.nvts ? core_frames.n : media_player.nvts - 1;
      media_player.play_ns = media_player.vts[slot] - media_player.frame_ns / 2;
      retro_run();
      Sleep(1);
   }
   y_size = (size_t)core_frames.w * core_frames.h
         * (core_format == IMAGE_PIXFMT_P010 ? 2 : 1);
   frame_size = y_size * 3 / 2;
   matches = core_done && core_frames.n > 0 && frame_size
         && raw_size == frame_size * core_frames.n;
   for (i = 0; matches && i < (size_t)core_frames.n; i++)
      matches = !memcmp(core_frames.y[i], raw + frame_size * i, y_size)
         && !memcmp(core_frames.uv[i], raw + frame_size * i + y_size, y_size / 2);
   printf("Movie core VP9 through D3D%s: %d frames, %s\n",
         use_d3d12 ? "12" : "11", core_frames.n, game.path);
   check("movie core: every GPU sample matches oracle", matches);
   matches = 1;
   for (i = 0; i < (size_t)media_player.width * media_player.height; i++)
      if (media_player.fb[i] != UINT32_C(0xa5a5a5a5)) matches = 0;
   check("movie core: CPU canvas untouched", matches);
   {
      int target = media_player.nvts / 2;
      frames_free(&core_frames);
      memset(&core_frames, 0, sizeof(core_frames));
      media_seek(&media_player, media_player.vts[target]);
      for (tries = 0; tries < 2000 && !core_frames.n; tries++)
      {
         retro_run();
         Sleep(1);
      }
      check("movie core: GPU reference chain survives skipped exports", core_frames.n == 1
            && !memcmp(core_frames.y[0], raw + frame_size * target, y_size)
            && !memcmp(core_frames.uv[0], raw + frame_size * target + y_size, y_size / 2));
   }
done:
   retro_unload_game();
   if (core_held.image)
   {
      int taken = use_d3d12 ? take_hw12(&core_frames, &core_held)
            : take_hw(&core_frames, &core_reader, &core_held);
      check("movie core: held DPB picture survives reuse, rewind and close", taken
            && core_frames.n > 0 && !memcmp(core_frames.y[core_frames.n - 1], raw, y_size)
            && !memcmp(core_frames.uv[core_frames.n - 1], raw + y_size, y_size / 2));
      memset(&core_held, 0, sizeof(core_held));
   }
   opened_clear(&core_reader); frames_free(&core_frames); free(raw);
}

int main(void)
{
   static const D3D_FEATURE_LEVEL levels[2] = {
      D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
   if (!getenv("VP9_TEST_WEBM") || !getenv("VP9_TEST_RAW"))
   {
      puts("Movie core VP9 GPU oracle: SKIP (set VP9_TEST_WEBM and VP9_TEST_RAW)");
      return 0;
   }
   if (FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
            levels, 2, D3D11_SDK_VERSION, &dev, NULL, &ctx))
         || FAILED(dev->lpVtbl->QueryInterface(dev, &d3d11vd_iid_device5, (void**)&dev5))
         || FAILED(ctx->lpVtbl->QueryInterface(ctx, &d3d11vd_iid_context4, (void**)&ctx4))
         || !d3d11_video_decode_init_formats(dev, IMAGE_PIXFMT_NV12 | IMAGE_PIXFMT_P010
            | (getenv("D3D11_TEST_SHARED_DPB") ? D3D11_VIDEO_DECODE_SHARED_DPB : 0)))
   {
      puts("Movie core VP9 GPU oracle: no hardware device");
      return 1;
   }
   media_set_video_hw_refresh(core_sink, NULL);
   retro_set_environment(core_env); retro_set_video_refresh(core_video);
   retro_set_input_poll(core_poll); retro_set_input_state(core_input);
   retro_set_audio_sample_batch(core_audio);
   core_run_fixture();
   if (d12_init())
   {
      use_d3d12 = 1;
      core_run_fixture();
      use_d3d12 = 0;
   }
   d12_deinit();
   d3d11_video_decode_deinit();
   check("movie core: decoder sessions closed", !retro_atomic_load_acquire_int(&d3d11vd.live));
   D3D11VD_RELEASE(ctx4); D3D11VD_RELEASE(dev5);
   D3D11VD_RELEASE(ctx); D3D11VD_RELEASE(dev);
   return fails ? 1 : 0;
}

#endif
