/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#if !defined(HAVE_RSOUND) && defined(HAVE_SL)

#define audio_init_func(device, rate, latency)  sl_init(device, rate, latency)
#define audio_write_func(buf, size)             sl_write(driver.audio_data, buf, size)
#define audio_stop_func()                       sl_stop(driver.audio_data)
#define audio_start_func()                      sl_start(driver.audio_data)
#define audio_set_nonblock_state_func(state)    sl_set_nonblock_state(driver.audio_data, state)
#define audio_free_func()                       sl_free(driver.audio_data)
#define audio_use_float_func()                  driver.audio->use_float(driver.audio_data)
#define audio_write_avail_func()                sl_write_avail(driver.audio_data)
#define audio_buffer_size_func()                (BUFFER_SIZE * NUM_BUFFERS)

#else

#define audio_init_func(device, rate, latency)  driver.audio->init(device, rate, latency)
#define audio_write_func(buf, size)             driver.audio->write(driver.audio_data, buf, size)
#define audio_stop_func()                       driver.audio->stop(driver.audio_data)
#define audio_start_func()                      driver.audio->start(driver.audio_data)
#define audio_set_nonblock_state_func(state)    driver.audio->set_nonblock_state(driver.audio_data, state)
#define audio_free_func()                       driver.audio->free(driver.audio_data)
#define audio_use_float_func()                  driver.audio->use_float(driver.audio_data)
#define audio_write_avail_func()                driver.audio->write_avail(driver.audio_data)
#define audio_buffer_size_func()                driver.audio->buffer_size(driver.audio_data)

#endif

/*============================================================
	VIDEO
============================================================ */

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES) /* GL */
#define video_init_func(video_info, input, input_data) \
                                                gl_init(video_info, input, input_data)
#define video_frame_func(data, width, height, pitch, msg) \
                                                gl_frame(driver.video_data, data, width, height, pitch, msg)
#define video_set_nonblock_state_func(state) driver.video->set_nonblock_state(driver.video_data, state)
#define video_alive_func()                      gl_alive(driver.video_data)
#define video_focus_func()                      gl_focus(driver.video_data)
#define video_xml_shader_func(path)             driver.video->xml_shader(driver.video_data, path)
#define video_free_func()                       gl_free(driver.video_data)
#define video_set_rotation_func(rotation)	gl_set_rotation(driver.video_data, rotation)
#define video_set_aspect_ratio_func(aspectratio_idx) gfx_ctx_set_aspect_ratio(driver.video_data, aspectratio_idx)
#define video_stop_func()			gl_stop()
#define video_start_func()			gl_start()
#define video_set_shader_func(type, path)  gl_set_shader(driver.video_data, type, path)

#define gfx_ctx_window_has_focus()		(true)

#elif defined(_XBOX) && (defined(HAVE_D3D8) || defined(HAVE_D3D9)) /* D3D */

#define video_init_func(video_info, input, input_data) \
                                                xdk_d3d_init(video_info, input, input_data)
#define video_frame_func(data, width, height, pitch, msg) \
                                                xdk_d3d_frame(driver.video_data, data, width, height, pitch, msg)
#define video_set_nonblock_state_func(state) driver.video->set_nonblock_state(driver.video_data, state)
#define video_alive_func()                      xdk_d3d_alive(driver.video_data)
#define video_focus_func()                      xdk_d3d_focus(driver.video_data)
#define video_xml_shader_func(path)             driver.video->xml_shader(driver.video_data, path)
#define video_free_func()                       xdk_d3d_free(driver.video_data)
#define video_set_rotation_func(rotation)	xdk_d3d_set_rotation(driver.video_data, rotation)
#define video_set_aspect_ratio_func(aspectratio_idx) gfx_ctx_set_aspect_ratio(driver.video_data, aspectratio_idx)
#define video_stop_func()			xdk_d3d_stop()
#define video_start_func()			xdk_d3d_start()

#define gfx_ctx_window_has_focus()		(true)

#elif defined(GEKKO) /* Gamecube, Wii */

#define video_init_func(video_info, input, input_data) gx_init(video_info, input, input_data)
#define video_frame_func(data, width, height, pitch, msg) \
                                                gx_frame(driver.video_data, data, width, height, pitch, msg)
#define video_set_nonblock_state_func(state) gx_set_nonblock_state(driver.video_data, state)
#define video_alive_func()                      gx_alive(driver.video_data)
#define video_focus_func()                      gx_focus(driver.video_data)
#define video_xml_shader_func(path)             driver.video->xml_shader(driver.video_data, path)
#define video_free_func()                       gx_free(driver.video_data)
#define video_set_rotation_func(orientation)	gx_set_rotation(driver.video_data, orientation)
#define video_set_aspect_ratio_func(aspectratio_idx) gx_set_aspect_ratio(driver.video_data, aspectratio_idx)
#define video_stop_func()			gx_stop()
#define video_start_func()			gx_start()
#define video_viewport_size_func(width, height) ((void)0)
#define video_read_viewport_func(buffer)        (false)

#else /* NULL */
#define video_init_func(video_info, input, input_data) null_gfx_init(video_info, input, input_data)
#define video_frame_func(data, width, height, pitch, msg) \
                                                null_gfx_frame(driver.video_data, data, width, height, pitch, msg)
#define video_set_nonblock_state_func(state) null_gfx_set_nonblock_state(driver.video_data, state)
#define video_alive_func()                      null_gfx_alive(driver.video_data)
#define video_focus_func()                      null_gfx_focus(driver.video_data)
#define video_xml_shader_func(path)             driver.video->xml_shader(driver.video_data, path)
#define video_free_func()                       null_gfx_free(driver.video_data)
#define video_set_rotation_func(orientation)	(true)
#define video_set_aspect_ratio_func(aspectratio_idx) (true)
#define video_stop_func()			null_gfx_stop()
#define video_start_func()			null_gfx_start()

#endif

/*============================================================
	INPUT
============================================================ */

#if defined(_XBOX) && (defined(HAVE_D3D8) || defined(HAVE_D3D9)) /* D3D */

#define input_init_func()                       xinput_input_init()
#define input_poll_func()                       xinput_input_poll(driver.input_data)
#define input_input_state_func(retro_keybinds, port, device, index, id) \
                                                xinput_input_state(driver.input_data, retro_keybinds, port, device, index, id)
#define input_key_pressed_func(key)             xinput_input_key_pressed(driver.input_data, key)
#define input_free_func()                       xinput_input_free_input(driver.input_data)

#elif defined(GEKKO) /* Gamecube, Wii */

#define input_init_func()                       gx_input_initialize()
#define input_poll_func()                       gx_input_poll(driver.input_data)
#define input_input_state_func(retro_keybinds, port, device, index, id) \
                                                gx_input_state(driver.input_data, retro_keybinds, port, device, index, id)
#define input_key_pressed_func(key)             gx_key_pressed(driver.input_data, key)
#define input_free_func()                       gx_free_input(driver.input_data)
#define gfx_ctx_window_has_focus()		(true)

#elif defined(__CELLOS_LV2__) /* PS3 */
#define input_init_func()                       ps3_input_initialize()
#define input_poll_func()                       ps3_input_poll(driver.input_data)
#define input_input_state_func(retro_keybinds, port, device, index, id) \
                                                ps3_input_state(driver.input_data, retro_keybinds, port, device, index, id)
#define input_key_pressed_func(key)             ps3_key_pressed(driver.input_data, key)
#define input_free_func()                       ps3_free_input(driver.input_data)

#elif defined(ANDROID) /* ANDROID */
#define input_init_func()                       android_input_initialize()
#define input_poll_func()                       android_input_poll(driver.input_data)
#define input_input_state_func(retro_keybinds, port, device, index, id) \
                                                android_input_state(driver.input_data, retro_keybinds, port, device, index, id)
#define input_key_pressed_func(key)             android_input_key_pressed(driver.input_data, key)
#define input_free_func()                       android_input_free(driver.input_data)
#else

#define input_init_func()                       null_input_init()
#define input_poll_func()                       null_input_poll(driver.input_data)
#define input_input_state_func(retro_keybinds, port, device, index, id) \
                                                null_input_state(driver.input_data, retro_keybinds, port, device, index, id)
#define input_key_pressed_func(key)             null_input_key_pressed(driver.input_data, key)
#define input_free_func()                       null_input_free(driver.input_data)
#define gfx_ctx_window_has_focus()		(true)

#endif
