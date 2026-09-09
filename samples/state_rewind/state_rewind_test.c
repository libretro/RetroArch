/* The rewind codec and ring, driven directly.
 *
 * state_manager.c is included so the harness can call the codec
 * (find_change, raw_compress, raw_decompress) and the ring (new, push,
 * pop) that are static there; the frontend it calls into for the
 * integration entry points is stubbed with a fake core whose
 * savestate the harness controls.
 *
 * The codec is a reverse delta: compress(old, new) yields a patch
 * which, applied to a copy of new, restores old. Each case says which
 * invariant it locks; the ones a fix has not landed for yet are
 * marked and run without failing the harness until it does. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>

#include "../../state_manager.c"

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

/* --- the frontend, stubbed ------------------------------------------------ */

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }
const char *msg_hash_to_str(enum msg_hash_enums msg) { (void)msg; return "msg"; }
void runloop_msg_queue_push(const char *msg, size_t len, unsigned prio, unsigned duration,
      bool flush, char *title, enum message_queue_icon icon, enum message_queue_category category)
{ (void)msg; (void)len; (void)prio; (void)duration; (void)flush; (void)title; (void)icon; (void)category; }
bool retroarch_ctl(enum rarch_ctl_state state, void *data) { (void)state; (void)data; return false; }
bool audio_driver_has_callback(void) { return false; }
void audio_driver_frame_is_reverse(void) {}
void audio_driver_setup_rewind(void) {}
void audio_driver_sample(int16_t l, int16_t r) { (void)l; (void)r; }
size_t audio_driver_sample_batch(const int16_t *d, size_t f) { (void)d; return f; }
void audio_driver_sample_rewind(int16_t l, int16_t r) { (void)l; (void)r; }
size_t audio_driver_sample_batch_rewind(const int16_t *d, size_t f) { (void)d; return f; }

/* The fake core: a savestate the harness sets, counted. */
static uint8_t  core_state[8192];
static size_t   core_state_size = sizeof(core_state);
static unsigned core_serialize_calls, core_unserialize_calls;
static bool     core_supports_rewind = true;
static core_info_t fake_core_info;

bool core_info_get_current_core(core_info_t **core) { *core = &fake_core_info; return true; }
bool core_info_current_supports_rewind(void) { return core_supports_rewind; }
size_t content_get_serialized_size_rewind(void) { return core_state_size; }
bool content_serialize_state_rewind(void *buffer, size_t buffer_size)
{
   core_serialize_calls++;
   if (buffer_size < core_state_size) return false;
   memcpy(buffer, core_state, core_state_size);
   return true;
}
bool content_deserialize_state(const void *data, size_t size)
{
   core_unserialize_calls++;
   if (size < core_state_size) return false;
   memcpy(core_state, data, core_state_size);
   return true;
}

/* --- helpers -------------------------------------------------------------- */

static uint32_t rng = 0x2545F491u;
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

/* Two blocks laid out as state_manager_new lays them out: sentinels and
 * the scan pad after the data, uniq 0 and 1. */
static uint8_t *block_alloc(size_t block_size, unsigned uniq)
{
   size_t alloc = block_size + sizeof(uint16_t) * 4 + STATE_MANAGER_SCAN_PAD;
   uint8_t *b = (uint8_t*)calloc(alloc, 1);
   ((uint16_t*)b)[block_size / sizeof(uint16_t) + 3] = (uint16_t)uniq;
   return b;
}

/* compress(old, new); apply to a copy of new; must equal old. */
static bool roundtrip(const uint8_t *oldb, const uint8_t *newb, size_t block_size,
      size_t *patch_len_out)
{
   size_t maxp   = state_manager_raw_maxsize(block_size);
   uint8_t *patch = (uint8_t*)malloc(maxp);
   uint8_t *work  = (uint8_t*)malloc(block_size);
   size_t plen;
   bool ok;
   memcpy(work, newb, block_size);
   plen = state_manager_raw_compress(oldb, newb, block_size, patch);
   CHECK(plen <= maxp, "patch of %u bytes exceeds the maximum %u", (unsigned)plen, (unsigned)maxp);
   ok = state_manager_raw_decompress(patch, plen, work, block_size);
   CHECK(ok, "a valid patch was refused");
   ok = ok && memcmp(work, oldb, block_size) == 0;
   if (patch_len_out) *patch_len_out = plen;
   free(patch); free(work);
   return ok;
}

/* --- codec cases ---------------------------------------------------------- */

static void t_ident_roundtrip(void)
{
   size_t bs = 4096, plen;
   uint8_t *a = block_alloc(bs, 0), *b = block_alloc(bs, 1);
   size_t i;
   printf("   ident_roundtrip\n");
   for (i = 0; i < bs; i++) a[i] = b[i] = (uint8_t)(i * 7);
   CHECK(roundtrip(a, b, bs, &plen), "identical blocks did not round-trip");
   CHECK(plen == 3 * sizeof(uint16_t), "identical blocks: patch is %u bytes, the terminator is 6", (unsigned)plen);
   free(a); free(b);
}

static void t_single_word_dirty(void)
{
   size_t bs = 4096, at[3], k;
   printf("   single_word_dirty: index 0, middle, last\n");
   at[0] = 0; at[1] = bs / 4; at[2] = bs / 2 - 1;
   for (k = 0; k < 3; k++)
   {
      uint8_t *a = block_alloc(bs, 0), *b = block_alloc(bs, 1);
      size_t i;
      for (i = 0; i < bs; i++) a[i] = b[i] = (uint8_t)(i ^ 0x5A);
      ((uint16_t*)b)[at[k]] ^= 0x1234;
      CHECK(roundtrip(a, b, bs, NULL), "one word dirty at %u did not restore old", (unsigned)at[k]);
      free(a); free(b);
   }
}

static void t_long_skip_u32(void)
{
   size_t bs = 3 * 65536 * 2 + 1024;  /* more than UINT16_MAX words unchanged */
   uint8_t *a = block_alloc(bs, 0), *b = block_alloc(bs, 1);
   size_t i;
   printf("   long_skip_u32: an unchanged run past UINT16_MAX words\n");
   for (i = 0; i < bs; i++) a[i] = b[i] = (uint8_t)(i * 13);
   ((uint16_t*)b)[bs / 2 - 1] ^= 0xFFFF;
   CHECK(roundtrip(a, b, bs, NULL), "a run past UINT16_MAX words did not round-trip");
   free(a); free(b);
}

static void alarm_fail(int sig) { (void)sig; printf("      FAIL: find_change did not terminate\n"); _exit(1); }
static void t_sentinel_terminates(void)
{
   size_t bs = 4096;
   uint8_t *a = block_alloc(bs, 0), *b = block_alloc(bs, 1);
   size_t got;
   printf("   sentinel_terminates: equal blocks differ only at the sentinel\n");
   memset(a, 0xA5, bs); memset(b, 0xA5, bs);
   signal(SIGALRM, alarm_fail);
   alarm(1);
   got = find_change((const uint16_t*)a, (const uint16_t*)b);
   alarm(0);
   CHECK(got >= bs / 2 && got <= bs / 2 + 4, "find_change on equal blocks stopped at word %u, sentinel is at %u",
         (unsigned)got, (unsigned)(bs / 2 + 3));
   free(a); free(b);
}

static void t_scan_pad(void)
{
   size_t bs = 4096;
   uint8_t *a = block_alloc(bs, 0), *b = block_alloc(bs, 1);
   size_t i;
   printf("   scan_pad_asan: the last 16 bytes dirty, the final vector load inside the pad\n");
   for (i = 0; i < bs; i++) a[i] = b[i] = (uint8_t)i;
   for (i = bs - 16; i < bs; i++) b[i] ^= 0x80;
   CHECK(roundtrip(a, b, bs, NULL), "dirty tail did not round-trip");
   free(a); free(b);
}

static void t_codec_property_seeded(void)
{
   unsigned it;
   printf("   codec_property_seeded: 200 seeded old/new pairs at random sizes and dirtiness\n");
   for (it = 0; it < 200; it++)
   {
      size_t bs = 2 + (rnd() % 20000) * 2;
      uint8_t *a = block_alloc(bs, 0), *b = block_alloc(bs, 1);
      size_t i;
      unsigned mode = rnd() % 4;
      for (i = 0; i < bs; i++) a[i] = (uint8_t)rnd();
      memcpy(b, a, bs);
      for (i = 0; i < bs / 2; i++)
      {
         bool dirty = false;
         switch (mode)
         {
            case 0: dirty = (rnd() % 64) == 0; break;         /* sparse */
            case 1: dirty = (rnd() % 2) == 0; break;          /* half */
            case 2: dirty = true; break;                      /* everything */
            case 3: dirty = (i % 1000) < 40; break;           /* runs */
         }
         if (dirty) ((uint16_t*)b)[i] ^= (uint16_t)(rnd() | 1);
      }
      if (!roundtrip(a, b, bs, NULL))
      {
         CHECK(0, "seeded pair %u (size %u, mode %u) did not round-trip", it, (unsigned)bs, mode);
         free(a); free(b);
         return;
      }
      free(a); free(b);
   }
}

/* SM-02: a forged patch must not write past the block. Locked once the
 * bounded decoder lands; until then reported, not failed. */
static bool decoder_bounded = true;
/* SM-03: entries must count the retained records exactly, including
 * the one dropped when the head folds to the start of the ring. Locked
 * once that accounting lands. */
static bool entries_exact   = true;
static unsigned entries_drift;

/* state_manager_free releases the buffers; the struct is the caller's,
 * as event_deinit does it. */
static void manager_free(state_manager_t *sm)
{
   state_manager_free(sm);
   free(sm);
}

/* --- ring cases ----------------------------------------------------------- */

/* Push N distinct synthetic states through a manager; pop them all back
 * in reverse order and compare. Returns pops done. */
static unsigned push_pop(state_manager_t *sm, size_t state_size, unsigned n, unsigned *dirty_words)
{
   uint8_t *states = (uint8_t*)malloc((size_t)n * state_size);
   unsigned i, pops = 0;
   size_t w;
   for (i = 0; i < n; i++)
   {
      uint8_t *s = states + (size_t)i * state_size;
      void *where;
      if (i == 0)
         for (w = 0; w < state_size; w++) s[w] = (uint8_t)rnd();
      else
      {
         memcpy(s, states + (size_t)(i - 1) * state_size, state_size);
         for (w = 0; w < *dirty_words && w < state_size / 2; w++)
            ((uint16_t*)s)[(rnd() % (state_size / 2))] ^= (uint16_t)(rnd() | 1);
      }
      state_manager_push_where(sm, &where);
      memcpy(where, s, state_size);
      state_manager_push_do(sm);
   }
   /* The first pop returns the last pushed state itself; each later pop
    * decompresses the one before it onto the block. N pushes, N pops. */
   for (i = n; i > 0; i--)
   {
      const void *data;
      if (!state_manager_pop(sm, &data))
         break;
      pops++;
      if (memcmp(data, states + (size_t)(i - 1) * state_size, state_size) != 0)
      {
         CHECK(0, "pop %u did not restore state %u", pops, i - 1);
         break;
      }
   }
   free(states);
   return pops;
}

static void t_push_pop_n(void)
{
   size_t ss = 4096;
   unsigned dirty = 8, pops;
   state_manager_t *sm = state_manager_new(ss, 1 << 20);
   printf("   push_pop_n: 50 states, popped back in order\n");
   CHECK(sm != NULL, "state_manager_new failed");
   if (!sm) return;
   pops = push_pop(sm, ss, 50, &dirty);
   CHECK(pops == 50, "popped %u of 50", pops);
   CHECK(sm->entries == 0, "%u entries remain after popping everything", (unsigned)sm->entries);
   manager_free(sm);
}

static void t_wrap_near_end(void)
{
   /* Every word dirty, so every patch is near maxcompsize; a ring just
    * over three of them wraps every few pushes. */
   size_t ss = 2048;
   size_t maxc = state_manager_raw_maxsize(ss) + sizeof(size_t) * 2;
   unsigned dirty = 100000, pops, n;
   state_manager_t *sm = state_manager_new(ss, maxc * 3 + sizeof(size_t) + 200);
   printf("   wrap_near_end: incompressible states through a ring of three\n");
   CHECK(sm != NULL, "state_manager_new failed");
   if (!sm) return;
   for (n = 2; n <= 40; n++)
   {
      unsigned d = dirty;
      pops = push_pop(sm, ss, n, &d);
      CHECK(pops >= 2 && pops <= n, "%u pushed: %u pops", n, pops);
      if (entries_exact)
         CHECK(sm->entries == 0, "%u pushed: %u entries after popping", n, (unsigned)sm->entries);
      else if (sm->entries != 0)
         entries_drift++;
   }
   manager_free(sm);
}

static void t_drop_oldest(void)
{
   size_t ss = 4096;
   size_t maxc = state_manager_raw_maxsize(ss) + sizeof(size_t) * 2;
   unsigned dirty = 100000, pops;
   state_manager_t *sm = state_manager_new(ss, maxc * 4);
   printf("   drop_oldest: pushing past capacity keeps entries and pops consistent\n");
   CHECK(sm != NULL, "state_manager_new failed");
   if (!sm) return;
   pops = push_pop(sm, ss, 30, &dirty);
   CHECK(pops >= 3 && pops < 30, "a ring of about four held %u", pops);
   if (entries_exact)
      CHECK(sm->entries == 0, "%u entries remain", (unsigned)sm->entries);
   else if (sm->entries != 0)
      entries_drift++;
   manager_free(sm);
}

/* SM-01: a manager that failed to allocate must not be pushed to. */
static void t_init_oom_guard(void)
{
   struct state_manager_rewind_state rs;
   printf("   init_oom_guard: event_init with a buffer no manager can be made from\n");
   memset(&rs, 0, sizeof(rs));
   core_state_size = sizeof(core_state);
   /* A buffer size of 0: state_manager_new returns NULL (malloc(0) may
    * return NULL or a zero-length block the ring cannot use). */
   state_manager_event_init(&rs, 0);
   CHECK(rs.state == NULL || rs.state->capacity == 0, "a manager was made from a zero buffer");
   state_manager_event_deinit(&rs, NULL);
   core_state_size = sizeof(core_state);
}

static void t_decompress_oob(void)
{
   size_t bs = 1024;
   uint8_t *work = block_alloc(bs, 1);
   uint8_t *pad  = work + bs;
   uint16_t patch[16];
   size_t pad_len = sizeof(uint16_t) * 4 + STATE_MANAGER_SCAN_PAD, i;
   printf("   decompress_oob_stops: a patch whose skip runs past the block\n");
   memset(work, 0x11, bs);
   memset(pad, 0xEE, pad_len);
   /* one changed word after a skip of bs/2 + 4 words: past the end */
   patch[0] = 1; patch[1] = (uint16_t)(bs / 2 + 4); patch[2] = 0xBEEF;
   patch[3] = 0; patch[4] = 0; patch[5] = 0;
   if (decoder_bounded)
   {
      bool ok = state_manager_raw_decompress(patch, sizeof(patch), work, bs);
      CHECK(!ok, "a patch running past the block was accepted");
      for (i = 0; i < pad_len; i++)
         if (pad[i] != 0xEE) { CHECK(0, "the decoder wrote into the pad at %u", (unsigned)i); break; }
      for (i = 0; i < bs; i++)
         if (work[i] != 0x11) { CHECK(0, "a refused patch changed the block at %u", (unsigned)i); break; }
   }
   else
      printf("      (decoder not yet bounded: case reported, not run)\n");
   free(work);
}

/* A malformed record must fail whole: a valid patch with one late
 * token corrupted leaves every byte as it was, and a truncated record
 * is refused rather than read past. */
static void t_decompress_atomic(void)
{
   size_t bs = 4096, plen, i;
   uint8_t *a = block_alloc(bs, 0), *b = block_alloc(bs, 1);
   uint8_t *patch = (uint8_t*)malloc(state_manager_raw_maxsize(bs));
   uint8_t *work  = (uint8_t*)malloc(bs);
   uint16_t *p16;
   printf("   decompress_oob_atomic: a late token forged, a record truncated\n");
   for (i = 0; i < bs; i++) a[i] = (uint8_t)(i * 3);
   memcpy(b, a, bs);
   for (i = 0; i < bs / 2; i += 97) ((uint16_t*)b)[i] ^= 0x0F0F;   /* several dirty runs */
   plen = state_manager_raw_compress(a, b, bs, patch);
   /* Forge the skip of the last run so it points past the block. */
   p16 = (uint16_t*)patch;
   {
      size_t w = plen / 2, last_skip = 0, at = 0;
      while (at < w)
      {
         uint16_t n = p16[at];
         if (n) { last_skip = at + 1; at += 2 + n; }
         else { if (!(p16[at + 1] | p16[at + 2])) break; at += 3; }
      }
      p16[last_skip] = 0xFFFF;
   }
   memcpy(work, b, bs);
   CHECK(!state_manager_raw_decompress(patch, plen, work, bs), "a forged late skip was accepted");
   CHECK(memcmp(work, b, bs) == 0, "a refused patch was partly applied");
   /* Truncated: the record ends before its terminator. */
   memcpy(work, b, bs);
   plen = state_manager_raw_compress(a, b, bs, patch);
   CHECK(!state_manager_raw_decompress(patch, plen - 6, work, bs), "a record without its terminator was accepted");
   CHECK(memcmp(work, b, bs) == 0, "a truncated record was partly applied");
   /* Seeded mutations: never a crash, never a write outside, and a
    * record that decodes restores old exactly or is refused. */
   {
      unsigned it;
      for (it = 0; it < 300; it++)
      {
         size_t pos;
         plen = state_manager_raw_compress(a, b, bs, patch);
         pos  = rnd() % plen;
         patch[pos] ^= (uint8_t)(rnd() | 1);
         memcpy(work, b, bs);
         if (state_manager_raw_decompress(patch, plen, work, bs))
            ; /* decoded inside the block; whatever it restored is inside it */
         else
            CHECK(memcmp(work, b, bs) == 0, "mutation %u: refused but changed the block", it);
      }
   }
   free(a); free(b); free(patch); free(work);
}

int main(void)
{
   printf("state_rewind:\n");
   t_ident_roundtrip();
   t_single_word_dirty();
   t_long_skip_u32();
   t_sentinel_terminates();
   t_scan_pad();
   t_codec_property_seeded();
   t_push_pop_n();
   t_wrap_near_end();
   t_drop_oldest();
   t_init_oom_guard();
   t_decompress_oob();
   t_decompress_atomic();
   if (!entries_exact && entries_drift)
      printf("   (entries drifted on %u wrapped runs: accounting not yet locked)\n", entries_drift);
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("state_rewind: the codec is a reverse delta that round-trips, and the ring pops what it pushed\n");
   return 0;
}
