/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (filestream_printf_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* filestream_printf(): a line of any length is written whole, lines
 * after a long one on the same stream too, and two threads printing to
 * two files at once each write only their own. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <rthreads/rthreads.h>
#include <streams/file_stream.h>

#define LINES 2000

static int fails;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL: %s\n", msg); \
         fails++; \
      } \
   } while (0)

static char *slurp(const char *path, int64_t *len)
{
   void *buf = NULL;
   if (!filestream_read_file(path, &buf, len))
      return NULL;
   return (char*)buf;
}

/* @len octets of text and "|end\n" in one call, then a short line */
static bool long_line(const char *path, size_t len)
{
   char    *text = (char*)malloc(len + 1);
   char    *back;
   int64_t  got  = 0;
   RFILE   *f;
   bool     ok;
   size_t   i;

   if (!text)
      return false;
   for (i = 0; i < len; i++)
      text[i] = (char)('a' + i % 26);
   text[len] = '\0';
   if (!(f = filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE,
               RETRO_VFS_FILE_ACCESS_HINT_NONE)))
   {
      free(text);
      return false;
   }
   ok = filestream_printf(f, "%s|%s\n", text, "end") == (int)len + 5
     && filestream_printf(f, "%d\n", 42) == 3;
   filestream_close(f);
   back = slurp(path, &got);
   ok   = ok && back && got == (int64_t)len + 8
            && !memcmp(back, text, len)
            && !memcmp(back + len, "|end\n42\n", 8);
   free(back);
   free(text);
   filestream_delete(path);
   return ok;
}

struct writer
{
   const char *path;
   char        tag;
   bool        ok;
};

static void write_lines(void *data)
{
   struct writer *w = (struct writer*)data;
   RFILE *f = filestream_open(w->path, RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   int    i;

   w->ok = f != NULL;
   for (i = 0; w->ok && i < LINES; i++)
      w->ok = filestream_printf(f, "%c%05d%c%c%c%c%c%c%c%c\n", w->tag, i,
            w->tag, w->tag, w->tag, w->tag, w->tag, w->tag, w->tag, w->tag)
         == 15;
   filestream_close(f);
}

static bool lines_are(const char *path, char tag)
{
   int64_t got  = 0;
   char   *back = slurp(path, &got);
   char    want[16];
   bool    ok   = back && got == (int64_t)LINES * 15;
   int     i;

   for (i = 0; ok && i < LINES; i++)
   {
      sprintf(want, "%c%05d%c%c%c%c%c%c%c%c\n", tag, i,
            tag, tag, tag, tag, tag, tag, tag, tag);
      ok = !memcmp(back + (size_t)i * 15, want, 15);
   }
   free(back);
   filestream_delete(path);
   return ok;
}

int main(void)
{
   struct writer a, b;
   sthread_t    *ta, *tb;

   CHECK(long_line("filestream_printf_short.txt", 10),
         "a short line is written whole");
   CHECK(long_line("filestream_printf_edge.txt", 1019),
         "a line one octet past the first buffer is written whole");
   CHECK(long_line("filestream_printf_long.txt", 20000),
         "a line longer than any fixed buffer is written whole");

   a.path = "filestream_printf_a.txt"; a.tag = 'A'; a.ok = false;
   b.path = "filestream_printf_b.txt"; b.tag = 'B'; b.ok = false;
   ta = sthread_create(write_lines, &a);
   tb = sthread_create(write_lines, &b);
   if (!ta || !tb)
   {
      printf("FAIL: no threads\n");
      return 1;
   }
   sthread_join(ta);
   sthread_join(tb);
   CHECK(a.ok && b.ok, "both threads print every line");
   CHECK(lines_are(a.path, 'A') && lines_are(b.path, 'B'),
         "each file holds only its own thread's lines");

   if (fails)
   {
      printf("%d FAIL\n", fails);
      return 1;
   }
   printf("filestream_printf_test: PASS\n");
   return 0;
}
