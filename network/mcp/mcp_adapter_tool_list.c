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

#include "mcp_adapter_tool_list.h"
#include "mcp_defines.h"
#include "mcp_json_templates.h"
#include "../../retroarch.h"
#include "../../core_info.h"
#include "../../runloop.h"
#include "../../content.h"
#include "../../paths.h"
#include "../../verbosity.h"
#include "../../command.h"
#include "../../configuration.h"
#include "../../version.h"
#include "../../gfx/video_driver.h"
#include "../../audio/audio_driver.h"

#include "mcp_adapter_tools.h"

/* ---- tool registry ----
 *
 * To add a new tool:
 *   1. Define its JSON fragment and handler in mcp_adapter_tools.h
 *   2. Add an entry to this array
 */

static const mcp_tool_t mcp_tools[] = {
   { "get_content_info",      tool_get_content_info_json,      mcp_tool_get_content_info },
   { "get_status",            tool_get_status_json,            mcp_tool_get_status },
   { "pause_resume",          tool_pause_resume_json,          mcp_tool_pause_resume },
   { "reset",                 tool_reset_json,                 mcp_tool_reset },
   { "save_state",            tool_save_state_json,            mcp_tool_save_state },
   { "load_state",            tool_load_state_json,            mcp_tool_load_state },
};

static const size_t mcp_tools_count = sizeof(mcp_tools) / sizeof(mcp_tools[0]);

/* ---- public API ---- */

void mcp_tools_build_list(int64_t id, char *buf, size_t buf_size)
{
   size_t pos = 0;
   size_t i;

   pos += snprintf(buf + pos, buf_size - pos,
         "{\"jsonrpc\":\"2.0\",\"id\":%lld,"
         "\"result\":{\"tools\":[",
         (long long)id);

   for (i = 0; i < mcp_tools_count; i++)
   {
      if (i > 0)
         buf[pos++] = ',';
      pos += strlcpy(buf + pos, mcp_tools[i].json_fragment, buf_size - pos);
   }

   strlcpy(buf + pos, "]}}", buf_size - pos);
}

void mcp_tools_call(int64_t id, const char *tool_name,
      const char *args_json, size_t args_len,
      char *buf, size_t buf_size)
{
   char normalized[128];
   size_t i;

   /* normalize dots to underscores (vscode fails if tool names have dots) */
   strlcpy(normalized, tool_name, sizeof(normalized));
   for (i = 0; normalized[i]; i++)
   {
      if (normalized[i] == '.')
         normalized[i] = '_';
   }

   for (i = 0; i < mcp_tools_count; i++)
   {
      if (string_is_equal(normalized, mcp_tools[i].name))
      {
         char escaped[MCP_JSON_MAX_RESPONSE];
         mcp_tools[i].handler(args_json, args_len, buf, buf_size);
         mcp_json_escape(escaped, sizeof(escaped), buf);
         snprintf(buf, buf_size, mcp_content_info_fmt, (long long)id, escaped);
         return;
      }
   }

   /* unknown tool */
   snprintf(buf, buf_size, mcp_error_fmt, (long long)id, -32601, "Unknown tool");
}
