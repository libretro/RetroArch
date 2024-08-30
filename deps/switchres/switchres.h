/**************************************************************

   switchres.h - SwichRes general header

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#ifndef __SWITCHRES_H__
#define __SWITCHRES_H__

#include <cstring>
#include <vector>
#include "monitor.h"
#include "modeline.h"
#include "display.h"
#include "edid.h"

//============================================================
//  CONSTANTS
//============================================================

#ifndef SWITCHRES_VERSION
#define SWITCHRES_VERSION "2.2.1"
#endif


class switchres_manager
{
public:

	switchres_manager();
	~switchres_manager();

	// getters
	display_manager *display() const { return m_current_display; }
	display_manager *display(int i) const { return i < (int)displays.size()? displays[i] : nullptr; }
	display_manager *display_factory() const { return m_display_factory; }
	static char* get_version() { return (char*) SWITCHRES_VERSION; };

	// setters (log manager)
	void set_log_level(int log_level);
	void set_log_verbose_fn(void *func_ptr);
	void set_log_info_fn(void *func_ptr);
	void set_log_error_fn(void *func_ptr);

	void set_current_display(int index);
	void set_option(const char* key, const char* value);

	// interface
	display_manager* add_display(bool parse_options = true);
	bool parse_config(const char *file_name);

	// display list
	std::vector<display_manager *> displays;

private:

	display_manager *m_display_factory = 0;
	display_manager *m_current_display = 0;
};


#endif
