/**************************************************************

   custom_video_drmkms.cpp - Linux DRM/KMS video management layer

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#include <stdio.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "custom_video_drmkms.h"
#include "log.h"

#define drmGetVersion p_drmGetVersion
#define drmFreeVersion p_drmFreeVersion
#define drmModeGetResources p_drmModeGetResources
#define drmModeGetConnector p_drmModeGetConnector
#define drmModeFreeConnector p_drmModeFreeConnector
#define drmModeFreeResources p_drmModeFreeResources
#define drmModeGetEncoder p_drmModeGetEncoder
#define drmModeFreeEncoder p_drmModeFreeEncoder
#define drmModeGetCrtc p_drmModeGetCrtc
#define drmModeSetCrtc p_drmModeSetCrtc
#define drmModeFreeCrtc p_drmModeFreeCrtc
#define drmModeAttachMode p_drmModeAttachMode
#define drmModeAddFB p_drmModeAddFB
#define drmModeRmFB p_drmModeRmFB
#define drmModeGetFB p_drmModeGetFB
#define drmModeFreeFB p_drmModeFreeFB
#define drmPrimeHandleToFD p_drmPrimeHandleToFD
#define drmModeGetPlaneResources p_drmModeGetPlaneResources
#define drmModeFreePlaneResources p_drmModeFreePlaneResources
#define drmIoctl p_drmIoctl
#define drmGetCap p_drmGetCap
#define drmIsMaster p_drmIsMaster
#define drmSetMaster p_drmSetMaster
#define drmDropMaster p_drmDropMaster

//============================================================
//  shared the privileges of the master fd
//============================================================

static int s_shared_fd[10] = {};
static int s_shared_count[10] = {};

//============================================================
//  list connector types
//============================================================

const char *get_connector_name(int mode)
{
	switch (mode)
	{
		case DRM_MODE_CONNECTOR_Unknown:
			return "Unknown";
		case DRM_MODE_CONNECTOR_VGA:
			return "VGA-";
		case DRM_MODE_CONNECTOR_DVII:
			return "DVI-I-";
		case DRM_MODE_CONNECTOR_DVID:
			return "DVI-D-";
		case DRM_MODE_CONNECTOR_DVIA:
			return "DVI-A-";
		case DRM_MODE_CONNECTOR_Composite:
			return "Composite-";
		case DRM_MODE_CONNECTOR_SVIDEO:
			return "SVIDEO-";
		case DRM_MODE_CONNECTOR_LVDS:
			return "LVDS-";
		case DRM_MODE_CONNECTOR_Component:
			return "Component-";
		case DRM_MODE_CONNECTOR_9PinDIN:
			return "9PinDIN-";
		case DRM_MODE_CONNECTOR_DisplayPort:
			return "DisplayPort-";
		case DRM_MODE_CONNECTOR_HDMIA:
			return "HDMI-A-";
		case DRM_MODE_CONNECTOR_HDMIB:
			return "HDMI-B-";
		case DRM_MODE_CONNECTOR_TV:
			return "TV-";
		case DRM_MODE_CONNECTOR_eDP:
			return "eDP-";
		case DRM_MODE_CONNECTOR_VIRTUAL:
			return "VIRTUAL-";
		case DRM_MODE_CONNECTOR_DSI:
			return "DSI-";
		case DRM_MODE_CONNECTOR_DPI:
			return "DPI-";
		default:
			return "not_defined-";
	}
}

//============================================================
//  id for class object (static)
//============================================================

static int static_id = 0;

//============================================================
//  drmkms_timing::drmkms_timing
//============================================================

drmkms_timing::drmkms_timing(char *device_name, custom_video_settings *vs)
{
	m_vs = *vs;
	m_id = ++static_id;

	log_verbose("DRM/KMS: <%d> (drmkms_timing) creation (%s)\n", m_id, device_name);
	// Copy screen device name and limit size
	if ((strlen(device_name) + 1) > 32)
	{
		strncpy(m_device_name, device_name, 31);
		log_error("DRM/KMS: <%d> (drmkms_timing) [ERROR] the devine name is too long it has been trucated to %s\n", m_id, m_device_name);
	}
	else
		strcpy(m_device_name, device_name);
}

//============================================================
//  drmkms_timing::~drmkms_timing
//============================================================

drmkms_timing::~drmkms_timing()
{
	// close DRM/KMS library
	if (mp_drm_handle)
		dlclose(mp_drm_handle);

	if (m_drm_fd > 0)
	{
		if (!--s_shared_count[m_card_id])
			close(m_drm_fd);
	}
}

//============================================================
//  drmkms_timing::init
//============================================================

bool drmkms_timing::init()
{
	log_verbose("DRM/KMS: <%d> (init) loading DRM/KMS library\n", m_id);
	mp_drm_handle = dlopen("libdrm.so", RTLD_NOW);
	if (mp_drm_handle)
	{
		p_drmGetVersion = (__typeof__(drmGetVersion)) dlsym(mp_drm_handle, "drmGetVersion");
		if (p_drmGetVersion == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmGetVersion", "DRM_LIBRARY");
			return false;
		}

		p_drmFreeVersion = (__typeof__(drmFreeVersion)) dlsym(mp_drm_handle, "drmFreeVersion");
		if (p_drmFreeVersion == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmFreeVersion", "DRM_LIBRARY");
			return false;
		}

		p_drmModeGetResources = (__typeof__(drmModeGetResources)) dlsym(mp_drm_handle, "drmModeGetResources");
		if (p_drmModeGetResources == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmModeGetResources", "DRM_LIBRARY");
			return false;
		}

		p_drmModeGetConnector = (__typeof__(drmModeGetConnector)) dlsym(mp_drm_handle, "drmModeGetConnector");
		if (p_drmModeGetConnector == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmModeGetConnector", "DRM_LIBRARY");
			return false;
		}

		p_drmModeFreeConnector = (__typeof__(drmModeFreeConnector)) dlsym(mp_drm_handle, "drmModeFreeConnector");
		if (p_drmModeFreeConnector == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmModeFreeConnector", "DRM_LIBRARY");
			return false;
		}

		p_drmModeFreeResources = (__typeof__(drmModeFreeResources)) dlsym(mp_drm_handle, "drmModeFreeResources");
		if (p_drmModeFreeResources == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmModeFreeResources", "DRM_LIBRARY");
			return false;
		}

		p_drmModeGetEncoder = (__typeof__(drmModeGetEncoder)) dlsym(mp_drm_handle, "drmModeGetEncoder");
		if (p_drmModeGetEncoder == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmModeGetEncoder", "DRM_LIBRARY");
			return false;
		}

		p_drmModeFreeEncoder = (__typeof__(drmModeFreeEncoder)) dlsym(mp_drm_handle, "drmModeFreeEncoder");
		if (p_drmModeFreeEncoder == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmModeFreeEncoder", "DRM_LIBRARY");
			return false;
		}

		p_drmModeGetCrtc = (__typeof__(drmModeGetCrtc)) dlsym(mp_drm_handle, "drmModeGetCrtc");
		if (p_drmModeGetCrtc == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmModeGetCrtc", "DRM_LIBRARY");
			return false;
		}

		p_drmModeSetCrtc = (__typeof__(drmModeSetCrtc)) dlsym(mp_drm_handle, "drmModeSetCrtc");
		if (p_drmModeSetCrtc == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmModeSetCrtc", "DRM_LIBRARY");
			return false;
		}

		p_drmModeFreeCrtc = (__typeof__(drmModeFreeCrtc)) dlsym(mp_drm_handle, "drmModeFreeCrtc");
		if (p_drmModeFreeCrtc == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmModeFreeCrtc", "DRM_LIBRARY");
			return false;
		}

		p_drmModeAttachMode = (__typeof__(drmModeAttachMode)) dlsym(mp_drm_handle, "drmModeAttachMode");
		if (p_drmModeAttachMode == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmModeAttachMode", "DRM_LIBRARY");
			return false;
		}

		p_drmModeAddFB = (__typeof__(drmModeAddFB)) dlsym(mp_drm_handle, "drmModeAddFB");
		if (p_drmModeAddFB == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmModeAddFB", "DRM_LIBRARY");
			return false;
		}

		p_drmModeRmFB = (__typeof__(drmModeRmFB)) dlsym(mp_drm_handle, "drmModeRmFB");
		if (p_drmModeRmFB == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmModeRmFB", "DRM_LIBRARY");
			return false;
		}

		p_drmModeGetFB = (__typeof__(drmModeGetFB)) dlsym(mp_drm_handle, "drmModeGetFB");
		if (p_drmModeGetFB == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmModeGetFB", "DRM_LIBRARY");
			return false;
		}

		p_drmModeFreeFB = (__typeof__(drmModeFreeFB)) dlsym(mp_drm_handle, "drmModeFreeFB");
		if (p_drmModeFreeFB == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmModeFreeFB", "DRM_LIBRARY");
			return false;
		}

		p_drmPrimeHandleToFD = (__typeof__(drmPrimeHandleToFD)) dlsym(mp_drm_handle, "drmPrimeHandleToFD");
		if (p_drmPrimeHandleToFD == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmPrimeHandleToFD", "DRM_LIBRARY");
			return false;
		}

		p_drmModeGetPlaneResources = (__typeof__(drmModeGetPlaneResources)) dlsym(mp_drm_handle, "drmModeGetPlaneResources");
		if (p_drmModeGetPlaneResources == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmModeGetPlaneResources", "DRM_LIBRARY");
			return false;
		}

		p_drmModeFreePlaneResources = (__typeof__(drmModeFreePlaneResources)) dlsym(mp_drm_handle, "drmModeFreePlaneResources");
		if (p_drmModeFreePlaneResources == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmModeFreePlaneResources", "DRM_LIBRARY");
			return false;
		}

		p_drmIoctl = (__typeof__(drmIoctl)) dlsym(mp_drm_handle, "drmIoctl");
		if (p_drmIoctl == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmIoctl", "DRM_LIBRARY");
			return false;
		}

		p_drmGetCap = (__typeof__(drmGetCap)) dlsym(mp_drm_handle, "drmGetCap");
		if (p_drmGetCap == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmGetCap", "DRM_LIBRARY");
			return false;
		}

		p_drmIsMaster = (__typeof__(drmIsMaster)) dlsym(mp_drm_handle, "drmIsMaster");
		if (p_drmIsMaster == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmIsMaster", "DRM_LIBRARY");
			return false;
		}

		p_drmSetMaster = (__typeof__(drmSetMaster)) dlsym(mp_drm_handle, "drmSetMaster");
		if (p_drmSetMaster == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmSetMaster", "DRM_LIBRARY");
			return false;
		}

		p_drmDropMaster = (__typeof__(drmDropMaster)) dlsym(mp_drm_handle, "drmDropMaster");
		if (p_drmDropMaster == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmDropMaster", "DRM_LIBRARY");
			return false;
		}
	}
	else
	{
		log_error("DRM/KMS: <%d> (init) [ERROR] missing %s library\n", m_id, "DRM/KMS_LIBRARY");
		return false;
	}

	int screen_pos = -1;

	// Handle the screen name, "auto", "screen[0-9]" and device name
	if (strlen(m_device_name) == 7 && !strncmp(m_device_name, "screen", 6) && m_device_name[6] >= '0' && m_device_name[6] <= '9')
		screen_pos = m_device_name[6] - '0';
	else if (strlen(m_device_name) == 1 && m_device_name[0] >= '0' && m_device_name[0] <= '9')
		screen_pos = m_device_name[0] - '0';

	char drm_name[15] = "/dev/dri/card_";
	drmModeRes *p_res;
	drmModeConnector *p_connector;

	int output_position = 0;
	for (int num = 0; !m_desktop_output && num < 10; num++)
	{
		drm_name[13] = '0' + num;
		m_drm_fd = open(drm_name, O_RDWR | O_CLOEXEC);

		if (m_drm_fd > 0)
		{
			drmVersion *version = drmGetVersion(m_drm_fd);
			log_verbose("DRM/KMS: <%d> (init) version %d.%d.%d type %s\n", m_id, version->version_major, version->version_minor, version->version_patchlevel, version->name);
			drmFreeVersion(version);

			uint64_t check_dumb = 0;
			if (drmGetCap(m_drm_fd, DRM_CAP_DUMB_BUFFER, &check_dumb) < 0)
				log_error("DRM/KMS: <%d> (init) [ERROR] ioctl DRM_CAP_DUMB_BUFFER\n", m_id);

			if (!check_dumb)
				log_error("DRM/KMS: <%d> (init) [ERROR] dumb buffer not supported\n", m_id);

			p_res = drmModeGetResources(m_drm_fd);

			for (int i = 0; i < p_res->count_connectors; i++)
			{
				p_connector = drmModeGetConnector(m_drm_fd, p_res->connectors[i]);
				if (p_connector)
				{
					char connector_name[32];
					snprintf(connector_name, 32, "%s%d", get_connector_name(p_connector->connector_type), p_connector->connector_type_id);
					log_verbose("DRM/KMS: <%d> (init) card %d connector %d id %d name %s status %d - modes %d\n", m_id, num, i, p_connector->connector_id, connector_name, p_connector->connection, p_connector->count_modes);
					// detect desktop connector
					if (!m_desktop_output && p_connector->connection == DRM_MODE_CONNECTED)
					{
						if (!strcmp(m_device_name, "auto") || !strcmp(m_device_name, connector_name) || output_position == screen_pos)
						{
							m_desktop_output = p_connector->connector_id;
							m_card_id = num;
							log_verbose("DRM/KMS: <%d> (init) card %d connector %d id %d name %s selected as primary output\n", m_id, num, i, m_desktop_output, connector_name);

							drmModeEncoder *p_encoder = drmModeGetEncoder(m_drm_fd, p_connector->encoder_id);

							if (p_encoder)
							{
								for (int e = 0; e < p_res->count_crtcs; e++)
								{
									mp_crtc_desktop = drmModeGetCrtc(m_drm_fd, p_res->crtcs[e]);

									if (mp_crtc_desktop->crtc_id == p_encoder->crtc_id)
									{
										log_verbose("DRM/KMS: <%d> (init) desktop mode name %s crtc %d fb %d valid %d\n", m_id, mp_crtc_desktop->mode.name, mp_crtc_desktop->crtc_id, mp_crtc_desktop->buffer_id, mp_crtc_desktop->mode_valid);
										break;
									}
									drmModeFreeCrtc(mp_crtc_desktop);
								}
							}
							if (!mp_crtc_desktop)
								log_error("DRM/KMS: <%d> (init) [ERROR] no crtc found\n", m_id);
							drmModeFreeEncoder(p_encoder);
						}
						output_position++;
					}
					drmModeFreeConnector(p_connector);
				}
				else
					log_error("DRM/KMS: <%d> (init) [ERROR] card %d connector %d - %d\n", m_id, num, i, p_res->connectors[i]);
			}
			drmModeFreeResources(p_res);
			if (!m_desktop_output)
				close(m_drm_fd);
			else
			{
				if (drmIsMaster(m_drm_fd))
				{
					s_shared_fd[m_card_id] = m_drm_fd;
					s_shared_count[m_card_id] = 1;
					drmDropMaster(m_drm_fd);
				}
				else
				{
					if (s_shared_count[m_card_id] > 0)
					{
						close(m_drm_fd);
						m_drm_fd = s_shared_fd[m_card_id];
						s_shared_count[m_card_id]++;
					}
					else if (m_id == 1)
					{
						log_verbose("DRM/KMS: <%d> (init) looking for the DRM master\n", m_id);
						int fd = drm_master_hook(m_drm_fd);
						if (fd)
						{
							close(m_drm_fd);
							m_drm_fd = fd;
							s_shared_fd[m_card_id] = m_drm_fd;
							// start at 2 to disable closing the fd
							s_shared_count[m_card_id] = 2;
						}
					}
				}
				if (!drmIsMaster(m_drm_fd))
					log_error("DRM/KMS: <%d> (init) [ERROR] limited DRM rights on this screen\n", m_id);
			}
		}
		else
		{
			if (!num)
				log_error("DRM/KMS: <%d> (init) [ERROR] cannot open device %s\n", m_id, drm_name);
			break;
		}
	}

	// Handle no screen detected case
	if (!m_desktop_output)
	{
		log_error("DRM/KMS: <%d> (init) [ERROR] no screen detected\n", m_id);
		return false;
	}
	else
	{
	}

	return true;
}

//============================================================
//  drmkms_timing::drm_master_hook
//============================================================

int drmkms_timing::drm_master_hook(int last_fd)
{
	for (int fd = 4; fd < last_fd; fd++)
	{
		struct stat st;
		if (!fstat(fd, &st))
		{
			// in case of multiple video cards, it wouldd be better to compare dri number
			if (S_ISCHR(st.st_mode))
			{
				if (drmIsMaster(fd))
				{
					drmVersion *version_hook = drmGetVersion(m_drm_fd);
					log_verbose("DRM/KMS: <%d> (init) DRM hook created version %d.%d.%d type %s\n", m_id, version_hook->version_major, version_hook->version_minor, version_hook->version_patchlevel, version_hook->name);
					drmFreeVersion(version_hook);
					return fd;
				}
			}
		}
	}
	return 0;
}

//============================================================
//  drmkms_timing::update_mode
//============================================================

bool drmkms_timing::update_mode(modeline *mode)
{
	if (!mode)
		return false;

	if (!m_desktop_output)
	{
		log_error("DRM/KMS: <%d> (update_mode) [ERROR] no screen detected\n", m_id);
		return false;
	}

	if (!delete_mode(mode))
	{
		log_error("DRM/KMS: <%d> (update_mode) [ERROR] delete operation not successful", m_id);
		return false;
	}

	if (!add_mode(mode))
	{
		log_error("DRM/KMS: <%d> (update_mode) [ERROR] add operation not successful", m_id);
		return false;
	}

	return true;
}

//============================================================
//  drmkms_timing::add_mode
//============================================================

bool drmkms_timing::add_mode(modeline *mode)
{
	if (!mode)
		return false;

	// Handle no screen detected case
	if (!m_desktop_output)
	{
		log_error("DRM/KMS: <%d> (add_mode) [ERROR] no screen detected\n", m_id);
		return false;
	}

	if (!mp_crtc_desktop)
	{
		log_error("DRM/KMS: <%d> (add_mode) [ERROR] no desktop crtc\n", m_id);
		return false;
	}

	if (!mode)
		return false;

	return true;
}

//============================================================
//  drmkms_timing::set_timing
//============================================================

bool drmkms_timing::set_timing(modeline *mode)
{
	if (!mode)
		return false;

	// Handle no screen detected case
	if (!m_desktop_output)
	{
		log_error("DRM/KMS: <%d> (set_timing) [ERROR] no screen detected\n", m_id);
		return false;
	}

	drmSetMaster(m_drm_fd);

	// Setup the DRM mode structure
	drmModeModeInfo dmode = {};

	// Create specific mode name
	snprintf(dmode.name, 32, "SR-%d_%dx%d@%.02f%s", m_id, mode->hactive, mode->vactive, mode->vfreq, mode->interlace ? "i" : "");
	dmode.clock       = mode->pclock / 1000;
	dmode.hdisplay    = mode->hactive;
	dmode.hsync_start = mode->hbegin;
	dmode.hsync_end   = mode->hend;
	dmode.htotal      = mode->htotal;
	dmode.vdisplay    = mode->vactive;
	dmode.vsync_start = mode->vbegin;
	dmode.vsync_end   = mode->vend;
	dmode.vtotal      = mode->vtotal;
	dmode.flags       = (mode->interlace ? DRM_MODE_FLAG_INTERLACE : 0) | (mode->doublescan ? DRM_MODE_FLAG_DBLSCAN : 0) | (mode->hsync ? DRM_MODE_FLAG_PHSYNC : DRM_MODE_FLAG_NHSYNC) | (mode->vsync ? DRM_MODE_FLAG_PVSYNC : DRM_MODE_FLAG_NVSYNC);

	dmode.hskew       = 0;
	dmode.vscan       = 0;

	dmode.vrefresh    = mode->refresh;  // Used only for human readable output

	dmode.type        = DRM_MODE_TYPE_USERDEF;  //DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

	mode->type |= CUSTOM_VIDEO_TIMING_DRMKMS;

	if (mode->platform_data == 4815162342)
	{
		log_verbose("DRM/KMS: <%d> (set_timing) <debug> restore desktop mode\n", m_id);
		drmModeSetCrtc(m_drm_fd, mp_crtc_desktop->crtc_id, mp_crtc_desktop->buffer_id, mp_crtc_desktop->x, mp_crtc_desktop->y, &m_desktop_output, 1, &mp_crtc_desktop->mode);
		if (m_dumb_handle)
		{
			int ret = ioctl(m_drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &m_dumb_handle);
			if (ret)
				log_verbose("DRM/KMS: <%d> (add_mode) [ERROR] ioctl DRM_IOCTL_MODE_DESTROY_DUMB %d\n", m_id, ret);
			m_dumb_handle = 0;
		}
		if (m_framebuffer_id && m_framebuffer_id != mp_crtc_desktop->buffer_id)
		{
			if (drmModeRmFB(m_drm_fd, m_framebuffer_id))
				log_verbose("DRM/KMS: <%d> (add_mode) [ERROR] remove frame buffer\n", m_id);
			m_framebuffer_id = 0;
		}
	}
	else
	{
		unsigned int old_dumb_handle = m_dumb_handle;

		drmModeFB *pframebuffer = drmModeGetFB(m_drm_fd, mp_crtc_desktop->buffer_id);
		log_verbose("DRM/KMS: <%d> (add_mode) <debug> existing frame buffer id %d size %dx%d bpp %d\n", m_id, mp_crtc_desktop->buffer_id, pframebuffer->width, pframebuffer->height, pframebuffer->bpp);
		//drmModePlaneRes *pplanes = drmModeGetPlaneResources(m_drm_fd);
		//log_verbose("DRM/KMS: <%d> (add_mode) <debug> total planes %d\n", m_id, pplanes->count_planes);
		//drmModeFreePlaneResources(pplanes);

		unsigned int framebuffer_id = mp_crtc_desktop->buffer_id;

		//if (pframebuffer->width < dmode.hdisplay || pframebuffer->height < dmode.vdisplay)
		if (1)
		{
			log_verbose("DRM/KMS: <%d> (add_mode) <debug> creating new frame buffer with size %dx%d\n", m_id, dmode.hdisplay, dmode.vdisplay);

			// create a new dumb fb (not driver specefic)
			drm_mode_create_dumb create_dumb = {};
			create_dumb.width = dmode.hdisplay;
			create_dumb.height = dmode.vdisplay;
			create_dumb.bpp = pframebuffer->bpp;

			int ret = ioctl(m_drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb);
			if (ret)
				log_verbose("DRM/KMS: <%d> (add_mode) [ERROR] ioctl DRM_IOCTL_MODE_CREATE_DUMB %d\n", m_id, ret);

			if (drmModeAddFB(m_drm_fd, dmode.hdisplay, dmode.vdisplay, pframebuffer->depth, pframebuffer->bpp, create_dumb.pitch, create_dumb.handle, &framebuffer_id))
				log_error("DRM/KMS: <%d> (add_mode) [ERROR] cannot add frame buffer\n", m_id);
			else
				m_dumb_handle = create_dumb.handle;

			drm_mode_map_dumb map_dumb = {};
			map_dumb.handle = create_dumb.handle;

			ret = drmIoctl(m_drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb);
			if (ret)
				log_verbose("DRM/KMS: <%d> (add_mode) [ERROR] ioctl DRM_IOCTL_MODE_MAP_DUMB %d\n", m_id, ret);

			void *map = mmap(0, create_dumb.size, PROT_READ | PROT_WRITE, MAP_SHARED, m_drm_fd, map_dumb.offset);
			if (map != MAP_FAILED)
			{
				// clear the frame buffer
				memset(map, 0, create_dumb.size);
			}
			else
				log_verbose("DRM/KMS: <%d> (add_mode) [ERROR] failed to map frame buffer %p\n", m_id, map);
		}
		else
			log_verbose("DRM/KMS: <%d> (add_mode) <debug> use existing frame buffer\n", m_id);

		drmModeFreeFB(pframebuffer);

		pframebuffer = drmModeGetFB(m_drm_fd, framebuffer_id);
		log_verbose("DRM/KMS: <%d> (add_mode) <debug> frame buffer id %d size %dx%d bpp %d\n", m_id, framebuffer_id, pframebuffer->width, pframebuffer->height, pframebuffer->bpp);
		drmModeFreeFB(pframebuffer);

		// set the mode on the crtc
		if (drmModeSetCrtc(m_drm_fd, mp_crtc_desktop->crtc_id, framebuffer_id, 0, 0, &m_desktop_output, 1, &dmode))
			log_error("DRM/KMS: <%d> (add_mode) [ERROR] cannot attach the mode to the crtc %d frame buffer %d\n", m_id, mp_crtc_desktop->crtc_id, framebuffer_id);
		else
		{
			if (old_dumb_handle)
			{
				log_verbose("DRM/KMS: <%d> (add_mode) <debug> remove old dumb %d\n", m_id, old_dumb_handle);
				int ret = ioctl(m_drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &old_dumb_handle);
				if (ret)
					log_verbose("DRM/KMS: <%d> (add_mode) [ERROR] ioctl DRM_IOCTL_MODE_DESTROY_DUMB %d\n", m_id, ret);
				old_dumb_handle = 0;
			}
			if (m_framebuffer_id && framebuffer_id != mp_crtc_desktop->buffer_id)
			{
				log_verbose("DRM/KMS: <%d> (add_mode) <debug> remove old frame buffer %d\n", m_id, m_framebuffer_id);
				drmModeRmFB(m_drm_fd, m_framebuffer_id);
				m_framebuffer_id = 0;
			}
			m_framebuffer_id = framebuffer_id;
		}
	}
	drmDropMaster(m_drm_fd);

	return true;
}

//============================================================
//  drmkms_timing::delete_mode
//============================================================

bool drmkms_timing::delete_mode(modeline *mode)
{
	if (!mode)
		return false;

	// Handle no screen detected case
	if (!m_desktop_output)
	{
		log_error("DRM/KMS: <%d> (delete_mode) [ERROR] no screen detected\n", m_id);
		return false;
	}

	return true;
}

//============================================================
//  drmkms_timing::get_timing
//============================================================

bool drmkms_timing::get_timing(modeline *mode)
{
	// Handle no screen detected case
	if (!m_desktop_output)
	{
		log_error("DRM/KMS: <%d> (get_timing) [ERROR] no screen detected\n", m_id);
		return false;
	}

	// INFO: not used vrefresh, hskew, vscan
	drmModeRes *p_res = drmModeGetResources(m_drm_fd);

	for (int i = 0; i < p_res->count_connectors; i++)
	{
		drmModeConnector *p_connector = drmModeGetConnector(m_drm_fd, p_res->connectors[i]);

		// Cycle through the modelines and report them back to the display manager
		if (p_connector && m_desktop_output == p_connector->connector_id)
		{
			if (m_video_modes_position < p_connector->count_modes)
			{
				drmModeModeInfo *pdmode = &p_connector->modes[m_video_modes_position++];

				// Use mode position as index
				mode->platform_data = m_video_modes_position;

				mode->pclock        = pdmode->clock * 1000;
				mode->hactive       = pdmode->hdisplay;
				mode->hbegin        = pdmode->hsync_start;
				mode->hend          = pdmode->hsync_end;
				mode->htotal        = pdmode->htotal;
				mode->vactive       = pdmode->vdisplay;
				mode->vbegin        = pdmode->vsync_start;
				mode->vend          = pdmode->vsync_end;
				mode->vtotal        = pdmode->vtotal;
				mode->interlace     = (pdmode->flags & DRM_MODE_FLAG_INTERLACE) ? 1 : 0;
				mode->doublescan    = (pdmode->flags & DRM_MODE_FLAG_DBLSCAN) ? 1 : 0;
				mode->hsync         = (pdmode->flags & DRM_MODE_FLAG_PHSYNC) ? 1 : 0;
				mode->vsync         = (pdmode->flags & DRM_MODE_FLAG_PVSYNC) ? 1 : 0;

				mode->hfreq         = mode->pclock / mode->htotal;
				mode->vfreq         = mode->hfreq / mode->vtotal * (mode->interlace ? 2 : 1);
				mode->refresh       = mode->vfreq;

				mode->width         = pdmode->hdisplay;
				mode->height        = pdmode->vdisplay;

				// Add the rotation flag from the plane (DRM_MODE_ROTATE_xxx)
				// TODO: mode->type |= MODE_ROTATED;

				mode->type |= CUSTOM_VIDEO_TIMING_DRMKMS;

				if (strncmp(pdmode->name, "SR-", 3) == 0)
					log_verbose("DRM/KMS: <%d> (get_timing) [WARNING] modeline %s detected\n", m_id, pdmode->name);
				else if (!strcmp(pdmode->name, mp_crtc_desktop->mode.name) && pdmode->clock == mp_crtc_desktop->mode.clock && pdmode->vrefresh == mp_crtc_desktop->mode.vrefresh)
				{
					// Add the desktop flag to desktop modeline
					log_verbose("DRM/KMS: <%d> (get_timing) desktop mode name %s refresh %d found\n", m_id, mp_crtc_desktop->mode.name, mp_crtc_desktop->mode.vrefresh);
					mode->type |= MODE_DESKTOP;
					mode->platform_data = 4815162342;
				}
			}
			else
			{
				// Inititalise the position for the modeline list
				m_video_modes_position = 0;
			}
		}
		drmModeFreeConnector(p_connector);
	}
	drmModeFreeResources(p_res);

	return true;
}
