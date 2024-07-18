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
#include <SDL.h>

#include "display_sdl2.h"
#include "log.h"


//============================================================
//  sdl2_display::sdl2_display
//============================================================

sdl2_display::sdl2_display(display_settings *ds)
{
	// First, we need to fin an active SDL2 window
	if (SDL_WasInit(SDL_INIT_VIDEO) != 0) {
		log_verbose("Switchres/SDL2: (%s): SDL2 video is initialized\n", __FUNCTION__);
	}
	else
	{
		log_verbose("Switchres/SDL2: (%s): SDL2 video wasn't initialized\n", __FUNCTION__);
		throw std::exception();
	}
	// For now, only allow the SDL2 display manager for the KMSDRM backend
	if ( strcmp("KMSDRM", SDL_GetCurrentVideoDriver()) != 0 )
	{
		log_info("Switchres/SDL2: (%s): SDL2 is only available for KMSDRM for now.\n", __FUNCTION__);
		throw std::exception();
	}

	// Get display settings
	m_ds = *ds;
}

//============================================================
//  sdl2_display::~sdl2_display
//============================================================

sdl2_display::~sdl2_display()
{
}

//============================================================
//  sdl2_display::init
//============================================================

bool sdl2_display::init(void* pf_data)
{
	m_sdlwindow = (SDL_Window*) pf_data;

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
	//No need to call get_desktop_mode() SDL2 will restore the desktop mode itself
	get_available_video_modes();

	if (!strcmp(m_ds.monitor, "lcd")) auto_specs();
	filter_modes();

	//SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);

	// Get SDL version information
	SDL_version version;
	SDL_GetVersion(&version);
	log_info("Switchres/SDL2: Detected SDL version %d.%d.%d\n",	(int)version.major, (int)version.minor, (int)version.patch);

	SDL_Window* window = NULL;
	Uint32 id = 0;

	/*
	// Alternative method, only when the render has been created
	if( SDL_GL_GetCurrentWindow() !=  NULL)
	    log_verbose("Swithres/SDL2: (%s) SDL_GL_GetCurrentWindow(); OK !\n", __FUNCTION__);
	*/
	if (pf_data == nullptr or pf_data == NULL)
	{
		int screen = atoi(m_ds.screen);

		while( (window = SDL_GetWindowFromID(++id)) )
		{
			log_verbose("Switchres/SDL2: (%s:%d) SDL display id vs SR display id: %d vs %d (window id: %d)\n", __FUNCTION__, __LINE__, SDL_GetWindowDisplayIndex(window), screen, id);
			if (SDL_GetWindowDisplayIndex(window) == screen)
			{
				log_verbose("Switchres/SDL2: (%s:%d) Found a display-matching SDL window\n ", __FUNCTION__, __LINE__);
				m_sdlwindow = window;
				return true;
			}
		}

		log_verbose("Switchres/SDL2: (%s:%d) No SDL window matching the display found\n ", __FUNCTION__, __LINE__);

	}
	else
	{
		id = SDL_GetWindowID((SDL_Window*)pf_data);
		if( id )
		{
			log_verbose("Switchres/SDL2: (%s:%d) got a valid SDL_Window pointer (window id: %d)\n", __FUNCTION__, __LINE__, id);
			m_sdlwindow = (SDL_Window*)pf_data;
		}
		else
			log_verbose("Switchres/SDL2: (%s:%d) No SDL2 window found, don't expect things to work good\n", __FUNCTION__, __LINE__);
	}

	// Need a check to see if SDL2 can refresh the modelist
	return true;
}

//============================================================
//  sdl2_display::set_mode
//============================================================

bool sdl2_display::set_mode(modeline *mode)
{
	// Call SDL2
	SDL_DisplayMode target, closest;
	target.w = mode->width;
	target.h = mode->height;
	target.format = 0;  // don't care
	target.refresh_rate = mode->refresh;

	/*
	 * Circumventing an annoying choice of SDL2: fullscreen modes are mutually exclusive
	 * which means you can't switch from one to another. If required, need to remove the flag, then
	 * set the new one. This will sadly trigger a window resizing
	 */
	if ( (SDL_GetWindowFlags(m_sdlwindow) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP)
	{
		if ( SDL_SetWindowFullscreen(m_sdlwindow, 0) != 0 )
		{
			log_error("Swithres/SDL2: (%s) Couldn't reset the fullscreen mode. %s\n", __FUNCTION__, SDL_GetError());
			return false;
		}
	}

	if ( (SDL_GetWindowFlags(m_sdlwindow) & SDL_WINDOW_FULLSCREEN) != SDL_WINDOW_FULLSCREEN )
	{
		// Now we set the right mode that allows SDL2 modeswitches
		if ( SDL_SetWindowFullscreen(m_sdlwindow, SDL_WINDOW_FULLSCREEN) != 0 )
		{
			log_error("Swithres/SDL2: (%s) Couldn't set the window to FULLSCREEN. %s\n", __FUNCTION__, SDL_GetError());
			return false;
		}
	}

	// We may first check if the mode was already added, so we don't force a probe of all modes
	if ( SDL_GetClosestDisplayMode(SDL_GetWindowDisplayIndex(m_sdlwindow), &target, &closest) == NULL )
	{
		// If the returned pointer is null, no match was found.
		log_error("Swithres/SDL2: (%s) No suitable display mode was found! %s\n\n", __FUNCTION__, SDL_GetError());
		return false;
	}
	log_verbose("  Received: \t%dx%dpx @ %dhz \n", closest.w, closest.h, closest.refresh_rate);

	if ( SDL_SetWindowDisplayMode(m_sdlwindow, &closest) != 0 )
	{
		log_error("Swithres/SDL2: (%s) Failed to switch mode: %s\n", __FUNCTION__, SDL_GetError());
		return false;
	}

	log_verbose("Swithres/SDL2: (%s) SDL2 display mode changed for window/display %d/%d!\n", __FUNCTION__, SDL_GetWindowID(m_sdlwindow), SDL_GetWindowDisplayIndex(m_sdlwindow));
	log_verbose("               to %dx%d@%d\n",closest.w, closest.h, closest.refresh_rate);

	set_current_mode(mode);
	return true;
}

//============================================================
//  sdl2_display::get_desktop_mode
//============================================================

bool sdl2_display::get_desktop_mode()
{
	if (video() == NULL)
		return false;

	return true;
}

//============================================================
//  sdl2_display::get_available_video_modes
//============================================================

int sdl2_display::get_available_video_modes()
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
		if (mode.type == 0)
			break;

		// set the desktop mode
		if (mode.type & MODE_DESKTOP)
		{
			memcpy(&desktop_mode, &mode, sizeof(modeline));
			if (current_mode() == nullptr)
				set_current_mode(&mode);

			if (mode.type & MODE_ROTATED) set_desktop_is_rotated(true);
		}

		video_modes.push_back(mode);
		backup_modes.push_back(mode);

		log_verbose("Switchres/SDL2: [%3ld] %4dx%4d @%3d%s%s %s: ", video_modes.size(), mode.width, mode.height, mode.refresh, mode.interlace ? "i" : "p", mode.type & MODE_DESKTOP ? "*" : "", mode.type & MODE_ROTATED ? "rot" : "");
		log_mode(&mode);
	};

	return true;
}
