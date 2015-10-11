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

#include "../../driver.h"
#include "../../general.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include "../video_viewport.h"
#include "../video_monitor.h"
#include "../font_renderer_driver.h"
#include <retro_inline.h>

#include "../common/x11_common.h"

#include <stdlib.h>
/*#include <sys/ipc.h>*/
#include <sys/shm.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

typedef struct xshm
{
	Display* display;
	int screen;
	
	Window parentwindow;
	Window wndw;
	
	unsigned int width;
	unsigned int height;
	unsigned int pitch;
#if 0
	videoformat bpp;
#endif
	
	XShmSegmentInfo shmInfo;
	XImage* image;
	GC gc;
} xshm_t;

video_driver_t video_xshm = {
   NULL,/*xshm_init,*/
   NULL,/*xshm_frame,*/
   NULL,/*xshm_set_nonblock_state,*/
   NULL,/*xshm_alive,*/
   NULL,/*xshm_focus,*/
   NULL,/*xshm_suppress_screensaver,*/
   NULL,/*xshm_has_windowed,*/
   NULL,/*xshm_set_shader,*/
   NULL,/*xshm_free,*/
   "xshm",
   NULL, /* set_viewport */
   NULL,/*xshm_set_rotation,*/
   NULL,/*xshm_viewport_info,*/
   NULL,/*xshm_read_viewport,*/
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL, /* overlay_interface */
#endif
   NULL/*xshm_get_poke_interface*/
};
