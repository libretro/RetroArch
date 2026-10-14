/* cheat_manager.c over a core memory map of several regions, each
 * allocated at its exact size so a sanitizer sees any access past one:
 * cheats and searches reach only whole items inside a single region,
 * a repeat crosses into the next region, and a condition that cannot
 * be read does not let the cheat after it run. */

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
retro_time_t cpu_features_get_time_usec(void) { return 0; }
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

static int failures = 0;
#define CHECK(c) do { if (!(c)) { \
   fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c); failures++; } } while (0)

/* A [0,6), B [6,16), C [16,19): sizes that are not multiples of 2 or 4 */
static unsigned char *mem_a, *mem_b, *mem_c;
static rarch_memory_descriptor_t descs[3];

static void map_memory(void)
{
   mem_a = (unsigned char*)calloc(6, 1);
   mem_b = (unsigned char*)calloc(10, 1);
   mem_c = (unsigned char*)calloc(3, 1);
   descs[0].core.flags = RETRO_MEMDESC_SYSTEM_RAM;
   descs[0].core.ptr   = mem_a;
   descs[0].core.len   = 6;
   descs[1].core.flags = RETRO_MEMDESC_SYSTEM_RAM;
   descs[1].core.ptr   = mem_b;
   descs[1].core.len   = 10;
   descs[2].core.flags = RETRO_MEMDESC_SYSTEM_RAM;
   descs[2].core.ptr   = mem_c;
   descs[2].core.len   = 3;
   test_runloop.system.mmaps.descriptors     = descs;
   test_runloop.system.mmaps.num_descriptors = 3;
}

static void clear_memory(void)
{
   memset(mem_a, 0, 6);
   memset(mem_b, 0, 10);
   memset(mem_c, 0, 3);
}

/* @n cheats, all retro cheats switched on */
static struct item_cheat *cheats(unsigned n)
{
   unsigned i;
   free(cheat_manager_state.cheats);
   cheat_manager_state.cheats = (struct item_cheat*)calloc(n,
         sizeof(struct item_cheat));
   cheat_manager_state.size   = n;
   for (i = 0; i < n; i++)
   {
      cheat_manager_state.cheats[i].handler      = CHEAT_HANDLER_TYPE_RETRO;
      cheat_manager_state.cheats[i].state        = true;
      cheat_manager_state.cheats[i].cheat_type   = CHEAT_TYPE_SET_TO_VALUE;
      cheat_manager_state.cheats[i].repeat_count = 1;
      cheat_manager_state.cheats[i].memory_search_size = 3;
   }
   return cheat_manager_state.cheats;
}

static void test_apply(void)
{
   struct item_cheat *c;
   static const unsigned char zero[10] = { 0 };

   /* a 4-byte value across A's end, and an address past the map,
    * write nothing */
   clear_memory();
   c = cheats(2);
   c[0].memory_search_size = 5;
   c[0].address            = 4;
   c[0].value              = 0x11223344;
   c[1].address            = 19;
   c[1].value              = 0x55;
   cheat_manager_apply_retro_cheats();
   CHECK(!memcmp(mem_a, zero, 6) && !memcmp(mem_b, zero, 10)
         && !memcmp(mem_c, zero, 3));

   /* a byte repeat steps from A into B */
   clear_memory();
   c = cheats(1);
   c[0].address               = 4;
   c[0].value                 = 0x11;
   c[0].repeat_count          = 4;
   c[0].repeat_add_to_value   = 1;
   c[0].repeat_add_to_address = 1;
   cheat_manager_apply_retro_cheats();
   CHECK(mem_a[4] == 0x11 && mem_a[5] == 0x12
         && mem_b[0] == 0x13 && mem_b[1] == 0x14 && mem_b[2] == 0);

   /* a 16-bit repeat stops at the first item crossing a region's end */
   clear_memory();
   c = cheats(1);
   c[0].memory_search_size    = 4;
   c[0].address               = 3;
   c[0].value                 = 0x0101;
   c[0].repeat_count          = 3;
   c[0].repeat_add_to_address = 1;
   cheat_manager_apply_retro_cheats();
   CHECK(mem_a[3] == 0x01 && mem_a[4] == 0x01 && mem_a[5] == 0
         && !memcmp(mem_b, zero, 10));

   /* a condition that cannot be read stops the cheat after it ... */
   clear_memory();
   c = cheats(2);
   c[0].cheat_type         = CHEAT_TYPE_RUN_NEXT_IF_EQ;
   c[0].memory_search_size = 5;
   c[0].address            = 4;
   c[0].value              = 0;
   c[1].address            = 11;
   c[1].value              = 0x77;
   cheat_manager_apply_retro_cheats();
   CHECK(mem_b[5] == 0);
   /* ... and one that can be read and holds lets it run */
   c[0].address = 8;
   cheat_manager_apply_retro_cheats();
   CHECK(mem_b[5] == 0x77);
}

static void test_search(void)
{
   rarch_setting_t setting;
   unsigned address = 0, mask = 0, prev = 0, curr = 0;
   unsigned i;

   memset(&setting, 0, sizeof(setting));
   clear_memory();
   cheats(1);
   cheat_manager_state.search_bit_size = 5;
   cheat_manager_initialize_memory(&setting, 0, false);
   /* 32-bit items at 0, 8 and 12 are whole; 4 crosses A's end and 16
    * runs past C's */
   CHECK(cheat_manager_state.num_matches == 3);
   cheat_manager_search_eq(&setting, 0, false);
   CHECK(cheat_manager_state.num_matches == 3);
   cheat_manager_add_matches(NULL, NULL, 0, 0, 0);
   for (i = 0; i < 3; i++)
   {
      static const unsigned want[3] = { 0, 8, 12 };
      address = 0xFFFF;
      cheat_manager_match_action(CHEAT_MATCH_ACTION_TYPE_VIEW, i,
            &address, &mask, &prev, &curr);
      CHECK(address == want[i]);
   }
   /* browsing an item that cannot be read leaves the values alone */
   address = 4;
   curr    = prev = 0xABCD;
   cheat_manager_match_action(CHEAT_MATCH_ACTION_TYPE_BROWSE, 0,
         &address, &mask, &prev, &curr);
   CHECK(curr == 0xABCD && prev == 0xABCD);
}

int main(void)
{
   map_memory();
   test_apply();
   test_search();
   printf("cheat_regions: %s (%d failures)\n",
         failures ? "FAIL" : "PASS", failures);
   return failures ? 1 : 0;
}
