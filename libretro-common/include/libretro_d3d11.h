/* Copyright (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------------
 * The following license statement only applies to this libretro API header (libretro_d3d.h)
 * ---------------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the
 * "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBRETRO_DIRECT3D11_H__
#define LIBRETRO_DIRECT3D11_H__

#include <d3d11.h>
#include <d3dcompiler.h>

/* Version 1 is the interface every existing core was built against, and
 * what a frontend hands out unless a core has asked for more. Cores
 * compare interface_version against this macro, so it stays 1. */
#define RETRO_HW_RENDER_INTERFACE_D3D11_VERSION 1

/* Version 2 lets the core and a frontend that presents from another
 * thread share the one immediate context a D3D11 device has, by taking
 * turns; it names the frame's texture outright; and it carries a sync
 * index, so the frame is handed over without being copied. See
 * "Version 2" below. A core gets it only by asking, through
 * retro_hw_render_context_negotiation_interface_d3d11. */
#define RETRO_HW_RENDER_INTERFACE_D3D11_VERSION_2 2

struct retro_hw_render_interface_d3d11
{
  /* Must be set to RETRO_HW_RENDER_INTERFACE_D3D11. */
  enum retro_hw_render_interface_type interface_type;
  /* RETRO_HW_RENDER_INTERFACE_D3D11_VERSION, or
   * RETRO_HW_RENDER_INTERFACE_D3D11_VERSION_2 if the core negotiated
   * it. The members from lock_context on exist only from version 2; a
   * core must not read them on version 1. */
  unsigned interface_version;

  /* Opaque handle to the d3d11 backend in the frontend
   * which must be passed along to all function pointers
   * in this interface.
   */
  void* handle;
  ID3D11Device *device;

  /* Version 1: the context the core draws with, and the frame is the
   * shader resource view it leaves bound at pixel shader slot 0 when it
   * calls video_refresh(RETRO_HW_FRAME_BUFFER_VALID). A D3D11 device has
   * one immediate context and it is not safe to use from two threads at
   * once, so this works only while the frontend draws on the thread that
   * runs the core. A frontend that presents from another thread cannot
   * share it under version 1's rules; it can only hand the core a
   * deferred context, which records but cannot read anything back - no
   * Map for reading, no query results.
   *
   * Version 2: always the device's immediate context, whatever thread
   * the frontend presents from. The core may use it only between
   * lock_context and unlock_context. */
  ID3D11DeviceContext *context;
  D3D_FEATURE_LEVEL featureLevel;
  pD3DCompile D3DCompile;

  /* --- Version 2 ------------------------------------------------------
   *
   * The immediate context is shared by taking turns. Whoever holds the
   * lock has the context to itself and everything D3D11 can do with it,
   * reading back included.
   *
   * A frame, from the core's side:
   *
   *   if (lock_context(handle))
   *      ... the frontend has had the context: rebind everything ...
   *   ... draw, read back, whatever the frame needs ...
   *   set_texture(handle, texture);
   *   unlock_context(handle);
   *   video_refresh(RETRO_HW_FRAME_BUFFER_VALID, width, height, 0);
   *
   * A core is free to lock and unlock as often as it likes within a
   * frame, and should hold the lock only around the work that uses the
   * context: the frontend's thread cannot start its frame while the core
   * has it. It must not hold it across a return from retro_run, and it
   * must not hold it across video_refresh: video_refresh may wait for
   * the frontend's other thread, and that thread may be waiting for the
   * lock.
   */

  /* Takes the context. Blocks while the frontend is using it. Recursive:
   * a thread that holds the lock may take it again, and must release it
   * as many times.
   *
   * Returns true if the frontend has used the context since this thread
   * last released it. Every binding and every piece of pipeline state is
   * then whatever the frontend left, and the core must set again
   * whatever it relies on before it draws - shaders, input layout,
   * buffers, views, samplers, render targets, viewport, scissor, blend,
   * depth-stencil and rasterizer state. Returns false if the context is
   * exactly as the core left it.
   *
   * video_refresh is the frontend's too: a frontend that draws on the
   * core's thread draws its whole frame inside it, and the core's next
   * lock_context says so. */
  bool (*lock_context)(void* handle);

  /* Gives the context back. The core must not touch it again until it
   * has taken the lock again. */
  void (*unlock_context)(void* handle);

  /* The frame, called with the lock held, before the core releases it
   * and calls video_refresh(RETRO_HW_FRAME_BUFFER_VALID).
   *
   * Replaces the pixel shader slot 0 convention, which a version 2 core
   * need not follow. NULL withdraws a texture handed over earlier. */
  void (*set_texture)(void* handle, ID3D11Texture2D* texture);

  /* --- The sync index -------------------------------------------------
   *
   * The frontend does not copy the frame anywhere: the core keeps one
   * present texture per sync index, the frontend reads the texture
   * itself, whenever its own thread gets to it, and says when it is
   * done. libretro_d3d12.h version 2 and the Vulkan interface work the
   * same way.
   *
   *   i = get_sync_index(handle);
   *   wait_sync_index(handle);          NOT with the lock held
   *   lock_context ... draw into texture[i] ... set_texture(texture[i]);
   *   unlock_context(handle);
   *   video_refresh(RETRO_HW_FRAME_BUFFER_VALID, width, height, 0);
   *
   * From set_texture until wait_sync_index next returns for the same
   * index, the texture is the frontend's: the core must not draw into
   * it, and must keep it alive. There is no fence, as there is in D3D12:
   * the device has one context, and what the frontend has issued on it
   * is ahead of whatever the core issues next.
   */

  /* The sync index the next frame belongs to. It changes only inside
   * video_refresh(RETRO_HW_FRAME_BUFFER_VALID) and is always a bit set in
   * get_sync_index_mask(). */
  unsigned (*get_sync_index)(void* handle);

  /* Bit i set: i is an index get_sync_index() can return. The core needs
   * one present texture per set bit. */
  unsigned (*get_sync_index_mask)(void* handle);

  /* Blocks until the frontend has finished with the texture last handed
   * over for the current sync index. Returns at once if there is none.
   * Must not be called with the lock held: the frontend's thread needs
   * the lock to finish. */
  void (*wait_sync_index)(void* handle);
};

/* How a core asks for version 2.
 *
 * An older frontend hands every core version 1, and an older core
 * rejects anything that is not version 1, so neither side can simply
 * start using 2. The core asks, with the negotiation interface libretro
 * already has:
 *
 *   struct retro_hw_render_context_negotiation_interface probe;
 *   probe.interface_type    = RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_D3D11;
 *   probe.interface_version = 0;
 *   if (   environ_cb(RETRO_ENVIRONMENT_GET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_SUPPORT, &probe)
 *       && probe.interface_version >= 1)
 *      environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE, &negotiation);
 *
 * where negotiation is one of these, alive until retro_deinit. A
 * frontend that does not know the type answers version 0 and the core
 * goes on with version 1, having asked for nothing. One that does hands
 * out min(max_render_interface_version, what it implements) from
 * RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE, and the core reads
 * interface_version to find out which it got. */
#define RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_D3D11_VERSION 1

struct retro_hw_render_context_negotiation_interface_d3d11
{
  /* Must be RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_D3D11. */
  enum retro_hw_render_context_negotiation_interface_type interface_type;
  /* Must be RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_D3D11_VERSION. */
  unsigned interface_version;
  /* The highest retro_hw_render_interface_d3d11 version the core can
   * use. */
  unsigned max_render_interface_version;
};

#endif /* LIBRETRO_DIRECT3D11_H__ */
