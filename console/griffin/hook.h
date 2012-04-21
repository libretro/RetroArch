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
#define audio_init_func(device, rate, latency)  driver.audio->init(device, rate, latency)
#define audio_write_func(buf, size)             driver.audio->write(driver.audio_data, buf, size)
#define audio_stop_func()                       driver.audio->stop(driver.audio_data)
#define audio_start_func()                      driver.audio->start(driver.audio_data)
#define audio_set_nonblock_state_func(state)    driver.audio->set_nonblock_state(driver.audio_data, state)
#define audio_free_func()                       driver.audio->free(driver.audio_data)
#define audio_use_float_func()                  driver.audio->use_float(driver.audio_data)
#define audio_write_avail_func()                driver.audio->write_avail(driver.audio_data)
#define audio_buffer_size_func()                driver.audio->buffer_size(driver.audio_data)

/*============================================================
	PLAYSTATION3
============================================================ */

#ifdef __CELLOS_LV2__
#define video_init_func(video_info, input, input_data) \
                                                gl_init(video_info, input, input_data)
#define video_frame_func(data, width, height, pitch, msg) \
                                                gl_frame(driver.video_data, data, width, height, pitch, msg)
#define video_set_nonblock_state_func(state) driver.video->set_nonblock_state(driver.video_data, state)
#define video_alive_func()                      gl_alive(driver.video_data)
#define video_focus_func()                      gl_focus(driver.video_data)
#define video_xml_shader_func(path)             driver.video->xml_shader(driver.video_data, path)
#define video_free_func()                       gl_free(driver.video_data)
#define video_set_rotation_func(orientation)	ps3graphics_set_orientation(driver.video_data, orientation)
#define video_set_aspect_ratio_func(aspectratio_idx) ps3graphics_set_aspect_ratio(driver.video_data, aspectratio_idx)

#define input_init_func()                       ps3_input_initialize()
#define input_poll_func()                       ps3_input_poll(driver.input_data)
#define input_input_state_func(snes_keybinds, port, device, index, id) \
                                                ps3_input_state(driver.input_data, snes_keybinds, port, device, index, id)
#define input_key_pressed_func(key)             ps3_key_pressed(driver.input_data, key)
#define input_free_func()                       ps3_free_input(driver.input_data)

/*============================================================
	XBOX 360
============================================================ */

#elif defined(_XBOX)

#define video_init_func(video_info, input, input_data) \
                                                xdk360_gfx_init(video_info, input, input_data)
#define video_frame_func(data, width, height, pitch, msg) \
                                                xdk360_gfx_frame(driver.video_data, data, width, height, pitch, msg)
#define video_set_nonblock_state_func(state) driver.video->set_nonblock_state(driver.video_data, state)
#define video_alive_func()                      xdk360_gfx_alive(driver.video_data)
#define video_focus_func()                      xdk360_gfx_focus(driver.video_data)
#define video_xml_shader_func(path)             driver.video->xml_shader(driver.video_data, path)
#define video_free_func()                       xdk360_gfx_free(driver.video_data)
#define video_set_rotation_func(orientation)	xdk360_set_orientation(driver.video_data, orientation)
#define video_set_aspect_ratio_func(aspectratio_idx) xdk360_set_aspect_ratio(driver.video_data, aspectratio_idx)

#define input_init_func()                       xdk360_input_initialize()
#define input_poll_func()                       xdk360_input_poll(driver.input_data)
#define input_input_state_func(snes_keybinds, port, device, index, id) \
                                                xdk360_input_state(driver.input_data, snes_keybinds, port, device, index, id)
#define input_key_pressed_func(key)             xdk360_key_pressed(driver.input_data, key)
#define input_free_func()                       xdk360_free_input(driver.input_data)

/*============================================================
	GAMECUBE / WII
============================================================ */

#elif defined(GEKKO)

#define video_init_func(video_info, input, input_data) wii_init(video_info, input, input_data)
#define video_frame_func(data, width, height, pitch, msg) \
                                                wii_frame(driver.video_data, data, width, height, pitch, msg)
#define video_set_nonblock_state_func(state) wii_set_nonblock_state(driver.video_data, state)
#define video_alive_func()                      wii_alive(driver.video_data)
#define video_focus_func()                      wii_focus(driver.video_data)
#define video_xml_shader_func(path)             driver.video->xml_shader(driver.video_data, path)
#define video_free_func()                       wii_free(driver.video_data)
#define video_set_rotation_func(orientation)	wii_set_rotation(driver.video_data, orientation)
#define video_set_aspect_ratio_func(aspectratio_idx) wii_set_aspect_ratio(driver.video_data, aspectratio_idx)

#define input_init_func()                       wii_input_initialize()
#define input_poll_func()                       wii_input_poll(driver.input_data)
#define input_input_state_func(snes_keybinds, port, device, index, id) \
                                                wii_input_state(driver.input_data, snes_keybinds, port, device, index, id)
#define input_key_pressed_func(key)             wii_key_pressed(driver.input_data, key)
#define input_free_func()                       wii_free_input(driver.input_data)

#endif
