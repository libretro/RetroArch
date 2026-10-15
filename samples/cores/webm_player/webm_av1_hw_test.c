/* SPDX-License-Identifier: MIT */
#include <formats/rav1_hw.h>
#include "../../../libretro-common/samples/formats/av1/rav1_webm_fixture.h"
#include "../../../cores/libretro-media/media_core.c"

static unsigned failures, opened, closed, live, decoded, exported, abandoned;
static unsigned taken, stopped, ticks, ready, sink_busy, decode_busy,
    output_busy, refuse;
static video_hw_frame_t held, retry;
static const char *fixture_path;
static unsigned container;
static uint64_t pcm_frames, pcm_nonzero;
static uint32_t pcm_hash;
#define CHECK(c)                                                               \
   do                                                                          \
   {                                                                           \
      if (!(c))                                                                \
      {                                                                        \
         fprintf(stderr, "line %u: %s\n", (unsigned)__LINE__, #c);             \
         failures++;                                                           \
      }                                                                        \
   } while (0)

typedef struct mock
{
   const uint8_t *packet;
   size_t size;
   unsigned refs;
   uint8_t first;
} mock_t;

static void release(mock_t *m)
{
   if (!--m->refs)
   {
      live--;
      free(m);
   }
}

static void abandon(void *owner, uint64_t done, uint64_t value)
{
   CHECK(done == 2 && value);
   abandoned++;
   release((mock_t *)owner);
}

static int decode(void *owner, const uint8_t *data, size_t size,
                  const rav1_sequence_t *seq, const rav1_hw_picture_t *picture)
{
   mock_t *m = (mock_t *)owner;
   CHECK(size && seq->max_width == 64 && picture->slot >= 0);
   if (m->packet)
      CHECK(m->packet == data && m->size == size && m->first == data[0]);
   if (decode_busy)
   {
      m->packet = data;
      m->size = size;
      m->first = data[0];
      decode_busy--;
      return 1;
   }
   m->packet = NULL;
   decoded++;
   return 0;
}

static int output(void *owner, unsigned slot, const rav1_grain_t *grain,
                  video_hw_frame_t *frame)
{
   mock_t *m = (mock_t *)owner;
   CHECK(slot < 9 && !grain->apply);
   if (output_busy)
   {
      output_busy--;
      return 1;
   }
   m->refs++;
   frame->image = 100 + ++exported;
   frame->width = frame->height = 64;
   frame->pixfmt = IMAGE_PIXFMT_NV12;
   frame->done = 2;
   frame->done_value = exported;
   frame->owner = m;
   frame->abandon = abandon;
   return 0;
}

static bool open_backend(void *user, const rav1_sequence_t *seq, unsigned slots,
                         rav1_hw_ops_t *ops)
{
   mock_t *m;
   (void)user;
   CHECK(seq->max_width == 64 && slots == 9);
   if (refuse)
      return false;
   m = (mock_t *)calloc(1, sizeof(*m));
   if (!m)
      return false;
   m->refs = 1;
   opened++;
   live++;
   ops->decoder = m;
   ops->decode = decode;
   ops->output = output;
   return true;
}

static void close_backend(rav1_hw_ops_t *ops)
{
   closed++;
   release((mock_t *)ops->decoder);
}
static const rav1_hw_backend_t backend = {NULL, open_backend, close_backend};

static bool sink(void *user, const video_hw_frame_t *frame)
{
   (void)user;
   if (!frame)
      return ready != 0;
   CHECK(frame->image && frame->width == 64 && frame->height == 64);
   CHECK(frame->pixfmt == IMAGE_PIXFMT_NV12);
   if (retry.image)
      CHECK(!memcmp(&retry, frame, sizeof(retry)));
   if (sink_busy)
   {
      retry = *frame;
      sink_busy--;
      return false;
   }
   memset(&retry, 0, sizeof(retry));
   if (held.image)
      video_hw_frame_abandon(&held);
   held = *frame;
   taken++;
   return true;
}

static bool env(unsigned cmd, void *data)
{
   if (cmd == RETRO_ENVIRONMENT_SET_PIXEL_FORMAT)
      return *(enum retro_pixel_format *)data == RETRO_PIXEL_FORMAT_XRGB8888;
   if (cmd == RETRO_ENVIRONMENT_SHUTDOWN)
   {
      stopped++;
      return true;
   }
   return false;
}

static void video(const void *data, unsigned w, unsigned h, size_t pitch)
{
   CHECK(!data && w == 64 && h == 64 && !pitch);
   ticks++;
}

static void poll(void)
{
}
static int16_t input(unsigned port, unsigned device, unsigned index,
                     unsigned id)
{
   (void)port;
   (void)device;
   (void)index;
   (void)id;
   return 0;
}
static size_t audio(const int16_t *data, size_t frames)
{
   size_t i;
   CHECK(data != NULL);
   pcm_frames += frames;
   for (i = 0; i < frames * 2; i++)
   {
      pcm_nonzero += data[i] != 0;
      pcm_hash = (pcm_hash ^ (uint16_t)data[i]) * UINT32_C(16777619);
   }
   return frames;
}

static bool load(void)
{
   struct retro_game_info game;
   bool ok;
   unsigned before = opened;
   stopped = 0;
   memset(&game, 0, sizeof(game));
   game.path = fixture_path;
   ok = retro_load_game(&game);
   CHECK(ok);
   CHECK(opened == before);
   if (ok)
   {
      int required = media_player.webm_hw_required;
#ifdef HAVE_RMP4
      required = required || media_player.mp4_hw_required;
#endif
      CHECK(required && !media_player.fb);
      CHECK(!media_player.hw_started && !media_player.hw_video);
      CHECK(!media_set_video_hw_refresh(NULL, NULL));
   }
   return ok;
}

static void unload(void)
{
   retro_unload_game();
   if (held.image)
   {
      CHECK(live > 0);
      video_hw_frame_abandon(&held);
      memset(&held, 0, sizeof(held));
   }
   memset(&retry, 0, sizeof(retry));
   CHECK(!live && opened == closed && exported == abandoned);
}

static void test_container(void)
{
   FILE *file;
   unsigned i, before, accepted;
   uint8_t state[40];
   if (container)
      make_webm(container == 2, 1, 1, 0);
   else
      make_mp4(1);
   file = fopen(fixture_path, "wb");
   if (!file)
   {
      CHECK(file != NULL);
      return;
   }
   CHECK(fwrite(bytes, 1, used, file) == used);
   CHECK(!fclose(file));
   retro_set_environment(env);
   retro_set_video_refresh(video);
   retro_set_input_poll(poll);
   retro_set_input_state(input);
   retro_set_audio_sample_batch(audio);
   rav1_set_hw_backend(NULL);
   CHECK(media_set_video_hw_refresh(sink, NULL));
   if (!load())
      goto done;
   CHECK(!media_player.hw_started && !media_player.eof);
   if (container)
   {
      rwebm_packet pkt;
      memset(&pkt, 0, sizeof(pkt));
      pkt.data = packet;
      pkt.size = 7;
      CHECK(!media_packet_shown(&media_player, &pkt));
      pkt.size = sizeof(packet);
      CHECK(media_packet_shown(&media_player, &pkt));
      pkt.data = config + 4;
      pkt.size = sizeof(config) - 4;
      CHECK(!media_packet_shown(&media_player, &pkt));
   }
   rav1_set_hw_backend(&backend);
   ready = 1;
   decode_busy = output_busy = sink_busy = 2;
   for (i = 0; i < 32 && !stopped; i++)
   {
      accepted = decoded;
      before = media_player.hw_frame.image != 0;
      retro_run();
      if (before)
         CHECK(accepted == decoded);
   }
   CHECK(stopped && taken && ticks && !media_player.hw_frame.image);
   unload();
   before = taken;
   if (!load())
      goto done;
   retro_run();
   CHECK(taken == before + 1 && media_player.vshown == 1);
   CHECK(retro_serialize(state, sizeof(state)));
   retro_run();
   CHECK(taken == before + 2 && media_player.vshown == 2);
   CHECK(retro_unserialize(state, sizeof(state)));
   CHECK(media_player.hw_frame.image || media_player.webm_seek_slot >= 0);
   retro_run();
   CHECK(taken > before + 2);
   retro_reset();
   CHECK(!media_player.fb && media_player.hw_video && !media_player.eof);
   before = taken;
   retro_run();
   CHECK(taken == before + 1);
   unload();
   if (!load())
      goto done;
   sink_busy = 100;
   retro_run();
   CHECK(media_player.hw_frame.image);
   before = abandoned;
   media_suspend_video_hw();
   CHECK(!media_player.hw_frame.image && abandoned > before);
   memset(&retry, 0, sizeof(retry));
   CHECK(!media_player.hw_started && !stopped);
   ready = 1;
   sink_busy = 0;
   retro_run();
   CHECK(media_player.hw_video);
   unload();
   if (container)
   {
      if (!load())
         goto done;
      CHECK(retro_unserialize(state, sizeof(state)));
      CHECK(!media_player.hw_started && media_player.hw_restart);
      before = taken;
      retro_run();
      CHECK(taken == before + 1 && media_player.vshown == 1);
      media_suspend_video_hw();
      retro_reset();
      CHECK(!media_player.hw_restart);
      retro_run();
      CHECK(media_player.hw_video && media_player.vshown == 1);
      unload();
   }
   refuse = 1;
   if (!load())
      goto done;
   retro_run();
   CHECK(stopped && !media_player.hw_video && !media_player.fb);
   unload();
   refuse = 0;
   ready = 0;
   if (!load())
      goto done;
   retro_run();
   CHECK(stopped && !media_player.hw_video && !media_player.fb);
   unload();
   ready = 1;
   rav1_set_hw_backend(NULL);
   stopped = 0;
   {
      struct retro_game_info game;
      memset(&game, 0, sizeof(game));
      game.path = fixture_path;
      CHECK(retro_load_game(&game));
      retro_run();
      CHECK(stopped && !media_player.hw_video && !media_player.fb);
      retro_unload_game();
   }
   rav1_set_hw_backend(&backend);
   CHECK(media_set_video_hw_refresh(NULL, NULL));
   {
      struct retro_game_info game;
      memset(&game, 0, sizeof(game));
      game.path = fixture_path;
      CHECK(!retro_load_game(&game));
      retro_unload_game();
   }
done:
   retro_unload_game();
   rav1_set_hw_backend(NULL);
   remove(fixture_path);
}

static void test_audio(const char *path)
{
   uint8_t state[40];
   unsigned i, before;
   uint32_t expected_hash;
   uint64_t expected_frames;
   fixture_path = path;
   rav1_set_hw_backend(&backend);
   CHECK(media_set_video_hw_refresh(sink, NULL));
   ready = 1;
   if (!load())
      return;
   CHECK(media_player.webm_hw_required && !media_player.fb);
#ifdef MEDIA_HAVE_AUDIO
   CHECK(media_player.actx != NULL);
#endif
   pcm_frames = pcm_nonzero = 0;
   pcm_hash = UINT32_C(2166136261);
   for (i = 0; i < 512 && !stopped; i++)
      retro_run();
   printf("%s: frames=%lu nonzero=%lu slot=%ld\n", path,
         (unsigned long)pcm_frames, (unsigned long)pcm_nonzero,
         (long)media_player.vshown);
   CHECK(stopped && media_player.vshown == 24);
#ifdef MEDIA_HAVE_AUDIO
   CHECK(pcm_frames >= 90000 && pcm_frames <= 105000);
   CHECK(pcm_nonzero >= 170000 && pcm_nonzero <= 200000);
#else
   CHECK(pcm_frames >= 90000 && !pcm_nonzero);
#endif
   retro_reset();
   for (i = 0; i < 6; i++)
      retro_run();
   CHECK(retro_serialize(state, sizeof(state)));
   pcm_frames = 0;
   pcm_hash = UINT32_C(2166136261);
   retro_run();
   expected_frames = pcm_frames;
   expected_hash = pcm_hash;
   CHECK(expected_frames > 0);
   for (i = 0; i < 2; i++)
   {
      if (i)
      {
         unload();
         if (!load())
            return;
      }
      CHECK(retro_unserialize(state, sizeof(state)));
      pcm_frames = 0;
      pcm_hash = UINT32_C(2166136261);
      before = taken;
      retro_run();
      CHECK(taken == before + 1);
      printf("restore %u: frames=%lu/%lu hash=%08x/%08x\n", i,
            (unsigned long)pcm_frames, (unsigned long)expected_frames,
            pcm_hash, expected_hash);
      CHECK(pcm_frames == expected_frames && pcm_hash == expected_hash);
      CHECK(!media_player.fb);
   }
   unload();
}

int main(void)
{
   container = 1;
#ifdef HAVE_RMP4
   container = 0;
#endif
   for (; container < 3; container++)
   {
      fixture_path = container ? "av1-core-fixture.webm"
            : "av1-core-fixture.mp4";
      test_container();
   }
   test_audio("fixtures/av1-opus.webm");
   test_audio("fixtures/av1-vorbis.webm");
   printf("AV1 core hardware handoff: %u failures\n", failures);
   return failures ? 1 : 0;
}
