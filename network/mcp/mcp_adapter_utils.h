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

#ifndef MCP_ADAPTER_UTILS_H__
#define MCP_ADAPTER_UTILS_H__

#include <formats/rjson.h>
#include <string/stdstring.h>
#include <compat/strl.h>

static void mcp_json_escape(char *dst, size_t dst_size, const char *src)
{
   size_t w = 0;
   if (!src)
   {
      if (dst_size > 0)
         dst[0] = '\0';
      return;
   }
   for (; *src && w + 6 < dst_size; src++)
   {
      switch (*src)
      {
         case '"':  dst[w++] = '\\'; dst[w++] = '"';  break;
         case '\\': dst[w++] = '\\'; dst[w++] = '\\'; break;
         case '\n': dst[w++] = '\\'; dst[w++] = 'n';  break;
         case '\r': dst[w++] = '\\'; dst[w++] = 'r';  break;
         case '\t': dst[w++] = '\\'; dst[w++] = 't';  break;
         default:
            if ((unsigned char)*src < 0x20)
               w += snprintf(dst + w, dst_size - w, "\\u%04x",
                     (unsigned)(unsigned char)*src);
            else
               dst[w++] = *src;
            break;
      }
   }
   if (w < dst_size)
      dst[w] = '\0';
}

static void mcp_json_add(char *buf, size_t buf_size,
      const char *key, const char *value)
{
   char escaped[2048];
   char tmp[2048 + 256];
   size_t len = strlen(buf);
   mcp_json_escape(escaped, sizeof(escaped), value);
   snprintf(tmp, sizeof(tmp), "\"%s\":\"%s\"", key, escaped);
   if (len > 1)
      strlcat(buf, ",", buf_size);
   strlcat(buf, tmp, buf_size);
}

static void mcp_json_add_int(char *buf, size_t buf_size,
      const char *key, int64_t value)
{
   char tmp[256];
   size_t len = strlen(buf);
   snprintf(tmp, sizeof(tmp), "\"%s\":%lld", key, (long long)value);
   if (len > 1)
      strlcat(buf, ",", buf_size);
   strlcat(buf, tmp, buf_size);
}

static void mcp_json_add_bool(char *buf, size_t buf_size,
      const char *key, bool value)
{
   char tmp[256];
   size_t len = strlen(buf);
   snprintf(tmp, sizeof(tmp), "\"%s\":%s", key, value ? "true" : "false");
   if (len > 1)
      strlcat(buf, ",", buf_size);
   strlcat(buf, tmp, buf_size);
}

/*
 * Extract a string value for a given key from a flat JSON object.
 * Returns true if found.
 */
static bool mcp_json_extract_string(
      const char *json, size_t json_len,
      const char *key, char *value, size_t value_size)
{
   rjson_t *parser;
   enum rjson_type type;
   const char *pending_key = NULL;
   int depth               = 0;

   if (!json || !key || json_len == 0)
      return false;

   parser = rjson_open_buffer(json, json_len);
   if (!parser)
      return false;

   for (;;)
   {
      type = rjson_next(parser);
      if (type == RJSON_DONE || type == RJSON_ERROR)
         break;
      if (type == RJSON_OBJECT || type == RJSON_ARRAY)
      {
         depth++;
         pending_key = NULL;
         continue;
      }
      if (type == RJSON_OBJECT_END || type == RJSON_ARRAY_END)
      {
         depth--;
         continue;
      }
      if (depth != 1)
         continue;
      if (type == RJSON_STRING)
      {
         size_t slen;
         const char *s = rjson_get_string(parser, &slen);
         if (!pending_key)
            pending_key = s;
         else
         {
            if (string_is_equal(pending_key, key))
            {
               strlcpy(value, s, value_size);
               rjson_free(parser);
               return true;
            }
            pending_key = NULL;
         }
      }
      else
         pending_key = NULL;
   }

   rjson_free(parser);
   return false;
}

/*
 * Extract an integer value for a given key from a flat JSON object.
 * Returns true if found.
 */
static bool mcp_json_extract_int(
      const char *json, size_t json_len,
      const char *key, int *value)
{
   rjson_t *parser;
   enum rjson_type type;
   const char *pending_key = NULL;
   int depth               = 0;

   if (!json || !key || json_len == 0)
      return false;

   parser = rjson_open_buffer(json, json_len);
   if (!parser)
      return false;

   for (;;)
   {
      type = rjson_next(parser);
      if (type == RJSON_DONE || type == RJSON_ERROR)
         break;
      if (type == RJSON_OBJECT || type == RJSON_ARRAY)
      {
         depth++;
         pending_key = NULL;
         continue;
      }
      if (type == RJSON_OBJECT_END || type == RJSON_ARRAY_END)
      {
         depth--;
         continue;
      }
      if (depth != 1)
         continue;
      if (type == RJSON_STRING && !pending_key)
      {
         size_t slen;
         pending_key = rjson_get_string(parser, &slen);
         continue;
      }
      if (type == RJSON_NUMBER && pending_key)
      {
         if (string_is_equal(pending_key, key))
         {
            *value = rjson_get_int(parser);
            rjson_free(parser);
            return true;
         }
         pending_key = NULL;
         continue;
      }
      pending_key = NULL;
   }

   rjson_free(parser);
   return false;
}

#endif /* MCP_ADAPTER_UTILS_H__ */
