/**************************************************************

   modeline.h - Modeline generation header

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#ifndef __MODELINE_H__
#define __MODELINE_H__

#include <stdint.h>
#include <math.h>
#include <cstddef>
#include "monitor.h"


//============================================================
//  CONSTANTS
//============================================================

// Modeline print flags
#define MS_LABEL      0x00000001
#define MS_LABEL_SDL  0x00000002
#define MS_PARAMS     0x00000004
#define MS_FULL       MS_LABEL | MS_PARAMS

// Modeline result
#define R_V_FREQ_OFF    0x00000001
#define R_RES_STRETCH   0x00000002
#define R_OUT_OF_RANGE  0x00000004

// Mode types
#define MODE_OK         0x00000000
#define MODE_DESKTOP    0x01000000
#define MODE_ROTATED    0x02000000
#define MODE_DISABLED   0x04000000
#define MODE_USER_DEF   0x08000000
#define MODE_UPDATE     0x10000000
#define MODE_ADD        0x20000000
#define MODE_DELETE     0x40000000
#define MODE_ERROR      0x80000000
#define V_FREQ_EDITABLE 0x00000001
#define X_RES_EDITABLE  0x00000002
#define Y_RES_EDITABLE  0x00000004
#define SCAN_EDITABLE   0x00000008
#define XYV_EDITABLE   (X_RES_EDITABLE | Y_RES_EDITABLE | V_FREQ_EDITABLE )

#define DUMMY_WIDTH 1234
#define MAX_MODELINES 256

//============================================================
//  TYPE DEFINITIONS
//============================================================

typedef struct mode_result
{
	int     weight;
	int     scan_penalty;
	int     x_scale;
	int     y_scale;
	int     v_scale;
	double  x_diff;
	double  y_diff;
	double  v_diff;
	double  x_ratio;
	double  y_ratio;
	double  v_ratio;
} mode_result;

typedef struct modeline
{
	uint64_t    pclock;
	int    hactive;
	int    hbegin;
	int    hend;
	int    htotal;
	int    vactive;
	int    vbegin;
	int    vend;
	int    vtotal;
	int    interlace;
	int    doublescan;
	int    hsync;
	int    vsync;
	//
	double vfreq;
	double hfreq;
	//
	int    width;
	int    height;
	int    refresh;
	int    refresh_label;
	//
	int    type;
	int    range;
	uint64_t platform_data;
	//
	mode_result result;
} modeline;

typedef struct generator_settings
{
	int      interlace;
	int      doublescan;
	uint64_t pclock_min;
	bool     rotation;
	double   monitor_aspect;
	double   refresh_tolerance;
	int      super_width;
	int      v_shift_correct;
	int      pixel_precision;
	int      interlace_force_even;
} generator_settings;

//============================================================
//  PROTOTYPES
//============================================================

int modeline_create(modeline *s_mode, modeline *t_mode, monitor_range *range, generator_settings *cs);
int modeline_compare(modeline *t_mode, modeline *best_mode);
char * modeline_print(modeline *mode, char *modeline, int flags);
char * modeline_result(modeline *mode, char *result);
int modeline_vesa_gtf(modeline *m);
int modeline_parse(const char *user_modeline, modeline *mode);
int modeline_to_monitor_range(monitor_range *range, modeline *mode);
int modeline_is_different(modeline *n, modeline *p);

int round_near(double number);
int normalize(int a, int b);
int real_res(int x);


#endif
