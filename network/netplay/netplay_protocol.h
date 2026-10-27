/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Gregor Richards
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

#ifndef __RARCH_NETPLAY_PROTOCOL_H
#define __RARCH_NETPLAY_PROTOCOL_H

#define LOW_NETPLAY_PROTOCOL_VERSION  5
#define HIGH_NETPLAY_PROTOCOL_VERSION 8
/* Understands NETPLAY_DEVICE_ANALOG_JOYPAD_FLAG */
#define NETPLAY_PROTOCOL_ANALOG_JOYPADS 8
/* Takes a state labeled with a frame whose input it has */
#define NETPLAY_PROTOCOL_LOCKSTEP 8

#define NETPLAY_PROTOCOL_VERSION HIGH_NETPLAY_PROTOCOL_VERSION

#endif
