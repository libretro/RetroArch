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

#ifndef __RARCH_MCP_SERVER_H
#define __RARCH_MCP_SERVER_H

#include <stdint.h>
#include <retro_common_api.h>

#include "../command.h"

RETRO_BEGIN_DECLS

/**
 * command_mcp_new:
 * @port             : TCP port to listen on.
 * @bind_address     : Address to listen on; empty means 127.0.0.1.
 * @token            : Bearer token every request must carry; the
 *                     server does not start without one.
 *
 * The MCP server, as a command interface: tools are the commands in
 * command.h, served over the Model Context Protocol's Streamable HTTP
 * transport at http://<address>:<port>/mcp.
 *
 * Returns: the interface, or NULL when it could not listen.
 **/
command_t *command_mcp_new(uint16_t port, const char *bind_address,
      const char *token);

RETRO_END_DECLS

#endif
