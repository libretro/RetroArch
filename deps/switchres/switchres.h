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

#define SWITCHRES_VERSION "2.002"

//============================================================
//  TYPE DEFINITIONS
//============================================================

typedef struct config_settings
{
	bool mode_switching;
} config_settings;


class switchres_manager
{
public:

	switchres_manager();
	~switchres_manager();

	// getters
	display_manager *display() const { return displays[0]; }
	display_manager *display(int i) const { return i < (int)displays.size()? displays[i] : nullptr; }

	// setters (log manager)
	void set_log_level(int log_level);
	void set_log_verbose_fn(void *func_ptr);
	void set_log_info_fn(void *func_ptr);
	void set_log_error_fn(void *func_ptr);

	// setters (display manager)
	void set_monitor(const char *preset) { strncpy(ds.monitor, preset, sizeof(ds.monitor)-1); }
	void set_modeline(const char *modeline) { strncpy(ds.user_modeline, modeline, sizeof(ds.user_modeline)-1); }
	void set_user_mode(modeline *user_mode) { ds.user_mode = *user_mode;}
	void set_crt_range(int i, const char *range) { strncpy(ds.crt_range[i], range, sizeof(ds.crt_range[i])-1); }
	void set_lcd_range(const char *range) { strncpy(ds.lcd_range, range, sizeof(ds.lcd_range)-1); }
	void set_screen(const char *screen) { strncpy(ds.screen, screen, sizeof(ds.screen)-1); }
	void set_api(const char *api) { strncpy(ds.api, api, sizeof(ds.api)-1); }
	void set_modeline_generation(bool value) { ds.modeline_generation = value; }
	void set_lock_unsupported_modes(bool value) { ds.lock_unsupported_modes = value; }
	void set_lock_system_modes(bool value) { ds.lock_system_modes = value; }
	void set_refresh_dont_care(bool value) { ds.refresh_dont_care = value; }
	void set_keep_changes(bool value) { ds.keep_changes = value; }

	// setters (modeline generator)
	void set_interlace(bool value) { ds.gs.interlace = value; }
	void set_doublescan(bool value) { ds.gs.doublescan = value; }
	void set_dotclock_min(double value) { ds.gs.pclock_min = value * 1000000; }
	void set_refresh_tolerance(double value) { ds.gs.refresh_tolerance = value; }
	void set_super_width(int value) { ds.gs.super_width = value; }
	void set_rotation(bool value) { ds.gs.rotation = value; }
	void set_monitor_aspect(double value) { ds.gs.monitor_aspect = value; }
	void set_monitor_aspect(const char* aspect) { set_monitor_aspect(get_aspect(aspect)); }
	void set_v_shift_correct(int value) { ds.gs.v_shift_correct = value; }
	void set_pixel_precision(int value) { ds.gs.pixel_precision = value; }
	void set_interlace_force_even(int value) { ds.gs.interlace_force_even = value; }

	// setters (custom_video backend)
	void set_screen_compositing(bool value) { ds.vs.screen_compositing = value; }
	void set_screen_reordering(bool value) { ds.vs.screen_reordering = value; }
	void set_allow_hardware_refresh(bool value) { ds.vs.allow_hardware_refresh = value; }
	void set_custom_timing(const char *custom_timing) { strncpy(ds.vs.custom_timing, custom_timing, sizeof(ds.vs.custom_timing)-1); }

	// interface
	display_manager* add_display();
	bool parse_config(const char *file_name);

	//settings
	config_settings cs = {};
	display_settings ds = {};

	// display list
	std::vector<display_manager *> displays;

private:

	display_manager *m_display_factory = 0;

	double get_aspect(const char* aspect);
};


#endif
