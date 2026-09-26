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

#ifndef __RARCH_TLS_CONFIG_H
#define __RARCH_TLS_CONFIG_H

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* TLS certificate-verification policy for outbound HTTPS connections
 * (cloud sync, cheevos, online updater), selected by the
 * tls_verify_mode setting (settings->uints.tls_verify_mode).
 *
 * REQUIRED is 0 so a fresh / unset / default-initialised config lands on
 * the safe value. This numbering is RA-side; ssl_socket_set_verify_mode()
 * translates it into the backend's native authmode. */
enum tls_verify_mode
{
   TLS_VERIFY_REQUIRED = 0,  /* reject bad certs (default, fail-closed) */
   TLS_VERIFY_OPTIONAL = 1,  /* verify, but proceed + log on failure     */
   TLS_VERIFY_DISABLED = 2,  /* skip verification, warn every connect     */
   TLS_VERIFY_MODE_LAST      /* count sentinel (one past the last mode)   */
};

RETRO_END_DECLS

#endif
