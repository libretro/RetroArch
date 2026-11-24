/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/image.h>
#include <formats/rwebp.h>
#include <formats/video_player.h>
#include <retro_atomic.h>
#include <retro_timers.h>
#include <rthreads/rthreads.h>
#include <time/rtime.h>

static const unsigned char anim_webp[] = {
   0x52, 0x49, 0x46, 0x46, 0xb4, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50,
   0x56, 0x50, 0x38, 0x58, 0x0a, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
   0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x41, 0x4e, 0x49, 0x4d, 0x06, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x4e, 0x4d, 0x46,
   0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00,
   0x00, 0x03, 0x00, 0x00, 0x32, 0x00, 0x00, 0x02, 0x56, 0x50, 0x38, 0x4c,
   0x0f, 0x00, 0x00, 0x00, 0x2f, 0x03, 0xc0, 0x00, 0x00, 0x07, 0x10, 0xfd,
   0x8f, 0xfe, 0x07, 0x22, 0xa2, 0xff, 0x01, 0x00, 0x41, 0x4e, 0x4d, 0x46,
   0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00,
   0x00, 0x03, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x56, 0x50, 0x38, 0x4c,
   0x0f, 0x00, 0x00, 0x00, 0x2f, 0x03, 0xc0, 0x00, 0x00, 0x07, 0x10, 0xd1,
   0xff, 0xfe, 0x07, 0x22, 0xa2, 0xff, 0x01, 0x00, 0x41, 0x4e, 0x4d, 0x46,
   0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00,
   0x00, 0x03, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x56, 0x50, 0x38, 0x4c,
   0x0f, 0x00, 0x00, 0x00, 0x2f, 0x03, 0xc0, 0x00, 0x00, 0x07, 0xd0, 0xff,
   0x88, 0xfe, 0x07, 0x22, 0xa2, 0xff, 0x01, 0x00
};
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

static void independent(unsigned char *blob)
{
   size_t p;
   for (p = 12; p + 8 <= sizeof(anim_webp);)
   {
      size_t n = blob[p+4] | ((size_t)blob[p+5] << 8)
            | ((size_t)blob[p+6] << 16) | ((size_t)blob[p+7] << 24);
      if (!memcmp(blob + p, "ANMF", 4)) blob[p + 8 + 15] |= 2;
      p += 8 + n + (n & 1);
   }
}

static void decoder_lease(void)
{
   unsigned char blob[sizeof(anim_webp)];
   uint32_t saved[16];
   const uint32_t *pixels;
   void *owner;
   rwebp_anim_stream_t *s;
   int duration;
   memcpy(blob, anim_webp, sizeof(blob));
   independent(blob);
   s = rwebp_anim_stream_open(blob, sizeof(blob));
   CHECK(s && !rwebp_anim_stream_take_pixels(s));
   pixels = rwebp_anim_stream_next(s, &duration);
   CHECK(pixels && duration == 50);
   memcpy(saved, pixels, sizeof(saved));
   owner = rwebp_anim_stream_take_pixels(s);
   CHECK(owner == pixels && !rwebp_anim_stream_take_pixels(s));
   CHECK(rwebp_anim_stream_next(s, &duration));
   rwebp_anim_stream_set_argb(s, 1);
   rwebp_anim_stream_rewind(s);
   CHECK(rwebp_anim_stream_next(s, &duration));
   rwebp_anim_stream_close(s);
   CHECK(!memcmp(saved, owner, sizeof(saved)));
   free(owner);
   s = rwebp_anim_stream_open(anim_webp, sizeof(anim_webp));
   CHECK(s && rwebp_anim_stream_next(s, &duration));
   CHECK(!rwebp_anim_stream_take_pixels(s));
   rwebp_anim_stream_close(s);
}

#ifdef HAVE_THREADS
struct sink_state
{
   uint32_t slots[2][16], saved[2][16];
   const uint32_t *held[2];
   unsigned shown;
   retro_atomic_int_t copies;
   int borrow, accept;
};
static int start(void *user, unsigned w, unsigned h, unsigned slots)
{
   struct sink_state *k = (struct sink_state*)user;
   CHECK(w == 4 && h == 4 && slots == 2);
   return VIDEO_PLAYER_RGBA | (k->accept ? VIDEO_PLAYER_BORROW_PIXELS : 0);
}
static uint32_t *begin(void *user, unsigned slot)
{ CHECK(slot < 2); return ((struct sink_state*)user)->slots[slot]; }
static enum video_player_present present(void *user, const video_player_frame_t *fr)
{
   struct sink_state *k = (struct sink_state*)user;
   unsigned slot = fr->slot;
   CHECK(slot < 2 && !k->held[slot]);
   CHECK(k->borrow ? fr->pixels != NULL : fr->pixels == NULL);
   CHECK(!fr->planar && !fr->hw);
   k->held[slot] = fr->pixels ? fr->pixels : k->slots[slot];
   memcpy(k->saved[slot], k->held[slot], sizeof(k->saved[slot]));
   k->shown++;
   return VIDEO_PLAYER_PRESENT_QUEUED;
}
static void decoded(void *user, enum video_player_how how)
{
   if (how == VIDEO_PLAYER_COPY || how == VIDEO_PLAYER_SWIZZLE)
      retro_atomic_fetch_add_int(&((struct sink_state*)user)->copies, 1);
}
static void worker_lease(int borrow, int separate)
{
   unsigned char blob[sizeof(anim_webp)];
   struct sink_state k;
   video_player_sink_t sink;
   video_player_t *p;
   rwebp_anim_stream_t *s;
   unsigned i, j;
   memset(&k, 0, sizeof(k)); memset(&sink, 0, sizeof(sink));
   memset(k.slots, 0x55, sizeof(k.slots)); k.accept = borrow; k.borrow = borrow && separate;
   memcpy(blob, anim_webp, sizeof(blob));
   if (separate) independent(blob);
   s = rwebp_anim_stream_open(blob, sizeof(blob)); CHECK(s);
   sink.user = &k; sink.start = start; sink.slot_begin = begin;
   sink.present = present; sink.decoded = decoded;
   p = video_player_new(s, IMAGE_TYPE_WEBP, 1, &sink, 0); CHECK(p);
   for (i = 0; i < 2000 && k.shown < 2; i++)
   {
      CHECK(video_player_poll(p, (int64_t)i * 10000) == VIDEO_PLAYER_PLAYING);
      retro_sleep(1);
   }
   CHECK(k.shown == 2 && k.held[0] && k.held[1]);
   CHECK(k.borrow ? retro_atomic_load_acquire_int(&k.copies) == 0
                : retro_atomic_load_acquire_int(&k.copies) >= 2);
   for (i = 0; i < 2; i++)
   {
      CHECK(!memcmp(k.held[i], k.saved[i], sizeof(k.saved[i])));
      if (k.borrow)
         for (j = 0; j < 16; j++) CHECK(k.slots[i][j] == 0x55555555u);
   }
   if (k.borrow) CHECK(!video_player_stop(p));
   video_player_slot_released(p, 0); video_player_slot_released(p, 1);
   for (i = 0; i < 2000 && !video_player_stop(p); i++) retro_sleep(1);
   CHECK(i < 2000);
   video_player_free(p); rwebp_anim_stream_close(s);
   video_player_worker_deinit();
}
#endif
int main(void)
{
   decoder_lease();
#ifdef HAVE_THREADS
   worker_lease(1, 1); worker_lease(0, 1); worker_lease(1, 0);
#endif
   puts("PASS: WebP canvas ownership, two worker slots and copy fallback");
   return 0;
}
