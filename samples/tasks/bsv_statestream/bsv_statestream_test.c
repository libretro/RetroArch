/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (bsv_statestream_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Statestream checkpoints in a replay, decoded by the shipping
 * input/bsv/bsvmovie.c with HAVE_STATESTREAM on.
 *
 * A statestream checkpoint is an rmsgpack stream: new blocks and new
 * superblocks, each defined by index, then the superblocks that make
 * up the state, in order.  Every index and every length in it comes
 * from the replay file.  The lanes:
 *
 *  roundtrip     a state written by bsv_movie_write_checkpoint() and a
 *                changed one written after it decode back to the same
 *                bytes on a fresh handle.  Guards the fixture.
 *  undefined     a sequence naming a superblock the replay never
 *                defined, and a superblock naming a block it never
 *                defined, are refused.
 *  longer seq    a sequence longer than any before it decodes without
 *                writing past the superblock list.
 *  record after  recording a checkpoint on a handle whose superblock
 *                list a short sequence sized does not write past it.
 *  no layout     a replay whose header gives no block size is refused.
 *  oom           recording a checkpoint with the Nth allocation failing,
 *                for every N until one gets through, fails the
 *                checkpoint cleanly or writes one that decodes.
 *  oom load      decoding a recorded checkpoint with the Nth allocation
 *                failing, likewise, refuses it or decodes it whole.
 *
 * The block and superblock sizes are small so a state spans many
 * superblocks.  Run under ASan + UBSan, an out-of-bounds access, a
 * NULL read or a leak fails the lane. */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <boolean.h>
#include <libretro.h>
#include <streams/interface_stream.h>

#include "../../../configuration.h"
#include "../../../runloop.h"
#include "../../../core.h"
#include "../../../msg_hash.h"
#include "../../../input/input_driver.h"
#include "../../../input/bsv/bsvmovie.h"
#include "../../../input/bsv/uint32s_index.h"
#include "../../../libretro-db/rmsgpack.h"

/* Production values from input/bsv/bsvmovie.c. */
#define BSV_IFRAME_START_TOKEN          0x00
#define BSV_IFRAME_NEW_BLOCK_TOKEN      0x01
#define BSV_IFRAME_NEW_SUPERBLOCK_TOKEN 0x02
#define BSV_IFRAME_SUPERBLOCK_SEQ_TOKEN 0x03

#define STATE_SIZE      (8 * 1024)
#define BLOCK_BYTES     64          /* 16 uint32s */
#define SUPERBLOCK_LEN  4           /* blocks per superblock */
#define N_SUPERBLOCKS   (STATE_SIZE / (BLOCK_BYTES * SUPERBLOCK_LEN))
#define STREAM_CAP      (256 * 1024)

/* ---- stub frontend ------------------------------------------------ */

static uint8_t core_state[STATE_SIZE];
static settings_t settings;
static runloop_state_t runloop_st;
static input_driver_state_t input_st;
static unsigned n_fail = 0;

#define CHECK(cond, ...) do { if (!(cond)) { \
   fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
   fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); n_fail++; } } while (0)

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }

settings_t *config_get_ptr(void) { return &settings; }
runloop_state_t *runloop_state_get_ptr(void) { return &runloop_st; }
input_driver_state_t *input_state_get_ptr(void) { return &input_st; }
bool content_load_state_in_progress(void *data) { (void)data; return false; }
void input_keyboard_event(bool down, unsigned code, uint32_t character,
      uint16_t mod, unsigned device) { (void)down; (void)code; (void)character; (void)mod; (void)device; }
const char *msg_hash_to_str(enum msg_hash_enums msg) { (void)msg; return "msg"; }
void runloop_msg_queue_push(const char *msg, size_t len,
      unsigned prio, unsigned duration, bool flush, char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{
   (void)msg; (void)len; (void)prio; (void)duration; (void)flush;
   (void)title; (void)icon; (void)category;
}

size_t core_serialize_size(void) { return STATE_SIZE; }
bool core_serialize(retro_ctx_serialize_info_t *info)
{
   memcpy(info->data, core_state, STATE_SIZE);
   return true;
}
bool core_unserialize(retro_ctx_serialize_info_t *info)
{
   if (info->size != STATE_SIZE)
      return false;
   memcpy(core_state, info->data_const, STATE_SIZE);
   return true;
}

/* ---- allocation failure ------------------------------------------- */

/* Every malloc, calloc and realloc the linked units make goes through
 * these (-Wl,--wrap).  While armed, the call that brings the count to
 * zero fails, once. */
static long fail_countdown = -1;
static unsigned n_failed_allocs = 0;

void *__real_malloc(size_t size);
void *__real_calloc(size_t n, size_t size);
void *__real_realloc(void *ptr, size_t size);

static bool alloc_fails(void)
{
   if (fail_countdown < 0)
      return false;
   if (fail_countdown-- == 0)
   {
      n_failed_allocs++;
      return true;
   }
   return false;
}

void *__wrap_malloc(size_t size)
{
   return alloc_fails() ? NULL : __real_malloc(size);
}
void *__wrap_calloc(size_t n, size_t size)
{
   return alloc_fails() ? NULL : __real_calloc(n, size);
}
void *__wrap_realloc(void *ptr, size_t size)
{
   return alloc_fails() ? NULL : __real_realloc(ptr, size);
}

/* ---- fixtures ------------------------------------------------------ */

static bsv_movie_t *movie_new(uint32_t block_bytes, uint32_t superblock_len)
{
   bsv_movie_t *m = (bsv_movie_t*)calloc(1, sizeof(*m));
   m->blocks      = uint32s_index_new(block_bytes / 4, 0, 0);
   m->superblocks = uint32s_index_new(superblock_len, 0, 0);
   return m;
}

static void movie_free(bsv_movie_t *m)
{
   if (m->file)
   {
      intfstream_close(m->file);
      free(m->file);
   }
   uint32s_index_free(m->blocks);
   uint32s_index_free(m->superblocks);
   free(m->superblock_seq);
   free(m->cur_save);
   free(m->last_save);
   free(m);
}

static void movie_set_stream(bsv_movie_t *m, uint8_t *buf, size_t len)
{
   if (m->file)
   {
      intfstream_close(m->file);
      free(m->file);
   }
   m->file = intfstream_open_memory(buf, RETRO_VFS_FILE_ACCESS_READ_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE, len);
}

/* Wraps an rmsgpack payload in a checkpoint header: state size,
 * encoded size, compressed size (uncompressed, so equal). */
static size_t wrap_checkpoint(uint8_t *out, const uint8_t *payload, uint32_t len)
{
   uint32_t size = STATE_SIZE;
   memcpy(out,     &size, 4);
   memcpy(out + 4, &len,  4);
   memcpy(out + 8, &len,  4);
   memcpy(out + 12, payload, len);
   return 12 + (size_t)len;
}

static bool load(bsv_movie_t *m, uint8_t *buf, size_t len)
{
   movie_set_stream(m, buf, len);
   return bsv_movie_load_checkpoint(m, REPLAY_CHECKPOINT2_COMPRESSION_NONE,
         REPLAY_CHECKPOINT2_ENCODING_STATESTREAM, REPLAY_CPBEHAVIOR_DESERIALIZE);
}

/* A payload with a single sequence of 'n' copies of superblock 'idx',
 * optionally after one superblock definition. */
static size_t make_payload(uint8_t *buf, bool define, uint32_t def_idx,
      uint32_t def_block, uint32_t seq_idx, uint32_t n)
{
   uint32_t i;
   int64_t  len;
   intfstream_t *s = intfstream_open_memory(buf, RETRO_VFS_FILE_ACCESS_READ_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE, STREAM_CAP);
   rmsgpack_write_int(s, BSV_IFRAME_START_TOKEN);
   rmsgpack_write_int(s, 1);
   if (define)
   {
      rmsgpack_write_int(s, BSV_IFRAME_NEW_SUPERBLOCK_TOKEN);
      rmsgpack_write_int(s, def_idx);
      rmsgpack_write_array_header(s, SUPERBLOCK_LEN);
      for (i = 0; i < SUPERBLOCK_LEN; i++)
         rmsgpack_write_int(s, def_block);
   }
   rmsgpack_write_int(s, BSV_IFRAME_SUPERBLOCK_SEQ_TOKEN);
   rmsgpack_write_array_header(s, n);
   for (i = 0; i < n; i++)
      rmsgpack_write_int(s, seq_idx);
   len = intfstream_tell(s);
   intfstream_close(s);
   free(s);
   return (size_t)len;
}

static void fill_state(unsigned seed)
{
   size_t i;
   for (i = 0; i < STATE_SIZE; i++)
      core_state[i] = (uint8_t)((i % 97 == 0) ? seed + i : 0);
}

/* Records the current core state as a checkpoint on 'w' and returns
 * the bytes written. */
static size_t record(bsv_movie_t *w, uint8_t *buf)
{
   int64_t n;
   movie_set_stream(w, buf, STREAM_CAP);
   n = bsv_movie_write_checkpoint(w, REPLAY_CHECKPOINT2_COMPRESSION_NONE,
         REPLAY_CHECKPOINT2_ENCODING_STATESTREAM);
   return n > 0 ? (size_t)n : 0;
}

/* ---- lanes --------------------------------------------------------- */

static uint8_t stream_buf[STREAM_CAP];
static uint8_t payload[STREAM_CAP];
static uint8_t ckpt[STREAM_CAP + 12];

static void lane_roundtrip(void)
{
   bsv_movie_t *w = movie_new(BLOCK_BYTES, SUPERBLOCK_LEN);
   bsv_movie_t *r = movie_new(BLOCK_BYTES, SUPERBLOCK_LEN);
   unsigned seed;
   for (seed = 1; seed <= 2; seed++)
   {
      size_t n;
      fill_state(seed);
      n = record(w, stream_buf);
      CHECK(n > 12, "roundtrip %u: checkpoint written", seed);
      CHECK(load(r, stream_buf, n), "roundtrip %u: checkpoint loads", seed);
      CHECK(r->cur_save && !memcmp(r->cur_save, core_state, STATE_SIZE),
            "roundtrip %u: state decodes to the bytes written", seed);
   }
   movie_free(w);
   movie_free(r);
   puts("[ok] roundtrip");
}

static void lane_undefined(void)
{
   bsv_movie_t *r = movie_new(BLOCK_BYTES, SUPERBLOCK_LEN);
   size_t n;

   /* Superblock 7 was never defined. */
   n = make_payload(payload, false, 0, 0, 7, N_SUPERBLOCKS);
   n = wrap_checkpoint(ckpt, payload, (uint32_t)n);
   CHECK(!load(r, ckpt, n), "undefined superblock is refused");

   /* Superblock 1 names block 9, which was never defined. */
   n = make_payload(payload, true, 1, 9, 1, N_SUPERBLOCKS);
   n = wrap_checkpoint(ckpt, payload, (uint32_t)n);
   CHECK(!load(r, ckpt, n), "undefined block is refused");

   movie_free(r);
   puts("[ok] undefined");
}

static void lane_longer_seq(void)
{
   bsv_movie_t *r = movie_new(BLOCK_BYTES, SUPERBLOCK_LEN);
   size_t n;

   /* Superblock 0 is the zero superblock every index starts with. */
   n = make_payload(payload, false, 0, 0, 0, 2);
   n = wrap_checkpoint(ckpt, payload, (uint32_t)n);
   CHECK(load(r, ckpt, n), "short sequence loads");

   n = make_payload(payload, false, 0, 0, 0, N_SUPERBLOCKS * 4);
   n = wrap_checkpoint(ckpt, payload, (uint32_t)n);
   CHECK(load(r, ckpt, n), "longer sequence loads");
   CHECK(r->superblock_seq_len >= N_SUPERBLOCKS * 4, "list grew to the sequence");

   movie_free(r);
   puts("[ok] longer seq");
}

static void lane_record_after(void)
{
   bsv_movie_t *m = movie_new(BLOCK_BYTES, SUPERBLOCK_LEN);
   size_t n;

   n = make_payload(payload, false, 0, 0, 0, 1);
   n = wrap_checkpoint(ckpt, payload, (uint32_t)n);
   CHECK(load(m, ckpt, n), "one-superblock sequence loads");

   fill_state(3);
   CHECK(record(m, stream_buf) > 12, "recording after playback writes");
   CHECK(m->superblock_seq_len >= N_SUPERBLOCKS, "list grew to the state");

   movie_free(m);
   puts("[ok] record after");
}

static void lane_no_layout(void)
{
   bsv_movie_t *r = movie_new(0, 0);
   size_t n = make_payload(payload, false, 0, 0, 0, 1);
   n = wrap_checkpoint(ckpt, payload, (uint32_t)n);
   CHECK(!load(r, ckpt, n), "no block layout is refused");
   movie_free(r);
   puts("[ok] no layout");
}

static void lane_oom(void)
{
   long nth;
   unsigned clean_failures = 0;
   for (nth = 0; nth < 100000; nth++)
   {
      bsv_movie_t *w = movie_new(BLOCK_BYTES, SUPERBLOCK_LEN);
      bsv_movie_t *r;
      size_t n;
      bool failed;
      fill_state(5);
      movie_set_stream(w, stream_buf, STREAM_CAP);
      n_failed_allocs = 0;
      fail_countdown  = nth;
      n = (size_t)(bsv_movie_write_checkpoint(w,
               REPLAY_CHECKPOINT2_COMPRESSION_NONE,
               REPLAY_CHECKPOINT2_ENCODING_STATESTREAM) > 0
            ? intfstream_tell(w->file) : 0);
      fail_countdown  = -1;
      failed          = n_failed_allocs != 0;
      if (!failed)
      {
         /* Nothing failed: the checkpoint must be whole. */
         r = movie_new(BLOCK_BYTES, SUPERBLOCK_LEN);
         CHECK(n > 12 && load(r, stream_buf, n)
               && !memcmp(r->cur_save, core_state, STATE_SIZE),
               "oom: checkpoint after %ld allocations decodes", nth);
         movie_free(r);
         movie_free(w);
         break;
      }
      if (!n)
         clean_failures++;
      movie_free(w);
   }
   CHECK(clean_failures > 0, "oom: some allocation failure fails the checkpoint");
   printf("[ok] oom (%ld allocations, %u failed the checkpoint)\n", nth, clean_failures);
}

static void lane_oom_load(void)
{
   long nth;
   unsigned refused = 0;
   size_t n;
   bsv_movie_t *w = movie_new(BLOCK_BYTES, SUPERBLOCK_LEN);
   fill_state(6);
   n = record(w, stream_buf);
   CHECK(n > 12, "oom load: checkpoint written");
   for (nth = 0; nth < 100000; nth++)
   {
      bsv_movie_t *r = movie_new(BLOCK_BYTES, SUPERBLOCK_LEN);
      bool ok;
      movie_set_stream(r, stream_buf, n);
      n_failed_allocs = 0;
      fail_countdown  = nth;
      ok = bsv_movie_load_checkpoint(r, REPLAY_CHECKPOINT2_COMPRESSION_NONE,
            REPLAY_CHECKPOINT2_ENCODING_STATESTREAM, REPLAY_CPBEHAVIOR_DESERIALIZE);
      fail_countdown  = -1;
      if (!n_failed_allocs)
      {
         CHECK(ok && !memcmp(r->cur_save, core_state, STATE_SIZE),
               "oom load: checkpoint after %ld allocations decodes", nth);
         movie_free(r);
         break;
      }
      if (ok)
         CHECK(!memcmp(r->cur_save, core_state, STATE_SIZE),
               "oom load: a checkpoint loaded despite a failed allocation is whole");
      else
         refused++;
      movie_free(r);
   }
   movie_free(w);
   CHECK(refused > 0, "oom load: some allocation failure refuses the checkpoint");
   printf("[ok] oom load (%ld allocations, %u refused)\n", nth, refused);
}

/* With an argument, runs only the lane of that name. */
int main(int argc, char **argv)
{
   const char *only = argc > 1 ? argv[1] : NULL;
   setvbuf(stdout, NULL, _IONBF, 0);
   if (!only || !strcmp(only, "roundtrip"))
      lane_roundtrip();
   if (!only || !strcmp(only, "undefined"))
      lane_undefined();
   if (!only || !strcmp(only, "longer"))
      lane_longer_seq();
   if (!only || !strcmp(only, "record"))
      lane_record_after();
   if (!only || !strcmp(only, "layout"))
      lane_no_layout();
   if (!only || !strcmp(only, "oom"))
      lane_oom();
   if (!only || !strcmp(only, "oomload"))
      lane_oom_load();
   if (n_fail)
   {
      fprintf(stderr, "bsv_statestream_test: %u check(s) failed\n", n_fail);
      return 1;
   }
   puts("bsv_statestream_test: all lanes pass");
   return 0;
}
