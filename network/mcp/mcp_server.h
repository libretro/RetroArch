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

#ifndef MCP_SERVER_H__
#define MCP_SERVER_H__

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Poll the MCP server (call once per frame from main loop).
 * Reads settings internally: starts/stops the server
 * when the enable toggle changes, and processes requests
 * while running.*/
void mcp_server_poll(void);

/* Stop and clean up the MCP server. */
void mcp_server_deinit(void);

RETRO_END_DECLS

#endif /* MCP_SERVER_H__ */
