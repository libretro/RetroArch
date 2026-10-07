/*  RetroArch - A frontend for libretro.
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

#ifndef INPUT_OUTPUT_WRITER_H__
#define INPUT_OUTPUT_WRITER_H__

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* A thread for a joypad driver's writes to its controllers: rumble,
 * and whatever else goes out to a device.
 *
 * A write to a controller returns when the device, or the program
 * behind a virtual one, has taken it. Made from the frame loop it
 * holds the frame for as long. A driver instead keeps what is wanted
 * of each controller in atomics of its own, and wakes this thread to
 * write it.
 *
 * Nothing is queued here and nothing is passed through: a wake says
 * only that there is something to look at. Wakes that arrive while the
 * thread is writing fold into one, so a controller slow to answer is
 * given the latest of what is wanted and not each value in turn.
 *
 * There is no lock. The thread is woken through an event (Windows) or
 * a pipe. */
typedef struct input_output_writer input_output_writer_t;

/* Called on the writer's thread after each wake: the driver compares
 * what is wanted with what it last wrote, and writes what differs.
 * @last is true for one final call as the writer is freed, for the
 * driver to let go of what that thread holds. */
typedef void (*input_output_writer_cb)(void *userdata, bool last);

/* NULL where there are no threads, or if the thread cannot be
 * started: the driver then makes its writes itself, as it used to. */
input_output_writer_t *input_output_writer_new(
      input_output_writer_cb cb, void *userdata);

/* From any thread; returns at once. */
void input_output_writer_wake(input_output_writer_t *writer);

/* Makes the final call, ends the thread and waits for it. */
void input_output_writer_free(input_output_writer_t *writer);

RETRO_END_DECLS

#endif
