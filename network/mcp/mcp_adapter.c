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

#include <string.h>
#include <stdio.h>

#include <boolean.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <formats/rjson.h>

#include "mcp_adapter.h"
#include "mcp_defines.h"
#include "mcp_adapter_tool_list.h"
#include "mcp_adapter_utils.h"
#include "mcp_json_templates.h"
#include "../../version.h"
#include "../../verbosity.h"

/* ---- JSON-RPC request parsing ---- */

typedef struct
{
   int64_t id;
   bool    has_id;
   char    method[128];
   char    tool_name[128];
   char    protocol_version[32];
   char    args_json[MCP_JSON_MAX_REQUEST];
   size_t  args_len;
   bool    is_notification;
} mcp_request_t;

static bool mcp_parse_request(const char *json, size_t len, mcp_request_t *req)
{
   rjson_t *parser;
   enum rjson_type type;
   int depth         = 0;
   bool in_params    = false;
   int params_depth  = 0;
   bool in_arguments = false;
   int args_depth    = 0;
   const char *args_key = NULL;
   const char *key   = NULL;
   size_t key_len    = 0;

   memset(req, 0, sizeof(*req));

   parser = rjson_open_buffer(json, len);
   if (!parser)
      return false;

   for (;;)
   {
      type = rjson_next(parser);

      if (type == RJSON_DONE || type == RJSON_ERROR)
         break;

      if (type == RJSON_OBJECT)
      {
         depth++;
         if (in_arguments)
         {
            args_depth++;
         }
         else if (in_params)
         {
            params_depth++;
            if (params_depth == 2 && key && string_is_equal(key, "arguments"))
            {
               in_arguments   = true;
               args_depth     = 1;
               args_key       = NULL;
               req->args_json[0] = '{';
               req->args_json[1] = '\0';
               req->args_len     = 1;
               key            = NULL;
            }
         }
         else if (depth == 2 && key && string_is_equal(key, "params"))
         {
            in_params    = true;
            params_depth = 1;
            key          = NULL;
         }
         continue;
      }
      if (type == RJSON_OBJECT_END)
      {
         if (in_arguments)
         {
            args_depth--;
            if (args_depth <= 0)
            {
               if (req->args_len + 2 < sizeof(req->args_json))
               {
                  req->args_json[req->args_len++] = '}';
                  req->args_json[req->args_len]   = '\0';
               }
               in_arguments = false;
            }
         }
         if (in_params)
         {
            params_depth--;
            if (params_depth <= 0)
               in_params = false;
         }
         depth--;
         continue;
      }
      if (type == RJSON_ARRAY)
      {
         if (in_arguments)
            args_depth++;
         else if (in_params)
            params_depth++;
         continue;
      }
      if (type == RJSON_ARRAY_END)
      {
         if (in_arguments)
            args_depth--;
         else if (in_params)
            params_depth--;
         continue;
      }

      /* inside arguments - capture flat key/value pairs */
      if (in_arguments && args_depth == 1)
      {
         if (type == RJSON_STRING)
         {
            size_t slen;
            const char *s = rjson_get_string(parser, &slen);
            if (!args_key)
               args_key = s;
            else
            {
               /* string value: append "key":"value" */
               char tmp[512];
               char escaped[256];
               mcp_json_escape(escaped, sizeof(escaped), s);
               if (req->args_len > 1)
                  req->args_json[req->args_len++] = ',';
               snprintf(tmp, sizeof(tmp), "\"%s\":\"%s\"", args_key, escaped);
               req->args_len += strlcpy(
                     req->args_json + req->args_len,
                     tmp,
                     sizeof(req->args_json) - req->args_len);
               args_key = NULL;
            }
         }
         else if (type == RJSON_NUMBER && args_key)
         {
            /* number value: append "key":123 */
            char tmp[256];
            if (req->args_len > 1)
               req->args_json[req->args_len++] = ',';
            snprintf(tmp, sizeof(tmp), "\"%s\":%d", args_key,
                  rjson_get_int(parser));
            req->args_len += strlcpy(
                  req->args_json + req->args_len,
                  tmp,
                  sizeof(req->args_json) - req->args_len);
            args_key = NULL;
         }
         else if ((type == RJSON_TRUE || type == RJSON_FALSE) && args_key)
         {
            char tmp[256];
            if (req->args_len > 1)
               req->args_json[req->args_len++] = ',';
            snprintf(tmp, sizeof(tmp), "\"%s\":%s", args_key,
                  type == RJSON_TRUE ? "true" : "false");
            req->args_len += strlcpy(
                  req->args_json + req->args_len,
                  tmp,
                  sizeof(req->args_json) - req->args_len);
            args_key = NULL;
         }
         else
            args_key = NULL;
         continue;
      }

      /* inside params - look for "name" key for tools/call */
      if (in_params && params_depth == 1 && type == RJSON_STRING)
      {
         size_t slen;
         const char *s = rjson_get_string(parser, &slen);

         if (key && string_is_equal(key, "name"))
         {
            strlcpy(req->tool_name, s, sizeof(req->tool_name));
            key = NULL;
         }
         else if (key && string_is_equal(key, "protocolVersion"))
         {
            strlcpy(req->protocol_version, s, sizeof(req->protocol_version));
            key = NULL;
         }
         else
            key = s;
         continue;
      }

      if (in_params)
      {
         key = NULL;
         continue;
      }

      /* top-level key name (string when no key pending) */
      if (depth == 1 && type == RJSON_STRING && !key)
      {
         size_t slen;
         key     = rjson_get_string(parser, &slen);
         key_len = slen;
         continue;
      }

      /* top-level string value */
      if (depth == 1 && key && type == RJSON_STRING)
      {
         size_t slen;
         const char *s = rjson_get_string(parser, &slen);

         if (string_is_equal(key, "method"))
            strlcpy(req->method, s, sizeof(req->method));

         key = NULL;
         continue;
      }

      /* top-level non-string value */
      if (depth == 1 && key)
      {
         if (string_is_equal(key, "id") && type == RJSON_NUMBER)
         {
            req->id     = (int64_t)rjson_get_int(parser);
            req->has_id = true;
         }
         key = NULL;
         continue;
      }

      key = NULL;
   }

   req->is_notification = (!req->has_id);

   rjson_free(parser);
   return (req->method[0] != '\0');
}

/* ---- response builders ---- */

static void mcp_build_error(char *buf, size_t buf_size,
      int64_t id, int code, const char *message)
{
   snprintf(buf, buf_size, mcp_error_fmt, (long long)id, code, message);
}

static void mcp_build_initialize_response(char *buf, size_t buf_size,
      int64_t id, const char *client_version)
{
   const char *version = (client_version && *client_version)
      ? client_version : MCP_PROTOCOL_VERSION;
   snprintf(buf, buf_size,
         mcp_initialize_fmt, (long long)id,
         version, PACKAGE_VERSION);
}

/* ---- public API ---- */

void mcp_adapter_handle_request(const char *json_request, size_t len,
      char *response, size_t response_size)
{
   mcp_request_t req;

   response[0] = '\0';

   if (!mcp_parse_request(json_request, len, &req))
   {
      RARCH_DBG("[MCP] Parse error for request: %.*s\n",
            len > 200 ? 200 : (int)len, json_request);
      mcp_build_error(response, response_size,0, -32700, "Parse error: Invalid JSON");
      return;
   }

   RARCH_DBG("[MCP] Parsed request: method='%s', id=%lld, tool='%s'\n",
         req.method, (long long)req.id, req.tool_name);

   if (string_is_equal(req.method, "initialize"))
   {
      mcp_build_initialize_response(response, response_size, req.id, req.protocol_version);
      return;
   }

   if (string_is_equal(req.method, "notifications/initialized"))
   {
      strlcpy(response, "{}", response_size);
      return;
   }

   if (string_is_equal(req.method, "tools/list"))
   {
      mcp_tools_build_list(req.id, response, response_size);
      return;
   }

   if (string_is_equal(req.method, "tools/call"))
   {
      if (string_is_empty(req.tool_name))
      {
         mcp_build_error(response, response_size, req.id, -32602,
               "Invalid params: missing tool name");
         return;
      }
      mcp_tools_call(req.id, req.tool_name,
            req.args_json, req.args_len,
            response, response_size);
      return;
   }

   if (string_is_equal(req.method, "resources/list"))
   {
      snprintf(response, response_size, mcp_resources_list_fmt, (long long)req.id);
      return;
   }

   RARCH_DBG("[MCP] Unknown method: '%s'\n", req.method);
   mcp_build_error(response, response_size, req.id, -32601, "Method not found");
}
