/**************************************************************

    custom_video_adl.h - ATI/AMD ADL library header

    ---------------------------------------------------------

    Switchres   Modeline generation engine for emulation

    License     GPL-2.0+
    Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                          Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#include <windows.h>
#include "custom_video.h"
#include "resync_windows.h"

//  Constants and structures ported from AMD ADL SDK files
#define ADL_MAX_PATH   256
#define ADL_OK           0
#define ADL_ERR         -1

//ADL_DETAILED_TIMING.sTimingFlags
#define ADL_DL_TIMINGFLAG_DOUBLE_SCAN               0x0001
#define ADL_DL_TIMINGFLAG_INTERLACED                0x0002
#define ADL_DL_TIMINGFLAG_H_SYNC_POLARITY           0x0004
#define ADL_DL_TIMINGFLAG_V_SYNC_POLARITY           0x0008

//ADL_DISPLAY_MODE_INFO.iTimingStandard
#define ADL_DL_MODETIMING_STANDARD_CVT              0x00000001 // CVT Standard
#define ADL_DL_MODETIMING_STANDARD_GTF              0x00000002 // GFT Standard
#define ADL_DL_MODETIMING_STANDARD_DMT              0x00000004 // DMT Standard
#define ADL_DL_MODETIMING_STANDARD_CUSTOM           0x00000008 // User-defined standard
#define ADL_DL_MODETIMING_STANDARD_DRIVER_DEFAULT   0x00000010 // Remove Mode from overriden list
#define ADL_DL_MODETIMING_STANDARD_CVT_RB           0x00000020 // CVT-RB Standard

typedef struct AdapterInfo
{
	int iSize;
	int iAdapterIndex;
	char strUDID[ADL_MAX_PATH];
	int iBusNumber;
	int iDeviceNumber;
	int iFunctionNumber;
	int iVendorID;
	char strAdapterName[ADL_MAX_PATH];
	char strDisplayName[ADL_MAX_PATH];
	int iPresent;
	int iExist;
	char strDriverPath[ADL_MAX_PATH];
	char strDriverPathExt[ADL_MAX_PATH];
	char strPNPString[ADL_MAX_PATH];
	int iOSDisplayIndex;
} AdapterInfo, *LPAdapterInfo;

typedef struct ADLDisplayID
{
	int iDisplayLogicalIndex;
	int iDisplayPhysicalIndex;
	int iDisplayLogicalAdapterIndex;
	int iDisplayPhysicalAdapterIndex;
} ADLDisplayID, *LPADLDisplayID;


typedef struct ADLDisplayInfo
{
	ADLDisplayID displayID;
	int iDisplayControllerIndex;
	char strDisplayName[ADL_MAX_PATH];
	char strDisplayManufacturerName[ADL_MAX_PATH];
	int iDisplayType;
	int iDisplayOutputType;
	int iDisplayConnector;
	int iDisplayInfoMask;
	int iDisplayInfoValue;
} ADLDisplayInfo, *LPADLDisplayInfo;

typedef struct ADLDisplayMode
{
	int iPelsHeight;
	int iPelsWidth;
	int iBitsPerPel;
	int iDisplayFrequency;
} ADLDisplayMode;

typedef struct ADLDetailedTiming
{
	int   iSize;
	short sTimingFlags;
	short sHTotal;
	short sHDisplay;
	short sHSyncStart;
	short sHSyncWidth;
	short sVTotal;
	short sVDisplay;
	short sVSyncStart;
	short sVSyncWidth;
	unsigned short sPixelClock;
	short sHOverscanRight;
	short sHOverscanLeft;
	short sVOverscanBottom;
	short sVOverscanTop;
	short sOverscan8B;
	short sOverscanGR;
} ADLDetailedTiming;

typedef struct ADLDisplayModeInfo
{
	int iTimingStandard;
	int iPossibleStandard;
	int iRefreshRate;
	int iPelsWidth;
	int iPelsHeight;
	ADLDetailedTiming sDetailedTiming;
} ADLDisplayModeInfo;

typedef struct AdapterList
{
	int m_index;
	int m_bus;
	char m_name[ADL_MAX_PATH];
	char m_display_name[ADL_MAX_PATH];
	int m_num_of_displays;
	ADLDisplayInfo *m_display_list;
} AdapterList, *LPAdapterList;


typedef void* ADL_CONTEXT_HANDLE;
typedef void* (__stdcall *ADL_MAIN_MALLOC_CALLBACK)(int);
typedef int (*ADL2_MAIN_CONTROL_CREATE)(ADL_MAIN_MALLOC_CALLBACK, int,  ADL_CONTEXT_HANDLE *);
typedef int (*ADL2_MAIN_CONTROL_DESTROY)(ADL_CONTEXT_HANDLE);
typedef int (*ADL2_ADAPTER_NUMBEROFADAPTERS_GET) (ADL_CONTEXT_HANDLE, int*);
typedef int (*ADL2_ADAPTER_ADAPTERINFO_GET) (ADL_CONTEXT_HANDLE, LPAdapterInfo, int);
typedef int (*ADL2_DISPLAY_DISPLAYINFO_GET) (ADL_CONTEXT_HANDLE, int, int *, ADLDisplayInfo **, int);
typedef int (*ADL2_DISPLAY_MODETIMINGOVERRIDE_GET) (ADL_CONTEXT_HANDLE, int iAdapterIndex, int iDisplayIndex, ADLDisplayMode *lpModeIn, ADLDisplayModeInfo *lpModeInfoOut);
typedef int (*ADL2_DISPLAY_MODETIMINGOVERRIDE_SET) (ADL_CONTEXT_HANDLE, int iAdapterIndex, int iDisplayIndex, ADLDisplayModeInfo *lpMode, int iForceUpdate);
typedef int (*ADL2_DISPLAY_MODETIMINGOVERRIDELIST_GET) (ADL_CONTEXT_HANDLE, int iAdapterIndex, int iDisplayIndex, int iMaxNumOfOverrides, ADLDisplayModeInfo *lpModeInfoList, int *lpNumOfOverrides);
typedef int (*ADL2_FLUSH_DRIVER_DATA) (ADL_CONTEXT_HANDLE, int iAdapterIndex);


class adl_timing : public custom_video
{
	public:
		adl_timing(char *display_name, custom_video_settings *vs);
		~adl_timing();
		const char *api_name() { return "AMD ADL"; }
		bool init();
		void close();
		int caps() { return allow_hardware_refresh()? CUSTOM_VIDEO_CAPS_UPDATE | CUSTOM_VIDEO_CAPS_ADD | CUSTOM_VIDEO_CAPS_DESKTOP_EDITABLE : is_patched? CUSTOM_VIDEO_CAPS_UPDATE : 0; }

		bool add_mode(modeline *mode);
		bool delete_mode(modeline *mode);
		bool update_mode(modeline *mode);

		bool get_timing(modeline *m);
		bool set_timing(modeline *m);

		bool process_modelist(std::vector<modeline *>);

	private:
		int open();
		bool get_driver_version(char *device_key);
		bool enum_displays();
		bool get_device_mapping_from_display_name();
		bool display_mode_info_to_modeline(ADLDisplayModeInfo *dmi, modeline *m);
		bool get_timing_list();
		bool get_timing_from_cache(modeline *m);
		bool set_timing_override(modeline *m, int update_mode);

		char m_display_name[32];
		char m_device_key[128];

		int m_adapter_index = 0;
		int m_display_index = 0;

		ADL2_ADAPTER_NUMBEROFADAPTERS_GET        ADL2_Adapter_NumberOfAdapters_Get;
		ADL2_ADAPTER_ADAPTERINFO_GET             ADL2_Adapter_AdapterInfo_Get;
		ADL2_DISPLAY_DISPLAYINFO_GET             ADL2_Display_DisplayInfo_Get;
		ADL2_DISPLAY_MODETIMINGOVERRIDE_GET      ADL2_Display_ModeTimingOverride_Get;
		ADL2_DISPLAY_MODETIMINGOVERRIDE_SET      ADL2_Display_ModeTimingOverride_Set;
		ADL2_DISPLAY_MODETIMINGOVERRIDELIST_GET  ADL2_Display_ModeTimingOverrideList_Get;
		ADL2_FLUSH_DRIVER_DATA                   ADL2_Flush_Driver_Data;

		HINSTANCE hDLL;
		LPAdapterInfo lpAdapterInfo = NULL;
		LPAdapterList lpAdapter = NULL;;
		int iNumberAdapters = 0;
		int cat_version = 0;
		int sub_version = 0;
		bool is_patched = false;

		ADL_CONTEXT_HANDLE m_adl = 0;
		ADLDisplayModeInfo adl_mode[MAX_MODELINES];
		int m_num_of_adl_modes = 0;

		resync_handler m_resync;

		int invert_pol(bool on_read) { return ((cat_version <= 12) || (cat_version >= 15 && on_read)); }
		int interlace_factor(bool interlace, bool on_read) { return interlace && ((cat_version <= 12) || (cat_version >= 15 && on_read))? 2 : 1; }
};
