/*
 *  Copyright (C) 2008 dhewg, #wiidev efnet
 *
 *  this file is part of geckoloader
 *  http://wiibrew.org/index.php?title=Geckoloader
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _DOL_H_
#define _DOL_H_

#include <gctypes.h>

uint32_t *load_dol_image (void *dolstart);
void dol_copy_argv(struct __argv *argv);

#endif

