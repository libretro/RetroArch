/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <string.h>

#include <compat/strl.h>
#include <encodings/crc32.h>
#include <formats/rjson.h>
#include <string/stdstring.h>

#include "vulkan_custom_driver.h"

enum meta_key
{
   META_KEY_NONE = 0,
   META_KEY_SCHEMA,
   META_KEY_VENDOR,
   META_KEY_LIBRARY,
   META_KEY_MIN_API,
   META_KEY_NAME,
   META_KEY_VERSION
};

struct meta_ctx
{
   struct vulkan_custom_driver_meta *meta;
   const char *err;
   int api_level;
   unsigned depth;
   enum meta_key key;
   bool schema;
};

static bool meta_is(const char *s, size_t len, const char *name)
{
   return len == strlen(name) && !memcmp(s, name, len);
}

/* Whole numbers only: meta.json has no other kind. */
static bool meta_int(const char *s, size_t len, int *out)
{
   size_t i;
   int v = 0;
   if (len < 1 || len > 6)
      return false;
   for (i = 0; i < len; i++)
   {
      if (s[i] < '0' || s[i] > '9')
         return false;
      v = v * 10 + (s[i] - '0');
   }
   *out = v;
   return true;
}

static const char *meta_library(const char *s, size_t len, char *out)
{
   if (len < 1 || len >= VULKAN_CUSTOM_DRIVER_LIBRARY_MAX)
      return "libraryName is not 1 to 63 characters";
   if (     s[0] == '.'
         || memchr(s, '/', len)
         || memchr(s, '\\', len)
         || memchr(s, '\0', len))
      return "libraryName is not a plain file name";
   if (len < 4 || memcmp(s + len - 3, ".so", 3))
      return "libraryName does not end in .so";
   memcpy(out, s, len);
   out[len] = '\0';
   return NULL;
}

/* Cosmetic text: cut to fit, without splitting a UTF-8 character. */
static void meta_text(const char *s, size_t len, char *out)
{
   if (len >= VULKAN_CUSTOM_DRIVER_TEXT_MAX)
   {
      len = VULKAN_CUSTOM_DRIVER_TEXT_MAX - 1;
      while (len && ((unsigned char)s[len] & 0xC0) == 0x80)
         len--;
   }
   memcpy(out, s, len);
   out[len] = '\0';
}

static bool meta_member(void *data, const char *s, size_t len)
{
   struct meta_ctx *ctx = (struct meta_ctx*)data;
   if (ctx->depth != 1)
      return true;
   if (meta_is(s, len, "schemaVersion"))
      ctx->key = META_KEY_SCHEMA;
   else if (meta_is(s, len, "vendor"))
      ctx->key = META_KEY_VENDOR;
   else if (meta_is(s, len, "libraryName"))
      ctx->key = META_KEY_LIBRARY;
   else if (meta_is(s, len, "minApi"))
      ctx->key = META_KEY_MIN_API;
   else if (meta_is(s, len, "name"))
      ctx->key = META_KEY_NAME;
   else if (meta_is(s, len, "driverVersion"))
      ctx->key = META_KEY_VERSION;
   else
      ctx->key = META_KEY_NONE;
   return true;
}

/* A value the current key cannot take, or a root that is not an object.
 * name and driverVersion are cosmetic: a wrong type leaves them empty. */
static bool meta_other(struct meta_ctx *ctx)
{
   if (ctx->depth == 0)
      ctx->err = "meta.json is not an object";
   else if (     ctx->depth == 1
              && ctx->key != META_KEY_NONE
              && ctx->key != META_KEY_NAME
              && ctx->key != META_KEY_VERSION)
      ctx->err = "meta.json has a value of the wrong type";
   ctx->key = META_KEY_NONE;
   return !ctx->err;
}

static bool meta_string(void *data, const char *s, size_t len)
{
   struct meta_ctx *ctx = (struct meta_ctx*)data;
   if (ctx->depth != 1)
      return ctx->depth ? true : meta_other(ctx);
   switch (ctx->key)
   {
      case META_KEY_VENDOR:
         if (len < 1 || len >= VULKAN_CUSTOM_DRIVER_TEXT_MAX)
            ctx->err = "vendor is not 1 to 127 characters";
         else
            meta_text(s, len, ctx->meta->vendor);
         break;
      case META_KEY_LIBRARY:
         ctx->err = meta_library(s, len, ctx->meta->library_name);
         break;
      case META_KEY_NAME:
         meta_text(s, len, ctx->meta->name);
         break;
      case META_KEY_VERSION:
         meta_text(s, len, ctx->meta->version);
         break;
      case META_KEY_NONE:
         break;
      default:
         return meta_other(ctx);
   }
   ctx->key = META_KEY_NONE;
   return !ctx->err;
}

static bool meta_number(void *data, const char *s, size_t len)
{
   int v;
   struct meta_ctx *ctx = (struct meta_ctx*)data;
   if (ctx->depth != 1)
      return ctx->depth ? true : meta_other(ctx);
   switch (ctx->key)
   {
      case META_KEY_SCHEMA:
         ctx->schema = meta_int(s, len, &v) && v == 1;
         if (!ctx->schema)
            ctx->err = "schemaVersion is not 1";
         break;
      case META_KEY_MIN_API:
         if (!meta_int(s, len, &v))
            ctx->err = "minApi is not a whole number";
         else if (v > ctx->api_level)
            ctx->err = "minApi is above the device's API level";
         break;
      case META_KEY_NONE:
         break;
      default:
         return meta_other(ctx);
   }
   ctx->key = META_KEY_NONE;
   return !ctx->err;
}

static bool meta_start_object(void *data)
{
   struct meta_ctx *ctx = (struct meta_ctx*)data;
   if (ctx->depth && !meta_other(ctx))
      return false;
   ctx->depth++;
   return true;
}

static bool meta_start_array(void *data)
{
   struct meta_ctx *ctx = (struct meta_ctx*)data;
   if (!meta_other(ctx))
      return false;
   ctx->depth++;
   return true;
}

static bool meta_end(void *data)
{
   ((struct meta_ctx*)data)->depth--;
   return true;
}

static bool meta_bool(void *data, bool value)
{
   (void)value;
   return meta_other((struct meta_ctx*)data);
}

static bool meta_null(void *data)
{
   return meta_other((struct meta_ctx*)data);
}

const char *vulkan_custom_driver_parse_meta(const char *buf, size_t len,
      int api_level, struct vulkan_custom_driver_meta *meta)
{
   struct meta_ctx ctx;
   memset(meta, 0, sizeof(*meta));
   if (len > VULKAN_CUSTOM_DRIVER_META_MAX)
      return "meta.json is over 4 KiB";
   ctx.meta      = meta;
   ctx.err       = NULL;
   ctx.api_level = api_level;
   ctx.depth     = 0;
   ctx.key       = META_KEY_NONE;
   ctx.schema    = false;
   if (!rjson_parse_quick(buf, len, &ctx, RJSON_OPTION_ALLOW_UTF8BOM,
         meta_member, meta_string, meta_number,
         meta_start_object, meta_end, meta_start_array, meta_end,
         meta_bool, meta_null, NULL))
      return ctx.err ? ctx.err : "meta.json is not valid JSON";
   if (!ctx.schema)
      return "schemaVersion is not 1";
   if (!*meta->vendor)
      return "vendor is missing";
   if (!*meta->library_name)
      return "libraryName is missing";
   meta->mesa = string_is_equal_case_insensitive(meta->vendor,
         VULKAN_CUSTOM_DRIVER_VENDOR);
   return NULL;
}

const char *vulkan_custom_driver_check_members(
      const struct string_list *members, const char *library_name)
{
   size_t i;
   unsigned so    = 0;
   bool has_meta  = false;
   bool has_lib   = false;
   for (i = 0; i < members->size; i++)
   {
      const char *e = members->elems[i].data;
      if (string_is_equal(e, "meta.json"))
         has_meta = true;
      else if (string_ends_with(e, ".so"))
      {
         so++;
         if (string_is_equal(e, library_name))
            has_lib = true;
      }
   }
   if (!has_meta)
      return "no meta.json at the zip root";
   if (so != 1)
      return "the zip does not hold exactly one .so";
   if (!has_lib)
      return "the .so is not libraryName at the zip root";
   return NULL;
}

const char *vulkan_custom_driver_check_elf(const unsigned char *head,
      size_t len, int64_t size)
{
   if (size > VULKAN_CUSTOM_DRIVER_SO_MAX)
      return "the driver is over 64 MiB";
   if (len < VULKAN_CUSTOM_DRIVER_ELF_HEAD || memcmp(head, "\177ELF", 4))
      return "the driver is not an ELF file";
   /* ELFCLASS64, ELFDATA2LSB */
   if (head[4] != 2 || head[5] != 1)
      return "the driver is not 64-bit little-endian";
   if ((head[16] | (head[17] << 8)) != 3) /* ET_DYN */
      return "the driver is not a shared object";
   if ((head[18] | (head[19] << 8)) != 183) /* EM_AARCH64 */
      return "the driver is not arm64";
   return NULL;
}

uint32_t vulkan_custom_driver_key(const char *path,
      int64_t size, int64_t mtime)
{
   unsigned i;
   uint8_t b[16];
   uint32_t crc = encoding_crc32(0, (const uint8_t*)path, strlen(path));
   for (i = 0; i < 8; i++)
   {
      b[i]     = (uint8_t)((uint64_t)size  >> (8 * i));
      b[8 + i] = (uint8_t)((uint64_t)mtime >> (8 * i));
   }
   return encoding_crc32(crc, b, sizeof(b));
}

/* Length of the line at s with its newline; 0 for a torn last line. */
static size_t crash_line(const char *s, size_t len)
{
   const char *nl = (const char*)memchr(s, '\n', len);
   return nl ? (size_t)(nl - s) + 1 : 0;
}

/* A NULL core matches every core. */
static bool crash_match(const char *s, size_t line,
      const char *path, size_t path_len,
      const char *core, size_t core_len)
{
   const char *tab = (const char*)memchr(s, '\t', line);
   size_t      len = tab ? (size_t)(tab - s) : 0;
   if (!tab || line - len - 2 != path_len || memcmp(tab + 1, path, path_len))
      return false;
   return !core || (len == core_len && !memcmp(s, core, core_len));
}

static bool crash_has(const char *state, size_t len,
      const char *path, size_t path_len,
      const char *core, size_t core_len)
{
   size_t i = 0;
   size_t line;
   while (i < len && (line = crash_line(state + i, len - i)))
   {
      if (crash_match(state + i, line, path, path_len, core, core_len))
         return true;
      i += line;
   }
   return false;
}

static size_t crash_put(char *state, size_t len, size_t cap,
      const char *path, size_t path_len,
      const char *core, size_t core_len)
{
   size_t need = core_len + path_len + 2;
   while (len && state[len - 1] != '\n')
      len--;
   if (     !path_len
         || need > cap
         || memchr(path, '\n', path_len)
         || memchr(core, '\n', core_len)
         || memchr(core, '\t', core_len)
         || crash_has(state, len, path, path_len, core, core_len))
      return len;
   while (len + need > cap)
   {
      size_t line = crash_line(state, len);
      memmove(state, state + line, len - line);
      len -= line;
   }
   memcpy(state + len, core, core_len);
   state[len + core_len] = '\t';
   memcpy(state + len + core_len + 1, path, path_len);
   state[len + need - 1] = '\n';
   return len + need;
}

bool vulkan_custom_driver_crash_find(const char *state, size_t len,
      const char *path, const char *core)
{
   if (!core)
      core = "";
   return crash_has(state, len, path, strlen(path), core, strlen(core));
}

size_t vulkan_custom_driver_crash_add(char *state, size_t len, size_t cap,
      const char *path, const char *core)
{
   if (!core)
      core = "";
   return crash_put(state, len, cap, path, strlen(path), core, strlen(core));
}

size_t vulkan_custom_driver_crash_clear(char *state, size_t len,
      const char *path)
{
   size_t i        = 0;
   size_t path_len = strlen(path);
   while (i < len)
   {
      size_t line = crash_line(state + i, len - i);
      if (!line)
         return i;
      if (crash_match(state + i, line, path, path_len, NULL, 0))
      {
         memmove(state + i, state + i + line, len - i - line);
         len -= line;
      }
      else
         i += line;
   }
   return len;
}

bool vulkan_custom_driver_session_init(vulkan_custom_driver_session_t *s,
      vulkan_custom_driver_load_t load, void *load_data,
      const char *state, size_t state_len,
      const char *marker, size_t marker_len)
{
   const char *tab;
   size_t line, path_len;
   memset(s, 0, sizeof(*s));
   s->load      = load;
   s->load_data = load_data;
   if (state_len > sizeof(s->state))
      state_len = sizeof(s->state);
   if (state_len)
      memcpy(s->state, state, state_len);
   while (state_len && s->state[state_len - 1] != '\n')
      state_len--;
   s->state_len = state_len;
   if (     !marker_len
         || !(line = crash_line(marker, marker_len))
         || !(tab  = (const char*)memchr(marker, '\t', line))
         || !(path_len = line - (size_t)(tab - marker) - 2))
      return false;
   s->state_len = crash_put(s->state, s->state_len, sizeof(s->state),
         tab + 1, path_len, marker, (size_t)(tab - marker));
   return true;
}

static struct vulkan_custom_driver_path *session_find(
      vulkan_custom_driver_session_t *s, const char *path)
{
   unsigned i;
   for (i = 0; i < s->path_count; i++)
      if (string_is_equal(s->paths[i].path, path))
         return &s->paths[i];
   return NULL;
}

/* NULL for a new path once the table is full. */
static struct vulkan_custom_driver_path *session_path(
      vulkan_custom_driver_session_t *s, const char *path)
{
   struct vulkan_custom_driver_path *p = session_find(s, path);
   if (p || s->path_count == VULKAN_CUSTOM_DRIVER_PATHS)
      return p;
   p = &s->paths[s->path_count++];
   strlcpy(p->path, path, sizeof(p->path));
   return p;
}

static enum vulkan_custom_driver_msg session_once(
      struct vulkan_custom_driver_path *p, enum vulkan_custom_driver_msg msg)
{
   if (p->shown & (1u << msg))
      return VULKAN_CUSTOM_DRIVER_MSG_NONE;
   p->shown |= 1u << msg;
   return msg;
}

/* True when req names a custom driver this session may use. */
static bool session_wants(vulkan_custom_driver_session_t *s,
      const struct vulkan_custom_driver_request *req,
      struct vulkan_custom_driver_path *p,
      enum vulkan_custom_driver_msg *msg)
{
   if (!req->supported)
   {
      *msg = session_once(p, VULKAN_CUSTOM_DRIVER_MSG_UNSUPPORTED);
      return false;
   }
   if (vulkan_custom_driver_crash_find(s->state, s->state_len,
            req->path, req->core))
   {
      *msg = session_once(p, VULKAN_CUSTOM_DRIVER_MSG_CRASHED);
      return false;
   }
   if (p->failed || p->invalid)
      return false;
   if (req->size < 0)
   {
      p->invalid = true;
      *msg       = session_once(p, VULKAN_CUSTOM_DRIVER_MSG_INVALID);
      return false;
   }
   return true;
}

void *vulkan_custom_driver_session_open(vulkan_custom_driver_session_t *s,
      const struct vulkan_custom_driver_request *req,
      const struct vulkan_custom_driver_meta **meta,
      enum vulkan_custom_driver_msg *msg)
{
   unsigned i;
   uint32_t key;
   struct vulkan_custom_driver_path *p;
   struct vulkan_custom_driver_slot *slot = NULL;
   enum vulkan_custom_driver_msg err      = VULKAN_CUSTOM_DRIVER_MSG_NONE;

   *meta      = NULL;
   *msg       = VULKAN_CUSTOM_DRIVER_MSG_NONE;
   s->current = NULL;
   s->full    = false;
   if (string_is_empty(req->path) || req->safe_mode)
      return NULL;
   /* Past a full table, as past full slots: the system driver. */
   if (!(p = session_path(s, req->path)))
   {
      if (!s->full_shown)
         *msg = VULKAN_CUSTOM_DRIVER_MSG_RESTART;
      s->full       = true;
      s->full_shown = true;
      return NULL;
   }
   if (!session_wants(s, req, p, msg))
      return NULL;

   key = vulkan_custom_driver_key(req->path, req->size, req->mtime);
   for (i = 0; i < VULKAN_CUSTOM_DRIVER_SLOTS; i++)
   {
      if (s->slots[i].handle && s->slots[i].key == key)
      {
         slot = &s->slots[i];
         break;
      }
      if (!s->slots[i].handle && !slot)
         slot = &s->slots[i];
   }
   if (!slot)
   {
      *msg    = session_once(p, VULKAN_CUSTOM_DRIVER_MSG_RESTART);
      s->full = true;
      return NULL;
   }
   if (!slot->handle)
   {
      if (!(slot->handle = s->load(s->load_data, req->path, key,
            &slot->meta, &err)))
      {
         if (err == VULKAN_CUSTOM_DRIVER_MSG_INVALID)
            p->invalid = true;
         else
         {
            err       = VULKAN_CUSTOM_DRIVER_MSG_FAILED;
            p->failed = true;
         }
         *msg = session_once(p, err);
         return NULL;
      }
      slot->key = key;
   }
   s->current = p;
   s->pending = true;
   *meta      = &slot->meta;
   return slot->handle;
}

enum vulkan_custom_driver_msg vulkan_custom_driver_session_failed(
      vulkan_custom_driver_session_t *s)
{
   struct vulkan_custom_driver_path *p = s->current;
   s->current = NULL;
   s->pending = false;
   if (!p)
      return VULKAN_CUSTOM_DRIVER_MSG_NONE;
   p->failed = true;
   return session_once(p, VULKAN_CUSTOM_DRIVER_MSG_FAILED);
}

bool vulkan_custom_driver_session_started(vulkan_custom_driver_session_t *s)
{
   bool pending = s->pending;
   s->pending   = false;
   return pending;
}

/* pending stays: the marker still goes once the kept instance is up. */
void vulkan_custom_driver_session_kept(vulkan_custom_driver_session_t *s)
{
   s->current = NULL;
}

bool vulkan_custom_driver_session_current(
      const vulkan_custom_driver_session_t *s)
{
   return s->current && s->pending;
}

enum vulkan_custom_driver_msg vulkan_custom_driver_session_picked(
      vulkan_custom_driver_session_t *s, const char *path)
{
   struct vulkan_custom_driver_path *p = session_find(s, path);
   s->state_len = vulkan_custom_driver_crash_clear(s->state, s->state_len,
         path);
   if (!p)
   {
      if (s->path_count < VULKAN_CUSTOM_DRIVER_PATHS)
         return VULKAN_CUSTOM_DRIVER_MSG_NONE;
      s->full_shown = true;
      return VULKAN_CUSTOM_DRIVER_MSG_RESTART;
   }
   if (p->failed)
      return VULKAN_CUSTOM_DRIVER_MSG_RESTART;
   p->invalid  = false;
   p->shown   &= ~(1u << VULKAN_CUSTOM_DRIVER_MSG_INVALID);
   return VULKAN_CUSTOM_DRIVER_MSG_NONE;
}
