/* Right-to-left text laid out for drawing.
 *
 * font_driver_reshape_msg() puts each line of a message in visual
 * order with a single-line subset of the Unicode bidi algorithm,
 * mirrors brackets in right-to-left runs and shapes Arabic. Each case
 * gives the text as typed and what is drawn, read left to right. Text
 * with nothing right-to-left comes back byte for byte, and no input
 * makes it write past the buffer it is given: every buffer here has a
 * canary after it.
 *
 * font_driver.c is included rather than linked, for the static
 * reshaper. Strings are spelled in escapes so the source stays ASCII. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Allocations made by font_driver.c, for the check that laying a line
 * out allocates nothing */
static unsigned long test_mallocs = 0;
static void *test_malloc(size_t n)
{
   test_mallocs++;
   return malloc(n);
}
#define malloc test_malloc
#include "../../../gfx/font_driver.c"
#undef malloc

/* The stubs' junk reads, which these tests do not make */
int read_should_fail = 0;

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

/* Hebrew letters */
#define ALEF    "\xD7\x90"
#define BET     "\xD7\x91"
#define GIMEL   "\xD7\x92"
#define DALET   "\xD7\x93"
#define HE      "\xD7\x94"
#define VAV     "\xD7\x95"
#define HET     "\xD7\x97"
#define TET     "\xD7\x98"
#define YOD     "\xD7\x99"
#define LAMED   "\xD7\x9C"
#define MEM_F   "\xD7\x9D"
#define MEM     "\xD7\x9E"
#define NUN_F   "\xD7\x9F"
#define SAMEKH  "\xD7\xA1"
#define AYIN    "\xD7\xA2"
#define TSADI_F "\xD7\xA5"
#define QOF     "\xD7\xA7"
#define RESH    "\xD7\xA8"
#define SHIN    "\xD7\xA9"
#define TAV     "\xD7\xAA"

/* Arabic letters, a diacritic and digits */
#define AR_ALEF  "\xD8\xA7"
#define AR_DAL   "\xD8\xAF"
#define AR_SEEN  "\xD8\xB3"
#define AR_AIN   "\xD8\xB9"
#define AR_LAM   "\xD9\x84"
#define AR_MEEM  "\xD9\x85"
#define AR_FATHA "\xD9\x8E"
#define AR_RAY   "\xD8\x88" /* U+0608, a letter with no presentation forms */
#define AR_COMMA "\xD8\x8C" /* U+060C */
#define AR_1     "\xD9\xA1" /* U+0661 */
#define AR_3     "\xD9\xA3" /* U+0663 */
#define QAMATS   "\xD6\xB8" /* U+05B8 */
#define HOLAM    "\xD6\xB9" /* U+05B9 */
#define ACUTE    "\xCC\x81" /* U+0301 */
#define AR_KEHEH "\xDA\xA9"
#define AR_2     "\xD9\xA2" /* U+0662 */
#define AR_5     "\xD9\xA5" /* U+0665 */
#define FA_0     "\xDB\xB0" /* U+06F0 */
#define FA_2     "\xDB\xB2" /* U+06F2 */
#define FA_3     "\xDB\xB3" /* U+06F3 */
#define FA_5     "\xDB\xB5" /* U+06F5 */

/* Arabic presentation forms */
#define AR_DAL_ISOLATED      "\xEF\xBA\xA9" /* U+FEA9 */
#define AR_DAL_FINAL         "\xEF\xBA\xAA" /* U+FEAA */
#define AR_AIN_INITIAL       "\xEF\xBB\x8B" /* U+FECB */
#define AR_SEEN_INITIAL      "\xEF\xBA\xB3" /* U+FEB3 */
#define AR_MEEM_ISOLATED     "\xEF\xBB\xA1" /* U+FEE1 */
#define AR_KEHEH_ISOLATED    "\xEF\xAE\x8E" /* U+FB8E */
#define AR_LAM_ALEF_ISOLATED "\xEF\xBB\xBB" /* U+FEFB */
#define AR_LAM_ALEF_FINAL    "\xEF\xBB\xBC" /* U+FEFC */

#define SHALOM  SHIN LAMED VAV MEM_F
#define OLAM    AYIN VAV LAMED MEM_F
#define GIRSA   GIMEL RESH SAMEKH HE
#define SHALOM_DRAWN MEM_F VAV LAMED SHIN
#define OLAM_DRAWN   MEM_F LAMED VAV AYIN
#define GIRSA_DRAWN  HE SAMEKH RESH GIMEL

struct bidi_case
{
   const char *name;
   const char *typed;
   const char *drawn;
};

static const struct bidi_case cases[] = {
   { "a Hebrew line is reversed",
      SHALOM " " OLAM,
      OLAM_DRAWN " " SHALOM_DRAWN },
   { "an English word in a Hebrew line keeps its order",
      HE GIMEL DALET RESH VAV TAV " MIDI",
      "MIDI " TAV VAV RESH DALET GIMEL HE },
   { "a number in a Hebrew line keeps its order",
      GIRSA " 1.22",
      "1.22 " GIRSA_DRAWN },
   { "brackets around a Hebrew line are mirrored",
      "(" SHIN MEM VAV RESH ")",
      "(" RESH VAV MEM SHIN ")" },
   { "Hebrew in brackets in an English line",
      "Load ROM (" QOF VAV BET TSADI_F ")",
      "Load ROM (" TSADI_F BET VAV QOF ")" },
   { "a number after Hebrew in an English line",
      "abc " SHALOM " 123",
      "abc 123 " SHALOM_DRAWN },
   { "each line is laid out on its own",
      SHALOM "\n" OLAM,
      SHALOM_DRAWN "\n" OLAM_DRAWN },
   { "an English word in brackets inside a Hebrew line",
      TET AYIN NUN_F " (ROM) " QOF VAV BET TSADI_F,
      TSADI_F BET VAV QOF " (ROM) " NUN_F AYIN TET },
   { "a version in brackets after Hebrew",
      GIRSA " (v1.22)",
      "(v1.22) " GIRSA_DRAWN },
   { "a currency sign stays with its number",
      MEM HET YOD RESH " $5",
      "$5 " RESH YOD HET MEM },
   { "a percentage before Hebrew",
      "50% " HE VAV SHIN LAMED MEM_F,
      MEM_F LAMED SHIN VAV HE " 50%" },
   { "lines of different directions",
      "Load " SHALOM " ROM\n" SHALOM " Load " OLAM,
      "Load " SHALOM_DRAWN " ROM\n" OLAM_DRAWN " Load " SHALOM_DRAWN },
   { "trailing spaces stay at the end of a Hebrew line",
      SHALOM "  ",
      "  " SHALOM_DRAWN },
   { "Arabic with digits",
      AR_AIN AR_DAL AR_DAL " 25",
      "25 " AR_DAL_ISOLATED AR_DAL_FINAL AR_AIN_INITIAL },
   { "lam and alef make one glyph",
      AR_LAM AR_ALEF,
      AR_LAM_ALEF_ISOLATED },
   { "lam and alef inside a word",
      AR_SEEN AR_LAM AR_ALEF AR_MEEM,
      AR_MEEM_ISOLATED AR_LAM_ALEF_FINAL AR_SEEN_INITIAL },
   { "an English word and a version in brackets inside a Hebrew line",
      SHALOM " ROM (v2) " OLAM,
      OLAM_DRAWN " ROM (v2) " SHALOM_DRAWN },
   { "a name and a version in brackets inside a Hebrew line",
      GIRSA " Snes9x (1.62) " OLAM,
      OLAM_DRAWN " Snes9x (1.62) " GIRSA_DRAWN },
   { "Hebrew in brackets after Hebrew in an English line",
      "abc " SHALOM " (" QOF VAV BET TSADI_F ") def",
      "abc (" TSADI_F BET VAV QOF ") " SHALOM_DRAWN " def" },
   { "Arabic-Indic digits keep their order",
      AR_AIN AR_DAL AR_DAL " " AR_2 AR_5,
      AR_2 AR_5 " " AR_DAL_ISOLATED AR_DAL_FINAL AR_AIN_INITIAL },
   { "Persian digits keep their order",
      FA_5 FA_0 " " AR_AIN AR_DAL AR_DAL,
      AR_DAL_ISOLATED AR_DAL_FINAL AR_AIN_INITIAL " " FA_5 FA_0 },
   { "Persian digits after English",
      "CRC" FA_3 FA_2 ": ",
      "CRC" FA_3 FA_2 ": " },
   { "a number after Arabic is an Arabic number and leaves % out",
      AR_RAY " 50%",
      "%50 " AR_RAY },
   { "a number after Hebrew takes its %",
      ALEF " 50%",
      "50% " ALEF },
   { "a comma joins Arabic numbers",
      ALEF " " AR_1 "," AR_2 " a",
      "a " AR_1 "," AR_2 " " ALEF },
   { "a plus does not join Arabic numbers",
      ALEF " " AR_1 "+" AR_2,
      AR_2 "+" AR_1 " " ALEF },
   { "the Arabic comma joins numbers",
      "1" AR_COMMA "2 " ALEF,
      ALEF " 1" AR_COMMA "2" },
   { "Arabic numbers in an English line",
      "a " AR_1 AR_2 "-" AR_3 " b",
      "a " AR_3 "-" AR_1 AR_2 " b" },
   { "a Hebrew point stays after its letter",
      "abc " SHIN QAMATS LAMED VAV HOLAM MEM_F " def",
      "abc " MEM_F VAV HOLAM LAMED SHIN QAMATS " def" },
   { "a mark on a bracket goes with it",
      ALEF " [b]" ACUTE " " BET,
      BET " [" ACUTE "b] " ALEF },
   { "a diacritic after lam and alef stays after the glyph",
      AR_SEEN AR_LAM AR_ALEF AR_FATHA AR_MEEM,
      AR_MEEM_ISOLATED AR_LAM_ALEF_FINAL AR_FATHA AR_SEEN_INITIAL },
   { "lines before the first Hebrew one are copied as typed",
      "\xD0\x9F\xD1\x80\xD0\xB8 (1.22)\nab\n" SHALOM,
      "\xD0\x9F\xD1\x80\xD0\xB8 (1.22)\nab\n" SHALOM_DRAWN },
   { "lam and alef at the start of a line",
      AR_KEHEH "\n" AR_LAM AR_ALEF,
      AR_KEHEH_ISOLATED "\n" AR_LAM_ALEF_ISOLATED },
   { "a diacritic between lam and alef",
      AR_SEEN AR_LAM AR_FATHA AR_ALEF AR_MEEM,
      AR_MEEM_ISOLATED AR_LAM_ALEF_FINAL AR_FATHA AR_SEEN_INITIAL },
   { "Arabic and Hebrew with brackets and a version",
      GIRSA " (v1.22) " AR_LAM AR_ALEF,
      AR_LAM_ALEF_ISOLATED " (v1.22) " GIRSA_DRAWN },
};

/* Text with nothing right-to-left */
static const char *const unchanged[] = {
   "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E (1.22) \xE3\x83\x86\xE3\x82\xB9\xE3\x83\x88",
   "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82, 1.22 (\xD0\xBC\xD0\xB8\xD1\x80)",
   "Caf\xC3\xA9 d\xC3\xA9j\xC3\xA0 vu: 1,5 \xE2\x82\xAC \xC2\xAB" "a" "\xC2\xBB",
   "smile \xF0\x9F\x98\x80 <ok>\nline two [x]",
};

/* A font for measuring: every codepoint has its own advance, mirrored
 * brackets share one, U+2603 is missing and '?' stands in for it. Each
 * lookup is counted. */
static unsigned long           glyph_lookups = 0;
static struct font_glyph       glyph_ring[64];
static struct font_glyph       glyph_q_slot;
static unsigned                glyph_next    = 0;

static const struct font_glyph *fake_get_glyph(void *data, uint32_t code)
{
   struct font_glyph *g;
   (void)data;
   glyph_lookups++;
   if (code == 0x2603)
      return NULL;
   if (code == '?')
   {
      glyph_q_slot.advance_x = 7;
      return &glyph_q_slot;
   }
   g = &glyph_ring[glyph_next++ & 63];
   memset(g, 0, sizeof(*g));
   if (code < 0x80 && strchr("()[]{}<>", (int)code))
      g->advance_x = 5;
   else
      g->advance_x = 3 + (int)(((code * 2654435761u) >> 27) % 13);
   return g;
}

static font_renderer_driver_t fake_renderer;

static int measure(const char *msg, size_t len)
{
   fake_renderer.get_glyph = fake_get_glyph;
   return font_renderer_get_message_width(&fake_renderer,
         &fake_renderer, msg, len, 1.0f);
}

/* The width of what is drawn: msg laid out, then each glyph's advance */
static int drawn_width(const char *msg, size_t len)
{
   static unsigned char buf[1536];
   size_t               out_len;
   const char          *p   = font_driver_reshape_msg(msg, len, buf,
         sizeof(buf), &out_len);
   const char          *end = p + out_len;
   int                  w   = 0;
   while (p < end && *p)
   {
      const struct font_glyph *g = fake_get_glyph(NULL, utf8_walk(&p));
      if (!g)
         g = fake_get_glyph(NULL, '?');
      w += g->advance_x;
   }
   return w;
}

#define CANARY      16
#define CANARY_BYTE 0xA5

/* Lays msg_len bytes of msg out into a buffer of len bytes with a
 * canary after it, and copies the result into out. Returns the
 * result's length, or -1 when the canary was written, the result is
 * not terminated inside the buffer or its length is wrong. *same is
 * set when msg itself came back. */
static long run(const char *msg, size_t msg_len, size_t len,
      char *out, size_t out_size, int *same)
{
   unsigned char *buf = (unsigned char*)malloc(len + CANARY);
   size_t         res_len = (size_t)-1;
   long           ret     = -1;
   char          *res;
   size_t         i;

   if (!buf)
      return -1;
   memset(buf, CANARY_BYTE, len + CANARY);
   res   = font_driver_reshape_msg(msg, msg_len, buf, len, &res_len);
   *same = (res == msg);

   for (i = len; i < len + CANARY; i++)
      if (buf[i] != CANARY_BYTE)
         break;

   if (i == len + CANARY && res_len < out_size)
   {
      if (res == msg)
         ret = (res_len == msg_len) ? (long)res_len : -1;
      else if (res == (char*)buf && res_len < len && res[res_len] == '\0')
         ret = (long)res_len;
      if (ret >= 0)
      {
         memcpy(out, res, res_len);
         out[res_len] = '\0';
      }
   }
   free(buf);
   return ret;
}

/* Whole UTF-8 sequences only: a cut never splits a character */
static int utf8_whole(const char *s, size_t n)
{
   size_t i = 0;
   while (i < n)
   {
      unsigned char c = (unsigned char)s[i];
      size_t        k = c < 0x80 ? 1 : c < 0xC0 ? 0 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
      size_t        j;
      if (!k || i + k > n)
         return 0;
      for (j = 1; j < k; j++)
         if (((unsigned char)s[i + j] & 0xC0) != 0x80)
            return 0;
      i += k;
   }
   return 1;
}

int main(void)
{
   static char out[4096];
   static char in[4096];
   size_t      i;
   int         same;
   long        n;

   /* 1. The expected layouts */
   for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
   {
      n = run(cases[i].typed, strlen(cases[i].typed), 1536,
            out, sizeof(out), &same);
      CHECK(n >= 0, cases[i].name);
      if (n >= 0 && strcmp(out, cases[i].drawn))
      {
         size_t k;
         printf("  FAIL: %s\n    got: ", cases[i].name);
         for (k = 0; out[k]; k++)
            printf("%02X ", (unsigned char)out[k]);
         printf("\n   want: ");
         for (k = 0; cases[i].drawn[k]; k++)
            printf("%02X ", (unsigned char)cases[i].drawn[k]);
         printf("\n");
         fails++;
      }
   }

   /* 2. Plain ASCII comes back as it is, unchanged and uncopied */
   {
      const char *ascii = "Load ROM (1.22) [x] <y> {z} a-b, c/d: 50% #1 $2";
      n = run(ascii, strlen(ascii), 1536, out, sizeof(out), &same);
      CHECK(n == (long)strlen(ascii) && same,
            "ASCII text is handed back as it is");
   }

   /* 3. Nothing right-to-left: byte for byte */
   for (i = 0; i < sizeof(unchanged) / sizeof(unchanged[0]); i++)
   {
      n = run(unchanged[i], strlen(unchanged[i]), 1536,
            out, sizeof(out), &same);
      CHECK(n == (long)strlen(unchanged[i]) && !strcmp(out, unchanged[i]),
            "text with nothing right-to-left is byte for byte the same");
   }

   /* 4. Only msg_len bytes are read: an exact copy with no terminator */
   {
      const char *msg  = SHALOM " " OLAM;
      size_t      part = strlen(SHALOM);
      char       *copy = (char*)malloc(part);
      if (copy)
      {
         memcpy(copy, msg, part);
         n = run(copy, part, 1536, out, sizeof(out), &same);
         CHECK(n >= 0 && !strcmp(out, SHALOM_DRAWN),
               "only the first msg_len bytes are laid out");
         free(copy);
      }
   }

   /* 5. Lines at and over the 256 characters laid out on the stack */
   {
      static char want[4096];
      size_t      k;
      int         ok;
      in[0] = '\0';
      for (k = 0; k < 256; k++)
         strcat(in, ALEF);
      in[0] = BET[0];
      in[1] = BET[1];
      for (k = 256; k <= 257; k++)
      {
         size_t m;
         n  = run(in, strlen(in), 1536, out, sizeof(out), &same);
         ok = n == (long)strlen(in) && !memcmp(out + n - 2, BET, 2);
         for (m = 0; ok && m + 2 < (size_t)n; m += 2)
            ok = !memcmp(out + m, ALEF, 2);
         CHECK(ok, k == 256
               ? "a line of 256 characters is laid out right to left"
               : "a line of 257 characters is laid out right to left");
         strcat(in, ALEF);
      }

      in[0]   = '\0';
      want[0] = '\0';
      for (k = 0; k < 60; k++)
      {
         strcat(in, k ? " " AR_SEEN AR_LAM AR_ALEF AR_MEEM
               : AR_SEEN AR_LAM AR_ALEF AR_MEEM);
         strcat(want, k ? " " AR_MEEM_ISOLATED AR_LAM_ALEF_FINAL AR_SEEN_INITIAL
               : AR_MEEM_ISOLATED AR_LAM_ALEF_FINAL AR_SEEN_INITIAL);
      }
      n = run(in, strlen(in), 1536, out, sizeof(out), &same);
      CHECK(n >= 0 && !strcmp(out, want),
            "an Arabic line of 299 characters is shaped and laid out");

      /* 20 brackets deep and 20 pairs in a row, past the pairs kept */
      in[0]   = '\0';
      want[0] = '\0';
      for (k = 0; k < 20; k++)
         strcat(in, "(");
      strcat(in, SHALOM);
      for (k = 0; k < 20; k++)
         strcat(in, ")");
      for (k = 0; k < 20; k++)
         strcat(want, "(");
      strcat(want, SHALOM_DRAWN);
      for (k = 0; k < 20; k++)
         strcat(want, ")");
      n = run(in, strlen(in), 1536, out, sizeof(out), &same);
      CHECK(n >= 0 && !strcmp(out, want), "brackets 20 deep");
      in[0] = '\0';
      for (k = 0; k < 20; k++)
         strcat(in, k ? " (" ALEF ")" : "(" ALEF ")");
      n = run(in, strlen(in), 1536, out, sizeof(out), &same);
      CHECK(n >= 0 && !strcmp(out, in), "20 bracket pairs in a row");
   }

   /* 6. Input longer than the buffer is cut and still laid out */
   {
      size_t k;
      int    ok = 1;
      in[0] = '\0';
      for (k = 0; k < 300; k++)
         strcat(in, SHALOM "\n");
      n = run(in, strlen(in), 1536, out, sizeof(out), &same);
      CHECK(n > 0 && n < 1536 && utf8_whole(out, (size_t)n),
            "a message over the buffer is cut whole inside it");
      for (k = 0; n > 0 && k + 9 <= (size_t)n; k += 9)
         ok = ok && !memcmp(out + k, SHALOM_DRAWN "\n", 9);
      CHECK(ok, "every line of a cut message is laid out");

      in[0] = '\0';
      strcat(in, BET);
      for (k = 0; k < 699; k++)
         strcat(in, ALEF);
      /* Hebrew stays two bytes, so 1023 of the 1535 bytes are kept */
      n  = run(in, strlen(in), 1536, out, sizeof(out), &same);
      ok = n == 1022 && !memcmp(out + n - 2, BET, 2);
      for (k = 0; ok && k + 2 < (size_t)n; k += 2)
         ok = !memcmp(out + k, ALEF, 2);
      CHECK(ok, "a long line cut to the buffer is laid out right to left");

      in[0] = '\0';
      for (k = 0; k < 100; k++)
         strcat(in, SHALOM " ");
      n = run(in, strlen(in), 64, out, sizeof(out), &same);
      CHECK(n > 0 && n < 64 && utf8_whole(out, (size_t)n)
            && !memcmp(out + n - 2, SHIN, 2),
            "a cut line keeps its start, drawn rightmost");
   }

   /* 7. Brackets nested deeper than 16 are still paired */
   {
      static char want[4096];
      size_t      k;
      char       *p = in;
      char       *w = want;
      strcpy(p, ALEF " b");
      p += strlen(p);
      strcpy(w, BET " b");
      w += strlen(w);
      for (k = 0; k < 40; k++)
      {
         *p++ = '(';
         *w++ = '(';
      }
      *p++ = 'c';
      *w++ = 'c';
      for (k = 0; k < 40; k++)
      {
         *p++ = ')';
         *w++ = ')';
      }
      strcpy(p, " " BET);
      strcpy(w, " " ALEF);
      n = run(in, strlen(in), 1536, out, sizeof(out), &same);
      CHECK(n >= 0 && !strcmp(out, want),
            "40 nested pairs keep their closing brackets with the English");
   }

   /* 8. Laying a line out allocates nothing, however long */
   {
      static unsigned char buf[1536];
      size_t               k;
      size_t               len;
      unsigned long        before;
      int                  ok = 1;

      in[0] = '\0';
      strcat(in, SHALOM "\n");
      for (k = 0; k < 900; k++)
         strcat(in, "a");
      before = test_mallocs;
      font_driver_reshape_msg(in, strlen(in), buf, sizeof(buf), &len);
      CHECK(test_mallocs == before && len == strlen(in)
            && !memcmp(buf + 9, in + 9, 900),
            "a 900 character English line beside Hebrew allocates nothing");

      in[0] = '\0';
      for (k = 0; k < 400; k++)
         strcat(in, k & 1 ? "b " : ALEF " ");
      before = test_mallocs;
      font_driver_reshape_msg(in, strlen(in), buf, sizeof(buf), &len);
      ok = test_mallocs == before && len == strlen(in);
      /* Drawn right to left: the last word typed comes first */
      ok = ok && !memcmp(buf, " b " ALEF, 5);
      CHECK(ok, "a 1000 byte mixed line is laid out without allocating");
   }

   /* 9. Arabic grows by half: the most input kept still fits */
   {
      size_t k;
      in[0] = '\0';
      for (k = 0; k < 600; k++)
         strcat(in, AR_SEEN);
      n = run(in, strlen(in), 1536, out, sizeof(out), &same);
      CHECK(n == 511 * 3 && utf8_whole(out, (size_t)n),
            "1022 bytes of Arabic shaped into a 1536 byte buffer");
   }

   /* 10. Right-to-left text is found at every alignment */
   {
      size_t k;
      for (k = 0; k < 40; k++)
      {
         size_t start;
         for (start = 0; start < 8; start++)
         {
            char *msg = in + start;
            memset(msg, 'a', k);
            strcpy(msg + k, ALEF);
            n = run(msg, k + 2, 1536, out, sizeof(out), &same);
            if (n != (long)(k + 2) || same || memcmp(out, msg, k + 2))
            {
               printf("  FAIL: Hebrew after %u bytes at offset %u\n",
                     (unsigned)k, (unsigned)start);
               fails++;
            }
         }
      }
   }

   /* 11. Arabic is measured as it is drawn */
   {
      static const char *const pieces[] = {
         AR_LAM, AR_LAM, AR_ALEF, "\xD8\xA2", "\xD8\xA3", "\xD8\xA5",
         "\xD8\xA8", AR_SEEN, AR_MEEM, "\xD8\xA9", AR_FATHA,
         "\xD9\x91", AR_KEHEH, AR_DAL, AR_AIN, AR_2, AR_5, FA_3,
         "\xD9\xAA", "\xD8\x80", " ", " ", "a", "1", "(", ")", "%",
         "\n", ALEF, BET };
      static const char *const fixed[] = {
         AR_SEEN AR_LAM AR_ALEF AR_MEEM,
         AR_LAM AR_FATHA AR_ALEF " " AR_LAM AR_ALEF,
         "CRC " AR_AIN AR_DAL AR_DAL " (" AR_2 AR_5 "%)",
         AR_KEHEH "\n" AR_LAM AR_ALEF,
         " h" AR_LAM AR_ALEF };
      unsigned long k;
      unsigned      seed = 12345;
      int           bad  = 0;
      for (k = 0; k < sizeof(fixed) / sizeof(fixed[0]); k++)
         if (measure(fixed[k], strlen(fixed[k]))
               != drawn_width(fixed[k], strlen(fixed[k])))
            bad++;
      CHECK(!bad, "Arabic labels measure as wide as they are drawn");
      for (k = 0; k < 20000 && bad < 5; k++)
      {
         size_t m, parts;
         in[0] = '\0';
         seed  = seed * 1103515245u + 12345u;
         parts = 1 + (seed >> 16) % 24;
         for (m = 0; m < parts; m++)
         {
            seed = seed * 1103515245u + 12345u;
            strcat(in, pieces[(seed >> 16)
                  % (sizeof(pieces) / sizeof(pieces[0]))]);
         }
         if (measure(in, strlen(in)) != drawn_width(in, strlen(in)))
         {
            printf("  FAIL: measured %d, drawn %d:",
                  measure(in, strlen(in)), drawn_width(in, strlen(in)));
            for (m = 0; in[m]; m++)
               printf(" %02X", (unsigned char)in[m]);
            printf("\n");
            bad++;
            fails++;
         }
      }
   }

   /* 12. A lam after an English letter is not joined to it */
   {
      static unsigned char buf[64];
      size_t               len;
      const char          *msg = " h" AR_LAM AR_ALEF;
      font_driver_reshape_msg(msg, strlen(msg), buf, sizeof(buf), &len);
      CHECK(len == 5 && !memcmp(buf, " h" AR_LAM_ALEF_ISOLATED, 5),
            "lam and alef after English take the isolated ligature");
   }

   /* 13. A font missing nothing is asked once per glyph; '?' is looked
    * up only for a glyph the font lacks */
   {
      unsigned long before = glyph_lookups;
      int           w      = measure("Hello", 5);
      CHECK(glyph_lookups - before == 5 && w == drawn_width("Hello", 5),
            "five glyphs measured with five lookups");
      before = glyph_lookups;
      w      = measure("a\xE2\x98\x83" "b\xE2\x98\x83", 8);
      CHECK(glyph_lookups - before == 5
            && w == measure("a?b?", 4),
            "a missing glyph is measured as '?', looked up once");
   }

   /* 14. Every buffer size from 1 to 64 for a mixed line */
   {
      const struct bidi_case *mixed = &cases[sizeof(cases) / sizeof(cases[0]) - 1];
      size_t                  len;
      size_t                  typed = strlen(mixed->typed);
      for (len = 1; len <= 64; len++)
      {
         int ok;
         n  = run(mixed->typed, typed, len, out, sizeof(out), &same);
         ok = n >= 0 && n < (long)len && utf8_whole(out, (size_t)n);
         if (ok && len >= typed * 2 + 1)
            ok = !strcmp(out, mixed->drawn);
         if (!ok)
         {
            printf("  FAIL: a buffer of %u bytes\n", (unsigned)len);
            fails++;
         }
      }
   }

   if (fails)
   {
      printf("%d failure(s)\n", fails);
      return 1;
   }
   printf("font_bidi_test: all checks passed\n");
   return 0;
}
