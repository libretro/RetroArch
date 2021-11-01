/**************************************************************

   custom_video_pstrip.cpp - PowerStrip interface routines

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

/*  http://forums.entechtaiwan.com/index.php?topic=5534.msg20902;topicseen#msg20902

    UM_SETCUSTOMTIMING = WM_USER+200;
    wparam = monitor number, zero-based
    lparam = atom for string pointer
    lresult = -1 for failure else current pixel clock (integer in Hz)
    Note: pass full PowerStrip timing string*

    UM_SETREFRESHRATE = WM_USER+201;
    wparam = monitor number, zero-based
    lparam = refresh rate (integer in Hz), or 0 for read-only
    lresult = -1 for failure else current refresh rate (integer in Hz)

    UM_SETPOLARITY = WM_USER+202;
    wparam = monitor number, zero-based
    lparam = polarity bits
    lresult = -1 for failure else current polarity bits+1

    UM_REMOTECONTROL = WM_USER+210;
    wparam = 99
    lparam =
        0 to hide tray icon
        1 to show tray icon,
        2 to get build number
       10 to show Performance profiles
       11 to show Color profiles
       12 to show Display profiles
       13 to show Application profiles
       14 to show Adapter information
       15 to show Monitor information
       16 to show Hotkey manager
       17 to show Resource manager
       18 to show Preferences
       19 to show Online services
       20 to show About screen
       21 to show Tip-of-the-day
       22 to show Setup wizard
       23 to show Screen fonts
       24 to show Advanced timing options
       25 to show Custom resolutions
       99 to close PS
    lresult = -1 for failure else lparam+1 for success or build number (e.g., 335)
    if lparam was 2

    UM_SETGAMMARAMP = WM_USER+203;
    wparam = monitor number, zero-based
    lparam = atom for string pointer
    lresult = -1 for failure, 1 for success

    UM_CREATERESOLUTION = WM_USER+204;
    wparam = monitor number, zero-based
    lparam = atom for string pointer
    lresult = -1 for failure, 1 for success
    Note: pass full PowerStrip timing string*; reboot is usually necessary to see if
    the resolution is accepted by the display driver

    UM_GETTIMING = WM_USER+205;
    wparam = monitor number, zero-based
    lresult = -1 for failure else GlobalAtom number identifiying the timing string*
    Note: be sure to call GlobalDeleteAtom after reading the string associated with
    the atom

    UM_GETSETCLOCKS = WM_USER+206;
    wparam = monitor number, zero-based
    lparam = atom for string pointer
    lresult = -1 for failure else GlobalAtom number identifiying the performance
    string**
    Note: pass full PowerStrip performance string** to set the clocks, and ull to
    get clocks; be sure to call GlobalDeleteAtom after reading the string associated
    with the atom

    NegativeHorizontalPolarity = 0x02;
    NegativeVerticalPolarity = 0x04;

    *Timing string parameter definition:
     1 = horizontal active pixels
     2 = horizontal front porch
     3 = horizontal sync width
     4 = horizontal back porch
     5 = vertical active pixels
     6 = vertical front porch
     7 = vertical sync width
     8 = vertical back porch
     9 = pixel clock in hertz
    10 = timing flags, where bit:
         1 = negative horizontal porlarity
         2 = negative vertical polarity
         3 = interlaced
         5 = composite sync
         7 = sync-on-green
         all other bits reserved

    **Performance string parameter definition:
     1 = memory clock in hertz
     2 = engine clock in hertz
     3 = reserved
     4 = reserved
     5 = reserved
     6 = reserved
     7 = reserved
     8 = reserved
     9 = 2D memory clock in hertz (if different from 3D)
    10 = 2D engine clock in hertz (if different from 3D) */

#include <windows.h>
#include <stdio.h>

#include "custom_video_pstrip.h"
#include "log.h"

//============================================================
//  CONSTANTS
//============================================================

#define UM_SETCUSTOMTIMING      (WM_USER+200)
#define UM_SETREFRESHRATE       (WM_USER+201)
#define UM_SETPOLARITY          (WM_USER+202)
#define UM_REMOTECONTROL        (WM_USER+210)
#define UM_SETGAMMARAMP         (WM_USER+203)
#define UM_CREATERESOLUTION     (WM_USER+204)
#define UM_GETTIMING            (WM_USER+205)
#define UM_GETSETCLOCKS         (WM_USER+206)
#define UM_SETCUSTOMTIMINGFAST  (WM_USER+211) // glitches vertical sync with PS 3.65 build 568

#define NegativeHorizontalPolarity      0x02
#define NegativeVerticalPolarity        0x04
#define Interlace                       0x08

#define HideTrayIcon                    0x00
#define ShowTrayIcon                    0x01
#define ClosePowerStrip                 0x63

//============================================================
//  pstrip_timing::pstrip_timing
//============================================================

pstrip_timing::pstrip_timing(char *device_name, custom_video_settings *vs)
{
	m_vs = *vs;
	strcpy (m_device_name, device_name);
	strcpy (m_ps_timing, m_vs.custom_timing);
}

//============================================================
//  pstrip_timing::~pstrip_timing()
//============================================================

pstrip_timing::~pstrip_timing()
{
	ps_reset();
}

//============================================================
//  pstrip_timing::init
//============================================================

bool pstrip_timing::init()
{
	m_monitor_index = ps_monitor_index(m_device_name);

	hPSWnd = FindWindowA("TPShidden", NULL);

	if (hPSWnd)
	{
		log_verbose("PStrip: PowerStrip found!\n");

		// Save current settings
		ps_get_monitor_timing(&m_timing_backup);

		// If we have a -ps_timing string defined, use it as user defined modeline
		if (strcmp(m_ps_timing, "auto"))
		{
			MonitorTiming timing;
			if (ps_read_timing_string(m_ps_timing, &timing))
			{
				ps_pstiming_to_modeline(&timing, &m_user_mode);
				m_user_mode.type |= CUSTOM_VIDEO_TIMING_POWERSTRIP;

				char modeline_txt[256]={'\x00'};
				log_verbose("SwitchRes: ps_string: %s (%s)\n", m_ps_timing, modeline_print(&m_user_mode, modeline_txt, MS_PARAMS));
			}
			else
				log_verbose("Switchres: ps_timing string with invalid format\n");
		}
	}
	else
	{
		log_verbose("PStrip: Could not get PowerStrip API interface\n");
		return false;
	}

	return true;
}

//============================================================
//  pstrip_timing::get_timing
//============================================================

bool pstrip_timing::get_timing(modeline *mode)
{
	// If we have an user defined mode (ps_timing), lock any non matching mode
	if (m_user_mode.hactive)
	{
		if (mode->width != m_user_mode.width || mode->height != m_user_mode.height)
		{
			mode->type |= MODE_DISABLED;
			return false;
		}
	}

	modeline m_temp = {};
	if (ps_get_modeline(&m_temp))
	{
		// We can only get the timings of the current desktop mode, so filter out anything different
		if (m_temp.width == mode->width && m_temp.height == mode->height && m_temp.refresh == mode->refresh)
		{
			*mode = m_temp;
		}
		mode->type |= CUSTOM_VIDEO_TIMING_POWERSTRIP;
		return true;
	}

	return false;
}

//============================================================
//  pstrip_timing::set_timing
//============================================================

bool pstrip_timing::set_timing(modeline *mode)
{
	// In case -ps_timing is provided, pass it as raw string
	if (m_user_mode.hactive)
		ps_set_monitor_timing_string(m_ps_timing);

	// Otherwise pass it as modeline
	else
		ps_set_modeline(mode);

	Sleep(100);
	return true;
}


//============================================================
//  pstrip_timing::ps_reset
//============================================================

int pstrip_timing::ps_reset()
{
	return ps_set_monitor_timing(&m_timing_backup);
}

//============================================================
//  ps_get_modeline
//============================================================

int pstrip_timing::ps_get_modeline(modeline *modeline)
{
	MonitorTiming timing = {};

	if (ps_get_monitor_timing(&timing))
	{
		ps_pstiming_to_modeline(&timing, modeline);
		return 1;
	}
	else return 0;
}

//============================================================
//  pstrip_timing::ps_set_modeline
//============================================================

bool pstrip_timing::ps_set_modeline(modeline *modeline)
{
	MonitorTiming timing = {};

	if (!ps_modeline_to_pstiming(modeline, &timing))
		return false;

	timing.PixelClockInKiloHertz = ps_best_pclock(&timing, timing.PixelClockInKiloHertz);

	if (ps_set_monitor_timing(&timing))
		return true;
	else
		return false;
}

//============================================================
//  pstrip_timing::ps_get_monitor_timing
//============================================================

int pstrip_timing::ps_get_monitor_timing(MonitorTiming *timing)
{
	LRESULT lresult;
	char in[256];

	if (!hPSWnd) return 0;

	lresult = SendMessage(hPSWnd, UM_GETTIMING, m_monitor_index, 0);

	if (lresult == -1)
	{
		log_verbose("PStrip: Could not get PowerStrip timing string\n");
		return 0;
	}

	if (!GlobalGetAtomNameA(lresult, in, sizeof(in)))
	{
		log_verbose("PStrip: GlobalGetAtomName failed\n");
		return 0;
	}

	log_verbose("PStrip: ps_get_monitor_timing(%d): %s\n", m_monitor_index, in);

	ps_read_timing_string(in, timing);

	GlobalDeleteAtom(lresult); // delete atom created by PowerStrip

	return 1;
}

//============================================================
//  pstrip_timing::ps_set_monitor_timing
//============================================================

int pstrip_timing::ps_set_monitor_timing(MonitorTiming *timing)
{
	LRESULT lresult;
	ATOM atom;
	char out[256];

	if (!hPSWnd) return 0;

	ps_fill_timing_string(out, timing);
	atom = GlobalAddAtomA(out);

	if (atom)
	{
		lresult = SendMessage(hPSWnd, UM_SETCUSTOMTIMING, m_monitor_index, atom);

		if (lresult < 0)
		{
			log_verbose("PStrip: SendMessage failed\n");
			GlobalDeleteAtom(atom);
		}
		else
		{
			log_verbose("PStrip: ps_set_monitor_timing(%d): %s\n", m_monitor_index, out);
			return 1;
		}
	}
	else log_verbose("PStrip: ps_set_monitor_timing atom creation failed\n");

	return 0;
}

//============================================================
//  pstrip_timing::ps_set_monitor_timing_string
//============================================================

int pstrip_timing::ps_set_monitor_timing_string(char *in)
{
	MonitorTiming timing = {};

	ps_read_timing_string(in, &timing);
	return ps_set_monitor_timing(&timing);
}

//============================================================
//  pstrip_timing::ps_set_refresh
//============================================================

int pstrip_timing::ps_set_refresh(double vfreq)
{
	MonitorTiming timing = {};
	int hht, vvt, new_vvt;
	int desired_pClock;
	int best_pClock;

	memcpy(&timing, &m_timing_backup, sizeof(MonitorTiming));

	hht = timing.HorizontalActivePixels
		+ timing.HorizontalFrontPorch
		+ timing.HorizontalSyncWidth
		+ timing.HorizontalBackPorch;

	vvt = timing.VerticalActivePixels
		+ timing.VerticalFrontPorch
		+ timing.VerticalSyncWidth
		+ timing.VerticalBackPorch;

	desired_pClock = hht * vvt * vfreq / 1000;
	best_pClock = ps_best_pclock(&timing, desired_pClock);

	new_vvt = best_pClock * 1000 / (vfreq * hht);

	timing.VerticalBackPorch += (new_vvt - vvt);
	timing.PixelClockInKiloHertz = best_pClock;

	ps_set_monitor_timing(&timing);
	ps_get_monitor_timing(&timing);

	return 1;
}

//============================================================
//  pstrip_timing::ps_best_pclock
//============================================================

int pstrip_timing::ps_best_pclock(MonitorTiming *timing, int desired_pclock)
{
	MonitorTiming timing_read = {};
	int best_pclock = 0;

	log_verbose("PStrip: ps_best_pclock(%d), getting stable dotclocks for %d...\n", m_monitor_index, desired_pclock);

	for (int i = -50; i <= 50; i += 25)
	{
		timing->PixelClockInKiloHertz = desired_pclock + i;

		ps_set_monitor_timing(timing);
		ps_get_monitor_timing(&timing_read);

		if (abs(timing_read.PixelClockInKiloHertz - desired_pclock) < abs(desired_pclock - best_pclock))
			best_pclock = timing_read.PixelClockInKiloHertz;
	}

	log_verbose("PStrip: ps_best_pclock(%d), new dotclock: %d\n", m_monitor_index, best_pclock);

	return best_pclock;
}

//============================================================
//  pstrip_timing::ps_create_resolution
//============================================================

int pstrip_timing::ps_create_resolution(modeline *modeline)
{
	LRESULT     lresult;
	ATOM        atom;
	char        out[256];
	MonitorTiming timing = {};

	if (!hPSWnd) return 0;

	ps_modeline_to_pstiming(modeline, &timing);

	ps_fill_timing_string(out, &timing);
	atom = GlobalAddAtomA(out);

	if (atom)
	{
		lresult = SendMessage(hPSWnd, UM_CREATERESOLUTION, m_monitor_index, atom);

		if (lresult < 0)
			{
				log_verbose("PStrip: SendMessage failed\n");
				GlobalDeleteAtom(atom);
			}
			else
			{
				log_verbose("PStrip: ps_create_resolution(%d): %dx%d succeded \n",
					modeline->width, modeline->height, m_monitor_index);
				return 1;
			}
		}
		else log_verbose("PStrip: ps_create_resolution atom creation failed\n");

	return 0;
}

//============================================================
//  pstrip_timing::ps_read_timing_string
//============================================================

bool pstrip_timing::ps_read_timing_string(char *in, MonitorTiming *timing)
{
	if (sscanf(in,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
		&timing->HorizontalActivePixels,
		&timing->HorizontalFrontPorch,
		&timing->HorizontalSyncWidth,
		&timing->HorizontalBackPorch,
		&timing->VerticalActivePixels,
		&timing->VerticalFrontPorch,
		&timing->VerticalSyncWidth,
		&timing->VerticalBackPorch,
		&timing->PixelClockInKiloHertz,
		&timing->TimingFlags.w) == 10) return true;

	return false;
}

//============================================================
//  pstrip_timing::ps_fill_timing_string
//============================================================

void pstrip_timing::ps_fill_timing_string(char *out, MonitorTiming *timing)
{
	sprintf(out, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
		timing->HorizontalActivePixels,
		timing->HorizontalFrontPorch,
		timing->HorizontalSyncWidth,
		timing->HorizontalBackPorch,
		timing->VerticalActivePixels,
		timing->VerticalFrontPorch,
		timing->VerticalSyncWidth,
		timing->VerticalBackPorch,
		timing->PixelClockInKiloHertz,
		timing->TimingFlags.w);
}

//============================================================
//  pstrip_timing::ps_modeline_to_pstiming
//============================================================

bool pstrip_timing::ps_modeline_to_pstiming(modeline *modeline, MonitorTiming *timing)
{
	if (modeline->pclock == 0 || modeline->hactive == 0 || modeline->vactive == 0)
	{
		log_verbose("ps_modeline_to_pstiming error: invalid modeline\n");
		return false;
	}

	timing->HorizontalActivePixels = modeline->hactive;
	timing->HorizontalFrontPorch = modeline->hbegin - modeline->hactive;
	timing->HorizontalSyncWidth = modeline->hend - modeline->hbegin;
	timing->HorizontalBackPorch = modeline->htotal - modeline->hend;

	timing->VerticalActivePixels = modeline->vactive;
	timing->VerticalFrontPorch = modeline->vbegin - modeline->vactive;
	timing->VerticalSyncWidth = modeline->vend - modeline->vbegin;
	timing->VerticalBackPorch = modeline->vtotal - modeline->vend;

	timing->PixelClockInKiloHertz = modeline->pclock / 1000;

	if (modeline->hsync == 0)
		timing->TimingFlags.w |= NegativeHorizontalPolarity;
	if (modeline->vsync == 0)
		timing->TimingFlags.w |= NegativeVerticalPolarity;
	if (modeline->interlace)
		timing->TimingFlags.w |= Interlace;

	return true;
}

//============================================================
//  pstrip_timing::ps_pstiming_to_modeline
//============================================================

int pstrip_timing::ps_pstiming_to_modeline(MonitorTiming *timing, modeline *modeline)
{
	modeline->hactive = timing->HorizontalActivePixels;
	modeline->hbegin = modeline->hactive + timing->HorizontalFrontPorch;
	modeline->hend = modeline->hbegin + timing->HorizontalSyncWidth;
	modeline->htotal = modeline->hend + timing->HorizontalBackPorch;

	modeline->vactive = timing->VerticalActivePixels;
	modeline->vbegin = modeline->vactive + timing->VerticalFrontPorch;
	modeline->vend = modeline->vbegin + timing->VerticalSyncWidth;
	modeline->vtotal = modeline->vend + timing->VerticalBackPorch;

	modeline->width = modeline->hactive;
	modeline->height = modeline->vactive;

	modeline->pclock = timing->PixelClockInKiloHertz * 1000;

	if (!(timing->TimingFlags.w & NegativeHorizontalPolarity))
		modeline->hsync = 1;

	if (!(timing->TimingFlags.w & NegativeVerticalPolarity))
		modeline->vsync = 1;

	if ((timing->TimingFlags.w & Interlace))
		modeline->interlace = 1;

	modeline->hfreq = modeline->pclock / modeline->htotal;
	modeline->vfreq = modeline->hfreq / modeline->vtotal * (modeline->interlace?2:1);
	modeline->refresh = int(modeline->vfreq);

	return 0;
}

//============================================================
//  pstrip_timing::ps_monitor_index
//============================================================

int pstrip_timing::ps_monitor_index (const char *display_name)
{
	int monitor_index = 0;
	char sub_index[2];

	sub_index[0] = display_name[strlen(display_name)-1];
	sub_index[1] = 0;
	if (sscanf(sub_index,"%d", &monitor_index) == 1)
		monitor_index --;

	return monitor_index;
}

//============================================================
//  pstrip_timing::update_mode
//============================================================

bool pstrip_timing::update_mode(modeline *mode)
{
	if (!set_timing(mode))
	{
		return false;
	}

	mode->type |= CUSTOM_VIDEO_TIMING_POWERSTRIP;
	return true;
}
