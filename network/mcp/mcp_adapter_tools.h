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

#ifndef MCP_ADAPTER_TOOLS_H__
#define MCP_ADAPTER_TOOLS_H__

#include "mcp_adapter_utils.h"

/*
 * Tool definitions.
 *
 * Each tool has two parts:
 *   1. A JSON fragment describing the tool
 *   2. The handler function implementing the tool
 *
 * To add a new tool, add both parts here and then register it
 * in the mcp_tools[] array in mcp_adapter_tool_list.c.
 */

/* ================================================================
 * get_content_info
 * ================================================================ */

static const char tool_get_content_info_json[] =
   "{"
      "\"name\":\"get_content_info\","
      "\"title\":\"Get Content Info\","
      "\"description\":\"Get information about the currently "
         "running content in RetroArch (game name, core, system, "
         "path, CRC32). Returns content status and details "
         "if content is loaded.\","
      "\"inputSchema\":{"
         "\"type\":\"object\","
         "\"properties\":{},"
         "\"additionalProperties\":false"
      "}"
   "}";

static void mcp_tool_get_content_info(char *buf, size_t buf_size)
{
   char result[4096] = "{";
   uint8_t flags = content_get_flags();

   if (flags & CONTENT_ST_FLAG_IS_INITED)
   {
      core_info_t *core_info      = NULL;
      runloop_state_t *runloop_st = runloop_state_get_ptr();
      const char *content_path    = path_get(RARCH_PATH_CONTENT);
      const char *basename_path   = path_get(RARCH_PATH_BASENAME);
      const char *core_path       = path_get(RARCH_PATH_CORE);
      uint32_t crc                = content_get_crc();

      core_info_get_current_core(&core_info);

      mcp_json_add(result, sizeof(result), "status",
            (runloop_st && (runloop_st->flags & RUNLOOP_FLAG_PAUSED))
            ? "paused" : "playing");

      if (core_info)
      {
         if (core_info->core_name)
            mcp_json_add(result, sizeof(result), "core_name", core_info->core_name);
         if (core_info->systemname)
            mcp_json_add(result, sizeof(result), "system_name", core_info->systemname);
         if (core_info->system_id)
            mcp_json_add(result, sizeof(result), "system_id", core_info->system_id);
         if (core_info->display_name)
            mcp_json_add(result, sizeof(result), "core_display_name", core_info->display_name);
      }
      else if (runloop_st && runloop_st->system.info.library_name)
      {
         mcp_json_add(result, sizeof(result), "core_name", runloop_st->system.info.library_name);
      }

      if (content_path)
         mcp_json_add(result, sizeof(result), "content_path", content_path);

      if (basename_path)
      {
         const char *basename = path_basename(basename_path);
         if (basename)
            mcp_json_add(result, sizeof(result), "content_name", basename);
      }

      if (core_path)
         mcp_json_add(result, sizeof(result), "core_path", core_path);

      if (crc != 0)
      {
         char crc_str[16];
         snprintf(crc_str, sizeof(crc_str), "%08lx", (unsigned long)crc);
         mcp_json_add(result, sizeof(result), "crc32", crc_str);
      }
   }
   else
      mcp_json_add(result, sizeof(result), "status", "no_content");

   strlcat(result, "}", sizeof(result));

   strlcpy(buf, result, buf_size);
}

#endif /* MCP_ADAPTER_TOOLS_H__ */
