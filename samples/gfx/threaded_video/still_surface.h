/* Minification must sample the replacement image at every mip level. */
static gfx_surface_t *still_test_surface;

static void still_test_frame(void *data, video_frame_info_t *info)
{
   static float white[16] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
   gfx_display_t *display = disp_get_ptr();
   struct video_coords coords;
   gfx_display_ctx_draw_t draw;
   (void)data;
   if (!display->dispctx || !still_test_surface || !still_test_surface->handle)
      return;
   memset(&draw, 0, sizeof(draw));
   memset(&coords, 0, sizeof(coords));
   coords.vertices = 4;
   coords.color = white;
   draw.pos = VIDEO_POS_PACK((int)VIDEO_SCALE_W(info->dims) / 2 - 32,
         (int)VIDEO_SCALE_H(info->dims) / 2 - 32);
   draw.dims = VIDEO_SCALE_PACK(64,64);
   draw.coords = &coords;
   draw.texture = still_test_surface->handle;
   draw.scale_factor = 1;
   if (display->dispctx->blend_begin)
      display->dispctx->blend_begin(info->userdata);
   gfx_display_draw(display->dispctx, &draw, info->userdata, info->dims);
   if (display->dispctx->blend_end)
      display->dispctx->blend_end(info->userdata);
}

static void lane_still_surface(void)
{
   static const enum texture_filter_type filters[2] = {
      TEXTURE_FILTER_MIPMAP_LINEAR, TEXTURE_FILTER_MIPMAP_NEAREST};
   struct menu_state *menu = menu_state_get_ptr();
   const menu_ctx_driver_t *inner = menu->driver_ctx;
   menu_ctx_driver_t replacement = *inner;
   video_driver_state_t *video = video_state_get_ptr();
   unsigned pass, f, image, i, had = failures;
   replacement.frame = still_test_frame;
   for (pass = 0; pass < 2; pass++)
   for (f = 0; f < 2; f++)
   {
      set_threaded_via_setting(pass != 0);
      run_frames(3);
      still_test_surface = gfx_surface_new_still(filters[f]);
      CHECK(still_test_surface != NULL, "still surface: allocation");
      if (!still_test_surface)
         continue;
      for (image = 0; image < 2; image++)
      {
         struct texture_image *img = (struct texture_image*)calloc(1, sizeof(*img));
         struct video_viewport vp;
         uint8_t *capture;
         unsigned w, h, tries;
         int expected = image ? 64 : 255;
         bool ok;
         CHECK(img != NULL, "still surface: image descriptor");
         if (!img)
            break;
         img->width = img->height = 256;
         img->supports_rgba = (video_driver_get_disp_flags() & VIDEO_FLAG_USE_RGBA) != 0;
         img->pixels = (uint32_t*)malloc(256 * 256 * sizeof(uint32_t));
         CHECK(img->pixels != NULL, "still surface: pixels");
         if (!img->pixels)
         {
            free(img);
            break;
         }
         for (i = 0; i < 256 * 256; i++)
            img->pixels[i] = image ? UINT32_C(0xff404040) : UINT32_C(0xffffffff);
         CHECK(gfx_surface_submit_image(still_test_surface, img), "still surface: submit");
         for (tries = 0; tries < 16 && still_test_surface->inflight; tries++)
            run_frames(1);
         CHECK(still_test_surface->handle && !still_test_surface->inflight,
               "still surface: upload did not land");
         if (pass)
            video_thread_wait_idle();
         menu->driver_ctx = &replacement;
         run_frames(4);
         memset(&vp, 0, sizeof(vp));
         video_driver_get_viewport_info(&vp);
         w = VIDEO_SCALE_W(vp.dims);
         h = VIDEO_SCALE_H(vp.dims);
         capture = (uint8_t*)calloc((size_t)w * h, 3);
         ok = capture && w && h && video->current_video->read_viewport
            && video->current_video->read_viewport(video->data, capture, false);
         CHECK(ok, "still surface: capture pass %u filter %u", pass, f);
         for (i = 0; ok && i < 3; i++)
         {
            int actual = capture[((size_t)(h / 2) * w + w / 2) * 3 + i];
            CHECK(abs(actual - expected) <= 2,
                  "still surface: pass %u filter %u image %u got %d expected %d",
                  pass, f, image, actual, expected);
         }
         free(capture);
         if (pass)
            video_thread_wait_idle();
         menu->driver_ctx = inner;
      }
      gfx_surface_free(still_test_surface);
      still_test_surface = NULL;
      run_frames(3);
   }
   set_threaded_via_setting(false);
   if (failures == had)
      fprintf(stderr, "[pass] still surface minification (direct/threaded, both mip filters)\n");
}
