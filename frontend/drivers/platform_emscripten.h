/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2017 - Daniel De Matteis
 * Copyright (C) 2012-2015 - Michael Lelli
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _PLATFORM_EMSCRIPTEN_H
#define _PLATFORM_EMSCRIPTEN_H

#include <boolean.h>

/**
 * Synchronously run a function on the browser thread.
 *
 * @param func Function to call
 * @param arg Argument to pass to \c func
 */
void platform_emscripten_run_on_browser_thread_sync(void (*func)(void*), void* arg);

/**
 * Asynchronously run a function on the browser thread. Returns immediately.
 *
 * @param func Function to call
 * @param arg Argument to pass to \c func
 */
void platform_emscripten_run_on_browser_thread_async(void (*func)(void*), void* arg);

/**
 * Asynchronously run a function on the program thread. Returns immediately.
 *
 * @param func Function to call
 * @param arg Argument to pass to \c func
 */
void platform_emscripten_run_on_program_thread_async(void (*func)(void*), void* arg);

/**
 * Send command reply.
 *
 * @param msg Reply string
 * @param len Length of \c msg
 */
void platform_emscripten_command_reply(const char *msg, size_t len);

/**
 * Read a command from the command queue.
 *
 * @param into Pointer to store command string
 * @param max_len Length of \c *into
 * @return Length of stored command.
 * Returns 0 if there are no commands in queue.
 */
size_t platform_emscripten_command_read(char **into, size_t max_len);

/**
 * Get the real screen dimensions of the canvas on the page.
 *
 * @param width Pointer to store canvas width
 * @param height Pointer to store canvas height
 */
void platform_emscripten_get_canvas_size(int *width, int *height);

/**
 * Get the ratio of CSS pixels to real screen pixels. Useful for input scaling.
 *
 * @return devicePixelRatio
 */
double platform_emscripten_get_dpr(void);

/**
 * Check if the browser supports Atomics.waitAsync.
 *
 * @return True if async atomics are available.
 */
bool platform_emscripten_has_async_atomics(void);

/**
 * Check if the window is hidden.
 * 
 * @return True if the window is hidden.
 */
bool platform_emscripten_is_window_hidden(void);

/**
 * Whether the frame should be dropped.
 * Currently returns true if the window is hidden and vsync is on.
 *
 * @return True if the frame should be dropped.
 */
bool platform_emscripten_should_drop_iter(void);

/**
 * Block until a vsync interval, if vsync is enabled.
 * PROXY_TO_PTHREAD only.
 */
void platform_emscripten_wait_for_frame(void);

/**
 * Enter a loop that blocks the main loop function.
 * !PROXY_TO_PTHREAD only.
 *
 * @param ms Milliseconds between iterations.
 * Use 1 for balanced performance and CPU usage.
 */
void platform_emscripten_enter_fake_block(int ms);

/**
 * Exit the main loop blocking function.
 * !PROXY_TO_PTHREAD only.
 */
void platform_emscripten_exit_fake_block(void);

/**
 * Set the vsync interval for the main loop.
 *
 * @param interval Vsync interval to set.
 * Use 0 to disable vsync.
 */
void platform_emscripten_set_main_loop_interval(int interval);

#endif
