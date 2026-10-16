/* Real surface/UI bindings and HDR screenshot capture; readback is test-only. */
#include <formats/rpng.h>

static gfx_surface_t *hdr_test_surface;
static unsigned hdr_test_dims;

static void hdr_test_frame(void *data, video_frame_info_t *info)
{
   static float white[16] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
   gfx_display_t *display = disp_get_ptr();
   struct video_coords coords;
   gfx_display_ctx_draw_t draw;
   (void)data;
   if (!display->dispctx || !hdr_test_surface || !hdr_test_surface->handle)
      return;
   memset(&draw, 0, sizeof(draw));
   memset(&coords, 0, sizeof(coords));
   coords.vertices = 4;
   coords.color = white;
   draw.dims = info->dims;
   draw.coords = &coords;
   draw.texture = hdr_test_surface->handle;
   draw.scale_factor = 1;
   hdr_test_dims = info->dims;
   gfx_display_draw(display->dispctx, &draw, info->userdata, info->dims);
}

static double hdr_test_nits(uint16_t code)
{
   double p = pow((double)code / 65535.0, 32.0 / 2523.0);
   double n = p - 3424.0 / 4096.0;
   return 10000.0 * pow((n > 0 ? n : 0)
         / (2413.0 / 128.0 - 2392.0 / 128.0 * p), 16384.0 / 2610.0);
}

static void lane_hdr_surface(void)
{
   /* 1000-nit BT.2020 patches in linear scRGB. */
   static const uint16_t pixels[8][4] = {{0x0000,0x0000,0x0000,0x3c00},
      {0x4a40,0x4a40,0x4a40,0x3c00},
      {0x4d30,0xbe3a,0xb343,0x3c00},
      {0xc758,0x4b15,0xbd07,0x3c00},
      {0xbb49,0xaeae,0x4afe,0x3c00},
      {0xc821,0x4b07,0x4a5d,0x3c00},
      {0x4cf6,0xbea5,0x4ae1,0x3c00},
      {0xb50b,0x47c3,0x49f9,0x3c00}};
   static const double colors[8][3] = {
      {0,0,0},{1,1,1},{1,0,0},{0,1,0},{0,0,1},{0,1,1},{1,0,1},{.23,.58,.91}};
   static const double to709[3][3] = {
      {1.660491,-.5876411,-.0728499},
      {-.1245505,1.1328999,-.0083494},
      {-.0181508,-.1005789,1.1187297}};
   struct menu_state *menu = menu_state_get_ptr();
   const menu_ctx_driver_t *inner = menu->driver_ctx;
   menu_ctx_driver_t replacement = *inner;
   video_driver_state_t *video = video_state_get_ptr();
   unsigned pass, i, ch, n, had = failures;

   const char *driver_name = getenv("HARNESS_VIDEO_DRIVER");
   if (!driver_name || (strcmp(driver_name, "d3d11") && strcmp(driver_name, "d3d12")))
   {
      CHECK(false, "HDR surface: native D3D11/D3D12 required");
      return;
   }
   replacement.frame = hdr_test_frame;
   for (pass = 0; pass < 2; pass++)
   {
      const video_driver_t *driver;
      void *data;
      struct video_viewport vp;
      struct rpng_hdr_metadata metadata;
      uint16_t *capture;
      unsigned w, h, samples = 0;
      enum gfx_surface_submit_result submitted;
      bool ok;
      set_threaded_via_setting(pass != 0);
      run_frames(3);
      CHECK(video_driver_supports_texture_format(TEXTURE_GPU_FORMAT_RGBA16F),
            "HDR surface: FP16 unavailable, pass %u", pass);
      hdr_test_surface = gfx_surface_new(VIDEO_SCALE_PACK(8,1), 1,
            IMAGE_PIXFMT_FP16, TEXTURE_FILTER_NEAREST, NULL, NULL);
      CHECK(hdr_test_surface != NULL, "HDR surface: allocation");
      if (!hdr_test_surface)
         break;
      memcpy(hdr_test_surface->slots[0], pixels, sizeof(pixels));
      submitted = gfx_surface_submit(hdr_test_surface, 0, true);
      CHECK(submitted == GFX_SURFACE_SUBMIT_DONE || submitted == GFX_SURFACE_SUBMIT_QUEUED,
            "HDR surface: submit returned %d", submitted);
      for (n = 0; n < 16 && hdr_test_surface->inflight; n++)
         run_frames(1);
      CHECK(hdr_test_surface->handle != 0, "HDR surface: no texture");
      if (pass)
         video_thread_wait_idle();
      menu->driver_ctx = &replacement;
      run_frames(8);
      if (pass)
         video_thread_wait_idle();
      driver = video->current_video;
      data = video->data;
      if (pass)
      {
         thread_video_t *thread = (thread_video_t*)data;
         driver = thread->driver;
         data = thread->driver_data;
      }
      memset(&vp, 0, sizeof(vp));
      driver->viewport_info(data, &vp);
      w = VIDEO_SCALE_W(vp.dims);
      h = VIDEO_SCALE_H(vp.dims);
      capture = (uint16_t*)calloc((size_t)w * h, 6);
      CHECK(capture && w && h && driver->read_viewport_hdr,
            "HDR surface: no capture path");
      memset(&metadata, 0, sizeof(metadata));
      ok = capture && w && h && driver->read_viewport_hdr
         && driver->read_viewport_hdr(data, capture, true, &metadata);
      CHECK(ok, "HDR surface: capture failed, pass %u", pass);
      CHECK(!ok || (metadata.transfer_function == 16
            && metadata.colour_primaries == (atoi(getenv("HARNESS_HDR_MODE")) == 1 ? 9 : 1)),
            "HDR surface: unexpected capture color space");
      for (i = 0; ok && i < 8; i++)
      {
         unsigned fullw = VIDEO_SCALE_W(hdr_test_dims);
         int x = (int)((i * 2 + 1) * fullw / 16) - VIDEO_POS_X(vp.pos);
         if (x < 0 || (unsigned)x >= w)
            continue;
         samples++;
         for (ch = 0; ch < 3; ch++)
         {
            double expected = colors[i][ch] * 1000;
            double actual = hdr_test_nits(capture[((size_t)(h / 2) * w + x) * 3 + ch]);
            if (metadata.colour_primaries == 1)
            {
               expected = 1000 * (to709[ch][0] * colors[i][0]
                     + to709[ch][1] * colors[i][1] + to709[ch][2] * colors[i][2]);
               if (expected < 0)
                  expected = 0;
            }
            CHECK(fabs(actual - expected) <= 4 + .01 * expected,
                  "HDR surface: pass %u pixel %u/%u got %.2f nits, expected %.2f",
                  pass, i, ch, actual, expected);
         }
      }
      CHECK(!ok || samples >= 4, "HDR surface: too few visible reference patches");
      free(capture);
      menu->driver_ctx = inner;
      gfx_surface_free(hdr_test_surface);
      hdr_test_surface = NULL;
      run_frames(3);
   }
   set_threaded_via_setting(false);
   if (failures == had)
      fprintf(stderr, "[pass] HDR surface UI lane (real direct/threaded bindings and HDR capture)\n");
}
