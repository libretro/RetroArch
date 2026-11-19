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

#include "../../../gfx/font_driver.c"

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
      n  = run(in, strlen(in), 1536, out, sizeof(out), &same);
      ok = n == 766 && !memcmp(out + n - 2, BET, 2);
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

   /* 7. Every buffer size from 1 to 64 for a mixed line */
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
