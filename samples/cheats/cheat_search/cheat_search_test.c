/* cheat_manager.c's memory search against the algorithm it replaced,
 * kept here as the reference: random core memory maps (regions of
 * sizes that are not multiples of 2 or 4), every item size (1, 2 and 4
 * bits, bytes, 16- and 32-bit words), both byte orders, every search
 * type, the item size changed between searches without starting over,
 * chained searches with memory changing between them - in runs,
 * scattered, or not at all - and after each search the match bytes,
 * the match count and the saved copy of memory must agree exactly.
 * features_cpu.c is linked for real and the whole run repeats with the
 * match-byte scan capped at each tier (SSE2, AVX2, AVX-512BW), so every
 * one this CPU has is checked. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "cheat_manager.h"
#include "runloop.h"
#include "core.h"
#include "msg_hash.h"
#include "verbosity.h"

/* What cheat_manager.c calls outside itself */
static runloop_state_t test_runloop;
runloop_state_t *runloop_state_get_ptr(void) { return &test_runloop; }
void runloop_msg_queue_push(const char *msg, size_t len, unsigned prio,
      unsigned duration, bool flush, char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{ (void)msg; (void)len; (void)prio; (void)duration; (void)flush;
  (void)title; (void)icon; (void)category; }
void runloop_frame_work_set(unsigned work, bool on) { (void)work; (void)on; }
const char *msg_hash_to_str(enum msg_hash_enums msg) { (void)msg; return ""; }
bool core_get_memory(retro_ctx_memory_info_t *info) { (void)info; return false; }
bool core_get_system_info(struct retro_system_info *info) { (void)info; return false; }
bool core_set_cheat(retro_ctx_cheat_info_t *info) { (void)info; return true; }
bool core_reset_cheat(void) { return true; }
bool input_set_rumble_state(unsigned port, enum retro_rumble_effect e,
      uint16_t strength) { (void)port; (void)e; (void)strength; return true; }
config_file_t *config_file_new_from_path_to_string(const char *p) { (void)p; return NULL; }
config_file_t *config_file_new_alloc(void) { return NULL; }
config_file_t *config_file_new_with_callback(const char *p,
      config_file_cb_t *cb) { (void)p; (void)cb; return NULL; }
void config_file_free(config_file_t *c) { (void)c; }
bool config_file_write(config_file_t *c, const char *p, bool s)
{ (void)c; (void)p; (void)s; return false; }
size_t config_set_int(config_file_t *c, const char *k, int v) { (void)c; (void)k; (void)v; return 0; }
size_t config_set_uint(config_file_t *c, const char *k, unsigned v) { (void)c; (void)k; (void)v; return 0; }
void config_set_string(config_file_t *c, const char *k, const char *v) { (void)c; (void)k; (void)v; }
int filestream_delete(const char *p) { (void)p; return -1; }
size_t fill_pathname_join(char *o, const char *d, const char *p, size_t n)
{ (void)d; (void)p; if (n) *o = '\0'; return 0; }
const char *path_basename_nocompression(const char *p) { return p; }
bool path_is_valid(const char *p) { (void)p; return false; }
bool path_mkdir(const char *p) { (void)p; return false; }


void RARCH_LOG(const char *fmt, ...) { (void)fmt; }

static unsigned failures;
#ifdef CHEAT_SCAN_TEST_HOOKS
extern int cheat_scan_tier_cap;
#endif
static uint32_t rng = 0xC0FFEEu;
static unsigned rnd(void) { rng = rng * 1103515245u + 12345u; return rng >> 8; }

#define MAX_REGIONS 6
static unsigned char *mem[MAX_REGIONS];
static unsigned mem_len[MAX_REGIONS];
static unsigned nregions;
static rarch_memory_descriptor_t descs[MAX_REGIONS];

static void map_memory(unsigned scale)
{
   unsigned i;
   nregions = 1 + rnd() % MAX_REGIONS;
   for (i = 0; i < nregions; i++)
   {
      unsigned j;
      mem_len[i] = 1 + rnd() % scale;
      mem[i]     = (unsigned char*)malloc(mem_len[i]);
      for (j = 0; j < mem_len[i]; j++)
         mem[i][j] = (unsigned char)((rnd() & 3) ? rnd() % 4 : rnd());
      memset(&descs[i], 0, sizeof(descs[i]));
      descs[i].core.flags = RETRO_MEMDESC_SYSTEM_RAM;
      descs[i].core.ptr   = mem[i];
      descs[i].core.len   = mem_len[i];
   }
   test_runloop.system.mmaps.descriptors     = descs;
   test_runloop.system.mmaps.num_descriptors = nregions;
}

static void unmap_memory(void)
{
   unsigned i;
   for (i = 0; i < nregions; i++)
      free(mem[i]);
}

/* The previous search, item by item through a region lookup per item. */
static bool ref_seek(unsigned address, unsigned size, unsigned char **base,
      unsigned *start)
{
   unsigned i, s = 0;
   cheat_manager_t *st = &cheat_manager_state;
   for (i = 0; i < st->num_memory_buffers; i++)
   {
      unsigned len = st->memory_size_list[i];
      if (address - s < len)
      {
         if (size > len - (address - s))
            return false;
         *base  = st->memory_buf_list[i];
         *start = s;
         return true;
      }
      s += len;
   }
   return false;
}

static void ref_search(enum cheat_search_type type, uint8_t *matches,
      unsigned *num_matches, uint8_t *prev)
{
   cheat_manager_t *st = &cheat_manager_state;
   unsigned bytes_per_item = 1, bits = 8, mask = 0xFF, idx, i, off = 0;
   switch (st->search_bit_size)
   {
      case 0: bits = 1; mask = 0x01; break;
      case 1: bits = 2; mask = 0x03; break;
      case 2: bits = 4; mask = 0x0F; break;
      case 3: break;
      case 4: bytes_per_item = 2; mask = 0xFFFF; break;
      case 5: bytes_per_item = 4; mask = 0xFFFFFFFF; break;
   }
   for (idx = 0; idx < st->total_memory_size; idx += bytes_per_item)
   {
      unsigned char *curr;
      unsigned offset, curr_val, prev_val, byte_part;
      if (!ref_seek(idx, bytes_per_item, &curr, &offset))
         continue;
      switch (bytes_per_item)
      {
         case 2:
            curr_val = st->big_endian
               ? (curr[idx - offset] * 256) + curr[idx + 1 - offset]
               : curr[idx - offset] + (curr[idx + 1 - offset] * 256);
            prev_val = st->big_endian
               ? (prev[idx] * 256) + prev[idx + 1]
               : prev[idx] + (prev[idx + 1] * 256);
            break;
         case 4:
            curr_val = st->big_endian
               ? ((unsigned)curr[idx - offset] << 24) | (curr[idx + 1 - offset] << 16)
                 | (curr[idx + 2 - offset] << 8) | curr[idx + 3 - offset]
               : curr[idx - offset] | (curr[idx + 1 - offset] << 8)
                 | (curr[idx + 2 - offset] << 16) | ((unsigned)curr[idx + 3 - offset] << 24);
            prev_val = st->big_endian
               ? ((unsigned)prev[idx] << 24) | (prev[idx + 1] << 16)
                 | (prev[idx + 2] << 8) | prev[idx + 3]
               : prev[idx] | (prev[idx + 1] << 8)
                 | (prev[idx + 2] << 16) | ((unsigned)prev[idx + 3] << 24);
            break;
         default:
            curr_val = curr[idx - offset];
            prev_val = prev[idx];
            break;
      }
      for (byte_part = 0; byte_part < 8 / bits; byte_part++)
      {
         unsigned cs = (curr_val >> (byte_part * bits)) & mask;
         unsigned ps = (prev_val >> (byte_part * bits)) & mask;
         unsigned pm = (bits < 8) ? (matches[idx] & (mask << (byte_part * bits)))
                                  : matches[idx];
         bool m = false;
         if (!pm)
            continue;
         switch (type)
         {
            case CHEAT_SEARCH_TYPE_EXACT:   m = cs == st->search_exact_value; break;
            case CHEAT_SEARCH_TYPE_LT:      m = cs <  ps; break;
            case CHEAT_SEARCH_TYPE_GT:      m = cs >  ps; break;
            case CHEAT_SEARCH_TYPE_LTE:     m = cs <= ps; break;
            case CHEAT_SEARCH_TYPE_GTE:     m = cs >= ps; break;
            case CHEAT_SEARCH_TYPE_EQ:      m = cs == ps; break;
            case CHEAT_SEARCH_TYPE_NEQ:     m = cs != ps; break;
            case CHEAT_SEARCH_TYPE_EQPLUS:  m = cs == ps + st->search_eqplus_value; break;
            case CHEAT_SEARCH_TYPE_EQMINUS: m = cs == ps - st->search_eqminus_value; break;
         }
         if (!m)
         {
            if (bits < 8)
               matches[idx] &= (~(mask << (byte_part * bits))) & 0xFF;
            else
               memset(matches + idx, 0, bytes_per_item);
            if (*num_matches > 0)
               (*num_matches)--;
         }
      }
   }
   for (i = 0; i < st->num_memory_buffers; i++)
   {
      memcpy(prev + off, st->memory_buf_list[i], st->memory_size_list[i]);
      off += st->memory_size_list[i];
   }
}

static void run_search(enum cheat_search_type type, rarch_setting_t *s)
{
   switch (type)
   {
      case CHEAT_SEARCH_TYPE_EXACT:   cheat_manager_search_exact(s, 0, false); break;
      case CHEAT_SEARCH_TYPE_LT:      cheat_manager_search_lt(s, 0, false); break;
      case CHEAT_SEARCH_TYPE_GT:      cheat_manager_search_gt(s, 0, false); break;
      case CHEAT_SEARCH_TYPE_LTE:     cheat_manager_search_lte(s, 0, false); break;
      case CHEAT_SEARCH_TYPE_GTE:     cheat_manager_search_gte(s, 0, false); break;
      case CHEAT_SEARCH_TYPE_EQ:      cheat_manager_search_eq(s, 0, false); break;
      case CHEAT_SEARCH_TYPE_NEQ:     cheat_manager_search_neq(s, 0, false); break;
      case CHEAT_SEARCH_TYPE_EQPLUS:  cheat_manager_search_eqplus(s, 0, false); break;
      case CHEAT_SEARCH_TYPE_EQMINUS: cheat_manager_search_eqminus(s, 0, false); break;
   }
}

/* Changes a few bytes, a run, or nothing. */
static void mutate(void)
{
   unsigned r = rnd() % nregions, k, n;
   switch (rnd() % 3)
   {
      case 0:
         n = rnd() % 4;
         for (k = 0; k < n; k++)
            mem[r][rnd() % mem_len[r]] += (unsigned char)(1 + rnd() % 3);
         break;
      case 1:
      {
         unsigned at = rnd() % mem_len[r], len = 1 + rnd() % 40;
         for (k = at; k < at + len && k < mem_len[r]; k++)
            mem[r][k] = (unsigned char)(mem[r][k] - 1);
         break;
      }
      default:
         break;
   }
}

int main(void)
{
   static const enum cheat_search_type types[] = {
      CHEAT_SEARCH_TYPE_EXACT, CHEAT_SEARCH_TYPE_LT, CHEAT_SEARCH_TYPE_GT,
      CHEAT_SEARCH_TYPE_LTE, CHEAT_SEARCH_TYPE_GTE, CHEAT_SEARCH_TYPE_EQ,
      CHEAT_SEARCH_TYPE_NEQ, CHEAT_SEARCH_TYPE_EQPLUS, CHEAT_SEARCH_TYPE_EQMINUS
   };
   unsigned round, searches = 0;
   int tier;
   rarch_setting_t setting;
   memset(&setting, 0, sizeof(setting));

   for (tier = 0; tier <= 2; tier++)
   for (round = 0; round < 600; round++)
   {
      cheat_manager_t *st = &cheat_manager_state;
      unsigned total, step, scale = (round % 10 == 0) ? 3000 : 70;
      uint8_t *m_ref, *p_ref;
      unsigned n_ref;

#ifdef CHEAT_SCAN_TEST_HOOKS
      cheat_scan_tier_cap = tier;
#endif
      map_memory(scale);
      st->search_bit_size = rnd() % 6;
      cheat_manager_initialize_memory(&setting, 0, false);
      st->big_endian      = (rnd() & 1) != 0;
      total               = st->total_memory_size;
      if (!st->matches || !st->prev_memory_buf)
      {
         printf("FAIL round %u: search not initialised\n", round);
         failures++;
         unmap_memory();
         continue;
      }
      m_ref = (uint8_t*)malloc(total);
      p_ref = (uint8_t*)malloc(total);
      memcpy(m_ref, st->matches, total);
      memcpy(p_ref, st->prev_memory_buf, total);
      n_ref = st->num_matches;

      for (step = 0; step < 8; step++)
      {
         enum cheat_search_type t = types[rnd() % 9];
         /* The size can change in the menu without the search being
          * started again; match bytes cleared at one size are then read
          * at another. */
         if (rnd() % 8 == 0)
            st->search_bit_size = rnd() % 6;
         st->search_exact_value   = rnd() % 4;
         st->search_eqplus_value  = 1 + rnd() % 2;
         st->search_eqminus_value = 1 + rnd() % 2;
         mutate();
         ref_search(t, m_ref, &n_ref, p_ref);
         run_search(t, &setting);
         searches++;
         if (     st->num_matches != n_ref
               || memcmp(st->matches, m_ref, total)
               || memcmp(st->prev_memory_buf, p_ref, total))
         {
            printf("FAIL tier %d round %u step %u: type %d, bit size %u, %s, %u bytes "
                  "in %u regions: %u matches, reference %u\n",
                  tier, round, step, (int)t, st->search_bit_size,
                  st->big_endian ? "big-endian" : "little-endian",
                  total, nregions, st->num_matches, n_ref);
            failures++;
            break;
         }
      }
      free(m_ref);
      free(p_ref);
      unmap_memory();
   }

   if (failures)
   {
      printf("cheat_search: %u failing rounds\n", failures);
      return 1;
   }
   printf("cheat_search: %u searches agree with the reference\n", searches);
   return 0;
}
