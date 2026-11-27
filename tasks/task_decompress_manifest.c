/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* What an archive's last extraction into a folder wrote: one line per
 * member, "crc size mtime name".  A member whose line still matches,
 * and whose file still has the line's mtime, need not be written again.
 * The manifest can only save work: any doubt means extract. */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <array/rhmap.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "tasks_internal.h"

#define DECOMPRESS_MANIFEST_HEADER "decompress_manifest 1"

typedef struct
{
   int64_t  mtime;
   uint64_t size;
   char    *name;
   uint32_t crc;
   bool     seen;   /* asked about, or written, by this walk */
} decompress_manifest_line_t;

struct decompress_manifest
{
   uint64_t pending_size;
   char    *path;
   decompress_manifest_line_t *lines;   /* in file order */
   uint32_t *map;            /* name to index in lines; borrows the names */
   char    *pending_name;    /* the member being written; no line yet */
   char    *pending_path;
   size_t   count;
   size_t   cap;
   size_t   cursor;          /* the line after the last hit */
   uint32_t pending_crc;
   bool     loaded;
   bool     dirty;
   bool     no_mtime;        /* a written member had none: stop recording */
};

static char *decompress_manifest_strdup(const char *s, size_t len)
{
   char *copy = (char*)malloc(len + 1);
   if (!copy)
      return NULL;
   memcpy(copy, s, len);
   copy[len] = '\0';
   return copy;
}

/* The digits of @base up to @end; NULL on none or on overflow */
static const char *decompress_manifest_parse_u64(const char *s,
      const char *end, unsigned base, uint64_t *out)
{
   const char *start = s;
   uint64_t v        = 0;

   for (; s < end; s++)
   {
      unsigned d;
      if (*s >= '0' && *s <= '9')
         d = (unsigned)(*s - '0');
      else if (base == 16 && *s >= 'a' && *s <= 'f')
         d = (unsigned)(*s - 'a' + 10);
      else if (base == 16 && *s >= 'A' && *s <= 'F')
         d = (unsigned)(*s - 'A' + 10);
      else
         break;
      if (v > (~(uint64_t)0 - d) / base)
         return NULL;
      v = v * base + d;
   }
   if (s == start)
      return NULL;
   *out = v;
   return s;
}

/* Writes @v in @base, @min_digits or more; returns the length */
static size_t decompress_manifest_put_u64(char *s, uint64_t v,
      unsigned base, size_t min_digits)
{
   size_t i;
   size_t n = 0;
   char digits[20];

   do
   {
      unsigned d  = (unsigned)(v % base);
      digits[n++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
      v          /= base;
   } while (v);
   while (n < min_digits)
      digits[n++] = '0';
   for (i = 0; i < n; i++)
      s[i] = digits[n - 1 - i];
   return n;
}

/* Takes @name on success */
static bool decompress_manifest_append(decompress_manifest_t *m, char *name,
      uint64_t size, int64_t mtime, uint32_t crc)
{
   decompress_manifest_line_t *line;

   if (m->count == m->cap)
   {
      size_t cap = m->cap ? m->cap * 2 : 64;
      decompress_manifest_line_t *lines = (decompress_manifest_line_t*)
         realloc(m->lines, cap * sizeof(*lines));
      if (!lines)
         return false;
      m->lines = lines;
      m->cap   = cap;
   }

   line        = &m->lines[m->count++];
   line->mtime = mtime;
   line->size  = size;
   line->name  = name;
   line->crc   = crc;
   line->seen  = false;
   return true;
}

/* A missing, unreadable or foreign file is an empty manifest */
static void decompress_manifest_load(decompress_manifest_t *m)
{
   void *buf   = NULL;
   int64_t len = 0;
   const char *p;
   const char *end;
   size_t hlen = STRLEN_CONST(DECOMPRESS_MANIFEST_HEADER);

   if (m->loaded)
      return;
   m->loaded = true;
   if (!filestream_read_file(m->path, &buf, &len) || !buf)
      return;

   p   = (const char*)buf;
   end = p + len;

   if (     (size_t)(end - p) < hlen
         || memcmp(p, DECOMPRESS_MANIFEST_HEADER, hlen)
         || (p + hlen < end && p[hlen] != '\n' && p[hlen] != '\r'))
   {
      free(buf);
      return;
   }
   p += hlen;

   while (p < end)
   {
      uint64_t crc, size, mtime;
      const char *line_end;
      const char *name;
      size_t name_len;
      char *copy;

      while (p < end && (*p == '\n' || *p == '\r'))
         p++;
      if (p >= end)
         break;
      for (line_end = p; line_end < end && *line_end != '\n'; line_end++)
         ;

      /* Malformed lines are skipped, not trusted */
      if (     !(p = decompress_manifest_parse_u64(p, line_end, 16, &crc))
            || p >= line_end || *p++ != ' '
            || !(p = decompress_manifest_parse_u64(p, line_end, 10, &size))
            || p >= line_end || *p++ != ' '
            || !(p = decompress_manifest_parse_u64(p, line_end, 10, &mtime))
            || p >= line_end || *p++ != ' ')
      {
         p = line_end;
         continue;
      }

      name     = p;
      name_len = (size_t)(line_end - name);
      if (name_len > 0 && name[name_len - 1] == '\r')
         name_len--;
      p        = line_end;

      if (     name_len == 0
            || name_len >= PATH_MAX_LENGTH
            || crc > 0xFFFFFFFFu
            || (mtime >> 63)
            || memchr(name, '\0', name_len)
            || memchr(name, '\r', name_len))
         continue;

      if (!(copy = decompress_manifest_strdup(name, name_len)))
         break;
      if (!decompress_manifest_append(m, copy, size, (int64_t)mtime,
               (uint32_t)crc))
      {
         free(copy);
         break;
      }
   }

   free(buf);
}

/* Of two lines with one name the first stays */
static bool decompress_manifest_map_build(decompress_manifest_t *m)
{
   size_t i;
   size_t kept    = 0;
   size_t cursor  = m->cursor;
   uint32_t *map  = NULL;

   if (m->map)
      return true;
   if (!RHMAP_TRYFIT(map, m->count + 1))
      return false;
   RHMAP_BORROW_KEYS(map);

   for (i = 0; i < m->count; i++)
   {
      if (RHMAP_HAS_STR(map, m->lines[i].name))
      {
         free(m->lines[i].name);
         if (i < m->cursor)
            cursor--;
         continue;
      }
      m->lines[kept] = m->lines[i];
      RHMAP_SET_STR(map, m->lines[kept].name, (uint32_t)kept);
      kept++;
   }

   m->count  = kept;
   m->cursor = cursor;
   m->map    = map;
   return true;
}

/* The index of @name's line, or -1 */
static ptrdiff_t decompress_manifest_find(decompress_manifest_t *m,
      const char *name)
{
   ptrdiff_t slot;

   /* Members keep their order from one archive to the next */
   if (     m->cursor < m->count
         && !strcmp(m->lines[m->cursor].name, name))
      return (ptrdiff_t)m->cursor++;

   if (!decompress_manifest_map_build(m))
      return -1;
   if ((slot = RHMAP_IDX_STR(m->map, name)) < 0)
      return -1;
   m->cursor = (size_t)m->map[slot] + 1;
   return (ptrdiff_t)m->map[slot];
}

/* An mtime of 0 is none: some platforms' stat leaves it 0 (the 3DS's) */
static bool decompress_manifest_get_mtime(const char *path, int64_t *mtime)
{
   return path_get_mtime(path, mtime) && *mtime;
}

/* The pending member is finished: it becomes a line */
static void decompress_manifest_commit(decompress_manifest_t *m)
{
   ptrdiff_t idx;
   int64_t mtime = 0;
   char *name    = m->pending_name;
   char *path    = m->pending_path;

   if (!name)
      return;
   m->pending_name = NULL;
   m->pending_path = NULL;

   if (!decompress_manifest_get_mtime(path, &mtime))
      m->no_mtime = true;
   else if (mtime >= 0)
   {
      if ((idx = decompress_manifest_find(m, name)) >= 0)
      {
         decompress_manifest_line_t *line = &m->lines[idx];
         if (     line->mtime != mtime
               || line->size  != m->pending_size
               || line->crc   != m->pending_crc)
         {
            line->mtime = mtime;
            line->size  = m->pending_size;
            line->crc   = m->pending_crc;
            m->dirty    = true;
         }
         line->seen = true;
      }
      /* Without the map a second line of this name could be added */
      else if (m->map
            && RHMAP_TRYFIT(m->map, m->count + 1)
            && decompress_manifest_append(m, name, m->pending_size, mtime,
               m->pending_crc))
      {
         m->lines[m->count - 1].seen = true;
         RHMAP_SET_STR(m->map, name, (uint32_t)(m->count - 1));
         m->cursor = m->count;
         m->dirty  = true;
         name      = NULL;
      }
   }

   free(name);
   free(path);
}

decompress_manifest_t *decompress_manifest_new(const char *target_dir,
      const char *archive_path)
{
   const char *base;
   char *leaf;
   size_t base_len;
   size_t leaf_size;
   size_t path_size;
   decompress_manifest_t *m;

   if (!target_dir || !*target_dir || !archive_path || !*archive_path)
      return NULL;
   base     = path_basename_nocompression(archive_path);
   base_len = strlen(base);
   if (!base_len)
      return NULL;

   /* ".<base>.crc" */
   leaf_size = 1 + base_len + sizeof(".crc");
   path_size = strlen(target_dir) + 1 + leaf_size;
   m         = (decompress_manifest_t*)calloc(1, sizeof(*m));
   leaf      = (char*)malloc(leaf_size);
   if (!m || !leaf || !(m->path = (char*)malloc(path_size)))
   {
      free(leaf);
      free(m);
      return NULL;
   }

   leaf[0] = '.';
   memcpy(leaf + 1, base, base_len);
   memcpy(leaf + 1 + base_len, ".crc", sizeof(".crc"));
   fill_pathname_join_special(m->path, target_dir, leaf, path_size);
   free(leaf);
   return m;
}

void decompress_manifest_free(decompress_manifest_t *m)
{
   size_t i;

   if (!m)
      return;
   RHMAP_FREE(m->map);
   for (i = 0; i < m->count; i++)
      free(m->lines[i].name);
   free(m->lines);
   free(m->pending_name);
   free(m->pending_path);
   free(m->path);
   free(m);
}

bool decompress_manifest_unchanged(decompress_manifest_t *m, const char *name,
      const char *path, uint64_t size, uint32_t crc)
{
   ptrdiff_t idx;
   int64_t mtime = 0;
   const decompress_manifest_line_t *line;

   if (!m || !name || !path)
      return false;
   decompress_manifest_load(m);
   decompress_manifest_commit(m);

   if ((idx = decompress_manifest_find(m, name)) < 0)
      return false;
   m->lines[idx].seen = true;
   line               = &m->lines[idx];

   /* The one stat a member costs, and only where its line could match */
   if (     m->no_mtime
         || line->size != size
         || line->crc  != crc
         || !decompress_manifest_get_mtime(path, &mtime)
         || mtime < 0)
      return false;

   /* FAT keeps mtimes in 2-second steps */
   if (mtime > line->mtime)
      return mtime - line->mtime <= 1;
   return line->mtime - mtime <= 1;
}

void decompress_manifest_begin(decompress_manifest_t *m, const char *name,
      const char *path, uint64_t size, uint32_t crc)
{
   size_t name_len;

   if (!m || !name || !path)
      return;
   decompress_manifest_load(m);
   decompress_manifest_commit(m);

   /* A name that is not one line of the file gets none */
   name_len = strlen(name);
   if (     m->no_mtime
         || name_len == 0
         || name_len >= PATH_MAX_LENGTH
         || strpbrk(name, "\r\n"))
      return;

   m->pending_name = decompress_manifest_strdup(name, name_len);
   m->pending_path = decompress_manifest_strdup(path, strlen(path));
   m->pending_size = size;
   m->pending_crc  = crc;
   if (!m->pending_name || !m->pending_path)
   {
      free(m->pending_name);
      free(m->pending_path);
      m->pending_name = NULL;
      m->pending_path = NULL;
   }
}

bool decompress_manifest_save(decompress_manifest_t *m, bool clean)
{
   size_t i;
   size_t cap;
   size_t len;
   char *text;
   bool ok;

   if (!m)
      return true;

   if (clean)
   {
      size_t kept = 0;

      decompress_manifest_load(m);
      decompress_manifest_commit(m);

      /* The archive no longer has the members not seen */
      for (i = 0; i < m->count; i++)
      {
         if (m->lines[i].seen)
            m->lines[kept++] = m->lines[i];
         else
            free(m->lines[i].name);
      }
      if (kept != m->count)
      {
         RHMAP_FREE(m->map);
         m->count  = kept;
         m->cursor = 0;
         m->dirty  = true;
      }
   }
   else
   {
      free(m->pending_name);
      free(m->pending_path);
      m->pending_name = NULL;
      m->pending_path = NULL;
   }

   if (!m->dirty)
      return true;

   /* crc, size and mtime take 8 + 20 + 20 digits at most */
   cap = sizeof(DECOMPRESS_MANIFEST_HEADER);
   for (i = 0; i < m->count; i++)
      cap += strlen(m->lines[i].name) + 52;
   if (!(text = (char*)malloc(cap)))
      return false;

   len = STRLEN_CONST(DECOMPRESS_MANIFEST_HEADER);
   memcpy(text, DECOMPRESS_MANIFEST_HEADER, len);
   text[len++] = '\n';
   for (i = 0; i < m->count; i++)
   {
      const decompress_manifest_line_t *line = &m->lines[i];
      size_t name_len                        = strlen(line->name);

      len        += decompress_manifest_put_u64(text + len, line->crc, 16, 8);
      text[len++] = ' ';
      len        += decompress_manifest_put_u64(text + len, line->size, 10, 1);
      text[len++] = ' ';
      len        += decompress_manifest_put_u64(text + len,
            (uint64_t)line->mtime, 10, 1);
      text[len++] = ' ';
      memcpy(text + len, line->name, name_len);
      len        += name_len;
      text[len++] = '\n';
   }

   /* Where it cannot be renamed into place it is written over the old
    * one: a torn manifest only reads as fewer lines.  One not written
    * at all is removed, which costs the next run work and nothing else. */
   ok =    filestream_write_file_atomic(m->path, text, (int64_t)len)
        || filestream_write_file(m->path, text, (int64_t)len);
   if (!ok)
      filestream_delete(m->path);
   free(text);
   if (ok)
      m->dirty = false;
   return ok;
}
