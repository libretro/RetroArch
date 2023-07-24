/**************************************************************

   switchres_main.cpp - Swichres standalone launcher

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#include <iostream>
#include <cstring>
#include <getopt.h>
#include "switchres.h"
#include "log.h"

using namespace std;

int show_version();
int show_usage();

enum
 {
	OPT_MODELINE = 128
 };

//============================================================
//  main
//============================================================

int main(int argc, char **argv)
{

	switchres_manager switchres;
	display_manager* df = switchres.display_factory();

	switchres.parse_config("switchres.ini");

	int width = 0;
	int height = 0;
	float refresh = 0.0;
	modeline user_mode = {};
	int index = 0;

	int version_flag = false;
	bool help_flag = false;
	bool resolution_flag = false;
	bool calculate_flag = false;
	bool edid_flag = false;
	bool switch_flag = false;
	bool launch_flag = false;
	bool force_flag = false;
	bool interlaced_flag = false;
	bool rotated_flag = false;
	bool user_ini_flag = false;
	bool keep_changes_flag = false;
	bool geometry_flag = false;
	int status_code = 0;

	string ini_file;
	string launch_command;

	while (1)
	{
		static struct option long_options[] =
		{
			{"version",     no_argument,       &version_flag, '1'},
			{"help",        no_argument,       0, 'h'},
			{"calc",        no_argument,       0, 'c'},
			{"switch",      no_argument,       0, 's'},
			{"launch",      required_argument, 0, 'l'},
			{"monitor",     required_argument, 0, 'm'},
			{"aspect",      required_argument, 0, 'a'},
			{"edid",        no_argument,       0, 'e'},
			{"rotated",     no_argument,       0, 'r'},
			{"display",     required_argument, 0, 'd'},
			{"force",       required_argument, 0, 'f'},
			{"ini",         required_argument, 0, 'i'},
			{"verbose",     no_argument,       0, 'v'},
			{"backend",     required_argument, 0, 'b'},
			{"keep",        no_argument,       0, 'k'},
			{"geometry",    required_argument, 0, 'g'},
			{"modeline",    required_argument, 0, OPT_MODELINE},
			{0, 0, 0, 0}
		};

		int option_index = 0;
		int c = getopt_long(argc, argv, "vhcsl:m:a:erd:f:i:b:kg:", long_options, &option_index);

		if (c == -1)
			break;

		if (version_flag)
		{
			show_version();
			return 0;
		}

		switch (c)
		{
			case OPT_MODELINE:
				df->set_modeline(optarg);
				break;

			case 'v':
				switchres.set_log_level(3);
				switchres.set_log_error_fn((void*)printf);
				switchres.set_log_info_fn((void*)printf);
				switchres.set_log_verbose_fn((void*)printf);
				break;

			case 'h':
				help_flag = true;
				break;

			case 'c':
				calculate_flag = true;
				break;

			case 's':
				switch_flag = true;
				break;

			case 'l':
				launch_flag = true;
				launch_command = optarg;
				break;

			case 'm':
				df->set_monitor(optarg);
				break;

			case 'r':
				rotated_flag = true;
				break;

			case 'd':
				// Add new display in multi-monitor case
				if (index > 0) switchres.add_display();
				index ++;
				df->set_screen(optarg);
				break;

			case 'a':
				df->set_monitor_aspect(optarg);
				break;

			case 'e':
				edid_flag = true;
				break;

			case 'f':
				force_flag = true;
				if (sscanf(optarg, "%dx%d@%d", &user_mode.width, &user_mode.height, &user_mode.refresh) < 1)
					log_error("Error: use format --force <w>x<h>@<r>\n");
				break;

			case 'i':
				user_ini_flag = true;
				ini_file = optarg;
				break;

			case 'b':
				df->set_api(optarg);
				break;

			case 'k':
				keep_changes_flag = true;
				df->set_keep_changes(true);
				break;

			case 'g':
				double h_size; int h_shift, v_shift;
				if (sscanf(optarg, "%lf:%d:%d", &h_size, &h_shift, &v_shift) < 3)
					log_error("Error: use format --geometry <h_size>:<h_shift>:<v_shift>\n");
				geometry_flag = true;
				df->set_h_size(h_size);
				df->set_h_shift(h_shift);
				df->set_v_shift(v_shift);
				break;

			default:
				return 0;
		}
	}

	if (help_flag)
		goto usage;

	// Get user video mode information from command line
	if ((argc - optind) < 3)
	{
		log_error("Error: missing argument\n");
		goto usage;
	}
	else if ((argc - optind) > 3)
	{
		log_error("Error: too many arguments\n");
		goto usage;
	}
	else
	{
		resolution_flag = true;
		width = atoi(argv[optind]);
		height = atoi(argv[optind + 1]);
		refresh = atof(argv[optind + 2]);

		if (width <= 0 || height <= 0 || refresh <= 0.0f)
		{
			log_error("Error: wrong video mode request: %sx%s@%s\n", argv[optind], argv[optind + 1], argv[optind + 2]);
			goto usage;
		}

		char scan_mode = argv[optind + 2][strlen(argv[optind + 2]) -1];
		if (scan_mode == 'i')
			interlaced_flag = true;
	}

	if (user_ini_flag)
		switchres.parse_config(ini_file.c_str());

	if (calculate_flag)
		switchres.display()->set_screen("dummy");

	switchres.add_display();

	if (force_flag)
		switchres.display()->set_user_mode(&user_mode);

	if (!calculate_flag && !edid_flag)
	{
		for (auto &display : switchres.displays)
			display->init();
	}

	if (resolution_flag)
	{
		for (auto &display : switchres.displays)
		{
			int flags = (interlaced_flag? SR_MODE_INTERLACED : 0) | (rotated_flag? SR_MODE_ROTATED : 0);
			modeline *mode = display->get_mode(width, height, refresh, flags);
			if (mode) display->flush_modes();

			if (mode && geometry_flag)
			{
				monitor_range range = {};
				modeline_to_monitor_range(&range, mode);
				log_info("Adjusted geometry (%.3f:%d:%d) H: %.3f, %.3f, %.3f V: %.3f, %.3f, %.3f\n",
						display->h_size(), display->h_shift(), display->v_shift(),
						range.hfront_porch, range.hsync_pulse, range.hback_porch,
						range.vfront_porch * 1000, range.vsync_pulse * 1000, range.vback_porch * 1000);
			}
		}

		if (edid_flag)
		{
			edid_block edid = {};
			modeline *mode = switchres.display()->selected_mode();
			if (mode)
			{
				monitor_range *range = &switchres.display()->range[mode->range];
				edid_from_modeline(mode, range, switchres.display()->monitor(), &edid);

				char file_name[strlen(switchres.display()->monitor()) + 4];
				sprintf(file_name, "%s.bin", switchres.display()->monitor());

				FILE *file = fopen(file_name, "wb");
				if (file)
				{
					fwrite(&edid, sizeof(edid), 1, file);
					fclose (file);
					log_info("EDID saved as %s\n", file_name);
				}
			}
		}

		if (switch_flag) for (auto &display : switchres.displays) display->set_mode(display->selected_mode());

		if (switch_flag && !launch_flag && !keep_changes_flag)
		{
			log_info("Press ENTER to exit...\n");
			cin.get();
		}

		if (launch_flag)
		{
			status_code = system(launch_command.c_str());
			#ifdef __linux__
			status_code = WEXITSTATUS(status_code);
			#endif
			log_info("Process exited with value %d\n", status_code);
		}
	}

	return (status_code);

usage:
	show_usage();
	return 0;
}

//============================================================
//  show_version
//============================================================

int show_version()
{
	char version[]
	{
		"Switchres " SWITCHRES_VERSION "\n"
		"Modeline generation engine for emulation\n"
		"Copyright (C) 2010-2021 - Chris Kennedy, Antonio Giner, Alexandre Wodarczyk, Gil Delescluse\n"
		"License GPL-2.0+\n"
		"This is free software: you are free to change and redistribute it.\n"
		"There is NO WARRANTY, to the extent permitted by law.\n"
	};

	log_info("%s", version);
	return 0;
}

//============================================================
//  show_usage
//============================================================

int show_usage()
{
	char usage[] =
	{
		"Usage: switchres <width> <height> <refresh> [options]\n"
		"Options:\n"
		"  -c, --calc                        Calculate video mode and exit\n"
		"  -s, --switch                      Switch to video mode\n"
		"  -l, --launch <command>            Launch <command>\n"
		"  -m, --monitor <preset>            Monitor preset (generic_15, arcade_15, pal, ntsc, etc.)\n"
		"  -a, --aspect <num:den>            Monitor aspect ratio\n"
		"  -r, --rotated                     Original mode's native orientation is rotated\n"
		"  -d, --display <OS_display_name>   Use target display (Windows: \\\\.\\DISPLAY1, ... Linux: VGA-0, ...)\n"
		"  -f, --force <w>x<h>@<r>           Force a specific video mode from display mode list\n"
		"  -i, --ini <file.ini>              Specify an ini file\n"
		"  -b, --backend <api_name>          Specify the api name\n"
		"  -e, --edid                        Create an EDID binary with calculated video modes\n"
		"  -k, --keep                        Keep changes on exit (warning: this disables cleanup)\n"
		"  -g, --geometry <h_size>:<h_shift>:<v_shift>  Adjust geometry of generated modeline\n"
		"  --modeline <\"pclk hdisp hsst hsend htot vdisp vsst vsend vtot flags\">  Force an XFree86 modeline\n"
	};

	log_info("%s", usage);
	return 0;
}
