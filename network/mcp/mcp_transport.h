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

#ifndef MCP_TRANSPORT_H__
#define MCP_TRANSPORT_H__

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Generic transport interface for MCP server.
 * Each transport implements init/poll/send/close.
 * poll is non-blocking and returns data when available. */

typedef struct mcp_transport mcp_transport_t;

typedef struct mcp_transport_interface
{
   bool (*init)(mcp_transport_t *transport, const char *bind_address, unsigned port, const char *password);

   /* poll: non-blocking check for incoming request.
    * Returns true if a complete request body is available in out_buf. */
   bool (*poll)(mcp_transport_t *transport, char *out_buf, size_t out_buf_size, size_t *out_len);

   /* send: sends a JSON response to the current client.
    * Returns true on success. */
   bool (*send)(mcp_transport_t *transport, const char *data, size_t len);

   void (*close)(mcp_transport_t *transport);
} mcp_transport_interface_t;

struct mcp_transport
{
   const mcp_transport_interface_t *iface;
   void *state;  /* transport-specific state */
};

RETRO_END_DECLS

#endif /* MCP_TRANSPORT_H__ */
