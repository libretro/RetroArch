/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - libretroadmin
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

#ifndef RARCH_D3D11_DEFERRED_PROXY_H__
#define RARCH_D3D11_DEFERRED_PROXY_H__

#define CINTERFACE
#define COBJMACROS
#include <d3d11.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* See d3d11_deferred_proxy.c. Takes a reference on the deferred context;
 * the proxy's own Release drops it. The core drives the proxy; the
 * driver reaches the deferred context underneath with _real(). */
ID3D11DeviceContext *d3d11_deferred_proxy_new(ID3D11DeviceContext *deferred);
ID3D11DeviceContext *d3d11_deferred_proxy_real(ID3D11DeviceContext *proxy);

RETRO_END_DECLS

#endif
