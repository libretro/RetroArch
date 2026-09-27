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

#ifndef LIBRETRO_DIRECT3D12_H__
#define LIBRETRO_DIRECT3D12_H__

#include <d3d12.h>
#include <d3dcompiler.h>

/* Version 1 is the interface every existing core was built against, and
 * what a frontend hands out unless a core has asked for more. Cores
 * compare interface_version against this macro, so it stays 1. */
#define RETRO_HW_RENDER_INTERFACE_D3D12_VERSION 1

/* Version 2 adds ownership of the frame: which texture the core may draw
 * into, when the frontend may read it, and when the core may have it
 * back. See "Version 2" below. A core gets it only by asking, through
 * retro_hw_render_context_negotiation_interface_d3d12. */
#define RETRO_HW_RENDER_INTERFACE_D3D12_VERSION_2 2

struct retro_hw_render_interface_d3d12
{
  /* Must be set to RETRO_HW_RENDER_INTERFACE_D3D12. */
  enum retro_hw_render_interface_type interface_type;
  /* RETRO_HW_RENDER_INTERFACE_D3D12_VERSION, or
   * RETRO_HW_RENDER_INTERFACE_D3D12_VERSION_2 if the core negotiated it.
   * The members from get_sync_index on exist only from version 2; a core
   * must not read them on a lower version. */
  unsigned interface_version;

  /* Opaque handle to the d3d12 backend in the frontend
   * which must be passed along to all function pointers
   * in this interface.
   */
  void* handle;
  ID3D12Device *device;
  ID3D12CommandQueue *queue;
  pD3DCompile D3DCompile;
  D3D12_RESOURCE_STATES required_state;

  /* Version 1 handoff: the texture holding the frame, called before
   * video_refresh(RETRO_HW_FRAME_BUFFER_VALID). It says nothing about
   * when the texture's contents are complete or when the core may draw
   * into it again, so the only thing a frontend can safely do is copy it
   * before video_refresh returns - and one that presents from another
   * thread has to do that copy on the core's thread. Still valid in
   * version 2, with the same meaning and the same cost. */
  void (*set_texture)(void* handle, ID3D12Resource* texture, DXGI_FORMAT format);

  /* --- Version 2 ------------------------------------------------------
   *
   * The core keeps one present texture per sync index and the frontend
   * tells it which to use, as the Vulkan interface does. Nothing is
   * copied to make the handoff safe; what makes it safe is that each
   * side says when it is done.
   *
   * A frame, from the core's side:
   *
   *   i = get_sync_index(handle);
   *   wait_sync_index(handle);        the frontend is done with texture[i]
   *   ... draw into texture[i], leave it in required_state, submit ...
   *   set_texture_fenced(handle, texture[i], format, fence, value);
   *   video_refresh(RETRO_HW_FRAME_BUFFER_VALID, width, height, 0);
   *
   * A core that has nothing new calls video_refresh(NULL, ...) and none
   * of the above, as always.
   */

  /* The sync index the next frame belongs to. It changes only inside
   * video_refresh(RETRO_HW_FRAME_BUFFER_VALID) and is always a bit set in
   * get_sync_index_mask(). */
  unsigned (*get_sync_index)(void* handle);

  /* Bit i set: i is an index get_sync_index() can return. The core needs
   * one present texture per set bit. The mask can change when the
   * frontend rebuilds its swapchain, which it announces the usual way:
   * context_destroy, then context_reset. */
  unsigned (*get_sync_index_mask)(void* handle);

  /* Blocks until the frontend has finished every use of the texture last
   * handed over for the current sync index - until then the core must
   * not write to it, transition it or release it. Returns at once if
   * there is none. A core may skip the call if it waits by other means
   * for the same thing, but there is no other means in this interface,
   * so in practice it calls it. */
  void (*wait_sync_index)(void* handle);

  /* The frame, for the current sync index. The texture is complete, and
   * in required_state, once @fence has reached @value; the frontend does
   * not read it before then, on whatever queue and thread it reads it.
   * A core that submits on this interface's queue can pass the fence it
   * signals after its last submit; one with nothing to wait for passes a
   * NULL fence. The frontend holds references to the texture and the
   * fence for as long as it uses them.
   *
   * Replaces set_texture for the frame: call one or the other. */
  void (*set_texture_fenced)(void* handle, ID3D12Resource* texture,
        DXGI_FORMAT format, ID3D12Fence* fence, UINT64 value);
};

/* How a core asks for version 2.
 *
 * An older frontend hands every core version 1, and an older core
 * rejects anything that is not version 1, so neither side can simply
 * start using 2. The core asks, with the negotiation interface libretro
 * already has:
 *
 *   struct retro_hw_render_context_negotiation_interface probe;
 *   probe.interface_type    = RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_D3D12;
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
#define RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_D3D12_VERSION 1

struct retro_hw_render_context_negotiation_interface_d3d12
{
  /* Must be RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_D3D12. */
  enum retro_hw_render_context_negotiation_interface_type interface_type;
  /* Must be RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_D3D12_VERSION. */
  unsigned interface_version;
  /* The highest retro_hw_render_interface_d3d12 version the core can
   * use. */
  unsigned max_render_interface_version;
};

#endif /* LIBRETRO_DIRECT3D12_H__ */
