/*****************************************************************************
 * 
 *  Copyright (c) 2020 by SonicMastr <sonicmastr@gmail.com>
 * 
 *  This file is part of Pigs In A Blanket
 * 
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 * 
 ****************************************************************************/

#ifndef HOOKS_H_
#define HOOKS_H_

#include "../include/pib.h"
#include <taihen.h>

#define NUM_HOOKS 20

extern tai_hook_ref_t hookRef[NUM_HOOKS];
extern SceUID hook[NUM_HOOKS];
extern int customResolutionMode;
extern tai_module_info_t modInfo;
extern int systemMode;
extern int msaaEnabled;
extern int isCreatingSurface;

void loadHooks(PibOptions options);
void releaseHooks(void);

#endif /* HOOKS_H_ */
