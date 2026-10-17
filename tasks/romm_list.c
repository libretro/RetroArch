/*  RetroArch - A frontend for libretro.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_miscellaneous.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <string/stdstring.h>
#include <formats/rjson.h>

#include "task_romm.h"

typedef struct romm_parse
{
   romm_list_cb_t cb;
   void *data;
   const char *next_ctx;      /* list context past its offset */
   unsigned depth;
   unsigned item_depth;
   unsigned id;
   unsigned count;
   unsigned total;
   unsigned offset;
   char key[32];
   char name[NAME_MAX_LENGTH];
   char fs_name[NAME_MAX_LENGTH];
   char system[NAME_MAX_LENGTH];
   char fs_slug[NAME_MAX_LENGTH];
   char cover[NAME_MAX_LENGTH];
   char path[4 * NAME_MAX_LENGTH];
   char label[NAME_MAX_LENGTH + 16];
   bool roms;
   bool in_items;
   bool multi;
   bool display_name;
   bool bad;
} romm_parse_t;

/* Ids, counts and offsets are small; a negative or huge value from
 * the server must not wrap a sum later */
static unsigned romm_uint(const char *s)
{
   unsigned long v;

   if (*s == '-')
      return 0;
   v = strtoul(s, NULL, 10);
   return v > 0x3fffffffUL ? 0x3fffffffU : (unsigned)v;
}

/* Windows device names, with or without an extension */
static bool romm_reserved_name(const char *s, size_t len)
{
   static const char *const devices[] = { "con", "prn", "aux", "nul" };
   size_t stem = 0, i;

   while (stem < len && s[stem] != '.')
      stem++;
   while (stem > 0 && s[stem - 1] == ' ')
      stem--;
   if (stem == 3)
      for (i = 0; i < ARRAY_SIZE(devices); i++)
         if (!strncasecmp(s, devices[i], 3))
            return true;
   if (stem == 6 && !strncasecmp(s, "conin$", 6))
      return true;
   if (stem == 7 && !strncasecmp(s, "conout$", 7))
      return true;
   return stem == 4 && s[3] >= '1' && s[3] <= '9'
      && (!strncasecmp(s, "com", 3) || !strncasecmp(s, "lpt", 3));
}

/* Nothing that is a bad file name on any system we run on: separators,
 * drives, wildcards, controls, device names, and a trailing dot or space
 * that Windows drops, which would make two names one */
static bool romm_span_is_safe(const char *s, size_t len)
{
   size_t i;

   if (   !len
       || (len == 1 && s[0] == '.')
       || (len == 2 && s[0] == '.' && s[1] == '.')
       || s[len - 1] == '.' || s[len - 1] == ' '
       || romm_reserved_name(s, len))
      return false;
   for (i = 0; i < len; i++)
      if ((unsigned char)s[i] < 0x20 || strchr("/\\:*?\"<>|", s[i]))
         return false;
   return true;
}

bool romm_name_is_safe(const char *s)
{
   return s && romm_span_is_safe(s, strlen(s));
}

/* Served from the server's own /assets; no other location is fetched */
static bool romm_cover_is_safe(const char *s)
{
   if (!string_starts_with(s, "/assets/") || strstr(s, ".."))
      return false;
   for (; *s; s++)
      if ((unsigned char)*s <= 0x20 || *s == '\\' || *s == '?' || *s == '#' || *s == '|')
         return false;
   return true;
}

/* Copies the field up to the next '|' into @out, unless that is NULL.
 * Returns the start of the following field, or NULL at the last one. */
static const char *romm_field(const char *s, char *out, size_t size,
      bool *ok)
{
   const char *end = strchr(s, '|');
   size_t len      = end ? (size_t)(end - s) : strlen(s);

   if (out)
   {
      if (len >= size)
      {
         *ok = false;
         len = 0;
      }
      memcpy(out, s, len);
      out[len] = '\0';
   }
   return end ? end + 1 : NULL;
}

bool romm_list_ctx_split(const char *ctx, unsigned *offset, unsigned *id,
      char *system, size_t system_size, char *fs_slug, size_t slug_size)
{
   char num[16];
   bool ok = true;

   if (!ctx || !(ctx = romm_field(ctx, num, sizeof(num), &ok)))
      return false;
   *offset = romm_uint(num);
   if (!(ctx = romm_field(ctx, num, sizeof(num), &ok)))
      return false;
   *id = romm_uint(num);
   if (!(ctx = romm_field(ctx, system, system_size, &ok)))
      return false;
   romm_field(ctx, fs_slug, slug_size, &ok);
   return ok && *id
      && (!system  || romm_name_is_safe(system))
      && (!fs_slug || romm_name_is_safe(fs_slug));
}

bool romm_rom_ctx_split(const char *ctx, unsigned *id, bool *multi,
      char *cover, size_t cover_size, char *fs_name, size_t name_size)
{
   char num[16];
   bool ok = true;

   if (!ctx || !(ctx = romm_field(ctx, num, sizeof(num), &ok)))
      return false;
   *id = romm_uint(num);
   if (!(ctx = romm_field(ctx, num, sizeof(num), &ok)))
      return false;
   *multi = num[0] == '1';
   if (!(ctx = romm_field(ctx, cover, cover_size, &ok)))
      return false;
   /* The last field is the file name, which is checked not to hold '|' */
   if (strlcpy(fs_name, ctx, name_size) >= name_size)
      return false;
   return ok && *id && romm_name_is_safe(fs_name)
      && (!*cover || romm_cover_is_safe(cover));
}

static void romm_set(romm_parse_t *p, char *dst, size_t size,
      const char *str, size_t len)
{
   if (len >= size || memchr(str, 0, len))
   {
      p->bad = true;
      return;
   }
   memcpy(dst, str, len);
   dst[len] = '\0';
}

static bool romm_emit(romm_parse_t *p)
{
   const char *label;

   if (p->bad || !p->id)
      return true;

   if (p->roms)
   {
      if (!romm_name_is_safe(p->fs_name))
         return true;
      if (!romm_cover_is_safe(p->cover))
         p->cover[0] = '\0';
      snprintf(p->path, sizeof(p->path), "%u|%u|%s|%s",
            p->id, (unsigned)p->multi, p->cover, p->fs_name);
      label = p->name[0] ? p->name : p->fs_name;
      return p->cb(p->data, p->path, label, false);
   }

   if (!p->count)
      return true;
   if (!p->name[0])
      strlcpy(p->name, p->fs_slug, sizeof(p->name));
   if (!p->system[0])
   {
      /* A platform libretro has no name for is called what RomM calls it,
       * made fit for a file name */
      size_t n = strlcpy(p->system, p->name, sizeof(p->system));
      char *c;

      while ((c = strpbrk(p->system, "/\\:*?\"<>|")))
         *c = '_';
      while (n && (p->system[n - 1] == '.' || p->system[n - 1] == ' '))
         p->system[--n] = '\0';
   }
   if (!romm_name_is_safe(p->system) || !romm_name_is_safe(p->fs_slug))
      return true;
   snprintf(p->path, sizeof(p->path), "0|%u|%s|%s",
         p->id, p->system, p->fs_slug);
   snprintf(p->label, sizeof(p->label), "%s (%u)", p->name, p->count);
   return p->cb(p->data, p->path, p->label, false);
}

static bool romm_json_member(void *context, const char *str, size_t len)
{
   romm_parse_t *p = (romm_parse_t*)context;
   (void)len;

   if (p->roms && p->depth == 1)
      p->in_items = string_is_equal(str, "items");
   strlcpy(p->key, str, sizeof(p->key));
   return true;
}

static bool romm_json_string(void *context, const char *str, size_t len)
{
   romm_parse_t *p = (romm_parse_t*)context;

   if (p->depth != p->item_depth || (p->roms && !p->in_items))
      return true;

   if (p->roms)
   {
      if (string_is_equal(p->key, "fs_name"))
         romm_set(p, p->fs_name, sizeof(p->fs_name), str, len);
      else if (string_is_equal(p->key, "fs_name_no_ext"))
         romm_set(p, p->name, sizeof(p->name), str, len);
      else if (string_is_equal(p->key, "path_cover_large"))
      {
         /* Drop the cache-busting query */
         const char *q = memchr(str, '?', len);
         romm_set(p, p->cover, sizeof(p->cover), str, q ? (size_t)(q - str) : len);
      }
   }
   else if (string_is_equal(p->key, "display_name"))
   {
      p->display_name = true;
      romm_set(p, p->name, sizeof(p->name), str, len);
   }
   else if (string_is_equal(p->key, "name") && !p->display_name)
      romm_set(p, p->name, sizeof(p->name), str, len);
   else if (string_is_equal(p->key, "libretro_slug"))
      romm_set(p, p->system, sizeof(p->system), str, len);
   else if (string_is_equal(p->key, "fs_slug"))
      romm_set(p, p->fs_slug, sizeof(p->fs_slug), str, len);
   return true;
}

static bool romm_json_number(void *context, const char *str, size_t len)
{
   romm_parse_t *p = (romm_parse_t*)context;
   (void)len;

   if (p->depth == p->item_depth && (!p->roms || p->in_items))
   {
      if (string_is_equal(p->key, "id"))
         p->id = romm_uint(str);
      else if (string_is_equal(p->key, "rom_count"))
         p->count = romm_uint(str);
   }
   else if (p->roms && p->depth == 1 && string_is_equal(p->key, "total"))
      p->total = romm_uint(str);
   return true;
}

static bool romm_json_start_object(void *context)
{
   romm_parse_t *p = (romm_parse_t*)context;

   if (++p->depth == p->item_depth && (!p->roms || p->in_items))
   {
      p->id           = 0;
      p->count        = 0;
      p->multi        = false;
      p->display_name = false;
      p->bad          = false;
      p->name[0]      = '\0';
      p->fs_name[0]   = '\0';
      p->system[0]    = '\0';
      p->fs_slug[0]   = '\0';
      p->cover[0]     = '\0';
   }
   return true;
}

static bool romm_json_end_object(void *context)
{
   romm_parse_t *p = (romm_parse_t*)context;
   bool ok         = true;

   if (p->depth == p->item_depth && (!p->roms || p->in_items))
      ok = romm_emit(p);
   p->depth--;
   return ok;
}

static bool romm_json_start_array(void *context)
{
   ((romm_parse_t*)context)->depth++;
   return true;
}

static bool romm_json_end_array(void *context)
{
   ((romm_parse_t*)context)->depth--;
   return true;
}

static bool romm_json_boolean(void *context, bool value)
{
   romm_parse_t *p = (romm_parse_t*)context;

   if (     p->depth == p->item_depth && p->roms && p->in_items
         && string_is_equal(p->key, "has_multiple_files"))
      p->multi = value;
   return true;
}

bool romm_list_parse(const char *json, size_t len, const char *list_ctx,
      romm_list_cb_t cb, void *data)
{
   rjson_t *rj;
   romm_parse_t *p;
   enum rjson_type end;
   bool ok = false;

   if (!json || !len || !cb)
      return false;
   if (!(p = (romm_parse_t*)calloc(1, sizeof(*p))))
      return false;

   p->cb   = cb;
   p->data = data;
   p->roms = list_ctx && *list_ctx;

   if (p->roms)
   {
      const char *bar = strchr(list_ctx, '|');
      if (!bar)
         goto done;
      p->offset       = romm_uint(list_ctx);
      p->next_ctx     = bar + 1;
      p->item_depth   = 3;
   }
   else
      p->item_depth   = 2;

   if (!(rj = rjson_open_buffer(json, len)))
      goto done;

   end = rjson_parse(rj, p,
         romm_json_member, romm_json_string, romm_json_number,
         romm_json_start_object, romm_json_end_object,
         romm_json_start_array, romm_json_end_array,
         romm_json_boolean, NULL);
   rjson_free(rj);

   if (end != RJSON_DONE)
      goto done;
   ok = true;

   if (p->roms && p->total > p->offset + ROMM_PAGE_SIZE)
   {
      snprintf(p->path, sizeof(p->path), "%u|%s",
            p->offset + ROMM_PAGE_SIZE, p->next_ctx);
      ok = p->cb(p->data, p->path, NULL, true);
   }

done:
   free(p);
   return ok;
}

/* Archive members */

/* Every path segment is a safe name; a trailing '/' marks a directory.
 * No more than ROMM_ENTRY_MAX_DEPTH of them, which bounds what removing
 * an unpack that failed has to walk. */
#define ROMM_ENTRY_MAX_DEPTH 8

bool romm_entry_is_safe(const char *name)
{
   const char *seg = name;
   unsigned depth  = 0;

   if (!name || !*name)
      return false;
   for (;;)
   {
      const char *end = strchr(seg, '/');
      size_t len      = end ? (size_t)(end - seg) : strlen(seg);

      if (!len && (end || seg == name))
         return false;
      if (len && (++depth > ROMM_ENTRY_MAX_DEPTH || !romm_span_is_safe(seg, len)))
         return false;
      if (!end)
         return true;
      seg = end + 1;
   }
}

static bool romm_ext_in(const char *name, const char *list)
{
   const char *base = strrchr(name, '/');
   const char *dot;
   size_t len;

   base = base ? base + 1 : name;
   if (!(dot = strrchr(base, '.')))
      return false;
   len = strlen(++dot);
   while (*list)
   {
      size_t n = strcspn(list, ",");
      if (n == len && !strncasecmp(dot, list, n))
         return true;
      list += n + (list[n] == ',');
   }
   return false;
}

static bool romm_word_at(const char **p, const char *word)
{
   size_t n = strlen(word);

   if (strncasecmp(*p, word, n))
      return false;
   *p += n;
   return true;
}

/* The disc a "(Disc 2)", "(CD 2)", "(Disk 1 of 2)", "(Disc 2A)" or
 * "(Disc B)" tag in the name claims, as RomM reads it; -1 for none */
static int romm_disc_tag(const char *name)
{
   for (; *name && *name != '/'; name++)
   {
      const char *p = name + 1;
      bool spaced;
      int number = 0;

      if (*name != '(')
         continue;
      if (   !romm_word_at(&p, "disc") && !romm_word_at(&p, "disk")
          && !romm_word_at(&p, "cd")   && !romm_word_at(&p, "disque"))
         continue;
      for (spaced = false; *p == ' '; p++)
         spaced = true;
      if (*p >= '0' && *p <= '9')
      {
         for (number = 0; *p >= '0' && *p <= '9'; p++)
            if (number < 100000000)
               number = number * 10 + (*p - '0');
         if (    (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z'))
            p++;
      }
      else if (spaced && (   (*p >= 'a' && *p <= 'z')
                          || (*p >= 'A' && *p <= 'Z')))
      {
         number = (*p | 0x20) - 'a' + 1;
         p++;
      }
      else
         continue;
      if (*p == ' ')
      {
         const char *q = p;
         while (*q == ' ')
            q++;
         if (romm_word_at(&q, "of") && *q == ' ')
         {
            while (*q == ' ')
               q++;
            if (*q >= '0' && *q <= '9')
            {
               while (*q >= '0' && *q <= '9')
                  q++;
               p = q;
            }
         }
      }
      if (*p == ')')
         return number;
   }
   return -1;
}

/* From the file name, else from the same tag in the name of the folder
 * it is directly in ("G (Disc 2)/g.cue"), not any further up */
static int romm_disc_number(const char *name)
{
   const char *base = strrchr(name, '/');
   const char *dir  = name;
   const char *s;
   int number       = romm_disc_tag(base ? base + 1 : name);

   if (number >= 0 || !base)
      return number;
   for (s = name; s < base; s++)
      if (*s == '/')
         dir = s + 1;
   return romm_disc_tag(dir);
}

/* RomM's own order: numbered discs first and in order, then by name */
static int romm_disc_cmp(const char *a, const char *b)
{
   int na = romm_disc_number(a);
   int nb = romm_disc_number(b);

   if ((na < 0) != (nb < 0))
      return na < 0 ? 1 : -1;
   if (na != nb)
      return na < nb ? -1 : 1;
   return strcmp(a, b);
}

/* Descriptors name their tracks; those track files are not discs.
 * A set without one is its images. */
#define ROMM_DESCRIPTORS "cue,gdi,ccd,mds"
#define ROMM_IMAGES      "chd,iso,pbp,cso,cdi,nrg,gcm,wbfs,rvz,wia,ciso,zso"
#define ROMM_RAW_IMAGES  "bin,img,mdf"

bool romm_archive_target(const char *const *names, const uint32_t *sizes,
      size_t count, const char *m3u_name, char *target, size_t target_size,
      char **m3u)
{
   size_t i, j, discs = 0, best = count, m3u_len = 0;
   bool descriptors   = false;
   size_t *pick       = NULL;
   uint32_t best_size = 0;

   *m3u = NULL;
   for (i = 0; i < count; i++)
      if (romm_ext_in(names[i], ROMM_DESCRIPTORS))
         descriptors = true;

   if (!(pick = (size_t*)malloc((count ? count : 1) * sizeof(*pick))))
      return false;

   for (i = 0; i < count; i++)
   {
      size_t len = strlen(names[i]);

      if (!len || names[i][len - 1] == '/' || romm_ext_in(names[i], "m3u"))
         continue;
      if (   romm_ext_in(names[i], ROMM_DESCRIPTORS)
          || romm_ext_in(names[i], ROMM_IMAGES)
          || (!descriptors && romm_ext_in(names[i], ROMM_RAW_IMAGES)))
      {
         /* Insertion sort into RomM's order */
         for (j = discs; j > 0 && romm_disc_cmp(names[pick[j - 1]], names[i]) > 0; j--)
            pick[j] = pick[j - 1];
         pick[j] = i;
         discs++;
      }
      else if (best == count || sizes[i] > best_size)
      {
         best      = i;
         best_size = sizes[i];
      }
   }

   if (discs == 1)
   {
      if (strlcpy(target, names[pick[0]], target_size) >= target_size)
         goto fail;
   }
   else if (discs > 1)
   {
      char *text;

      for (i = 0; i < discs; i++)
         m3u_len += strlen(names[pick[i]]) + 1;
      if (!(text = (char*)malloc(m3u_len + 1)))
         goto fail;
      for (i = 0, m3u_len = 0; i < discs; i++)
      {
         size_t n = strlen(names[pick[i]]);

         memcpy(text + m3u_len, names[pick[i]], n);
         m3u_len += n;
         text[m3u_len++] = '\n';
      }
      text[m3u_len] = '\0';
      *m3u          = text;
      if (strlcpy(target, m3u_name, target_size) >= target_size)
         goto fail;
   }
   else if (best != count)
   {
      if (strlcpy(target, names[best], target_size) >= target_size)
         goto fail;
   }
   else
      goto fail;

   free(pick);
   return true;

fail:
   free(*m3u);
   *m3u = NULL;
   free(pick);
   return false;
}
