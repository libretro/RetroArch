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
#elif defined(__linux__)
	#define SR_CONFIG_PATHS ";./;./ini/;/etc/;"
#else
	#define SR_CONFIG_PATHS ";./"
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
	// Set Switchres default config options
	set_monitor("generic_15");
	set_modeline("auto");
	set_lcd_range("auto");
	for (int i = 0; i++ < MAX_RANGES;) set_crt_range(i, "auto");

	// Set display manager default options
	set_screen("auto");
	set_modeline_generation(true);
	set_lock_unsupported_modes(true);
	set_lock_system_modes(true);
	set_refresh_dont_care(false);

	// Set modeline generator default options
	set_interlace(true);
	set_doublescan(true);
	set_dotclock_min(0.0f);
	set_rotation(false);
	set_monitor_aspect(STANDARD_CRT_ASPECT);
	set_refresh_tolerance(2.0f);
	set_super_width(2560);
	set_v_shift_correct(0);
	set_pixel_precision(1);
	set_interlace_force_even(0);

	// Create our display manager
	m_display_factory = new display_manager();

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

display_manager* switchres_manager::add_display()
{
	// Parse display specific ini, if it exists
	display_settings base_ds = ds;
	char file_name[32] = {0};
	sprintf(file_name, "display%d.ini", (int)displays.size());
	parse_config(file_name);

	// Create new display
	display_manager *display = m_display_factory->make(&ds);
	display->set_index(displays.size());
	displays.push_back(display);

	log_verbose("Switchres(v%s) display[%d]: monitor[%s] generation[%s]\n",
		SWITCHRES_VERSION, display->index(), ds.monitor, ds.modeline_generation?"on":"off");

	display->parse_options();

	// restore base display settings
	ds = base_ds;

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
		{
			switch (s2i(key.c_str()))
			{
				// Switchres options
				case s2i("verbose"):
					if (atoi(value.c_str())) set_log_verbose_fn((void*)printf);
					break;
				case s2i("monitor"):
					transform(value.begin(), value.end(), value.begin(), ::tolower);
					set_monitor(value.c_str());
					break;
				case s2i("crt_range0"):
					set_crt_range(0, value.c_str());
					break;
				case s2i("crt_range1"):
					set_crt_range(1, value.c_str());
					break;
				case s2i("crt_range2"):
					set_crt_range(2, value.c_str());
					break;
				case s2i("crt_range3"):
					set_crt_range(3, value.c_str());
					break;
				case s2i("crt_range4"):
					set_crt_range(4, value.c_str());
					break;
				case s2i("crt_range5"):
					set_crt_range(5, value.c_str());
					break;
				case s2i("crt_range6"):
					set_crt_range(6, value.c_str());
					break;
				case s2i("crt_range7"):
					set_crt_range(7, value.c_str());
					break;
				case s2i("crt_range8"):
					set_crt_range(8, value.c_str());
					break;
				case s2i("crt_range9"):
					set_crt_range(9, value.c_str());
					break;
				case s2i("lcd_range"):
					set_lcd_range(value.c_str());
					break;
				case s2i("modeline"):
					set_modeline(value.c_str());
					break;
				case s2i("user_mode"):
				{
					modeline user_mode = {};
					if (strcmp(value.c_str(), "auto"))
					{
						if (sscanf(value.c_str(), "%dx%d@%d", &user_mode.width, &user_mode.height, &user_mode.refresh) < 1)
						{
							log_error("Error: use format resolution <w>x<h>@<r>\n");
							break;
						}
					}
					set_user_mode(&user_mode);
					break;
				}

				// Display options
				case s2i("display"):
					set_screen(value.c_str());
					break;
				case s2i("api"):
					set_api(value.c_str());
					break;
				case s2i("modeline_generation"):
					set_modeline_generation(atoi(value.c_str()));
					break;
				case s2i("lock_unsupported_modes"):
					set_lock_unsupported_modes(atoi(value.c_str()));
					break;
				case s2i("lock_system_modes"):
					set_lock_system_modes(atoi(value.c_str()));
					break;
				case s2i("refresh_dont_care"):
					set_refresh_dont_care(atoi(value.c_str()));
					break;
				case s2i("keep_changes"):
					set_keep_changes(atoi(value.c_str()));
					break;

				// Modeline generation options
				case s2i("interlace"):
					set_interlace(atoi(value.c_str()));
					break;
				case s2i("doublescan"):
					set_doublescan(atoi(value.c_str()));
					break;
				case s2i("dotclock_min"):
				{
					double pclock_min = 0.0f;
					sscanf(value.c_str(), "%lf", &pclock_min);
					set_dotclock_min(pclock_min);
					break;
				}
				case s2i("sync_refresh_tolerance"):
				{
					double refresh_tolerance = 0.0f;
					sscanf(value.c_str(), "%lf", &refresh_tolerance);
					set_refresh_tolerance(refresh_tolerance);
					break;
				}
				case s2i("super_width"):
				{
					int super_width = 0;
					sscanf(value.c_str(), "%d", &super_width);
					set_super_width(super_width);
					break;
				}
				case s2i("aspect"):
					set_monitor_aspect(get_aspect(value.c_str()));
					break;

				case s2i("v_shift_correct"):
					set_v_shift_correct(atoi(value.c_str()));
					break;

				case s2i("pixel_precision"):
					set_pixel_precision(atoi(value.c_str()));
					break;

				case s2i("interlace_force_even"):
					set_interlace_force_even(atoi(value.c_str()));
					break;

				// Custom video backend options
				case s2i("screen_compositing"):
					set_screen_compositing(atoi(value.c_str()));
					break;
				case s2i("screen_reordering"):
					set_screen_reordering(atoi(value.c_str()));
					break;
				case s2i("allow_hardware_refresh"):
					set_allow_hardware_refresh(atoi(value.c_str()));
					break;
				case s2i("custom_timing"):
					set_custom_timing(value.c_str());
					break;

				// Various
				case s2i("verbosity"):
				{
					int verbosity_level = 1;
					sscanf(value.c_str(), "%d", &verbosity_level);
					set_log_level(verbosity_level);
					break;
				}

				default:
					log_error("Invalid option %s\n", key.c_str());
					break;
			}
		}
	}
	config_file.close();
	return true;
}

//============================================================
//  switchres_manager::get_aspect
//============================================================

double switchres_manager::get_aspect(const char* aspect)
{
	int num, den;
	if (sscanf(aspect, "%d:%d", &num, &den) == 2)
	{
		if (den == 0)
		{
			log_error("Error: denominator can't be zero\n");
			return STANDARD_CRT_ASPECT;
		}
		return (double(num)/double(den));
	}

	log_error("Error: use format --aspect <num:den>\n");
	return STANDARD_CRT_ASPECT;
}
