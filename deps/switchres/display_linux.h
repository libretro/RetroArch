/**************************************************************

   display_linux.h - Display manager for Linux

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#include "display.h"

class linux_display : public display_manager
{
	public:
		linux_display(display_settings *ds);
		~linux_display();
		bool init();
		bool set_mode(modeline *mode);

	private:
		bool get_desktop_mode();
		bool set_desktop_mode(modeline *mode, int flags);
		bool restore_desktop_mode();
		int get_available_video_modes();
};
