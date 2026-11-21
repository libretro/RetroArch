/* Right-to-left labels in the smooth ticker.
 *
 * A label too wide for its field scrolls through it. Each frame here
 * runs gfx_animation_ticker_smooth() and draws what it gives through
 * font_driver_render_msg(), as the menus do, into a renderer that
 * records the text and measures it with widths that differ per
 * codepoint, so a letter drawn in another Arabic form has another
 * width. What is drawn has to be a window onto the whole label as it
 * would be drawn unscrolled:
 *
 *   - the glyphs drawn are a run of the glyphs the whole label draws
 *     (the strip of label, spacer, label for a looping ticker), with
 *     the same Arabic forms;
 *   - every drawn glyph stands where it stands in the whole label,
 *     shifted by one amount per frame, and that shift moves at most
 *     one pixel from one frame to the next;
 *   - a right-to-left label starts at its right end, and a bouncing
 *     ticker reaches both ends;
 *   - what is drawn fits the field and is as wide as the ticker says.
 *
 * Labels with nothing right-to-left are checked the same way. Strings
 * are spelled in escapes so the source stays ASCII. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <encodings/utf.h>

#include "../../../gfx/font_driver.h"
#include "../../../gfx/gfx_animation.h"

int read_should_fail = 0;

/* The vertical ticker's wrapping asks for it; nothing here wraps */
const char *msg_hash_get_wideglyph_str(void) { return NULL; }

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

#define TICKER_FRAMES 1200
#define DRAWN_MAX     2048

/* What the last font_driver_render_msg() handed the renderer */
static char   drawn[DRAWN_MAX];
static size_t drawn_len;

static unsigned glyph_width(uint32_t cp)
{
   if (cp < 0x80)
      return 6;
   /* U+202D drawn would be the font's missing-glyph mark */
   if (cp == 0x202D)
      return 9;
   return 5 + (cp % 7);
}

static int fake_width(void *data, const char *msg, size_t len, float scale)
{
   const char *s   = msg;
   const char *end = msg + len;
   int         w   = 0;
   (void)data;
   while (s < end && *s)
      w += (int)glyph_width(utf8_walk(&s));
   return (int)(w * scale);
}

static void fake_render(void *userdata, void *data, const char *msg,
      size_t msg_len, const struct font_params *params)
{
   (void)userdata;
   (void)data;
   (void)params;
   if (msg_len >= DRAWN_MAX)
      msg_len = DRAWN_MAX - 1;
   memcpy(drawn, msg, msg_len);
   drawn[msg_len] = '\0';
   drawn_len      = msg_len;
}

static font_renderer_t fake_font = {
   NULL, NULL, fake_render, "ticker", NULL, NULL, NULL, fake_width, NULL
};

static font_data_t font;

static void draw(const char *s)
{
   drawn_len = 0;
   drawn[0]  = '\0';
   if (*s)
      font_driver_render_msg(NULL, s, strlen(s), NULL, &font);
}

/* x of byte offset off of s, drawn from 0 */
static int x_at(const char *s, size_t off)
{
   return fake_width(NULL, s, off, 1.0f);
}

/* Hebrew */
#define ALEF    "\xD7\x90"
#define BET     "\xD7\x91"
#define GIMEL   "\xD7\x92"
#define DALET   "\xD7\x93"
#define HE      "\xD7\x94"
#define VAV     "\xD7\x95"
#define ZAYIN   "\xD7\x96"
#define HET     "\xD7\x97"
#define TET     "\xD7\x98"
#define YOD     "\xD7\x99"
#define KAF     "\xD7\x9B"
#define LAMED   "\xD7\x9C"
#define MEM     "\xD7\x9E"
#define NUN     "\xD7\xA0"
#define SAMEKH  "\xD7\xA1"
#define AYIN    "\xD7\xA2"
#define PE      "\xD7\xA4"
#define TSADI   "\xD7\xA6"
#define QOF     "\xD7\xA7"
#define RESH    "\xD7\xA8"
#define SHIN    "\xD7\xA9"
#define TAV     "\xD7\xAA"
#define QAMATS  "\xD6\xB8"

/* Arabic */
#define AR_ALEF  "\xD8\xA7"
#define AR_BEH   "\xD8\xA8"
#define AR_TEH   "\xD8\xAA"
#define AR_JEEM  "\xD8\xAC"
#define AR_DAL   "\xD8\xAF"
#define AR_REH   "\xD8\xB1"
#define AR_SEEN  "\xD8\xB3"
#define AR_AIN   "\xD8\xB9"
#define AR_FEH   "\xD9\x81"
#define AR_QAF   "\xD9\x82"
#define AR_KAF   "\xD9\x83"
#define AR_LAM   "\xD9\x84"
#define AR_MEEM  "\xD9\x85"
#define AR_NOON  "\xD9\x86"
#define AR_HEH   "\xD9\x87"
#define AR_WAW   "\xD9\x88"
#define AR_YEH   "\xD9\x8A"
#define AR_FATHA "\xD9\x8E"

struct ticker_case
{
   const char *name;
   const char *label;
   bool        rtl;
};

static const struct ticker_case cases[] = {
   { "an English label",
      "Show Advanced Settings In The Quick Menu", false },
   { "a Hebrew label",
      ALEF BET GIMEL DALET " " HE VAV ZAYIN HET " " TET YOD KAF LAMED
      " " MEM NUN SAMEKH AYIN " " PE TSADI QOF RESH " " SHIN TAV, true },
   { "a Hebrew label with points",
      SHIN QAMATS LAMED VAV MEM " " AYIN VAV LAMED MEM " " BET QAMATS RESH
      " " GIMEL DALET HE " " ZAYIN KAF TAV, true },
   { "an Arabic label",
      AR_TEH AR_FEH AR_AIN AR_YEH AR_LAM " " AR_ALEF AR_LAM AR_JEEM AR_WAW
      AR_DAL AR_HEH " " AR_ALEF AR_LAM AR_KAF AR_ALEF AR_MEEM AR_LAM AR_HEH
      " " AR_LAM AR_LAM AR_SEEN AR_QAF AR_NOON AR_BEH AR_REH, true },
   { "an Arabic label with a lam-alef and a mark",
      AR_ALEF AR_LAM AR_ALEF AR_SEEN AR_TEH AR_MEEM AR_REH AR_ALEF AR_REH
      " " AR_BEH AR_FATHA AR_AIN AR_DAL " " AR_ALEF AR_LAM AR_TEH AR_HEH
      AR_YEH AR_QAF " " AR_MEEM AR_NOON " " AR_JEEM AR_DAL AR_YEH AR_DAL,
      true },
   { "an English word in a Hebrew label",
      HE GIMEL DALET RESH VAV TAV " MIDI " QOF VAV BET TSADI " " SHIN MEM
      AYIN " " LAMED HE PE AYIN LAMED HE, true },
   { "Hebrew in an English label",
      "Selected language for the interface: " AYIN BET RESH YOD TAV
      " (" ALEF LAMED PE BET YOD TAV ")", false },
};

/* Byte offset of the occurrence of needle in hay whose shift from x
 * lies nearest prev, or -1 */
static long find_window(const char *hay, size_t hay_len,
      const char *needle, size_t n, int x, int prev, int *shift)
{
   size_t i;
   long   best = -1;
   int    best_d = 0;
   if (n > hay_len)
      return -1;
   for (i = 0; i + n <= hay_len; i++)
   {
      int s, d;
      if ((hay[i] & 0xC0) == 0x80 || memcmp(hay + i, needle, n))
         continue;
      s = x - x_at(hay, i);
      d = s > prev ? s - prev : prev - s;
      if (best < 0 || d < best_d)
      {
         best   = (long)i;
         best_d = d;
         *shift = s;
      }
   }
   return best;
}

static void run_case(const struct ticker_case *c, enum gfx_animation_ticker_type type)
{
   char        full[DRAWN_MAX];
   char        strip[DRAWN_MAX * 2 + 64];
   char        dst[512];
   char        msg[256];
   size_t      full_len, strip_len;
   int         full_w, spacer_w, period;
   int         prev_shift = 0;
   int         min_shift  = 0x7FFFFFFF;
   int         max_shift  = -0x7FFFFFFF;
   unsigned    field;
   unsigned    jumps      = 0;
   unsigned    broken     = 0;
   unsigned    leaks      = 0;
   unsigned    overflows  = 0;
   unsigned    max_jump   = 0;
   unsigned    backwards  = 0;
   unsigned    idx;
   bool        loop       = (type == TICKER_TYPE_LOOP);

   draw(c->label);
   memcpy(full, drawn, drawn_len + 1);
   full_len = drawn_len;
   full_w   = x_at(full, full_len);
   field    = (unsigned)full_w / 2;

   /* The strip a looping ticker scrolls over, as drawn */
   spacer_w = x_at(TICKER_SPACER_DEFAULT, strlen(TICKER_SPACER_DEFAULT));
   memcpy(strip, full, full_len);
   memcpy(strip + full_len, TICKER_SPACER_DEFAULT,
         strlen(TICKER_SPACER_DEFAULT));
   strip_len = full_len + strlen(TICKER_SPACER_DEFAULT);
   memcpy(strip + strip_len, full, full_len);
   strip_len += full_len;
   strip[strip_len] = '\0';
   period = full_w + spacer_w;

   for (idx = 0; idx < TICKER_FRAMES; idx++)
   {
      gfx_animation_ctx_ticker_smooth_t t;
      unsigned x_offset = 0;
      unsigned width    = 0;
      int      shift    = 0;
      long     at;

      memset(&t, 0, sizeof(t));
      t.font          = &font;
      t.font_scale    = 1.0f;
      t.type_enum     = type;
      t.spacer        = NULL;
      t.idx           = idx;
      t.selected      = true;
      t.field_width   = field;
      t.src_str       = c->label;
      t.dst_str       = dst;
      t.dst_str_len   = sizeof(dst);
      t.x_offset      = &x_offset;
      t.dst_str_width = &width;
      gfx_animation_ticker_smooth(&t);

      draw(dst);
      if (strstr(drawn, "\xE2\x80\xAD"))
         leaks++;
      if (     (unsigned)x_at(drawn, drawn_len) != width
            || x_offset + width > field)
         overflows++;
      if (!drawn_len)
         continue;

      if (loop)
         at = find_window(strip, strip_len, drawn, drawn_len,
               (int)x_offset, prev_shift, &shift);
      else
         at = find_window(full, full_len, drawn, drawn_len,
               (int)x_offset, prev_shift, &shift);
      if (at < 0)
      {
         broken++;
         continue;
      }

      if (idx == 0)
      {
         if (c->rtl && !loop && shift != (int)field - full_w)
         {
            sprintf(msg,
                  "%s: starts at shift %d, its right end is at %d",
                  c->name, shift, (int)field - full_w);
            CHECK(0, msg);
         }
         if (!c->rtl && shift != 0)
         {
            sprintf(msg,
                  "%s: starts at shift %d, not at its left end",
                  c->name, shift);
            CHECK(0, msg);
         }
      }
      else
      {
         int d = shift - prev_shift;
         if (loop && (d > period / 2 || d < -period / 2))
            d += (d > 0) ? -period : period;
         /* A looping label only ever moves on: right when it reads
          * right to left */
         if (loop && (c->rtl ? d < 0 : d > 0))
            backwards++;
         if (d < 0)
            d = -d;
         if (d > 1)
         {
            jumps++;
            if ((unsigned)d > max_jump)
               max_jump = (unsigned)d;
         }
      }
      prev_shift = shift;
      if (shift < min_shift)
         min_shift = shift;
      if (shift > max_shift)
         max_shift = shift;
   }

   printf("  %-44s %s: %u jumps (largest %u px), %u frames broken, "
         "%u backwards, %u marks drawn, %u misfits\n", c->name,
         loop ? "loop  " : "bounce", jumps, max_jump, broken, backwards,
         leaks, overflows);

   sprintf(msg, "%s: glyphs jump between frames", c->name);
   CHECK(jumps == 0, msg);
   sprintf(msg, "%s: scrolls the wrong way", c->name);
   CHECK(backwards == 0, msg);
   sprintf(msg, "%s: drawn glyphs are not a run of the label",
         c->name);
   CHECK(broken == 0, msg);
   sprintf(msg, "%s: the override mark is drawn", c->name);
   CHECK(leaks == 0, msg);
   sprintf(msg, "%s: drawn text overflows or is mismeasured",
         c->name);
   CHECK(overflows == 0, msg);
   if (!loop && broken == 0)
   {
      sprintf(msg, "%s: scrolls over %d..%d, not %d..0",
            c->name, min_shift, max_shift, (int)field - full_w);
      CHECK(min_shift == (int)field - full_w && max_shift == 0, msg);
   }
}

/* Glyphs drawn from s to s + n: characters, a mark counted with the
 * glyph before it */
static int glyphs_in(const char *s, size_t n)
{
   const char *p   = s;
   const char *end = s + n;
   int         g   = 0;
   while (p < end)
   {
      const char *q = p;
      utf8_walk(&q);
      if (!font_driver_is_mark(p, (size_t)(q - p)))
         g++;
      p = q;
   }
   return g;
}

/* The stepped ticker over a selected label: every frame a run of the
 * whole line as drawn, one glyph on from the frame before in reading
 * order at most, the first one at the label's reading start */
static void run_stepped_case(const struct ticker_case *c,
      enum gfx_animation_ticker_type type)
{
   char        full[DRAWN_MAX];
   char        strip[DRAWN_MAX * 2 + 64];
   char        dst[512];
   char        msg[256];
   size_t      full_len, strip_len, hay_len;
   const char *hay;
   const char *p;
   unsigned    idx, chars = 0;
   unsigned    broken = 0, jumps = 0, backwards = 0, leaks = 0;
   long        prev   = -1;
   bool        loop   = (type == TICKER_TYPE_LOOP);

   draw(c->label);
   memcpy(full, drawn, drawn_len + 1);
   full_len = drawn_len;
   memcpy(strip, full, full_len);
   memcpy(strip + full_len, TICKER_SPACER_DEFAULT,
         strlen(TICKER_SPACER_DEFAULT));
   strip_len = full_len + strlen(TICKER_SPACER_DEFAULT);
   memcpy(strip + strip_len, full, full_len);
   strip_len += full_len;
   strip[strip_len] = '\0';
   hay       = loop ? strip : full;
   hay_len   = loop ? strip_len : full_len;
   for (p = c->label; *p; p++)
      chars += ((*p & 0xC0) != 0x80);

   for (idx = 0; idx < 400; idx++)
   {
      gfx_animation_ctx_ticker_t t;
      long   at = -1;
      size_t i;

      memset(&t, 0, sizeof(t));
      t.type_enum = type;
      t.idx       = idx;
      t.selected  = true;
      t.len       = chars / 2;
      t.str       = c->label;
      t.s         = dst;
      t.s_len     = sizeof(dst);
      gfx_animation_ticker(&t);

      draw(dst);
      if (strstr(drawn, "\xE2\x80\xAD"))
         leaks++;
      if (!drawn_len)
         continue;
      for (i = 0; i + drawn_len <= hay_len; i++)
         if (     (hay[i] & 0xC0) != 0x80
               && !memcmp(hay + i, drawn, drawn_len)
               && (at < 0 || labs((long)i - prev) < labs(at - prev)))
            at = (long)i;
      if (at < 0)
      {
         broken++;
         continue;
      }

      if (idx == 0)
      {
         /* Reading starts at the right end of a right-to-left line */
         long start = c->rtl ? (long)(full_len - drawn_len) : 0;
         if (!loop && at != start)
         {
            sprintf(msg, "%s: stepped %s starts at byte %ld, not %ld",
                  c->name, "bounce", at, start);
            CHECK(0, msg);
         }
      }
      else if (at != prev)
      {
         int d = glyphs_in(hay + (at < prev ? at : prev),
               (size_t)labs(at - prev));
         bool right = at > prev;
         /* A loop wraps back by a whole period: not a move */
         if (loop && d > glyphs_in(hay, hay_len) / 3)
            ;
         else
         {
            if (d > 1)
               jumps++;
            if (loop && (c->rtl ? right : !right))
               backwards++;
         }
      }
      prev = at;
   }

   printf("  %-44s %s: %u jumps, %u frames broken, %u backwards, "
         "%u marks drawn\n", c->name, loop ? "loop  " : "bounce", jumps,
         broken, backwards, leaks);
   sprintf(msg, "%s: stepped frames are not a run of the line", c->name);
   CHECK(!broken, msg);
   sprintf(msg, "%s: stepped frames move more than a glyph", c->name);
   CHECK(!jumps, msg);
   sprintf(msg, "%s: stepped loop moves against its reading", c->name);
   CHECK(!backwards, msg);
   sprintf(msg, "%s: the override mark reaches the renderer", c->name);
   CHECK(!leaks, msg);
}

/* The stepped ticker as it stepped over typed characters, through
 * utf8len() and utf8cpy(): what every label without right-to-left text,
 * and every unselected or fitting one, must still get byte for byte */
static size_t ref_put(char *s, size_t len, size_t at, const char *str,
      size_t off, size_t n)
{
   if (!n)
      return 0;
   return utf8cpy(s + at, len - at, utf8skip(str, off), n);
}

static void ref_stepped(const char *str, const char *spacer, size_t len,
      bool selected, enum gfx_animation_ticker_type type, uint64_t idx,
      char *s, size_t s_len)
{
   size_t str_len = 0;
   if (selected)
      str_len = utf8len(str);
   else
   {
      const unsigned char *p = (const unsigned char*)str;
      while (*p)
         if ((*p++ & 0xc0) != 0x80 && ++str_len > len)
            break;
   }
   if (str_len <= len)
   {
      utf8cpy(s, s_len, str, len);
      return;
   }
   if (!selected)
   {
      size_t _len = utf8cpy(s, s_len, str, len - 3);
      if (_len + 4 <= s_len)
         strcpy(s + _len, "...");
      else
         s[s_len - 1] = '\0';
      return;
   }
   if (type == TICKER_TYPE_LOOP)
   {
      int    sp     = (int)utf8len(spacer);
      int    period = (int)str_len + sp;
      int    phase  = (int)(idx % (uint64_t)period);
      int    w1, o1 = 0, o2, w2, w3;
      size_t at;
      w1 = (int)str_len - phase;
      if (w1 < 0)
         w1 = 0;
      else if (w1 > (int)len)
         w1 = (int)len;
      if (phase < (int)str_len)
         o1 = phase;
      o2 = phase - (int)str_len;
      if (o2 < 0)
         o2 = 0;
      w2 = (int)len - w1;
      if (w2 > sp)
         w2 = sp;
      w2 -= o2;
      if (w2 < 0)
         w2 = 0;
      w3 = (int)len - (w1 + w2);
      if (w3 < 0)
         w3 = 0;
      s[0] = '\0';
      at   = ref_put(s, s_len, 0, str, (size_t)o1, (size_t)w1);
      at  += ref_put(s, s_len, at, spacer, (size_t)o2, (size_t)w2);
      ref_put(s, s_len, at, str, 0, (size_t)w3);
   }
   else
   {
      int old    = (int)(str_len - len);
      int period = 2 * old + 4;
      int phase  = (int)(idx % (uint64_t)period);
      int off;
      if (phase < 2)
         off = 0;
      else if (phase < 2 + old)
         off = phase - 2;
      else if (phase < 4 + old)
         off = old;
      else
         off = old - (phase - (4 + old));
      utf8cpy(s, s_len, utf8skip(str, (size_t)off), len);
   }
}

static void check_stepped_ref(const char *name, const char *label,
      bool rtl)
{
   static const char *const spacers[] = { NULL, " \xE2\x80\xA2 " };
   char     got[80], want[80];
   size_t   len, s_len, sp;
   unsigned idx, mode;
   int      bad = 0;
   for (mode = 0; mode < 4 && !bad; mode++)
   {
      bool selected = (mode < 2);
      enum gfx_animation_ticker_type type = (mode & 1)
         ? TICKER_TYPE_BOUNCE : TICKER_TYPE_LOOP;
      for (sp = 0; sp < 2 && !bad; sp++)
      for (len = 0; len <= 30 && !bad; len++)
      for (s_len = 1; s_len <= 64 && !bad; s_len += (s_len < 12) ? 1 : 7)
      for (idx = 0; idx < 90 && !bad; idx += selected ? 1 : 45)
      {
         gfx_animation_ctx_ticker_t t;
         const char *spacer = spacers[sp] ? spacers[sp]
            : TICKER_SPACER_DEFAULT;
         /* A selected label with right-to-left text steps over its
          * drawn glyphs instead, once it does not fit */
         if (rtl && selected && utf8len(label) > len)
            continue;
         memset(got, 0x5A, sizeof(got));
         memset(want, 0x5A, sizeof(want));
         memset(&t, 0, sizeof(t));
         t.type_enum = type;
         t.idx       = idx;
         t.selected  = selected;
         t.len       = len;
         t.str       = label;
         t.spacer    = spacers[sp];
         t.s         = got;
         t.s_len     = s_len;
         gfx_animation_ticker(&t);
         ref_stepped(label, spacer, len, selected, type, idx, want,
               s_len);
         if (     strcmp(got, want)
               || (unsigned char)got[s_len] != 0x5A)
         {
            printf("  FAIL: %s, %s%s, field %u, %u bytes, frame %u: "
                  "\"%s\", not \"%s\"\n", name,
                  type == TICKER_TYPE_LOOP ? "loop" : "bounce",
                  selected ? "" : ", unselected", (unsigned)len,
                  (unsigned)s_len, idx, got, want);
            bad = 1;
            fails++;
         }
      }
   }
}

/* The caret of an input line at every typed position, against the line
 * as drawn */
static int caret(const char *line, size_t at)
{
   return font_driver_get_caret_x(&font, line, strlen(line), at, 1.0f);
}

static void check_caret(void)
{
   char        msg[256];
   const char *he  = SHIN LAMED VAV MEM " " AYIN VAV LAMED MEM;
   const char *mix = "abc " DALET HE VAV " ghi";
   const char *num = SHIN LAMED VAV MEM " 123";
   const char *ar  = AR_TEH AR_FEH AR_AIN AR_YEH AR_LAM " " AR_JEEM AR_DAL
      AR_YEH AR_DAL;
   const char *la  = AR_LAM AR_ALEF;
   size_t      i, n;
   int         w, prev, bad;

   /* With nothing right-to-left, as typed */
   n = strlen(mix) - 10;
   CHECK(caret("abc def", 3) == x_at("abc def", 3),
         "caret: an English line places it as typed");

   /* A Hebrew line reads right to left: before a character is at its
    * right edge */
   draw(he);
   w   = x_at(drawn, drawn_len);
   bad = 0;
   for (i = 0; i <= strlen(he); i = (size_t)(utf8skip(he + i, 1) - he))
   {
      if (caret(he, i) != w - x_at(he, i))
         bad++;
      if (!he[i])
         break;
   }
   CHECK(!bad, "caret: a Hebrew line, before each character at its right "
         "edge, at the end at the left one");

   /* A Hebrew word in an English line: its right edge before the word,
    * the left edge of the space after it */
   {
      int lead = x_at(mix, 4);
      int word = x_at(mix + 4, 6);
      CHECK(caret(mix, 4) == lead + word,
            "caret: before a Hebrew word in English, at its right end");
      CHECK(caret(mix, 6) == lead + word - x_at(mix + 4, 2),
            "caret: inside it, at the right edge of the next letter");
      CHECK(caret(mix, 10) == lead + word,
            "caret: after it, at the left edge of the space after it");
      CHECK(caret(mix, 12) == x_at(mix, 12),
            "caret: in the English after it, as typed");
   }
   (void)n;

   /* Digits in a Hebrew line read left to right, at its left end */
   draw(num);
   w = x_at(drawn, drawn_len);
   {
      size_t d = strlen(num) - 3;
      CHECK(caret(num, d) == 0,
            "caret: before the digits of a Hebrew line, at their left");
      CHECK(caret(num, d + 1) == x_at("1", 1),
            "caret: between digits, as they read");
      CHECK(caret(num, strlen(num)) == x_at("123", 3),
            "caret: after the last digit, at its right");
      CHECK(caret(num, 0) == w,
            "caret: at the start of the Hebrew, at the right end");
   }

   /* Arabic: the ends of the drawn line, every caret on a boundary of
    * its glyphs, moving left as the typing goes on */
   draw(ar);
   w = x_at(drawn, drawn_len);
   CHECK(caret(ar, 0) == w && caret(ar, strlen(ar)) == 0,
         "caret: an Arabic line starts at its right end, ends at its left");
   prev = w + 1;
   bad  = 0;
   for (i = 0; i <= strlen(ar); i = (size_t)(utf8skip(ar + i, 1) - ar))
   {
      int    x = caret(ar, i);
      size_t b;
      bool   edge = false;
      for (b = 0; b <= drawn_len; b++)
         if (     (b == drawn_len || (drawn[b] & 0xC0) != 0x80)
               && x_at(drawn, b) == x)
            edge = true;
      if (!edge || x >= prev)
         bad++;
      prev = x;
      if (!ar[i])
         break;
   }
   sprintf(msg, "caret: Arabic carets fall on its drawn glyphs' edges and "
         "move left (%d off)", bad);
   CHECK(!bad, msg);

   /* Lam-alef is one glyph: before either, at its right edge */
   draw(la);
   w = x_at(drawn, drawn_len);
   CHECK(caret(la, 0) == w && caret(la, 2) == w && caret(la, 4) == 0,
         "caret: lam-alef is one glyph, the caret on either side of it");
}

/* A label of one-byte characters is copied as byte runs. Into a
 * shorter buffer, every frame of it is the frame a buffer big enough
 * holds, cut to fit and terminated. */
static void check_bytes_cut(const char *label, enum gfx_animation_ticker_type
      type, bool selected, unsigned field)
{
   char     full[512];
   char     dst[64 + 16];
   unsigned frame;
   int      bad = 0;
   for (frame = 0; frame < 400 && !bad; frame += 7)
   {
      size_t len;
      for (len = 1; len <= 64 && !bad; len++)
      {
         gfx_animation_ctx_ticker_smooth_t t;
         unsigned x = 0, w = 0, x2 = 0, w2 = 0;
         size_t   k;
         memset(&t, 0, sizeof(t));
         t.font        = &font;
         t.font_scale  = 1.0f;
         t.type_enum   = type;
         t.idx         = frame;
         t.selected    = selected;
         t.field_width = field;
         t.src_str     = label;
         t.dst_str     = full;
         t.dst_str_len = sizeof(full);
         t.x_offset    = &x;
         t.dst_str_width = &w;
         gfx_animation_ticker_smooth(&t);

         memset(dst, 0x5A, sizeof(dst));
         t.dst_str       = dst;
         t.dst_str_len   = len;
         t.x_offset      = &x2;
         t.dst_str_width = &w2;
         gfx_animation_ticker_smooth(&t);

         k = strlen(full);
         if (k > len - 1)
            k = len - 1;
         /* The unselected ellipsis is added only when it fits whole */
         if (!selected && strlen(full) > len - 1)
            continue;
         if (     memcmp(dst, full, k) || dst[k] != '\0'
               || (unsigned char)dst[len] != 0x5A)
         {
            printf("  FAIL: %s, %s%s, frame %u, %u bytes: got \"%s\"\n",
                  label, type == TICKER_TYPE_LOOP ? "loop" : "bounce",
                  selected ? "" : ", unselected", frame, (unsigned)len, dst);
            bad = 1;
            fails++;
         }
      }
   }
}

int main(void)
{
   size_t i;

   font.renderer = &fake_font;
   font.size     = 16.0f;

   printf("smooth ticker over right-to-left labels\n");
   for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
   {
      run_case(&cases[i], TICKER_TYPE_BOUNCE);
      run_case(&cases[i], TICKER_TYPE_LOOP);
   }

   check_caret();

   printf("stepped ticker:\n");
   for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
   {
      run_stepped_case(&cases[i], TICKER_TYPE_LOOP);
      run_stepped_case(&cases[i], TICKER_TYPE_BOUNCE);
   }

   check_stepped_ref("English", "Show Advanced Settings In The Quick Menu",
         false);
   check_stepped_ref("Cyrillic", "\xD0\x9F\xD0\xBE\xD0\xBA\xD0\xB0"
         "\xD0\xB7\xD0\xB0\xD1\x82\xD1\x8C \xD1\x80\xD0\xB0\xD1\x81"
         "\xD1\x88\xD0\xB8\xD1\x80\xD0\xB5\xD0\xBD\xD0\xBD\xD1\x8B"
         "\xD0\xB5 (v1.22)", false);
   check_stepped_ref("Latin-1 and emoji", "Caf\xC3\xA9 \xF0\x9F\x8E\xAE"
         " r\xC3\xA9tro \xF0\x9F\x95\xB9 na\xC3\xAFve list", false);
   check_stepped_ref("a stray continuation byte first",
         "\x80\x81 opens on continuation bytes, then text", false);
   check_stepped_ref("Hebrew", cases[1].label, true);
   check_stepped_ref("Arabic", cases[3].label, true);

   check_bytes_cut("Configuration files and directories", TICKER_TYPE_LOOP,
         true, 100);
   check_bytes_cut("Configuration files and directories",
         TICKER_TYPE_BOUNCE, true, 100);
   check_bytes_cut("Configuration files and directories", TICKER_TYPE_LOOP,
         false, 100);
   check_bytes_cut("Settings", TICKER_TYPE_LOOP, true, 100);

   /* A label the reshaper is handed already in visual order after the
    * override mark is drawn as given, without the mark */
   draw("\xE2\x80\xAD" SHIN LAMED " (" AR_LAM AR_ALEF ")");
   CHECK(!strcmp(drawn, SHIN LAMED " (" AR_LAM AR_ALEF ")"),
         "a line after the override mark is not drawn as given");

   if (fails)
   {
      printf("%d failure(s)\n", fails);
      return 1;
   }
   printf("all passed\n");
   return 0;
}
