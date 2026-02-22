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

#ifndef MCP_ADAPTER_TOOL_LIST_H__
#define MCP_ADAPTER_TOOL_LIST_H__

#include <stdint.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Handler function for a single MCP tool.
 * Writes the raw JSON result object into the provided buffer.
 * The dispatcher wraps it in the JSON-RPC response envelope. */
typedef void (*mcp_tool_handler_t)(char *buf, size_t buf_size);

/* Definition of one MCP tool. */
typedef struct
{
   const char          *name;          /* tool name used for dispatch    */
   const char          *json_fragment; /* full JSON object for tools/list */
   mcp_tool_handler_t   handler;       /* implements tools/call          */
} mcp_tool_t;

/* Build the JSON-RPC response for "tools/list". */
void mcp_tools_build_list(int64_t id, char *buf, size_t buf_size);

/* Dispatch a "tools/call" request to the matching tool handler. */
void mcp_tools_call(int64_t id, const char *tool_name, char *buf, size_t buf_size);

RETRO_END_DECLS

#endif /* MCP_ADAPTER_TOOL_LIST_H__ */
