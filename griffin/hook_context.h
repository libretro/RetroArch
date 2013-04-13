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

#if defined(ANDROID)
#define context_get_video_size_func(win, height)     gfx_ctx_get_video_size(win, height)
#define context_update_window_title_func()        gfx_ctx_update_window_title()
#define context_destroy_func()                       gfx_ctx_destroy()

#define context_translate_aspect_func(width, height) (device_aspect)
#define context_set_resize_func(width, height)       gl->ctx_driver->set_resize(width, height)
#define context_swap_buffers_func()                  eglSwapBuffers(g_egl_dpy, g_egl_surf)
#define context_swap_interval_func(var)              eglSwapInterval(g_egl_dpy, var)
#define context_has_focus_func()                     (true)
#define context_check_window_func(quit, resize, width, height, frame_count) gfx_ctx_check_window(quit, resize, width, height, frame_count)
#define context_set_video_mode_func(width, height, fullscreen) gfx_ctx_set_video_mode(width, height, fullscreen)
#define context_input_driver_func(input, input_data) gl->ctx_driver->input_driver(input, input_data)
#endif

#ifdef HAVE_EGL
#define context_init_egl_image_buffer_func(video)    gl->ctx_driver->init_egl_image_buffer(video)

#define context_write_egl_image_func(frame, width, height, pitch, base_size, tex_index, img) gl->ctx_driver->write_egl_image(frame, width, height, pitch, base_size, tex_index,img)
#endif

#define context_post_render_func(gl)                 ((void)0)
