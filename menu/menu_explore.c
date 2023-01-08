/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2020 - Daniel De Matteis
 *  Copyright (C) 2020      - Psyraven
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

#include <stddef.h>

#include <compat/strcasestr.h>
#include <compat/strl.h>
#include <array/rbuf.h>
#include <array/rhmap.h>
#include <formats/rjson.h>
#include <formats/rjson_helpers.h>

#include "menu_driver.h"
#include "menu_cbs.h"
#include "../retroarch.h"
#include "../configuration.h"
#include "../file_path_special.h"
#include "../playlist.h"
#include "../verbosity.h"
#include "../libretro-db/libretrodb.h"
#include "../tasks/tasks_internal.h"

/* Explore */
enum
{
   EXPLORE_BY_DEVELOPER          = 0,
   EXPLORE_BY_PUBLISHER,
   EXPLORE_BY_RELEASEYEAR,
   EXPLORE_BY_PLAYERCOUNT,
   EXPLORE_BY_GENRE,
   EXPLORE_BY_ACHIEVEMENTS,
   EXPLORE_BY_CATEGORY,
   EXPLORE_BY_LANGUAGE,
   EXPLORE_BY_CONSOLE_EXCLUSIVE,
   EXPLORE_BY_PLATFORM_EXCLUSIVE,
   EXPLORE_BY_RUMBLE,
   EXPLORE_BY_SCORE,
   EXPLORE_BY_MEDIA,
   EXPLORE_BY_CONTROLS,
   EXPLORE_BY_ARTSTYLE,
   EXPLORE_BY_GAMEPLAY,
   EXPLORE_BY_NARRATIVE,
   EXPLORE_BY_PACING,
   EXPLORE_BY_PERSPECTIVE,
   EXPLORE_BY_SETTING,
   EXPLORE_BY_VISUAL,
   EXPLORE_BY_VEHICULAR,
   EXPLORE_BY_ORIGIN,
   EXPLORE_BY_REGION,
   EXPLORE_BY_FRANCHISE,
   EXPLORE_BY_TAGS,
   EXPLORE_BY_SYSTEM,
   EXPLORE_CAT_COUNT,

   EXPLORE_OP_EQUAL              = 0,
   EXPLORE_OP_MIN,
   EXPLORE_OP_MAX,
   EXPLORE_OP_RANGE,

   EXPLORE_ICONS_OFF             = 0,
   EXPLORE_ICONS_CONTENT         = 1,
   EXPLORE_ICONS_SYSTEM_CATEGORY = 2,

   EXPLORE_TYPE_ADDITIONALFILTER = FILE_TYPE_RDB, /* database icon */
   EXPLORE_TYPE_VIEW             = FILE_TYPE_PLAIN, /* file icon */
   EXPLORE_TYPE_FILTERNULL       = MENU_SETTINGS_LAST,
   EXPLORE_TYPE_SEARCH,
   EXPLORE_TYPE_SHOWALL,
   EXPLORE_TYPE_FIRSTCATEGORY,
   EXPLORE_TYPE_FIRSTITEM        = EXPLORE_TYPE_FIRSTCATEGORY + EXPLORE_CAT_COUNT
};

/* Arena allocator */
typedef struct ex_arena
{
   char *ptr;
   char *end;
   char **blocks;
} ex_arena;

typedef struct
{
   uint32_t idx;
   char str[1];
} explore_string_t;

typedef struct
{
   const struct playlist_entry* playlist_entry;
   explore_string_t *by[EXPLORE_CAT_COUNT];
   explore_string_t **split;
#ifdef EXPLORE_SHOW_ORIGINAL_TITLE
   char* original_title;
#endif
} explore_entry_t;

struct explore_state
{
   ex_arena arena;
   explore_string_t **by[EXPLORE_CAT_COUNT];
   explore_entry_t *entries;
   playlist_t **playlists;
   uintptr_t *icons;
   const char *label_explore_item_str;

   char title[1024];
   bool has_unknown[EXPLORE_CAT_COUNT];
   unsigned show_icons;

   unsigned          view_levels;
   char              view_search[1024];
   uint8_t           view_op[EXPLORE_CAT_COUNT];
   bool              view_use_split[EXPLORE_CAT_COUNT];
   unsigned          view_cats[EXPLORE_CAT_COUNT];
   explore_string_t* view_match[EXPLORE_CAT_COUNT];
   uint32_t          view_idx_min[EXPLORE_CAT_COUNT];
   uint32_t          view_idx_max[EXPLORE_CAT_COUNT];
};

static const struct
{
   const char* rdbkey;
   enum msg_hash_enums name_enum, by_enum;
   bool use_split, is_company, is_numeric, is_boolean;
}
explore_by_info[EXPLORE_CAT_COUNT] =
{
   { "developer",          MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,           MENU_ENUM_LABEL_VALUE_EXPLORE_BY_DEVELOPER,          true,  true,  false, false },
   { "publisher",          MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,           MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PUBLISHER,          true,  true,  false, false },
   { "releaseyear",        MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_RELEASE_YEAR, MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RELEASE_YEAR,       false, false, true,  false },
   { "users",              MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_PLAYER_COUNT, MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLAYER_COUNT,       false, false, true,  false },
   { "genre",              MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,               MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GENRE,              true,  false, false, false },
   { "achievements",       MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ACHIEVEMENTS,        MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ACHIEVEMENTS,       false, false, false, true  },
   { "category",           MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CATEGORY,            MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CATEGORY,           true,  false, false, false },
   { "language",           MENU_ENUM_LABEL_VALUE_RDB_ENTRY_LANGUAGE,            MENU_ENUM_LABEL_VALUE_EXPLORE_BY_LANGUAGE,           true,  false, false, false },
   { "console_exclusive",  MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONSOLE_EXCLUSIVE,   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONSOLE_EXCLUSIVE,  false, false, false, true  },
   { "platform_exclusive", MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PLATFORM_EXCLUSIVE,  MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLATFORM_EXCLUSIVE, false, false, false, true  },
   { "rumble",             MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,              MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RUMBLE,             false, false, false, true  },
   { "score",              MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SCORE,               MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SCORE,              true,  false, false, false },
   { "media",              MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MEDIA,               MENU_ENUM_LABEL_VALUE_EXPLORE_BY_MEDIA,              true,  false, false, false },
   { "controls",           MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONTROLS,            MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONTROLS,           true,  false, false, false },
   { "artstyle",           MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ARTSTYLE,            MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ARTSTYLE,           true,  false, false, false },
   { "gameplay",           MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GAMEPLAY,            MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GAMEPLAY,           true,  false, false, false },
   { "narrative",          MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NARRATIVE,           MENU_ENUM_LABEL_VALUE_EXPLORE_BY_NARRATIVE,          true,  false, false, false },
   { "pacing",             MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PACING,              MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PACING,             true,  false, false, false },
   { "perspective",        MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PERSPECTIVE,         MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PERSPECTIVE,        true,  false, false, false },
   { "setting",            MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SETTING,             MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SETTING,            true,  false, false, false },
   { "visual",             MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VISUAL,              MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VISUAL,             true,  false, false, false },
   { "vehicular",          MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VEHICULAR,           MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VEHICULAR,          true,  false, false, false },
   { "origin",             MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,              MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ORIGIN,             false, false, false, false },
   { "region",             MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_REGION,       MENU_ENUM_LABEL_VALUE_EXPLORE_BY_REGION,             false, false, false, false },
   { "franchise",          MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,           MENU_ENUM_LABEL_VALUE_EXPLORE_BY_FRANCHISE,          false, false, false, false },
   { "tags",               MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_TAG,          MENU_ENUM_LABEL_VALUE_EXPLORE_BY_TAG,                true,  false, false, false },
   { "system",             MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,         MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SYSTEM_NAME,        false, false, false, false },
};

/* TODO/FIXME - static global */
static explore_state_t* explore_state;

#if defined(_MSC_VER)
#define EX_ALIGNOF(type) ((int)__alignof(type))
#else
#define EX_ALIGNOF(type) ((int)__alignof__(type))
#endif

#define EX_ARENA_ALIGNMENT 8
#define EX_ARENA_BLOCK_SIZE (64 * 1024)
#define EX_ARENA_ALIGN_UP(n, a) (((n) + (a) - 1) & ~((a) - 1))

static void ex_arena_grow(ex_arena *arena, size_t min_size)
{
   size_t size = EX_ARENA_ALIGN_UP(
         MAX(min_size, EX_ARENA_BLOCK_SIZE), EX_ARENA_ALIGNMENT);
   arena->ptr  = (char *)malloc(size);
   arena->end  = arena->ptr + size;
   RBUF_PUSH(arena->blocks, arena->ptr);
}

static void *ex_arena_alloc(ex_arena *arena, size_t size)
{
   void *ptr  = NULL;

   if (size > (size_t)(arena->end - arena->ptr))
      ex_arena_grow(arena, size);

   ptr        = arena->ptr;
   arena->ptr = (char *)
      EX_ARENA_ALIGN_UP((uintptr_t)(arena->ptr + size), EX_ARENA_ALIGNMENT);
   return ptr;
}

static void ex_arena_free(ex_arena *arena)
{
   char **it;

   for (it = arena->blocks; it != RBUF_END(arena->blocks); it++)
      free(*it);

   RBUF_FREE(arena->blocks);
   arena->ptr    = NULL;
   arena->end    = NULL;
   arena->blocks = NULL;
}

/* Hash function */
static uint32_t ex_hash32_nocase_filtered(
      const unsigned char* str, size_t len,
      unsigned char f_first, unsigned char f_last)
{
   const unsigned char *end = NULL;
   uint32_t hash            = (uint32_t)0x811c9dc5;
   for (end = str + len; str != end;)
   {
      unsigned char c = *(str++);
      if (c >= f_first && c <= f_last)
         hash = ((hash * (uint32_t)0x01000193) 
               ^ (uint32_t)((c >= 'A' && c <= 'Z') 
                  ? (c | 0x20) : c));
   }
   if (hash)
      return hash;
   return 1;
}

static int explore_qsort_func_nums(const void *a_, const void *b_)
{
   const char *a = (*(const explore_string_t**)a_)->str;
   const char *b = (*(const explore_string_t**)b_)->str;
   return atoi(a) - atoi(b);
}

static int explore_qsort_func_strings(const void *a_, const void *b_)
{
   const char *a = (*(const explore_string_t**)a_)->str;
   const char *b = (*(const explore_string_t**)b_)->str;
   int a0 = TOLOWER(a[0]), b0 = TOLOWER(b[0]);
   return (a0 != b0 ? (a0 - b0) : strcasecmp(a, b));
}

static int explore_qsort_func_entries(const void *a_, const void *b_)
{
   const char *a = ((const explore_entry_t*)a_)->playlist_entry->label;
   const char *b = ((const explore_entry_t*)b_)->playlist_entry->label;
   int a0 = TOLOWER(a[0]), b0 = TOLOWER(b[0]);
   if (a0 != b0) return a0 - b0;
   return strcasecmp(a, b);
}

static int explore_qsort_func_menulist(const void *a_, const void *b_)
{
   const struct item_file *a = (const struct item_file*)a_;
   const struct item_file *b = (const struct item_file*)b_;
   if (a->path[0] != b->path[0])
      return (unsigned char)a->path[0] - (unsigned char)b->path[0];
   return strcasecmp(a->path, b->path);
}

static int explore_check_company_suffix(const char* p, bool search_reverse)
{
   int p0, p0_lc, p1, p1_lc, p2, p2_lc;
   if (search_reverse)
   {
      p -= (p[-1] == '.' ? 4 : 3);
      if (p[-1] != ' ')
         return 0;
   }
   if (p[0] == '\0' || p[1] == '\0' || p[2] == '\0')
      return 0;
   p0     = p[0];
   p1     = p[1];
   p2     = p[2];
   p0_lc  = TOLOWER(p0);
   p1_lc  = TOLOWER(p1);
   p2_lc  = TOLOWER(p2);
   if (   (p0_lc == 'i' && p1_lc == 'n' && p2_lc == 'c') /*, Inc */
       || (p0_lc == 'l' && p1_lc == 't' && p2_lc == 'd') /*, Ltd */
       || (p0_lc == 't' && p1_lc == 'h' && p2_lc == 'e') /*, The */
         )
      return (p[3] == '.' ? 4 : 3);
   return 0;
}

static void explore_add_unique_string(
      explore_state_t *state,
      explore_string_t** maps[EXPLORE_CAT_COUNT], explore_entry_t *e,
      unsigned cat, const char *str,
      explore_string_t ***split_buf)
{
   bool is_company;
   const char *p;
   const char *p_next;
   if (!str || !*str)
   {
      state->has_unknown[cat] = true;
      return;
   }

   if (!explore_by_info[cat].use_split)
      split_buf = NULL;
   is_company   = explore_by_info[cat].is_company;

   for (p = str + 1;; p++)
   {
      size_t len              = 0;
      uint32_t hash           = 0;
      explore_string_t* entry = NULL;

      if (*p != '/' && *p != ',' && *p != '|' && *p != '\0')
         continue;

      if (!split_buf && *p != '\0')
         continue;

      p_next = p;
      while (*str == ' ')
         str++;
      while (p[-1] == ' ')
         p--;

      if (p == str)
      {
         if (*p == '\0')
            return;
         continue;
      }

      if (is_company && p - str > 5)
      {
         p -= explore_check_company_suffix(p, true);
         while (p[-1] == ' ')
            p--;
      }

      len                     = p - str;
      hash                    = ex_hash32_nocase_filtered(
            (unsigned char*)str, len, '0', 255);
      entry                   = RHMAP_GET(maps[cat], hash);

      if (!entry)
      {
         entry                = (explore_string_t*)
            ex_arena_alloc(&state->arena,
                  sizeof(explore_string_t) + len);
         memcpy(entry->str, str, len);
         entry->str[len]      = '\0';
         RBUF_PUSH(state->by[cat], entry);
         RHMAP_SET(maps[cat], hash, entry);
      }

      if (!e->by[cat])
         e->by[cat] = entry;
      else
         RBUF_PUSH(*split_buf, entry);

      if (*p_next == '\0')
         return;
      if (is_company && *p_next == ',')
      {
         p = p_next + 1;
         while (*p == ' ')
            p++;
         p += explore_check_company_suffix(p, false);
         while (*p == ' ')
            p++;
         if (*p == '\0')
            return;
         if (*p == '/' || *p == ',' || *p == '|')
            p_next = p;
      }
      p = p_next;
      str = p + 1;
   }
}

static void explore_unload_icons(explore_state_t *state)
{
   unsigned i;
   if (!state)
      return;
   for (i = 0; i != RBUF_LEN(state->icons); i++)
      if (state->icons[i])
         video_driver_texture_unload(&state->icons[i]);
}

static void explore_load_icons(explore_state_t *state)
{
   char path[PATH_MAX_LENGTH];
   size_t i, pathlen, system_count;
   if (!state)
      return;

   if ((system_count = RBUF_LEN(state->by[EXPLORE_BY_SYSTEM])) <= 0)
      return;

   /* unload any icons that could exist from a previous call to this */
   explore_unload_icons(state);

   /* RBUF_RESIZE leaves memory uninitialised, 
      have to zero it 'manually' */
   RBUF_RESIZE(state->icons, system_count);
   memset(state->icons, 0, RBUF_SIZEOF(state->icons));

   fill_pathname_application_special(path, sizeof(path),
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_SYSICONS);
   if (string_is_empty(path))
      return;

   fill_pathname_slash(path, sizeof(path));
   pathlen = strlen(path);

   for (i = 0; i != system_count; i++)
   {
      struct texture_image ti;

      strlcpy(path + pathlen,
            state->by[EXPLORE_BY_SYSTEM][i]->str, sizeof(path) - pathlen);
      strlcat(path, ".png", sizeof(path));
      if (!path_is_valid(path))
         continue;

      ti.width         = 0;
      ti.height        = 0;
      ti.pixels        = NULL;
      ti.supports_rgba = video_driver_supports_rgba();

      if (!image_texture_load(&ti, path))
         continue;

      if (ti.pixels)
         video_driver_texture_load(&ti,
               TEXTURE_FILTER_MIPMAP_LINEAR, &state->icons[i]);

      image_texture_free(&ti);
   }
}

explore_state_t *menu_explore_build_list(const char *directory_playlist,
      const char *directory_database)
{
   unsigned i;
   char tmp[PATH_MAX_LENGTH];
   struct explore_source
   {
      const struct playlist_entry *source;
      uint32_t entry_index, meta_count;
   };
   struct explore_rdb
   {
      libretrodb_t *handle;
      struct explore_source *playlist_crcs;
      struct explore_source *playlist_names;
      size_t count;
      char systemname[256];
   }
   *rdbs                                          = NULL;
   int *rdb_indices                               = NULL;
   explore_string_t **cat_maps[EXPLORE_CAT_COUNT] = {NULL};
   explore_string_t **split_buf                   = NULL;
   libretro_vfs_implementation_dir *dir           = NULL;

   explore_state_t *state = (explore_state_t*)calloc(1, sizeof(*state));

   if (!state)
      return NULL;

   state->label_explore_item_str    = 
      msg_hash_to_str(MENU_ENUM_LABEL_EXPLORE_ITEM);

   /* Index all playlists */
   for (dir = retro_vfs_opendir_impl(directory_playlist, false); dir;)
   {
      playlist_config_t playlist_config;
      size_t j, used_entries                    = 0;
      playlist_t *playlist                      = NULL;
      const char *fext                          = NULL;
      const char *fname                         = NULL;
      uint32_t fhash                            = 0;

      playlist_config.path[0]                   = '\0';
      playlist_config.base_content_directory[0] = '\0';
      playlist_config.capacity                  = 0;
      playlist_config.old_format                = false;
      playlist_config.compress                  = false;
      playlist_config.fuzzy_archive_match       = false;
      playlist_config.autofix_paths             = false;

      if (!retro_vfs_readdir_impl(dir))
      {
         retro_vfs_closedir_impl(dir);
         break;
      }

      fname                                     = retro_vfs_dirent_get_name_impl(dir);
      if (fname)
         fext                           = strrchr(fname, '.');

      if (!fext || strcasecmp(fext, ".lpl"))
         continue;

      fill_pathname_join_special(playlist_config.path,
            directory_playlist, fname, sizeof(playlist_config.path));
      playlist_config.capacity          = COLLECTION_SIZE;
      playlist                          = playlist_init(&playlist_config);

      fhash = ex_hash32_nocase_filtered(
            (unsigned char*)fname, fext - fname, '0', 255);

      for (j = 0; j < playlist_size(playlist); j++)
      {
         int rdb_num;
         uint32_t entry_crc32;
         struct explore_source src = { NULL, (uint32_t)-1, 0 };
         struct explore_rdb* rdb             = NULL;
         const struct playlist_entry *entry  = NULL;
         const char *db_name                 = fname;
         const char *db_ext                  = fext;
         uint32_t rdb_hash                   = fhash;
         playlist_get_index(playlist, j, &entry);

         /* We also could build label from file name, for now it's required */
         if (!entry->label || !*entry->label)
            continue;

         /* For auto scanned playlists the entry db_name matches the
          * lpl file name and we can just use that */
         if (entry->db_name && *entry->db_name
               && strcasecmp(entry->db_name, fname))
         {
            db_name = entry->db_name;
            db_ext = strrchr(db_name, '.');
            if (!db_ext)
               db_ext = db_name + strlen(db_name);
            rdb_hash = ex_hash32_nocase_filtered(
               (unsigned char*)db_name, db_ext - db_name, '0', 255);
         }

         rdb_num = RHMAP_GET(rdb_indices, rdb_hash);
         if (!rdb_num)
         {
            size_t systemname_len;
            struct explore_rdb newrdb;
            char *ext_path        = NULL;

            newrdb.handle         = libretrodb_new();
            newrdb.count          = 0;
            newrdb.playlist_crcs  = NULL;
            newrdb.playlist_names = NULL;

            systemname_len        = db_ext - db_name;
            if (systemname_len >= sizeof(newrdb.systemname))
               systemname_len = sizeof(newrdb.systemname)-1;
            memcpy(newrdb.systemname, db_name, systemname_len);
            newrdb.systemname[systemname_len] = '\0';

            fill_pathname_join_special(
                  tmp, directory_database, db_name, sizeof(tmp));

            /* Replace the extension - change 'lpl' to 'rdb' */
            if ((    ext_path = path_get_extension_mutable(tmp)) 
                  && ext_path[0] == '.'
                  && ext_path[1] == 'l'
                  && ext_path[2] == 'p'
                  && ext_path[3] == 'l')
            {
               ext_path[1] = 'r';
               ext_path[2] = 'd';
               ext_path[3] = 'b';
            }

            if (libretrodb_open(tmp, newrdb.handle) != 0)
            {
               /* Invalid RDB file */
               libretrodb_free(newrdb.handle);
               RHMAP_SET(rdb_indices, rdb_hash, -1);
               continue;
            }

            RBUF_PUSH(rdbs, newrdb);
            rdb_num = (uintptr_t)RBUF_LEN(rdbs);
            RHMAP_SET(rdb_indices, rdb_hash, rdb_num);
         }

         if (rdb_num == (uintptr_t)-1)
            continue;

         rdb = &rdbs[rdb_num - 1];
         rdb->count++;
         entry_crc32 = (uint32_t)strtoul(
               (entry->crc32 ? entry->crc32 : ""), NULL, 16);
         src.source = entry;
         if (entry_crc32)
         {
            RHMAP_SET(rdb->playlist_crcs, entry_crc32, src);
         }
         else
         {
            RHMAP_SET_STR(rdb->playlist_names, entry->label, src);
         }
         used_entries++;
      }

      if (used_entries)
         RBUF_PUSH(state->playlists, playlist);
      else
         playlist_free(playlist);
   }

   /* Loop through all RDBs referenced in the playlists 
    * and load meta data strings */
   for (i = 0; i != RBUF_LEN(rdbs); i++)
   {
      struct rmsgpack_dom_value item;
      struct explore_rdb* rdb  = &rdbs[i];
      libretrodb_cursor_t *cur = libretrodb_cursor_new();
      bool more                = 
         (
          libretrodb_cursor_open(rdb->handle, cur, NULL) == 0
          && libretrodb_cursor_read_item(cur, &item) == 0);

      for (; more; more = (rmsgpack_dom_value_free(&item),
               libretrodb_cursor_read_item(cur, &item) == 0))
      {
         unsigned k, l, cat;
         explore_entry_t* e;
         const char *fields[EXPLORE_CAT_COUNT];
         char numeric_buf[EXPLORE_CAT_COUNT][16];
         uint32_t crc32                     = 0;
         uint32_t meta_count                = 0;
         char *name                         = NULL;
#ifdef EXPLORE_SHOW_ORIGINAL_TITLE
         char *original_title               = NULL;
#endif
         struct explore_source* src         = NULL;

         if (item.type != RDT_MAP)
            continue;

         for (k = 0; k < EXPLORE_CAT_COUNT; k++)
            fields[k]                       = NULL;

         for (k = 0; k < item.val.map.len; k++)
         {
            const char *key_str             = NULL;
            struct rmsgpack_dom_value *key  = &item.val.map.items[k].key;
            struct rmsgpack_dom_value *val  = &item.val.map.items[k].value;
            if (!key || !val || key->type != RDT_STRING)
               continue;

            key_str                         = key->val.string.buff;
            if (string_is_equal(key_str, "crc"))
            {
               switch (val->val.binary.len)
               {
                  case 1:
                     crc32 = *(uint8_t*)val->val.binary.buff;
                     break;
                  case 2:
                     crc32 = swap_if_little16(*(uint16_t*)val->val.binary.buff);
                     break;
                  case 4:
                     crc32 = swap_if_little32(*(uint32_t*)val->val.binary.buff);
                     break;
                  default:
                     crc32 = 0;
                     break;
               }

               continue;
            }
            else if (string_is_equal(key_str, "name"))
            {
               name = val->val.string.buff;
               continue;
            }
#ifdef EXPLORE_SHOW_ORIGINAL_TITLE
            else if (string_is_equal(key_str, "original_title"))
            {
               original_title = val->val.string.buff;
               continue;
            }
#endif

            for (cat = 0; cat != EXPLORE_CAT_COUNT; cat++)
            {
               if (!string_is_equal(key_str, explore_by_info[cat].rdbkey))
                  continue;

               meta_count++;
               if (explore_by_info[cat].is_numeric)
               {
                  if (val->type >= RDT_STRING)
                     break;
                  snprintf(numeric_buf[cat],
                        sizeof(numeric_buf[cat]),
                        "%d", (int)val->val.int_);
                  fields[cat] = numeric_buf[cat];
                  break;
               }
               if (explore_by_info[cat].is_boolean)
               {
                  if (val->type >= RDT_STRING)
                     break;
                  fields[cat] = msg_hash_to_str(val->val.int_ ?
                        MENU_ENUM_LABEL_VALUE_YES : MENU_ENUM_LABEL_VALUE_NO);
                  break;
               }
               if (val->type != RDT_STRING)
                  break;
               fields[cat] = val->val.string.buff;
               break;
            }
         }

         if (crc32)
         {
            ptrdiff_t idx = RHMAP_IDX(rdb->playlist_crcs, crc32);
            src = (idx != -1 ? &rdb->playlist_crcs[idx] : NULL);
         }
         if (!src && name)
         {
            ptrdiff_t idx = RHMAP_IDX_STR(rdb->playlist_names, name);
            src = (idx != -1 ? &rdb->playlist_names[idx] : NULL);
         }
         if (!src)
            continue;
         if (src->entry_index != (uint32_t)-1 && src->meta_count >= meta_count)
            continue;

         if (src->entry_index == (uint32_t)-1)
         {
            src->entry_index = (uint32_t)RBUF_LEN(state->entries);
            RBUF_RESIZE(state->entries, src->entry_index + 1);
         }
         e = &state->entries[src->entry_index];
         src->meta_count = meta_count;
         e->playlist_entry = src->source;
         for (l = 0; l < EXPLORE_CAT_COUNT; l++)
            e->by[l]       = NULL;
         e->split          = NULL;
#ifdef EXPLORE_SHOW_ORIGINAL_TITLE
         e->original_title = NULL;
#endif

         fields[EXPLORE_BY_SYSTEM] = rdb->systemname;

         for (cat = 0; cat != EXPLORE_CAT_COUNT; cat++)
         {
            explore_add_unique_string(state,
                  cat_maps, e, cat,
                  fields[cat], &split_buf);
         }

#ifdef EXPLORE_SHOW_ORIGINAL_TITLE
         if (original_title && *original_title)
         {
            size_t len        = strlen(original_title) + 1;
            e->original_title = (char*)
               ex_arena_alloc(&state->arena, len);
            memcpy(e->original_title, original_title, len);
         }
#endif

         if (RBUF_LEN(split_buf))
         {
            size_t len;

            RBUF_PUSH(split_buf, NULL); /* terminator */
            len        = RBUF_SIZEOF(split_buf);
            e->split   = (explore_string_t **)
               ex_arena_alloc(&state->arena, len);
            memcpy(e->split, split_buf, len);
            RBUF_CLEAR(split_buf);
         }

         /* if all entries have found connections, we can leave early */
         if (--rdb->count == 0)
         {
            rmsgpack_dom_value_free(&item);
            break;
         }
      }

      libretrodb_cursor_close(cur);
      libretrodb_cursor_free(cur);
      libretrodb_close(rdb->handle);
      libretrodb_free(rdb->handle);
      RHMAP_FREE(rdb->playlist_crcs);
      RHMAP_FREE(rdb->playlist_names);
   }
   RBUF_FREE(split_buf);
   RHMAP_FREE(rdb_indices);
   RBUF_FREE(rdbs);

   for (i = 0; i != EXPLORE_CAT_COUNT; i++)
   {
      uint32_t idx;
      size_t len = RBUF_LEN(state->by[i]);

      if (state->by[i])
         qsort(state->by[i], len, sizeof(*state->by[i]),
               (explore_by_info[i].is_numeric ?
                  explore_qsort_func_nums : explore_qsort_func_strings));

      for (idx = 0; idx != len; idx++)
         state->by[i][idx]->idx = idx;

      RHMAP_FREE(cat_maps[i]);
   }
   /* NULL is not a valid value as a first argument for qsort */
   if (state->entries)
      qsort(state->entries,
         RBUF_LEN(state->entries),
         sizeof(*state->entries), explore_qsort_func_entries);
   return state;
}

static int explore_action_get_title(
      const char *path, const char *label,
      unsigned menu_type, char *s, size_t len)
{
   strlcpy(s, (explore_state ? explore_state->title : 
      msg_hash_to_str(MENU_ENUM_LABEL_VALUE_EXPLORE_TAB)), len);
   return 0;
}

static void explore_append_title(explore_state_t *state,
      const char* fmt, ...)
{
   va_list ap;
   size_t len = strlen(state->title);
   va_start(ap, fmt);
   vsnprintf(state->title + len,
         sizeof(state->title) - len, fmt, ap);
   va_end(ap);
}

static int explore_action_sublabel_spacer(
      file_list_t *list, unsigned type, unsigned i,
      const char *label, const char *path, char *s, size_t len)
{
   const char *menu_driver = menu_driver_ident();

   /* Only add a blank 'spacer' sublabel when
    * using Ozone
    * > In XMB/GLUI it upsets the vertical layout
    * > In RGUI it does nothing other than
    *   unnecessarily blank out the fallback
    *   core title text in the sublabel area */
   if (string_is_equal(menu_driver, "ozone"))
   {
      s[0] = ' ';
      s[1] = '\0';
   }

   return 1; /* 1 means it'll never change and can be cached */
}

static int explore_action_ok(const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   const char* explore_tab = msg_hash_to_str(MENU_ENUM_LABEL_EXPLORE_TAB);
   if (type >= EXPLORE_TYPE_FIRSTITEM || type == EXPLORE_TYPE_FILTERNULL)
   {
      file_list_t *menu_stack = menu_entries_get_menu_stack_ptr(0);
      unsigned prev_type      = menu_stack->list[menu_stack->size - 1].type;
      unsigned cat            = (prev_type - EXPLORE_TYPE_FIRSTCATEGORY);
      if (cat < EXPLORE_CAT_COUNT)
      {
         explore_state_t *state = explore_state;
         unsigned lvl, lvl_max = state->view_levels;
         for (lvl = 0; lvl != lvl_max; lvl++)
            if (state->view_cats[lvl] == cat)
               break;
         if (lvl == lvl_max)
         {
            /* new matching filter */
            state->view_op[lvl] = EXPLORE_OP_EQUAL;
            state->view_use_split[lvl] = explore_by_info[cat].use_split;
            state->view_cats[lvl] = cat;
            state->view_match[lvl] = (type == EXPLORE_TYPE_FILTERNULL ? 
                  NULL : state->by[cat][type-EXPLORE_TYPE_FIRSTITEM]);
            state->view_levels++;
         }
         else
         {
            /* switch from match to filter range */
            explore_string_t* r1 = state->view_match[lvl];
            explore_string_t* r2 = state->by[cat][type-EXPLORE_TYPE_FIRSTITEM];
            state->view_op[lvl] = EXPLORE_OP_RANGE;
            state->view_idx_min[lvl] = (r1->idx < r2->idx ? r1->idx : r2->idx);
            state->view_idx_max[lvl] = (r1->idx < r2->idx ? r2->idx : r1->idx);
         }
      }
   }
   filebrowser_clear_type();
   return generic_action_ok_displaylist_push(explore_tab,
         NULL, explore_tab, type, idx, entry_idx, ACTION_OK_DL_PUSH_DEFAULT);
}

static int explore_cancel(const char *path,
      const char *label, unsigned type, size_t idx)
{
   file_list_t *menu_stack = menu_entries_get_menu_stack_ptr(0);
   unsigned closed_type = menu_stack->list[menu_stack->size - 1].type;
   if (closed_type >= EXPLORE_TYPE_FIRSTITEM ||
       closed_type == EXPLORE_TYPE_FILTERNULL)
   {
      /* popping one filter, check if popping filter range or match */
      unsigned prev_type = menu_stack->list[menu_stack->size - 2].type;
      unsigned cat       = (prev_type - EXPLORE_TYPE_FIRSTCATEGORY);
      unsigned lvl, lvl_max = explore_state->view_levels;
      for (lvl = 0; lvl != lvl_max; lvl++)
      {
         if (explore_state->view_cats[lvl] != cat) continue;
         if (explore_state->view_op[lvl] == EXPLORE_OP_EQUAL)
            explore_state->view_levels--;
         else
         {
            explore_state->view_op[lvl] = EXPLORE_OP_EQUAL;
            if (!explore_state->view_match[lvl])
               explore_state->view_match[lvl] =
                     explore_state->by[cat][explore_state->view_idx_min[lvl]];
         }
         break;
      }
   }
   else if (closed_type == EXPLORE_TYPE_SEARCH)
      explore_state->view_search[0] = '\0';
   return action_cancel_pop_default(path, label, type, idx);
}

void explore_menu_entry(file_list_t *list, explore_state_t *state,
      const char *path, unsigned type, int (*action_ok)(const char *path,
            const char *label, unsigned type, size_t idx, size_t entry_idx))
{
   menu_file_list_cbs_t *cbs;
   if (!menu_entries_append(list, path, state->label_explore_item_str,
         MENU_ENUM_LABEL_EXPLORE_ITEM, type, 0, 0, NULL))
      return;
   cbs = ((menu_file_list_cbs_t*)list->list[list->size-1].actiondata);
   if (!cbs)
      return;
   cbs->action_ok = action_ok;
   cbs->action_cancel = explore_cancel;
}

static void explore_menu_add_spacer(file_list_t *list)
{
   if (list->size)
      ((menu_file_list_cbs_t*)list->list[list->size-1].actiondata)->action_sublabel = explore_action_sublabel_spacer;
}

static void explore_action_find_complete(void *userdata, const char *line)
{
   menu_input_dialog_end();
   if (line && *line)
   {
      strlcpy(explore_state->view_search, line,
            sizeof(explore_state->view_search));
      explore_action_ok(NULL, NULL, EXPLORE_TYPE_SEARCH, 0, 0);
   }
}

static int explore_action_ok_find(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_input_ctx_line_t line;
   line.label                 = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SEARCH);
   line.label_setting         = NULL;
   line.type                  = 0;
   line.idx                   = 0;
   line.cb                    = explore_action_find_complete;
   menu_input_dialog_start(&line);
   return 0;
}

static const char* explore_get_view_path(void)
{
   file_list_t *menu_stack = menu_entries_get_menu_stack_ptr(0);
   struct item_file *cur = (struct item_file *)&menu_stack->list[menu_stack->size - 1];

   /* check if we are opening a saved view from the horizontal/tabs menu */
   if (cur->type == MENU_SETTING_HORIZONTAL_MENU)
   {
      menu_ctx_list_t tabs, horizontal;
      tabs.type = MENU_LIST_TABS;
      if (menu_driver_list_get_selection(&tabs) && menu_driver_list_get_size(&tabs))
      {
         horizontal.type = MENU_LIST_HORIZONTAL;
         horizontal.idx = tabs.selection - (tabs.size + 1);
         if (menu_driver_list_get_entry(&horizontal))
         {
            /* label contains the path and path contains the label */
            return ((struct item_file*)horizontal.entry)->label;
         }
      }
   }

   /* check if we are opening a saved view via Content > Playlists */
   if (cur->type == MENU_EXPLORE_TAB && cur->path && !string_is_equal(cur->path,
               msg_hash_to_str(MENU_ENUM_LABEL_GOTO_EXPLORE)))
   {
      return cur->path;
   }

   return NULL;
}

static void explore_on_edit_views(enum msg_hash_enums msg)
{
   menu_ctx_environment_t menu_environ;
   menu_environ.type = MENU_ENVIRON_NONE;
   menu_environ.data = NULL;
   menu_environ.type = MENU_ENVIRON_RESET_HORIZONTAL_LIST;
   menu_driver_ctl(RARCH_MENU_CTL_ENVIRONMENT, &menu_environ);

   runloop_msg_queue_push(msg_hash_to_str(msg),
         1, 180, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}

static int explore_action_ok_deleteview(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   filestream_delete(explore_get_view_path());
   explore_on_edit_views(MENU_ENUM_LABEL_EXPLORE_VIEW_DELETED);

   if (menu_entries_get_menu_stack_ptr(0)->size == 1)
   {
      /* if we're at the top of the menu we can't cancel so just refresh
         what becomes selected after MENU_ENVIRON_RESET_HORIZONTAL_LIST. */
      bool refresh_nonblocking = false;
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh_nonblocking);
   }

   return explore_cancel(path, label, type, idx);
}

static void explore_action_saveview_complete(void *userdata, const char *name)
{
   int count = 0, op;
   char lvwpath[PATH_MAX_LENGTH];
   intfstream_t *file;
   rjsonwriter_t* w;
   explore_state_t *state = explore_state;

   menu_input_dialog_end();
   if (!name || !*name) return;

   fill_pathname_join_special(lvwpath,
         config_get_ptr()->paths.directory_playlist, name, sizeof(lvwpath));
   strlcat(lvwpath, ".lvw", sizeof(lvwpath));

   if (filestream_exists(lvwpath))
   {
      runloop_msg_queue_push(msg_hash_to_str(MENU_ENUM_LABEL_EXPLORE_VIEW_EXISTS),
            1, 360, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
      return;
   }

   file = intfstream_open_file(lvwpath,
         RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
   {
      RARCH_ERR("[explore view] Failed to write json file %s.\n", lvwpath);
      return;
   }

   w = rjsonwriter_open_stream(file);

   rjsonwriter_add_start_object(w);

   if (state->view_search[0])
   {
      rjsonwriter_add_newline(w);
      rjsonwriter_add_tabs(w, 1);
      rjsonwriter_add_string(w, "filter_name");
      rjsonwriter_add_colon(w);
      rjsonwriter_add_space(w);
      rjsonwriter_add_string(w, state->view_search);
      count++;
   }

   for (op = EXPLORE_OP_EQUAL; op <= EXPLORE_OP_MAX; op++)
   {
      unsigned i, n;
      for (i = n = 0; i != state->view_levels; i++)
      {
         uint8_t vop = state->view_op[i];
         unsigned vcat = state->view_cats[i];
         explore_string_t **by = state->by[vcat];
         if (vop != op && (vop != EXPLORE_OP_RANGE || op == EXPLORE_OP_EQUAL))
            continue;
         if (n++ == 0)
         {
            if (count++) rjsonwriter_add_comma(w);
            rjsonwriter_add_newline(w);
            rjsonwriter_add_tabs(w, 1);
            rjsonwriter_add_string(w,
                  (op == EXPLORE_OP_EQUAL ? "filter_equal" :
                  (op == EXPLORE_OP_MIN   ? "filter_min"   :
                  (op == EXPLORE_OP_MAX   ? "filter_max"   : ""))));
            rjsonwriter_add_colon(w);
            rjsonwriter_add_space(w);
            rjsonwriter_add_start_object(w);
         }
         if (n > 1) rjsonwriter_add_comma(w);
         rjsonwriter_add_newline(w);
         rjsonwriter_add_tabs(w, 2);
         rjsonwriter_add_string(w, explore_by_info[vcat].rdbkey);
         rjsonwriter_add_colon(w);
         rjsonwriter_add_space(w);
         rjsonwriter_add_string(w,
               ((op == EXPLORE_OP_EQUAL && !state->view_match[i]) ? "" :
               (op == EXPLORE_OP_EQUAL ? state->view_match[i]->str :
               (op == EXPLORE_OP_MIN   ? by[state->view_idx_min[i]]->str :
               (op == EXPLORE_OP_MAX   ? by[state->view_idx_max[i]]->str :
               "")))));
      }
      if (n)
      {
         rjsonwriter_add_newline(w);
         rjsonwriter_add_tabs(w, 1);
         rjsonwriter_add_end_object(w);
      }
   }
   rjsonwriter_add_newline(w);
   rjsonwriter_add_end_object(w);
   rjsonwriter_add_newline(w);
   rjsonwriter_free(w);
   intfstream_close(file);
   free(file);

   explore_on_edit_views(MENU_ENUM_LABEL_EXPLORE_VIEW_SAVED);
}

static int explore_action_ok_saveview(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_input_ctx_line_t line = {0};
   line.label                 = msg_hash_to_str(MENU_ENUM_LABEL_EXPLORE_NEW_VIEW);
   line.cb                    = explore_action_saveview_complete;
   menu_input_dialog_start(&line);
   return 0;
}

static void explore_load_view(explore_state_t *state, const char* path)
{
   intfstream_t *file;
   rjson_t* json;
   uint8_t op = ((uint8_t)-1);
   unsigned cat;
   enum rjson_type type;

   state->view_levels = 0;
   state->view_search[0] = '\0';

   if (!(file = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE)))
      return;

   json = rjson_open_stream(file);

   /* Configure parser */
   rjson_set_options(json,
      RJSON_OPTION_ALLOW_UTF8BOM|RJSON_OPTION_ALLOW_COMMENTS);

   while ((type = rjson_next(json)) != RJSON_DONE && type != RJSON_ERROR)
   {
      unsigned int depth = rjson_get_context_depth(json);
      if (depth == 1 && type == RJSON_STRING)
      {
         const char* key = rjson_get_string(json, NULL);
         if (string_is_equal(key, "filter_name") &&
               rjson_next(json) == RJSON_STRING)
            strlcpy(state->view_search,
                  rjson_get_string(json, NULL), sizeof(state->view_search));
         else if (string_is_equal(key, "filter_equal") &&
               rjson_next(json) == RJSON_OBJECT)
            op = EXPLORE_OP_EQUAL;
         else if (string_is_equal(key, "filter_min") &&
               rjson_next(json) == RJSON_OBJECT)
            op = EXPLORE_OP_MIN;
         else if (string_is_equal(key, "filter_max") &&
               rjson_next(json) == RJSON_OBJECT)
            op = EXPLORE_OP_MAX;
      }
      else if (depth == 2 && type == RJSON_STRING && op != ((uint8_t)-1))
      {
         const char* key = rjson_get_string(json, NULL);
         for (cat = 0; cat != EXPLORE_CAT_COUNT; cat++)
            if (string_is_equal(key, explore_by_info[cat].rdbkey)) 
               break;
         if (cat == EXPLORE_CAT_COUNT)
            rjson_next(json); /* skip value */
         else
         {
            explore_string_t **entries = state->by[cat];
            const char* value = NULL;
            unsigned lvl, lvl_max = state->view_levels;
            uint8_t valid_op = ((uint8_t)-1);

            type = rjson_next(json);
            if (type == RJSON_STRING || type == RJSON_NUMBER)
               value = rjson_get_string(json, NULL);
            if (value && !*value)
               value = NULL;

            for (lvl = 0; lvl != lvl_max; lvl++)
               if (state->view_cats[lvl] == cat)
                  break;

            if (!value && state->has_unknown[cat] && op == EXPLORE_OP_EQUAL)
            {
               state->view_match[lvl] = NULL;
               valid_op = EXPLORE_OP_EQUAL;
            }
            else if (value && entries)
            {
               /* use existing qsort function for binary search */
               explore_string_t *evalue = (explore_string_t *)
                     (value - offsetof(explore_string_t, str));
               uint32_t i, ifrom, ito, imax = (uint32_t)RBUF_LEN(entries);
               int cmp;
               int (*compare_func)(const void *, const void *) =
                     (explore_by_info[cat].is_numeric
                        ? explore_qsort_func_nums
                        : explore_qsort_func_strings);

               /* binary search index where entry <= value */
               for (i = 0, ifrom = 0, ito = imax, cmp = 1; ito && cmp;)
               {
                  size_t remain = ito % 2;
                  ito           = ito / 2;
                  i             = ifrom + ito;
                  cmp           = compare_func(&evalue, &entries[i]);
                  if (cmp >= 0)
                     ifrom      = (uint32_t)(i + remain);
               }

               if (op == EXPLORE_OP_EQUAL && !cmp)
               {
                  state->view_match[lvl] = entries[i];
                  valid_op = EXPLORE_OP_EQUAL;
               }
               else if (op == EXPLORE_OP_MIN)
               {
                  state->view_idx_min[lvl] = (cmp ? i + 1 : i);
                  valid_op = ((lvl != lvl_max && state->view_op[lvl] == EXPLORE_OP_MAX)
                                 ? EXPLORE_OP_RANGE : EXPLORE_OP_MIN);
               }
               else if (op == EXPLORE_OP_MAX)
               {
                  state->view_idx_max[lvl] = i;
                  valid_op = ((lvl != lvl_max && state->view_op[lvl] == EXPLORE_OP_MIN)
                                 ? EXPLORE_OP_RANGE : EXPLORE_OP_MAX);
               }
            }
            if (valid_op != ((uint8_t)-1))
            {
               state->view_op       [lvl] = valid_op;
               state->view_cats     [lvl] = cat;
               state->view_use_split[lvl] = explore_by_info[cat].use_split;
               if (lvl == lvl_max) state->view_levels++; 
            }
         }
      }
      else if (depth == 1 && type == RJSON_OBJECT_END)
      {
         op = ((uint8_t)-1);
      }
   }
   rjson_free(json);
   intfstream_close(file);
   free(file);
}

unsigned menu_displaylist_explore(file_list_t *list, settings_t *settings)
{
   unsigned i;
   char tmp[512];
   struct explore_state *state  = explore_state;
   struct file_list *menu_stack = menu_entries_get_menu_stack_ptr(0);
   struct item_file *stack_top  = menu_stack->list;
   size_t depth                 = menu_stack->size;
   unsigned current_type        = (depth > 0 ? stack_top[depth - 1].type : 0);
   unsigned previous_type       = (depth > 1 ? stack_top[depth - 2].type : 0);
   unsigned current_cat         = current_type - EXPLORE_TYPE_FIRSTCATEGORY;

   if (depth > 1)
   {
      /* overwrite the menu title function with our custom one */
      /* depth 1 is never popped so we can only do this on sub menus */
      ((menu_file_list_cbs_t*)stack_top[depth - 1].actiondata)
         ->action_get_title = explore_action_get_title;
   }

   if (!state)
   {
      if (!menu_explore_init_in_progress(NULL))
         task_push_menu_explore_init(
               settings->paths.directory_playlist,
               settings->paths.path_content_database);

      menu_entries_append(list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_EXPLORE_INITIALISING_LIST),
            msg_hash_to_str(MENU_ENUM_LABEL_EXPLORE_INITIALISING_LIST),
            MENU_ENUM_LABEL_EXPLORE_INITIALISING_LIST,
            FILE_TYPE_NONE, 0, 0, NULL);

      return (unsigned)list->size;
   }

   /* check if we are opening a saved view */
   if (current_type == MENU_SETTING_HORIZONTAL_MENU || current_type == MENU_EXPLORE_TAB)
   {
      const char* view_path = explore_get_view_path();
      if (view_path)
      {
         explore_load_view(state, view_path);
         current_type = EXPLORE_TYPE_VIEW;
      }
   }

   /* clear any filter remaining from showing a view on the horizontal menu */
   if (current_type == MENU_EXPLORE_TAB)
   {
      state->view_levels = 0;
      state->view_search[0] = '\0';
   }

   /* clear title string */
   state->title[0] = '\0';

   /* append filtered categories to title */
   for (i = 0; i != state->view_levels; i++)
   {
      unsigned cat = state->view_cats[i];
      explore_string_t **entries = state->by[cat];
      explore_string_t* match = state->view_match[i];
      explore_append_title(state, "%s%s: ", (i ? " / " : ""),
            msg_hash_to_str(explore_by_info[cat].name_enum));
      switch (state->view_op[i])
      {
         case EXPLORE_OP_EQUAL:
            explore_append_title(state, "%s", (match ? match->str
                  : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UNKNOWN)));
            break;
         case EXPLORE_OP_MIN:
            explore_append_title(state, "%s - %s",
                  entries[state->view_idx_min[i]]->str,
                  entries[RBUF_LEN(entries)-1]->str);
            break;
         case EXPLORE_OP_MAX:
            explore_append_title(state, "%s - %s",
                  entries[0]->str,
                  entries[state->view_idx_max[i]]->str);
            break;
         case EXPLORE_OP_RANGE:
            explore_append_title(state, "%s - %s",
                  entries[state->view_idx_min[i]]->str,
                  entries[state->view_idx_max[i]]->str);
            break;
      }
   }

   /* append string search to title */
   if (*state->view_search)
      explore_append_title(state, "%s%s: '%s'",
            (state->view_levels ? " / " : ""),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME),
            state->view_search);

   state->show_icons = (current_cat == EXPLORE_BY_SYSTEM)
         ? EXPLORE_ICONS_SYSTEM_CATEGORY
         : (current_type >= EXPLORE_TYPE_FIRSTITEM)
               ? EXPLORE_ICONS_CONTENT
               : EXPLORE_ICONS_OFF;

   if (     current_type == MENU_EXPLORE_TAB 
         || current_type == EXPLORE_TYPE_ADDITIONALFILTER)
   {
      /* Explore top or selecting an additional filter category */
      unsigned cat;
      bool is_top = (current_type == MENU_EXPLORE_TAB);
      if (is_top)
         strlcpy(state->title,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_EXPLORE_TAB),
               sizeof(state->title));
      else
         explore_append_title(state, " / %s",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_EXPLORE_ADDITIONAL_FILTER));

      if (!*state->view_search)
      {
         explore_menu_entry(
               list, state,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_EXPLORE_SEARCH_NAME),
               EXPLORE_TYPE_SEARCH, explore_action_ok_find);
         explore_menu_add_spacer(list);
      }

      for (cat = 0; cat < EXPLORE_CAT_COUNT; cat++)
      {
         explore_string_t **entries = state->by[cat];
         if (!RBUF_LEN(entries))
            continue;

         /* don't list already filtered categories unless it can be filtered by range */
         for (i = 0; i != state->view_levels; i++)
            if (state->view_cats[i] == cat)
               break;

         if (i == state->view_levels || (i == state->view_levels - 1
                  && state->view_op[i] == EXPLORE_OP_EQUAL
                  && !explore_by_info[cat].is_boolean
                  && RBUF_LEN(state->by[cat]) > 1))
         {
            size_t tmplen = strlcpy(tmp,
                  msg_hash_to_str(explore_by_info[cat].by_enum), sizeof(tmp));

            if (is_top)
            {
               if (explore_by_info[cat].is_numeric)
                  snprintf(tmp + tmplen, sizeof(tmp) - tmplen, " (%s - %s)",
                        entries[0]->str, entries[RBUF_LEN(entries) - 1]->str);
               else if (!explore_by_info[cat].is_boolean)
               {
                  strlcat(tmp, " (", sizeof(tmp));
                  snprintf(tmp + tmplen + 2, sizeof(tmp) - tmplen - 2,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_EXPLORE_ITEMS_COUNT),
                        (unsigned)RBUF_LEN(entries));
                  strlcat(tmp, ")", sizeof(tmp));
               }
            }
            else if (i != state->view_levels)
            {
               strlcat(tmp, " (", sizeof(tmp));
               strlcat(tmp, msg_hash_to_str(MENU_ENUM_LABEL_EXPLORE_RANGE_FILTER), sizeof(tmp));
               strlcat(tmp, ")", sizeof(tmp));
            }

            explore_menu_entry(list, state,
                  tmp, cat + EXPLORE_TYPE_FIRSTCATEGORY, explore_action_ok);
         }
      }

      if (is_top)
      {
         explore_menu_add_spacer(list);
         explore_menu_entry(list, state,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_EXPLORE_SHOW_ALL),
               EXPLORE_TYPE_SHOWALL, explore_action_ok);
      }
   }
   else if ((!state->view_levels && !*state->view_search
         && current_cat < EXPLORE_CAT_COUNT))
   {
      /* Unfiltered list of all items in a selected explore by category */
      explore_string_t **entries = state->by[current_cat];
      size_t i_last              = RBUF_LEN(entries) - 1;
      for (i = 0; i <= i_last; i++)
         explore_menu_entry(list, state,
               entries[i]->str, EXPLORE_TYPE_FIRSTITEM + i, explore_action_ok);

      if (state->has_unknown[current_cat])
      {
         explore_menu_add_spacer(list);
         explore_menu_entry(list, state,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UNKNOWN),
               EXPLORE_TYPE_FILTERNULL, explore_action_ok);
      }

      explore_append_title(state, "%s",
            msg_hash_to_str(explore_by_info[current_cat].by_enum));
   }
   else if (current_type < EXPLORE_TYPE_FIRSTITEM
         || (previous_type >= EXPLORE_TYPE_FIRSTCATEGORY
            && previous_type < EXPLORE_TYPE_FIRSTITEM))
   {
      unsigned           view_levels      = state->view_levels;
      char*              view_search      = state->view_search;
      uint8_t*           view_op          = state->view_op;
      bool*              view_use_split   = state->view_use_split;
      unsigned*          view_cats        = state->view_cats;
      explore_string_t** view_match       = state->view_match;
      uint32_t*          view_idx_min     = state->view_idx_min;
      uint32_t*          view_idx_max     = state->view_idx_max;
      explore_entry_t*   entries          = state->entries, *e, *eend;
      bool* map_filtered_category         = NULL;
      bool has_search                     = !!*view_search;
      bool is_show_all                    = (!view_levels && !has_search);
      bool is_filtered_category           = (current_cat < EXPLORE_CAT_COUNT);
      bool filtered_category_have_unknown = false;
      size_t first_list_entry;

      if (is_show_all)
      {
         explore_append_title(state,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_EXPLORE_ALL));
      }

      if (is_filtered_category)
      {
         /* List filtered items in a selected explore by category */
         if (!view_levels || view_cats[view_levels - 1] != current_cat)
         {
            explore_append_title(state, " / %s",
                  msg_hash_to_str(explore_by_info[current_cat].by_enum));
         }
         else
         {
            /* List all items again when setting a range filter */
            explore_append_title(state, " (%s)",
                  msg_hash_to_str(MENU_ENUM_LABEL_EXPLORE_RANGE_FILTER));
            view_levels--;
         }
      }
      else if (current_type == EXPLORE_TYPE_VIEW)
      {
         /* Show a saved view */
         state->show_icons = EXPLORE_ICONS_CONTENT;
         explore_menu_entry(list, state,
               msg_hash_to_str(MENU_ENUM_LABEL_EXPLORE_DELETE_VIEW),
               EXPLORE_TYPE_VIEW, explore_action_ok_deleteview);
         explore_menu_add_spacer(list);
      }
      else
      {
         /* Game list */
         state->show_icons = EXPLORE_ICONS_CONTENT;
         explore_menu_entry(list, state,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_EXPLORE_ADD_ADDITIONAL_FILTER),
               EXPLORE_TYPE_ADDITIONALFILTER, explore_action_ok);
         explore_menu_entry(list, state,
               msg_hash_to_str(MENU_ENUM_LABEL_EXPLORE_SAVE_VIEW),
               EXPLORE_TYPE_VIEW, explore_action_ok_saveview);
         explore_menu_add_spacer(list);
      }

      first_list_entry = list->size;
      for (e = entries, eend = RBUF_END(entries); e != eend; e++)
      {
         for (i = 0; i != view_levels; i++)
         {
            explore_string_t* eby = e->by[view_cats[i]];
            switch (view_op[i])
            {
               case EXPLORE_OP_EQUAL:
                  if (view_match[i] == eby)
                     continue;
                  if (view_use_split[i] && e->split)
                  {
                     explore_string_t** split = e->split;
                     do
                     {
                        if (*split == view_match[i]) 
                           break;
                     } while (*(++split));
                     if (*split)
                        continue;
                  }
                  goto SKIP_ENTRY;
               case EXPLORE_OP_MIN:
                  if (eby && eby->idx >= view_idx_min[i])
                     continue;
                  goto SKIP_ENTRY;
               case EXPLORE_OP_MAX:
                  if (eby && eby->idx <= view_idx_max[i])
                     continue;
                  goto SKIP_ENTRY;
               case EXPLORE_OP_RANGE:
                  if (eby && eby->idx >= view_idx_min[i]
                          && eby->idx <= view_idx_max[i])
                     continue;
                  goto SKIP_ENTRY;
            }
         }

         if (has_search && !strcasestr(e->playlist_entry->label, view_search))
            goto SKIP_ENTRY;

         if (is_filtered_category)
         {
            explore_string_t* str = e->by[current_cat];
            if (!str)
            {
               filtered_category_have_unknown = true;
               continue;
            }
            if (RHMAP_HAS(map_filtered_category, str->idx + 1))
               continue;
            RHMAP_SET(map_filtered_category, str->idx + 1, true);
            explore_menu_entry(list, state, str->str,
                  EXPLORE_TYPE_FIRSTITEM + str->idx, explore_action_ok);
         }
#ifdef EXPLORE_SHOW_ORIGINAL_TITLE
         else if (e->original_title)
            explore_menu_entry(list, state, e->original_title,
                  EXPLORE_TYPE_FIRSTITEM + (e - entries), explore_action_ok);
#endif
         else
            explore_menu_entry(list, state, e->playlist_entry->label,
                  EXPLORE_TYPE_FIRSTITEM + (e - entries), explore_action_ok);
SKIP_ENTRY:;
      }

      if (is_filtered_category)
         qsort(list->list, list->size, sizeof(*list->list), explore_qsort_func_menulist);

      explore_append_title(state,
            " (%u)", (unsigned)(list->size - first_list_entry));

      if (is_filtered_category && filtered_category_have_unknown)
      {
         explore_menu_add_spacer(list);
         explore_menu_entry(list, state,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UNKNOWN),
               EXPLORE_TYPE_FILTERNULL, explore_action_ok);
      }

      RHMAP_FREE(map_filtered_category);
   }
   else
   {
      /* Content page of selected game */
      int pl_idx;
      const struct playlist_entry *pl_entry = 
         state->entries[current_type - EXPLORE_TYPE_FIRSTITEM].playlist_entry;
      menu_handle_t                   *menu = menu_state_get_ptr()->driver_data;

      strlcpy(state->title,
            pl_entry->label, sizeof(state->title));

      for (pl_idx = 0; pl_idx != RBUF_LEN(state->playlists); pl_idx++)
      {
         menu_displaylist_info_t          info;
         const struct playlist_entry* pl_first = NULL;
         playlist_t                       *pl  = 
            state->playlists[pl_idx];

         playlist_get_index(pl, 0, &pl_first);

         if (  pl_entry <  pl_first || 
               pl_entry >= pl_first + playlist_size(pl))
            continue;

         /* Fake all the state so the content screen 
          * and information screen think we're viewing via playlist */
         menu_displaylist_info_init(&info);
         playlist_set_cached_external(pl);
         menu->rpl_entry_selection_ptr = (unsigned)(pl_entry - pl_first);
         strlcpy(menu->deferred_path,
               pl_entry->path, sizeof(menu->deferred_path));
         info.list                     = list;
         menu_displaylist_ctl(DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS, &info,
               settings);
         break;
      }
   }

   return (unsigned)list->size;
}

uintptr_t menu_explore_get_entry_icon(unsigned type)
{
   unsigned i;
   if (!explore_state || !explore_state->show_icons
         || type < EXPLORE_TYPE_FIRSTITEM)
      return 0;

   i = (type - EXPLORE_TYPE_FIRSTITEM);
   if (explore_state->show_icons == EXPLORE_ICONS_CONTENT)
   {
      explore_entry_t* e = &explore_state->entries[i];
      if (e < RBUF_END(explore_state->entries))
         return explore_state->icons[e->by[EXPLORE_BY_SYSTEM]->idx];
   }
   else if (explore_state->show_icons == EXPLORE_ICONS_SYSTEM_CATEGORY)
   {
      if (i < RBUF_LEN(explore_state->icons))
         return explore_state->icons[i];
   }
   return 0;
}

ssize_t menu_explore_get_entry_playlist_index(unsigned type,
      playlist_t **playlist, const struct playlist_entry **playlist_entry,
      file_list_t *list, size_t *list_pos, size_t *list_size)
{
   int              pl_idx;
   explore_entry_t* entry;

   if (!explore_state || type < EXPLORE_TYPE_FIRSTITEM
         || explore_state->show_icons != EXPLORE_ICONS_CONTENT)
      return -1;

   entry = &explore_state->entries[type - EXPLORE_TYPE_FIRSTITEM];
   if (entry >= RBUF_END(explore_state->entries)
         || !entry->playlist_entry)
      return -1;

   for (pl_idx = 0; pl_idx != RBUF_LEN(explore_state->playlists); pl_idx++)
   {
      const struct playlist_entry* pl_first = NULL;
      playlist_t *pl  = explore_state->playlists[pl_idx];

      playlist_get_index(pl, 0, &pl_first);

      if (  entry->playlist_entry <  pl_first ||
            entry->playlist_entry >= pl_first + playlist_size(pl))
         continue;

      if (playlist) *playlist = pl;
      if (playlist_entry) *playlist_entry = entry->playlist_entry;

      /* correct numbers of list pos and list size */ 
      if (list && list_pos && list_size)
         while (*list_size && list->list[list->size-*list_size].type < EXPLORE_TYPE_FIRSTITEM)
            { (*list_size)--; (*list_pos)--; }

      /* Playlist needs to get cached for on-demand thumbnails */
      playlist_set_cached_external(pl);
      return (ssize_t)(entry->playlist_entry - pl_first);
   }
   return -1;
}

ssize_t menu_explore_set_playlist_thumbnail(unsigned type,
      gfx_thumbnail_path_data_t *thumbnail_path_data)
{
   const char *db_name;
   ssize_t playlist_index = -1;
   playlist_t *playlist = NULL;
   explore_entry_t* entry;

   if (!explore_state || type < EXPLORE_TYPE_FIRSTITEM
         || explore_state->show_icons != EXPLORE_ICONS_CONTENT)
      return -1;

   entry = &explore_state->entries[type - EXPLORE_TYPE_FIRSTITEM];
   if (entry >= RBUF_END(explore_state->entries))
      return -1;

   db_name = entry->by[EXPLORE_BY_SYSTEM]->str;
   if (!string_is_empty(db_name))
      playlist_index = menu_explore_get_entry_playlist_index(type, &playlist, NULL, NULL, NULL, NULL);

   if (playlist_index >= 0 && playlist)
   {
      gfx_thumbnail_set_system(thumbnail_path_data, db_name, playlist);
      gfx_thumbnail_set_content_playlist(thumbnail_path_data,
            playlist, playlist_index);
      return playlist_index;
   }

   gfx_thumbnail_set_content_playlist(thumbnail_path_data, NULL, 0);
   return -1;
}

bool menu_explore_is_content_list(void)
{
   if (explore_state)
      return (explore_state->show_icons == EXPLORE_ICONS_CONTENT);
   return explore_get_view_path() != NULL;
}

void menu_explore_context_init(void)
{
   if (!explore_state)
      return;

   explore_load_icons(explore_state);
}

void menu_explore_context_deinit(void)
{
   if (!explore_state)
      return;

   explore_unload_icons(explore_state);
}

void menu_explore_free_state(explore_state_t *state)
{
   unsigned i;
   if (!state)
      return;
   for (i = 0; i != EXPLORE_CAT_COUNT; i++)
      RBUF_FREE(state->by[i]);

   RBUF_FREE(state->entries);

   for (i = 0; i != RBUF_LEN(state->playlists); i++)
      playlist_free(state->playlists[i]);
   RBUF_FREE(state->playlists);

   explore_unload_icons(state);
   RBUF_FREE(state->icons);

   ex_arena_free(&state->arena);
}

void menu_explore_free(void)
{
   if (!explore_state)
      return;

   menu_explore_free_state(explore_state);
   free(explore_state);
   explore_state = NULL;
}

void menu_explore_set_state(explore_state_t *state)
{
   if (!state)
      return;

   if (explore_state)
      menu_explore_free();

   /* needs to be done now on the main thread */
   explore_load_icons(state);

   explore_state = state;
}
