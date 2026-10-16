/* SPDX-License-Identifier: MIT */
#include <formats/rvp9_hw.h>
#include "../../../cores/libretro-media/media_core.c"

static unsigned failures, opens, closes, lives, submitted, exported, retired;
static unsigned taken, cpu_frames, gpu_ticks, sink_busy, ready = 1, refuse;
static unsigned stopped;
static uint64_t pcm_frames, pcm_nonzero;
static video_hw_frame_t held;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); failures++; } } while (0)

typedef struct mock
{
   const uint8_t *packet;
   size_t size;
   unsigned refs, w, h, output_busy;
   uint8_t first;
} mock_t;

static void release(mock_t *m)
{
   if (!--m->refs)
   {
      lives--;
      free(m);
   }
}

static void abandon(void *owner, uint64_t done, uint64_t value)
{
   (void)done; (void)value;
   retired++;
   release((mock_t*)owner);
}

static int decode(void *owner, const uint8_t *packet, size_t size,
      const rvp9_hw_picture_t *picture)
{
   mock_t *m = (mock_t*)owner;
   CHECK(picture->slot >= 0 && size > 0);
   if (!m->packet)
   {
      m->packet = packet; m->size = size; m->first = packet[0];
      return 1;
   }
   CHECK(packet == m->packet && size == m->size && packet[0] == m->first);
   m->packet = NULL;
   submitted++;
   return 0;
}

static int output(void *owner, unsigned slot, video_hw_frame_t *out)
{
   mock_t *m = (mock_t*)owner;
   if (!m->output_busy)
   {
      m->output_busy = 1;
      return 1;
   }
   m->output_busy = 0; m->refs++;
   memset(out, 0, sizeof(*out));
   out->image = ++exported; out->wait = slot + 1;
   out->width = m->w; out->height = m->h;
   out->pixfmt = IMAGE_PIXFMT_NV12;
   out->owner = m; out->abandon = abandon;
   return 0;
}

static bool open_backend(void *user, const rvp9_hdr *h, unsigned slots,
      rvp9_hw_ops_t *ops)
{
   mock_t *m;
   (void)user;
   CHECK(slots == 10 && h->profile == 0 && h->bit_depth == 8);
   if (refuse)
      return false;
   m = (mock_t*)calloc(1, sizeof(*m));
   if (!m)
      return false;
   m->refs = 1; m->w = h->w; m->h = h->h;
   lives++; opens++;
   ops->decoder = m; ops->decode = decode; ops->output = output;
   return true;
}

static void close_backend(rvp9_hw_ops_t *ops)
{
   closes++;
   release((mock_t*)ops->decoder);
}
static const rvp9_hw_backend_t backend = {NULL, open_backend, close_backend};

static bool sink(void *user, const video_hw_frame_t *frame)
{
   (void)user;
   if (!frame)
      return ready != 0;
   CHECK(frame->image && frame->pixfmt == IMAGE_PIXFMT_NV12);
   if (sink_busy)
   {
      sink_busy--;
      return false;
   }
   if (held.image)
      video_hw_frame_abandon(&held);
   held = *frame;
   taken++;
   return true;
}

static bool env(unsigned cmd, void *data)
{
   if (cmd == RETRO_ENVIRONMENT_SET_PIXEL_FORMAT)
      return *(enum retro_pixel_format*)data == RETRO_PIXEL_FORMAT_XRGB8888;
   if (cmd == RETRO_ENVIRONMENT_SHUTDOWN)
   {
      stopped++;
      return true;
   }
   return false;
}
static void video(const void *data, unsigned w, unsigned h, size_t pitch)
{
   CHECK(w == 64 && h == 48);
   if (media_player.webmvs)
   {
      CHECK(!data && !pitch);
      gpu_ticks++;
   }
   else
   {
      CHECK(data && pitch == w * 4);
      cpu_frames++;
   }
}
static void poll(void) { }
static int16_t input(unsigned port, unsigned device, unsigned index, unsigned id)
{ (void)port; (void)device; (void)index; (void)id; return 0; }
static size_t audio(const int16_t *data, size_t n)
{
   size_t i;
   CHECK(data != NULL);
   pcm_frames += n;
   if (data)
      for (i = 0; i < n * 2; i++)
         if (data[i]) pcm_nonzero++;
   return n;
}

static int load(void)
{
   struct retro_game_info game;
   game.data = NULL; game.size = 0; game.meta = NULL;
   game.path = "fixtures/vp9-vorbis.webm";
   stopped = 0;
   CHECK(retro_load_game(&game));
   if (!media_player.webm)
      return 0;
   CHECK(!media_player.webmvs && !media_player.hw_started);
   CHECK(!media_player.fb);
   return 1;
}
static void finish_seek(void)
{
   unsigned i;
   for (i = 0; i < 512 && media_player.webm_seek_slot >= 0; i++)
      retro_run();
   CHECK(media_player.webm_seek_slot < 0 && media_player.webmvs);
}
static void unload(void)
{
   retro_unload_game();
   if (held.image)
   {
      CHECK(lives == 1);
      video_hw_frame_abandon(&held);
      memset(&held, 0, sizeof(held));
   }
   CHECK(!lives && opens == closes && exported == retired);
}

static void check_software_skip(const char *path)
{
   void *data = NULL;
   int64_t len = 0;
   rwebm_video_stream_t *reference, *skipped;
   unsigned w, h, i;
   uint32_t *canvas;
   const uint32_t *expected = NULL, *actual;
   int reference_ms = 0, skipped_ms = 0;
   CHECK(filestream_read_file(path, &data, &len) && len > 0);
   if (!data) return;
   reference = rwebm_video_stream_open((const uint8_t*)data, (size_t)len);
   skipped = rwebm_video_stream_open((const uint8_t*)data, (size_t)len);
   CHECK(reference && skipped);
   if (!reference || !skipped) goto done;
   rwebm_video_stream_get_info(skipped, &w, &h, NULL, NULL);
   canvas = (uint32_t*)malloc((size_t)w * h * 4);
   CHECK(canvas != NULL);
   if (!canvas) goto done;
   memset(canvas, 0xa5, (size_t)w * h * 4);
   rwebm_video_stream_set_output(skipped, canvas);
   for (i = 0; i < 6; i++)
   {
      expected = rwebm_video_stream_next(reference, &reference_ms);
      CHECK(expected != NULL);
      if (i < 5)
      {
         unsigned pixel;
         CHECK(rwebm_video_stream_skip(skipped, &skipped_ms) == 1);
         CHECK(reference_ms == skipped_ms);
         for (pixel = 0; pixel < w * h; pixel++)
            CHECK(canvas[pixel] == UINT32_C(0xa5a5a5a5));
      }
   }
   actual = rwebm_video_stream_next(skipped, &skipped_ms);
   CHECK(actual && expected && reference_ms == skipped_ms);
   if (actual && expected) CHECK(!memcmp(actual, expected, (size_t)w * h * 4));
   free(canvas);
done:
   rwebm_video_stream_close(reference); rwebm_video_stream_close(skipped); free(data);
}

static void check_hidden_timeline(void)
{
   static const uint8_t data[] = {
      0x1a,0x45,0xdf,0xa3,0x80, 0x18,0x53,0x80,0x67,0xff,
      0x16,0x54,0xae,0x6b,0x97, 0xae,0x95, 0xd7,0x81,1, 0x83,0x81,1,
      0x86,0x85,'V','_','V','P','9', 0xe0,0x86,0xb0,0x81,64,0xba,0x81,48,
      0x1f,0x43,0xb6,0x75,0xff, 0xe7,0x81,0,
      0xa3,0x85,0x81,0,0,0x80,0x84,
      0xa3,0x85,0x81,0,40,0x80,0x82,
      0xa3,0x85,0x81,0,80,0x80,0x88,
      0xa3,0x8c,0x81,0,120,0x80,0x84,0x82,0x84,0xc2,1,1,1,0xc2,
      0xa3,0x8c,0x81,0,160,0x80,0x84,0x84,0x84,0xc2,1,1,1,0xc2
   };
   media_player_t p;
   memset(&p, 0, sizeof(p));
   p.codec = RWEBM_CODEC_VP9;
#ifdef MEDIA_HAVE_AUDIO
   p.atrack = -1;
#endif
   p.wgather = rwebm_open_memory(data, sizeof(data));
   CHECK(p.wgather != NULL);
   media_native_pump(&p);
   CHECK(p.nvts == 3);
   if (p.nvts == 3)
      CHECK(p.vts[0] == 40000000 && p.vts[1] == 80000000 && p.vts[2] == 120000000);
   rwebm_close(p.wgather); free(p.vts);
}

int main(void)
{
   unsigned i, before, decoded;
   uint8_t state[40];
   check_software_skip("fixtures/vp9-vorbis.webm");
   check_software_skip("fixtures/vp8-opus.webm");
   check_hidden_timeline();
   rvp9_set_hw_backend(&backend);
   CHECK(media_set_video_hw_refresh(sink, NULL));
   retro_set_environment(env); retro_set_video_refresh(video);
   retro_set_input_poll(poll); retro_set_input_state(input);
   retro_set_audio_sample_batch(audio);
   if (!load()) return 1;
   media_video_hw_ready();
   CHECK(media_player.webmvs && media_player.hw_video && !submitted);
   rwebm_video_stream_set_avail(media_player.webmvs,
         rwebm_video_stream_media_floor(media_player.webmvs));
   retro_run();
   CHECK(!media_player.eof && !submitted
         && rwebm_video_stream_waiting(media_player.webmvs));
   rwebm_video_stream_set_avail(media_player.webmvs,
         data_transfer_avail(media_player.dt));
   sink_busy = 3;
   for (i = 0; i < 512 && !stopped; i++)
   {
      decoded = submitted;
      before = media_player.hw_frame.image != 0;
      retro_run();
      if (before) CHECK(decoded == submitted);
   }
   CHECK(stopped && gpu_ticks && taken && !cpu_frames);
#ifdef MEDIA_HAVE_AUDIO
   CHECK(pcm_nonzero >= 180000 && pcm_nonzero <= 200000 && pcm_frames >= 90000);
#else
   CHECK(!pcm_nonzero);
#endif
   CHECK(media_player.vp9 && !media_player.vp9->buf_y);
   CHECK(!media_player.fb);
   CHECK(retro_serialize(state, sizeof(state)));
   CHECK(retro_unserialize(state, sizeof(state)));
   CHECK(media_player.eof && media_player.webm_seek_slot >= 0);
   finish_seek();
   CHECK(media_player.eof && media_player.webm_hw_shown == media_player.nvts);
   unload();

   if (!load()) return 1;
   media_video_hw_ready();
   before = exported;
   media_seek(&media_player, 750000000);
   CHECK(media_player.webmvs && media_player.webm_seek_slot >= 0);
   finish_seek();
   CHECK(exported == before + 1);
   CHECK(media_player.vpts_ns >= 650000000 && media_player.vpts_ns <= 750000000);
   retro_reset();
   CHECK(media_player.webmvs && media_player.webm_hw_shown == 0);
   sink_busy = 100;
   for (i = 0; i < 512 && !media_player.hw_frame.image; i++) retro_run();
   CHECK(media_player.hw_frame.image);
   CHECK(retro_serialize(state, sizeof(state)));
   before = retired;
   CHECK(retro_unserialize(state, sizeof(state)));
   CHECK(retired > before && !media_player.hw_frame.image);
   sink_busy = 0;
   finish_seek();
   CHECK(media_player.webmvs && media_player.webm_hw_shown > 0);
   before = closes;
   media_suspend_video_hw();
   CHECK(!media_player.webmvs && !media_player.hw_frame.image && closes > before);
   ready = 0;
   retro_run();
   CHECK(cpu_frames && !media_player.webmvs && !media_player.hw_restart);
   media_suspend_video_hw();
   ready = 1;
   before = cpu_frames;
   CHECK(media_player.file_len > 4096);
   data_transfer_discard(media_player.dt, 4096);
   media_player.discarded = 1;
   retro_run();
   CHECK(!media_player.discarded);
   CHECK(media_player.webmvs && media_player.hw_video && cpu_frames == before);
   finish_seek();
   rvp9_set_hw_backend(NULL);
   for (i = 0; i < 5; i++) retro_run();
   CHECK(media_player.webmvs);
   retro_reset();
   CHECK(!media_player.webmvs && !media_player.hw_video);
   before = cpu_frames;
   retro_run();
   CHECK(cpu_frames > before);
   unload();

   rvp9_set_hw_backend(&backend);
   refuse = 1;
   if (!load()) return 1;
   before = cpu_frames;
   for (i = 0; i < 5; i++) retro_run();
   CHECK(!media_player.webmvs && cpu_frames > before);
   CHECK(media_player.fb && media_player.fb[0] != 0);
   unload();
   rvp9_set_hw_backend(NULL);
   CHECK(media_set_video_hw_refresh(NULL, NULL));
   printf("WebM movie GPU playback: %u failures\n", failures);
   return failures ? 1 : 0;
}
