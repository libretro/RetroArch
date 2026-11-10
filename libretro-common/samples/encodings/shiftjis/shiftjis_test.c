/* Shift-JIS (code page 932) and code page 1252 conversions: known
 * characters each way, every two-octet code through UTF-8 and back,
 * the octets skipped, and output that stops at a character it has no
 * room for. */
#include <stdio.h>
#include <string.h>

#include <encodings/shiftjis.h>

static int failures = 0;

static void check(int ok, const char *what)
{
   if (!ok)
   {
      fprintf(stderr, "FAIL: %s\n", what);
      failures++;
   }
}

static void check_bytes(const char *got, size_t got_len, const char *want, size_t want_len,
      const char *what)
{
   if (got_len != want_len || memcmp(got, want, want_len) || got[got_len] != '\0')
   {
      size_t i;
      fprintf(stderr, "FAIL: %s: got", what);
      for (i = 0; i < got_len; i++)
         fprintf(stderr, " %02X", (unsigned char)got[i]);
      fprintf(stderr, "\n");
      failures++;
   }
}

#define CHECK_SJIS(sjis, utf8, what) \
   do { \
      char out[64]; \
      size_t n = sjis_to_utf8(out, sizeof(out), sjis, sizeof(sjis) - 1); \
      check_bytes(out, n, utf8, sizeof(utf8) - 1, what " (to UTF-8)"); \
   } while (0)

#define CHECK_BOTH(sjis, utf8, what) \
   do { \
      char out[64]; \
      size_t n; \
      CHECK_SJIS(sjis, utf8, what); \
      n = utf8_to_sjis(out, sizeof(out), utf8, sizeof(utf8) - 1); \
      check_bytes(out, n, sjis, sizeof(sjis) - 1, what " (to Shift-JIS)"); \
   } while (0)

static void check_known(void)
{
   CHECK_BOTH("abc\\~", "abc\\~", "ASCII, backslash and tilde as they are");
   CHECK_BOTH("\x82\xA0", "\xE3\x81\x82", "hiragana A");
   CHECK_BOTH("\x81\x40", "\xE3\x80\x80", "ideographic space");
   CHECK_BOTH("\x81\x5F", "\xEF\xBC\xBC", "full-width backslash");
   CHECK_BOTH("\x81\x60", "\xEF\xBD\x9E", "wave dash as code page 932 has it");
   CHECK_BOTH("\x88\x9F", "\xE4\xBA\x9C", "first level 1 kanji");
   CHECK_BOTH("\xEA\xA4", "\xE7\x86\x99", "last level 2 kanji");
   CHECK_BOTH("\x87\x40", "\xE2\x91\xA0", "NEC circled one");
   CHECK_SJIS("\xFA\x40", "\xE2\x85\xB0", "IBM small roman one");
   CHECK_BOTH("\xEE\xEF", "\xE2\x85\xB0", "NEC-selected IBM small roman one");
   CHECK_BOTH("\xB1", "\xEF\xBD\xB1", "half-width katakana A");
   CHECK_BOTH("\x93\xFA\x96\x7B", "\xE6\x97\xA5\xE6\x9C\xAC", "two kanji");
   /* the NEC duplicate of a kanji decodes to it and comes back as the
    * code page's own code for it */
   CHECK_SJIS("\xED\x40", "\xE7\xBA\x8A", "NEC-selected IBM kanji");
}

static void check_skipped(void)
{
   CHECK_SJIS("a\x80" "b\xA0" "c\xFD\xFE\xFF" "d", "abcd", "octets code page 932 sends to the PUA");
   CHECK_SJIS("a\x82", "a", "a lead octet at the end");
   CHECK_SJIS("\x82\x20" "b", " b", "a lead octet before a bad trail");
   CHECK_SJIS("\xF0\x40" "z", "@z", "the user-defined area");
   CHECK_SJIS("\x85\x40" "z", "@z", "an unassigned code");
   {
      char   out[16];
      size_t n;
      /* an overlong slash, a surrogate, an emoji and a lone
       * continuation octet */
      static const char bad[] = "a\xC0\xAF" "b\xED\xA0\x80" "c\xF0\x9F\x98\x80" "d\x80" "e";
      n = utf8_to_sjis(out, sizeof(out), bad, sizeof(bad) - 1);
      check_bytes(out, n, "abcde", 5, "UTF-8 it cannot convert");
   }
}

static void check_room(void)
{
   char   out[8];
   size_t n;
   /* three octets free: the second kanji does not fit */
   n = sjis_to_utf8(out, 5, "\x93\xFA\x96\x7B", 4);
   check_bytes(out, n, "\xE6\x97\xA5", 3, "stops before a character with no room");
   n = utf8_to_sjis(out, 4, "\xE6\x97\xA5\xE6\x9C\xAC", 6);
   check_bytes(out, n, "\x93\xFA", 2, "stops before a two-octet code with no room");
   n = cp1252_to_utf8(out, 3, "\x80", 1);
   check_bytes(out, n, "", 0, "stops before the euro sign with no room");
   check(sjis_to_utf8(out, 0, "a", 1) == 0, "no room at all");
}

static void check_cp1252(void)
{
   char   out[64];
   size_t n = cp1252_to_utf8(out, sizeof(out), "\x80\x81\x8A\x9F\xE9\xFF" "z", 7);
   check_bytes(out, n, "\xE2\x82\xAC\xC5\xA0\xC5\xB8\xC3\xA9\xC3\xBF" "z", 12, "code page 1252");
}

/* Every two-octet code that decodes comes back, through UTF-8, as a
 * code that decodes to the same character. */
static void check_round_trip(void)
{
   unsigned lead, trail, mapped = 0;
   for (lead = 0x81; lead <= 0xFC; lead++)
   {
      for (trail = 0x40; trail <= 0xFC; trail++)
      {
         char   in[2], utf8[8], lone[8], back[4], again[8];
         size_t n, m, k;
         in[0] = (char)lead;
         in[1] = (char)trail;
         /* a lead octet alone is skipped; anything else is a character */
         if (sjis_to_utf8(lone, sizeof(lone), in, 1))
            break;
         n = sjis_to_utf8(utf8, sizeof(utf8), in, 2);
         /* nothing, or the lead skipped and the trail read alone */
         if (n == sjis_to_utf8(lone, sizeof(lone), in + 1, 1) && !memcmp(utf8, lone, n))
            continue;
         mapped++;
         m = utf8_to_sjis(back, sizeof(back), utf8, n);
         k = sjis_to_utf8(again, sizeof(again), back, m);
         if (m != 2 || k != n || memcmp(utf8, again, n))
         {
            fprintf(stderr, "FAIL: %02X %02X does not come back\n", lead, trail);
            failures++;
            return;
         }
      }
   }
   check(mapped == 7724, "the number of two-octet codes");
   printf("%u two-octet codes through UTF-8 and back\n", mapped);
}

int main(void)
{
   check_known();
   check_skipped();
   check_room();
   check_cp1252();
   check_round_trip();
   if (failures)
   {
      fprintf(stderr, "FAIL: %d checks\n", failures);
      return 1;
   }
   printf("ok\n");
   return 0;
}
