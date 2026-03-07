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

#ifndef MCP_JSON_TEMPLATES_H__
#define MCP_JSON_TEMPLATES_H__

/* ---- JSON-RPC response templates ----
 *
 * Each constant holds a complete JSON-RPC 2.0 response with
 * printf-style placeholders for dynamic values.
 */

static const char mcp_error_fmt[] =
   "{"
      "\"jsonrpc\":\"2.0\","
      "\"id\":%lld,"
      "\"error\":{"
         "\"code\":%d,"
         "\"message\":\"%s\""
      "}"
   "}";

static const char mcp_initialize_fmt[] =
   "{"
      "\"jsonrpc\":\"2.0\","
      "\"id\":%lld,"
      "\"result\":{"
         "\"protocolVersion\":\"%s\","
         "\"capabilities\":{\"tools\":{},\"resources\":{}},"
         "\"serverInfo\":{"
            "\"name\":\"retroarch-mcp-server\","
            "\"title\":\"RetroArch MCP Server\","
            "\"description\":\"Query and control RetroArch frontend\","
            "\"version\":\"%s\""
         "}"
      "}"
   "}";

static const char mcp_content_info_fmt[] =
   "{"
      "\"jsonrpc\":\"2.0\","
      "\"id\":%lld,"
      "\"result\":{"
         "\"content\":[{"
            "\"type\":\"text\","
            "\"text\":\"%s\""
         "}]"
      "}"
   "}";

static const char mcp_resources_list_fmt[] =
   "{"
      "\"jsonrpc\":\"2.0\","
      "\"id\":%lld,"
      "\"result\":{"
         "\"resources\":[]"
      "}"
   "}";

#endif /* MCP_JSON_TEMPLATES_H__ */
