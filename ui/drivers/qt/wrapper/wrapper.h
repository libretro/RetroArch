/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Andres Suarez
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

#ifndef HAVE_MENU
#define HAVE_MENU
#endif

#include "config.h"
#include "configuration.h"

#ifndef WRAPPER_H
#define WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Wimp Wimp;
typedef settings_t (*config_get_ptr_cb);

Wimp* ctrWimp(int argc, char *argv[]);

int CreateMainWindow(Wimp* p);
void GetSettings(Wimp* p, settings_t *s);

#ifdef __cplusplus
}
#endif

#endif // WRAPPER_H
