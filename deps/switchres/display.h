/**************************************************************

   display.h - Display manager

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include <vector>
#include "modeline.h"
#include "custom_video.h"

// Mode flags
#define SR_MODE_INTERLACED    1<<0
#define SR_MODE_ROTATED       1<<1

typedef struct display_settings
{
	char   screen[32];
	char   api[32];
	bool   modeline_generation;
	bool   lock_unsupported_modes;
	bool   lock_system_modes;
	bool   refresh_dont_care;
	bool   keep_changes;
	char   monitor[32];
	char   crt_range[MAX_RANGES][256];
	char   lcd_range[256];
	char   user_modeline[256];
	modeline user_mode;

	generator_settings gs;
	custom_video_settings vs;
} display_settings;


class display_manager
{
public:

	display_manager() {};
	virtual ~display_manager()
	{
		if (!m_ds.keep_changes) restore_modes();
		if (m_factory) delete m_factory;
	};

	display_manager *make(display_settings *ds);
	void parse_options();
	virtual bool init(void* = nullptr);
	virtual int caps();

	// getters
	int index() const { return m_index; }
	custom_video *factory() const { return m_factory; }
	custom_video *video() const { return m_video; }
	bool has_ini() const { return m_has_ini; }

	// getters (modes)
	modeline user_mode() const { return m_user_mode; }
	modeline *selected_mode() const { return m_selected_mode; }
	modeline *current_mode() const { return m_current_mode; }

	// getters (display manager)
	const char *monitor() { return (const char*) &m_ds.monitor; }
	const char *user_modeline() { return (const char*) &m_ds.user_modeline; }
	const char *crt_range(int i) { return (const char*) &m_ds.crt_range[i]; }
	const char *lcd_range() { return (const char*) &m_ds.lcd_range; }
	const char *screen() { return (const char*) &m_ds.screen; }
	const char *api() { return (const char*) &m_ds.api; }
	bool modeline_generation() { return m_ds.modeline_generation; }
	bool lock_unsupported_modes() { return m_ds.lock_unsupported_modes; }
	bool lock_system_modes() { return m_ds.lock_system_modes; }
	bool refresh_dont_care() { return m_ds.refresh_dont_care; }
	bool keep_changes() { return m_ds.keep_changes; }
	bool desktop_is_rotated() const { return m_desktop_is_rotated; }

	// getters (modeline generator)
	bool interlace() { return m_ds.gs.interlace; }
	bool doublescan() { return m_ds.gs.doublescan; }
	double dotclock_min() { return m_ds.gs.pclock_min; }
	double refresh_tolerance() { return m_ds.gs.refresh_tolerance; }
	int super_width() { return m_ds.gs.super_width; }
	double monitor_aspect() { return m_ds.gs.monitor_aspect; }
	double h_size() { return m_ds.gs.h_size; }
	int h_shift() { return m_ds.gs.h_shift; }
	int v_shift() { return m_ds.gs.v_shift; }
	int v_shift_correct() { return m_ds.gs.v_shift_correct; }
	int pixel_precision() { return m_ds.gs.pixel_precision; }
	int interlace_force_even() { return m_ds.gs.interlace_force_even; }
	int scale_proportional() { return m_ds.gs.scale_proportional; }

	// getters (modeline result)
	bool got_mode() { return (m_selected_mode != nullptr); }
	int width() { return m_selected_mode != nullptr? m_selected_mode->width : 0; }
	int height() { return m_selected_mode != nullptr? m_selected_mode->height : 0; }
	int refresh() { return m_selected_mode != nullptr? m_selected_mode->refresh : 0; }
	double v_freq() { return m_selected_mode != nullptr? m_selected_mode->vfreq : 0; }
	double h_freq() { return m_selected_mode != nullptr? m_selected_mode->hfreq : 0; }
	double x_scale() { return m_selected_mode != nullptr? m_selected_mode->result.x_scale : 0; }
	double y_scale() { return m_selected_mode != nullptr? m_selected_mode->result.y_scale : 0; }
	double v_scale() { return m_selected_mode != nullptr? m_selected_mode->result.v_scale : 0; }
	bool is_interlaced() { return m_selected_mode != nullptr? m_selected_mode->interlace : false; }
	bool is_doublescanned() { return m_selected_mode != nullptr? m_selected_mode->doublescan : false; }
	bool is_stretched() { return m_selected_mode != nullptr? m_selected_mode->result.weight & R_RES_STRETCH : false; }
	bool is_refresh_off() { return m_selected_mode != nullptr? m_selected_mode->result.weight & R_V_FREQ_OFF : false; }
	bool is_switching_required() { return m_switching_required; }
	bool is_mode_updated() { return m_selected_mode != nullptr? m_selected_mode->type & MODE_UPDATE : false; }
	bool is_mode_new() { return m_selected_mode != nullptr? m_selected_mode->type & MODE_ADD : false; }

	// getters (custom_video backend)
	bool screen_compositing() { return m_ds.vs.screen_compositing; }
	bool screen_reordering() { return m_ds.vs.screen_reordering; }
	bool allow_hardware_refresh() { return m_ds.vs.allow_hardware_refresh; }
	const char *custom_timing() { return (const char*) &m_ds.vs.custom_timing; }

	// setters
	void set_index(int index) { m_index = index; }
	void set_factory(custom_video *factory) { m_factory = factory; }
	void set_custom_video(custom_video *video) { m_video = video; }
	void set_has_ini(bool value) { m_has_ini = value; }

	// setters (modes)
	void set_user_mode(modeline *mode) { m_ds.user_mode = m_user_mode = *mode; filter_modes(); }
	void set_selected_mode(modeline *mode) { m_selected_mode = mode; }
	void set_current_mode(modeline *mode) { m_current_mode = mode; }

	// setters (display_manager)
	void set_monitor(const char *preset) { strncpy(m_ds.monitor, preset, sizeof(m_ds.monitor)-1); set_preset(preset); }
	void set_modeline(const char *modeline) { strncpy(m_ds.user_modeline, modeline, sizeof(m_ds.user_modeline)-1); }
	void set_crt_range(int i, const char *range) { strncpy(m_ds.crt_range[i], range, sizeof(m_ds.crt_range[i])-1); }
	void set_lcd_range(const char *range) { strncpy(m_ds.lcd_range, range, sizeof(m_ds.lcd_range)-1); }
	void set_screen(const char *screen) { strncpy(m_ds.screen, screen, sizeof(m_ds.screen)-1); }
	void set_api(const char *api) { strncpy(m_ds.api, api, sizeof(m_ds.api)-1); }
	void set_modeline_generation(bool value) { m_ds.modeline_generation = value; }
	void set_lock_unsupported_modes(bool value) { m_ds.lock_unsupported_modes = value; }
	void set_lock_system_modes(bool value) { m_ds.lock_system_modes = value; }
	void set_refresh_dont_care(bool value) { m_ds.refresh_dont_care = value; }
	void set_keep_changes(bool value) { m_ds.keep_changes = value; }
	void set_desktop_is_rotated(bool value) { m_desktop_is_rotated = value; }

	// setters (modeline generator)
	void set_interlace(bool value) { m_ds.gs.interlace = value; }
	void set_doublescan(bool value) { m_ds.gs.doublescan = value; }
	void set_dotclock_min(double value) { m_ds.gs.pclock_min = value * 1000000; }
	void set_refresh_tolerance(double value) { m_ds.gs.refresh_tolerance = value; }
	void set_super_width(int value) { m_ds.gs.super_width = value; }
	void set_monitor_aspect(double value) { m_ds.gs.monitor_aspect = value; }
	void set_monitor_aspect(const char* aspect) { set_monitor_aspect(get_aspect(aspect)); }
	void set_h_size(double value) { m_ds.gs.h_size = value; }
	void set_h_shift(int value) { m_ds.gs.h_shift = value; }
	void set_v_shift(int value) { m_ds.gs.v_shift = value; }
	void set_v_shift_correct(int value) { m_ds.gs.v_shift_correct = value; }
	void set_pixel_precision(int value) { m_ds.gs.pixel_precision = value; }
	void set_interlace_force_even(int value) { m_ds.gs.interlace_force_even = value; }
	void set_scale_proportional(int value) { m_ds.gs.scale_proportional = value; }

	// setters (custom_video backend)
	void set_screen_compositing(bool value) { m_ds.vs.screen_compositing = value; }
	void set_screen_reordering(bool value) { m_ds.vs.screen_reordering = value; }
	void set_allow_hardware_refresh(bool value) { m_ds.vs.allow_hardware_refresh = value; }
	void set_custom_timing(const char *custom_timing) { strncpy(m_ds.vs.custom_timing, custom_timing, sizeof(m_ds.vs.custom_timing)-1); }

	// options
	display_settings m_ds = {};

	// mode setting interface
	modeline *get_mode(int width, int height, float refresh, int flags);
	bool add_mode(modeline *mode);
	bool delete_mode(modeline *mode);
	bool update_mode(modeline *mode);
	virtual bool set_mode(modeline *);
	void log_mode(modeline *mode);

	// mode list handling
	bool filter_modes();
	bool restore_modes();
	bool flush_modes();
	bool auto_specs();

	// mode list
	std::vector<modeline> video_modes = {};
	std::vector<modeline> backup_modes = {};
	modeline desktop_mode = {};

	// monitor preset
	monitor_range range[MAX_RANGES];

private:

	// custom video backend
	custom_video *m_factory = 0;
	custom_video *m_video = 0;

	modeline m_user_mode = {};
	modeline *m_selected_mode = 0;
	modeline *m_current_mode = 0;

	int m_index = 0;
	bool m_desktop_is_rotated = 0;
	bool m_switching_required = 0;
	bool m_has_ini = 0;
	int m_id_counter = 0;

	void set_preset(const char *preset);
	double get_aspect(const char* aspect);

protected:
	void* m_pf_data = nullptr;
};

class dummy_display : public display_manager
{
	public:
		dummy_display(display_settings *ds) { m_ds = *ds; };
};

#endif
