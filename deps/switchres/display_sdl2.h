/**************************************************************

   display_linux.h - Display manager for Linux

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#include "display.h"
#include "SDL.h"
#include "SDL_syswm.h"

class sdl2_display : public display_manager
{
	public:
		sdl2_display(display_settings *ds);
		~sdl2_display();
		bool init(void* pf_data);
		bool set_mode(modeline *mode);

	private:
		SDL_Window* m_sdlwindow = NULL;

		bool get_desktop_mode();
		int get_available_video_modes();
};
