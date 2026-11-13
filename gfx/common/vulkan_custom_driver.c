/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <stdio.h>
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

void vulkan_custom_driver_trim_dlerror(char *s, size_t len, const char *err)
{
   static const char prefix[] = "dlopen failed: ";
   static const char by[]     = " needed or dlopened by \"";
   const char *start;
   const char *end;
   const char *cut;
   const char *rest;
   size_t n;
   size_t m;
   if (!len)
      return;
   if (!err)
      err = "";
   start = err;
   if (!strncmp(start, prefix, sizeof(prefix) - 1))
      start += sizeof(prefix) - 1;
   end = start + strlen(start);
   if ((cut = strstr(start, ": needed by ")))
      end = cut;
   if ((cut = strstr(start, " referenced by ")) && cut < end)
      end = cut;
   if (end == start)
   {
      start = err;
      end   = err + strlen(err);
   }
   /* The "needed or dlopened by" path goes; "is not accessible" stays */
   if (     (cut  = strstr(start, by)) && cut < end
         && (rest = strchr(cut + sizeof(by) - 1, '"')) && rest < end)
      rest++;
   else
      cut = rest = end;
   n = (size_t)(cut - start);
   m = (size_t)(end - rest);
   if (n + m < len)
   {
      memcpy(s, start, n);
      memcpy(s + n, rest, m);
      s[n + m] = '\0';
      return;
   }
   if (n >= len - 1)
      memcpy(s, start, len - 1);
   else
   {
      memcpy(s, start, n);
      memcpy(s + n, rest, len - 1 - n);
   }
   s[len - 1] = '\0';
   if (len <= 4)
      return;
   /* "..." marks the cut, which never splits a UTF-8 sequence */
   n = len - 4;
   while (n && ((unsigned char)s[n] & 0xC0) == 0x80)
      n--;
   memcpy(s + n, "...", 4);
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

enum vulkan_custom_driver_msg vulkan_custom_driver_verdict(
      enum vulkan_custom_driver_hook hook, uint32_t gpus)
{
   if (hook == VULKAN_CUSTOM_DRIVER_HOOK_FAILED)
      return VULKAN_CUSTOM_DRIVER_MSG_NOT_LOADED;
   if (hook != VULKAN_CUSTOM_DRIVER_HOOK_LOADED)
      return VULKAN_CUSTOM_DRIVER_MSG_LOADER;
   if (!gpus)
      return VULKAN_CUSTOM_DRIVER_MSG_NO_GPU;
   return VULKAN_CUSTOM_DRIVER_MSG_NONE;
}

/* VkResult values, as vulkan_core.h has them */
static const struct
{
   int result;
   const char *name;
} vkcd_results[] = {
   { -1,          "VK_ERROR_OUT_OF_HOST_MEMORY" },
   { -2,          "VK_ERROR_OUT_OF_DEVICE_MEMORY" },
   { -3,          "VK_ERROR_INITIALIZATION_FAILED" },
   { -4,          "VK_ERROR_DEVICE_LOST" },
   { -5,          "VK_ERROR_MEMORY_MAP_FAILED" },
   { -6,          "VK_ERROR_LAYER_NOT_PRESENT" },
   { -7,          "VK_ERROR_EXTENSION_NOT_PRESENT" },
   { -8,          "VK_ERROR_FEATURE_NOT_PRESENT" },
   { -9,          "VK_ERROR_INCOMPATIBLE_DRIVER" },
   { -10,         "VK_ERROR_TOO_MANY_OBJECTS" },
   { -11,         "VK_ERROR_FORMAT_NOT_SUPPORTED" },
   { -12,         "VK_ERROR_FRAGMENTED_POOL" },
   { -13,         "VK_ERROR_UNKNOWN" },
   { -1000069000, "VK_ERROR_OUT_OF_POOL_MEMORY" },
   { -1000072003, "VK_ERROR_INVALID_EXTERNAL_HANDLE" },
   { -1000161000, "VK_ERROR_FRAGMENTATION" },
   { -1000000000, "VK_ERROR_SURFACE_LOST_KHR" },
   { -1000000001, "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR" },
   { -1000001004, "VK_ERROR_OUT_OF_DATE_KHR" },
   { -1000003001, "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR" },
   { -1000011001, "VK_ERROR_VALIDATION_FAILED_EXT" },
   { -1000023000, "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR" },
   { -1000174001, "VK_ERROR_NOT_PERMITTED_KHR" },
   { -1000338000, "VK_ERROR_COMPRESSION_EXHAUSTED_EXT" }
};

void vulkan_custom_driver_result_text(char *s, size_t len, int result)
{
   char num[24];
   size_t i;
   for (i = 0; i < sizeof(vkcd_results) / sizeof(vkcd_results[0]); i++)
      if (vkcd_results[i].result == result)
      {
         strlcpy(s, vkcd_results[i].name, len);
         return;
      }
   sprintf(num, "VkResult %d", result);
   strlcpy(s, num, len);
}

enum vulkan_custom_driver_msg vulkan_custom_driver_no_gpu(bool kgsl,
      bool mali)
{
   if (kgsl && !mali)
      return VULKAN_CUSTOM_DRIVER_MSG_NO_GPU_ADRENO;
   if (mali && !kgsl)
      return VULKAN_CUSTOM_DRIVER_MSG_NO_GPU_MALI;
   return VULKAN_CUSTOM_DRIVER_MSG_NO_GPU;
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

static void trust_hex(char *out, uint32_t key)
{
   static const char digits[] = "0123456789abcdef";
   int i;
   for (i = 7; i >= 0; i--, key >>= 4)
      out[i] = digits[key & 15];
   out[8] = '\0';
}

bool vulkan_custom_driver_trust_find(const char *trust, size_t len,
      const char *path, uint32_t key)
{
   char hex[9];
   trust_hex(hex, key);
   return crash_has(trust, len, path, strlen(path), hex, 8);
}

size_t vulkan_custom_driver_trust_add(char *trust, size_t len, size_t cap,
      const char *path, uint32_t key)
{
   char hex[9];
   trust_hex(hex, key);
   return crash_put(trust, len, cap, path, strlen(path), hex, 8);
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

/* For path's sublabel; NONE once it loads. */
static void session_why_set(struct vulkan_custom_driver_path *p,
      enum vulkan_custom_driver_msg why, const char *detail)
{
   p->why = why;
   strlcpy(p->detail, detail ? detail : "", sizeof(p->detail));
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
      session_why_set(p, VULKAN_CUSTOM_DRIVER_MSG_INVALID,
            "the zip is missing or not readable");
      *msg       = session_once(p, VULKAN_CUSTOM_DRIVER_MSG_INVALID);
      return false;
   }
   if (!req->trusted)
   {
      *msg = session_once(p, VULKAN_CUSTOM_DRIVER_MSG_UNTRUSTED);
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
   s->reason  = VULKAN_CUSTOM_DRIVER_MSG_NONE;
   s->result  = 0;
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
      const char *reason = NULL;
      if (!(slot->handle = s->load(s->load_data, req->path, key,
            &slot->meta, &err, &reason)))
      {
         if (err == VULKAN_CUSTOM_DRIVER_MSG_INVALID)
            p->invalid = true;
         else
         {
            err       = VULKAN_CUSTOM_DRIVER_MSG_FAILED;
            p->failed = true;
         }
         session_why_set(p, err, reason);
         *msg = session_once(p, err);
         return NULL;
      }
      slot->key = key;
   }
   session_why_set(p, VULKAN_CUSTOM_DRIVER_MSG_NONE, NULL);
   s->current = p;
   s->pending = true;
   *meta      = &slot->meta;
   return slot->handle;
}

enum vulkan_custom_driver_msg vulkan_custom_driver_session_failed(
      vulkan_custom_driver_session_t *s)
{
   struct vulkan_custom_driver_path *p = s->current;
   enum vulkan_custom_driver_msg msg   = s->reason;
   s->current = NULL;
   s->pending = false;
   s->reason  = VULKAN_CUSTOM_DRIVER_MSG_NONE;
   if (!p)
      return VULKAN_CUSTOM_DRIVER_MSG_NONE;
   p->failed = true;
   if (msg == VULKAN_CUSTOM_DRIVER_MSG_NONE && s->result < 0)
   {
      msg = VULKAN_CUSTOM_DRIVER_MSG_NO_START;
      vulkan_custom_driver_result_text(s->detail, sizeof(s->detail),
            s->result);
   }
   else if (msg == VULKAN_CUSTOM_DRIVER_MSG_NONE)
   {
      msg          = VULKAN_CUSTOM_DRIVER_MSG_FAILED;
      s->detail[0] = '\0';
   }
   session_why_set(p, msg, s->detail);
   return session_once(p, msg);
}

void vulkan_custom_driver_session_result(vulkan_custom_driver_session_t *s,
      int result)
{
   if (vulkan_custom_driver_session_current(s))
      s->result = result;
}

void vulkan_custom_driver_session_reason(vulkan_custom_driver_session_t *s,
      enum vulkan_custom_driver_msg msg, const char *detail)
{
   if (     vulkan_custom_driver_session_current(s)
         && s->reason == VULKAN_CUSTOM_DRIVER_MSG_NONE)
   {
      s->reason = msg;
      strlcpy(s->detail, detail ? detail : "", sizeof(s->detail));
   }
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

enum vulkan_custom_driver_msg vulkan_custom_driver_session_why(
      const vulkan_custom_driver_session_t *s, const char *path,
      const char **detail)
{
   unsigned i;
   for (i = 0; i < s->path_count; i++)
      if (string_is_equal(s->paths[i].path, path))
      {
         *detail = s->paths[i].detail;
         return s->paths[i].why;
      }
   *detail = "";
   return VULKAN_CUSTOM_DRIVER_MSG_NONE;
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
   p->shown   &= ~((1u << VULKAN_CUSTOM_DRIVER_MSG_INVALID)
               |  (1u << VULKAN_CUSTOM_DRIVER_MSG_UNTRUSTED));
   session_why_set(p, VULKAN_CUSTOM_DRIVER_MSG_NONE, NULL);
   return VULKAN_CUSTOM_DRIVER_MSG_NONE;
}
