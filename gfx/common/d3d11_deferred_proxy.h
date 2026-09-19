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

/* What the ring passes to FinishCommandList as
 * RestoreDeferredContextState when it closes the core's frame.
 *
 * A core written against the immediate context binds some things once
 * and never again, because there they stay bound: the frontend's own
 * frame does not touch those slots. With FALSE the deferred context is
 * reset to the default state at every FinishCommandList, so from the
 * second frame on such a binding is gone and nothing tells the core. The
 * PS2 core's vertex-expansion buffer is bound to VS slot 0 once at
 * device creation; without it every sprite, line and point collapses,
 * the screen clears that games draw as sprites with them, and the
 * picture is black from the second frame on. TRUE keeps the deferred
 * context's state across lists, which is what the immediate context
 * the core was written for does.
 *
 * Here and not in d3d11.c so that samples/gfx/d3d11_hw_ring tests the
 * value the driver uses. */
#define D3D11_HW_RING_KEEP_CONTEXT_STATE TRUE

RETRO_END_DECLS

#endif
