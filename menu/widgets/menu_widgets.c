/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2018      - natinusala
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

#include "menu_widgets.h"

#include "../../verbosity.h"
#include "../../retroarch.h"
#include "../../configuration.h"
#include "../../msg_hash.h"

#include "../../tasks/task_content.h"
#include "../../ui/ui_companion_driver.h"

#include "../menu_driver.h"
#include "../menu_animation.h"

#include "../../gfx/font_driver.h"

#include <lists/file_list.h>
#include <queues/fifo_queue.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <formats/image.h>
#include <string/stdstring.h>

