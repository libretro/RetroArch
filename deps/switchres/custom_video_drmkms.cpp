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
#include <dirent.h>
#include "custom_video_drmkms.h"
#include "log.h"

#define drmGetVersion p_drmGetVersion
#define drmFreeVersion p_drmFreeVersion
#define drmModeGetResources p_drmModeGetResources
#define drmModeGetConnector p_drmModeGetConnector
#define drmModeGetConnectorCurrent p_drmModeGetConnectorCurrent
#define drmModeFreeConnector p_drmModeFreeConnector
#define drmModeFreeResources p_drmModeFreeResources
#define drmModeGetEncoder p_drmModeGetEncoder
#define drmModeFreeEncoder p_drmModeFreeEncoder
#define drmModeGetCrtc p_drmModeGetCrtc
#define drmModeSetCrtc p_drmModeSetCrtc
#define drmModeFreeCrtc p_drmModeFreeCrtc
#define drmModeAttachMode p_drmModeAttachMode
#define drmModeDetachMode p_drmModeDetachMode
#define drmModeAddFB p_drmModeAddFB
#define drmModeRmFB p_drmModeRmFB
#define drmModeGetFB p_drmModeGetFB
#define drmModeFreeFB p_drmModeFreeFB
#define drmPrimeHandleToFD p_drmPrimeHandleToFD
#define drmModeGetPlaneResources p_drmModeGetPlaneResources
#define drmModeFreePlaneResources p_drmModeFreePlaneResources
#define drmIoctl p_drmIoctl
#define drmGetCap p_drmGetCap
#define drmGetDevices2 p_drmGetDevices2
#define drmIsMaster p_drmIsMaster
#define drmSetMaster p_drmSetMaster
#define drmDropMaster p_drmDropMaster

# define MAX_CARD_ID 10
# define MAX_DRM_DEVICES 16

// To enable libdrmhook: make SR_WITH_DRMHOOK=1
#ifdef SR_WITH_DRMHOOK
	#define hook_handle RTLD_DEFAULT
	#define hook_log " (will attempt hook)"
#else
	#define hook_handle mp_drm_handle
	#define hook_log ""
#endif

//============================================================
//  shared the privileges of the master fd
//============================================================

// If 2 displays use the same GPU but a different connector, let's share the
// FD indexed on the card ID

static int s_shared_fd[MAX_CARD_ID] = {};

// The active shares on a fd, per card id

static int s_shared_count[MAX_CARD_ID] = {};

// What we're missing here, is also a list of the connector ids associated with
// the screen number, otherwise SR will try to use (again) the first connector
// that has an monitor plugged to it

static unsigned int s_shared_conn[MAX_CARD_ID] = {};

//============================================================
//  id for class object (static)
//============================================================

// This helps to trace counts of active displays accross vaious instances
// ++'ed at constructor, --'ed at destructor
// m_id will use the ++-ed value

static int static_id = 0;

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
//  Check if a connector is not used on a previous display
//============================================================

bool connector_already_used(unsigned int conn_id)
{
	// Don't remap to an already used connector
	for (int c = 1 ; c < static_id ; c++)
	{
		if (s_shared_conn[c] == conn_id)
			return true;
	}
	return false;
}

//============================================================
//  Convert a SR modeline to a DRM modeline
//============================================================

void modeline_to_drm_modeline(int id, modeline *mode, drmModeModeInfo *drmmode)
{
	// Clear struct
	memset(drmmode, 0, sizeof(drmModeModeInfo));

	// Create specific mode name
	snprintf(drmmode->name, 32, "SR-%d_%dx%d@%.02f%s", id, mode->hactive, mode->vactive, mode->vfreq, mode->interlace ? "i" : "");
	drmmode->clock       = mode->pclock / 1000;
	drmmode->hdisplay    = mode->hactive;
	drmmode->hsync_start = mode->hbegin;
	drmmode->hsync_end   = mode->hend;
	drmmode->htotal      = mode->htotal;
	drmmode->vdisplay    = mode->vactive;
	drmmode->vsync_start = mode->vbegin;
	drmmode->vsync_end   = mode->vend;
	drmmode->vtotal      = mode->vtotal;
	drmmode->flags       = (mode->interlace ? DRM_MODE_FLAG_INTERLACE : 0) | (mode->doublescan ? DRM_MODE_FLAG_DBLSCAN : 0) | (mode->hsync ? DRM_MODE_FLAG_PHSYNC : DRM_MODE_FLAG_NHSYNC) | (mode->vsync ? DRM_MODE_FLAG_PVSYNC : DRM_MODE_FLAG_NVSYNC);

	drmmode->hskew       = 0;
	drmmode->vscan       = 0;

	drmmode->vrefresh    = mode->refresh;  // Used only for human readable output
}

//============================================================
//  drmkms_timing::test_kernel_user_modes
//============================================================

bool drmkms_timing::test_kernel_user_modes()
{
	int ret = 0, first_modes_count = 0, second_modes_count = 0;
	int fd;
	drmModeModeInfo mode = {};
	const char* my_name = "KMS Test mode";
	drmModeConnector *conn;

	// Make sure we are master, that is required for the IOCTL
	fd = get_master_fd();
	if (fd < 0)
	{
		log_verbose("DRM/KMS: <%d> (%s) Need master to test kernel user modes\n", m_id, __FUNCTION__);
		return false;
	}

	// Create a dummy modeline with a pixel clock higher than 25MHz to avoid
	// drivers checks rejecting the mode. Use a modeline that no one would
	// ever use hopefully
	strcpy(mode.name, my_name);
	mode.clock       = 25212;
	mode.hdisplay    = 1234;
	mode.hsync_start = 1290;
	mode.hsync_end   = 1408;
	mode.htotal      = 1610;
	mode.vdisplay    = 234;
	mode.vsync_start = 238;
	mode.vsync_end   = 241;
	mode.vtotal      = 261;
	mode.flags       = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC;

	// Count the number of existing modes, so it should be +1 when attaching
	// a new mode. Could also check the mode name, still better
	conn = drmModeGetConnector(fd, m_desktop_output);
	if (!conn)
	{
		log_verbose("DRM/KMS: <%d> (%s) Cannot get connector\n", m_id, __FUNCTION__);
		m_kernel_user_modes = false;
		return false;
	}

	first_modes_count = conn->count_modes;
	ret = drmModeAttachMode(fd, m_desktop_output, &mode);
	drmModeFreeConnector(conn);

	// This case can only happen if we're not drmMaster. If the kernel doesn't
	// support adding new modes, the IOCTL will still return 0, not an error
	if (ret < 0)
	{
		// Let's fail, no need to go further
		log_verbose("DRM/KMS: <%d> (%s) Cannot add new kernel user mode\n", m_id, __FUNCTION__);
		m_kernel_user_modes = false;
		return false;
	}

	// Not using drmModeGetConnectorCurrent here since we need to force a
	// modelist connector refresh, so the kernel will probe the connector
	conn = drmModeGetConnector(fd, m_desktop_output);
	second_modes_count = conn->count_modes;
	if (first_modes_count != second_modes_count)
	{
		log_verbose("DRM/KMS: <%d> (%s) Kernel supports user modes (%d vs %d)\n", m_id, __FUNCTION__, first_modes_count, second_modes_count);
		m_kernel_user_modes = true;
		drmModeDetachMode(fd, m_desktop_output, &mode);
		if (fd != m_hook_fd)
			drmDropMaster(fd);
	}
	else
		log_verbose("DRM/KMS: <%d> (%s) Kernel doesn't supports user modes\n", m_id, __FUNCTION__);

	drmModeFreeConnector(conn);
	return m_kernel_user_modes;
}

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
	// Remove kernel user modes
	if (m_kernel_user_modes)
	{
		int i = 0, ret = 0;
		int fd;
		drmModeConnector *conn;

		fd = get_master_fd();
		if (fd >= 0)
		{
			conn = drmModeGetConnectorCurrent(fd, m_desktop_output);
			drmSetMaster(fd);
			for (i = 0; i < conn->count_modes; i++)
			{
				drmModeModeInfo *mode = &conn->modes[i];
				log_verbose("DRM/KMS: <%d> (%s) Checking kernel mode: %s\n", m_id, __FUNCTION__, mode->name);
				ret = strncmp(mode->name, "SR-", 3);
				if (ret == 0)
				{
					log_verbose("DRM/KMS: <%d> (%s) Removing kernel user mode: %s\n", m_id, __FUNCTION__, mode->name);
					drmModeDetachMode(fd, m_desktop_output, mode);
				}
			}
			if (fd != m_hook_fd)
				drmDropMaster(fd);

			drmModeFreeConnector(conn);
			if (fd != m_drm_fd and fd != m_hook_fd)
				close(fd);
		}
	}

	// Free the connector used
	s_shared_conn[m_id] = -1;

	// close DRM/KMS library
	if (mp_drm_handle)
		dlclose(mp_drm_handle);

	if (m_drm_fd > 0)
	{
		if (!--s_shared_count[m_card_id])
			close(m_drm_fd);
	}

	// Reset static data
	static_id = 0;
	memset(s_shared_fd, 0, sizeof(s_shared_fd));
	memset(s_shared_count, 0, sizeof(s_shared_count));
	memset(s_shared_conn, 0, sizeof(s_shared_conn));
}

//============================================================
//  drmkms_timing::init
//============================================================

bool drmkms_timing::init()
{
	log_verbose("DRM/KMS: <%d> (init) loading DRM/KMS library%s\n", m_id, hook_log);
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

		p_drmModeGetConnector = (__typeof__(drmModeGetConnector)) dlsym(hook_handle, "drmModeGetConnector");
		if (p_drmModeGetConnector == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmModeGetConnector", "DRM_LIBRARY");
			return false;
		}

		p_drmModeGetConnectorCurrent = (__typeof__(drmModeGetConnectorCurrent)) dlsym(hook_handle, "drmModeGetConnectorCurrent");
		if (p_drmModeGetConnectorCurrent == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmModeGetConnectorCurrent", "DRM_LIBRARY");
			return false;
		}

		p_drmModeFreeConnector = (__typeof__(drmModeFreeConnector)) dlsym(hook_handle, "drmModeFreeConnector");
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

		p_drmModeDetachMode = (__typeof__(drmModeDetachMode)) dlsym(mp_drm_handle, "drmModeDetachMode");
		if (p_drmModeDetachMode == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmModeDetachMode", "DRM_LIBRARY");
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

		p_drmGetDevices2 = (__typeof__(drmGetDevices2)) dlsym(mp_drm_handle, "drmGetDevices2");
		if (p_drmGetDevices2 == NULL)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] missing func %s in %s", m_id, "drmGetDevices2", "DRM_LIBRARY");
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

	// Get an array of drm devices to check
	drmDevicePtr devices[MAX_DRM_DEVICES];
	int num_devices = drmGetDevices2(0, NULL, 0);

	if (num_devices > MAX_DRM_DEVICES)
		num_devices = MAX_DRM_DEVICES;

	int ret = drmGetDevices2(0, devices, num_devices);
	if (ret < 0)
	{
		log_error("DRM/KMS: drmGetDevices2() returned an error %d\n", ret);
		return false;
	}

	char *drm_name;
	drmModeRes *p_res;
	drmModeConnector *p_connector;

	int output_position = 0;
	for (int num = 0; num < num_devices; num++)
	{
		// Skip non-primary nodes
		if (devices[num]->available_nodes & (1 << DRM_NODE_PRIMARY))
			drm_name = devices[num]->nodes[DRM_NODE_PRIMARY];
		else continue;

		if (!access(drm_name, F_OK) == 0)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] cannot open device %s\n", m_id, drm_name);
			break;
		}
		m_drm_fd = open(drm_name, O_RDWR | O_CLOEXEC);

		drmVersion *version = drmGetVersion(m_drm_fd);
		log_verbose("DRM/KMS: <%d> (init) version %d.%d.%d type %s\n", m_id, version->version_major, version->version_minor, version->version_patchlevel, version->name);
		drmFreeVersion(version);

		uint64_t check_dumb = 0;
		if (drmGetCap(m_drm_fd, DRM_CAP_DUMB_BUFFER, &check_dumb) < 0)
			log_error("DRM/KMS: <%d> (init) [ERROR] ioctl DRM_CAP_DUMB_BUFFER\n", m_id);

		if (!check_dumb)
		{
			log_error("DRM/KMS: <%d> (init) [ERROR] dumb buffer not supported\n", m_id);
			continue;
		}

		p_res = drmModeGetResources(m_drm_fd);

		for (int i = 0; i < p_res->count_connectors; i++)
		{
			p_connector = drmModeGetConnectorCurrent(m_drm_fd, p_res->connectors[i]);
			if (!p_connector)
			{
				log_error("DRM/KMS: <%d> (init) [ERROR] card %d connector %d - %d\n", m_id, num, i, p_res->connectors[i]);
				continue;
			}
			char connector_name[32];
			snprintf(connector_name, 32, "%s%d", get_connector_name(p_connector->connector_type), p_connector->connector_type_id);
			log_verbose("DRM/KMS: <%d> (init) card %d connector %d id %d name %s status %d - modes %d\n", m_id, num, i, p_connector->connector_id, connector_name, p_connector->connection, p_connector->count_modes);
			// detect desktop connector
			if (!m_desktop_output && p_connector->connection == DRM_MODE_CONNECTED)
			{
				if (!strcmp(m_device_name, "auto") || !strcmp(m_device_name, connector_name) || output_position == screen_pos)
				{
					// In a multihead setup, skip already used connectors
					if (connector_already_used(p_connector->connector_id))
					{
						drmModeFreeConnector(p_connector);
						continue;
					}
					m_desktop_output = p_connector->connector_id;
					m_card_id = num;
					strcpy(m_drm_name, drm_name);
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
					{
						m_desktop_output = 0;
						log_error("DRM/KMS: <%d> (init) [ERROR] no crtc found\n", m_id);
					}
					drmModeFreeEncoder(p_encoder);
				}
				output_position++;
			}
			drmModeFreeConnector(p_connector);
		}
		drmModeFreeResources(p_res);
		if (!m_desktop_output)
			close(m_drm_fd);
		else
		{
			if (drmIsMaster(m_drm_fd))
			{
				 // We've never called drmSetMaster before. This means we're the first app
				 // opening the device, so the kernel sets us as master by default.
				 // We drop master so other apps can become master
				log_verbose("DRM/KMS: <%d> (%s) Already DRM master\n", m_id, __FUNCTION__);
				s_shared_fd[m_card_id] = m_drm_fd;
				s_shared_count[m_card_id] = 1;
				drmDropMaster(m_drm_fd);
			}
			else
			{
				if (s_shared_count[m_card_id] > 0)
				{
					log_verbose("DRM/KMS: <%d> (%s : %d) The drm FD was substituted, expect the unexpected\n", m_id, __FUNCTION__, __LINE__);
					close(m_drm_fd);
					m_drm_fd = s_shared_fd[m_card_id];
					s_shared_count[m_card_id]++;
				}
				else if (m_id == 1)
				{
					log_verbose("DRM/KMS: <%d> (%s) looking for the DRM master\n", m_id, __FUNCTION__);
					int fd = get_master_fd();
					if (fd >= 0)
					{
						close(m_drm_fd);
						 // This statement is dangerous, as drmIsMaster can return 1
						 // on m_drm_fd if there is no master left, but it doesn't
						 // check if m_drm_fd is a valid fd

						m_drm_fd = fd;
						s_shared_fd[m_card_id] = m_drm_fd;
						// start at 2 to disable closing the fd
						s_shared_count[m_card_id] = 2;
					}
					if (!drmIsMaster(m_drm_fd))
					{
						m_desktop_output = 0;
						log_error("DRM/KMS: <%d> (%s) [ERROR] limited DRM rights on this screen\n", m_id, __FUNCTION__);
					}
				}
			}
			// If we're here and we have a valid output, we're done.
			if (m_desktop_output) break;
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

	// Check if we have a libdrm hook
	if (drmModeGetConnectorCurrent(-1, 0) != NULL)
	{
		log_verbose("DRM/KMS: libdrm hook found!\n");
		m_caps |= CUSTOM_VIDEO_CAPS_UPDATE;
	}
	// Check if the kernel handles user modes
	else if (test_kernel_user_modes())
		m_caps |= CUSTOM_VIDEO_CAPS_ADD;

	if (drmIsMaster(m_drm_fd) and m_drm_fd != m_hook_fd)
		drmDropMaster(m_drm_fd);

	return true;
}

//============================================================
//  drmkms_timing::get_master_fd
//============================================================
// BACKGROUND
// This is written as of Linux 5.14, 5.15 is just out, not yet tested.
// There are a few unexpected behaviours so far in DRM:
//   - drmSetMaster seems to always return -1 on 5.4, but ok on 5.14
//   - drmIsMaster doesn't care if the FD exists and will always return 1
//     if the there is no master on the DRI device
// That's why we can't trust drmIsMaster if we didn't make sure before that
// the FD does exist.
// get_master_fd will always return a valid master FD, or return -1 if it's
// impossible

int drmkms_timing::get_master_fd()
{
	const size_t path_length = 20;
	char procpath[path_length];
	char fullpath[512];
	char* actualpath;
	struct stat st;
	int fd;

	// CASE 1: m_drm_fd is a valid FD
	if (fstat(m_drm_fd, &st) == 0)
	{
		if (drmIsMaster(m_drm_fd))
			return m_drm_fd;
		if (drmSetMaster(m_drm_fd) == 0)
			return m_drm_fd;
	}

	// CASE 2: m_drm_fd can't be master, find the master FD
	if (m_card_id > MAX_CARD_ID - 1 or m_card_id < 0)
	{
		log_error("DRM/KMS: <%d> (%s) [ERROR] card id (%d) out of bounds (0 to %d)\n", m_id, __FUNCTION__, m_card_id, MAX_CARD_ID - 1);
		return -1;
	}

	if (!access(m_drm_name, F_OK) == 0)
	{
		log_error("DRM/KMS: <%d> (%s) [ERROR] Device %s doesn't exist\n", m_id, __FUNCTION__, m_drm_name);
		return -1;
	}

	sprintf(procpath, "/proc/%d/fd", getpid());
	auto dir = opendir(procpath);
	if (!dir)
		return -1;
	while (auto f = readdir(dir))
	{
		// Skip everything that starts with a dot
		if (f->d_name[0] == '.')
			continue;
		// Only symlinks matter
		if (f-> d_type != DT_LNK)
			continue;

		//log_verbose("File: %s\n", f->d_name);
		sprintf(fullpath, "%s/%s", procpath, f->d_name);
		if (stat(fullpath, &st))
			continue;
		if (!S_ISCHR(st.st_mode))
			continue;
		actualpath = realpath(fullpath, NULL);
		// Only check the device we expect
		if (strncmp(m_drm_name, actualpath, path_length) != 0)
		{
			free(actualpath);
			continue;
		}
		fd = atoi(f->d_name);
		//log_verbose("File: %s -> %s %d\n", fullpath, actualpath, fd);
		free(actualpath);

		if (drmIsMaster(fd))
		{
			log_verbose("DRM/KMS: <%d> (%s) DRM hook created on FD %d\n", m_id, __FUNCTION__, fd);
			closedir(dir);
			m_hook_fd = fd;
			return fd;
		}
	}
	closedir(dir);

	// CASE 3: m_drm_fd is not a master (and probably not even a valid FD), the currend pid doesn't have master rights
	// Or master is owned by a 3rd party app (like a frontend ...)
	log_verbose("DRM/KMS: <%d> (%s) Couldn't find a master FD, opening default %s\n", m_id, __FUNCTION__, m_drm_name);

	// mark our former hook as invalid
	m_hook_fd = -1;

	fd = open(m_drm_name, O_RDWR | O_CLOEXEC);
	if (fd < 0)
	{
		// Oh, we're totally screwed here, worst possible scenario
		log_error("DRM/KMS: <%d> (%s) Can't open %s, can't get master rights\n", m_id, __FUNCTION__, m_drm_name);
		return -1;
	}

	// Hardly any chance we reach here. I don't even know when to close the FD ...
	if (drmIsMaster(fd) or drmSetMaster(fd) == 0)
		return fd;

	// There is definitely no way we get master ...
	close(fd);
	log_error("DRM/KMS: <%d> (%s) No way to get master rights!\n", m_id, __FUNCTION__);
	return -1;
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

	// Without libdrm hook, the update method isn't natively supported, so we must delete
	// the mode and readd it with updated timings.
	if (!(m_caps & CUSTOM_VIDEO_CAPS_UPDATE))
	{
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

	// libdrn hook case, we can update timings directly in the connector's data
	drmModeConnector *conn = drmModeGetConnectorCurrent(m_drm_fd, m_desktop_output);
	if (conn)
	{
		for (int i = 0; i < conn->count_modes; i++)
		{
			drmModeModeInfo *drmmode = &conn->modes[i];
			if ((int)mode->platform_data == i)
			{
				int m_type = drmmode->type;
				modeline_to_drm_modeline(m_id, mode, drmmode);
				drmmode->type = m_type;
				return true;
			}
		}
	}

	return false;
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

	if (m_kernel_user_modes)
	{
		int ret = 0, fd = m_drm_fd;
		drmModeModeInfo drmmode;

		if (!drmIsMaster(fd))
			fd = get_master_fd();

		if (!drmIsMaster(fd))
		{
			log_error("DRM/KMS: <%d> (%s) Need master to add a kernel mode (%d)\n", m_id, __FUNCTION__, ret);
			return false;
		}

		modeline_to_drm_modeline(m_id, mode, &drmmode);
		drmmode.type = DRM_MODE_TYPE_USERDEF;

		log_verbose("DRM/KMS: <%d> (add_mode) [DEBUG] Adding a mode to the kernel: %dx%d %s\n", m_id, drmmode.hdisplay, drmmode.vdisplay, drmmode.name);
		// Calling drmModeGetConnector forces a refresh of the connector modes, which is slow, so don't do it
		ret = drmModeAttachMode(fd, m_desktop_output, &drmmode);
		if (ret != 0)
		{
			// This case hardly has any chance to happen, since at this point
			// we are drmMaster, and we have already checked that the kernel
			// supports user modes. If any error, it's on the kernel side
			log_verbose("DRM/KMS: <%d> (%s) Couldn't add mode (ret=%d)\n", m_id, __FUNCTION__, ret);
			if (fd != m_hook_fd)
				drmDropMaster(fd);
			return false;
		}
		log_verbose("DRM/KMS: <%d> (%s) Mode added\n", m_id, __FUNCTION__);
		if (fd != m_hook_fd)
			drmDropMaster(fd);
	}

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

	if (!kms_has_mode(mode))
		add_mode(mode);

	// If we can't be master, no need to go further
	drmSetMaster(m_drm_fd);
	if (!drmIsMaster(m_drm_fd))
		return false;

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

	if (mode->type & MODE_DESKTOP)
	{
		log_verbose("DRM/KMS: <%d> (set_timing) <debug> restore desktop mode\n", m_id);
		drmModeSetCrtc(m_drm_fd, mp_crtc_desktop->crtc_id, mp_crtc_desktop->buffer_id, mp_crtc_desktop->x, mp_crtc_desktop->y, &m_desktop_output, 1, &mp_crtc_desktop->mode);
		if (m_dumb_handle)
		{
			int ret = ioctl(m_drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &m_dumb_handle);
			if (ret)
				log_verbose("DRM/KMS: <%d> (set_timing) [ERROR] ioctl DRM_IOCTL_MODE_DESTROY_DUMB %d\n", m_id, ret);
			m_dumb_handle = 0;
		}
		if (m_framebuffer_id && m_framebuffer_id != mp_crtc_desktop->buffer_id)
		{
			if (drmModeRmFB(m_drm_fd, m_framebuffer_id))
				log_verbose("DRM/KMS: <%d> (set_timing) [ERROR] remove frame buffer\n", m_id);
			m_framebuffer_id = 0;
		}
	}
	else
	{
		unsigned int old_dumb_handle = m_dumb_handle;

		drmModeFB *pframebuffer = drmModeGetFB(m_drm_fd, mp_crtc_desktop->buffer_id);
		log_verbose("DRM/KMS: <%d> (set_timing) <debug> existing frame buffer id %d size %dx%d bpp %d\n", m_id, mp_crtc_desktop->buffer_id, pframebuffer->width, pframebuffer->height, pframebuffer->bpp);
		//drmModePlaneRes *pplanes = drmModeGetPlaneResources(m_drm_fd);
		//log_verbose("DRM/KMS: <%d> (add_mode) <debug> total planes %d\n", m_id, pplanes->count_planes);
		//drmModeFreePlaneResources(pplanes);

		unsigned int framebuffer_id = mp_crtc_desktop->buffer_id;

		//if (pframebuffer->width < dmode.hdisplay || pframebuffer->height < dmode.vdisplay)
		if (1)
		{
			log_verbose("DRM/KMS: <%d> (set_timing) <debug> creating new frame buffer with size %dx%d\n", m_id, dmode.hdisplay, dmode.vdisplay);

			// create a new dumb fb (not driver specefic)
			drm_mode_create_dumb create_dumb = {};
			create_dumb.width = dmode.hdisplay;
			create_dumb.height = dmode.vdisplay;
			create_dumb.bpp = pframebuffer->bpp;

			int ret = ioctl(m_drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb);
			if (ret)
				log_verbose("DRM/KMS: <%d> (set_timing) [ERROR] ioctl DRM_IOCTL_MODE_CREATE_DUMB %d\n", m_id, ret);

			if (drmModeAddFB(m_drm_fd, dmode.hdisplay, dmode.vdisplay, pframebuffer->depth, pframebuffer->bpp, create_dumb.pitch, create_dumb.handle, &framebuffer_id))
				log_error("DRM/KMS: <%d> (set_timing) [ERROR] cannot add frame buffer\n", m_id);
			else
				m_dumb_handle = create_dumb.handle;

			drm_mode_map_dumb map_dumb = {};
			map_dumb.handle = create_dumb.handle;

			ret = drmIoctl(m_drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb);
			if (ret)
				log_verbose("DRM/KMS: <%d> (set_timing) [ERROR] ioctl DRM_IOCTL_MODE_MAP_DUMB %d\n", m_id, ret);

			void *map = mmap(0, create_dumb.size, PROT_READ | PROT_WRITE, MAP_SHARED, m_drm_fd, map_dumb.offset);
			if (map != MAP_FAILED)
			{
				// clear the frame buffer
				memset(map, 0, create_dumb.size);
			}
			else
				log_verbose("DRM/KMS: <%d> (set_timing) [ERROR] failed to map frame buffer %p\n", m_id, map);
		}
		else
			log_verbose("DRM/KMS: <%d> (set_timing) <debug> use existing frame buffer\n", m_id);

		drmModeFreeFB(pframebuffer);

		pframebuffer = drmModeGetFB(m_drm_fd, framebuffer_id);
		log_verbose("DRM/KMS: <%d> (set_timing) <debug> frame buffer id %d size %dx%d bpp %d\n", m_id, framebuffer_id, pframebuffer->width, pframebuffer->height, pframebuffer->bpp);
		drmModeFreeFB(pframebuffer);

		// set the mode on the crtc
		if (drmModeSetCrtc(m_drm_fd, mp_crtc_desktop->crtc_id, framebuffer_id, 0, 0, &m_desktop_output, 1, &dmode))
			log_error("DRM/KMS: <%d> (set_timing) [ERROR] cannot attach the mode to the crtc %d frame buffer %d\n", m_id, mp_crtc_desktop->crtc_id, framebuffer_id);
		else
		{
			if (old_dumb_handle)
			{
				log_verbose("DRM/KMS: <%d> (set_timing) <debug> remove old dumb %d\n", m_id, old_dumb_handle);
				int ret = ioctl(m_drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &old_dumb_handle);
				if (ret)
					log_verbose("DRM/KMS: <%d> (set_timing) [ERROR] ioctl DRM_IOCTL_MODE_DESTROY_DUMB %d\n", m_id, ret);
				old_dumb_handle = 0;
			}
			if (m_framebuffer_id && framebuffer_id != mp_crtc_desktop->buffer_id)
			{
				log_verbose("DRM/KMS: <%d> (set_timing) <debug> remove old frame buffer %d\n", m_id, m_framebuffer_id);
				drmModeRmFB(m_drm_fd, m_framebuffer_id);
				m_framebuffer_id = 0;
			}
			m_framebuffer_id = framebuffer_id;
		}
	}
	if (can_drop_master)
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

	if (m_kernel_user_modes)
	{
		int i = 0, ret = 0, fd = -1;
		drmModeConnector *conn;

		// If SR was initilized before SDL2 for instance, SR lost the DRM
		fd = get_master_fd();
		if (fd < 0)
		{
			log_verbose("DRM/KMS: <%d> (%s) Need master to remove kernel user modes\n", m_id, __FUNCTION__);
			return false;
		}

		drmModeModeInfo srmode;
		conn = drmModeGetConnectorCurrent(fd, m_desktop_output);
		modeline_to_drm_modeline(m_id, mode, &srmode);
		for (i = 0; i < conn->count_modes; i++)
		{
			drmModeModeInfo *drmmode = &conn->modes[i];
			ret = strcmp(drmmode->name, srmode.name);
			if (ret != 0)
				continue;
			ret = drmModeDetachMode(fd, m_desktop_output, drmmode);
			if (fd != m_hook_fd)
				drmDropMaster(m_drm_fd);
			drmModeFreeConnector(conn);
			if (ret != 0)
			{
				log_verbose("DRM/KMS: <%d> (%s) Failed removing kernel user mode: %s (ret=%d)\n", m_id, __FUNCTION__, drmmode->name, ret);
				return false;
			}
			return true;
		}
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
		drmModeConnector *p_connector = drmModeGetConnectorCurrent(m_drm_fd, p_res->connectors[i]);

		// Cycle through the modelines and report them back to the display manager
		if (p_connector && m_desktop_output == p_connector->connector_id)
		{
			if (m_video_modes_position < p_connector->count_modes)
			{
				drmModeModeInfo *pdmode = &p_connector->modes[m_video_modes_position++];

				// Use mode position as index
				mode->platform_data = m_video_modes_position - 1;

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

				// Store drm's integer refresh to make sure we use the same rounding
				mode->refresh       = pdmode->vrefresh;

				mode->width         = pdmode->hdisplay;
				mode->height        = pdmode->vdisplay;

				// Add the rotation flag from the plane (DRM_MODE_ROTATE_xxx)
				// TODO: mode->type |= MODE_ROTATED;

				mode->type |= CUSTOM_VIDEO_TIMING_DRMKMS;

				// Check if this is a dummy mode
				if (pdmode->type & (1<<7)) mode->type |= XYV_EDITABLE | SCAN_EDITABLE;

				if (strncmp(pdmode->name, "SR-", 3) == 0)
					log_verbose("DRM/KMS: <%d> (get_timing) [WARNING] modeline %s detected\n", m_id, pdmode->name);
				else if (!strcmp(pdmode->name, mp_crtc_desktop->mode.name) && pdmode->clock == mp_crtc_desktop->mode.clock && pdmode->vrefresh == mp_crtc_desktop->mode.vrefresh)
				{
					// Add the desktop flag to desktop modeline
					log_verbose("DRM/KMS: <%d> (get_timing) desktop mode name %s refresh %d found\n", m_id, mp_crtc_desktop->mode.name, mp_crtc_desktop->mode.vrefresh);
					mode->type |= MODE_DESKTOP;
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

//============================================================
//  drmkms_timing::process_modelist
//============================================================

bool drmkms_timing::process_modelist(std::vector<modeline *> modelist)
{
	bool error = false;
	bool result = false;

	for (auto &mode : modelist)
	{
		if (mode->type & MODE_DELETE)
			result = delete_mode(mode);

		else if (mode->type & MODE_ADD)
			result = add_mode(mode);

		else if (mode->type & MODE_UPDATE)
			result = update_mode(mode);

		if (!result)
		{
			mode->type |= MODE_ERROR;
			error = true;
		}
		else
			// succeed
			mode->type &= ~MODE_ERROR;
	}

	return !error;
}

void drmkms_timing::list_drm_modes()
{
	int i = 0;
	drmModeConnector *conn;
	drmModeModeInfo *drmmode;

	conn = drmModeGetConnectorCurrent(m_drm_fd, m_desktop_output);
	for (i = 0; i < conn->count_modes; i++)
	{
		drmmode = &conn->modes[i];
		log_verbose("DRM/KMS: <%d> (%s) DRM mode: %dx%d %s\n", m_id, __FUNCTION__, drmmode->hdisplay, drmmode->vdisplay, drmmode->name);
	}
	drmModeFreeConnector(conn);
}

//============================================================
//  drmkms_timing::kms_has_mode
//============================================================

bool drmkms_timing::kms_has_mode(modeline* mode)
{
	int i = 0;
	drmModeConnector *conn;
	drmModeModeInfo drmmode;

	// To avoid matching issues, we just compare the relevant timing fields
	int size_to_compare = sizeof(drmModeModeInfo) - sizeof(drmModeModeInfo::type) - sizeof(drmModeModeInfo::name);

	modeline_to_drm_modeline(m_id,  mode, &drmmode);

	conn = drmModeGetConnectorCurrent(m_drm_fd, m_desktop_output);
	for (i = 0; i < conn->count_modes; i++)
	{
		if (memcmp(&drmmode, &conn->modes[i], size_to_compare) == 0)
		{
			log_verbose("DRM/KMS: <%d> (%s) Found the mode in the connector\n", m_id, __FUNCTION__);
			drmModeFreeConnector(conn);
			return true;
		}
	}
	log_verbose("DRM/KMS: <%d> (%s) Couldn't find the mode in the connector\n", m_id, __FUNCTION__);
	drmModeFreeConnector(conn);
	return false;
}
