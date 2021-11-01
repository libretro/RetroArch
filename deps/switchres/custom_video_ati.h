/**************************************************************

    custom_video_ati.h - ATI legacy library header

    ---------------------------------------------------------

    Switchres   Modeline generation engine for emulation

    License     GPL-2.0+
    Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                          Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#include <windows.h>
#include "custom_video.h"

#define CRTC_DOUBLE_SCAN                    0x0001
#define CRTC_INTERLACED                     0x0002
#define CRTC_H_SYNC_POLARITY                0x0004
#define CRTC_V_SYNC_POLARITY                0x0008

class ati_timing : public custom_video
{
	public:
		ati_timing(char *device_name, custom_video_settings *vs);
		~ati_timing() {};
		const char *api_name() { return "ATI Legacy"; }
		bool init();
		int caps() { return CUSTOM_VIDEO_CAPS_UPDATE | CUSTOM_VIDEO_CAPS_SCAN_EDITABLE; }

		bool update_mode(modeline *mode);

		bool get_timing(modeline *mode);
		bool set_timing(modeline *mode);

		bool process_modelist(std::vector<modeline *>);

	private:
		void refresh_timings(void);

		int get_DWORD(int i, char *lp_data);
		int get_DWORD_BCD(int i, char *lp_data);
		void set_DWORD(char *data_string, UINT32 data_word, int offset);
		void set_DWORD_BCD(char *data_string, UINT32 data_word, int offset);
		int os_version(void);
		bool is_elevated();
		int win_interlace_factor(modeline *mode);

		char m_device_name[32];
		char m_device_key[256];
		int win_version;
};
