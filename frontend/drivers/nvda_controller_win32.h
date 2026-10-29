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

#ifndef __RARCH_NVDA_CONTROLLER_WIN32_H
#define __RARCH_NVDA_CONTROLLER_WIN32_H

#include <wchar.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* The NVDA screen reader's controller interface, spoken to over local
 * RPC as NVDA's own controller client does: no DLL to ship or load.
 *
 * nvda_controller_init() binds to the NVDA instance on this session
 * and desktop; false when this Windows or this build has no way to
 * reach one (the calls below then return nonzero). It binds once and
 * holds the binding until nvda_controller_deinit(). Each call returns
 * 0 on success and the RPC status otherwise - nonzero, notably, when
 * NVDA is not running. */
bool nvda_controller_init(void);
void nvda_controller_deinit(void);

unsigned long nvda_controller_test_if_running(void);
unsigned long nvda_controller_cancel_speech(void);
unsigned long nvda_controller_speak_text(const wchar_t *text);
unsigned long nvda_controller_braille_message(const wchar_t *message);

RETRO_END_DECLS

#endif
