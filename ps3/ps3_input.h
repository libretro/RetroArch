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

#ifndef _PS3_INPUT_H_
#define _PS3_INPUT_H_

#include <stdbool.h>
#include <wchar.h>
#include <sysutil/sysutil_oskdialog.h>
#include <sysutil/sysutil_common.h>

#define MAX_PADS 7

#define CTRL_SELECT_MASK 0x01
#define CTRL_L3_MASK 0x2
#define CTRL_R3_MASK 0x4
#define CTRL_START_MASK 0x8
#define CTRL_UP_MASK 0x10
#define CTRL_RIGHT_MASK 0x20
#define CTRL_DOWN_MASK 0x40
#define CTRL_LEFT_MASK 0x80

#define CTRL_L2_MASK 0x100
#define CTRL_R2_MASK 0x200
#define CTRL_L1_MASK 0x400
#define CTRL_R1_MASK 0x800
#define CTRL_TRIANGLE_MASK 0x1000
#define CTRL_CIRCLE_MASK 0x2000
#define CTRL_CROSS_MASK 0x4000
#define CTRL_SQUARE_MASK 0x8000

// Big numbers, harhar.
#define CTRL_LSTICK_LEFT_MASK    0x1000000000000LLU
#define CTRL_LSTICK_RIGHT_MASK   0x2000000000000LLU
#define CTRL_LSTICK_UP_MASK      0x4000000000000LLU
#define CTRL_LSTICK_DOWN_MASK    0x8000000000000LLU
#define CTRL_RSTICK_LEFT_MASK    0x10000000000000LLU
#define CTRL_RSTICK_RIGHT_MASK   0x20000000000000LLU
#define CTRL_RSTICK_UP_MASK      0x40000000000000LLU
#define CTRL_RSTICK_DOWN_MASK    0x80000000000000LLU


#define CTRL_SELECT(state) (CTRL_SELECT_MASK & state)
#define CTRL_L3(state) (CTRL_L3_MASK & state)
#define CTRL_R3(state) (CTRL_R3_MASK & state)
#define CTRL_START(state) (CTRL_START_MASK & state)
#define CTRL_UP(state) (CTRL_UP_MASK & state)
#define CTRL_RIGHT(state) (CTRL_RIGHT_MASK & state)
#define CTRL_DOWN(state) (CTRL_DOWN_MASK & state)
#define CTRL_LEFT(state) (CTRL_LEFT_MASK & state)

#define CTRL_L2(state) (CTRL_L2_MASK & state)
#define CTRL_R2(state) (CTRL_R2_MASK & state)
#define CTRL_L1(state) (CTRL_L1_MASK & state)
#define CTRL_R1(state) (CTRL_R1_MASK & state)
#define CTRL_TRIANGLE(state) (CTRL_TRIANGLE_MASK & state)
#define CTRL_CIRCLE(state) (CTRL_CIRCLE_MASK & state)
#define CTRL_CROSS(state) (CTRL_CROSS_MASK & state)
#define CTRL_SQUARE(state) (CTRL_SQUARE_MASK & state)

#define CTRL_LSTICK_LEFT(state)     (CTRL_LSTICK_LEFT_MASK & state)
#define CTRL_LSTICK_RIGHT(state)    (CTRL_LSTICK_RIGHT_MASK & state)
#define CTRL_LSTICK_UP(state)       (CTRL_LSTICK_UP_MASK & state)
#define CTRL_LSTICK_DOWN(state)     (CTRL_LSTICK_DOWN_MASK & state)
#define CTRL_RSTICK_LEFT(state)     (CTRL_RSTICK_LEFT_MASK & state)
#define CTRL_RSTICK_RIGHT(state)    (CTRL_RSTICK_RIGHT_MASK & state)
#define CTRL_RSTICK_UP(state)       (CTRL_RSTICK_UP_MASK & state)
#define CTRL_RSTICK_DOWN(state)     (CTRL_RSTICK_DOWN_MASK & state)

#define CTRL_MASK(state, mask) (state & mask)

#define CTRL_AXIS_LSTICK_X(state) ((uint8_t)(((0xFF0000LLU & state) >> 16) & 0xFF))
#define CTRL_AXIS_LSTICK_Y(state) ((uint8_t)(((0xFF000000LLU & state) >> 24) & 0xFF))
#define CTRL_AXIS_RSTICK_X(state) ((uint8_t)(((0xFF00000000LLU & state) >> 32) & 0xFF))
#define CTRL_AXIS_RSTICK_Y(state) ((uint8_t)(((0xFF0000000000LLU & state) >> 40) & 0xFF))

#define LOWER_BUTTONS 2
#define HIGHER_BUTTONS 3
#define RSTICK_X 4
#define RSTICK_Y 5
#define LSTICK_X 6
#define LSTICK_Y 7

#define DEADZONE_LOW 55
#define DEADZONE_HIGH 210

#define PRESSED_LEFT_LSTICK(state)   (CTRL_AXIS_LSTICK_X(state) <= DEADZONE_LOW)
#define PRESSED_RIGHT_LSTICK(state)  (CTRL_AXIS_LSTICK_X(state) >= DEADZONE_HIGH)
#define PRESSED_UP_LSTICK(state)     (CTRL_AXIS_LSTICK_Y(state) <= DEADZONE_LOW)
#define PRESSED_DOWN_LSTICK(state)   (CTRL_AXIS_LSTICK_Y(state) >= DEADZONE_HIGH)
#define PRESSED_LEFT_RSTICK(state)   (CTRL_AXIS_RSTICK_X(state) <= DEADZONE_LOW)
#define PRESSED_RIGHT_RSTICK(state)  (CTRL_AXIS_RSTICK_X(state) >= DEADZONE_HIGH)
#define PRESSED_UP_RSTICK(state)     (CTRL_AXIS_RSTICK_Y(state) <= DEADZONE_LOW)
#define PRESSED_DOWN_RSTICK(state)   (CTRL_AXIS_RSTICK_Y(state) >= DEADZONE_HIGH)

#define LSTICK_LEFT_SHIFT 48
#define LSTICK_RIGHT_SHIFT 49
#define LSTICK_UP_SHIFT 50
#define LSTICK_DOWN_SHIFT 51

#define RSTICK_LEFT_SHIFT 52
#define RSTICK_RIGHT_SHIFT 53
#define RSTICK_UP_SHIFT 54
#define RSTICK_DOWN_SHIFT 55

#define OSK_IS_RUNNING(object) object.is_running
#define OUTPUT_TEXT_STRING(object) object.osk_text_buffer_char

typedef uint64_t cell_input_state_t;

int cell_pad_input_init(void);
void cell_pad_input_deinit(void);

uint32_t cell_pad_input_pads_connected(void);

cell_input_state_t cell_pad_input_poll_device(uint32_t id);

void ps3_input_init(void);
void ps3_input_map_dpad_to_stick(uint32_t map_dpad_enum, uint32_t controller_id);

typedef struct
{
   unsigned int osk_memorycontainer;
   wchar_t init_message[CELL_OSKDIALOG_STRING_SIZE + 1];
   wchar_t message[CELL_OSKDIALOG_STRING_SIZE + 1];
   wchar_t osk_text_buffer[CELL_OSKDIALOG_STRING_SIZE + 1];
   char osk_text_buffer_char[CELL_OSKDIALOG_STRING_SIZE + 1];
   uint32_t flags;
   bool is_running;
   bool text_can_be_fetched;
   sys_memory_container_t containerid;
   CellOskDialogPoint pos;
   CellOskDialogInputFieldInfo inputFieldInfo;
   CellOskDialogCallbackReturnParam outputInfo;
   CellOskDialogParam dialogParam;
} oskutil_params;

void oskutil_write_message(oskutil_params *params, const wchar_t* msg);
void oskutil_write_initial_message(oskutil_params *params, const wchar_t* msg);
void oskutil_init(oskutil_params *params, unsigned int containersize);
bool oskutil_start(oskutil_params *params);
void oskutil_stop(oskutil_params *params);
void oskutil_finished(oskutil_params *params);
void oskutil_close(oskutil_params *params);
void oskutil_unload(oskutil_params *params);


#endif
