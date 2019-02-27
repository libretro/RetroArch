
/* this ALWAYS GENERATED file contains the definitions for the interfaces */

 /* File created by MIDL compiler version 8.00.0613 */
/* @@MIDL_FILE_HEADING(  ) */

/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

/* verify that the <rpcsal.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCSAL_H_VERSION__
#define __REQUIRED_RPCSAL_H_VERSION__ 100
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif /* __RPCNDR_H_VERSION__ */

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __gameux_h__
#define __gameux_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */

#ifndef __IGameExplorer_FWD_DEFINED__
#define __IGameExplorer_FWD_DEFINED__
typedef interface IGameExplorer IGameExplorer;

#endif 	/* __IGameExplorer_FWD_DEFINED__ */

#ifndef __IGameStatistics_FWD_DEFINED__
#define __IGameStatistics_FWD_DEFINED__
typedef interface IGameStatistics IGameStatistics;

#endif 	/* __IGameStatistics_FWD_DEFINED__ */

#ifndef __IGameStatisticsMgr_FWD_DEFINED__
#define __IGameStatisticsMgr_FWD_DEFINED__
typedef interface IGameStatisticsMgr IGameStatisticsMgr;

#endif 	/* __IGameStatisticsMgr_FWD_DEFINED__ */

#ifndef __IGameExplorer2_FWD_DEFINED__
#define __IGameExplorer2_FWD_DEFINED__
typedef interface IGameExplorer2 IGameExplorer2;

#endif 	/* __IGameExplorer2_FWD_DEFINED__ */

#ifndef __GameExplorer_FWD_DEFINED__
#define __GameExplorer_FWD_DEFINED__

#ifdef __cplusplus
typedef class GameExplorer GameExplorer;
#else
typedef struct GameExplorer GameExplorer;
#endif /* __cplusplus */

#endif 	/* __GameExplorer_FWD_DEFINED__ */

#ifndef __GameStatistics_FWD_DEFINED__
#define __GameStatistics_FWD_DEFINED__

#ifdef __cplusplus
typedef class GameStatistics GameStatistics;
#else
typedef struct GameStatistics GameStatistics;
#endif /* __cplusplus */

#endif 	/* __GameStatistics_FWD_DEFINED__ */

/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"
#include "shobjidl_core.h"

#ifdef __cplusplus
extern "C"{
#endif

/* interface __MIDL_itf_gameux_0000_0000 */
/* [local] */

#include <winapifamily.h>
#pragma region Desktop Family
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define ID_GDF_XML __GDF_XML
#define ID_GDF_THUMBNAIL __GDF_THUMBNAIL
#define ID_ICON_ICO __ICON_ICO
#define ID_GDF_XML_STR L"__GDF_XML"
#define ID_GDF_THUMBNAIL_STR L"__GDF_THUMBNAIL"
typedef /* [v1_enum] */
enum GAME_INSTALL_SCOPE
    {
        GIS_NOT_INSTALLED	= 1,
        GIS_CURRENT_USER	= 2,
        GIS_ALL_USERS	= 3
    } 	GAME_INSTALL_SCOPE;

extern RPC_IF_HANDLE __MIDL_itf_gameux_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_gameux_0000_0000_v0_0_s_ifspec;

#ifndef __IGameExplorer_INTERFACE_DEFINED__
#define __IGameExplorer_INTERFACE_DEFINED__

/* interface IGameExplorer */
/* [unique][helpstring][uuid][object] */

EXTERN_C const IID IID_IGameExplorer;

#if defined(__cplusplus) && !defined(CINTERFACE)

    MIDL_INTERFACE("E7B2FB72-D728-49B3-A5F2-18EBF5F1349E")
    IGameExplorer : public IUnknown
    {
    public:
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE AddGame(
            /* [in] */ __RPC__in BSTR bstrGDFBinaryPath,
            /* [in] */ __RPC__in BSTR bstrGameInstallDirectory,
            /* [in] */ GAME_INSTALL_SCOPE installScope,
            /* [out][in] */ __RPC__inout GUID *pguidInstanceID) = 0;

        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE RemoveGame(
            /* [in] */ GUID guidInstanceID) = 0;

        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE UpdateGame(
            /* [in] */ GUID guidInstanceID) = 0;

        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE VerifyAccess(
            /* [in] */ __RPC__in BSTR bstrGDFBinaryPath,
            /* [out] */ __RPC__out BOOL *pfHasAccess) = 0;

    };

#else 	/* C style interface */

    typedef struct IGameExplorerVtbl
    {
        BEGIN_INTERFACE

        HRESULT ( STDMETHODCALLTYPE *QueryInterface )(
            __RPC__in IGameExplorer * This,
            /* [in] */ __RPC__in REFIID riid,
            /* [annotation][iid_is][out] */
            _COM_Outptr_  void **ppvObject);

        ULONG ( STDMETHODCALLTYPE *AddRef )(
            __RPC__in IGameExplorer * This);

        ULONG ( STDMETHODCALLTYPE *Release )(
            __RPC__in IGameExplorer * This);

        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *AddGame )(
            __RPC__in IGameExplorer * This,
            /* [in] */ __RPC__in BSTR bstrGDFBinaryPath,
            /* [in] */ __RPC__in BSTR bstrGameInstallDirectory,
            /* [in] */ GAME_INSTALL_SCOPE installScope,
            /* [out][in] */ __RPC__inout GUID *pguidInstanceID);

        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *RemoveGame )(
            __RPC__in IGameExplorer * This,
            /* [in] */ GUID guidInstanceID);

        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *UpdateGame )(
            __RPC__in IGameExplorer * This,
            /* [in] */ GUID guidInstanceID);

        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *VerifyAccess )(
            __RPC__in IGameExplorer * This,
            /* [in] */ __RPC__in BSTR bstrGDFBinaryPath,
            /* [out] */ __RPC__out BOOL *pfHasAccess);

        END_INTERFACE
    } IGameExplorerVtbl;

    interface IGameExplorer
    {
        CONST_VTBL struct IGameExplorerVtbl *lpVtbl;
    };

#ifdef COBJMACROS

#define IGameExplorer_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) )

#define IGameExplorer_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) )

#define IGameExplorer_Release(This)	\
    ( (This)->lpVtbl -> Release(This) )

#define IGameExplorer_AddGame(This,bstrGDFBinaryPath,bstrGameInstallDirectory,installScope,pguidInstanceID)	\
    ( (This)->lpVtbl -> AddGame(This,bstrGDFBinaryPath,bstrGameInstallDirectory,installScope,pguidInstanceID) )

#define IGameExplorer_RemoveGame(This,guidInstanceID)	\
    ( (This)->lpVtbl -> RemoveGame(This,guidInstanceID) )

#define IGameExplorer_UpdateGame(This,guidInstanceID)	\
    ( (This)->lpVtbl -> UpdateGame(This,guidInstanceID) )

#define IGameExplorer_VerifyAccess(This,bstrGDFBinaryPath,pfHasAccess)	\
    ( (This)->lpVtbl -> VerifyAccess(This,bstrGDFBinaryPath,pfHasAccess) )

#endif /* COBJMACROS */

#endif 	/* C style interface */

#endif 	/* __IGameExplorer_INTERFACE_DEFINED__ */

/* interface __MIDL_itf_gameux_0000_0001 */
/* [local] */

typedef /* [v1_enum] */
enum GAMESTATS_OPEN_TYPE
    {
        GAMESTATS_OPEN_OPENORCREATE	= 0,
        GAMESTATS_OPEN_OPENONLY	= 1
    } 	GAMESTATS_OPEN_TYPE;

typedef /* [v1_enum] */
enum GAMESTATS_OPEN_RESULT
    {
        GAMESTATS_OPEN_CREATED	= 0,
        GAMESTATS_OPEN_OPENED	= 1
    } 	GAMESTATS_OPEN_RESULT;

extern RPC_IF_HANDLE __MIDL_itf_gameux_0000_0001_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_gameux_0000_0001_v0_0_s_ifspec;

#ifndef __IGameStatistics_INTERFACE_DEFINED__
#define __IGameStatistics_INTERFACE_DEFINED__

/* interface IGameStatistics */
/* [unique][helpstring][uuid][object] */

EXTERN_C const IID IID_IGameStatistics;

#if defined(__cplusplus) && !defined(CINTERFACE)

    MIDL_INTERFACE("3887C9CA-04A0-42ae-BC4C-5FA6C7721145")
    IGameStatistics : public IUnknown
    {
    public:
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE GetMaxCategoryLength(
            /* [retval][out] */ __RPC__out UINT *cch) = 0;

        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE GetMaxNameLength(
            /* [retval][out] */ __RPC__out UINT *cch) = 0;

        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE GetMaxValueLength(
            /* [retval][out] */ __RPC__out UINT *cch) = 0;

        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE GetMaxCategories(
            /* [retval][out] */ __RPC__out WORD *pMax) = 0;

        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE GetMaxStatsPerCategory(
            /* [retval][out] */ __RPC__out WORD *pMax) = 0;

        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE SetCategoryTitle(
            /* [in] */ WORD categoryIndex,
            /* [string][in] */ __RPC__in_string LPCWSTR title) = 0;

        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE GetCategoryTitle(
            /* [in] */ WORD categoryIndex,
            /* [retval][string][out] */ __RPC__deref_out_opt_string LPWSTR *pTitle) = 0;

        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE GetStatistic(
            /* [in] */ WORD categoryIndex,
            /* [in] */ WORD statIndex,
            /* [string][unique][out][in] */ __RPC__deref_opt_inout_opt_string LPWSTR *pName,
            /* [string][unique][out][in] */ __RPC__deref_opt_inout_opt_string LPWSTR *pValue) = 0;

        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE SetStatistic(
            /* [in] */ WORD categoryIndex,
            /* [in] */ WORD statIndex,
            /* [string][in] */ __RPC__in_string LPCWSTR name,
            /* [string][in] */ __RPC__in_string LPCWSTR value) = 0;

        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE Save(
            /* [in] */ BOOL trackChanges) = 0;

        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE SetLastPlayedCategory(
            /* [in] */ UINT categoryIndex) = 0;

        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE GetLastPlayedCategory(
            /* [retval][out] */ __RPC__out UINT *pCategoryIndex) = 0;

    };

#else 	/* C style interface */

    typedef struct IGameStatisticsVtbl
    {
        BEGIN_INTERFACE

        HRESULT ( STDMETHODCALLTYPE *QueryInterface )(
            __RPC__in IGameStatistics * This,
            /* [in] */ __RPC__in REFIID riid,
            /* [annotation][iid_is][out] */
            _COM_Outptr_  void **ppvObject);

        ULONG ( STDMETHODCALLTYPE *AddRef )(
            __RPC__in IGameStatistics * This);

        ULONG ( STDMETHODCALLTYPE *Release )(
            __RPC__in IGameStatistics * This);

        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *GetMaxCategoryLength )(
            __RPC__in IGameStatistics * This,
            /* [retval][out] */ __RPC__out UINT *cch);

        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *GetMaxNameLength )(
            __RPC__in IGameStatistics * This,
            /* [retval][out] */ __RPC__out UINT *cch);

        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *GetMaxValueLength )(
            __RPC__in IGameStatistics * This,
            /* [retval][out] */ __RPC__out UINT *cch);

        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *GetMaxCategories )(
            __RPC__in IGameStatistics * This,
            /* [retval][out] */ __RPC__out WORD *pMax);

        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *GetMaxStatsPerCategory )(
            __RPC__in IGameStatistics * This,
            /* [retval][out] */ __RPC__out WORD *pMax);

        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *SetCategoryTitle )(
            __RPC__in IGameStatistics * This,
            /* [in] */ WORD categoryIndex,
            /* [string][in] */ __RPC__in_string LPCWSTR title);

        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *GetCategoryTitle )(
            __RPC__in IGameStatistics * This,
            /* [in] */ WORD categoryIndex,
            /* [retval][string][out] */ __RPC__deref_out_opt_string LPWSTR *pTitle);

        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *GetStatistic )(
            __RPC__in IGameStatistics * This,
            /* [in] */ WORD categoryIndex,
            /* [in] */ WORD statIndex,
            /* [string][unique][out][in] */ __RPC__deref_opt_inout_opt_string LPWSTR *pName,
            /* [string][unique][out][in] */ __RPC__deref_opt_inout_opt_string LPWSTR *pValue);

        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *SetStatistic )(
            __RPC__in IGameStatistics * This,
            /* [in] */ WORD categoryIndex,
            /* [in] */ WORD statIndex,
            /* [string][in] */ __RPC__in_string LPCWSTR name,
            /* [string][in] */ __RPC__in_string LPCWSTR value);

        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *Save )(
            __RPC__in IGameStatistics * This,
            /* [in] */ BOOL trackChanges);

        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *SetLastPlayedCategory )(
            __RPC__in IGameStatistics * This,
            /* [in] */ UINT categoryIndex);

        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *GetLastPlayedCategory )(
            __RPC__in IGameStatistics * This,
            /* [retval][out] */ __RPC__out UINT *pCategoryIndex);

        END_INTERFACE
    } IGameStatisticsVtbl;

    interface IGameStatistics
    {
        CONST_VTBL struct IGameStatisticsVtbl *lpVtbl;
    };

#ifdef COBJMACROS

#define IGameStatistics_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) )

#define IGameStatistics_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) )

#define IGameStatistics_Release(This)	\
    ( (This)->lpVtbl -> Release(This) )

#define IGameStatistics_GetMaxCategoryLength(This,cch)	\
    ( (This)->lpVtbl -> GetMaxCategoryLength(This,cch) )

#define IGameStatistics_GetMaxNameLength(This,cch)	\
    ( (This)->lpVtbl -> GetMaxNameLength(This,cch) )

#define IGameStatistics_GetMaxValueLength(This,cch)	\
    ( (This)->lpVtbl -> GetMaxValueLength(This,cch) )

#define IGameStatistics_GetMaxCategories(This,pMax)	\
    ( (This)->lpVtbl -> GetMaxCategories(This,pMax) )

#define IGameStatistics_GetMaxStatsPerCategory(This,pMax)	\
    ( (This)->lpVtbl -> GetMaxStatsPerCategory(This,pMax) )

#define IGameStatistics_SetCategoryTitle(This,categoryIndex,title)	\
    ( (This)->lpVtbl -> SetCategoryTitle(This,categoryIndex,title) )

#define IGameStatistics_GetCategoryTitle(This,categoryIndex,pTitle)	\
    ( (This)->lpVtbl -> GetCategoryTitle(This,categoryIndex,pTitle) )

#define IGameStatistics_GetStatistic(This,categoryIndex,statIndex,pName,pValue)	\
    ( (This)->lpVtbl -> GetStatistic(This,categoryIndex,statIndex,pName,pValue) )

#define IGameStatistics_SetStatistic(This,categoryIndex,statIndex,name,value)	\
    ( (This)->lpVtbl -> SetStatistic(This,categoryIndex,statIndex,name,value) )

#define IGameStatistics_Save(This,trackChanges)	\
    ( (This)->lpVtbl -> Save(This,trackChanges) )

#define IGameStatistics_SetLastPlayedCategory(This,categoryIndex)	\
    ( (This)->lpVtbl -> SetLastPlayedCategory(This,categoryIndex) )

#define IGameStatistics_GetLastPlayedCategory(This,pCategoryIndex)	\
    ( (This)->lpVtbl -> GetLastPlayedCategory(This,pCategoryIndex) )

#endif /* COBJMACROS */

#endif 	/* C style interface */

#endif 	/* __IGameStatistics_INTERFACE_DEFINED__ */

#ifndef __IGameStatisticsMgr_INTERFACE_DEFINED__
#define __IGameStatisticsMgr_INTERFACE_DEFINED__

/* interface IGameStatisticsMgr */
/* [unique][helpstring][uuid][object] */

EXTERN_C const IID IID_IGameStatisticsMgr;

#if defined(__cplusplus) && !defined(CINTERFACE)

    MIDL_INTERFACE("AFF3EA11-E70E-407d-95DD-35E612C41CE2")
    IGameStatisticsMgr : public IUnknown
    {
    public:
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE GetGameStatistics(
            /* [string][in] */ __RPC__in_string LPCWSTR GDFBinaryPath,
            /* [in] */ GAMESTATS_OPEN_TYPE openType,
            /* [out] */ __RPC__out GAMESTATS_OPEN_RESULT *pOpenResult,
            /* [retval][out] */ __RPC__deref_out_opt IGameStatistics **ppiStats) = 0;

        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE RemoveGameStatistics(
            /* [string][in] */ __RPC__in_string LPCWSTR GDFBinaryPath) = 0;

    };

#else 	/* C style interface */

    typedef struct IGameStatisticsMgrVtbl
    {
        BEGIN_INTERFACE

        HRESULT ( STDMETHODCALLTYPE *QueryInterface )(
            __RPC__in IGameStatisticsMgr * This,
            /* [in] */ __RPC__in REFIID riid,
            /* [annotation][iid_is][out] */
            _COM_Outptr_  void **ppvObject);

        ULONG ( STDMETHODCALLTYPE *AddRef )(
            __RPC__in IGameStatisticsMgr * This);

        ULONG ( STDMETHODCALLTYPE *Release )(
            __RPC__in IGameStatisticsMgr * This);

        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *GetGameStatistics )(
            __RPC__in IGameStatisticsMgr * This,
            /* [string][in] */ __RPC__in_string LPCWSTR GDFBinaryPath,
            /* [in] */ GAMESTATS_OPEN_TYPE openType,
            /* [out] */ __RPC__out GAMESTATS_OPEN_RESULT *pOpenResult,
            /* [retval][out] */ __RPC__deref_out_opt IGameStatistics **ppiStats);

        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *RemoveGameStatistics )(
            __RPC__in IGameStatisticsMgr * This,
            /* [string][in] */ __RPC__in_string LPCWSTR GDFBinaryPath);

        END_INTERFACE
    } IGameStatisticsMgrVtbl;

    interface IGameStatisticsMgr
    {
        CONST_VTBL struct IGameStatisticsMgrVtbl *lpVtbl;
    };

#ifdef COBJMACROS

#define IGameStatisticsMgr_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) )

#define IGameStatisticsMgr_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) )

#define IGameStatisticsMgr_Release(This)	\
    ( (This)->lpVtbl -> Release(This) )

#define IGameStatisticsMgr_GetGameStatistics(This,GDFBinaryPath,openType,pOpenResult,ppiStats)	\
    ( (This)->lpVtbl -> GetGameStatistics(This,GDFBinaryPath,openType,pOpenResult,ppiStats) )

#define IGameStatisticsMgr_RemoveGameStatistics(This,GDFBinaryPath)	\
    ( (This)->lpVtbl -> RemoveGameStatistics(This,GDFBinaryPath) )

#endif /* COBJMACROS */

#endif 	/* C style interface */

#endif 	/* __IGameStatisticsMgr_INTERFACE_DEFINED__ */

#ifndef __IGameExplorer2_INTERFACE_DEFINED__
#define __IGameExplorer2_INTERFACE_DEFINED__

/* interface IGameExplorer2 */
/* [unique][helpstring][uuid][object] */

EXTERN_C const IID IID_IGameExplorer2;

#if defined(__cplusplus) && !defined(CINTERFACE)

    MIDL_INTERFACE("86874AA7-A1ED-450d-A7EB-B89E20B2FFF3")
    IGameExplorer2 : public IUnknown
    {
    public:
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE InstallGame(
            /* [string][in] */ __RPC__in_string LPCWSTR binaryGDFPath,
            /* [unique][in] */ __RPC__in_opt LPCWSTR installDirectory,
            /* [in] */ GAME_INSTALL_SCOPE installScope) = 0;

        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE UninstallGame(
            /* [string][in] */ __RPC__in_string LPCWSTR binaryGDFPath) = 0;

        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE CheckAccess(
            /* [string][in] */ __RPC__in_string LPCWSTR binaryGDFPath,
            /* [retval][out] */ __RPC__out BOOL *pHasAccess) = 0;

    };

#else 	/* C style interface */

    typedef struct IGameExplorer2Vtbl
    {
        BEGIN_INTERFACE

        HRESULT ( STDMETHODCALLTYPE *QueryInterface )(
            __RPC__in IGameExplorer2 * This,
            /* [in] */ __RPC__in REFIID riid,
            /* [annotation][iid_is][out] */
            _COM_Outptr_  void **ppvObject);

        ULONG ( STDMETHODCALLTYPE *AddRef )(
            __RPC__in IGameExplorer2 * This);

        ULONG ( STDMETHODCALLTYPE *Release )(
            __RPC__in IGameExplorer2 * This);

        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *InstallGame )(
            __RPC__in IGameExplorer2 * This,
            /* [string][in] */ __RPC__in_string LPCWSTR binaryGDFPath,
            /* [unique][in] */ __RPC__in_opt LPCWSTR installDirectory,
            /* [in] */ GAME_INSTALL_SCOPE installScope);

        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *UninstallGame )(
            __RPC__in IGameExplorer2 * This,
            /* [string][in] */ __RPC__in_string LPCWSTR binaryGDFPath);

        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *CheckAccess )(
            __RPC__in IGameExplorer2 * This,
            /* [string][in] */ __RPC__in_string LPCWSTR binaryGDFPath,
            /* [retval][out] */ __RPC__out BOOL *pHasAccess);

        END_INTERFACE
    } IGameExplorer2Vtbl;

    interface IGameExplorer2
    {
        CONST_VTBL struct IGameExplorer2Vtbl *lpVtbl;
    };

#ifdef COBJMACROS

#define IGameExplorer2_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) )

#define IGameExplorer2_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) )

#define IGameExplorer2_Release(This)	\
    ( (This)->lpVtbl -> Release(This) )

#define IGameExplorer2_InstallGame(This,binaryGDFPath,installDirectory,installScope)	\
    ( (This)->lpVtbl -> InstallGame(This,binaryGDFPath,installDirectory,installScope) )

#define IGameExplorer2_UninstallGame(This,binaryGDFPath)	\
    ( (This)->lpVtbl -> UninstallGame(This,binaryGDFPath) )

#define IGameExplorer2_CheckAccess(This,binaryGDFPath,pHasAccess)	\
    ( (This)->lpVtbl -> CheckAccess(This,binaryGDFPath,pHasAccess) )

#endif /* COBJMACROS */

#endif 	/* C style interface */

#endif 	/* __IGameExplorer2_INTERFACE_DEFINED__ */

#ifndef __gameuxLib_LIBRARY_DEFINED__
#define __gameuxLib_LIBRARY_DEFINED__

/* library gameuxLib */
/* [helpstring][version][uuid] */

EXTERN_C const IID LIBID_gameuxLib;

EXTERN_C const CLSID CLSID_GameExplorer;

#ifdef __cplusplus

class DECLSPEC_UUID("9A5EA990-3034-4D6F-9128-01F3C61022BC")
GameExplorer;
#endif

EXTERN_C const CLSID CLSID_GameStatistics;

#ifdef __cplusplus

class DECLSPEC_UUID("DBC85A2C-C0DC-4961-B6E2-D28B62C11AD4")
GameStatistics;
#endif
#endif /* __gameuxLib_LIBRARY_DEFINED__ */

/* interface __MIDL_itf_gameux_0000_0005 */
/* [local] */

#endif /* WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) */
#pragma endregion

extern RPC_IF_HANDLE __MIDL_itf_gameux_0000_0005_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_gameux_0000_0005_v0_0_s_ifspec;

/* Additional Prototypes for ALL interfaces */

unsigned long             __RPC_USER  BSTR_UserSize(     __RPC__in unsigned long *, unsigned long            , __RPC__in BSTR * );
unsigned char * __RPC_USER  BSTR_UserMarshal(  __RPC__in unsigned long *, __RPC__inout_xcount(0) unsigned char *, __RPC__in BSTR * );
unsigned char * __RPC_USER  BSTR_UserUnmarshal(__RPC__in unsigned long *, __RPC__in_xcount(0) unsigned char *, __RPC__out BSTR * );
void                      __RPC_USER  BSTR_UserFree(     __RPC__in unsigned long *, __RPC__in BSTR * );

unsigned long             __RPC_USER  BSTR_UserSize64(     __RPC__in unsigned long *, unsigned long            , __RPC__in BSTR * );
unsigned char * __RPC_USER  BSTR_UserMarshal64(  __RPC__in unsigned long *, __RPC__inout_xcount(0) unsigned char *, __RPC__in BSTR * );
unsigned char * __RPC_USER  BSTR_UserUnmarshal64(__RPC__in unsigned long *, __RPC__in_xcount(0) unsigned char *, __RPC__out BSTR * );
void                      __RPC_USER  BSTR_UserFree64(     __RPC__in unsigned long *, __RPC__in BSTR * );

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif
