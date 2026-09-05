/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2021 - Chris Kennedy, Antonio Giner,
 *                            Alexandre Wodarczyk, Gil Delescluse
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __VIDEO_MODELINE_CORE_H
#define __VIDEO_MODELINE_CORE_H

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Video modeline engine.
 *
 * Given a source (width, height, vertical refresh, rotation) and a
 * description of what the display can sync to, produce a complete
 * video timing: pixel clock, active/sync/total counts on both axes,
 * polarities and scan flags. The engine does not touch a display.
 * The display server owns application; the CRT consumer in
 * gfx/video_crt_switch.c is the first client. */

/* Modeline print flags */
#define MODELINE_PRINT_LABEL      0x00000001
#define MODELINE_PRINT_LABEL_SDL  0x00000002
#define MODELINE_PRINT_PARAMS     0x00000004
#define MODELINE_PRINT_FULL       (MODELINE_PRINT_LABEL | MODELINE_PRINT_PARAMS)

/* Scoring result bits (video_modeline_t.result.weight) */
#define MODELINE_R_V_FREQ_OFF     0x00000001
#define MODELINE_R_RES_STRETCH    0x00000002
#define MODELINE_R_OUT_OF_RANGE   0x00000004

/* Mode type bits (video_modeline_t.type) */
#define MODELINE_OK               0x00000000
#define MODELINE_DESKTOP          0x01000000
#define MODELINE_ROTATED          0x02000000
#define MODELINE_DISABLED         0x04000000
#define MODELINE_USER_DEF         0x08000000
#define MODELINE_UPDATE           0x10000000
#define MODELINE_ADD              0x20000000
#define MODELINE_DELETE           0x40000000
#define MODELINE_ERROR            0x80000000
#define MODELINE_V_FREQ_EDITABLE  0x00000001
#define MODELINE_X_RES_EDITABLE   0x00000002
#define MODELINE_Y_RES_EDITABLE   0x00000004
#define MODELINE_SCAN_EDITABLE    0x00000008
#define MODELINE_XYV_EDITABLE     (MODELINE_X_RES_EDITABLE | MODELINE_Y_RES_EDITABLE | MODELINE_V_FREQ_EDITABLE)

/* Timing provenance bits (video_modeline_t.type). A backend tags the
 * modes it enumerates; MODELINE_TIMING_SYSTEM marks a mode the OS
 * listed without detailed timings. */
#define MODELINE_TIMING_MASK      0x00000ff0
#define MODELINE_TIMING_AUTO      0x00000000
#define MODELINE_TIMING_SYSTEM    0x00000010
#define MODELINE_TIMING_XRANDR    0x00000020
#define MODELINE_TIMING_POWERSTRIP 0x00000040
#define MODELINE_TIMING_ATI_LEGACY 0x00000080
#define MODELINE_TIMING_ATI_ADL   0x00000100
#define MODELINE_TIMING_DRMKMS    0x00000200

/* Backend capability bits (video_modeline_ops_t.caps) */
#define MODELINE_CAPS_UPDATE           0x001
#define MODELINE_CAPS_ADD              0x002
#define MODELINE_CAPS_DESKTOP_EDITABLE 0x004
#define MODELINE_CAPS_SCAN_EDITABLE    0x008

/* Request flags for modeline_get() */
#define MODELINE_REQ_INTERLACED   (1 << 0)
#define MODELINE_REQ_ROTATED      (1 << 1)

#define MODELINE_DUMMY_WIDTH      1234
#define MODELINE_MAX_MODES        256
#define MODELINE_MAX_RANGES       10
#define MODELINE_STANDARD_CRT_ASPECT (4.0 / 3.0)

typedef struct video_modeline_result
{
   double x_scale;
   double y_scale;
   double v_scale;
   double x_diff;
   double y_diff;
   double v_diff;
   int    weight;
   int    scan_penalty;
} video_modeline_result_t;

/* The full timing. Every field has a consumer after generation:
 * pclock/porches/totals are the mode itself, hsync/vsync/interlace/
 * doublescan become backend flags, vfreq/hfreq feed the runloop and
 * range checks, width/height/refresh are the labels the OS lists the
 * mode under (refresh is vfreq truncated toward zero on purpose),
 * result carries the scale factors for aspect and the compromise
 * bits, id/type/platform_data/range tie the entry to the backend's
 * list and to the range it was generated from. */
typedef struct video_modeline
{
   uint64_t pclock;
   uint64_t platform_data;
   double   vfreq;
   double   hfreq;
   video_modeline_result_t result;
   int      hactive;
   int      hbegin;
   int      hend;
   int      htotal;
   int      vactive;
   int      vbegin;
   int      vend;
   int      vtotal;
   int      interlace;
   int      doublescan;
   int      hsync;
   int      vsync;
   int      width;
   int      height;
   int      refresh;
   int      refresh_label;
   int      id;
   int      type;
   int      range;
} video_modeline_t;

/* One band of what a monitor can sync: horizontal and vertical
 * frequency limits, porch times in microseconds (vertical ones in
 * milliseconds on the wire, stored as ms/1000 here), polarities and
 * active line limits for progressive and interlaced scan. */
typedef struct video_modeline_range
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
   double vertical_blank;
   int    hsync_polarity;
   int    vsync_polarity;
   int    progressive_lines_min;
   int    progressive_lines_max;
   int    interlaced_lines_min;
   int    interlaced_lines_max;
} video_modeline_range_t;

/* Display-facing settings a backend needs when it opens: which
 * screen, which API, and the PowerStrip timing string. */
typedef struct video_modeline_disp
{
   char screen[32];
   char api[32];
   char custom_timing[256];
   bool screen_compositing;
   bool screen_reordering;
   bool allow_hardware_refresh;
   bool lock_unsupported_modes;
   bool keep_changes;
} video_modeline_disp_t;

/* Generator state and policy. Policy inputs (super_width, dotclock
 * minimum, geometry sliders, monitor aspect, pixel precision) live
 * here rather than inside the mode. */
typedef struct video_modeline_gen
{
   uint64_t pclock_min;
   double   monitor_aspect;
   double   refresh_tolerance;
   double   h_size;
   video_modeline_t *modes;      /* MODELINE_MAX_MODES entries */
   video_modeline_t *backup;     /* mirror of modes as enumerated */
   video_modeline_t *selected;   /* points into modes, or NULL */
   video_modeline_t *current;    /* points into modes, or NULL */
   video_modeline_t user_mode;
   video_modeline_t desktop_mode;
   video_modeline_range_t range[MODELINE_MAX_RANGES];
   video_modeline_disp_t disp;
   int      interlace;
   int      doublescan;
   int      super_width;
   int      h_shift;
   int      v_shift;
   int      v_shift_correct;
   int      pixel_precision;
   int      interlace_force_even;
   int      scale_proportional;
   int      num_modes;
   int      num_backup;
   int      id_counter;
   int      index;
   unsigned caps;
   char     monitor[32];
   char     crt_range[MODELINE_MAX_RANGES][256];
   char     lcd_range[256];
   char     user_modeline[256];
   bool     modeline_generation;
   bool     lock_system_modes;
   bool     refresh_dont_care;
   bool     desktop_is_rotated;
   bool     switching_required;
   bool     has_ini;
} video_modeline_gen_t;

/* Generation and scoring */
int   modeline_create(video_modeline_t *s_mode, video_modeline_t *t_mode,
      video_modeline_range_t *range, video_modeline_gen_t *cs);
int   modeline_compare(video_modeline_t *t, video_modeline_t *best);
int   modeline_adjust(video_modeline_t *mode, double hfreq_max,
      video_modeline_gen_t *cs);
int   modeline_vesa_gtf(video_modeline_t *m);
int   modeline_parse(const char *user_modeline, video_modeline_t *mode);
int   modeline_to_monitor_range(video_modeline_range_t *range,
      video_modeline_t *mode);
int   modeline_is_different(const video_modeline_t *n,
      const video_modeline_t *p);
void  modeline_copy_timings(video_modeline_t *n, const video_modeline_t *p);

/* Text forms */
char *modeline_print(const video_modeline_t *mode, char *s, size_t len,
      int flags);
char *modeline_result(const video_modeline_t *mode, char *s, size_t len);

/* Rounding helpers shared with the monitor and list modules */
int   modeline_round_near(double number);
int   modeline_round_near_odd(double number);
int   modeline_round_near_even(double number);
int   modeline_normalize(int a, int b);
int   modeline_real_res(int x);

RETRO_END_DECLS

#endif
