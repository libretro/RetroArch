/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018 The RetroArch team
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

#ifndef __MIDI_DRIVER__H
#define __MIDI_DRIVER__H

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

struct string_list;

typedef struct
{
   uint8_t *data;
   size_t data_size;
   uint32_t delta_time;
} midi_event_t;

typedef struct midi_driver
{
   /**
    * ident:
    * Driver name (visible in the menu). Must be unique among the MIDI drivers.
    **/
   const char *ident;

   /**
    * get_avail_inputs:
    * Populates provided list with currently available inputs (if any).
    *
    * @inputs                 : List of available inputs (visible in the menu).
    *                           This argument will never be NULL.
    *
    * Returns: True if successful (even if there are no inputs available),
    *          false otherwise.
    *
    * Remarks: Before this function is called the list will already contain one
    *          member ("Off") which can be used by the user to disable input.
    *
    *          If the driver supports input but underlying API don't offers this
    *          list, driver should/must provide some "fake" name(s) here,
    *          otherwise user will only see "Off" option in the menu.
    **/
   bool (*get_avail_inputs)(struct string_list *inputs);

   /**
    * get_avail_outputs:
    * Populates provided list with currently available outputs (if any).
    *
    * @outputs                : List of available outputs (visible in the menu).
    *                           This argument will never be NULL.
    *
    * Returns: True if successful (even if there are no outputs available),
    *          false otherwise.
    *
    * Remarks: Before this function is called the list will already contain one
    *          member ("Off") which can be used by the user to disable output.
    *
    *          If the driver supports output but underlying API don't offers this
    *          list, driver should/must provide some "fake" name(s) here,
    *          otherwise user will only see "Off" option in the menu.
    **/
   bool (*get_avail_outputs)(struct string_list *outputs);

   /**
    * init:
    * Initializes driver and starts input (if 'input' argument is non NULL) and
    * starts output (if 'output' argument is non NULL).
    *
    * @input                  : One of the input names previously returned by
    *                           the 'get_avail_inputs' function or NULL which
    *                           means don't initialize input.
    * @output                 : One of the output names previously returned by
    *                           the 'get_avail_outputs' function or NULL which
    *                           means don't initialize output.
    *
    * Returns: Driver specific data pointer if successful, NULL otherwise.
    *
    * Remarks: If requested input/output is unavailable driver should return
    *          NULL (don't implement "fallback" mechanism).
    **/
   void *(*init)(const char *input, const char *output);

   /**
    * free:
    * Stops I/O and releases any resources used by the driver.
    *
    * @p                      : Driver specific data pointer previously returned
    *                           by the 'init' function.
    *                           This argument will never be NULL.
    **/
   void (*free)(void *p);

   /**
    * set_input:
    * This function is used for changing/disabling input.
    *
    * @p                      : Driver specific data pointer previously returned
    *                           by the 'init' function.
    *                           This argument will never be NULL.
    * @input                  : One of the input names previously returned by
    *                           the 'get_avail_inputs' function or NULL which
    *                           means disable input.
    *
    * Returns: True if successful, false otherwise.
    *
    * Remarks: On error, driver should leave the current input as is (if possible).
    **/
   bool (*set_input)(void *p, const char *input);

   /**
    * set_output:
    * This function is used for changing/disabling output.
    *
    * @p                      : Driver specific data pointer previously returned
    *                           by the 'init' function.
    *                           This argument will never be NULL.
    * @output                 : One of the output names previously returned by
    *                           the 'get_avail_outputs' function or NULL which
    *                           means disable output.
    *
    * Returns: True if successful, false otherwise.
    *
    * Remarks: On error, driver should leave the current output as is (if possible).
    **/
   bool (*set_output)(void *p, const char *output);

   /**
    * read:
    * This function is used for reading received data. When called, 'data_size'
    * member of the 'event' argument will hold max data size (in bytes)
    * that event can hold. After successful read driver should set this member
    * to actual data size that is returned, and 'delta_time' member should be
    * set to time (in microseconds) between previous event and this one (if this
    * is the first event set 'delta_time' to 0).
    *
    * @p                      : Driver specific data pointer previously returned
    *                           by the 'init' function.
    *                           This argument will never be NULL.
    * @event                  : Address of the event structure.
    *                           This argument will never be NULL, 'data' member
    *                           will never be NULL and 'data_size' member
    *                           will never be 0.
    *
    * Returns: True if event was available and read successfully, false otherwise.
    *
    * Remarks: If the driver can't supply timing info 'delta_time' member
    *          of the 'event' argument should be set to 0.
    **/
   bool (*read)(void *p, midi_event_t *event);

   /**
    * write:
    * This function is used for writing events.
    *
    * @p                      : Driver specific data pointer previously returned
    *                           by the 'init' function.
    *                           This argument will never be NULL.
    * @event                  : Address of the event structure.
    *                           This argument will never be NULL, 'data' member
    *                           will never be NULL and 'data_size' member
    *                           will never be 0.
    *
    * Returns: True if event was written successfully, false otherwise.
    *
    * Remarks: This event should be buffered by the driver and sent later
    *          when 'flush' function is called.
    *
    *          If the underlying API don't support event scheduling 'delta_time'
    *          member of the 'event' argument may be ignored by the driver.
    **/
   bool (*write)(void *p, const midi_event_t *event);

   /**
    * flush:
    * This function is used for transferring events previously stored by the
    * 'write' function.
    *
    * @p                      : Driver specific data pointer previously returned
    *                           by the 'init' function.
    *                           This argument will never be NULL.
    *
    * Returns: True if successful, false otherwise.
    *
    * Remarks: On error, drivers should keep the events (don't drop them).
    **/
   bool (*flush)(void *p);
} midi_driver_t;

struct string_list *midi_driver_get_avail_inputs(void);
struct string_list *midi_driver_get_avail_outputs(void);

bool midi_driver_set_volume(unsigned volume);

bool midi_driver_set_input(const char *input);
bool midi_driver_set_output(const char *output);

/**
 * midi_driver_get_event_size:
 * This is a convenience function for finding out the size of the event based
 * on the status byte.
 *
 * @status                 : Status (first) byte of the event.
 *
 * Returns: Size of the event (in bytes). If 'status' argument is invalid
 *          (< 0x80) or 'status' represents start of the "SysEx" event (0xF0)
 *          this function will return 0.
 **/
size_t midi_driver_get_event_size(uint8_t status);

RETRO_END_DECLS

#endif
