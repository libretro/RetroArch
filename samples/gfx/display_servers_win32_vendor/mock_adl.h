/* ADL SDK subset, copied from gfx/display_servers/win32_modeline_adl.c so
 * the mock dll and the test agree with the backend on every layout. Keep
 * in step with that file. */
#ifndef MOCK_ADL_H
#define MOCK_ADL_H

#include <windows.h>

#define ADL_MAX_PATH   256
#define ADL_OK           0
#define ADL_ERR         -1

#define ADL_DL_TIMINGFLAG_DOUBLE_SCAN               0x0001
#define ADL_DL_TIMINGFLAG_INTERLACED                0x0002
#define ADL_DL_TIMINGFLAG_H_SYNC_POLARITY           0x0004
#define ADL_DL_TIMINGFLAG_V_SYNC_POLARITY           0x0008

#define ADL_DL_MODETIMING_STANDARD_CVT              0x00000001
#define ADL_DL_MODETIMING_STANDARD_GTF              0x00000002
#define ADL_DL_MODETIMING_STANDARD_DMT              0x00000004
#define ADL_DL_MODETIMING_STANDARD_CUSTOM           0x00000008
#define ADL_DL_MODETIMING_STANDARD_DRIVER_DEFAULT   0x00000010
#define ADL_DL_MODETIMING_STANDARD_CVT_RB           0x00000020

/* Timing commands */
#define TIMING_DELETE      0x001
#define TIMING_CREATE      0x002
#define TIMING_UPDATE      0x004
#define TIMING_UPDATE_LIST 0x008

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
} ADLDisplayID;

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
} ADLDisplayInfo;

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
   ADLDisplayInfo *m_display_list;
   int m_index;
   int m_bus;
   int m_num_of_displays;
   char m_name[ADL_MAX_PATH];
   char m_display_name[ADL_MAX_PATH];
} AdapterList;

typedef void *ADL_CONTEXT_HANDLE;
typedef void *(__stdcall *ADL_MAIN_MALLOC_CALLBACK)(int);
typedef int (*ADL2_MAIN_CONTROL_CREATE)(ADL_MAIN_MALLOC_CALLBACK, int, ADL_CONTEXT_HANDLE *);
typedef int (*ADL2_MAIN_CONTROL_DESTROY)(ADL_CONTEXT_HANDLE);
typedef int (*ADL2_ADAPTER_NUMBEROFADAPTERS_GET)(ADL_CONTEXT_HANDLE, int*);
typedef int (*ADL2_ADAPTER_ADAPTERINFO_GET)(ADL_CONTEXT_HANDLE, LPAdapterInfo, int);
typedef int (*ADL2_DISPLAY_DISPLAYINFO_GET)(ADL_CONTEXT_HANDLE, int, int *, ADLDisplayInfo **, int);
typedef int (*ADL2_DISPLAY_MODETIMINGOVERRIDE_GET)(ADL_CONTEXT_HANDLE, int, int, ADLDisplayMode *, ADLDisplayModeInfo *);
typedef int (*ADL2_DISPLAY_MODETIMINGOVERRIDE_SET)(ADL_CONTEXT_HANDLE, int, int, ADLDisplayModeInfo *, int);
typedef int (*ADL2_DISPLAY_MODETIMINGOVERRIDELIST_GET)(ADL_CONTEXT_HANDLE, int, int, int, ADLDisplayModeInfo *, int *);
typedef int (*ADL2_FLUSH_DRIVER_DATA)(ADL_CONTEXT_HANDLE, int);


#endif
