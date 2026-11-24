/* Regression test for word_wrap_offset() (libretro-common/string/
 * stdstring.c), which the menu drivers use to place the input
 * dialog's caret in the text's own wrap.
 *
 * For every text, line width and wrap function, and with the line
 * ends left as '\n' or made '\0' as the drivers make them, every
 * character boundary of the source must map to the byte of the wrap
 * that holds the same character - or a line end, for a space or a
 * newline the wrap turned into one - and the offsets must rise with
 * the source's. The end of the source maps to the end of the wrap,
 * and no wrap grows past a line end before each character - as the
 * wide-glyph wrap did at a line width under a wide glyph's, putting
 * line ends before the same glyph until the buffer was full.
 *
 * And the case the caret was wrong in: the cursor before a word the
 * wrap moved to the next line is on that line, at its start. */

#include <stdio.h>
#include <string.h>

#include <string/stdstring.h>
#include <encodings/utf.h>

typedef size_t (*wrap_fn)(char *s, size_t len, const char *src,
      size_t src_len, int line_width, int wideglyph_width,
      unsigned max_lines);

static unsigned failures = 0;

/* word_wrap_offset() as it walked byte by byte: what the run-wise walk
 * must still return for every source, wrap and offset */
static size_t ref_offset(const char *src, size_t at, const char *wrapped,
      size_t wrapped_len)
{
   size_t i = 0;
   size_t d = 0;
   while (i < at && d < wrapped_len)
   {
      char w = wrapped[d];
      char c = src[i];
      if (w == c)
         i++;
      else if (w == '\0' || w == '\n')
      {
         if (c == ' ' || c == '\n')
            i++;
      }
      else
         break;
      d++;
   }
   if (i == at && src[i] != ' ' && src[i] != '\n')
      while (d < wrapped_len && (wrapped[d] == '\0' || wrapped[d] == '\n'))
         d++;
   return d;
}

/* Random sources wrapped, with their wraps changed here and there as a
 * wrap of other text would be, against the byte-wise walk */
static void check_runs(wrap_fn wrap, const char *name)
{
   static const char *const pieces[] = { "a", "bc", " ", " ", "\n",
      "\xD7\x90", "\xE4\xB8\xAD", "word", "x" };
   unsigned seed = 2024;
   unsigned k;
   for (k = 0; k < 4000; k++)
   {
      char   src[256];
      char   out[1024];
      size_t src_len = 0, len, at, m, parts;
      seed  = seed * 1103515245u + 12345u;
      parts = 1 + (seed >> 16) % 40;
      src[0] = '\0';
      for (m = 0; m < parts; m++)
      {
         const char *pc;
         seed = seed * 1103515245u + 12345u;
         pc   = pieces[(seed >> 16) % (sizeof(pieces) / sizeof(pieces[0]))];
         if (src_len + strlen(pc) >= sizeof(src))
            break;
         strcpy(src + src_len, pc);
         src_len += strlen(pc);
      }
      seed = seed * 1103515245u + 12345u;
      if (!(len = wrap(out, sizeof(out), src, src_len,
                  1 + (int)((seed >> 16) % 12), 2, 0)))
         continue;
      if (k & 1)
         for (m = 0; m < len; m++)
            if (out[m] == '\n')
               out[m] = '\0';
      /* A changed byte, so the walks stop where the wrap differs */
      if (k % 3 == 0)
      {
         seed = seed * 1103515245u + 12345u;
         out[(seed >> 16) % len] = '#';
      }
      for (at = 0; at <= src_len; at++)
      {
         size_t got  = word_wrap_offset(src, at, out, len);
         size_t want = ref_offset(src, at, out, len);
         if (got != want)
         {
            if (failures < 10)
               fprintf(stderr, "[FAIL] %s: source of %u bytes at %u: %u, "
                     "the byte-wise walk %u\n", name, (unsigned)src_len,
                     (unsigned)at, (unsigned)got, (unsigned)want);
            failures++;
            break;
         }
      }
   }
}

static void check_text(wrap_fn wrap, const char *name, const char *src,
      int width, int nul_ends)
{
   char out[1024];
   size_t src_len = strlen(src);
   size_t len     = wrap(out, sizeof(out), src, src_len, width, 200, 0);
   size_t at, prev = 0;
   size_t i;

   if (!len)
      return;
   /* At most a line end before each character: more is a runaway */
   if (len > 2 * src_len)
   {
      if (failures < 10)
         fprintf(stderr, "[FAIL] %s width %d: %u bytes wrapped to %u\n",
               name, width, (unsigned)src_len, (unsigned)len);
      failures++;
      return;
   }
   if (nul_ends)
      for (i = 0; i < len; i++)
         if (out[i] == '\n')
            out[i] = '\0';

   for (at = 0; at <= src_len;
         at = (at < src_len) ? (size_t)(utf8skip(src + at, 1) - src) : at + 1)
   {
      size_t d  = word_wrap_offset(src, at, out, len);
      int    ok = 1;
      if (at == src_len)
         ok = (d == len);
      else if (d >= len)
         ok = 0;
      else if (out[d] != src[at])
         ok = (out[d] == '\n' || out[d] == '\0')
            && (src[at] == ' ' || src[at] == '\n');
      if (ok && at && d <= prev)
         ok = 0;
      if (!ok)
      {
         if (failures < 10)
            fprintf(stderr, "[FAIL] %s width %d%s: byte %u of \"%s\" "
                  "maps to %u of %u\n", name, width,
                  nul_ends ? " (NUL ends)" : "", (unsigned)at, src,
                  (unsigned)d, (unsigned)len);
         failures++;
         return;
      }
      prev = d;
   }
}

/* The line a byte of the wrap is on, and its column */
static void line_of(const char *out, size_t d, unsigned *line,
      size_t *col)
{
   size_t i, start = 0;
   *line = 0;
   for (i = 0; i < d; i++)
      if (out[i] == '\n')
      {
         (*line)++;
         start = i + 1;
      }
   *col = d - start;
}

int main(void)
{
   static const char *texts[] = {
      "Enter name\nhello world this is a long line of text that wraps",
      "Label\nabc def ghi jkl mno pqr stu vwx yz",
      "a\n\nb  c   d\n e ",
      "Label\nsupercalifragilisticexpialidocious_and_more_text_without_spaces",
      /* Japanese: wide glyphs the wideglyph wrap breaks before */
      "\xe5\x90\x8d\xe5\x89\x8d\n\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe3\x81"
      "\xae\xe3\x83\x86\xe3\x82\xad\xe3\x82\xb9\xe3\x83\x88\xe6\x97\xa5\xe6"
      "\x9c\xac\xe8\xaa\x9e\xe3\x81\xae\xe3\x83\x86\xe3\x82\xad\xe3\x82\xb9"
      "\xe3\x83\x88",
      /* Mixed */
      "Name\nab \xe6\x97\xa5\xe6\x9c\xac cd \xe8\xaa\x9e\xe3\x81\xae ef gh "
      "\xe3\x83\x86\xe3\x82\xad\xe3\x82\xb9\xe3\x83\x88 ij",
      NULL };
   int t, width, nul;

   for (t = 0; texts[t]; t++)
      for (width = 1; width < 40; width++)
         for (nul = 0; nul < 2; nul++)
         {
            check_text(word_wrap, "word_wrap", texts[t], width, nul);
            check_text(word_wrap_wideglyph, "word_wrap_wideglyph",
                  texts[t], width, nul);
         }

   check_runs(word_wrap, "word_wrap");
   check_runs(word_wrap_wideglyph, "word_wrap_wideglyph");

   /* "is" moved to the next line: the caret before it is there */
   {
      const char *src = texts[0];
      char out[1024];
      size_t len = word_wrap(out, sizeof(out), src, strlen(src), 6, 0, 0);
      size_t at  = (size_t)(strstr(src, "is a") - src);
      size_t d   = word_wrap_offset(src, at, out, len);
      unsigned line;
      size_t col;
      line_of(out, d, &line, &col);
      if (line != 5 || col != 0)
      {
         fprintf(stderr, "[FAIL] the caret before \"is\" is on line %u "
               "column %u, not line 5 column 0\n", line, (unsigned)col);
         failures++;
      }
   }

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] word_wrap_offset\n");
   return 0;
}
