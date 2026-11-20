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
