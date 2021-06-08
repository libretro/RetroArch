/**************************************************************

   monitor.h - Monitor presets header

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#ifndef __MONITOR_H__
#define __MONITOR_H__

//============================================================
//  CONSTANTS
//============================================================

#define MAX_RANGES 10
#define MONITOR_CRT 0
#define MONITOR_LCD 1
#define STANDARD_CRT_ASPECT 4.0/3.0

//============================================================
//  TYPE DEFINITIONS
//============================================================

typedef struct monitor_range
{
	double hfreq_min;
	double hfreq_max;
	double vfreq_min;
	double vfreq_max;
	double hfront_porch;
	double hsync_pulse;
	double hback_porch;
	double vfront_porch;
	double vsync_pulse;
	double vback_porch;
	int    hsync_polarity;
	int    vsync_polarity;
	int    progressive_lines_min;
	int    progressive_lines_max;
	int    interlaced_lines_min;
	int    interlaced_lines_max;
	double vertical_blank;
} monitor_range;

//============================================================
//  PROTOTYPES
//============================================================

int monitor_fill_range(monitor_range *range, const char *specs_line);
int monitor_show_range(monitor_range *range);
int monitor_set_preset(char *type, monitor_range *range);
int monitor_fill_lcd_range(monitor_range *range, const char *specs_line);
int monitor_fill_vesa_gtf(monitor_range *range, const char *max_lines);
int monitor_fill_vesa_range(monitor_range *range, int lines_min, int lines_max);
int monitor_evaluate_range(monitor_range *range);

#endif
