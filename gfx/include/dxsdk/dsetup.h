/*==========================================================================
 *
 *  Copyright (C) 1995-1997 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       dsetup.h
 *  Content:    DirectXSetup, error codes and flags
 ***************************************************************************/

#ifndef __DSETUP_H__
#define __DSETUP_H__

#include <windows.h>      // windows stuff

#ifdef __cplusplus
extern "C" {
#endif

#define FOURCC_VERS mmioFOURCC('v','e','r','s')

// DSETUP Error Codes, must remain compatible with previous setup.
#define DSETUPERR_SUCCESS_RESTART        1
#define DSETUPERR_SUCCESS                0
#define DSETUPERR_BADWINDOWSVERSION     -1
#define DSETUPERR_SOURCEFILENOTFOUND    -2
#define DSETUPERR_NOCOPY                -5
#define DSETUPERR_OUTOFDISKSPACE        -6
#define DSETUPERR_CANTFINDINF           -7
#define DSETUPERR_CANTFINDDIR           -8
#define DSETUPERR_INTERNAL              -9
#define DSETUPERR_UNKNOWNOS             -11
#define DSETUPERR_NEWERVERSION          -14
#define DSETUPERR_NOTADMIN              -15
#define DSETUPERR_UNSUPPORTEDPROCESSOR  -16
#define DSETUPERR_MISSINGCAB_MANAGEDDX  -17
#define DSETUPERR_NODOTNETFRAMEWORKINSTALLED -18
#define DSETUPERR_CABDOWNLOADFAIL       -19
#define DSETUPERR_DXCOMPONENTFILEINUSE  -20
#define DSETUPERR_UNTRUSTEDCABINETFILE  -21

// DSETUP flags. DirectX 5.0 apps should use these flags only.
#define DSETUP_DDRAWDRV         0x00000008      /* install DirectDraw Drivers           */
#define DSETUP_DSOUNDDRV        0x00000010      /* install DirectSound Drivers          */
#define DSETUP_DXCORE           0x00010000      /* install DirectX runtime              */
#define DSETUP_DIRECTX  (DSETUP_DXCORE|DSETUP_DDRAWDRV|DSETUP_DSOUNDDRV)
#define DSETUP_MANAGEDDX        0x00004000      /* OBSOLETE. install managed DirectX    */
#define DSETUP_TESTINSTALL      0x00020000      /* just test install, don't do anything */

// These OBSOLETE flags are here for compatibility with pre-DX5 apps only.
// They are present to allow DX3 apps to be recompiled with DX5 and still work.
// DO NOT USE THEM for DX5. They will go away in future DX releases.
#define DSETUP_DDRAW            0x00000001      /* OBSOLETE. install DirectDraw           */
#define DSETUP_DSOUND           0x00000002      /* OBSOLETE. install DirectSound          */
#define DSETUP_DPLAY            0x00000004      /* OBSOLETE. install DirectPlay           */
#define DSETUP_DPLAYSP          0x00000020      /* OBSOLETE. install DirectPlay Providers */
#define DSETUP_DVIDEO           0x00000040      /* OBSOLETE. install DirectVideo          */
#define DSETUP_D3D              0x00000200      /* OBSOLETE. install Direct3D             */
#define DSETUP_DINPUT           0x00000800      /* OBSOLETE. install DirectInput          */
#define DSETUP_DIRECTXSETUP     0x00001000      /* OBSOLETE. install DirectXSetup DLL's   */
#define DSETUP_NOUI             0x00002000      /* OBSOLETE. install DirectX with NO UI   */
#define DSETUP_PROMPTFORDRIVERS 0x10000000      /* OBSOLETE. prompt when replacing display/audio drivers */
#define DSETUP_RESTOREDRIVERS   0x20000000      /* OBSOLETE. restore display/audio drivers */



//******************************************************************
// DirectX Setup Callback mechanism
//******************************************************************

// DSETUP Message Info Codes, passed to callback as Reason parameter.
#define DSETUP_CB_MSG_NOMESSAGE                     0
#define DSETUP_CB_MSG_INTERNAL_ERROR                10
#define DSETUP_CB_MSG_BEGIN_INSTALL                 13
#define DSETUP_CB_MSG_BEGIN_INSTALL_RUNTIME         14
#define DSETUP_CB_MSG_PROGRESS                      18
#define DSETUP_CB_MSG_WARNING_DISABLED_COMPONENT    19






typedef struct _DSETUP_CB_PROGRESS
{
    DWORD dwPhase;
    DWORD dwInPhaseMaximum;
    DWORD dwInPhaseProgress;
    DWORD dwOverallMaximum;
    DWORD dwOverallProgress;
} DSETUP_CB_PROGRESS;

 
enum _DSETUP_CB_PROGRESS_PHASE
{
    DSETUP_INITIALIZING,
    DSETUP_EXTRACTING,
    DSETUP_COPYING,
    DSETUP_FINALIZING
};


#ifdef _WIN32
//
// Data Structures
//
#ifndef UNICODE_ONLY

typedef struct _DIRECTXREGISTERAPPA {
    DWORD    dwSize;
    DWORD    dwFlags;
    LPSTR    lpszApplicationName;
    LPGUID   lpGUID;
    LPSTR    lpszFilename;
    LPSTR    lpszCommandLine;
    LPSTR    lpszPath;
    LPSTR    lpszCurrentDirectory;
} DIRECTXREGISTERAPPA, *PDIRECTXREGISTERAPPA, *LPDIRECTXREGISTERAPPA;

typedef struct _DIRECTXREGISTERAPP2A {
    DWORD    dwSize;
    DWORD    dwFlags;
    LPSTR    lpszApplicationName;
    LPGUID   lpGUID;
    LPSTR    lpszFilename;
    LPSTR    lpszCommandLine;
    LPSTR    lpszPath;
    LPSTR    lpszCurrentDirectory;
    LPSTR    lpszLauncherName;
} DIRECTXREGISTERAPP2A, *PDIRECTXREGISTERAPP2A, *LPDIRECTXREGISTERAPP2A;

#endif //!UNICODE_ONLY
#ifndef ANSI_ONLY

typedef struct _DIRECTXREGISTERAPPW {
    DWORD    dwSize;
    DWORD    dwFlags;
    LPWSTR   lpszApplicationName;
    LPGUID   lpGUID;
    LPWSTR   lpszFilename;
    LPWSTR   lpszCommandLine;
    LPWSTR   lpszPath;
    LPWSTR   lpszCurrentDirectory;
} DIRECTXREGISTERAPPW, *PDIRECTXREGISTERAPPW, *LPDIRECTXREGISTERAPPW;

typedef struct _DIRECTXREGISTERAPP2W {
    DWORD    dwSize;
    DWORD    dwFlags;
    LPWSTR   lpszApplicationName;
    LPGUID   lpGUID;
    LPWSTR   lpszFilename;
    LPWSTR   lpszCommandLine;
    LPWSTR   lpszPath;
    LPWSTR   lpszCurrentDirectory;
    LPWSTR  lpszLauncherName;
} DIRECTXREGISTERAPP2W, *PDIRECTXREGISTERAPP2W, *LPDIRECTXREGISTERAPP2W;
#endif //!ANSI_ONLY
#ifdef UNICODE
typedef DIRECTXREGISTERAPPW DIRECTXREGISTERAPP;
typedef PDIRECTXREGISTERAPPW PDIRECTXREGISTERAPP;
typedef LPDIRECTXREGISTERAPPW LPDIRECTXREGISTERAPP;
typedef DIRECTXREGISTERAPP2W DIRECTXREGISTERAPP2;
typedef PDIRECTXREGISTERAPP2W PDIRECTXREGISTERAPP2;
typedef LPDIRECTXREGISTERAPP2W LPDIRECTXREGISTERAPP2;
#else
typedef DIRECTXREGISTERAPPA DIRECTXREGISTERAPP;
typedef PDIRECTXREGISTERAPPA PDIRECTXREGISTERAPP;
typedef LPDIRECTXREGISTERAPPA LPDIRECTXREGISTERAPP;
typedef DIRECTXREGISTERAPP2A DIRECTXREGISTERAPP2;
typedef PDIRECTXREGISTERAPP2A PDIRECTXREGISTERAPP2;
typedef LPDIRECTXREGISTERAPP2A LPDIRECTXREGISTERAPP2;
#endif // UNICODE


//
// API
//

#ifndef UNICODE_ONLY
INT
WINAPI
DirectXSetupA(
             HWND  hWnd,
    __in_opt LPSTR lpszRootPath,
             DWORD dwFlags
    );
#endif //!UNICODE_ONLY
#ifndef ANSI_ONLY
INT
WINAPI
DirectXSetupW(
             HWND   hWnd,
    __in_opt LPWSTR lpszRootPath,
             DWORD  dwFlags
    );
#endif //!ANSI_ONLY
#ifdef UNICODE
#define DirectXSetup  DirectXSetupW
#else
#define DirectXSetup  DirectXSetupA
#endif // !UNICODE

#ifndef UNICODE_ONLY
INT
WINAPI
DirectXRegisterApplicationA(
    HWND                  hWnd,
    LPVOID                lpDXRegApp
    );
#endif //!UNICODE_ONLY
#ifndef ANSI_ONLY
INT
WINAPI
DirectXRegisterApplicationW(
    HWND                  hWnd,
    LPVOID                lpDXRegApp
    );
#endif //!ANSI_ONLY
#ifdef UNICODE
#define DirectXRegisterApplication  DirectXRegisterApplicationW
#else
#define DirectXRegisterApplication  DirectXRegisterApplicationA
#endif // !UNICODE

INT
WINAPI
DirectXUnRegisterApplication(
    HWND     hWnd,
    LPGUID   lpGUID
    );

//
// Function Pointers
//
#ifdef UNICODE
typedef INT (WINAPI * LPDIRECTXSETUP)(HWND, LPWSTR, DWORD);
typedef INT (WINAPI * LPDIRECTXREGISTERAPPLICATION)(HWND, LPVOID);
#else
typedef INT (WINAPI * LPDIRECTXSETUP)(HWND, LPSTR, DWORD);
typedef INT (WINAPI * LPDIRECTXREGISTERAPPLICATION)(HWND, LPVOID);
#endif // UNICODE

typedef DWORD (FAR PASCAL * DSETUP_CALLBACK)(DWORD Reason,
                                  DWORD MsgType,       /* Same as flags to MessageBox */
                                  LPSTR szMessage,
                                  LPSTR szName,
                                  void *pInfo);

INT WINAPI DirectXSetupSetCallback(DSETUP_CALLBACK Callback);
INT WINAPI DirectXSetupGetVersion(DWORD *lpdwVersion, DWORD *lpdwMinorVersion);
INT WINAPI DirectXSetupShowEULA(HWND hWndParent);
#ifndef UNICODE_ONLY
UINT
WINAPI
DirectXSetupGetEULAA(
    __out_ecount(cchEULA) LPSTR lpszEULA,
                          UINT  cchEULA,
                          WORD  LangID
    );
#endif //!UNICODE_ONLY
#ifndef ANSI_ONLY
UINT
WINAPI
DirectXSetupGetEULAW(
    __out_ecount(cchEULA) LPWSTR lpszEULA,
                          UINT   cchEULA,
                          WORD   LangID
    );
#endif //!ANSI_ONLY
#ifdef UNICODE
#define DirectXSetupGetEULA  DirectXSetupGetEULAW
typedef UINT (WINAPI * LPDIRECTXSETUPGETEULA)(LPWSTR, UINT, WORD);
#else
#define DirectXSetupGetEULA  DirectXSetupGetEULAA
typedef UINT (WINAPI * LPDIRECTXSETUPGETEULA)(LPSTR, UINT, WORD);
#endif // !UNICODE

#endif // WIN32


#ifdef __cplusplus
};
#endif

#endif
