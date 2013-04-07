/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

// Convenience macros.
#ifndef _RARCH_DRIVER_FUNCS_H
#define _RARCH_DRIVER_FUNCS_H

#define audio_init_func(device, rate, latency)  driver.audio->init(device, rate, latency)
#define audio_write_func(buf, size)             driver.audio->write(driver.audio_data, buf, size)
#define audio_stop_func()                       driver.audio->stop(driver.audio_data)
#define audio_start_func()                      driver.audio->start(driver.audio_data)
#define audio_set_nonblock_state_func(state)    driver.audio->set_nonblock_state(driver.audio_data, state)
#define audio_free_func()                       driver.audio->free(driver.audio_data)
#define audio_use_float_func()                  driver.audio->use_float(driver.audio_data)
#define audio_write_avail_func()                driver.audio->write_avail(driver.audio_data)
#define audio_buffer_size_func()                driver.audio->buffer_size(driver.audio_data)

#if !defined(RARCH_CONSOLE) /* Normal */

#define video_init_func(video_info, input, input_data) \
   driver.video->init(video_info, input, input_data)
#define video_frame_func(data, width, height, pitch, msg) \
   driver.video->frame(driver.video_data, data, width, height, pitch, msg)
#define video_set_nonblock_state_func(state) driver.video->set_nonblock_state(driver.video_data, state)
#define video_alive_func() driver.video->alive(driver.video_data)
#define video_focus_func() driver.video->focus(driver.video_data)
#define video_set_shader_func(type, path) driver.video->set_shader(driver.video_data, type, path)
#define video_set_rotation_func(rotate) driver.video->set_rotation(driver.video_data, rotate)
#define video_set_aspect_ratio_func(aspect_idx) driver.video->set_aspect_ratio(driver.video_data, aspect_idx)
#define video_viewport_info_func(info) driver.video->viewport_info(driver.video_data, info)
#define video_read_viewport_func(buffer) driver.video->read_viewport(driver.video_data, buffer)
#define video_overlay_interface_func(iface) driver.video->overlay_interface(driver.video_data, iface)
#define video_free_func() driver.video->free(driver.video_data)
#define input_init_func() driver.input->init()
#ifdef HAVE_ASYNC_POLL
#define input_async_poll_func() driver.input->poll(driver.input_data)
#define input_poll_func()
#else
#define input_poll_func() driver.input->poll(driver.input_data)
#define input_async_poll_func()
#endif
#define input_input_state_func(retro_keybinds, port, device, index, id) \
   driver.input->input_state(driver.input_data, retro_keybinds, port, device, index, id)
#define input_free_func() driver.input->free(driver.input_data)

static inline bool input_key_pressed_func(int key)
{
   if (driver.block_hotkey)
      return false;

   bool ret = driver.input->key_pressed(driver.input_data, key);

#ifdef HAVE_OVERLAY
   ret |= driver.overlay_state & (UINT64_C(1) << key);
#endif

#ifdef HAVE_COMMAND
   if (!ret && driver.command)
      ret = rarch_cmd_get(driver.command, key);
#endif

   return ret;
}

#else

/*============================================================
  VIDEO
  ============================================================ */

#define CONCAT2(A, B) A##B

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES) /* GL */
#define MAKENAME_VIDEO(A) CONCAT2(gl, A) 

#define video_set_aspect_ratio_func(aspectratio_idx) gfx_ctx_set_aspect_ratio(driver.video_data, aspectratio_idx)

#define gfx_ctx_window_has_focus() (true)

#elif defined(_XBOX) && (defined(HAVE_D3D8) || defined(HAVE_D3D9)) /* D3D */
#define MAKENAME_VIDEO(A) CONCAT2(xdk_d3d, A) 

#elif defined(XENON) /* XENON */
#define MAKENAME_VIDEO(A) CONCAT2(xenon360_gfx, A)

#define video_set_aspect_ratio_func(aspectratio_idx) gfx_ctx_set_aspect_ratio(driver.video_data, aspectratio_idx)

#define gfx_ctx_window_has_focus() (true)

#elif defined(GEKKO) /* Gamecube, Wii */
#define MAKENAME_VIDEO(A) CONCAT2(gx, A) 

#define video_set_aspect_ratio_func(aspectratio_idx) gx_set_aspect_ratio(driver.video_data, aspectratio_idx)
#define video_viewport_size_func(width, height) ((void)0)
#define video_read_viewport_func(buffer) (false)

//#elif defined(PSP) /* PSP1 */
//#define MAKENAME_VIDEO(A) CONCAT2(psp, A) 
//#define video_set_aspect_ratio_func(aspectratio_idx) (true)

#else /* NULL */
#define MAKENAME_VIDEO(A) CONCAT2(nullvideo, A) 

#define video_set_aspect_ratio_func(aspectratio_idx) (true)

#endif

#define video_viewport_info_func(info) driver.video->viewport_info(driver.video_data, info)

#define video_init_func(video_info, input, input_data) MAKENAME_VIDEO(_init)(video_info, input, input_data)
#define video_frame_func(data, width, height, pitch, msg) \
   MAKENAME_VIDEO(_frame)(driver.video_data, data, width, height, pitch, msg)
#define video_alive_func() MAKENAME_VIDEO(_alive)(driver.video_data)
#define video_focus_func() MAKENAME_VIDEO(_focus)(driver.video_data)
#define video_free_func() MAKENAME_VIDEO(_free)(driver.video_data)
#define video_set_nonblock_state_func(state) MAKENAME_VIDEO(_set_nonblock_state)(driver.video_data, state)
#define video_set_rotation_func(rotation)	MAKENAME_VIDEO(_set_rotation)(driver.video_data, rotation)
#define video_stop_func() MAKENAME_VIDEO(_stop)()
#define video_start_func() MAKENAME_VIDEO(_start)()
#define video_set_shader_func(type, path, mask) MAKENAME_VIDEO(_set_shader)(driver.video_data, type, path, mask)
#define video_xml_shader_func(path) driver.video->xml_shader(driver.video_data, path)

/*============================================================
  INPUT
  ============================================================ */

#if defined(_XBOX) && (defined(HAVE_D3D8) || defined(HAVE_D3D9)) /* D3D */
#define MAKENAME_INPUT(A) CONCAT2(xdk, A)
#elif defined(GEKKO) /* Gamecube, Wii */
#define MAKENAME_INPUT(A) CONCAT2(gx, A) 
#define gfx_ctx_window_has_focus() (true)
#elif defined(__CELLOS_LV2__) /* PS3 */
#define MAKENAME_INPUT(A) CONCAT2(ps3, A) 
#elif defined(ANDROID) /* ANDROID */
#define MAKENAME_INPUT(A) CONCAT2(android, A) 
#elif defined(XENON) /* XENON */
#define MAKENAME_INPUT(A) CONCAT2(xenon360, A)
#else
#define MAKENAME_INPUT(A) CONCAT2(nullinput, A) 
#endif

#define gfx_ctx_window_has_focus() (true)

#define input_init_func() MAKENAME_INPUT(_input_init)()
#define input_async_poll_func()
#define input_poll_func() MAKENAME_INPUT(_input_poll)(driver.input_data)
#define input_input_state_func(retro_keybinds, port, device, index, id) \
   MAKENAME_INPUT(_input_state)(driver.input_data, retro_keybinds, port, device, index, id)
#define input_key_pressed_func(key) MAKENAME_INPUT(_input_key_pressed)(driver.input_data, key)
#define input_free_func() MAKENAME_INPUT(_input_free_input)(driver.input_data)

#define video_overlay_interface_func(iface) driver.video->overlay_interface(driver.video_data, iface)

#endif

#endif /* _RARCH_DRIVER_FUNCS_H */
