//==================================================================================================
// PIXPlugin.h
//
// Microsoft PIX Plugin Header
//
// Copyright (c) Microsoft Corporation, All rights reserved
//==================================================================================================

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

//==================================================================================================
// PIX_PLUGIN_SYSTEM_VERSION - Indicates version of the plugin interface the plugin is built with.
//==================================================================================================
#define PIX_PLUGIN_SYSTEM_VERSION 0x101

//==================================================================================================
// PIXCOUNTERID - A unique identifier for each PIX plugin counter.
//==================================================================================================
typedef int PIXCOUNTERID;

//==================================================================================================
// PIXCOUNTERDATATYPE - Indicates what type of data the counter produces.
//==================================================================================================
enum PIXCOUNTERDATATYPE
{
    PCDT_RESERVED,
    PCDT_FLOAT,
    PCDT_INT,
    PCDT_INT64,
    PCDT_STRING,
};

//==================================================================================================
// PIXPLUGININFO - This structure is filled out by PIXGetPluginInfo and passed back to PIX.
//==================================================================================================
struct PIXPLUGININFO
{
    // Filled in by caller:
    HINSTANCE hinst;

    // Filled in by PIXGetPluginInfo:
    WCHAR* pstrPluginName;              // Name of plugin
    int iPluginVersion;                 // Version of this particular plugin
    int iPluginSystemVersion;           // Version of PIX's plugin system this plugin was designed for
};

//==================================================================================================
// PIXCOUNTERINFO - This structure is filled out by PIXGetCounterInfo and passed back to PIX
//                  to allow PIX to determine information about the counters in the plugin.
//==================================================================================================
struct PIXCOUNTERINFO
{
    PIXCOUNTERID counterID;             // Used to uniquely ID this counter
    WCHAR* pstrName;                    // String name of the counter
    PIXCOUNTERDATATYPE pcdtDataType;    // Data type returned by this counter
};

//==================================================================================================
// PIXGetPluginInfo - This returns basic information about this plugin to PIX.
//==================================================================================================
BOOL WINAPI PIXGetPluginInfo( PIXPLUGININFO* pPIXPluginInfo );

//==================================================================================================
// PIXGetCounterInfo - This returns an array of PIXCOUNTERINFO structs to PIX.
//                     These PIXCOUNTERINFOs allow PIX to enumerate the counters contained
//                     in this plugin.
//==================================================================================================
BOOL WINAPI PIXGetCounterInfo( DWORD* pdwReturnCounters, PIXCOUNTERINFO** ppCounterInfoList );

//==================================================================================================
// PIXGetCounterDesc - This is called by PIX to request a description of the indicated counter.
//==================================================================================================
BOOL WINAPI PIXGetCounterDesc( PIXCOUNTERID id, WCHAR** ppstrCounterDesc );

//==================================================================================================
// PIXBeginExperiment - This called by PIX once per counter when instrumentation starts.
//==================================================================================================
BOOL WINAPI PIXBeginExperiment( PIXCOUNTERID id, const WCHAR* pstrApplication );

//==================================================================================================
// PIXEndFrame - This is called by PIX once per counter at the end of each frame to gather the
//               counter value for that frame.  Note that the pointer to the return data must
//               continue to point to valid counter data until the next call to PIXEndFrame (or
//               PIXEndExperiment) for the same counter.  So do not set *ppReturnData to the same
//               pointer for multiple counters, or point to a local variable that will go out of
//               scope.  See the sample PIX plugin for an example of how to structure a plugin
//               properly.
//==================================================================================================
BOOL WINAPI PIXEndFrame( PIXCOUNTERID id, UINT iFrame, DWORD* pdwReturnBytes, BYTE** ppReturnData );

//==================================================================================================
// PIXEndExperiment - This is called by PIX once per counter when instrumentation ends.
//==================================================================================================
BOOL WINAPI PIXEndExperiment( PIXCOUNTERID id );

#ifdef __cplusplus
};
#endif

//==================================================================================================
// eof: PIXPlugin.h
//==================================================================================================
