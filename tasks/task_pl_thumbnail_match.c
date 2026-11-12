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

#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>
#include <encodings/crc32.h>
#include <file/file_path.h>
#include <formats/rjson.h>
#include <queues/task_queue.h>
#include <retro_dirent.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "tasks_internal.h"
#include "../file_path_special.h"
#include "../playlist.h"
#include "../gfx/gfx_thumbnail_match.h"
#include "../gfx/gfx_thumbnail_results.h"
#include "../ui/ui_companion_driver.h"
#if defined(RARCH_INTERNAL) && defined(HAVE_MENU)
#include "../menu/menu_driver.h"
#endif

/* Bytes read or written per work item */
#define PL_THUMB_MATCH_CHUNK     65536
#define PL_THUMB_MATCH_MAX_CACHE (64 * 1024 * 1024)
#define PL_THUMB_MATCH_VERSION   "1"
#define PL_THUMB_MATCH_NONE      ((size_t)-1)

enum pl_thumb_match_status
{
   PL_THUMB_MATCH_CACHE_OPEN = 0,
   PL_THUMB_MATCH_CACHE_READ,
   PL_THUMB_MATCH_CACHE_PARSE,
   PL_THUMB_MATCH_CACHE_SORT,
   PL_THUMB_MATCH_PLAYLIST,
   PL_THUMB_MATCH_KEYS,
   PL_THUMB_MATCH_LIST,
   PL_THUMB_MATCH_FINALIZE,
   PL_THUMB_MATCH_RESOLVE,
   PL_THUMB_MATCH_SORT,
   PL_THUMB_MATCH_SERIALIZE,
   PL_THUMB_MATCH_SUM,
   PL_THUMB_MATCH_WRITE,
   PL_THUMB_MATCH_DONE
};

enum pl_thumb_match_flags
{
   /* Resolve the playlist; without it, only load the cache */
   PL_THUMB_MATCH_FLAG_RESOLVE       = (1 << 0),
   /* A load whose callback pushes the resolve */
   PL_THUMB_MATCH_FLAG_THEN_RESOLVE  = (1 << 1),
   PL_THUMB_MATCH_FLAG_ALLOW_NON_PNG = (1 << 2),
   /* Not history or favourites */
   PL_THUMB_MATCH_FLAG_COLLECTION    = (1 << 3),
   PL_THUMB_MATCH_FLAG_CACHE_FOUND   = (1 << 4),
   /* The resolved table differs from the cache */
   PL_THUMB_MATCH_FLAG_CHANGED       = (1 << 5),
   /* The load handed its cache_crc over */
   PL_THUMB_MATCH_FLAG_SUMMED        = (1 << 6)
};

/* Pointers borrow from the task's own playlist. */
typedef struct pl_thumb_match_entry
{
   const char *path;
   const char *label;  /* NULL: the file name stem */
   const char *crc32;
   const char *t1;     /* the entry's name in its rdb */
   size_t rec;
   unsigned system;
} pl_thumb_match_entry_t;

/* History and favourites: the db_name core info gives a core's entries
 * that have none, read on the main thread at push. */
typedef struct pl_thumb_match_core
{
   char *core_path;    /* "" for an entry without one */
   char *db_name;      /* NULL: none; the content's folder stands in */
} pl_thumb_match_core_t;

typedef struct pl_thumb_match
{
   playlist_config_t playlist_config;
   char dir_thumbnails[DIR_MAX_LENGTH];
   char dir_database[DIR_MAX_LENGTH];
   char cache_path[PATH_MAX_LENGTH];
   char path[PATH_MAX_LENGTH];
   char item[4][PATH_MAX_LENGTH];
   char name[NAME_MAX_LENGTH];
   char stem[NAME_MAX_LENGTH];
   char scrub[NAME_MAX_LENGTH];
   RFILE *file;
   char *buf;
   const char *out;
   rjson_t *json;
   rjsonwriter_t *writer;
   playlist_parse_t *parse;
   playlist_t *playlist;
   pl_thumb_match_entry_t *entries;
   char **systems;
   pl_thumb_match_core_t *cores;
   struct RDIR *dir;
   gfx_thumbnail_match_index_t *index;
   gfx_thumbnail_results_t *results;
   size_t buf_len;
   size_t buf_pos;
   size_t entry_count;
   size_t system_count;
   size_t core_count;
   size_t index_count;
   size_t pos;
   size_t item_rec;
   uint32_t cache_crc;
   uint32_t out_crc;
   unsigned cur_system;
   unsigned cur_type;
   unsigned depth;
   int member;
   enum pl_thumb_match_status status;
   uint8_t flags;
   uint8_t item_keys;
   bool version_ok;
} pl_thumb_match_t;

static const char *const pl_thumb_match_keys[4] =
{
   "path",
   "label",
   "crc32",
   "system"
};

static bool pl_thumb_match_push(const playlist_config_t *config,
      const char *dir_thumbnails, const char *dir_database, uint8_t flags,
      uint32_t cache_crc, pl_thumb_match_core_t *cores, size_t core_count);

static void pl_thumb_match_cores_free(pl_thumb_match_core_t *cores,
      size_t count)
{
   size_t i;
   for (i = 0; i < count; i++)
   {
      free(cores[i].core_path);
      free(cores[i].db_name);
   }
   free(cores);
}

/* The push's lookup for @core_path, or NULL: it made none. */
static const pl_thumb_match_core_t *pl_thumb_match_core(
      const pl_thumb_match_t *m, const char *core_path)
{
   size_t i;
   if (!core_path)
      core_path = "";
   for (i = 0; i < m->core_count; i++)
      if (string_is_equal(m->cores[i].core_path, core_path))
         return &m->cores[i];
   return NULL;
}

static bool pl_thumb_match_within(void *ud)
{
   return task_nbio_slice_within_budget(ud, 0, 0);
}

/* A write cut short leaves no temporary file. */
static void pl_thumb_match_close(pl_thumb_match_t *m)
{
   if (!m->file)
      return;
   if (m->status == PL_THUMB_MATCH_WRITE)
      filestream_commit_atomic(m->file, m->cache_path, false);
   else
      filestream_close(m->file);
   m->file = NULL;
}

/* The table holds its own copies of what it took from the playlist. */
static void pl_thumb_match_drop_playlist(pl_thumb_match_t *m)
{
   if (m->parse)
      playlist_parse_abort(m->parse);
   m->parse = NULL;
   playlist_free(m->playlist);
   m->playlist = NULL;
   free(m->entries);
   m->entries     = NULL;
   m->entry_count = 0;
}

/* On the handler's thread, so cleanup has little left to free. */
static void pl_thumb_match_fail(pl_thumb_match_t *m)
{
   pl_thumb_match_close(m);
   pl_thumb_match_drop_playlist(m);
   gfx_thumbnail_results_free(m->results);
   m->results = NULL;
   m->status  = PL_THUMB_MATCH_DONE;
}

/* A load ends here; a resolve goes on to the playlist. */
static void pl_thumb_match_cache_done(pl_thumb_match_t *m)
{
   if (m->file)
   {
      filestream_close(m->file);
      m->file = NULL;
   }
   free(m->buf);
   m->buf    = NULL;
   m->status = (m->flags & PL_THUMB_MATCH_FLAG_RESOLVE)
      ? PL_THUMB_MATCH_PLAYLIST : PL_THUMB_MATCH_DONE;
}

static void pl_thumb_match_cache_open(pl_thumb_match_t *m)
{
   int64_t size;
   if (!(m->file = filestream_open(m->cache_path,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE)))
   {
      pl_thumb_match_cache_done(m);
      return;
   }
   m->flags |= PL_THUMB_MATCH_FLAG_CACHE_FOUND;
   size      = filestream_get_size(m->file);
   /* A resolve only sums it, through one chunk */
   if (     size <= 0
         || size > PL_THUMB_MATCH_MAX_CACHE
         || !(m->buf = (char*)malloc((m->flags & PL_THUMB_MATCH_FLAG_RESOLVE)
               ? PL_THUMB_MATCH_CHUNK : (size_t)size)))
   {
      pl_thumb_match_cache_done(m);
      return;
   }
   m->buf_len = (size_t)size;
   m->buf_pos = 0;
   m->status  = PL_THUMB_MATCH_CACHE_READ;
}

static void pl_thumb_match_cache_read(pl_thumb_match_t *m)
{
   int64_t got;
   size_t want = m->buf_len - m->buf_pos;
   char *dst   = (m->flags & PL_THUMB_MATCH_FLAG_RESOLVE)
      ? m->buf : m->buf + m->buf_pos;

   if (want > PL_THUMB_MATCH_CHUNK)
      want = PL_THUMB_MATCH_CHUNK;
   got = filestream_read(m->file, dst, (int64_t)want);
   if (got > 0)
   {
      m->cache_crc = encoding_crc32(m->cache_crc, (const uint8_t*)dst,
            (size_t)got);
      m->buf_pos  += (size_t)got;
      if (m->buf_pos < m->buf_len)
         return;
   }
   filestream_close(m->file);
   m->file = NULL;
   /* A resolve wants only the crc */
   if (     m->buf_pos < m->buf_len
         || (m->flags & PL_THUMB_MATCH_FLAG_RESOLVE)
         || !(m->json = rjson_open_buffer(m->buf, m->buf_len)))
   {
      pl_thumb_match_cache_done(m);
      return;
   }
   m->depth  = 0;
   m->member = -1;
   m->status = PL_THUMB_MATCH_CACHE_PARSE;
}

/* One token: 1 at the end, -1 when the file is not ours. */
static int pl_thumb_match_cache_token(pl_thumb_match_t *m)
{
   size_t _len;
   const char *str;

   switch (rjson_next(m->json))
   {
      case RJSON_OBJECT:
      case RJSON_ARRAY:
         if (++m->depth == 3)
         {
            m->item_keys = 0;
            m->item_rec  = PL_THUMB_MATCH_NONE;
         }
         return 0;
      case RJSON_OBJECT_END:
      case RJSON_ARRAY_END:
         m->depth--;
         return 0;
      case RJSON_STRING:
      case RJSON_NUMBER:
         break;
      case RJSON_DONE:
         return m->version_ok ? 1 : -1;
      case RJSON_ERROR:
         return -1;
      default:
         return 0;
   }

   str = rjson_get_string(m->json, &_len);
   if (     rjson_get_context_type(m->json) == RJSON_OBJECT
         && (rjson_get_context_count(m->json) & 1))
   {
      unsigned i;
      m->member = -1;
      if (m->depth == 1 && string_is_equal(str, "version"))
         m->member = 0;
      else if (m->depth == 3)
      {
         for (i = 0; i < 4; i++)
            if (string_is_equal(str, pl_thumb_match_keys[i]))
               m->member = 1 + (int)i;
         if ((i = gfx_thumbnail_result_type_of(str))
               < GFX_THUMBNAIL_RESULT_TYPES)
            m->member = 5 + (int)i;
      }
      return 0;
   }

   if (m->member == 0)
   {
      m->version_ok = string_is_equal(str, PL_THUMB_MATCH_VERSION);
      return m->version_ok ? 0 : -1;
   }
   if (m->member >= 1 && m->member <= 4)
   {
      unsigned k = (unsigned)m->member - 1;
      strlcpy(m->item[k], str, sizeof(m->item[k]));
      m->item_keys |= (uint8_t)(1 << k);
      if (m->item_keys == 0xF && m->item_rec == PL_THUMB_MATCH_NONE)
         m->item_rec = gfx_thumbnail_results_add(m->results, m->item[0],
               m->item[1], m->item[2], m->item[3]);
   }
   else if (m->member >= 5 && m->item_rec != PL_THUMB_MATCH_NONE)
      gfx_thumbnail_results_set(m->results, m->item_rec,
            (unsigned)m->member - 5, str);
   return 0;
}

static void pl_thumb_match_cache_parse(pl_thumb_match_t *m)
{
   int r = pl_thumb_match_cache_token(m);
   if (!r)
      return;
   rjson_free(m->json);
   m->json = NULL;
   if (r < 0)
   {
      gfx_thumbnail_results_free(m->results);
      m->results = gfx_thumbnail_results_new();
   }
   m->status = PL_THUMB_MATCH_CACHE_SORT;
}

static void pl_thumb_match_cache_sort(pl_thumb_match_t *m)
{
   if (!gfx_thumbnail_results_sort_step(m->results))
      pl_thumb_match_cache_done(m);
}

static void pl_thumb_match_playlist(pl_thumb_match_t *m, nbio_budget_t *b)
{
   int r;
   if (!m->parse && !(m->parse = playlist_parse_begin(&m->playlist_config)))
   {
      pl_thumb_match_fail(m);
      return;
   }
   if (!(r = playlist_parse_step(m->parse, pl_thumb_match_within, b)))
      return;
   m->playlist = playlist_parse_end(m->parse);
   m->parse    = NULL;
   if (     r < 0
         || !m->playlist
         || !(m->entries = (pl_thumb_match_entry_t*)calloc(
               playlist_size(m->playlist) + 1, sizeof(*m->entries))))
   {
      pl_thumb_match_fail(m);
      return;
   }
   m->pos    = 0;
   m->status = PL_THUMB_MATCH_KEYS;
}

static void pl_thumb_match_key(pl_thumb_match_t *m)
{
   unsigned i;
   pl_thumb_match_entry_t *e;
   const char *db_name                = NULL;
   const struct playlist_entry *entry = NULL;

   if (m->pos >= playlist_size(m->playlist))
   {
      m->cur_system = 0;
      m->cur_type   = 0;
      m->status     = PL_THUMB_MATCH_LIST;
      return;
   }
   playlist_get_index(m->playlist, m->pos++, &entry);
   if (!entry || string_is_empty(entry->path))
      return;
   /* As in the menu (gfx_thumbnail_get_content_dir), a core that names
    * no database leaves the content's folder name. */
   if (     string_is_empty(entry->db_name)
         && !(m->flags & PL_THUMB_MATCH_FLAG_COLLECTION))
   {
      const pl_thumb_match_core_t *core =
         pl_thumb_match_core(m, entry->core_path);
      if (!core)
         return;
      db_name = core->db_name;
      if (     !gfx_thumbnail_system_name(m->name, sizeof(m->name), db_name)
            && !fill_pathname_parent_dir_name(m->name, entry->path,
               sizeof(m->name)))
         return;
   }
   else
   {
      playlist_get_db_name(m->playlist, m->pos - 1, &db_name);
      if (!gfx_thumbnail_system_name(m->name, sizeof(m->name), db_name))
         return;
   }
   for (i = 0; i < m->system_count; i++)
      if (string_is_equal(m->systems[i], m->name))
         break;
   if (i == m->system_count)
   {
      char *copy;
      char **systems = (char**)realloc(m->systems,
            (m->system_count + 1) * sizeof(*systems));
      if (!systems)
         return;
      m->systems = systems;
      if (!(copy = strdup(m->name)))
         return;
      m->systems[m->system_count++] = copy;
   }
   e         = &m->entries[m->entry_count++];
   e->path   = entry->path;
   e->label  = string_is_empty(entry->label) ? NULL : entry->label;
   e->crc32  = entry->crc32 ? entry->crc32 : "";
   e->t1     = NULL;
   e->rec    = PL_THUMB_MATCH_NONE;
   e->system = i;
}

static void pl_thumb_match_next_type(pl_thumb_match_t *m)
{
   if (m->dir)
   {
      retro_closedir(m->dir);
      m->dir = NULL;
   }
   if (m->index)
   {
      gfx_thumbnail_match_index_free(m->index);
      m->index = NULL;
   }
   if (++m->cur_type >= GFX_THUMBNAIL_RESULT_TYPES)
   {
      m->cur_type = 0;
      m->cur_system++;
   }
   m->status = PL_THUMB_MATCH_LIST;
}

/* One folder entry per call; a missing folder has no candidates. */
static void pl_thumb_match_list(pl_thumb_match_t *m)
{
   if (m->cur_system >= m->system_count)
   {
      pl_thumb_match_drop_playlist(m);
      m->status = PL_THUMB_MATCH_SORT;
      return;
   }
   if (!m->index)
   {
      fill_pathname_join_special(m->path, m->dir_thumbnails,
            m->systems[m->cur_system], sizeof(m->path));
      fill_pathname_join_special(m->path, m->path,
            gfx_thumbnail_result_type_dir(m->cur_type), sizeof(m->path));
      if (     !(m->dir = retro_opendir(m->path))
            || retro_dirent_error(m->dir)
            || !(m->index = gfx_thumbnail_match_index_new()))
         pl_thumb_match_next_type(m);
      return;
   }
   if (retro_readdir(m->dir))
   {
      const char *name = retro_dirent_get_name(m->dir);
      if (     name
            && !retro_dirent_is_dir(m->dir, NULL)
            && gfx_thumbnail_name_allowed(name,
               (m->flags & PL_THUMB_MATCH_FLAG_ALLOW_NON_PNG) != 0)
            && !gfx_thumbnail_match_index_add(m->index, name))
         pl_thumb_match_fail(m);
      return;
   }
   retro_closedir(m->dir);
   m->dir = NULL;
   if (!(m->index_count = gfx_thumbnail_match_index_count(m->index)))
   {
      pl_thumb_match_next_type(m);
      return;
   }
   m->status = PL_THUMB_MATCH_FINALIZE;
}

/* Out of memory, the engine finds nothing: keep the cache rather than
 * write a poorer one. */
static void pl_thumb_match_finalize(pl_thumb_match_t *m)
{
   if (gfx_thumbnail_match_index_finalize_step(m->index))
      return;
   if (gfx_thumbnail_match_index_count(m->index) != m->index_count)
   {
      pl_thumb_match_fail(m);
      return;
   }
   m->pos    = 0;
   m->status = PL_THUMB_MATCH_RESOLVE;
}

/* Whether the menu finds @found by @name itself (label or file name). */
static bool pl_thumb_match_is_exact(pl_thumb_match_t *m,
      const char *found, const char *name)
{
   size_t _len = gfx_thumbnail_scrub_name(m->scrub, sizeof(m->scrub), name);
   return    strncmp(found, m->scrub, _len) == 0
          && found[_len] == '.'
          && !strchr(found + _len + 1, '.');
}

/* @s, or its copy in @buf cut where the menu cuts its own */
static const char *pl_thumb_match_cut(const char *s, char *buf, size_t len)
{
   if (strlen(s) < len)
      return s;
   strlcpy(buf, s, len);
   return buf;
}

/* One entry of the current system per call. */
static void pl_thumb_match_resolve(pl_thumb_match_t *m)
{
   const char *queries[3];
   const char *label;
   const char *found = NULL;
   pl_thumb_match_entry_t *e;

   if (m->pos >= m->entry_count)
   {
      pl_thumb_match_next_type(m);
      return;
   }
   e = &m->entries[m->pos++];
   if (e->system != m->cur_system)
      return;
   /* The file name without its extension, as the menu's label */
   fill_pathname(m->stem, path_basename(e->path), "", sizeof(m->stem));
   label      = e->label
      ? pl_thumb_match_cut(e->label, m->name, sizeof(m->name)) : m->stem;
   queries[0] = e->t1;
   queries[1] = label;
   queries[2] = m->stem;
   if (     gfx_thumbnail_match_find_first(m->index, queries, 3, &found)
            != GFX_THUMBNAIL_MATCH_FOUND
         || !found
         || pl_thumb_match_is_exact(m, found, label)
         || pl_thumb_match_is_exact(m, found, m->stem))
      return;
   if (     e->rec == PL_THUMB_MATCH_NONE
         && (e->rec = gfx_thumbnail_results_add(m->results,
               pl_thumb_match_cut(e->path, m->path, sizeof(m->path)), label,
               e->crc32, m->systems[e->system])) == PL_THUMB_MATCH_NONE)
      return;
   gfx_thumbnail_results_set(m->results, e->rec, m->cur_type, found);
}

static void pl_thumb_match_sort(pl_thumb_match_t *m)
{
   if (gfx_thumbnail_results_sort_step(m->results))
      return;
   m->pos    = 0;
   m->status = PL_THUMB_MATCH_SERIALIZE;
}

static void pl_thumb_match_raw(rjsonwriter_t *w, const char *s)
{
   rjsonwriter_raw(w, s, (int)strlen(s));
}

static void pl_thumb_match_member(rjsonwriter_t *w, const char *name,
      const char *value)
{
   pl_thumb_match_raw(w, ", ");
   rjsonwriter_add_string(w, name);
   pl_thumb_match_raw(w, ": ");
   rjsonwriter_add_string(w, value);
}

/* One record per call, into memory, so unchanged bytes are not written. */
static void pl_thumb_match_serialize(pl_thumb_match_t *m)
{
   unsigned i;
   int _len = 0;
   const char *key[4];
   const char *names[GFX_THUMBNAIL_RESULT_TYPES];

   if (!m->writer)
   {
      if (!(m->writer = rjsonwriter_open_memory()))
      {
         m->status = PL_THUMB_MATCH_DONE;
         return;
      }
      pl_thumb_match_raw(m->writer, "{\n  \"version\": ");
      rjsonwriter_add_string(m->writer, PL_THUMB_MATCH_VERSION);
      pl_thumb_match_raw(m->writer, ",\n  \"items\": [");
      return;
   }
   if (m->pos < gfx_thumbnail_results_count(m->results))
   {
      gfx_thumbnail_results_get(m->results, m->pos, key, names);
      pl_thumb_match_raw(m->writer, m->pos ? ",\n    {" : "\n    {");
      rjsonwriter_add_string(m->writer, pl_thumb_match_keys[0]);
      pl_thumb_match_raw(m->writer, ": ");
      rjsonwriter_add_string(m->writer, key[0]);
      for (i = 1; i < 4; i++)
         pl_thumb_match_member(m->writer, pl_thumb_match_keys[i], key[i]);
      for (i = 0; i < GFX_THUMBNAIL_RESULT_TYPES; i++)
         if (names[i])
            pl_thumb_match_member(m->writer,
                  gfx_thumbnail_result_type_dir(i), names[i]);
      pl_thumb_match_raw(m->writer, "}");
      m->pos++;
      return;
   }
   pl_thumb_match_raw(m->writer, "\n  ]\n}\n");
   if (!(m->out = rjsonwriter_get_memory_buffer(m->writer, &_len)))
   {
      m->status = PL_THUMB_MATCH_DONE;
      return;
   }
   m->buf_len = (size_t)_len;
   m->buf_pos = 0;
   m->status  = PL_THUMB_MATCH_SUM;
}

/* The new bytes' crc, a chunk per call */
static void pl_thumb_match_sum(pl_thumb_match_t *m)
{
   bool same;
   size_t want = m->buf_len - m->buf_pos;

   if (want > PL_THUMB_MATCH_CHUNK)
      want = PL_THUMB_MATCH_CHUNK;
   m->out_crc = encoding_crc32(m->out_crc,
         (const uint8_t*)m->out + m->buf_pos, want);
   if ((m->buf_pos += want) < m->buf_len)
      return;
   same = (m->flags & PL_THUMB_MATCH_FLAG_CACHE_FOUND)
      ? (m->out_crc == m->cache_crc)
      : !gfx_thumbnail_results_count(m->results);
   if (!same)
      m->flags |= PL_THUMB_MATCH_FLAG_CHANGED;
   /* Not for a playlist deleted meanwhile */
   if (same || !path_is_valid(m->playlist_config.path))
   {
      m->status = PL_THUMB_MATCH_DONE;
      return;
   }
   m->buf_pos = 0;
   m->status  = PL_THUMB_MATCH_WRITE;
}

/* Temporary file, a chunk per call, renamed into place. */
static void pl_thumb_match_write(pl_thumb_match_t *m)
{
   int ret;
   int64_t want;
   if (!m->file)
   {
      /* A read-only folder keeps the table in memory only */
      if (!(m->file = filestream_open_atomic(m->cache_path)))
         m->status = PL_THUMB_MATCH_DONE;
      return;
   }
   if (m->buf_pos < m->buf_len)
   {
      want = (int64_t)(m->buf_len - m->buf_pos);
      if (want > PL_THUMB_MATCH_CHUNK)
         want = PL_THUMB_MATCH_CHUNK;
      if (filestream_write(m->file, m->out + m->buf_pos, want) == want)
      {
         m->buf_pos += (size_t)want;
         return;
      }
      filestream_commit_atomic(m->file, m->cache_path, false);
      m->file   = NULL;
      m->status = PL_THUMB_MATCH_DONE;
      return;
   }
   /* Nor for one deleted while this was written */
   ret       = filestream_commit_atomic(m->file, m->cache_path,
         path_is_valid(m->playlist_config.path));
   m->file   = NULL;
   m->status = PL_THUMB_MATCH_DONE;
   if (ret > 0)
   {
      filestream_atomic_temp_name(m->path, sizeof(m->path), m->cache_path);
      filestream_delete(m->path);
   }
   /* or between that check and the rename */
   else if (!ret && !path_is_valid(m->playlist_config.path))
      filestream_delete(m->cache_path);
}

static void pl_thumb_match_step(pl_thumb_match_t *m, nbio_budget_t *b)
{
   switch (m->status)
   {
      case PL_THUMB_MATCH_CACHE_OPEN:
         pl_thumb_match_cache_open(m);
         break;
      case PL_THUMB_MATCH_CACHE_READ:
         pl_thumb_match_cache_read(m);
         break;
      case PL_THUMB_MATCH_CACHE_PARSE:
         pl_thumb_match_cache_parse(m);
         break;
      case PL_THUMB_MATCH_CACHE_SORT:
         pl_thumb_match_cache_sort(m);
         break;
      case PL_THUMB_MATCH_PLAYLIST:
         pl_thumb_match_playlist(m, b);
         break;
      case PL_THUMB_MATCH_KEYS:
         pl_thumb_match_key(m);
         break;
      case PL_THUMB_MATCH_LIST:
         pl_thumb_match_list(m);
         break;
      case PL_THUMB_MATCH_FINALIZE:
         pl_thumb_match_finalize(m);
         break;
      case PL_THUMB_MATCH_RESOLVE:
         pl_thumb_match_resolve(m);
         break;
      case PL_THUMB_MATCH_SORT:
         pl_thumb_match_sort(m);
         break;
      case PL_THUMB_MATCH_SERIALIZE:
         pl_thumb_match_serialize(m);
         break;
      case PL_THUMB_MATCH_SUM:
         pl_thumb_match_sum(m);
         break;
      case PL_THUMB_MATCH_WRITE:
         pl_thumb_match_write(m);
         break;
      default:
         break;
   }
}

static void pl_thumb_match_free(pl_thumb_match_t *m)
{
   size_t i;
   if (!m)
      return;
   pl_thumb_match_close(m);
   if (m->dir)
      retro_closedir(m->dir);
   if (m->json)
      rjson_free(m->json);
   if (m->writer)
      rjsonwriter_free(m->writer);
   pl_thumb_match_drop_playlist(m);
   if (m->index)
      gfx_thumbnail_match_index_free(m->index);
   for (i = 0; i < m->system_count; i++)
      free(m->systems[i]);
   free(m->systems);
   pl_thumb_match_cores_free(m->cores, m->core_count);
   free(m->buf);
   gfx_thumbnail_results_free(m->results);
   free(m);
}

static void task_pl_thumbnail_match_handler(retro_task_t *task)
{
   nbio_budget_t b;
   pl_thumb_match_t *m = (pl_thumb_match_t*)task->state;

   if (!m || (task_get_flags(task) & RETRO_TASK_FLG_CANCELLED))
   {
      if (m)
         pl_thumb_match_fail(m);
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
      return;
   }
   task_nbio_slice_open(&b);
   while (     m->status != PL_THUMB_MATCH_DONE
         && task_nbio_slice_within_budget(&b, 0, 0))
      pl_thumb_match_step(m, &b);
   task_nbio_slice_close(&b);
   if (m->status == PL_THUMB_MATCH_DONE)
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void pl_thumb_match_refresh(const char *playlist_path)
{
#if defined(RARCH_INTERNAL) && defined(HAVE_MENU)
   struct menu_state *menu_st = menu_state_get_ptr();
   playlist_t *current        = playlist_get_cached();
   const char *path           = current
      ? playlist_get_conf_path(current) : NULL;
   if (     path
         && string_is_equal(path, playlist_path)
         && menu_st->driver_ctx
         && menu_st->driver_ctx->refresh_thumbnail_image)
      menu_st->driver_ctx->refresh_thumbnail_image(menu_st->userdata,
            menu_st->selection_ptr);
#else
   (void)playlist_path;
#endif
}

static void cb_task_pl_thumbnail_match(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   gfx_thumbnail_results_t *results;
   bool changed;
   pl_thumb_match_t *m = task ? (pl_thumb_match_t*)task->state : NULL;

   if (!m || !(results = m->results))
      return;
   m->results = NULL;
   /* Before the load the menu found nothing here */
   changed    = (m->flags & PL_THUMB_MATCH_FLAG_RESOLVE)
      ? (m->flags & PL_THUMB_MATCH_FLAG_CHANGED) != 0
      : gfx_thumbnail_results_count(results) > 0;
   if (!gfx_thumbnail_results_install(m->playlist_config.path, results,
         (m->flags & PL_THUMB_MATCH_FLAG_RESOLVE) != 0))
      return;
   if (m->flags & PL_THUMB_MATCH_FLAG_THEN_RESOLVE)
   {
      /* The resolve takes the cores and the cache's crc over */
      pl_thumb_match_push(&m->playlist_config, m->dir_thumbnails,
            m->dir_database,
            (uint8_t)((m->flags & (PL_THUMB_MATCH_FLAG_ALLOW_NON_PNG
                                 | PL_THUMB_MATCH_FLAG_COLLECTION
                                 | PL_THUMB_MATCH_FLAG_CACHE_FOUND))
                    | PL_THUMB_MATCH_FLAG_RESOLVE
                    | PL_THUMB_MATCH_FLAG_SUMMED),
            m->cache_crc, m->cores, m->core_count);
      m->cores      = NULL;
      m->core_count = 0;
   }
   if (changed)
      pl_thumb_match_refresh(m->playlist_config.path);
   if (m->flags & PL_THUMB_MATCH_FLAG_CHANGED)
      ui_companion_driver_notify_refresh();
}

static void task_pl_thumbnail_match_cleanup(retro_task_t *task)
{
   if (task)
      pl_thumb_match_free((pl_thumb_match_t*)task->state);
}

static bool task_pl_thumbnail_match_finder(retro_task_t *task,
      void *user_data)
{
   pl_thumb_match_t *m;
   if (     !task || !user_data
         || task->handler != task_pl_thumbnail_match_handler
         || !(m = (pl_thumb_match_t*)task->state))
      return false;
   return string_is_equal((const char*)user_data, m->playlist_config.path);
}

/* Takes @cores over, also on failure. */
static bool pl_thumb_match_push(const playlist_config_t *config,
      const char *dir_thumbnails, const char *dir_database, uint8_t flags,
      uint32_t cache_crc, pl_thumb_match_core_t *cores, size_t core_count)
{
   size_t _len;
   retro_task_t *task  = task_init();
   pl_thumb_match_t *m = (pl_thumb_match_t*)calloc(1, sizeof(*m));

   if (     !task || !m
         || !playlist_config_copy(config, &m->playlist_config)
         || !(m->results = gfx_thumbnail_results_new()))
      goto error;
   /* An autofix write would race the main thread's playlist writes */
   m->playlist_config.autofix_paths = false;
   _len = strlcpy(m->cache_path, config->path, sizeof(m->cache_path));
   if (_len + STRLEN_CONST(FILE_PATH_THUMBS_EXTENSION) >= sizeof(m->cache_path))
      goto error;
   strlcpy(m->cache_path + _len, FILE_PATH_THUMBS_EXTENSION,
         sizeof(m->cache_path) - _len);
   strlcpy(m->dir_thumbnails, dir_thumbnails, sizeof(m->dir_thumbnails));
   strlcpy(m->dir_database, dir_database ? dir_database : "",
         sizeof(m->dir_database));
   m->item_rec    = PL_THUMB_MATCH_NONE;
   m->member      = -1;
   m->status      = (flags & PL_THUMB_MATCH_FLAG_SUMMED)
      ? PL_THUMB_MATCH_PLAYLIST : PL_THUMB_MATCH_CACHE_OPEN;
   m->flags       = flags;
   m->cache_crc   = cache_crc;
   m->cores       = cores;
   m->core_count  = core_count;

   task->handler  = task_pl_thumbnail_match_handler;
   task->state    = m;
   task->callback = cb_task_pl_thumbnail_match;
   task->cleanup  = task_pl_thumbnail_match_cleanup;
   task->flags   |= RETRO_TASK_FLG_MUTE;
   task_queue_push(task);
   return true;

error:
   free(task);
   pl_thumb_match_cores_free(cores, core_count);
   if (m)
   {
      gfx_thumbnail_results_free(m->results);
      free(m);
   }
   return false;
}

/* History and favourites: playlist_get_db_name falls back to core info
 * for entries without a db_name. Core info is main-thread state, so it
 * is read here, once per core; history is bounded. */
static size_t pl_thumb_match_cores(playlist_t *playlist,
      pl_thumb_match_core_t **out)
{
   size_t i, j;
   size_t count                 = 0;
   size_t size                  = playlist_size(playlist);
   pl_thumb_match_core_t *cores = NULL;

   for (i = 0; i < size; i++)
   {
      pl_thumb_match_core_t *grown;
      const char *core_path;
      const char *db_name                = NULL;
      const struct playlist_entry *entry = NULL;

      playlist_get_index(playlist, i, &entry);
      if (!entry || !string_is_empty(entry->db_name))
         continue;
      core_path = entry->core_path ? entry->core_path : "";
      for (j = 0; j < count; j++)
         if (string_is_equal(cores[j].core_path, core_path))
            break;
      if (j < count)
         continue;
      if (!(grown = (pl_thumb_match_core_t*)realloc(cores,
            (count + 1) * sizeof(*cores))))
         break;
      cores = grown;
      if (!(cores[count].core_path = strdup(core_path)))
         break;
      playlist_get_db_name(playlist, i, &db_name);
      cores[count].db_name = string_is_empty(db_name)
         ? NULL : strdup(db_name);
      count++;
   }
   *out = cores;
   return count;
}

bool task_push_pl_thumbnail_match(const playlist_config_t *playlist_config,
      playlist_t *playlist, const char *dir_thumbnails,
      const char *dir_database, bool allow_non_png)
{
   task_finder_data_t find_data;
   const char *name;
   unsigned need;
   bool collection;
   uint8_t flags                = 0;
   size_t core_count            = 0;
   pl_thumb_match_core_t *cores = NULL;

   if (     !playlist_config || !*playlist_config->path
         || !dir_thumbnails  || !*dir_thumbnails)
      return false;
   name       = path_basename_nocompression(playlist_config->path);
   collection =    !string_is_equal(name, FILE_PATH_CONTENT_HISTORY)
                && !string_is_equal(name, FILE_PATH_CONTENT_FAVORITES);
   /* Image, music and video history */
   if (collection && string_ends_with_size(name, "_history.lpl",
         strlen(name), STRLEN_CONST("_history.lpl")))
      return false;
   if (!(need = gfx_thumbnail_results_open(playlist_config->path)))
      return false;
   find_data.func     = task_pl_thumbnail_match_finder;
   find_data.userdata = (void*)playlist_config->path;
   if (task_queue_find(&find_data))
      return false;
   if (collection)
      flags |= PL_THUMB_MATCH_FLAG_COLLECTION;
   if (allow_non_png)
      flags |= PL_THUMB_MATCH_FLAG_ALLOW_NON_PNG;
   if (!(need & GFX_THUMBNAIL_RESULTS_LOAD))
      flags |= PL_THUMB_MATCH_FLAG_RESOLVE;
   else if (need & GFX_THUMBNAIL_RESULTS_RESOLVE)
      flags |= PL_THUMB_MATCH_FLAG_THEN_RESOLVE;
   if (     !collection && playlist
         && (flags & (PL_THUMB_MATCH_FLAG_RESOLVE
                    | PL_THUMB_MATCH_FLAG_THEN_RESOLVE)))
      core_count = pl_thumb_match_cores(playlist, &cores);
   return pl_thumb_match_push(playlist_config, dir_thumbnails,
         dir_database, flags, 0, cores, core_count);
}
