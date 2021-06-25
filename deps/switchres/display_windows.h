/**************************************************************

   display_windows.h - Display manager for Windows

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#include <windows.h>
#include "display.h"

//============================================================
//  PARAMETERS
//============================================================

// display modes
#define DM_INTERLACED 0x00000002
#define DISPLAY_MAX 16


class windows_display : public display_manager
{
	public:
		windows_display(display_settings *ds);
		~windows_display();
		bool init();
		bool set_mode(modeline *mode);

	private:
		bool get_desktop_mode();
		bool set_desktop_mode(modeline *mode, int flags);
		bool restore_desktop_mode();
		int get_available_video_modes();

		char m_device_name[32];
		char m_device_id[128];
		char m_device_key[128];
		DEVMODEA m_devmode;
};
