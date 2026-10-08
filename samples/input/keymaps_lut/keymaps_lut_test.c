/* input/input_keymaps.c's keysym reverse table.
 *
 * input_keymaps_init_keyboard_lut() builds a table from platform key
 * codes to RETROK_*; input_keymaps_translate_keysym_to_rk() reads it,
 * or walks the forward table when there is none. The file is
 * compiled in here with calloc() routed through the test, so the
 * table's allocation can be failed:
 *
 *  - with memory, every mapped code translates and an unmapped one
 *    gives RETROK_UNKNOWN;
 *  - with the table's allocation failing, the same answers come from
 *    the forward table;
 *  - a map whose codes reach 65536 has no table either, and answers
 *    the same.
 *
 * Plain C, no platform headers, so it builds the same with mingw. */

#include <stdio.h>
#include <stdlib.h>

static int fail_next_calloc;

static void *test_calloc(size_t nmemb, size_t size)
{
   if (fail_next_calloc)
   {
      fail_next_calloc = 0;
      return NULL;
   }
   return calloc(nmemb, size);
}

#define calloc test_calloc
#include "../../../input/input_keymaps.c"
#undef calloc

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { \
   fprintf(stderr, "FAIL: " __VA_ARGS__); \
   fprintf(stderr, "\n"); failures++; } } while (0)

static const struct rarch_key_map small_map[] = {
   { 10,  RETROK_a      },
   { 11,  RETROK_b      },
   { 200, RETROK_RETURN },
   { 0,   RETROK_UNKNOWN }
};

static const struct rarch_key_map wide_map[] = {
   { 10,     RETROK_a      },
   { 70000,  RETROK_b      },
   { 0,      RETROK_UNKNOWN }
};

static void check_small(const char *how)
{
   CHECK(input_keymaps_translate_keysym_to_rk(10)  == RETROK_a,
         "%s: code 10 is a", how);
   CHECK(input_keymaps_translate_keysym_to_rk(11)  == RETROK_b,
         "%s: code 11 is b", how);
   CHECK(input_keymaps_translate_keysym_to_rk(200) == RETROK_RETURN,
         "%s: code 200 is return", how);
   CHECK(input_keymaps_translate_keysym_to_rk(12)  == RETROK_UNKNOWN,
         "%s: code 12 is unknown", how);
}

int main(void)
{
   input_keymaps_init_keyboard_lut(small_map);
   CHECK(rarch_keysym_rlut != NULL, "with memory: a table");
   check_small("with memory");

   fail_next_calloc = 1;
   input_keymaps_init_keyboard_lut(small_map);
   CHECK(!fail_next_calloc, "the table's allocation was not made");
   CHECK(rarch_keysym_rlut == NULL, "no memory: no table");
   check_small("no memory");

   input_keymaps_init_keyboard_lut(small_map);
   input_keymaps_init_keyboard_lut(wide_map);
   CHECK(input_keymaps_translate_keysym_to_rk(10)    == RETROK_a,
         "wide map: code 10 is a");
   CHECK(input_keymaps_translate_keysym_to_rk(70000) == RETROK_b,
         "wide map: code 70000 is b");
   CHECK(input_keymaps_translate_keysym_to_rk(200)   == RETROK_UNKNOWN,
         "wide map: code 200, from the map before, is unknown");

   free(rarch_keysym_rlut);
   rarch_keysym_rlut = NULL;

   if (failures)
   {
      fprintf(stderr, "%d failure(s)\n", failures);
      return 1;
   }
   printf("keymaps_lut: all passed\n");
   return 0;
}
