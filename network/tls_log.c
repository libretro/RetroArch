/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <net/net_socket_ssl.h>

#include "../verbosity.h"

/* Strong overrides of the weak logging hooks declared in net_socket_ssl.h.
 * Routing TLS verification outcomes into the RetroArch log lives here, on the
 * RA side, so verbosity.h never has to be pulled into vendored libretro-common
 * (which ships with every libretro core and may be built standalone). */

void ssl_socket_log_verify_fail(int mode_required, const char *domain,
      const char *verify_info)
{
   if (mode_required)
      RARCH_ERR("[TLS] Cert verification failed for %s: %s\n",
            domain      ? domain      : "(unknown)",
            verify_info ? verify_info : "");
   else
      RARCH_WARN("[TLS] Cert verification soft-failed for %s: %s\n",
            domain      ? domain      : "(unknown)",
            verify_info ? verify_info : "");
}

void ssl_socket_log_verify_disabled(const char *domain)
{
   (void)domain;
   RARCH_WARN("[TLS] Certificate verification disabled - connections vulnerable to MITM.\n");
}
