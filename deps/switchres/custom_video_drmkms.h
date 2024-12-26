/**************************************************************

   custom_video_drmkms.h - Linux DRM/KMS video management layer

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#ifndef __CUSTOM_VIDEO_DRMKMS_
#define __CUSTOM_VIDEO_DRMKMS_

// DRM headers
#include <xf86drm.h>
#include <xf86drmMode.h>
#include "custom_video.h"

class drmkms_timing : public custom_video
{
	public:
		drmkms_timing(char *device_name, custom_video_settings *vs);
		~drmkms_timing();
		const char *api_name() { return "DRMKMS"; }
		int caps() { return m_caps; }
		bool init();

		bool add_mode(modeline *mode);
		bool delete_mode(modeline *mode);
		bool update_mode(modeline *mode);

		bool process_modelist(std::vector<modeline *>);

		bool get_timing(modeline *mode);
		bool set_timing(modeline *mode);

	private:
		/*
		 * Consider m_id as the "display number": 1 for the 1st, 2 for the second etc...
		 */
		int m_id = 0;

		int m_drm_fd = -1;
		drmModeCrtc *mp_crtc_desktop = NULL;
		int m_card_id = 0;
		bool m_kernel_user_modes = false;
		bool can_drop_master = true;
		int m_hook_fd = -1;
		int m_caps = 0;

		char m_device_name[32];
		char m_drm_name[32];
		unsigned int m_desktop_output = 0;
		int m_video_modes_position = 0;

		void *mp_drm_handle = NULL;
		unsigned int m_dumb_handle = 0;
		unsigned int m_framebuffer_id = 0;

		__typeof__(drmGetVersion) *p_drmGetVersion;
		__typeof__(drmFreeVersion) *p_drmFreeVersion;
		__typeof__(drmModeGetResources) *p_drmModeGetResources;
		__typeof__(drmModeGetConnector) *p_drmModeGetConnector;
		__typeof__(drmModeGetConnectorCurrent) *p_drmModeGetConnectorCurrent;
		__typeof__(drmModeFreeConnector) *p_drmModeFreeConnector;
		__typeof__(drmModeFreeResources) *p_drmModeFreeResources;
		__typeof__(drmModeGetEncoder) *p_drmModeGetEncoder;
		__typeof__(drmModeFreeEncoder) *p_drmModeFreeEncoder;
		__typeof__(drmModeGetCrtc) *p_drmModeGetCrtc;
		__typeof__(drmModeSetCrtc) *p_drmModeSetCrtc;
		__typeof__(drmModeFreeCrtc) *p_drmModeFreeCrtc;
		__typeof__(drmModeAttachMode) *p_drmModeAttachMode;
		__typeof__(drmModeDetachMode) *p_drmModeDetachMode;
		__typeof__(drmModeAddFB) *p_drmModeAddFB;
		__typeof__(drmModeRmFB) *p_drmModeRmFB;
		__typeof__(drmModeGetFB) *p_drmModeGetFB;
		__typeof__(drmModeFreeFB) *p_drmModeFreeFB;
		__typeof__(drmPrimeHandleToFD) *p_drmPrimeHandleToFD;
		__typeof__(drmModeGetPlaneResources) *p_drmModeGetPlaneResources;
		__typeof__(drmModeFreePlaneResources) *p_drmModeFreePlaneResources;
		__typeof__(drmIoctl) *p_drmIoctl;
		__typeof__(drmGetCap) *p_drmGetCap;
		__typeof__(drmGetDevices2) *p_drmGetDevices2;
		__typeof__(drmIsMaster) *p_drmIsMaster;
		__typeof__(drmSetMaster) *p_drmSetMaster;
		__typeof__(drmDropMaster) *p_drmDropMaster;

		bool test_kernel_user_modes();
		bool kms_has_mode(modeline*);
		void list_drm_modes();
		int get_master_fd();

};

#endif
