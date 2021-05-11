/**************************************************************

   custom_video_xrandr.h - Linux XRANDR video management layer

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#ifndef __CUSTOM_VIDEO_XRANDR__
#define __CUSTOM_VIDEO_XRANDR__

// X11 Xrandr headers
#include <X11/extensions/Xrandr.h>
#include "custom_video.h"

// Set timing option flags
#define XRANDR_DISABLE_CRTC_RELOCATION  0x00000001
#define XRANDR_ENABLE_SCREEN_REORDERING 0x00000002

// Set timing internal flags
#define XRANDR_SETMODE_IS_DESKTOP          0x00000001
#define XRANDR_SETMODE_RESTORE_DESKTOP     0x00000002
#define XRANDR_SETMODE_UPDATE_DESKTOP_CRTC 0x00000010
#define XRANDR_SETMODE_UPDATE_OTHER_CRTC   0x00000020
#define XRANDR_SETMODE_UPDATE_REORDERING   0x00000040

#define XRANDR_SETMODE_INFO_MASK           0x0000000F
#define XRANDR_SETMODE_UPDATE_MASK     0x000000F0

// Super resolution placement, vertical stacking, reserved XRANDR_REORDERING_MAXIMUM_HEIGHT pixels
//TODO confirm 1024 height is sufficient
#define XRANDR_REORDERING_MAXIMUM_HEIGHT 1024

class xrandr_timing : public custom_video
{
	public:
		xrandr_timing(char *device_name, custom_video_settings *vs);
		~xrandr_timing();
		const char *api_name() { return "XRANDR"; }
		int caps() { return CUSTOM_VIDEO_CAPS_ADD; }
		bool init();

		bool add_mode(modeline *mode);
		bool delete_mode(modeline *mode);
		bool update_mode(modeline *mode);

		bool get_timing(modeline *mode);
		bool set_timing(modeline *mode);

		bool process_modelist(std::vector<modeline *>);

		static int ms_xerrors;
		static int ms_xerrors_flag;

	private:
		int m_id = 0;
		int m_managed = 0;
		int m_enable_screen_reordering = 0;
		int m_enable_screen_compositing = 0;

		XRRModeInfo *find_mode(modeline *mode);
		XRRModeInfo *find_mode_by_name(char *name);

		bool set_timing(modeline *mode, int flags);

		int m_video_modes_position = 0;
		char m_device_name[32];
		Rotation m_desktop_rotation;
		unsigned int m_min_width;
		unsigned int m_max_width;
		unsigned int m_min_height;
		unsigned int m_max_height;

		Display *m_pdisplay = NULL;
		Window m_root;
		int m_screen;

		int m_desktop_output = -1;
		XRRModeInfo m_desktop_mode = {};
		int m_crtc_flags = 0;

		XRRCrtcInfo m_last_crtc = {};

		void *m_xrandr_handle = 0;

		__typeof__(XRRAddOutputMode) *p_XRRAddOutputMode;
		__typeof__(XRRConfigCurrentConfiguration) *p_XRRConfigCurrentConfiguration;
		__typeof__(XRRCreateMode) *p_XRRCreateMode;
		__typeof__(XRRDeleteOutputMode) *p_XRRDeleteOutputMode;
		__typeof__(XRRDestroyMode) *p_XRRDestroyMode;
		__typeof__(XRRFreeCrtcInfo) *p_XRRFreeCrtcInfo;
		__typeof__(XRRFreeOutputInfo) *p_XRRFreeOutputInfo;
		__typeof__(XRRFreeScreenConfigInfo) *p_XRRFreeScreenConfigInfo;
		__typeof__(XRRFreeScreenResources) *p_XRRFreeScreenResources;
		__typeof__(XRRGetCrtcInfo) *p_XRRGetCrtcInfo;
		__typeof__(XRRGetOutputInfo) *p_XRRGetOutputInfo;
		__typeof__(XRRGetScreenInfo) *p_XRRGetScreenInfo;
		__typeof__(XRRGetScreenResourcesCurrent) *p_XRRGetScreenResourcesCurrent;
		__typeof__(XRRQueryVersion) *p_XRRQueryVersion;
		__typeof__(XRRSetCrtcConfig) *p_XRRSetCrtcConfig;
		__typeof__(XRRSetScreenSize) *p_XRRSetScreenSize;
		__typeof__(XRRGetScreenSizeRange) *p_XRRGetScreenSizeRange;

		void *m_x11_handle = 0;

		__typeof__(XCloseDisplay) *p_XCloseDisplay;
		__typeof__(XGrabServer) *p_XGrabServer;
		__typeof__(XOpenDisplay) *p_XOpenDisplay;
		__typeof__(XSync) *p_XSync;
		__typeof__(XUngrabServer) *p_XUngrabServer;
		__typeof__(XSetErrorHandler) *p_XSetErrorHandler;
		__typeof__(XClearWindow) *p_XClearWindow;
		__typeof__(XFillRectangle) *p_XFillRectangle;
		__typeof__(XCreateGC) *p_XCreateGC;
};

#endif
