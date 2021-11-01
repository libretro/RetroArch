/**************************************************************

    custom_video_adl.cpp - ATI/AMD ADL library

    ---------------------------------------------------------

    Switchres   Modeline generation engine for emulation

    License     GPL-2.0+
    Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                          Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

//  Constants and structures ported from AMD ADL SDK files

#include <windows.h>
#include <stdio.h>
#include "custom_video_adl.h"
#include "log.h"


//============================================================
//  memory allocation callbacks
//============================================================

void* __stdcall ADL_Main_Memory_Alloc(int iSize)
{
	void* lpBuffer = malloc(iSize);
	return lpBuffer;
}

void __stdcall ADL_Main_Memory_Free(void** lpBuffer)
{
	if (NULL != *lpBuffer)
	{
		free(*lpBuffer);
		*lpBuffer = NULL;
	}
}

//============================================================
//  adl_timing::adl_timing
//============================================================

adl_timing::adl_timing(char *display_name, custom_video_settings *vs)
{
	m_vs = *vs;
	strcpy (m_display_name, display_name);
	strcpy (m_device_key, m_vs.device_reg_key);
}

//============================================================
//  adl_timing::~adl_timing
//============================================================

adl_timing::~adl_timing()
{
	close();
}

//============================================================
//  adl_timing::init
//============================================================

bool adl_timing::init()
{
	int ADL_Err = ADL_ERR;

	log_verbose("ATI/AMD ADL init\n");

	ADL_Err = open();
	if (ADL_Err != ADL_OK)
	{
		log_verbose("ERROR: ADL Initialization error!\n");
		return false;
	}

	ADL2_Adapter_NumberOfAdapters_Get = (ADL2_ADAPTER_NUMBEROFADAPTERS_GET) (void *) GetProcAddress(hDLL,"ADL2_Adapter_NumberOfAdapters_Get");
	if (ADL2_Adapter_NumberOfAdapters_Get == NULL)
	{
		log_verbose("ERROR: ADL2_Adapter_NumberOfAdapters_Get not available!");
		return false;
	}
	ADL2_Adapter_AdapterInfo_Get = (ADL2_ADAPTER_ADAPTERINFO_GET) (void *) GetProcAddress(hDLL,"ADL2_Adapter_AdapterInfo_Get");
	if (ADL2_Adapter_AdapterInfo_Get == NULL)
	{
		log_verbose("ERROR: ADL2_Adapter_AdapterInfo_Get not available!");
		return false;
	}
	ADL2_Display_DisplayInfo_Get = (ADL2_DISPLAY_DISPLAYINFO_GET) (void *) GetProcAddress(hDLL,"ADL2_Display_DisplayInfo_Get");
	if (ADL2_Display_DisplayInfo_Get == NULL)
	{
		log_verbose("ERROR: ADL2_Display_DisplayInfo_Get not available!");
		return false;
	}
	ADL2_Display_ModeTimingOverride_Get = (ADL2_DISPLAY_MODETIMINGOVERRIDE_GET) (void *) GetProcAddress(hDLL,"ADL2_Display_ModeTimingOverride_Get");
	if (ADL2_Display_ModeTimingOverride_Get == NULL)
	{
		log_verbose("ERROR: ADL2_Display_ModeTimingOverride_Get not available!");
		return false;
	}
	ADL2_Display_ModeTimingOverride_Set = (ADL2_DISPLAY_MODETIMINGOVERRIDE_SET) (void *) GetProcAddress(hDLL,"ADL2_Display_ModeTimingOverride_Set");
	if (ADL2_Display_ModeTimingOverride_Set == NULL)
	{
		log_verbose("ERROR: ADL2_Display_ModeTimingOverride_Set not available!");
		return false;
	}
	ADL2_Display_ModeTimingOverrideList_Get = (ADL2_DISPLAY_MODETIMINGOVERRIDELIST_GET) (void *) GetProcAddress(hDLL,"ADL2_Display_ModeTimingOverrideList_Get");
	if (ADL2_Display_ModeTimingOverrideList_Get == NULL)
	{
		log_verbose("ERROR: ADL2_Display_ModeTimingOverrideList_Get not available!");
		return false;
	}

	ADL2_Flush_Driver_Data = (ADL2_FLUSH_DRIVER_DATA) (void *) GetProcAddress(hDLL,"ADL2_Flush_Driver_Data");
	if (ADL2_Flush_Driver_Data == NULL)
	{
		log_verbose("ERROR: ADL2_Flush_Driver_Data not available!");
		return false;
	}

	if (!enum_displays())
	{
		log_error("ADL error enumerating displays.\n");
		return false;
	}

	if (!get_device_mapping_from_display_name())
	{
		log_error("ADL error mapping display.\n");
		return false;
	}

	if (!get_driver_version(m_device_key))
	{
		log_error("ADL driver version unknown!.\n");
	}

	if (!get_timing_list())
	{
		log_error("ADL error getting list of timing overrides.\n");
	}

	log_verbose("ADL functions retrieved successfully.\n");
	return true;
}

//============================================================
//  adl_timing::adl_open
//============================================================

int adl_timing::open()
{
	ADL2_MAIN_CONTROL_CREATE ADL2_Main_Control_Create;
	int ADL_Err = ADL_ERR;

	hDLL = LoadLibraryA("atiadlxx.dll");
	if (hDLL == NULL) hDLL = LoadLibraryA("atiadlxy.dll");

	if (hDLL != NULL)
	{
		ADL2_Main_Control_Create = (ADL2_MAIN_CONTROL_CREATE) (void *) GetProcAddress(hDLL, "ADL2_Main_Control_Create");
		if (ADL2_Main_Control_Create != NULL)
				ADL_Err = ADL2_Main_Control_Create(ADL_Main_Memory_Alloc, 1, &m_adl);
	}
	else
	{
		log_verbose("ADL Library not found!\n");
	}

	return ADL_Err;
}

//============================================================
//  adl_timing::close
//============================================================

void adl_timing::close()
{
	ADL2_MAIN_CONTROL_DESTROY ADL2_Main_Control_Destroy;

	log_verbose("ATI/AMD ADL close\n");

	for (int i = 0; i <= iNumberAdapters - 1; i++)
		ADL_Main_Memory_Free((void **)&lpAdapter[i].m_display_list);

	ADL_Main_Memory_Free((void **)&lpAdapterInfo);
	ADL_Main_Memory_Free((void **)&lpAdapter);

	ADL2_Main_Control_Destroy = (ADL2_MAIN_CONTROL_DESTROY) (void *) GetProcAddress(hDLL, "ADL2_Main_Control_Destroy");
	if (ADL2_Main_Control_Destroy != NULL)
		ADL2_Main_Control_Destroy(m_adl);

	FreeLibrary(hDLL);
}

//============================================================
//  adl_timing::get_driver_version
//============================================================

bool adl_timing::get_driver_version(char *device_key)
{
	HKEY hkey;
	bool found = false;

	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, device_key, 0, KEY_READ , &hkey) == ERROR_SUCCESS)
	{
		BYTE cat_ver[32];
		DWORD length = sizeof(cat_ver);
		if ((RegQueryValueExA(hkey, "Catalyst_Version", NULL, NULL, cat_ver, &length) == ERROR_SUCCESS) ||
			(RegQueryValueExA(hkey, "RadeonSoftwareVersion", NULL, NULL, cat_ver, &length) == ERROR_SUCCESS) ||
			(RegQueryValueExA(hkey, "DriverVersion", NULL, NULL, cat_ver, &length) == ERROR_SUCCESS))
		{
			found = true;
			is_patched = (RegQueryValueExA(hkey, "CalamityRelease", NULL, NULL, NULL, NULL) == ERROR_SUCCESS);
			sscanf((char *)cat_ver, "%d.%d", &cat_version, &sub_version);
			log_verbose("AMD driver version %d.%d%s\n", cat_version, sub_version, is_patched? "(patched)":"");
		}
		RegCloseKey(hkey);
	}
	return found;
}

//============================================================
//  adl_timing::enum_displays
//============================================================

bool adl_timing::enum_displays()
{
	ADL2_Adapter_NumberOfAdapters_Get(m_adl, &iNumberAdapters);

	lpAdapterInfo = (LPAdapterInfo)malloc(sizeof(AdapterInfo) * iNumberAdapters);
	memset(lpAdapterInfo, '\0', sizeof(AdapterInfo) * iNumberAdapters);
	ADL2_Adapter_AdapterInfo_Get(m_adl, lpAdapterInfo, sizeof(AdapterInfo) * iNumberAdapters);

	lpAdapter = (LPAdapterList)malloc(sizeof(AdapterList) * iNumberAdapters);
	for (int i = 0; i <= iNumberAdapters - 1; i++)
	{
		lpAdapter[i].m_index = lpAdapterInfo[i].iAdapterIndex;
		lpAdapter[i].m_bus   = lpAdapterInfo[i].iBusNumber;
		memcpy(&lpAdapter[i].m_name, &lpAdapterInfo[i].strAdapterName, ADL_MAX_PATH);
		memcpy(&lpAdapter[i].m_display_name, &lpAdapterInfo[i].strDisplayName, ADL_MAX_PATH);
		lpAdapter[i].m_num_of_displays = 0;
		lpAdapter[i].m_display_list = 0;

		// Only get display info from target adapter (this api is very slow!)
		if (!strcmp(lpAdapter[i].m_display_name, m_display_name))
			ADL2_Display_DisplayInfo_Get(m_adl, lpAdapter[i].m_index, &lpAdapter[i].m_num_of_displays, &lpAdapter[i].m_display_list, 1);
	}
	return true;
}

//============================================================
//  adl_timing::get_device_mapping_from_display_name
//============================================================

bool adl_timing::get_device_mapping_from_display_name()
{
	for (int i = 0; i <= iNumberAdapters -1; i++)
	{
		if (!strcmp(m_display_name, lpAdapter[i].m_display_name))
		{
			ADLDisplayInfo *display_list;
			display_list = lpAdapter[i].m_display_list;

			for (int j = 0; j <= lpAdapter[i].m_num_of_displays - 1; j++)
			{
				if (lpAdapter[i].m_index == display_list[j].displayID.iDisplayLogicalAdapterIndex)
				{
					m_adapter_index = lpAdapter[i].m_index;
					m_display_index = display_list[j].displayID.iDisplayLogicalIndex;
					return true;
				}
			}
		}
	}
	return false;
}

//============================================================
//  adl_timing::display_mode_info_to_modeline
//============================================================

bool adl_timing::display_mode_info_to_modeline(ADLDisplayModeInfo *dmi, modeline *m)
{
	if (dmi->sDetailedTiming.sHTotal == 0) return false;

	ADLDetailedTiming dt;
	memcpy(&dt, &dmi->sDetailedTiming, sizeof(ADLDetailedTiming));

	if (dt.sHTotal == 0) return false;

	m->htotal    = dt.sHTotal;
	m->hactive   = dt.sHDisplay;
	m->hbegin    = dt.sHSyncStart;
	m->hend      = dt.sHSyncWidth + m->hbegin;
	m->vtotal    = dt.sVTotal;
	m->vactive   = dt.sVDisplay;
	m->vbegin    = dt.sVSyncStart;
	m->vend      = dt.sVSyncWidth + m->vbegin;
	m->interlace = (dt.sTimingFlags & ADL_DL_TIMINGFLAG_INTERLACED)? 1 : 0;
	m->doublescan = (dt.sTimingFlags & ADL_DL_TIMINGFLAG_DOUBLE_SCAN)? 1 : 0;
	m->hsync     = ((dt.sTimingFlags & ADL_DL_TIMINGFLAG_H_SYNC_POLARITY)? 1 : 0) ^ invert_pol(1);
	m->vsync     = ((dt.sTimingFlags & ADL_DL_TIMINGFLAG_V_SYNC_POLARITY)? 1 : 0) ^ invert_pol(1) ;
	m->pclock    = dt.sPixelClock * 10000;

	m->height  = m->height? m->height : dmi->iPelsHeight;
	m->width   = m->width? m->width : dmi->iPelsWidth;
	m->refresh = m->refresh? m->refresh : dmi->iRefreshRate / interlace_factor(m->interlace, 1);;
	m->hfreq = float(m->pclock / m->htotal);
	m->vfreq = float(m->hfreq / m->vtotal) * (m->interlace? 2 : 1);

	return true;
}

//============================================================
//  adl_timing::get_timing_list
//============================================================

bool adl_timing::get_timing_list()
{
	if (ADL2_Display_ModeTimingOverrideList_Get(m_adl, m_adapter_index, m_display_index, MAX_MODELINES, adl_mode, &m_num_of_adl_modes) != ADL_OK) return false;

	return true;
}

//============================================================
//  adl_timing::get_timing_from_cache
//============================================================

bool adl_timing::get_timing_from_cache(modeline *m)
{
	ADLDisplayModeInfo *mode = 0;

	for (int i = 0; i < m_num_of_adl_modes; i++)
	{
		mode = &adl_mode[i];
		if (mode->iPelsWidth == m->width && mode->iPelsHeight == m->height && mode->iRefreshRate == m->refresh)
		{
			if ((m->interlace) && !(mode->sDetailedTiming.sTimingFlags & ADL_DL_TIMINGFLAG_INTERLACED))
				continue;
			goto found;
		}
	}

	return false;

	found:
	if (display_mode_info_to_modeline(mode, m)) return true;

	return false;
}

//============================================================
//  adl_timing::get_timing
//============================================================

bool adl_timing::get_timing(modeline *m)
{
	ADLDisplayMode mode_in;
	ADLDisplayModeInfo mode_info_out;
	modeline m_temp = *m;

	//modeline to ADLDisplayMode
	mode_in.iPelsHeight       = m->height;
	mode_in.iPelsWidth        = m->width;
	mode_in.iBitsPerPel       = 32;
	mode_in.iDisplayFrequency = m->refresh * interlace_factor(m->interlace, 1);

	if (ADL2_Display_ModeTimingOverride_Get(m_adl, m_adapter_index, m_display_index, &mode_in, &mode_info_out) != ADL_OK) goto not_found;
	if (display_mode_info_to_modeline(&mode_info_out, &m_temp))
	{
		if (m_temp.interlace == m->interlace)
		{
			memcpy(m, &m_temp, sizeof(modeline));
			m->type |= CUSTOM_VIDEO_TIMING_ATI_ADL;
			return true;
		}
	}

	not_found:

	// Try to get timing from our cache (interlaced modes are not properly retrieved by ADL_Display_ModeTimingOverride_Get)
	if (get_timing_from_cache(m))
	{
		m->type |= CUSTOM_VIDEO_TIMING_ATI_ADL;
		return true;
	}

	return false;
}

//============================================================
//  adl_timing::set_timing
//============================================================

bool adl_timing::set_timing(modeline *m)
{
	return set_timing_override(m, TIMING_UPDATE);
}

//============================================================
//  adl_timing::set_timing_override
//============================================================

bool adl_timing::set_timing_override(modeline *m, int update_mode)
{
	ADLDisplayModeInfo mode_info = {};
	ADLDetailedTiming *dt;
	modeline m_temp;

	//modeline to ADLDisplayModeInfo
	mode_info.iTimingStandard   = (update_mode & TIMING_DELETE)? ADL_DL_MODETIMING_STANDARD_DRIVER_DEFAULT : ADL_DL_MODETIMING_STANDARD_CUSTOM;
	mode_info.iPossibleStandard = 0;
	mode_info.iRefreshRate      = m->refresh * interlace_factor(m->interlace, 0);
	mode_info.iPelsWidth        = m->width;
	mode_info.iPelsHeight       = m->height;

	//modeline to ADLDetailedTiming
	dt = &mode_info.sDetailedTiming;
	dt->sTimingFlags     = (m->interlace? ADL_DL_TIMINGFLAG_INTERLACED : 0) |
						   (m->doublescan? ADL_DL_TIMINGFLAG_DOUBLE_SCAN: 0) |
						   (m->hsync ^ invert_pol(0)? ADL_DL_TIMINGFLAG_H_SYNC_POLARITY : 0) |
						   (m->vsync ^ invert_pol(0)? ADL_DL_TIMINGFLAG_V_SYNC_POLARITY : 0);
	dt->sHTotal          = m->htotal;
	dt->sHDisplay        = m->hactive;
	dt->sHSyncStart      = m->hbegin;
	dt->sHSyncWidth      = m->hend - m->hbegin;
	dt->sVTotal          = m->vtotal;
	dt->sVDisplay        = m->vactive;
	dt->sVSyncStart      = m->vbegin;
	dt->sVSyncWidth      = m->vend - m->vbegin;
	dt->sPixelClock      = m->pclock / 10000;
	dt->sHOverscanRight  = 0;
	dt->sHOverscanLeft   = 0;
	dt->sVOverscanBottom = 0;
	dt->sVOverscanTop    = 0;

	if (ADL2_Display_ModeTimingOverride_Set(m_adl, m_adapter_index, m_display_index, &mode_info, (update_mode & TIMING_UPDATE_LIST)? 1 : 0) != ADL_OK) return false;

	//ADL2_Flush_Driver_Data(m_adl, m_adapter_index);

	// read modeline to trigger timing refresh on modded drivers
	memcpy(&m_temp, m, sizeof(modeline));
	if (update_mode & TIMING_UPDATE) get_timing(&m_temp);

	return true;
}

//============================================================
//  adl_timing::add_mode
//============================================================

bool adl_timing::add_mode(modeline *mode)
{
	if (!set_timing_override(mode, TIMING_UPDATE_LIST))
	{
		return false;
	}

	m_resync.wait();
	mode->type |= CUSTOM_VIDEO_TIMING_ATI_ADL;

	return true;
}

//============================================================
//  adl_timing::delete_mode
//============================================================

bool adl_timing::delete_mode(modeline *mode)
{
	if (!set_timing_override(mode, TIMING_DELETE | TIMING_UPDATE_LIST))
	{
		return false;
	}

	m_resync.wait();

	return true;
}

//============================================================
//  adl_timing::update_mode
//============================================================

bool adl_timing::update_mode(modeline *mode)
{
	bool refresh_required = !is_patched || (mode->type & MODE_DESKTOP);

	if (!set_timing_override(mode, refresh_required? TIMING_UPDATE_LIST : TIMING_UPDATE))
	{
		return false;
	}

	if (refresh_required) m_resync.wait();
	mode->type |= CUSTOM_VIDEO_TIMING_ATI_ADL;
	return true;
}

//============================================================
//  adl_timing::process_modelist
//============================================================

bool adl_timing::process_modelist(std::vector<modeline *> modelist)
{
	bool refresh_required = false;
	bool error = false;

	for (auto &mode : modelist)
	{
		if (mode->type & MODE_DELETE || mode->type & MODE_ADD || (mode->type & MODE_UPDATE && (!is_patched || (mode->type & MODE_DESKTOP))))
			refresh_required = true;

		bool is_last = (mode == modelist.back());

		if (!set_timing_override(mode, (mode->type & MODE_DELETE? TIMING_DELETE : TIMING_UPDATE) | (is_last && refresh_required? TIMING_UPDATE_LIST : 0)))
		{
			mode->type |= MODE_ERROR;
			error = true;
		}
		else
		{
			mode->type &= ~MODE_ERROR;
			mode->type |= CUSTOM_VIDEO_TIMING_ATI_ADL;
		}
	}

	if (refresh_required) m_resync.wait();
	return !error;
}
