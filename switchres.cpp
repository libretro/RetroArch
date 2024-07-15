/**************************************************************

   switchres.cpp - Swichres manager

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#include <fstream>
#include <string.h>
#include <algorithm>
#include "switchres.h"
#include "log.h"

using namespace std;
const string WHITESPACE = " \n\r\t\f\v";

#if defined(_WIN32)
	#define SR_CONFIG_PATHS ";.\\;.\\ini\\;"
#else
	#define SR_CONFIG_PATHS ";./;./ini/;/etc/;"
#endif

//============================================================
//  logging
//============================================================

void switchres_manager::set_log_level(int log_level) { set_log_verbosity(log_level); }
void switchres_manager::set_log_verbose_fn(void *func_ptr) { set_log_verbose((void *)func_ptr); }
void switchres_manager::set_log_info_fn(void *func_ptr) { set_log_info((void *)func_ptr); }
void switchres_manager::set_log_error_fn(void *func_ptr) { set_log_error((void *)func_ptr); }

//============================================================
//  File parsing helpers
//============================================================

string ltrim(const string& s)
{
	size_t start = s.find_first_not_of(WHITESPACE);
	return (start == string::npos) ? "" : s.substr(start);
}

string rtrim(const string& s)
{
	size_t end = s.find_last_not_of(WHITESPACE);
	return (end == string::npos) ? "" : s.substr(0, end + 1);
}

string trim(const string& s)
{
	return rtrim(ltrim(s));
}

bool get_value(const string& line, string& key, string& value)
{
	size_t key_end = line.find_first_of(WHITESPACE);

	key = line.substr(0, key_end);
	value = ltrim(line.substr(key_end + 1));

	if (key.length() > 0 && value.length() > 0)
		return true;

	return false;
}

constexpr unsigned int s2i(const char* str, int h = 0)
{
	return !str[h] ? 5381 : (s2i(str, h+1)*33) ^ str[h];
}

//============================================================
//  switchres_manager::switchres_manager
//============================================================

switchres_manager::switchres_manager()
{
	// Create our display manager
	m_display_factory = new display_manager();
	m_current_display = m_display_factory;

	// Set display manager default options
	display()->set_monitor("generic_15");
	display()->set_modeline("auto");
	display()->set_lcd_range("auto");
	for (int i = 0; i < MAX_RANGES; i++) display()->set_crt_range(i, "auto");
	display()->set_screen("auto");
	display()->set_modeline_generation(true);
	display()->set_lock_unsupported_modes(true);
	display()->set_lock_system_modes(true);
	display()->set_refresh_dont_care(false);

	// Set modeline generator default options
	display()->set_interlace(true);
	display()->set_doublescan(true);
	display()->set_dotclock_min(0.0f);
	display()->set_monitor_aspect(STANDARD_CRT_ASPECT);
	display()->set_refresh_tolerance(2.0f);
	display()->set_super_width(2560);
	display()->set_h_shift(0);
	display()->set_v_shift(0);
	display()->set_h_size(1.0f);
	display()->set_v_shift_correct(0);
	display()->set_pixel_precision(1);
	display()->set_interlace_force_even(0);
	display()->set_scale_proportional(1);

	// Set logger properties
	set_log_info_fn((void*)printf);
	set_log_error_fn((void*)printf);
	set_log_verbose_fn((void*)printf);
	set_log_level(2);
}

//============================================================
//  switchres_manager::~switchres_manager
//============================================================

switchres_manager::~switchres_manager()
{
	if (m_display_factory) delete m_display_factory;

	for (auto &display : displays)
		delete display;
};

//============================================================
//  switchres_manager::add_display
//============================================================

display_manager* switchres_manager::add_display(bool parse_options)
{
	// Parse display specific ini, if it exists
	char file_name[32] = {0};
	sprintf(file_name, "display%d.ini", (int)displays.size());
	bool has_ini = parse_config(file_name);

	// Create new display
	display_manager *display = m_display_factory->make(&m_display_factory->m_ds);
	if (display == nullptr)
	{
		log_error("Switchres: error adding display\n");
		return nullptr;
	}

	m_current_display = display;
	display->set_index(displays.size());
	display->set_has_ini(has_ini);
	displays.push_back(display);

	log_verbose("Switchres(v%s) add display[%d]\n", SWITCHRES_VERSION, display->index());

	if (parse_options)
		display->parse_options();

	return display;
}

//============================================================
//  switchres_manager::parse_config
//============================================================

bool switchres_manager::parse_config(const char *file_name)
{
	ifstream config_file;

	// Search for ini file in our config paths
	auto start = 0U;
	while (true)
	{
		char full_path[256] = "";
		string paths = SR_CONFIG_PATHS;

		auto end = paths.find(";", start);
		if (end == string::npos) return false;

		snprintf(full_path, sizeof(full_path), "%s%s", paths.substr(start, end - start).c_str(), file_name);
		config_file.open(full_path);

		if (config_file.is_open())
		{
			log_verbose("parsing %s\n", full_path);
			break;
		}
		start = end + 1;
	}

	// Ini file found, parse it
	string line;
	while (getline(config_file, line))
	{
		line = trim(line);
		if (line.length() == 0 || line.at(0) == '#')
			continue;

		string key, value;
		if(get_value(line, key, value))
			set_option(key.c_str(), value.c_str());
	}
	config_file.close();
	return true;
}

//============================================================
//  switchres_manager::set_option
//============================================================

void switchres_manager::set_option(const char* key, const char* value)
{
	switch (s2i(key))
	{
		// Switchres options
		case s2i("verbose"):
			if (atoi(value)) set_log_verbose_fn((void*)printf);
			break;
		case s2i("monitor"):
			display()->set_monitor(value);
			break;
		case s2i("crt_range0"):
			display()->set_crt_range(0, value);
			break;
		case s2i("crt_range1"):
			display()->set_crt_range(1, value);
			break;
		case s2i("crt_range2"):
			display()->set_crt_range(2, value);
			break;
		case s2i("crt_range3"):
			display()->set_crt_range(3, value);
			break;
		case s2i("crt_range4"):
			display()->set_crt_range(4, value);
			break;
		case s2i("crt_range5"):
			display()->set_crt_range(5, value);
			break;
		case s2i("crt_range6"):
			display()->set_crt_range(6, value);
			break;
		case s2i("crt_range7"):
			display()->set_crt_range(7, value);
			break;
		case s2i("crt_range8"):
			display()->set_crt_range(8, value);
			break;
		case s2i("crt_range9"):
			display()->set_crt_range(9, value);
			break;
		case s2i("lcd_range"):
			display()->set_lcd_range(value);
			break;
		case s2i("modeline"):
			display()->set_modeline(value);
			break;
		case s2i("user_mode"):
		{
			modeline user_mode = {};
			if (strcmp(value, "auto"))
			{
				if (sscanf(value, "%dx%d@%d", &user_mode.width, &user_mode.height, &user_mode.refresh) < 1)
				{
					log_error("Error: use format resolution <w>x<h>@<r>\n");
					break;
				}
			}
			display()->set_user_mode(&user_mode);
			break;
		}

		// Display options
		case s2i("display"):
			display()->set_screen(value);
			break;
		case s2i("api"):
			display()->set_api(value);
			break;
		case s2i("modeline_generation"):
			display()->set_modeline_generation(atoi(value));
			break;
		case s2i("lock_unsupported_modes"):
			display()->set_lock_unsupported_modes(atoi(value));
			break;
		case s2i("lock_system_modes"):
			display()->set_lock_system_modes(atoi(value));
			break;
		case s2i("refresh_dont_care"):
			display()->set_refresh_dont_care(atoi(value));
			break;
		case s2i("keep_changes"):
			display()->set_keep_changes(atoi(value));
			break;

		// Modeline generation options
		case s2i("interlace"):
			display()->set_interlace(atoi(value));
			break;
		case s2i("doublescan"):
			display()->set_doublescan(atoi(value));
			break;
		case s2i("dotclock_min"):
		{
			double pclock_min = 0.0f;
			sscanf(value, "%lf", &pclock_min);
			display()->set_dotclock_min(pclock_min);
			break;
		}
		case s2i("sync_refresh_tolerance"):
		{
			double refresh_tolerance = 0.0f;
			sscanf(value, "%lf", &refresh_tolerance);
			display()->set_refresh_tolerance(refresh_tolerance);
			break;
		}
		case s2i("super_width"):
		{
			int super_width = 0;
			sscanf(value, "%d", &super_width);
			display()->set_super_width(super_width);
			break;
		}
		case s2i("aspect"):
			display()->set_monitor_aspect(value);
			break;
		case s2i("h_size"):
		{
			double h_size = 1.0f;
			sscanf(value, "%lf", &h_size);
			display()->set_h_size(h_size);
			break;
		}
		case s2i("h_shift"):
		{
			int h_shift = 0;
			sscanf(value, "%d", &h_shift);
			display()->set_h_shift(h_shift);
			break;
		}
		case s2i("v_shift"):
		{
			int v_shift = 0;
			sscanf(value, "%d", &v_shift);
			display()->set_v_shift(v_shift);
			break;
		}
		case s2i("v_shift_correct"):
			display()->set_v_shift_correct(atoi(value));
			break;

		case s2i("pixel_precision"):
			display()->set_pixel_precision(atoi(value));
			break;

		case s2i("interlace_force_even"):
			display()->set_interlace_force_even(atoi(value));
			break;

		case s2i("scale_proportional"):
			display()->set_scale_proportional(atoi(value));
			break;

		// Custom video backend options
		case s2i("screen_compositing"):
			display()->set_screen_compositing(atoi(value));
			break;
		case s2i("screen_reordering"):
			display()->set_screen_reordering(atoi(value));
			break;
		case s2i("allow_hardware_refresh"):
			display()->set_allow_hardware_refresh(atoi(value));
			break;
		case s2i("custom_timing"):
			display()->set_custom_timing(value);
			break;

		// Various
		case s2i("verbosity"):
		{
			int verbosity_level = 1;
			sscanf(value, "%d", &verbosity_level);
			set_log_level(verbosity_level);
			break;
		}

		default:
			log_error("Invalid option %s\n", key);
			break;
	}
}

//============================================================
//  switchres_manager::set_current_display
//============================================================

void switchres_manager::set_current_display(int index)
{
	int disp_index;

	if (index == -1)
	{
		m_current_display = m_display_factory;
		return;
	}
	else if (index < 0 || index >= (int)displays.size())
		disp_index = 0;

	else
		disp_index = index;

	m_current_display = displays[disp_index];
}
