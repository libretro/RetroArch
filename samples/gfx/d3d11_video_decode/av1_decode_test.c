/* AV1 hardware pixels against an independently decoded NV12/P010 reference. */
#include <stdio.h>
#ifdef _WIN32
#define main legacy_decoder_test_main
#include "d3d11_video_decode_test.c"
#undef main

static rav1_hw_session_t av1_session;
static rav1_sequence_t av1_sequence;
static video_hw_frame_t av1_held[4];
static unsigned av1_order[MAX_FRAMES];
static int refused_av1_sequence(const rav1_sequence_t *sequence, unsigned slots)
{
   rav1_hw_ops_t ops, zero;
   int live = retro_atomic_load_acquire_int(&d3d11vd.live);
   int serial = retro_atomic_load_acquire_int(&d3d11vd.serial);
   memset(&ops, 165, sizeof(ops));
   memset(&zero, 0, sizeof(zero));
   if (d3d11_video_decode_av1_open(sequence, slots, &ops))
   {
      d3d11_video_decode_av1_close(&ops);
      return 0;
   }
   return !memcmp(&ops, &zero, sizeof(ops)) &&
       retro_atomic_load_acquire_int(&d3d11vd.live) == live &&
       retro_atomic_load_acquire_int(&d3d11vd.serial) == serial;
}

static int av1_sequence_refusals(const rav1_sequence_t *sequence)
{
   rav1_sequence_t changed;
   unsigned i;
   if (!refused_av1_sequence(NULL, 9) ||
       !refused_av1_sequence(sequence, 8) ||
       !refused_av1_sequence(sequence, D3D11VD_MAX_SLOTS + 1)) return 0;
   for (i = 0; i < 10; i++)
   {
      changed = *sequence;
      switch (i)
      {
         case 0: changed.profile = 1; break;
         case 1: changed.bit_depth = 12; break;
         case 2: changed.monochrome = 1; break;
         case 3: changed.subsampling_x = 0; break;
         case 4: changed.film_grain_params_present = 1; break;
         case 5: changed.transfer_characteristics = 16; changed.matrix_coefficients = 10; break;
         case 6: changed.transfer_characteristics = 18; changed.color_primaries = 12; break;
         case 7: changed.matrix_coefficients = 10; break;
         case 8: changed.max_width |= 1; break;
         case 9: changed.max_height |= 1; break;
      }
      if (!refused_av1_sequence(&changed, 9))
      { printf("FAIL AV1 sequence refusal %u\n", i); return 0; }
   }
   puts("AV1 unsupported sequences refused without allocation or owner mutation");
   return 1;
}

/* Test owner retries while the reader's queued done signal completes. */
static int export_av1_frame(video_hw_frame_t *frame)
{
   DWORD began = GetTickCount();
   int ret;
   do
   {
      ret = rav1_hw_session_output(&av1_session, frame);
      if (ret != 1) return ret;
      Sleep(1);
   } while (GetTickCount() - began < 5000);
   return 1;
}

static int read_av1_frame(frames_t *hw, opened_t *opened,
      const video_hw_frame_t *frame, unsigned ordinal)
{
   if (hw->n >= MAX_FRAMES) return 0;
   av1_order[hw->n] = ordinal;
   return take_hw(hw, opened, frame);
}

static unsigned av1_expected_transfer, av1_expected_range;
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

static int av1_color_matches(const video_hw_frame_t *frame)
{
   if (!av1_expected_transfer) return 1;
   if (frame->color.transfer == av1_expected_transfer &&
       frame->color.primaries == 9 && frame->color.matrix == 9 &&
       frame->color.range == av1_expected_range &&
       frame->pixfmt == IMAGE_PIXFMT_P010 && av1_static_hdr_matches(frame)) return 1;
   puts("FAIL AV1 HDR shared-frame source colour");
   return 0;
}

static int av1_container_seek_frame(void *stream, enum image_type_enum type,
      opened_t *opened, const uint8_t *reference, size_t reference_size,
      unsigned expected)
{
   frames_t capture;
   video_hw_frame_t frame;
   unsigned polls, bps;
   size_t bytes, at;
   int ret = 0, ok = 0;
   memset(&capture, 0, sizeof(capture));
   if (!image_transfer_anim_stream_seek_begin(stream, type,
            (int64_t)expected * 1000000 / (type == IMAGE_TYPE_MP4 ? 24 : 25))) return 0;
   for (polls = 0; polls < 10000; polls++)
   {
      ret = image_transfer_anim_stream_seek_step(stream, type, NULL);
      if (ret) break;
      Sleep(1);
   }
   if (ret != 1 || polls == 10000) return 0;
   for (polls = 0; polls < 10000; polls++)
   {
      if (image_transfer_anim_stream_next(stream, type, NULL)) break;
      if (!image_transfer_anim_stream_waiting(stream, type)) return 0;
      Sleep(1);
   }
   if (polls == 10000 ||
       !image_transfer_anim_stream_hw_frame(stream, type, &frame)) return 0;
   if (!av1_color_matches(&frame))
   {
      video_hw_frame_abandon(&frame);
      return 0;
   }
   if (getenv("AV1_TEST_HDR_TIMELINE") &&
         (frame.color.max_cll != (expected >= 5 ? 2000u : 1000u) ||
          frame.color.max_fall != (expected >= 5 ? 600u : 400u)))
   {
      fprintf(stderr, "FAIL AV1 seek HDR timeline at picture %u: %u/%u\n",
            expected, frame.color.max_cll, frame.color.max_fall);
      video_hw_frame_abandon(&frame);
      return 0;
   }
   bps = frame.pixfmt == IMAGE_PIXFMT_P010 ? 2 : 1;
   if (!take_hw(&capture, opened, &frame)) goto done;
   bytes = (size_t)capture.w * capture.h * bps;
   at = (bytes + bytes / 2) * expected;
   if (at > reference_size || bytes + bytes / 2 > reference_size - at ||
       capture.n != 1 || !capture.y[0] || !capture.uv[0] ||
       memcmp(capture.y[0], reference + at, bytes) ||
       memcmp(capture.uv[0], reference + at + bytes, bytes / 2)) goto done;
   ok = 1;
done:
   frames_free(&capture);
   return ok;
}

#include "av1_core_decode.h"

int main(int argc, char **argv)
{
   static const D3D_FEATURE_LEVEL levels[2] =
      { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
   frames_t hw;
   opened_t opened;
   rav1_hw_ops_t ops;
   rav1_hw_backend_t factory;
   rav1_obu_t obu;
   uint8_t *data = NULL, *reference = NULL;
   void *stream = NULL;
   enum image_type_enum type = IMAGE_TYPE_NONE;
   size_t size = 0, reference_size = 0, offset = 0, consumed = 0, plane_size;
   unsigned i, bps = 1, displayed = 0;
   int ret = -1, result = 1, initialized = 0;
   bool retain = true;
   memset(&hw, 0, sizeof(hw)); memset(&opened, 0, sizeof(opened));
   memset(&ops, 0, sizeof(ops)); memset(&factory, 0, sizeof(factory));
   setvbuf(stdout, NULL, _IONBF, 0);
   if (argc != 3) { puts("usage: av1_decode_test stream.obu reference.nv12-or-p010"); return 1; }
   printf("AV1 fixture=%s retain=%u\n", argv[1], (unsigned)retain);
   if (strstr(argv[1], "smpte2084")) av1_expected_transfer = 16;
   if (strstr(argv[1], "arib-std-b67")) av1_expected_transfer = 18;
   av1_expected_range = strstr(argv[1], "-pc.") ? 2 : 1;
   if (FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
         levels, 2, D3D11_SDK_VERSION, &dev, NULL, &ctx)) ||
       FAILED(dev->lpVtbl->QueryInterface(dev, &d3d11vd_iid_device5, (void**)&dev5)) ||
       FAILED(ctx->lpVtbl->QueryInterface(ctx, &d3d11vd_iid_context4, (void**)&ctx4)))
   { puts("UNAVAILABLE D3D11.4 device"); result = 77; goto done; }
   if (!d3d11_video_decode_init_formats(dev, IMAGE_PIXFMT_NV12 | IMAGE_PIXFMT_P010 |
         (getenv("D3D11_TEST_SHARED_DPB") ? D3D11_VIDEO_DECODE_SHARED_DPB : 0)))
   { puts("UNAVAILABLE shared decoder backend"); result = 77; goto done; }
   data = read_fixture_file(argv[1], &size);
   reference = read_fixture_file(argv[2], &reference_size);
   if (!data || !reference) goto done;
   if (strstr(argv[1], ".webm")) type = IMAGE_TYPE_WEBM;
   if (strstr(argv[1], ".mp4")) type = IMAGE_TYPE_MP4;
   if (type != IMAGE_TYPE_NONE && getenv("AV1_TEST_CORE"))
   {
      if (!read_av1_core(argv[1], &hw, &opened, &bps)) goto done;
      displayed = (unsigned)hw.n;
      goto compare;
   }
   if (type != IMAGE_TYPE_NONE)
   {
      unsigned polls;
      int more = 0;
      stream = image_transfer_anim_stream_new_hw_avail(data, size, size,
            type, &more, NULL, NULL);
      if (!stream) { puts("FAIL AV1 container hardware stream open"); goto done; }
      if (getenv("AV1_TEST_SEEK"))
      {
         if (!av1_container_seek_frame(stream, type, &opened,
                  reference, reference_size, 9)) goto done;
         puts("AV1 container early seek before first display: pixels and metadata PASS");
         image_transfer_anim_stream_rewind(stream, type);
      }
      for (polls = 0; polls < 10000; polls++)
      {
         int duration = 0;
         const uint32_t *pixels = image_transfer_anim_stream_next(stream, type, &duration);
         if (pixels)
         {
            video_hw_frame_t frame;
            if (!image_transfer_anim_stream_hw_frame(stream, type, &frame)) goto done;
            if (!av1_color_matches(&frame)) { video_hw_frame_abandon(&frame); goto done; }
            bps = frame.pixfmt == IMAGE_PIXFMT_P010 ? 2 : 1;
            if (!read_av1_frame(&hw, &opened, &frame, displayed))
            { video_hw_frame_abandon(&frame); goto done; }
            displayed++;
         }
         else if (!image_transfer_anim_stream_waiting(stream, type))
            break;
         else
            Sleep(1);
      }
      if (polls == 10000 || !displayed) goto done;
      if (getenv("AV1_TEST_SEEK"))
      {
         unsigned attempt;
         if (displayed < 10 || !image_transfer_anim_stream_seekable(stream, type))
            goto done;
         for (attempt = 0; attempt < 3; attempt++)
         {
            unsigned expected = attempt == 1 ? 0 : 9;
            if (!av1_container_seek_frame(stream, type, &opened,
                     reference, reference_size, expected)) goto done;
         }
         puts("AV1 container seeks 9 to 0 to 9: independent pixels and metadata PASS");
      }
      image_transfer_anim_stream_free(stream, type); stream = NULL;
      goto compare;
   }
   while ((ret = rav1_obu_next(&obu, data, size, &offset, false)) == 1)
   {
      if (obu.type == 1)
      {
         if (!rav1_sequence_parse(&av1_sequence, obu.payload, obu.payload_size)) goto done;
         if (!initialized)
         {
            if (!av1_sequence_refusals(&av1_sequence)) goto done;
            if (!rav1_hw_backend_open(&av1_sequence, 9, &ops, &factory))
            { puts("UNAVAILABLE requested AV1 decoder"); result = 77; goto done; }
            if (!rav1_hw_session_init(&av1_session, &av1_sequence, &ops, 9)) goto done;
            bps = av1_sequence.bit_depth == 10 ? 2 : 1;
            initialized = 1;
         }
         else if (memcmp(&av1_sequence, &av1_session.sequence, sizeof(av1_sequence))) goto done;
      }
      if (obu.type != 3 && obu.type != 6) continue;
      if (!initialized) goto done;
      ret = rav1_hw_session_submit(&av1_session, obu.data, obu.size);
      if (ret) { printf("AV1 submit failed %d at byte %u\n", ret, (unsigned)(obu.data - data)); goto done; }
      /* Accepted packets are no longer borrowed from the caller. */
      memset((void*)obu.data, 0, obu.size);
      if (av1_session.output_slot >= 0)
      {
         video_hw_frame_t frame;
         ret = rav1_hw_session_output(&av1_session, &frame);
         if (retain && displayed == 4)
         {
            d3d11vd_dec_t *decoder = (d3d11vd_dec_t*)ops.decoder;
            uint32_t feedback = decoder->feedback;
            uint64_t ready = decoder->ready_value;
            int slot = av1_session.output_slot;
            if (ret != 1 || rav1_hw_session_output(&av1_session, &frame) != 1 ||
                decoder->feedback != feedback || decoder->ready_value != ready ||
                av1_session.output_slot != slot)
            { puts("FAIL AV1 held outputs must preserve BUSY state"); goto done; }
            if (!read_av1_frame(&hw, &opened, &av1_held[1], 1)) goto done;
            memset(&av1_held[1], 0, sizeof(av1_held[1]));
            ret = export_av1_frame(&frame);
            puts("AV1 output BUSY preserves state; returning non-oldest lease resumes export");
         }
         else if (ret == 1)
            ret = export_av1_frame(&frame);
         if (ret) { printf("AV1 export failed %d\n", ret); goto done; }
         if (retain && displayed < 4) av1_held[displayed] = frame;
         else if (!read_av1_frame(&hw, &opened, &frame, displayed))
         { video_hw_frame_abandon(&frame); goto done; }
         displayed++;
      }
   }
   if (ret || displayed < 5) goto done;
   factory.close(&ops);
   for (i = 0; i < 4; i++)
      if (av1_held[i].image)
      {
         if (!read_av1_frame(&hw, &opened, &av1_held[i], i)) goto done;
         memset(&av1_held[i], 0, sizeof(av1_held[i]));
      }
   if ((unsigned)hw.n != displayed) goto done;
compare:
   plane_size = (size_t)hw.w * hw.h * bps;
   if ((plane_size + plane_size / 2) * displayed != reference_size) goto done;
   for (i = 0; i < (unsigned)hw.n; i++)
   {
      size_t pixel, differences = 0;
      consumed = (plane_size + plane_size / 2) * av1_order[i];
      if (plane_size + plane_size / 2 > reference_size - consumed || !hw.y[i] || !hw.uv[i]) goto done;
      for (pixel = 0; pixel < plane_size; pixel++)
         if (hw.y[i][pixel] != reference[consumed + pixel]) differences++;
      consumed += plane_size;
      for (pixel = 0; pixel < plane_size / 2; pixel++)
         if (hw.uv[i][pixel] != reference[consumed + pixel]) differences++;
      consumed += plane_size / 2;
      printf("AV1 picture=%u depth=%u differences=%u\n", av1_order[i], bps == 2 ? 10 : 8,
            (unsigned)differences);
      if (differences) goto done;
   }
   puts(type == IMAGE_TYPE_NONE
         ? "PASS av1_decode_test (independent pixels, BUSY, non-oldest release, leases after close)"
         : "PASS av1_decode_test (container stream, automatic backend, independent shared-frame pixels)");
   result = 0;
done:
   if (stream) image_transfer_anim_stream_free(stream, type);
   for (i = 0; i < 4; i++)
      if (av1_held[i].image) video_hw_frame_abandon(&av1_held[i]);
   opened_clear(&opened); frames_free(&hw);
   if (ops.decoder) factory.close(&ops);
   if (d3d11vd.up) d3d11_video_decode_deinit();
   D3D11VD_RELEASE(ctx4); D3D11VD_RELEASE(dev5);
   D3D11VD_RELEASE(ctx); D3D11VD_RELEASE(dev);
   free(data); free(reference);
   return result;
}
#else
int main(void)
{
   puts("UNAVAILABLE D3D11 AV1 hardware on this platform");
   return 77;
}
#endif
