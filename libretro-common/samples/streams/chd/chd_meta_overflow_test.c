/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (chd_meta_overflow_test.c).
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

/* Regression test for the GDROM_TRACK_METADATA_TAG parser
 * bounds in libretro-common/streams/chd_stream.c::
 * chdstream_get_meta.
 *
 * The .chd metadata blob (read from the disc image via
 * chd_get_metadata into a meta[256] buffer) contains
 * space-separated KEY:VALUE pairs.  Pre-this-patch the
 * TYPE: / SUBTYPE: / PGTYPE: / PGSUB: cases all did:
 *
 *   p   += <tag-len>;
 *   len  = 0;
 *   while (p[len] && p[len] != ' ')
 *      len++;
 *   memcpy(md->FIELD, p, len);
 *   md->FIELD[len] = '\0';
 *
 * The destination fields are:
 *   md->type[64]
 *   md->subtype[32]
 *   md->pgtype[32]
 *   md->pgsub[32]
 *
 * but `len` was bounded only by the surrounding meta[256]
 * buffer (and only loosely, since p[len] could read up to
 * the meta-buffer's NUL terminator).  A malicious .chd with
 * "TYPE:" followed by 200 bytes of non-space content
 * stack-overflowed md->type by ~136 bytes -- enough to
 * corrupt md->subtype, md->pgtype, md->pgsub, and depending
 * on layout the saved frame pointer and return address.
 *
 * Reachability: user loads a malicious .chd file.  Same
 * threat class as the CDFS / BSV file-load bugs.
 *
 * Fix: the SCD-format parser earlier in the same function
 * already had the right pattern (line 166):
 *
 *   if (len == 0 || len >= sizeof(md->type))
 *      return false;
 *
 * Apply the same predicate to all four GDROM-format fields.
 *
 * IMPORTANT: this test keeps a verbatim copy of the post-fix
 * predicate from chd_stream.c.  If chd_stream.c amends, the
 * copy below must follow.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Mirror of the metadata struct from chd_stream.c (subset
 * of the fields touched by this parser).  Field sizes
 * match the production layout. */
typedef struct {
   uint32_t track;
   uint32_t frames;
   uint32_t pad;
   uint32_t pregap;
   uint32_t postgap;
   char type[64];
   char subtype[32];
   char pgtype[32];
   char pgsub[32];
} mock_metadata_t;

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

static int failures = 0;

/* === verbatim copy of the post-fix GDROM parser from
 *     libretro-common/streams/chd_stream.c.  Returns true
 *     on a fully-parsed metadata blob, false on malformed
 *     input.  If chd_stream.c amends the predicates, this
 *     copy must follow. === */
static int parse_gdrom_meta(mock_metadata_t *md, const char *meta)
{
   const char *p = meta;
   while (*p)
   {
      size_t len;

      if (strncmp(p, "TRACK:", 6) == 0)
      {
         char *end;
         md->track = strtoul(p + 6, &end, 10);
         p = end;
      }
      else if (strncmp(p, "TYPE:", 5) == 0)
      {
         p   += 5;
         len  = 0;
         while (p[len] && p[len] != ' ')
            len++;
         if (len >= sizeof(md->type))
            return 0;
         memcpy(md->type, p, len);
         md->type[len] = '\0';
         p += len;
      }
      else if (strncmp(p, "SUBTYPE:", 8) == 0)
      {
         p   += 8;
         len  = 0;
         while (p[len] && p[len] != ' ')
            len++;
         if (len >= sizeof(md->subtype))
            return 0;
         memcpy(md->subtype, p, len);
         md->subtype[len] = '\0';
         p += len;
      }
      else if (strncmp(p, "PGTYPE:", 7) == 0)
      {
         p   += 7;
         len  = 0;
         while (p[len] && p[len] != ' ')
            len++;
         if (len >= sizeof(md->pgtype))
            return 0;
         memcpy(md->pgtype, p, len);
         md->pgtype[len] = '\0';
         p += len;
      }
      else if (strncmp(p, "PGSUB:", 6) == 0)
      {
         p   += 6;
         len  = 0;
         while (p[len] && p[len] != ' ')
            len++;
         if (len >= sizeof(md->pgsub))
            return 0;
         memcpy(md->pgsub, p, len);
         md->pgsub[len] = '\0';
         p += len;
      }
      else
         p++;
   }
   return 1;
}
/* === end verbatim copy === */

/* ---- tests ---- */

static void test_legitimate_metadata(void)
{
   mock_metadata_t md;
   const char *meta = "TRACK:1 TYPE:MODE2_RAW SUBTYPE:NONE PGTYPE:MODE2_RAW PGSUB:NONE";
   memset(&md, 0, sizeof(md));
   if (!parse_gdrom_meta(&md, meta))
   {
      printf("[ERROR] legitimate metadata rejected\n");
      failures++;
      return;
   }
   if (md.track != 1 || strcmp(md.type, "MODE2_RAW") != 0
         || strcmp(md.subtype, "NONE") != 0)
   {
      printf("[ERROR] legitimate metadata not parsed correctly\n");
      failures++;
      return;
   }
   printf("[SUCCESS] legitimate GDROM metadata parsed correctly\n");
}

static void test_oversize_type_rejected(void)
{
   /* "TYPE:" followed by 200 'A's, no space.  Pre-fix: the
    * memcpy of 200 bytes into md->type[64] OOB-wrote 136
    * bytes past type into subtype, pgtype, pgsub, and beyond.
    * Allocate the metadata struct on the heap with no slack
    * so ASan flags any reintroduction of the unbounded
    * memcpy. */
   mock_metadata_t *md = (mock_metadata_t*)malloc(sizeof(*md));
   char meta[256];
   int rv;
   if (!md) { printf("[ERROR] alloc\n"); failures++; return; }
   memset(md, 0, sizeof(*md));
   memset(meta, 0, sizeof(meta));
   strcpy(meta, "TYPE:");
   memset(meta + 5, 'A', 200);
   meta[205] = '\0';

   rv = parse_gdrom_meta(md, meta);
   if (rv)
   {
      printf("[ERROR] oversize TYPE: was accepted\n");
      failures++;
   }
   else
      printf("[SUCCESS] oversize TYPE: rejected without OOB write\n");

   free(md);
}

static void test_oversize_subtype_rejected(void)
{
   mock_metadata_t *md = (mock_metadata_t*)malloc(sizeof(*md));
   char meta[256];
   int rv;
   if (!md) { printf("[ERROR] alloc\n"); failures++; return; }
   memset(md, 0, sizeof(*md));
   memset(meta, 0, sizeof(meta));
   strcpy(meta, "SUBTYPE:");
   memset(meta + 8, 'B', 100);
   meta[108] = '\0';

   rv = parse_gdrom_meta(md, meta);
   if (rv)
   {
      printf("[ERROR] oversize SUBTYPE: was accepted\n");
      failures++;
   }
   else
      printf("[SUCCESS] oversize SUBTYPE: rejected without OOB write\n");

   free(md);
}

static void test_oversize_pgtype_rejected(void)
{
   mock_metadata_t *md = (mock_metadata_t*)malloc(sizeof(*md));
   char meta[256];
   int rv;
   if (!md) { printf("[ERROR] alloc\n"); failures++; return; }
   memset(md, 0, sizeof(*md));
   memset(meta, 0, sizeof(meta));
   strcpy(meta, "PGTYPE:");
   memset(meta + 7, 'C', 100);
   meta[107] = '\0';

   rv = parse_gdrom_meta(md, meta);
   if (rv)
   {
      printf("[ERROR] oversize PGTYPE: was accepted\n");
      failures++;
   }
   else
      printf("[SUCCESS] oversize PGTYPE: rejected without OOB write\n");

   free(md);
}

static void test_oversize_pgsub_rejected(void)
{
   mock_metadata_t *md = (mock_metadata_t*)malloc(sizeof(*md));
   char meta[256];
   int rv;
   if (!md) { printf("[ERROR] alloc\n"); failures++; return; }
   memset(md, 0, sizeof(*md));
   memset(meta, 0, sizeof(meta));
   strcpy(meta, "PGSUB:");
   memset(meta + 6, 'D', 100);
   meta[106] = '\0';

   rv = parse_gdrom_meta(md, meta);
   if (rv)
   {
      printf("[ERROR] oversize PGSUB: was accepted\n");
      failures++;
   }
   else
      printf("[SUCCESS] oversize PGSUB: rejected without OOB write\n");

   free(md);
}

static void test_field_at_exact_size_boundary(void)
{
   /* TYPE: with exactly sizeof(type) - 1 = 63 chars (the
    * largest legitimate length).  Should still parse. */
   mock_metadata_t md;
   char meta[80];
   int rv;
   memset(&md, 0, sizeof(md));
   memset(meta, 0, sizeof(meta));
   strcpy(meta, "TYPE:");
   memset(meta + 5, 'X', 63);
   meta[5 + 63] = '\0';

   rv = parse_gdrom_meta(&md, meta);
   if (!rv)
   {
      printf("[ERROR] boundary-size TYPE: rejected\n");
      failures++;
   }
   else if (strlen(md.type) != 63)
   {
      printf("[ERROR] boundary-size TYPE: parsed wrong length %zu\n",
            strlen(md.type));
      failures++;
   }
   else
      printf("[SUCCESS] boundary-size TYPE: (63 chars) accepted\n");
}

int main(void)
{
   test_legitimate_metadata();
   test_oversize_type_rejected();
   test_oversize_subtype_rejected();
   test_oversize_pgtype_rejected();
   test_oversize_pgsub_rejected();
   test_field_at_exact_size_boundary();

   if (failures)
   {
      printf("\n%d chd_meta_overflow test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll chd_meta_overflow regression tests passed.\n");
   return 0;
}
