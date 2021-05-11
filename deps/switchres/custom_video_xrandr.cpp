/**************************************************************

   custom_video_xrandr.cpp - Linux XRANDR video management layer

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#include <stdio.h>
#include <exception>
#include <dlfcn.h>
#include <string.h>
#include "custom_video_xrandr.h"
#include "log.h"

//============================================================
//  library functions
//============================================================

#define XRRAddOutputMode p_XRRAddOutputMode
#define XRRConfigCurrentConfiguration p_XRRConfigCurrentConfiguration
#define XRRCreateMode p_XRRCreateMode
#define XRRDeleteOutputMode p_XRRDeleteOutputMode
#define XRRDestroyMode p_XRRDestroyMode
#define XRRFreeCrtcInfo p_XRRFreeCrtcInfo
#define XRRFreeOutputInfo p_XRRFreeOutputInfo
#define XRRFreeScreenConfigInfo p_XRRFreeScreenConfigInfo
#define XRRFreeScreenResources p_XRRFreeScreenResources
#define XRRGetCrtcInfo p_XRRGetCrtcInfo
#define XRRGetOutputInfo p_XRRGetOutputInfo
#define XRRGetScreenInfo p_XRRGetScreenInfo
#define XRRGetScreenResourcesCurrent p_XRRGetScreenResourcesCurrent
#define XRRQueryVersion p_XRRQueryVersion
#define XRRSetCrtcConfig p_XRRSetCrtcConfig
#define XRRSetScreenSize p_XRRSetScreenSize
#define XRRGetScreenSizeRange p_XRRGetScreenSizeRange

#define XCloseDisplay p_XCloseDisplay
#define XGrabServer p_XGrabServer
#define XOpenDisplay p_XOpenDisplay
#define XSync p_XSync
#define XUngrabServer p_XUngrabServer
#define XSetErrorHandler p_XSetErrorHandler
#define XClearWindow p_XClearWindow
#define XFillRectangle p_XFillRectangle
#define XCreateGC p_XCreateGC

//============================================================
//  error_handler
//  xorg error handler (static)
//============================================================

int xrandr_timing::ms_xerrors = 0;
int xrandr_timing::ms_xerrors_flag = 0;
static int (*old_error_handler)(Display *, XErrorEvent *);

static __typeof__(XGetErrorText) *p_XGetErrorText;
#define XGetErrorText p_XGetErrorText

static int error_handler(Display *dpy, XErrorEvent *err)
{
	char buf[64];
	XGetErrorText(dpy, err->error_code, buf, 64);
	buf[0] = '\0';
	xrandr_timing::ms_xerrors |= xrandr_timing::ms_xerrors_flag;
	old_error_handler(dpy, err);
	log_error("XRANDR: <-> (error_handler) [ERROR] %s error code %d flags %02x\n", buf, err->error_code, xrandr_timing::ms_xerrors);
	return 0;
}

//============================================================
//  id for class object (static)
//============================================================

static int s_id = 0;

//============================================================
//  screen management exclusivity array (static)
//============================================================

static int s_total_managed_screen = 0;
static int *sp_shared_screen_manager = NULL;

//============================================================
//  desktop screen positions (static)
//============================================================

static XRRCrtcInfo *sp_desktop_crtc = NULL;

//============================================================
//  xrandr_timing::xrandr_timing
//============================================================

xrandr_timing::xrandr_timing(char *device_name, custom_video_settings *vs)
{
	m_vs = *vs;

	// Increment id for each new screen
	m_id = ++s_id;

	log_verbose("XRANDR: <%d> (xrandr_timing) creation (%s)\n", m_id, device_name);
	// Copy screen device name and limit size
	if ((strlen(device_name) + 1) > 32)
	{
		strncpy(m_device_name, device_name, 31);
		log_error("XRANDR: <%d> (xrandr_timing) [ERROR] the device name is too long it has been trucated to %s\n", m_id, m_device_name);
	}
	else
		strcpy(m_device_name, device_name);

	if (m_vs.screen_reordering)
	{
		if (m_id == 1)
			m_enable_screen_reordering = 1;
	}
	else if (m_vs.screen_compositing)
		m_enable_screen_compositing = 1;

	log_verbose("XRANDR: <%d> (xrandr_timing) checking X availability (early stub)\n", m_id);

	m_x11_handle = dlopen("libX11.so", RTLD_NOW);

	if (m_x11_handle)
	{
		p_XOpenDisplay = (__typeof__(XOpenDisplay)) dlsym(m_x11_handle, "XOpenDisplay");
		if (p_XOpenDisplay == NULL)
		{
			log_error("XRANDR: <%d> (xrandr_timing) [ERROR] missing func %s in %s\n", m_id, "XOpenDisplay", "X11_LIBRARY");
			throw new std::exception();
		}
		else
		{
			if (!XOpenDisplay(NULL))
			{
				log_verbose("XRANDR: <%d> (xrandr_timing) X server not found\n", m_id);
				throw new std::exception();
			}
		}
	}
	else
	{
		log_error("XRANDR: <%d> (xrandr_timing) [ERROR] missing %s library\n", m_id, "X11_LIBRARY");
		throw new std::exception();
	}

	s_total_managed_screen++;
}

//============================================================
//  xrandr_timing::~xrandr_timing
//============================================================

xrandr_timing::~xrandr_timing()
{
	s_total_managed_screen--;
	if (s_total_managed_screen == 0)
	{
		if (sp_desktop_crtc)
			delete[]sp_desktop_crtc;

		if (sp_shared_screen_manager)
			delete[]sp_shared_screen_manager;

		// Restore default desktop background
		XClearWindow(m_pdisplay, m_root);
	}

	// Free the display
	if (m_pdisplay != NULL)
		XCloseDisplay(m_pdisplay);

	// close Xrandr library
	if (m_xrandr_handle)
		dlclose(m_xrandr_handle);

	// close X11 library
	if (m_x11_handle)
		dlclose(m_x11_handle);
}

//============================================================
//  xrandr_timing::init
//============================================================

bool xrandr_timing::init()
{
	log_verbose("XRANDR: <%d> (init) loading Xrandr library\n", m_id);
	if (!m_xrandr_handle)
		m_xrandr_handle = dlopen("libXrandr.so", RTLD_NOW);
	if (m_xrandr_handle)
	{
		p_XRRAddOutputMode = (__typeof__(XRRAddOutputMode)) dlsym(m_xrandr_handle, "XRRAddOutputMode");
		if (p_XRRAddOutputMode == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s", m_id, "XRRAddOutputMode", "XRANDR_LIBRARY");
			return false;
		}

		p_XRRConfigCurrentConfiguration = (__typeof__(XRRConfigCurrentConfiguration)) dlsym(m_xrandr_handle, "XRRConfigCurrentConfiguration");
		if (p_XRRConfigCurrentConfiguration == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s", m_id, "XRRConfigCurrentConfiguration", "XRANDR_LIBRARY");
			return false;
		}

		p_XRRCreateMode = (__typeof__(XRRCreateMode)) dlsym(m_xrandr_handle, "XRRCreateMode");
		if (p_XRRCreateMode == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s", m_id, "XRRCreateMode", "XRANDR_LIBRARY");
			return false;
		}

		p_XRRDeleteOutputMode = (__typeof__(XRRDeleteOutputMode)) dlsym(m_xrandr_handle, "XRRDeleteOutputMode");
		if (p_XRRDeleteOutputMode == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s", m_id, "XRRDeleteOutputMode", "XRANDR_LIBRARY");
			return false;
		}

		p_XRRDestroyMode = (__typeof__(XRRDestroyMode)) dlsym(m_xrandr_handle, "XRRDestroyMode");
		if (p_XRRDestroyMode == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s", m_id, "XRRDestroyMode", "XRANDR_LIBRARY");
			return false;
		}

		p_XRRFreeCrtcInfo = (__typeof__(XRRFreeCrtcInfo)) dlsym(m_xrandr_handle, "XRRFreeCrtcInfo");
		if (p_XRRFreeCrtcInfo == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s", m_id, "XRRFreeCrtcInfo", "XRANDR_LIBRARY");
			return false;
		}

		p_XRRFreeOutputInfo = (__typeof__(XRRFreeOutputInfo)) dlsym(m_xrandr_handle, "XRRFreeOutputInfo");
		if (p_XRRFreeOutputInfo == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s", m_id, "XRRFreeOutputInfo", "XRANDR_LIBRARY");
			return false;
		}

		p_XRRFreeScreenConfigInfo = (__typeof__(XRRFreeScreenConfigInfo)) dlsym(m_xrandr_handle, "XRRFreeScreenConfigInfo");
		if (p_XRRFreeScreenConfigInfo == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s", m_id, "XRRFreeScreenConfigInfo", "XRANDR_LIBRARY");
			return false;
		}

		p_XRRFreeScreenResources = (__typeof__(XRRFreeScreenResources)) dlsym(m_xrandr_handle, "XRRFreeScreenResources");
		if (p_XRRFreeScreenResources == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s", m_id, "XRRFreeScreenResources", "XRANDR_LIBRARY");
			return false;
		}

		p_XRRGetCrtcInfo = (__typeof__(XRRGetCrtcInfo)) dlsym(m_xrandr_handle, "XRRGetCrtcInfo");
		if (p_XRRGetCrtcInfo == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s", m_id, "XRRGetCrtcInfo", "XRANDR_LIBRARY");
			return false;
		}

		p_XRRGetOutputInfo = (__typeof__(XRRGetOutputInfo)) dlsym(m_xrandr_handle, "XRRGetOutputInfo");
		if (p_XRRGetOutputInfo == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s", m_id, "XRRGetOutputInfo", "XRANDR_LIBRARY");
			return false;
		}

		p_XRRGetScreenInfo = (__typeof__(XRRGetScreenInfo)) dlsym(m_xrandr_handle, "XRRGetScreenInfo");
		if (p_XRRGetScreenInfo == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s", m_id, "XRRGetScreenInfo", "XRANDR_LIBRARY");
			return false;
		}

		p_XRRGetScreenResourcesCurrent = (__typeof__(XRRGetScreenResourcesCurrent)) dlsym(m_xrandr_handle, "XRRGetScreenResourcesCurrent");
		if (p_XRRGetScreenResourcesCurrent == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s", m_id, "XRRGetScreenResourcesCurrent", "XRANDR_LIBRARY");
			return false;
		}

		p_XRRQueryVersion = (__typeof__(XRRQueryVersion)) dlsym(m_xrandr_handle, "XRRQueryVersion");
		if (p_XRRQueryVersion == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s", m_id, "XRRQueryVersion", "XRANDR_LIBRARY");
			return false;
		}

		p_XRRSetCrtcConfig = (__typeof__(XRRSetCrtcConfig)) dlsym(m_xrandr_handle, "XRRSetCrtcConfig");
		if (p_XRRSetCrtcConfig == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s", m_id, "XRRSetCrtcConfig", "XRANDR_LIBRARY");
			return false;
		}

		p_XRRSetScreenSize = (__typeof__(XRRSetScreenSize)) dlsym(m_xrandr_handle, "XRRSetScreenSize");
		if (p_XRRSetScreenSize == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s", m_id, "XRRSetScreenSize", "XRANDR_LIBRARY");
			return false;
		}

		p_XRRGetScreenSizeRange = (__typeof__(XRRGetScreenSizeRange)) dlsym(m_xrandr_handle, "XRRGetScreenSizeRange");
		if (p_XRRGetScreenSizeRange == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s", m_id, "XRRSetScreenSize", "XRANDR_LIBRARY");
			return false;
		}
	}
	else
	{
		log_error("XRANDR: <%d> (init) [ERROR] missing %s library\n", m_id, "XRANDR_LIBRARY");
		return false;
	}

	log_verbose("XRANDR: <%d> (init) loading X11 library\n", m_id);
	if (!m_x11_handle)
		m_x11_handle = dlopen("libX11.so", RTLD_NOW);
	if (m_x11_handle)
	{
		p_XCloseDisplay = (__typeof__(XCloseDisplay)) dlsym(m_x11_handle, "XCloseDisplay");
		if (p_XCloseDisplay == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s\n", m_id, "XCloseDisplay", "X11_LIBRARY");
			return false;
		}

		p_XGrabServer = (__typeof__(XGrabServer)) dlsym(m_x11_handle, "XGrabServer");
		if (p_XGrabServer == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s\n", m_id, "XGrabServer", "X11_LIBRARY");
			return false;
		}

		p_XOpenDisplay = (__typeof__(XOpenDisplay)) dlsym(m_x11_handle, "XOpenDisplay");
		if (p_XOpenDisplay == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s\n", m_id, "XOpenDisplay", "X11_LIBRARY");
			return false;
		}

		p_XSync = (__typeof__(XSync)) dlsym(m_x11_handle, "XSync");
		if (p_XSync == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s\n", m_id, "XSync", "X11_LIBRARY");
			return false;
		}

		p_XUngrabServer = (__typeof__(XUngrabServer)) dlsym(m_x11_handle, "XUngrabServer");
		if (p_XUngrabServer == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s\n", m_id, "XUngrabServer", "X11_LIBRARY");
			return false;
		}

		p_XSetErrorHandler = (__typeof__(XSetErrorHandler)) dlsym(m_x11_handle, "XSetErrorHandler");
		if (p_XSetErrorHandler == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s\n", m_id, "XSetErrorHandler", "X11_LIBRARY");
			return false;
		}

		p_XGetErrorText = (__typeof__(XGetErrorText)) dlsym(m_x11_handle, "XGetErrorText");
		if (p_XGetErrorText == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s\n", m_id, "XGetErrorText", "X11_LIBRARY");
			return false;
		}

		p_XClearWindow = (__typeof__(XClearWindow)) dlsym(m_x11_handle, "XClearWindow");
		if (p_XClearWindow == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s", m_id, "XClearWindow", "X11_LIBRARY");
			return false;
		}

		p_XFillRectangle = (__typeof__(XFillRectangle)) dlsym(m_x11_handle, "XFillRectangle");
		if (p_XFillRectangle == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s", m_id, "XFillRectangle", "X11_LIBRARY");
			return false;
		}

		p_XCreateGC = (__typeof__(XCreateGC)) dlsym(m_x11_handle, "XCreateGC");
		if (p_XCreateGC == NULL)
		{
			log_error("XRANDR: <%d> (init) [ERROR] missing func %s in %s", m_id, "XCreateGC", "X11_LIBRARY");
			return false;
		}
	}
	else
	{
		log_error("XRANDR: <%d> (init) [ERROR] missing %s library\n", m_id, "X11_LIBRARY");
		return false;
	}

	// Select current display and root window
	// m_pdisplay is global to reduce open/close calls, resource is freed when class is destroyed
	if (!m_pdisplay)
		m_pdisplay = XOpenDisplay(NULL);

	if (!m_pdisplay)
	{
		log_error("XRANDR: <%d> (init) [ERROR] failed to connect to the X server\n", m_id);
		return false;
	}

	// Display XRANDR version
	int major_version, minor_version;
	XRRQueryVersion(m_pdisplay, &major_version, &minor_version);
	log_verbose("XRANDR: <%d> (init) version %d.%d\n", m_id, major_version, minor_version);

	if (major_version < 1 || (major_version == 1 && minor_version < 2))
	{
		log_error("XRANDR: <%d> (init) [ERROR] Xrandr version 1.2 or above is required\n", m_id);
		return false;
	}

	// screen_pos defines screen position, 0 is default first screen position and equivalent to 'auto'
	int screen_pos = -1;
	bool detected = false;

	// Handle the screen name, "auto", "screen[0-9]" and XRANDR device name
	if (strlen(m_device_name) == 7 && !strncmp(m_device_name, "screen", 6) && m_device_name[6] >= '0' && m_device_name[6] <= '9')
		screen_pos = m_device_name[6] - '0';
	else if (strlen(m_device_name) == 1 && m_device_name[0] >= '0' && m_device_name[0] <= '9')
		screen_pos = m_device_name[0] - '0';

	if (ScreenCount(m_pdisplay) > 1)
		log_verbose("XRANDR: <%d> (init) [WARNING] screen count is %d, unpredictable behavior to be expected\n", m_id, ScreenCount(m_pdisplay));

	for (int screen = 0; !detected && screen < ScreenCount(m_pdisplay); screen++)
	{
		log_verbose("XRANDR: <%d> (init) check screen number %d\n", m_id, screen);
		m_screen = screen;
		m_root = RootWindow(m_pdisplay, screen);

		XRRScreenResources *resources = XRRGetScreenResourcesCurrent(m_pdisplay, m_root);

		if (m_id == 1)
		{
			// Prepare the shared screen array
			sp_shared_screen_manager = new int[resources->noutput];
			for (int o = 0; o < resources->noutput; o++)
				sp_shared_screen_manager[o] = 0;

			// Save all active crtc positions
			sp_desktop_crtc = new XRRCrtcInfo[resources->ncrtc];
			for (int c = 0; c < resources->ncrtc; c++)
				memcpy(&sp_desktop_crtc[c], XRRGetCrtcInfo(m_pdisplay, resources, resources->crtcs[c]), sizeof(XRRCrtcInfo));
		}

		// Get default screen rotation from screen configuration
		XRRScreenConfiguration *sc = XRRGetScreenInfo(m_pdisplay, m_root);
		XRRConfigCurrentConfiguration(sc, &m_desktop_rotation);
		XRRFreeScreenConfigInfo(sc);

		Rotation current_rotation = 0;
		int output_position = 0;
		for (int o = 0; o < resources->noutput; o++)
		{
			XRROutputInfo *output_info = XRRGetOutputInfo(m_pdisplay, resources, resources->outputs[o]);
			if (!output_info)
			{
				log_error("XRANDR: <%d> (init) [ERROR] could not get output 0x%x information\n", m_id, (unsigned int)resources->outputs[o]);
				continue;
			}
			// Check all connected output
			if (m_desktop_output == -1 && output_info->connection == RR_Connected && output_info->crtc)
			{

				if (!strcmp(m_device_name, "auto") || !strcmp(m_device_name, output_info->name) || output_position == screen_pos)
				{
					// store the output connector
					m_desktop_output = o;

					// store screen minium and maximum resolutions
					int min_width;
					int max_width;
					int min_height;
					int max_height;
					XRRGetScreenSizeRange (m_pdisplay, m_root, &min_width, &min_height, &max_width, &max_height);
					m_min_width = min_width;
					m_max_width = max_width;
					m_min_height = min_height;
					m_max_height = max_height;

					if (sp_shared_screen_manager[m_desktop_output] == 0)
					{
						sp_shared_screen_manager[m_desktop_output] = m_id;
						m_managed = 1;
					}

					// identify the current modeline and rotation
					XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(m_pdisplay, resources, output_info->crtc);
					current_rotation = crtc_info->rotation;
					for (int m = 0; m < resources->nmode && m_desktop_mode.id == 0; m++)
					{
						// Get screen mode
						if (crtc_info->mode == resources->modes[m].id)
						{
							m_desktop_mode = resources->modes[m];
							m_last_crtc = *crtc_info;
						}
					}
					XRRFreeCrtcInfo(crtc_info);

					// check screen rotation (left or right)
					if (current_rotation & 0xe)
					{
						m_crtc_flags = MODE_ROTATED;
						log_verbose("XRANDR: <%d> (init) desktop rotation is %s\n", m_id, (current_rotation & 0x2) ? "left" : ((current_rotation & 0x8) ? "right" : "inverted"));
					}
				}
				output_position++;
			}
			log_verbose("XRANDR: <%d> (init) check output connector '%s' active %d crtc %d %s\n", m_id, output_info->name, output_info->connection == RR_Connected ? 1 : 0, output_info->crtc ? 1 : 0, m_desktop_output == o ? (m_managed ? "[SELECTED]" : "[UNMANAGED]") : "");
			XRRFreeOutputInfo(output_info);
		}
		XRRFreeScreenResources(resources);

		// Check if screen has been detected
		detected = m_desktop_output != -1;
	}

	if (!detected)
		log_error("XRANDR: <%d> (init) [ERROR] no screen detected\n", m_id);

	else if (m_enable_screen_reordering)
	{
		// Global screen placement
		modeline mode = {};
		mode.type = MODE_DESKTOP;
		set_timing(&mode, XRANDR_ENABLE_SCREEN_REORDERING);
	}

	return detected;
}

//============================================================
//  xrandr_timing::update_mode
//============================================================

bool xrandr_timing::update_mode(modeline *mode)
{
	if (!mode)
		return false;

	// Handle no screen detected case
	if (m_desktop_output == -1)
	{
		log_error("XRANDR: <%d> (update_mode) [ERROR] no screen detected\n", m_id);
		return false;
	}

	if (!delete_mode(mode))
	{
		log_error("XRANDR: <%d> (update_mode) [ERROR] delete operation not successful", m_id);
		return false;
	}

	if (!add_mode(mode))
	{
		log_error("XRANDR: <%d> (update_mode) [ERROR] add operation not successful", m_id);
		return false;
	}

	return true;
}

//============================================================
//  xrandr_timing::add_mode
//============================================================

bool xrandr_timing::add_mode(modeline *mode)
{
	if (!mode)
		return false;

	// Handle no screen detected case
	if (m_desktop_output == -1)
	{
		log_error("XRANDR: <%d> (add_mode) [ERROR] no screen detected\n", m_id);
		return false;
	}

	if (!m_managed)
	{
		log_error("XRANDR: <%d> (add_mode) [WARNING] this screen is managed by <%d>\n", m_id, sp_shared_screen_manager[m_desktop_output]);
		return false;
	}

	// Check if mode is available from the plaftform_data mode id
	XRRModeInfo *pxmode = find_mode(mode);
	if (pxmode != NULL)
	{
		log_error("XRANDR: <%d> (add_mode) [WARNING] mode already exist\n", m_id);
		return true;
	}

	// Create specific mode name
	char name[48];
	sprintf(name, "SR-%d_%dx%d@%.02f%s", m_id, mode->hactive, mode->vactive, mode->vfreq, mode->interlace ? "i" : "");

	// Check if mode is available from the SR name (should not be the case, otherwise it means that we recevied twice the same mode request)
	pxmode = find_mode_by_name(name);
	if (pxmode != NULL)
	{
		log_error("XRANDR: <%d> (add_mode) [WARNING] mode already exist (duplicate request)\n", m_id);
		mode->platform_data = pxmode->id;
		return true;
	}

	log_verbose("XRANDR: <%d> (add_mode) create mode %s\n", m_id, name);

	// Setup the xrandr mode structure
	XRRModeInfo xmode = {};

	xmode.name       = name;
	xmode.nameLength = strlen(name);
	xmode.dotClock   = mode->pclock;
	xmode.width      = mode->hactive;
	xmode.hSyncStart = mode->hbegin;
	xmode.hSyncEnd   = mode->hend;
	xmode.hTotal     = mode->htotal;
	xmode.height     = mode->vactive;
	xmode.vSyncStart = mode->vbegin;
	xmode.vSyncEnd   = mode->vend;
	xmode.vTotal     = mode->vtotal;
	xmode.modeFlags  = (mode->interlace ? RR_Interlace : 0) | (mode->doublescan ? RR_DoubleScan : 0) | (mode->hsync ? RR_HSyncPositive : RR_HSyncNegative) | (mode->vsync ? RR_VSyncPositive : RR_VSyncNegative);
	xmode.hSkew      = 0;

	mode->type |= CUSTOM_VIDEO_TIMING_XRANDR;

	// Create the modeline
	XSync(m_pdisplay, False);
	ms_xerrors = 0;
	ms_xerrors_flag = 0x01;
	old_error_handler = XSetErrorHandler(error_handler);
	RRMode gmid = XRRCreateMode(m_pdisplay, m_root, &xmode);
	XSync(m_pdisplay, False);
	XSetErrorHandler(old_error_handler);
	if (ms_xerrors & ms_xerrors_flag)
	{
		log_error("XRANDR: <%d> (add_mode) [ERROR] in %s\n", m_id, "XRRCreateMode");
		return false;
	}

	mode->platform_data = gmid;

	// Add new modeline to primary output
	XRRScreenResources *resources = XRRGetScreenResourcesCurrent(m_pdisplay, m_root);

	XSync(m_pdisplay, False);
	ms_xerrors_flag = 0x02;
	old_error_handler = XSetErrorHandler(error_handler);
	XRRAddOutputMode(m_pdisplay, resources->outputs[m_desktop_output], mode->platform_data);
	XSync(m_pdisplay, False);
	XSetErrorHandler(old_error_handler);

	XRRFreeScreenResources(resources);

	if (ms_xerrors & ms_xerrors_flag)
	{
		log_error("XRANDR: <%d> (add_mode) [ERROR] in %s\n", m_id, "XRRAddOutputMode");

		// remove unlinked modeline
		if (mode->platform_data)
		{
			log_error("XRANDR: <%d> (add_mode) [ERROR] remove mode [%04lx]\n", m_id, mode->platform_data);
			XRRDestroyMode(m_pdisplay, mode->platform_data);
			mode->platform_data = 0;
		}
	}
	else
		log_verbose("XRANDR: <%d> (add_mode) mode %04lx %dx%d refresh %.6f added\n", m_id, mode->platform_data, mode->hactive, mode->vactive, mode->vfreq);

	return ms_xerrors == 0;
}

//============================================================
//  xrandr_timing::find_mode_by_name
//============================================================

XRRModeInfo *xrandr_timing::find_mode_by_name(char *name)
{
	XRRModeInfo *pxmode = NULL;
	XRRScreenResources *resources = XRRGetScreenResourcesCurrent(m_pdisplay, m_root);

	// use SR name to return the mode
	for (int m = 0; m < resources->nmode; m++)
	{
		if (strcmp(resources->modes[m].name, name) == 0)
		{
			pxmode = &resources->modes[m];
			break;
		}
	}

	XRRFreeScreenResources(resources);

	return pxmode;
}

//============================================================
//  xrandr_timing::find_mode
//============================================================

XRRModeInfo *xrandr_timing::find_mode(modeline *mode)
{
	XRRModeInfo *pxmode = NULL;
	XRRScreenResources *resources = XRRGetScreenResourcesCurrent(m_pdisplay, m_root);

	// use platform_data (mode id) to return the mode
	for (int m = 0; m < resources->nmode; m++)
	{
		if (mode->platform_data == resources->modes[m].id)
		{
			pxmode = &resources->modes[m];
			break;
		}
	}

	XRRFreeScreenResources(resources);

	return pxmode;
}

//============================================================
//  xrandr_timing::set_timing
//============================================================

bool xrandr_timing::set_timing(modeline *mode)
{
	if (m_enable_screen_compositing)
		return set_timing(mode, 0);

	return set_timing(mode, XRANDR_DISABLE_CRTC_RELOCATION);
}

//============================================================
//  xrandr_timing::set_timing
//============================================================

bool xrandr_timing::set_timing(modeline *mode, int flags)
{
	// Handle no screen detected case
	if (m_desktop_output == -1)
	{
		log_error("XRANDR: <%d> (set_timing) [ERROR] no screen detected\n", m_id);
		return false;
	}

	if (!m_managed)
	{
		log_error("XRANDR: <%d> (set_timing) [WARNING] this screen is managed by <%d>\n", m_id, sp_shared_screen_manager[m_desktop_output]);
		return false;
	}

	if (m_id != 1 && (flags & XRANDR_ENABLE_SCREEN_REORDERING))
		flags = XRANDR_DISABLE_CRTC_RELOCATION; // only master can do global screen preparation

	XRRModeInfo *pxmode = NULL;

	if (mode->type & MODE_DESKTOP)
		pxmode = &m_desktop_mode;
	else
		pxmode = find_mode(mode);

	if (pxmode == NULL)
	{
		log_error("XRANDR: <%d> (set_timing) [ERROR] mode not found\n", m_id);
		return false;
	}

	// Use xrandr to switch to new mode.
	XRRScreenResources *resources = XRRGetScreenResourcesCurrent(m_pdisplay, m_root);
	XRROutputInfo *output_info = XRRGetOutputInfo(m_pdisplay, resources, resources->outputs[m_desktop_output]);
	XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(m_pdisplay, resources, output_info->crtc);

	if (flags & XRANDR_DISABLE_CRTC_RELOCATION)
		log_verbose("XRANDR: <%d> (set_timing) DISABLE crtc relocation\n", m_id);

	if (flags & XRANDR_ENABLE_SCREEN_REORDERING)
		log_verbose("XRANDR: <%d> (set_timing) GLOBAL desktop screen preparation\n", m_id);
	else if (m_last_crtc.mode == crtc_info->mode && m_last_crtc.x == crtc_info->x && m_last_crtc.y == crtc_info->y && pxmode->id == crtc_info->mode)
		log_verbose("XRANDR: <%d> (set_timing) requested mode is already active [%04lx] %ux%u+%d+%d\n", m_id, crtc_info->mode, crtc_info->width, crtc_info->height, crtc_info->x, crtc_info->y);
	else if (m_last_crtc.mode != crtc_info->mode)
	{
		log_verbose("XRANDR: <%d> (set_timing) [WARNING] unexpected active modeline detected (last:[%04lx] now:[%04lx] %ux%u+%d+%d want:[%04lx])\n", m_id, m_last_crtc.mode, crtc_info->mode, crtc_info->width, crtc_info->height, crtc_info->x, crtc_info->y, pxmode->id);
		*crtc_info = m_last_crtc;
	}

	// Grab X server to prevent unwanted interaction from the window manager
	XGrabServer(m_pdisplay);

	unsigned int width = m_min_width;
	unsigned int height = m_min_height;

	unsigned int active_crtc = 0;

	unsigned int reordering_last_y = 0;

	ms_xerrors = 0;

	XRRCrtcInfo *global_crtc = new XRRCrtcInfo[resources->ncrtc];
	XRRCrtcInfo *original_crtc = new XRRCrtcInfo[resources->ncrtc];

	// caculate necessary screen size and of crtc neighborhood if they have at least one side aligned with the mode changed crtc
	for (int c = 0; c < resources->ncrtc; c++)
	{
		// Prepare crtc references
		memcpy(&original_crtc[c], XRRGetCrtcInfo(m_pdisplay, resources, resources->crtcs[c]), sizeof(XRRCrtcInfo));
		memcpy(&global_crtc[c], XRRGetCrtcInfo(m_pdisplay, resources, resources->crtcs[c]), sizeof(XRRCrtcInfo));
		// Original state
		XRRCrtcInfo *crtc_info0 = &original_crtc[c];
		// Modified state
		XRRCrtcInfo *crtc_info1 = &global_crtc[c];
		// clear timestamp
		crtc_info1->timestamp = 0;

		// Skip unused crtc
		if (output_info->crtc != 0 && crtc_info0->mode != 0)
		{
			if (flags & XRANDR_ENABLE_SCREEN_REORDERING)
			{
				// Relocate all crtcs
				// Super resolution placement, vertical stacking, reserved XRANDR_REORDERING_MAXIMUM_HEIGHT pixels
				crtc_info1->x = 0;
				crtc_info1->y = reordering_last_y;
				if (crtc_info1->height > XRANDR_REORDERING_MAXIMUM_HEIGHT)
					reordering_last_y += crtc_info1->height;
				else
					reordering_last_y += XRANDR_REORDERING_MAXIMUM_HEIGHT;
				crtc_info1->timestamp |= XRANDR_SETMODE_UPDATE_REORDERING;
				active_crtc++;
			}
			// Switchres selected desktop output
			else if (resources->crtcs[c] == output_info->crtc)
			{
				crtc_info1->timestamp |= XRANDR_SETMODE_IS_DESKTOP;
				crtc_info1->mode = pxmode->id;
				crtc_info1->width = pxmode->width;
				crtc_info1->height = pxmode->height;

				if (mode->type & MODE_DESKTOP)
				{
					if (!m_enable_screen_compositing && (crtc_info1->x != sp_desktop_crtc[c].x || crtc_info1->y != sp_desktop_crtc[c].y))
					{
						// Restore original desktop position
						crtc_info1->x = sp_desktop_crtc[c].x;
						crtc_info1->y = sp_desktop_crtc[c].y;
						crtc_info1->timestamp |= XRANDR_SETMODE_RESTORE_DESKTOP;
					}
				}
				else
				{
					// Use curent position
					crtc_info1->x = crtc_info->x;
					crtc_info1->y = crtc_info->y;
				}

				if (crtc_info0->mode != crtc_info1->mode || crtc_info0->width != crtc_info1->width || crtc_info0->height != crtc_info1->height || crtc_info0->x != crtc_info1->x || crtc_info0->y != crtc_info1->y)
					crtc_info1->timestamp |= XRANDR_SETMODE_UPDATE_DESKTOP_CRTC;
			}
			else if (mode->type & MODE_DESKTOP && m_enable_screen_reordering && (crtc_info1->x != sp_desktop_crtc[c].x || crtc_info1->y != sp_desktop_crtc[c].y))
			{
				crtc_info1->x = sp_desktop_crtc[c].x;
				crtc_info1->y = sp_desktop_crtc[c].y;
				crtc_info1->timestamp |= (XRANDR_SETMODE_RESTORE_DESKTOP | XRANDR_SETMODE_UPDATE_REORDERING);
			}
		}
	}

	for (int c = 0; c < resources->ncrtc; c++)
	{
		// Original state
		XRRCrtcInfo *crtc_info0 = &original_crtc[c];
		// Modified state
		XRRCrtcInfo *crtc_info1 = &global_crtc[c];

		// Skip unused crtc
		if (output_info->crtc != 0 && crtc_info0->mode != 0)
		{
			if ((flags & XRANDR_DISABLE_CRTC_RELOCATION) == 0 && (crtc_info1->timestamp & XRANDR_SETMODE_IS_DESKTOP) == 0)
			{
				// relocate crtc impacted by new width
				if (crtc_info1->x >= crtc_info->x + (int)crtc_info->width)
				{
					crtc_info1->x += pxmode->width - crtc_info->width;
					crtc_info1->timestamp |= XRANDR_SETMODE_UPDATE_OTHER_CRTC;
				}

				// relocate crtc impacted by new height
				if (crtc_info1->y >= crtc_info->y + (int)crtc_info->height)
				{
					crtc_info1->y += pxmode->height - crtc_info->height;
					crtc_info1->timestamp |= XRANDR_SETMODE_UPDATE_OTHER_CRTC;
				}
			}

			// Calculate overall screen size based on crtcs placement
			if (crtc_info1->x + crtc_info1->width > width)
				width = crtc_info1->x + crtc_info1->width;

			if (crtc_info1->y + crtc_info1->height > height)
				height = crtc_info1->y + crtc_info1->height;

			if (width > m_max_width)
			{
				log_error("XRANDR: <%d> (set_timing) [ERROR] width is above allowed maximum (%d > %d)\n", m_id, width, m_max_width);
				width = m_max_width;
			}

			if (height > m_max_height)
			{
				log_error("XRANDR: <%d> (set_timing) [ERROR] height is above allowed maximum (%d > %d)\n", m_id, height, m_max_height);
				height = m_max_height;
			}

			if (crtc_info1->timestamp & XRANDR_SETMODE_UPDATE_MASK)
				log_verbose("XRANDR: <%d> (set_timing) crtc %d%s [%04lx] %ux%u+%d+%d --> [%04lx] %ux%u+%d+%d flags [%02lx]\n", m_id, c, crtc_info1->timestamp & 1 ? "*" : " ", crtc_info0->mode, crtc_info0->width, crtc_info0->height, crtc_info0->x, crtc_info0->y, crtc_info1->mode, crtc_info1->width, crtc_info1->height, crtc_info1->x, crtc_info1->y, crtc_info1->timestamp);
			else if (crtc_info1->timestamp & XRANDR_SETMODE_INFO_MASK)
				log_verbose("XRANDR: <%d> (set_timing) crtc %d%s [%04lx] %ux%u+%d+%d flags [%02lx]\n", m_id, c, crtc_info1->timestamp & 1 ? "*" : " ", crtc_info1->mode, crtc_info1->width, crtc_info1->height, crtc_info1->x, crtc_info1->y, crtc_info1->timestamp);
			else
				log_verbose("XRANDR: <%d> (set_timing) crtc %d  [%04lx] %ux%u+%d+%d\n", m_id, c, crtc_info1->mode, crtc_info1->width, crtc_info1->height, crtc_info1->x, crtc_info1->y);
		}
	}

	// Disable crtc with pending modification
	for (int c = 0; c < resources->ncrtc; c++)
	{
		// Modified state
		if (global_crtc[c].timestamp & XRANDR_SETMODE_UPDATE_MASK)
		{
			if (XRRSetCrtcConfig(m_pdisplay, resources, resources->crtcs[c], CurrentTime, 0, 0, None, RR_Rotate_0, NULL, 0) != RRSetConfigSuccess)
			{
				log_error("XRANDR: <%d> (set_timing) [ERROR] when disabling crtc %d\n", m_id, c);
				ms_xerrors_flag = 0x01;
				ms_xerrors |= ms_xerrors_flag;
			}
		}
	}

	// Set the framebuffer screen size to enable all crtc
	if (ms_xerrors == 0)
	{
		log_verbose("XRANDR: <%d> (set_timing) setting screen size to %d x %d\n", m_id, width, height);
		XSync(m_pdisplay, False);
		ms_xerrors_flag = 0x02;
		old_error_handler = XSetErrorHandler(error_handler);
		XRRSetScreenSize(m_pdisplay, m_root, width, height, (int) ((25.4 * width) / 96.0), (int) ((25.4 * height) / 96.0));
		XSync(m_pdisplay, False);
		XSetErrorHandler(old_error_handler);
		if (ms_xerrors & ms_xerrors_flag)
			log_error("XRANDR: <%d> (set_timing) [ERROR] in %s\n", m_id, "XRRSetScreenSize");
	}

	// Refresh all crtc, switch modeline and set new placement
	for (int c = 0; c < resources->ncrtc; c++)
	{
		// Modified state
		XRRCrtcInfo *crtc_info1 = &global_crtc[c];
		if (crtc_info1->timestamp & XRANDR_SETMODE_UPDATE_MASK)
		{
			if (crtc_info1->timestamp & XRANDR_SETMODE_IS_DESKTOP)
				XFillRectangle(m_pdisplay, m_root, XCreateGC(m_pdisplay, m_root, 0, 0), crtc_info1->x, crtc_info1->y, crtc_info1->width, crtc_info1->height);
			// enable crtc with updated parameters
			XSync(m_pdisplay, False);
			ms_xerrors_flag = 0x14;
			old_error_handler = XSetErrorHandler(error_handler);
			XRRSetCrtcConfig(m_pdisplay, resources, resources->crtcs[c], CurrentTime, crtc_info1->x, crtc_info1->y, crtc_info1->mode, crtc_info1->rotation, crtc_info1->outputs, crtc_info1->noutput);
			XSync(m_pdisplay, False);
			XSetErrorHandler(old_error_handler);
			if (ms_xerrors & 0x10)
			{
				log_error("XRANDR: <%d> (set_timing) [ERROR] in %s crtc %d set modeline %04lx\n", m_id, "XRRSetCrtcConfig", c, crtc_info1->mode);
				ms_xerrors &= 0xEF;
			}
		}
	}
	delete[]original_crtc;
	delete[]global_crtc;

	// Release X server, events can be processed now
	XUngrabServer(m_pdisplay);

	if (ms_xerrors & ms_xerrors_flag)
		log_error("XRANDR: <%d> (set_timing) [ERROR] in %s\n", m_id, "XRRSetCrtcConfig");

	// Recall the impacted crtc to settle parameters
	XRRFreeCrtcInfo(crtc_info);
	crtc_info = XRRGetCrtcInfo(m_pdisplay, resources, output_info->crtc);

	// crtc config modeline change fail
	if (crtc_info->mode == 0)
		log_error("XRANDR: <%d> (set_timing) [ERROR] switching resolution failed, no modeline is set\n", m_id);
	else
		// save last crtc
		m_last_crtc = *crtc_info;

	XRRFreeCrtcInfo(crtc_info);
	XRRFreeOutputInfo(output_info);
	XRRFreeScreenResources(resources);

	return (ms_xerrors == 0 && crtc_info->mode != 0);
}

//============================================================
//  xrandr_timing::delete_mode
//============================================================

bool xrandr_timing::delete_mode(modeline *mode)
{
	// Handle no screen detected case
	if (m_desktop_output == -1)
	{
		log_error("XRANDR: <%d> (delete_mode) [ERROR] no screen detected\n", m_id);
		return false;
	}

	if (!m_managed)
	{
		log_error("XRANDR: <%d> (delete_mode) [WARNING] this screen is managed by <%d>\n", m_id, sp_shared_screen_manager[m_desktop_output]);
		return false;
	}

	if (!mode)
		return false;

	XRRScreenResources *resources = XRRGetScreenResourcesCurrent(m_pdisplay, m_root);

	int total_xerrors = 0;
	// Delete modeline
	for (int m = 0; m < resources->nmode && mode->platform_data != 0; m++)
	{
		if (mode->platform_data == resources->modes[m].id)
		{
			XRROutputInfo *output_info = XRRGetOutputInfo(m_pdisplay, resources, resources->outputs[m_desktop_output]);
			XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(m_pdisplay, resources, output_info->crtc);
			if (resources->modes[m].id == crtc_info->mode)
				log_verbose("XRANDR: <%d> (delete_mode) [WARNING] modeline [%04lx] is currently active\n", m_id, resources->modes[m].id);

			XRRFreeCrtcInfo(crtc_info);
			XRRFreeOutputInfo(output_info);

			log_verbose("XRANDR: <%d> (delete_mode) remove mode %s\n", m_id, resources->modes[m].name);

			XSync(m_pdisplay, False);
			ms_xerrors = 0;
			ms_xerrors_flag = 0x01;
			old_error_handler = XSetErrorHandler(error_handler);
			XRRDeleteOutputMode(m_pdisplay, resources->outputs[m_desktop_output], resources->modes[m].id);
			if (ms_xerrors & ms_xerrors_flag)
			{
				log_error("XRANDR: <%d> (delete_mode) [ERROR] in %s\n", m_id, "XRRDeleteOutputMode");
				total_xerrors++;
			}

			ms_xerrors_flag = 0x02;
			XRRDestroyMode(m_pdisplay, resources->modes[m].id);
			XSync(m_pdisplay, False);
			XSetErrorHandler(old_error_handler);
			if (ms_xerrors & ms_xerrors_flag)
			{
				log_error("XRANDR: <%d> (delete_mode) [ERROR] in %s\n", m_id, "XRRDestroyMode");
				total_xerrors++;
			}
			mode->platform_data = 0;
		}
	}

	XRRFreeScreenResources(resources);

	return total_xerrors == 0;
}

//============================================================
//  xrandr_timing::get_timing
//============================================================

bool xrandr_timing::get_timing(modeline *mode)
{
	// Handle no screen detected case
	if (m_desktop_output == -1)
	{
		log_error("XRANDR: <%d> (get_timing) [ERROR] no screen detected\n", m_id);
		return false;
	}

	XRRScreenResources *resources = XRRGetScreenResourcesCurrent(m_pdisplay, m_root);
	XRROutputInfo *output_info = XRRGetOutputInfo(m_pdisplay, resources, resources->outputs[m_desktop_output]);

	// Cycle through the modelines and report them back to the display manager
	if (m_video_modes_position < output_info->nmode)
	{
		for (int m = 0; m < resources->nmode; m++)
		{
			XRRModeInfo *pxmode = &resources->modes[m];

			if (pxmode->id == output_info->modes[m_video_modes_position])
			{
				mode->platform_data = pxmode->id;

				mode->pclock     = pxmode->dotClock;
				mode->hactive    = pxmode->width;
				mode->hbegin     = pxmode->hSyncStart;
				mode->hend       = pxmode->hSyncEnd;
				mode->htotal     = pxmode->hTotal;
				mode->vactive    = pxmode->height;
				mode->vbegin     = pxmode->vSyncStart;
				mode->vend       = pxmode->vSyncEnd;
				mode->vtotal     = pxmode->vTotal;
				mode->interlace  = (pxmode->modeFlags & RR_Interlace) ? 1 : 0;
				mode->doublescan = (pxmode->modeFlags & RR_DoubleScan) ? 1 : 0;
				mode->hsync      = (pxmode->modeFlags & RR_HSyncPositive) ? 1 : 0;
				mode->vsync      = (pxmode->modeFlags & RR_VSyncPositive) ? 1 : 0;

				mode->hfreq      = mode->pclock / mode->htotal;
				mode->vfreq      = mode->hfreq / mode->vtotal * (mode->interlace ? 2 : 1);
				mode->refresh    = mode->vfreq;

				mode->width      = pxmode->width;
				mode->height     = pxmode->height;

				// Add the rotation flag from the crtc
				mode->type |= m_crtc_flags;

				mode->type |= CUSTOM_VIDEO_TIMING_XRANDR;

				if (strncmp(pxmode->name, "SR-", 3) == 0)
					log_verbose("XRANDR: <%d> (get_timing) [WARNING] modeline %s detected\n", m_id, pxmode->name);

				// Add the desktop flag to desktop modeline
				if (m_desktop_mode.id == pxmode->id)
					mode->type |= MODE_DESKTOP;

				log_verbose("XRANDR: <%d> (get_timing) mode %04lx %dx%d refresh %.6f added\n", m_id, pxmode->id, pxmode->width, pxmode->height, mode->vfreq);
			}
		}
		m_video_modes_position++;
	}
	else
	{
		// Inititalise the position for the modeline list
		m_video_modes_position = 0;
	}

	XRRFreeOutputInfo(output_info);
	XRRFreeScreenResources(resources);

	return true;
}

//============================================================
//  xrandr_timing::process_modelist
//============================================================

bool xrandr_timing::process_modelist(std::vector<modeline *> modelist)
{
	bool error = false;
	bool result = false;

	for (auto &mode : modelist)
	{
		if (mode->type & MODE_DELETE)
			result = delete_mode(mode);

		else if (mode->type & MODE_ADD)
			result = add_mode(mode);

		if (!result)
		{
			mode->type |= MODE_ERROR;
			error = true;
		}
		else
			// succeed
			mode->type &= ~MODE_ERROR;
	}

	return !error;
}
