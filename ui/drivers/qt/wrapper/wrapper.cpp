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


#include  "../wimp/wimp.h"
#include  "../wimp/wimp_global.h"
#include "wrapper.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct Wimp Wimp;

Wimp* ctrWimp(int argc, char *argv[]){
    return new Wimp(argc,argv);
}

int CreateMainWindow(Wimp* p)
{
    return p->CreateMainWindow();
}

void GetSettings(Wimp* p, settings_t *s)
{
    return p->GetSettings(s);
}

#ifdef __cplusplus
}
#endif
