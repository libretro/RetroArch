/**************************************************************

     custom_video_powerstrip.h - PowerStrip interface routines

     ---------------------------------------------------------

     Switchres   Modeline generation engine for emulation

     License     GPL-2.0+
     Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                           Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#include "custom_video.h"

//============================================================
//  TYPE DEFINITIONS
//============================================================

typedef struct
{
	int HorizontalActivePixels;
	int HorizontalFrontPorch;
	int HorizontalSyncWidth;
	int HorizontalBackPorch;
	int VerticalActivePixels;
	int VerticalFrontPorch;
	int VerticalSyncWidth;
	int VerticalBackPorch;
	int PixelClockInKiloHertz;
	union
	{
		int w;
		struct
		{
			unsigned :1;
			unsigned HorizontalPolarityNegative:1;
			unsigned VerticalPolarityNegative:1;
			unsigned :29;
		} b;
	} TimingFlags;
} MonitorTiming;


class pstrip_timing : public custom_video
{
	public:
		pstrip_timing(char *device_name, custom_video_settings *vs);
		~pstrip_timing();
		const char *api_name() { return "PowerStrip"; }
		bool init();
		int caps() { return CUSTOM_VIDEO_CAPS_UPDATE | CUSTOM_VIDEO_CAPS_SCAN_EDITABLE | CUSTOM_VIDEO_CAPS_DESKTOP_EDITABLE; }

		bool update_mode(modeline *mode);

		bool get_timing(modeline *mode);
		bool set_timing(modeline *m);

	private:

		int ps_reset();
		int ps_get_modeline(modeline *modeline);
		bool ps_set_modeline(modeline *modeline);
		int ps_get_monitor_timing(MonitorTiming *timing);
		int ps_set_monitor_timing(MonitorTiming *timing);
		int ps_set_monitor_timing_string(char *in);
		int ps_set_refresh(double vfreq);
		int ps_best_pclock(MonitorTiming *timing, int desired_pclock);
		int ps_create_resolution(modeline *modeline);
		bool ps_read_timing_string(char *in, MonitorTiming *timing);
		void ps_fill_timing_string(char *out, MonitorTiming *timing);
		bool ps_modeline_to_pstiming(modeline *modeline, MonitorTiming *timing);
		int ps_pstiming_to_modeline(MonitorTiming *timing, modeline *modeline);
		int ps_monitor_index (const char *display_name);

		char m_device_name[32];
		char m_ps_timing[256];
		int m_monitor_index = 0;
		modeline m_user_mode = {};
		MonitorTiming m_timing_backup = {};
		HWND hPSWnd = 0;
};
