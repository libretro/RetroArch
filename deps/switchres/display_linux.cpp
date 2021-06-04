/**************************************************************

   display_linux.cpp - Display manager for Linux

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#include <stdio.h>
#include <string.h>

#include "display_linux.h"
#include "log.h"

//============================================================
//  linux_display::linux_display
//============================================================

linux_display::linux_display(display_settings *ds)
{
	// Get display settings
	m_ds = *ds;
}

//============================================================
//  linux_display::~linux_display
//============================================================

linux_display::~linux_display()
{
	if (!m_ds.keep_changes)
		restore_desktop_mode();
}

//============================================================
//  linux_display::init
//============================================================

bool linux_display::init()
{
	// Initialize custom video
	int method = CUSTOM_VIDEO_TIMING_AUTO;

#ifdef SR_WITH_XRANDR
	if (!strcmp(m_ds.api, "xrandr"))
		method = CUSTOM_VIDEO_TIMING_XRANDR;
#endif
#ifdef SR_WITH_KMSDRM
	if (!strcmp(m_ds.api, "drmkms"))
		method = CUSTOM_VIDEO_TIMING_DRMKMS;
#endif

	set_factory(new custom_video);
	set_custom_video(factory()->make(m_ds.screen, NULL, method, &m_ds.vs));
	if (!video() or !video()->init())
		return false;

	// Build our display's mode list
	video_modes.clear();
	backup_modes.clear();
	get_desktop_mode();
	get_available_video_modes();

	if (!strcmp(m_ds.monitor, "lcd")) auto_specs();
	filter_modes();

	return true;
}

//============================================================
//  linux_display::set_mode
//============================================================

bool linux_display::set_mode(modeline *mode)
{
	if (mode && set_desktop_mode(mode, 0))
	{
		set_current_mode(mode);
		return true;
	}
	return false;
}

//============================================================
//  linux_display::get_desktop_mode
//============================================================

bool linux_display::get_desktop_mode()
{
	if (video() == NULL)
		return false;

	return true;
}

//============================================================
//  linux_display::set_desktop_mode
//============================================================

bool linux_display::set_desktop_mode(modeline *mode, int flags)
{
	if (!mode)
		return false;

	if (video() == NULL)
		return false;

	if (flags != 0)
		log_info("Set desktop mode flags value is 0x%x.\n", flags);

	return video()->set_timing(mode);
}

//============================================================
//  linux_display::restore_desktop_mode
//============================================================

bool linux_display::restore_desktop_mode()
{
	if (video() == NULL)
		return false;

	return video()->set_timing(&desktop_mode);
}

//============================================================
//  linux_display::get_available_video_modes
//============================================================

int linux_display::get_available_video_modes()
{
	if (video() == NULL)
		return false;

	// loop through all modes until NULL mode type is received
	for (;;)
	{
		modeline mode;
		memset(&mode, 0, sizeof(struct modeline));

		// get next mode
		video()->get_timing(&mode);
		if (mode.type == 0 || mode.platform_data == 0)
			break;

		// set the desktop mode
		if (mode.type & MODE_DESKTOP)
		{
			memcpy(&desktop_mode, &mode, sizeof(modeline));
			if (current_mode() == nullptr)
				set_current_mode(&mode);
		}

		video_modes.push_back(mode);
		backup_modes.push_back(mode);

		log_verbose("Switchres: [%3ld] %4dx%4d @%3d%s%s %s: ", video_modes.size(), mode.width, mode.height, mode.refresh, mode.interlace ? "i" : "p", mode.type & MODE_DESKTOP ? "*" : "", mode.type & MODE_ROTATED ? "rot" : "");
		log_mode(&mode);
	};

	return true;
}
