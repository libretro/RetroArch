/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2018 - Ali Bouhlel
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

#include <assert.h>
#include <string/stdstring.h>

#include "driver.h"
#include "verbosity.h"
#include "configuration.h"
#include "gfx/video_driver.h"
#include "gfx/common/win32_common.h"
#include "gfx/common/dxgi_common.h"
#include "gfx/common/d3d12_common.h"
#include "gfx/common/d3dcompiler_common.h"

static void d3d12_set_filtering(void* data, unsigned index, bool smooth)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   if (smooth)
      d3d12->frame.sampler = d3d12->sampler_linear;
   else
      d3d12->frame.sampler = d3d12->sampler_nearest;
}

static void*
d3d12_gfx_init(const video_info_t* video, const input_driver_t** input, void** input_data)
{
   WNDCLASSEX      wndclass = { 0 };
   settings_t*     settings = config_get_ptr();
   gfx_ctx_input_t inp      = { input, input_data };
   d3d12_video_t*  d3d12    = (d3d12_video_t*)calloc(1, sizeof(*d3d12));

   if (!d3d12)
      return NULL;

   win32_window_reset();
   win32_monitor_init();
   wndclass.lpfnWndProc = WndProcD3D;
   win32_window_init(&wndclass, true, NULL);

   if (!win32_set_video_mode(d3d12, video->width, video->height, video->fullscreen))
   {
      RARCH_ERR("[D3D12]: win32_set_video_mode failed.\n");
      goto error;
   }

   gfx_ctx_d3d.input_driver(NULL, settings->arrays.input_joypad_driver, input, input_data);

   if (!d3d12_init_base(d3d12))
      goto error;

   if (!d3d12_init_descriptors(d3d12))
      goto error;

   if (!d3d12_init_pipeline(d3d12))
      goto error;

   if (!d3d12_init_queue(d3d12))
      goto error;

   if (!d3d12_init_swapchain(d3d12, video->width, video->height, main_window.hwnd))
      goto error;

   d3d12_create_fullscreen_quad_vbo(d3d12->device, &d3d12->frame.vbo_view, &d3d12->frame.vbo);
   d3d12_create_fullscreen_quad_vbo(d3d12->device, &d3d12->menu.vbo_view, &d3d12->menu.vbo);

   d3d12_set_filtering(d3d12, 0, video->smooth);

   d3d12->chain.vsync = video->vsync;
   d3d12->format      = video->rgb32 ? DXGI_FORMAT_B8G8R8X8_UNORM : DXGI_FORMAT_B5G6R5_UNORM;
   d3d12->frame.texture.desc.Format = d3d12_get_closest_match_texture2D(d3d12->device, d3d12->format);

   return d3d12;

error:
   RARCH_ERR("[D3D12]: failed to init video driver.\n");
   free(d3d12);
   return NULL;
}

static bool d3d12_gfx_frame(
      void*               data,
      const void*         frame,
      unsigned            width,
      unsigned            height,
      uint64_t            frame_count,
      unsigned            pitch,
      const char*         msg,
      video_frame_info_t* video_info)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   (void)msg;
   PERF_START();
   D3D12ResetCommandAllocator(d3d12->queue.allocator);

   D3D12ResetGraphicsCommandList(d3d12->queue.cmd, d3d12->queue.allocator, d3d12->pipe.handle);
   D3D12SetGraphicsRootSignature(d3d12->queue.cmd, d3d12->pipe.rootSignature);
   {
      D3D12DescriptorHeap desc_heaps[] = { d3d12->pipe.srv_heap.handle,
                                           d3d12->pipe.sampler_heap.handle };
      D3D12SetDescriptorHeaps(d3d12->queue.cmd, countof(desc_heaps), desc_heaps);
   }

   d3d12_resource_transition(
         d3d12->queue.cmd, d3d12->chain.renderTargets[d3d12->chain.frame_index],
         D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

   D3D12RSSetViewports(d3d12->queue.cmd, 1, &d3d12->chain.viewport);
   D3D12RSSetScissorRects(d3d12->queue.cmd, 1, &d3d12->chain.scissorRect);
   D3D12OMSetRenderTargets(
         d3d12->queue.cmd, 1, &d3d12->chain.desc_handles[d3d12->chain.frame_index], FALSE, NULL);
   D3D12ClearRenderTargetView(
         d3d12->queue.cmd, d3d12->chain.desc_handles[d3d12->chain.frame_index],
         d3d12->chain.clearcolor, 0, NULL);

   D3D12IASetPrimitiveTopology(d3d12->queue.cmd, D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

   if (data && width && height)
   {
      if (!d3d12->frame.texture.handle || (d3d12->frame.texture.desc.Width != width) ||
          (d3d12->frame.texture.desc.Height != height))
      {
         d3d12->frame.texture.desc.Width  = width;
         d3d12->frame.texture.desc.Height = height;
         d3d12_init_texture(
               d3d12->device, &d3d12->pipe.srv_heap, SRV_HEAP_SLOT_FRAME_TEXTURE,
               &d3d12->frame.texture);
      }
      d3d12_update_texture(width, height, pitch, d3d12->format, frame, &d3d12->frame.texture);

      d3d12_upload_texture(d3d12->queue.cmd, &d3d12->frame.texture);
   }

   d3d12_set_texture(d3d12->queue.cmd, &d3d12->frame.texture);
   d3d12_set_sampler(d3d12->queue.cmd, d3d12->frame.sampler);
   D3D12IASetVertexBuffers(d3d12->queue.cmd, 0, 1, &d3d12->frame.vbo_view);
   D3D12DrawInstanced(d3d12->queue.cmd, 4, 1, 0, 0);

   if (d3d12->menu.enabled && d3d12->menu.texture.handle)
   {
      if (d3d12->menu.texture.dirty)
         d3d12_upload_texture(d3d12->queue.cmd, &d3d12->menu.texture);

      d3d12_set_texture(d3d12->queue.cmd, &d3d12->menu.texture);
      d3d12_set_sampler(d3d12->queue.cmd, d3d12->menu.sampler);
      D3D12IASetVertexBuffers(d3d12->queue.cmd, 0, 1, &d3d12->menu.vbo_view);
      D3D12DrawInstanced(d3d12->queue.cmd, 4, 1, 0, 0);
   }

   d3d12_resource_transition(
         d3d12->queue.cmd, d3d12->chain.renderTargets[d3d12->chain.frame_index],
         D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
   D3D12CloseGraphicsCommandList(d3d12->queue.cmd);

   D3D12ExecuteGraphicsCommandLists(d3d12->queue.handle, 1, &d3d12->queue.cmd);

#if 1
   DXGIPresent(d3d12->chain.handle, !!d3d12->chain.vsync, 0);
#else
   DXGI_PRESENT_PARAMETERS pp = { 0 };
   DXGIPresent1(d3d12->swapchain, 0, 0, &pp);
#endif

   /* wait_for_previous_frame */
   D3D12SignalCommandQueue(d3d12->queue.handle, d3d12->queue.fence, d3d12->queue.fenceValue);

   if (D3D12GetCompletedValue(d3d12->queue.fence) < d3d12->queue.fenceValue)
   {
      D3D12SetEventOnCompletion(
            d3d12->queue.fence, d3d12->queue.fenceValue, d3d12->queue.fenceEvent);
      WaitForSingleObject(d3d12->queue.fenceEvent, INFINITE);
   }

   d3d12->queue.fenceValue++;
   d3d12->chain.frame_index = DXGIGetCurrentBackBufferIndex(d3d12->chain.handle);
   PERF_STOP();

   if (msg && *msg)
      gfx_ctx_d3d.update_window_title(NULL, video_info);

   return true;
}

static void d3d12_gfx_set_nonblock_state(void* data, bool toggle)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;
   d3d12->chain.vsync   = !toggle;
}

static bool d3d12_gfx_alive(void* data)
{
   (void)data;
   bool     quit;
   bool     resize;
   unsigned width;
   unsigned height;

   win32_check_window(&quit, &resize, &width, &height);

   if (width != 0 && height != 0)
      video_driver_set_size(&width, &height);

   return !quit;
}

static bool d3d12_gfx_focus(void* data) { return win32_has_focus(); }

static bool d3d12_gfx_suppress_screensaver(void* data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool d3d12_gfx_has_windowed(void* data)
{
   (void)data;
   return true;
}

static void d3d12_gfx_free(void* data)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   Release(d3d12->frame.vbo);
   Release(d3d12->frame.texture.handle);
   Release(d3d12->frame.texture.upload_buffer);
   Release(d3d12->menu.vbo);
   Release(d3d12->menu.texture.handle);
   Release(d3d12->menu.texture.upload_buffer);

   Release(d3d12->pipe.sampler_heap.handle);
   Release(d3d12->pipe.srv_heap.handle);
   Release(d3d12->pipe.rtv_heap.handle);
   Release(d3d12->pipe.rootSignature);
   Release(d3d12->pipe.handle);

   Release(d3d12->queue.fence);
   Release(d3d12->chain.renderTargets[0]);
   Release(d3d12->chain.renderTargets[1]);
   Release(d3d12->chain.handle);

   Release(d3d12->queue.cmd);
   Release(d3d12->queue.allocator);
   Release(d3d12->queue.handle);

   Release(d3d12->factory);
   Release(d3d12->device);
   Release(d3d12->adapter);

   win32_monitor_from_window();
   win32_destroy_window();

   free(d3d12);
}

static bool d3d12_gfx_set_shader(void* data, enum rarch_shader_type type, const char* path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

static void d3d12_gfx_set_rotation(void* data, unsigned rotation)
{
   (void)data;
   (void)rotation;
}

static void d3d12_gfx_viewport_info(void* data, struct video_viewport* vp)
{
   (void)data;
   (void)vp;
}

static bool d3d12_gfx_read_viewport(void* data, uint8_t* buffer, bool is_idle)
{
   (void)data;
   (void)buffer;

   return true;
}

static void d3d12_set_menu_texture_frame(
      void* data, const void* frame, bool rgb32, unsigned width, unsigned height, float alpha)
{
   d3d12_video_t* d3d12  = (d3d12_video_t*)data;
   int            pitch  = width * (rgb32 ? sizeof(uint32_t) : sizeof(uint16_t));
   DXGI_FORMAT    format = rgb32 ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_EX_A4R4G4B4_UNORM;

   if (d3d12->menu.texture.desc.Width != width || d3d12->menu.texture.desc.Height != height)
   {
      d3d12->menu.texture.desc.Width  = width;
      d3d12->menu.texture.desc.Height = height;
      d3d12->menu.texture.desc.Format = d3d12_get_closest_match_texture2D(d3d12->device, format);
      d3d12_init_texture(
            d3d12->device, &d3d12->pipe.srv_heap, SRV_HEAP_SLOT_MENU_TEXTURE, &d3d12->menu.texture);
   }

   d3d12_update_texture(width, height, pitch, format, frame, &d3d12->menu.texture);

   d3d12->menu.alpha = alpha;

   {
      D3D12_RANGE     read_range = { 0, 0 };
      d3d12_vertex_t* v;

      D3D12Map(d3d12->menu.vbo, 0, &read_range, &v);
      v[0].color[3] = alpha;
      v[1].color[3] = alpha;
      v[2].color[3] = alpha;
      v[3].color[3] = alpha;
      D3D12Unmap(d3d12->menu.vbo, 0, NULL);
   }
   d3d12->menu.sampler = config_get_ptr()->bools.menu_linear_filter ? d3d12->sampler_linear
                                                                    : d3d12->sampler_nearest;
}
static void d3d12_set_menu_texture_enable(void* data, bool state, bool full_screen)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   d3d12->menu.enabled    = state;
   d3d12->menu.fullscreen = full_screen;
}

static const video_poke_interface_t d3d12_poke_interface = {
   NULL,                          /* set_coords */
   NULL,                          /* set_mvp */
   NULL,                          /* load_texture */
   NULL,                          /* unload_texture */
   NULL,                          /* set_video_mode */
   NULL,                          /* set_filtering */
   NULL,                          /* get_video_output_size */
   NULL,                          /* get_video_output_prev */
   NULL,                          /* get_video_output_next */
   NULL,                          /* get_current_framebuffer */
   NULL,                          /* get_proc_address */
   NULL,                          /* set_aspect_ratio */
   NULL,                          /* apply_state_changes */
   d3d12_set_menu_texture_frame,  /* set_texture_frame */
   d3d12_set_menu_texture_enable, /* set_texture_enable */
   NULL,                          /* set_osd_msg */
   NULL,                          /* show_mouse */
   NULL,                          /* grab_mouse_toggle */
   NULL,                          /* get_current_shader */
   NULL,                          /* get_current_software_framebuffer */
   NULL,                          /* get_hw_render_interface */
};

static void d3d12_gfx_get_poke_interface(void* data, const video_poke_interface_t** iface)
{
   *iface = &d3d12_poke_interface;
}

video_driver_t video_d3d12 = {
   d3d12_gfx_init,
   d3d12_gfx_frame,
   d3d12_gfx_set_nonblock_state,
   d3d12_gfx_alive,
   d3d12_gfx_focus,
   d3d12_gfx_suppress_screensaver,
   d3d12_gfx_has_windowed,
   d3d12_gfx_set_shader,
   d3d12_gfx_free,
   "d3d12",
   NULL, /* set_viewport */
   d3d12_gfx_set_rotation,
   d3d12_gfx_viewport_info,
   d3d12_gfx_read_viewport,
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
   NULL, /* overlay_interface */
#endif
   d3d12_gfx_get_poke_interface,
};
