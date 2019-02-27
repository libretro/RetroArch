/*==========================================================================;
 *
 *  Copyright (C) Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       dxdiag.h
 *  Content:    DirectX Diagnostic Tool include file
 *
 ****************************************************************************/

#ifndef _DXDIAG_H_
#define _DXDIAG_H_
#include <winapifamily.h>

#pragma region Desktop Family
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)

#include <ole2.h>      // for DECLARE_INTERFACE_ and HRESULT

// This identifier is passed to IDxDiagProvider::Initialize in order to ensure that an
// application was built against the correct header files. This number is
// incremented whenever a header (or other) change would require applications
// to be rebuilt. If the version doesn't match, IDxDiagProvider::Initialize will fail.
// (The number itself has no meaning.)
#define DXDIAG_DX9_SDK_VERSION 111

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************
 *
 * DxDiag Errors
 *
 ****************************************************************************/
#define DXDIAG_E_INSUFFICIENT_BUFFER       ((HRESULT)0x8007007AL)  // HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)

/****************************************************************************
 *
 * DxDiag CLSIDs
 *
 ****************************************************************************/

// {A65B8071-3BFE-4213-9A5B-491DA4461CA7}
DEFINE_GUID(CLSID_DxDiagProvider,
0xA65B8071, 0x3BFE, 0x4213, 0x9A, 0x5B, 0x49, 0x1D, 0xA4, 0x46, 0x1C, 0xA7);

/****************************************************************************
 *
 * DxDiag Interface IIDs
 *
 ****************************************************************************/

// {9C6B4CB0-23F8-49CC-A3ED-45A55000A6D2}
DEFINE_GUID(IID_IDxDiagProvider,
0x9C6B4CB0, 0x23F8, 0x49CC, 0xA3, 0xED, 0x45, 0xA5, 0x50, 0x00, 0xA6, 0xD2);

// {0x7D0F462F-0x4064-0x4862-BC7F-933E5058C10F}
DEFINE_GUID(IID_IDxDiagContainer,
0x7D0F462F, 0x4064, 0x4862, 0xBC, 0x7F, 0x93, 0x3E, 0x50, 0x58, 0xC1, 0x0F);

/****************************************************************************
 *
 * DxDiag Interface Pointer definitions
 *
 ****************************************************************************/

typedef struct IDxDiagProvider *LPDXDIAGPROVIDER, *PDXDIAGPROVIDER;

typedef struct IDxDiagContainer *LPDXDIAGCONTAINER, *PDXDIAGCONTAINER;

/****************************************************************************
 *
 * DxDiag Structures
 *
 ****************************************************************************/

typedef struct _DXDIAG_INIT_PARAMS
{
    DWORD   dwSize;                 // Size of this structure.
    DWORD   dwDxDiagHeaderVersion;  // Pass in DXDIAG_DX9_SDK_VERSION.  This verifies
                                    // the header and dll are correctly matched.
    BOOL    bAllowWHQLChecks;       // If true, allow dxdiag to check if drivers are
                                    // digital signed as logo'd by WHQL which may
                                    // connect via internet to update WHQL certificates.
    VOID*   pReserved;              // Reserved. Must be NULL.
} DXDIAG_INIT_PARAMS;

/****************************************************************************
 *
 * DxDiag Application Interfaces
 *
 ****************************************************************************/

//
// COM definition for IDxDiagProvider
//
#undef INTERFACE                // External COM Implementation
#define INTERFACE IDxDiagProvider
DECLARE_INTERFACE_(IDxDiagProvider,IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)               (THIS_ REFIID riid, LPVOID *ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)                (THIS) PURE;
    STDMETHOD_(ULONG,Release)               (THIS) PURE;

    /*** IDxDiagProvider methods ***/
    STDMETHOD(Initialize)                   (THIS_ DXDIAG_INIT_PARAMS* pParams) PURE;
    STDMETHOD(GetRootContainer)             (THIS_ IDxDiagContainer **ppInstance) PURE;
};

//
// COM definition for IDxDiagContainer
//
#undef INTERFACE                // External COM Implementation
#define INTERFACE IDxDiagContainer
DECLARE_INTERFACE_(IDxDiagContainer,IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)               (THIS_ REFIID riid, LPVOID *ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)                (THIS) PURE;
    STDMETHOD_(ULONG,Release)               (THIS) PURE;

    /*** IDxDiagContainer methods ***/
    STDMETHOD(GetNumberOfChildContainers)   (THIS_ DWORD *pdwCount) PURE;
    STDMETHOD(EnumChildContainerNames)      (THIS_ DWORD dwIndex, LPWSTR pwszContainer, DWORD cchContainer) PURE;
    STDMETHOD(GetChildContainer)            (THIS_ LPCWSTR pwszContainer, IDxDiagContainer **ppInstance) PURE;
    STDMETHOD(GetNumberOfProps)             (THIS_ DWORD *pdwCount) PURE;
    STDMETHOD(EnumPropNames)                (THIS_ DWORD dwIndex, LPWSTR pwszPropName, DWORD cchPropName) PURE;
    STDMETHOD(GetProp)                      (THIS_ LPCWSTR pwszPropName, VARIANT *pvarProp) PURE;
};

/****************************************************************************
 *
 * DxDiag application interface macros
 *
 ****************************************************************************/

#if !defined(__cplusplus) || defined(CINTERFACE)

#define IDxDiagProvider_QueryInterface(p,a,b)                   (p)->lpVtbl->QueryInterface(p,a,b)
#define IDxDiagProvider_AddRef(p)                               (p)->lpVtbl->AddRef(p)
#define IDxDiagProvider_Release(p)                              (p)->lpVtbl->Release(p)
#define IDxDiagProvider_Initialize(p,a,b)                       (p)->lpVtbl->Initialize(p,a,b)
#define IDxDiagProvider_GetRootContainer(p,a)                   (p)->lpVtbl->GetRootContainer(p,a)

#define IDxDiagContainer_QueryInterface(p,a,b)                  (p)->lpVtbl->QueryInterface(p,a,b)
#define IDxDiagContainer_AddRef(p)                              (p)->lpVtbl->AddRef(p)
#define IDxDiagContainer_Release(p)                             (p)->lpVtbl->Release(p)
#define IDxDiagContainer_GetNumberOfChildContainers(p,a)        (p)->lpVtbl->GetNumberOfChildContainers(p,a)
#define IDxDiagContainer_EnumChildContainerNames(p,a,b,c)       (p)->lpVtbl->EnumChildContainerNames(p,a,b,c)
#define IDxDiagContainer_GetChildContainer(p,a,b)               (p)->lpVtbl->GetChildContainer(p,a,b)
#define IDxDiagContainer_GetNumberOfProps(p,a)                  (p)->lpVtbl->GetNumberOfProps(p,a)
#define IDxDiagContainer_EnumProps(p,a,b)                       (p)->lpVtbl->EnumProps(p,a,b,c)
#define IDxDiagContainer_GetProp(p,a,b)                         (p)->lpVtbl->GetProp(p,a,b)

#else /* C++ */

#define IDxDiagProvider_QueryInterface(p,a,b)                   (p)->QueryInterface(p,a,b)
#define IDxDiagProvider_AddRef(p)                               (p)->AddRef(p)
#define IDxDiagProvider_Release(p)                              (p)->Release(p)
#define IDxDiagProvider_Initialize(p,a,b)                       (p)->Initialize(p,a,b)
#define IDxDiagProvider_GetRootContainer(p,a)                   (p)->GetRootContainer(p,a)

#define IDxDiagContainer_QueryInterface(p,a,b)                  (p)->QueryInterface(p,a,b)
#define IDxDiagContainer_AddRef(p)                              (p)->AddRef(p)
#define IDxDiagContainer_Release(p)                             (p)->Release(p)
#define IDxDiagContainer_GetNumberOfChildContainers(p,a)        (p)->GetNumberOfChildContainers(p,a)
#define IDxDiagContainer_EnumChildContainerNames(p,a,b,c)       (p)->EnumChildContainerNames(p,a,b,c)
#define IDxDiagContainer_GetChildContainer(p,a,b)               (p)->GetChildContainer(p,a,b)
#define IDxDiagContainer_GetNumberOfProps(p,a)                  (p)->GetNumberOfProps(p,a)
#define IDxDiagContainer_EnumProps(p,a,b)                       (p)->EnumProps(p,a,b,c)
#define IDxDiagContainer_GetProp(p,a,b)                         (p)->GetProp(p,a,b)

#endif

#ifdef __cplusplus
}
#endif

#endif /* WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) */
#pragma endregion

#endif /* _DXDIAG_H_ */
