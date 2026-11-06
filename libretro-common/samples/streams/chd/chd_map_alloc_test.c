/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (chd_map_alloc_test.c).
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

/* libchdr's v3/v4 hunk map (map_read) is sized from the header's
 * totalhunks field. The map is stored in the file right after the
 * header, one 16-byte record per hunk followed by a 16-byte
 * end-of-list cookie, so an image can only describe as many hunks as
 * it has map records for, and sizeof(map_entry) * totalhunks has to
 * fit size_t on a 32-bit host.
 *
 * Each image here is opened from memory through chd_open_core_file:
 *   - well-formed one- and three-hunk v3 and v4 images open;
 *   - a count of 0xFFFFFFFF, a count one past the records the file
 *     holds, and counts whose map size wraps a 32-bit size_t are all
 *     refused as invalid data, before the map is allocated.
 *
 * map_entry is 24 bytes where uint64_t is 8-aligned and 20 bytes on
 * the i386 SysV ABI, so each gets a count that wraps there: 0x0AAAAAAB
 * (24 bytes -> 8) and 0x0CCCCCCD (20 bytes -> 4). The wrapping images
 * carry a full map chunk, which is what a reader that only reads before
 * it writes needs to reach the extraction loop; a 32-bit ASan build
 * without the bound reports the heap overwrite there. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libchdr/chd.h>

#define V3_LEN    120
#define V4_LEN    108
#define REC       16
/* one full 512-record chunk of libchdr's map reader, plus the cookie */
#define CHUNK     512
#define IMG_MAX   (V3_LEN + (CHUNK + 1) * REC)

typedef struct
{
   const uint8_t *data;
   uint64_t       size;
   uint64_t       pos;
} mem_file_t;

static int failures = 0;

static uint64_t mem_fsize(core_file *f)
{
   return ((mem_file_t*)f->argp)->size;
}

static size_t mem_fread(void *ptr, size_t size, size_t nmemb, core_file *f)
{
   mem_file_t *m   = (mem_file_t*)f->argp;
   uint64_t   want = (uint64_t)size * nmemb;
   uint64_t   left = (m->pos < m->size) ? m->size - m->pos : 0;
   if (want > left)
      want = left;
   memcpy(ptr, m->data + m->pos, (size_t)want);
   m->pos += want;
   return size ? (size_t)(want / size) : 0;
}

static int mem_fclose(core_file *f)
{
   free(f);
   return 0;
}

static int mem_fseek(core_file *f, int64_t offset, int whence)
{
   mem_file_t *m = (mem_file_t*)f->argp;
   int64_t base  = 0;
   if (whence == SEEK_CUR)
      base = (int64_t)m->pos;
   else if (whence == SEEK_END)
      base = (int64_t)m->size;
   if (base + offset < 0)
      return -1;
   m->pos = (uint64_t)(base + offset);
   return 0;
}

static void put32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v >> 24);
   p[1] = (uint8_t)(v >> 16);
   p[2] = (uint8_t)(v >>  8);
   p[3] = (uint8_t)v;
}

static void put64(uint8_t *p, uint64_t v)
{
   put32(p,     (uint32_t)(v >> 32));
   put32(p + 4, (uint32_t)v);
}

/* An uncompressed image of `records` map records (each a mini hunk,
 * whose data lives in the record itself) and the end-of-list cookie,
 * whose header claims `totalhunks`. Returns the image size. */
static size_t build_image(uint8_t *img, uint32_t version,
      uint32_t totalhunks, unsigned records)
{
   const uint32_t hunkbytes = 512;
   uint32_t       len       = (version == 3) ? V3_LEN : V4_LEN;
   uint8_t       *map;
   unsigned       r;

   memset(img, 0, IMG_MAX);
   memcpy(img, "MComprHD", 8);
   put32(img +  8, len);
   put32(img + 12, version);
   put32(img + 16, 0);                       /* flags */
   put32(img + 20, CHDCOMPRESSION_NONE);
   put32(img + 24, totalhunks);
   put64(img + 28, (uint64_t)hunkbytes);     /* logicalbytes */
   put64(img + 36, 0);                       /* no metadata */
   put32(img + ((version == 3) ? 76 : 44), hunkbytes);

   map = img + len;
   for (r = 0; r < records; r++)
      map[r * REC + 15] = 3;                 /* V34_MAP_ENTRY_TYPE_MINI */
   memcpy(map + records * REC, "EndOfListCookie", REC);
   return len + (records + 1) * REC;
}

static chd_error open_image(const uint8_t *img, size_t size, chd_file **out)
{
   mem_file_t  m;
   core_file  *f = (core_file*)malloc(sizeof(*f));
   chd_error   err;

   *out    = NULL;
   if (!f)
      return CHDERR_OUT_OF_MEMORY;
   m.data  = img;
   m.size  = size;
   m.pos   = 0;
   f->argp   = &m;
   f->fsize  = mem_fsize;
   f->fread  = mem_fread;
   f->fclose = mem_fclose;
   f->fseek  = mem_fseek;

   err = chd_open_core_file(f, CHD_OPEN_READ, NULL, out);
   if (*out)
   {
      chd_close(*out);
      *out = (chd_file*)1;
   }
   return err;
}

static void expect(const char *what, uint32_t version, uint32_t totalhunks,
      unsigned records, chd_error want)
{
   static uint8_t img[IMG_MAX];
   chd_file  *chd;
   size_t     size = build_image(img, version, totalhunks, records);
   chd_error  got  = open_image(img, size, &chd);

   if (got != want)
   {
      printf("[FAIL] v%u %s: totalhunks=0x%08lx records=%u: %s, want %s\n",
            (unsigned)version, what, (unsigned long)totalhunks, records,
            chd_error_string(got), chd_error_string(want));
      failures++;
   }
   else
      printf("[ OK ] v%u %s\n", (unsigned)version, what);
}

int main(void)
{
   uint32_t v;

   for (v = 3; v <= 4; v++)
   {
      expect("well-formed one-hunk image", v, 1, 1, CHDERR_NONE);
      expect("well-formed three-hunk image", v, 3, 3, CHDERR_NONE);
      expect("count past the map records", v, 4, 3, CHDERR_INVALID_DATA);
      expect("count wrapping a 24-byte map entry on 32-bit", v,
            0x0AAAAAABu, CHUNK, CHDERR_INVALID_DATA);
      expect("count wrapping a 20-byte map entry on 32-bit", v,
            0x0CCCCCCDu, CHUNK, CHDERR_INVALID_DATA);
      expect("count of 0xFFFFFFFF", v, 0xFFFFFFFFu, 1, CHDERR_INVALID_DATA);
   }

   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("all passed\n");
   return 0;
}
