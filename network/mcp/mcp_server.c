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

#include <boolean.h>

#include "mcp_server.h"
#include "mcp_defines.h"
#include "mcp_transport.h"
#include "mcp_http_transport.h"
#include "mcp_adapter.h"
#include "../../configuration.h"
#include "../../verbosity.h"

static mcp_transport_t mcp_transport;
static bool            mcp_active = false;

static bool mcp_server_init(const char *bind_address, unsigned port, const char *password)
{
   if (mcp_active)
      return true;

   mcp_transport.iface = &mcp_http_transport;
   mcp_transport.state = NULL;

   if (!mcp_transport.iface->init(&mcp_transport, bind_address, port, password))
   {
      RARCH_ERR("[MCP] Failed to initialize transport\n");
      return false;
   }

   mcp_active = true;
   RARCH_LOG("[MCP] Server started\n");
   return true;
}

void mcp_server_poll(void)
{
   settings_t *settings = config_get_ptr();

   if (settings->bools.mcp_server_enable)
   {
      if (!mcp_active)
      {
         RARCH_DBG("[MCP] Initializing server on %s:%u\n",
               settings->arrays.mcp_server_address,
               settings->uints.mcp_server_port);
         mcp_server_init(
               settings->arrays.mcp_server_address,
               settings->uints.mcp_server_port,
               settings->arrays.mcp_server_password);
      }

      if (mcp_active)
      {
         char request[MCP_JSON_MAX_REQUEST];
         size_t request_len = 0;

         if (mcp_transport.iface->poll(&mcp_transport, request, sizeof(request), &request_len))
         {
            char response[MCP_JSON_MAX_RESPONSE];
            RARCH_DBG("[MCP] Request received (%u bytes)\n", (unsigned)request_len);
            mcp_adapter_handle_request( request, request_len, response, sizeof(response));

            if (response[0])
            {
               RARCH_DBG("[MCP] Sending response (%u bytes)\n", (unsigned)strlen(response));
               mcp_transport.iface->send(&mcp_transport, response, strlen(response));
            }
         }
      }
   }
   else if (mcp_active)
      mcp_server_deinit();
}

void mcp_server_deinit(void)
{
   if (!mcp_active)
      return;

   mcp_transport.iface->close(&mcp_transport);
   mcp_active = false;
   RARCH_LOG("[MCP] Server stopped\n");
}
