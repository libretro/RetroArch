/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stddef.h>
#include <string.h>

#include <compat/strl.h>
#include <retro_common_api.h>
#include <file/config_file.h>
#include <features/features_cpu.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <retro_math.h>
#include <retro_miscellaneous.h>

#include <retro_assert.h>
#include <gfx/scaler/pixconv.h>
#include <gfx/scaler/scaler.h>
#include <gfx/video_frame.h>
#include <formats/image.h>

#include "../menu/menu_shader.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "../dynamic.h"

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#include "../menu/menu_setting.h"
#ifdef HAVE_MENU_WIDGETS
#include "../menu/widgets/menu_widgets.h"
#endif
#endif

#ifdef HAVE_VIDEO_LAYOUT
#include "video_layout.h"
#endif

#include "video_thread_wrapper.h"
#include "video_driver.h"
#include "video_display_server.h"
#include "video_crt_switch.h"

#include "../frontend/frontend_driver.h"
#include "../record/record_driver.h"
#include "../config.def.h"
#include "../configuration.h"
#include "../driver.h"
#include "../retroarch.h"
#include "../input/input_driver.h"
#include "../list_special.h"
#include "../core.h"
#include "../command.h"
#include "../msg_hash.h"
#include "../verbosity.h"
