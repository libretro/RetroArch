/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 Daniel De Matteis
 *  Copyright (C) 2023 Jesse Talavera-Greenberg
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

/**
 * Contains WASAPI-specific support functions that are used
 * by the WASAPI audio and microphone drivers.
 */

#ifndef RETROARCH_COMMON_WASAPI_H
#define RETROARCH_COMMON_WASAPI_H

/* Shared buffer size replacement placeholders */
#define WASAPI_SH_BUFFER_AUDIO_LATENCY 0
#define WASAPI_SH_BUFFER_DEVICE_PERIOD 32
#define WASAPI_SH_BUFFER_CLIENT_BUFFER 64

#endif /* RETROARCH_COMMON_WASAPI_H */
