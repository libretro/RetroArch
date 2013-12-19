/* Copyright (C) 2010-2013 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this libretro API header (libretro_private.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBRETRO_PRIVATE_H__
#define LIBRETRO_PRIVATE_H__

// Private additions to libretro. No API/ABI stability guaranteed.

#include "libretro.h"

#define RETRO_ENVIRONMENT_SET_LIBRETRO_PATH (RETRO_ENVIRONMENT_PRIVATE | 0)
                                           // const char * --
                                           // Sets the absolute path for the libretro core pointed to. RETRO_ENVIRONMENT_EXEC will use the last libretro core set with this call.
                                           // Returns false if file for absolute path could not be found.
#define RETRO_ENVIRONMENT_EXEC             (RETRO_ENVIRONMENT_PRIVATE | 1)
                                           // const char * --
                                           // Requests that this core is deinitialized, and a new core is loaded.
                                           // The libretro core used is set with SET_LIBRETRO_PATH, and path to game is passed in _EXEC. NULL means no game.
#define RETRO_ENVIRONMENT_EXEC_ESCAPE     (RETRO_ENVIRONMENT_PRIVATE | 2)
                                           // const char * --
                                           // Requests that this core is deinitialized, and a new core is loaded. It also escapes the main loop the core is currently
                                           // bound to.
                                           // The libretro core used is set with SET_LIBRETRO_PATH, and path to game is passed in _EXEC. NULL means no game.
#define RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE (RETRO_ENVIRONMENT_PRIVATE | 3)
                                           // struct retro_location_interface * --
                                           // Gets access to the location interface.
                                           // The purpose of this interface is to be able to retrieve location-based information from the host device, 
                                           // such as current latitude / longitude.
                                           //

//Sets the interval of time and/or distance at which to update/poll location-based data.
//To ensure compatibility with all location-based implementations, values for both 
//interval_ms and interval_distance should be provided.
//interval_ms is the interval expressed in milliseconds
//interval_distance is the distance interval expressed in meters.
typedef void (*retro_location_set_interval_t)(int interval_ms, int interval_distance);

//Start location services. The device will start listening for changes to the
//current location at regular intervals (which are defined with retro_location_set_interval_t).
typedef void (*retro_location_start_t)(void);

//Stop location services. The device will stop listening for changes to the current
//location.
typedef void (*retro_location_stop_t)(void);

//Get the latitude of the current location.
typedef double (*retro_location_get_latitude_t)(void);

//Get the longitude of the current location.
typedef double (*retro_location_get_longitude_t)(void);

struct retro_location_interface
{
   int interval_in_ms;
   int interval_distance_in_meters;
   retro_location_start_t         start;
   retro_location_stop_t          stop;
   retro_location_get_latitude_t  get_latitude;
   retro_location_get_longitude_t get_longitude;
   retro_location_set_interval_t  set_interval; 
};

#endif

