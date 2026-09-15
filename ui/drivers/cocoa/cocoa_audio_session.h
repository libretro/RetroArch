/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef COCOA_AUDIO_SESSION_H
#define COCOA_AUDIO_SESSION_H

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* The AVAudioSession is Objective-C and belongs to the Cocoa layer; the
 * audio drivers are C and stay C. On iOS and tvOS the microphone needs
 * the session in a record category before its unit will capture, and
 * the rate the session settles on is the rate the unit will run at.
 * Implemented in ui_cocoatouch.m. Returns false when the session
 * refused the category; *actual_rate is the session's rate afterwards,
 * 0 if unknown. */
bool cocoa_audio_session_begin_record(unsigned preferred_rate,
      unsigned *actual_rate);

RETRO_END_DECLS

#endif
