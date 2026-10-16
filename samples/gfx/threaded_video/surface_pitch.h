#ifdef HAVE_VULKAN
typedef struct
{
   uintptr_t id;
   uint32_t *pixels;
} surface_pitch_read_t;

static uintptr_t surface_pitch_read(void *data)
{
   surface_pitch_read_t *read = (surface_pitch_read_t*)data;
   return planar_vk_readback(read->id, 300, 48, read->pixels);
}
#endif

/* Padded mapped rows, through the real surface and threaded wrapper. */
static void lane_surface_pitch(void)
{
   struct menu_state *menu = menu_state_get_ptr();
   const menu_ctx_driver_t *inner = menu->driver_ctx;
   menu_ctx_driver_t replacement = *inner;
   video_driver_state_t *video = video_state_get_ptr();
   const char *driver = getenv("HARNESS_VIDEO_DRIVER");
   bool fp16 = getenv("HARNESS_HDR_MODE") && driver && !strcmp(driver, "d3d12");
   unsigned pass, k, tries, x, y, had = failures;
   bool rgba = (video_driver_get_disp_flags() & VIDEO_FLAG_USE_RGBA) != 0;
   replacement.frame = still_test_frame;
   if (!driver || (strcmp(driver, "vulkan") && strcmp(driver, "d3d12")))
      return;
   for (pass = 0; pass < 4; pass++)
   {
      gfx_surface_t *s;
      unsigned slots = pass < 2 ? 2 : 1;
      bool threaded = (pass & 1) != 0;
      uintptr_t first = 0;
      bool lent = false;
      set_threaded_via_setting(threaded);
      run_frames(3);
      s = gfx_surface_new(VIDEO_SCALE_PACK(300, 48), slots,
            fp16 ? IMAGE_PIXFMT_FP16 : IMAGE_PIXFMT_8888,
            TEXTURE_FILTER_NEAREST, NULL, NULL);
      CHECK(s != NULL, "surface pitch: allocation");
      if (!s) continue;
      s->padded = 1;
      for (k = 0; k < 12; k++)
      {
         unsigned slot = k % slots;
         enum gfx_surface_submit_result result;
         for (tries = 0; tries < 32
               && (s->inflight || !gfx_surface_slot_writable(s, slot)); tries++)
            run_frames(1);
         CHECK(!s->inflight && gfx_surface_slot_writable(s, slot),
               "surface pitch: unavailable slot");
         if (s->inflight || !gfx_surface_slot_writable(s, slot)) break;
         for (y = 0; y < 48; y++)
         {
            uint32_t *row = (uint32_t*)((uint8_t*)s->slots[slot] + y * s->pitch[slot]);
            bool white = ((y / 8 == 1 || y / 8 == 4) != ((k & 1) != 0));
            for (x = 0; x < 300; x++)
               if (fp16)
               {
                  uint16_t *pixel = (uint16_t*)row + x * 4;
                  pixel[0] = pixel[1] = pixel[2] = white ? 0x3c00 : 0;
                  pixel[3] = 0x3c00;
               }
               else
                  row[x] = white ? 0xffffffffu : 0xff000000u;
         }
         if (k == 6)
            s->filter = TEXTURE_FILTER_LINEAR;
         for (tries = 0; tries < 32; tries++)
         {
            result = gfx_surface_submit(s, slot, rgba);
            if (result != GFX_SURFACE_SUBMIT_DROPPED) break;
            run_frames(1);
         }
         CHECK(result == GFX_SURFACE_SUBMIT_DONE || result == GFX_SURFACE_SUBMIT_QUEUED,
               "surface pitch: submit %u failed (%d)", k, result);
         for (tries = 0; tries < 32 && s->inflight; tries++) run_frames(1);
         CHECK(!s->inflight && s->handle, "surface pitch: upload did not finish");
         run_frames(2);
         if (!k) first = s->handle;
         if (k == 5 || k == 10)
         {
            struct video_viewport vp;
            struct rpng_hdr_metadata metadata;
            const video_driver_t *capture_driver;
            void *capture_data;
            uint8_t *capture;
            unsigned w, h, band, c;
            bool ok;
            CHECK(s->lent == 3, "surface pitch: %s pass %u lent %u", driver, pass, s->lent);
            CHECK(s->pitch[slot] >= (fp16 ? 2400u : 1200u), "surface pitch: short row");
            if (!strcmp(driver, "d3d12"))
               CHECK(s->pitch[slot] == (fp16 ? 2560u : 1280u), "surface pitch: D3D12 row is %u",
                     (unsigned)s->pitch[slot]);
            lent |= s->lent != 0;
            if (k == 10)
               CHECK(s->handle != first, "surface pitch: filter replacement did not load");
#ifdef HAVE_VULKAN
            if (!strcmp(driver, "vulkan"))
            {
               surface_pitch_read_t read;
               unsigned i;
               read.id = s->handle;
               read.pixels = (uint32_t*)calloc(300 * 48, sizeof(uint32_t));
               ok = read.pixels && (threaded
                  ? video_thread_texture_handle(&read, surface_pitch_read)
                  : surface_pitch_read(&read));
               CHECK(ok, "surface pitch: Vulkan texture capture");
               for (i = 0; ok && i < 300 * 48; i++)
               {
                  unsigned b = i / 300 / 8;
                  uint32_t expected = ((b == 1 || b == 4) != ((k & 1) != 0))
                     ? 0xffffffffu : 0xff000000u;
                  CHECK(read.pixels[i] == expected,
                        "surface pitch: Vulkan pass %u pixel %u got %08x expected %08x",
                        pass, i, read.pixels[i], expected);
               }
               free(read.pixels);
               continue;
            }
#endif
            if (threaded) video_thread_wait_idle();
            still_test_surface = s;
            menu->driver_ctx = &replacement;
            run_frames(4);
            if (threaded) video_thread_wait_idle();
            capture_driver = video->current_video;
            capture_data = video->data;
            if (threaded)
            {
               thread_video_t *thread = (thread_video_t*)capture_data;
               capture_driver = thread->driver;
               capture_data = thread->driver_data;
            }
            memset(&vp, 0, sizeof(vp));
            capture_driver->viewport_info(capture_data, &vp);
            w = VIDEO_SCALE_W(vp.dims);
            h = VIDEO_SCALE_H(vp.dims);
            capture = (uint8_t*)calloc((size_t)w * h, fp16 ? 6 : 3);
            memset(&metadata, 0, sizeof(metadata));
            if (fp16)
               ok = capture && w >= 64 && h >= 64 && capture_driver->read_viewport_hdr
                  && capture_driver->read_viewport_hdr(capture_data,
                        (uint16_t*)capture, true, &metadata);
            else
               ok = capture && w >= 64 && h >= 64 && capture_driver->read_viewport
                  && capture_driver->read_viewport(capture_data, capture, false);
            CHECK(ok, "surface pitch: capture %s pass %u", driver, pass);
            for (band = 0; ok && band < 6; band++)
            for (x = 0; x < 3; x++)
            for (c = 0; c < 3; c++)
            {
               unsigned cx = w / 2 - 20 + x * 20;
               unsigned cy = h / 2 - 32 + (2 * band + 1) * 64 / 12;
               bool white = (band == 1 || band == 4) != ((k & 1) != 0);
               if (fp16)
               {
                  double expected = white ? 80.0 : 0.0;
                  double actual = hdr_test_nits(((uint16_t*)capture)[
                        ((size_t)cy * w + cx) * 3 + c]);
                  CHECK(fabs(actual - expected) <= 3.0,
                        "surface pitch: FP16 pass %u band %u got %.2f expected %.2f nits",
                        pass, band, actual, expected);
               }
               else
               {
                  int expected = white ? 255 : 0;
                  int actual = capture[((size_t)cy * w + cx) * 3 + c];
                  CHECK(abs(actual - expected) <= 6,
                        "surface pitch: %s pass %u band %u got %d expected %d",
                        driver, pass, band, actual, expected);
               }
            }
            free(capture);
            if (threaded) video_thread_wait_idle();
            menu->driver_ctx = inner;
            still_test_surface = NULL;
         }
      }
      CHECK(lent, "surface pitch: no padded streaming");
      gfx_surface_free(s);
      run_frames(3);
   }
   set_threaded_via_setting(false);
   run_frames(2);
   if (failures == had)
      fprintf(stderr, "[pass] surface pitch (%s, 300 pixels, direct/threaded, one/two slots, replacement capture)\n",
            fp16 ? "FP16" : "RGBA8");
}
