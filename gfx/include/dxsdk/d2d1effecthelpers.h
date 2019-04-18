//---------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// D2D helper functions for effect authors.
//
// File name: D2D1EffectHelpers.h
//---------------------------------------------------------------------------
#pragma once

#ifndef _D2D1_EFFECT_HELPERS_H_
#define _D2D1_EFFECT_HELPERS_H_

/*#include <winapifamily.h>*/

/*#pragma region Application Family*/
/*#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)*/

#include <d2d1effectauthor.h>

//+-----------------------------------------------------------------------------
//
//  Function:
//      DeducingValueSetter
//
//  Synopsis:
//      Deduces the class and arguments and then calls a member-function property
//      setter callback for a value-type property.
//
//      This should not be called directly.
//
//--------------------------------------------------------------------------------
template<class C, typename P, typename I>
HRESULT DeducingValueSetter(
    _In_ HRESULT (C::*callback)(P),
    _In_ I *effect,
    _In_reads_(dataSize) const BYTE *data,
    UINT32 dataSize
    )
{
    // We must exactly match the value-type's size.
    if (dataSize != sizeof(P))
    {
        return E_INVALIDARG;
    }

    return (static_cast<C *>(effect)->*callback)(*reinterpret_cast<const P *>(data));
}

//+-----------------------------------------------------------------------------
//
//  Function:
//      ValueSetter
//
//  Synopsis:
//      Calls a member-function property setter callback for a value-type property.
//
//--------------------------------------------------------------------------------
template<typename T, T P, typename I>
HRESULT CALLBACK ValueSetter(
    _In_ IUnknown *effect,
    _In_reads_(dataSize) const BYTE *data,
    UINT32 dataSize
    )
{
    // Cast through I to resolve multiple-inheritance ambiguities.
    return DeducingValueSetter(P, static_cast<I *>(effect), data, dataSize);
}

//+-----------------------------------------------------------------------------
//
//  Function:
//      DeducingValueGetter
//
//  Synopsis:
//      Deduces the class and arguments and then calls a member-function property
//      getter callback for a value-type property.
//
//      This should not be called directly.
//
//--------------------------------------------------------------------------------
template<class C, typename P, typename I>
HRESULT DeducingValueGetter(
    _In_ P (C::*callback)() const,
    _In_ const I *effect,
    _Out_writes_opt_(dataSize) BYTE *data,
    UINT32 dataSize,
    _Out_opt_ UINT32 *actualSize
    )
{
    if (actualSize)
    {
        *actualSize = sizeof(P);
    }

    if (dataSize > 0 && data)
    {
        if (dataSize < sizeof(P))
        {
            return E_NOT_SUFFICIENT_BUFFER;
        }

        *reinterpret_cast<P *>(data) = (static_cast<const C *>(effect)->*callback)();
    }

    return S_OK;
}

//+-----------------------------------------------------------------------------
//
//  Function:
//      ValueGetter
//
//  Synopsis:
//      Calls a member-function property setter callback for a value-type property.
//
//--------------------------------------------------------------------------------
template<typename T, T P, typename I>
HRESULT CALLBACK ValueGetter(
    _In_ const IUnknown *effect,
    _Out_writes_opt_(dataSize) BYTE *data,
    UINT32 dataSize,
    _Out_opt_ UINT32 *actualSize
    )
{
    // Cast through I to resolve multiple-inheritance ambiguities.
    return DeducingValueGetter(P, static_cast<const I *>(effect), data, dataSize, actualSize);
}

//+-----------------------------------------------------------------------------
//
//  Function:
//      DeducingBlobSetter
//
//  Synopsis:
//      Deduces the class and arguments and then calls a member-function property
//      setter callback for a blob-type property.
//
//      This should not be called directly.
//
//--------------------------------------------------------------------------------
template<class C, typename I>
HRESULT DeducingBlobSetter(
    _In_ HRESULT (C::*callback)(const BYTE *, UINT32),
    _In_ I *effect,
    _In_reads_(dataSize) const BYTE *data,
    UINT32 dataSize
    )
{
    return (static_cast<C *>(effect)->*callback)(data, dataSize);
}

//+-----------------------------------------------------------------------------
//
//  Function:
//      BlobSetter
//
//  Synopsis:
//      Calls a member-function property setter callback for a blob-type property.
//
//--------------------------------------------------------------------------------
template<typename T, T P, typename I>
HRESULT CALLBACK BlobSetter(
    _In_ IUnknown *effect,
    _In_reads_(dataSize) const BYTE *data,
    UINT32 dataSize
    )
{
    // Cast through I to resolve multiple-inheritance ambiguities.
    return DeducingBlobSetter(P, static_cast<I *>(effect), data, dataSize);
}

//+-----------------------------------------------------------------------------
//
//  Function:
//      DeducingBlobGetter
//
//  Synopsis:
//      Deduces the class and arguments and then calls a member-function property
//      getter callback for a blob-type property.
//
//      This should not be called directly.
//
//--------------------------------------------------------------------------------
template<class C, typename I>
HRESULT DeducingBlobGetter(
    _In_ HRESULT (C::*callback)(BYTE *, UINT32, UINT32*) const,
    _In_ const I *effect,
    _Out_writes_opt_(dataSize) BYTE *data,
    UINT32 dataSize,
    _Out_opt_ UINT32 *actualSize
    )
{
    return (static_cast<const C *>(effect)->*callback)(data, dataSize, actualSize);
}

//+-----------------------------------------------------------------------------
//
//  Function:
//      BlobGetter
//
//  Synopsis:
//      Calls a member-function property getter callback for a blob-type property.
//
//--------------------------------------------------------------------------------
template<typename T, T P, typename I>
HRESULT CALLBACK BlobGetter(
    _In_ const IUnknown *effect,
    _Out_writes_opt_(dataSize) BYTE *data,
    UINT32 dataSize,
    _Out_opt_ UINT32 *actualSize
    )
{
    // Cast through I to resolve multiple-inheritance ambiguities.
    return DeducingBlobGetter(P, static_cast<const I *>(effect), data, dataSize, actualSize);
}

//+-----------------------------------------------------------------------------
//
//  Function:
//      DeducingStringSetter
//
//  Synopsis:
//      Deduces the class and arguments and then calls a member-function property
//      setter callback for a string-type property.
//
//      This should not be called directly.
//
//--------------------------------------------------------------------------------
template<class C, typename I>
HRESULT DeducingStringSetter(
    _In_ HRESULT (C::*callback)(PCWSTR string),
    _In_ I *effect,
    _In_reads_(dataSize) const BYTE *data,
    UINT32 dataSize
    )
{
    dataSize;

    return (static_cast<C *>(effect)->*callback)(reinterpret_cast<PCWSTR>(data));
}

//+-----------------------------------------------------------------------------
//
//  Function:
//      StringSetter
//
//  Synopsis:
//      Calls a member-function property setter callback for a string-type property.
//
//--------------------------------------------------------------------------------
template<typename T, T P, typename I>
HRESULT CALLBACK StringSetter(
    _In_ IUnknown *effect,
    _In_reads_(dataSize) const BYTE *data,
    UINT32 dataSize
    )
{
    // Cast through I to resolve multiple-inheritance ambiguities.
    return DeducingStringSetter(P, static_cast<I *>(effect), data, dataSize);
}

//+-----------------------------------------------------------------------------
//
//  Function:
//      DeducingStringGetter
//
//  Synopsis:
//      Deduces the class and arguments and then calls a member-function property
//      getter callback for a string-type property.
//
//      This should not be called directly.
//
//--------------------------------------------------------------------------------
template<class C, typename I>
HRESULT DeducingStringGetter(
    _In_ HRESULT (C::*callback)(PWSTR, UINT32, UINT32*) const,
    _In_ const I *effect,
    _Out_writes_opt_(dataSize) BYTE *data,
    UINT32 dataSize,
    _Out_opt_ UINT32 *actualSize
    )
{
    UINT32 cchString = 0;

    HRESULT hr = (static_cast<const C *>(effect)->*callback)(reinterpret_cast<PWSTR>(data), dataSize / sizeof(WCHAR), &cchString);

    if ((SUCCEEDED(hr) || hr == E_NOT_SUFFICIENT_BUFFER) && actualSize)
    {
        *actualSize = cchString * sizeof(WCHAR);
    }

    return hr;
}

//+-----------------------------------------------------------------------------
//
//  Function:
//      StringGetter
//
//  Synopsis:
//      Calls a member-function property getter callback for a string-type property.
//
//--------------------------------------------------------------------------------
template<typename T, T P, typename I>
HRESULT CALLBACK StringGetter(
    _In_ const IUnknown *effect,
    _Out_writes_opt_(dataSize) BYTE *data,
    UINT32 dataSize,
    _Out_opt_ UINT32 *actualSize
    )
{
    // Cast through I to resolve multiple-inheritance ambiguities.
    return DeducingStringGetter(P, static_cast<const I *>(effect), data, dataSize, actualSize);
}

//
// Simpler versions of the helpers can be declared if decltype is available:
//
#if _MSC_VER >= 1600
#define D2D1_SIMPLE_BINDING_MACROS
#endif

#ifdef D2D1_SIMPLE_BINDING_MACROS

//
// Helper to work around decltype issues:
//
template<typename T>
T GetType(T t) { return t; };

//
// Helper macros for declaring a D2D1_PROPERTY_BINDING for value, blob, or string callbacks.
//
#define D2D1_VALUE_TYPE_BINDING(NAME, SETTER, GETTER) \
    { NAME, &ValueSetter<decltype(GetType(SETTER)), SETTER, ID2D1EffectImpl>, &ValueGetter<decltype(GetType(GETTER)), GETTER, ID2D1EffectImpl> }

#define D2D1_BLOB_TYPE_BINDING(NAME, SETTER, GETTER) \
    { NAME, &BlobSetter<decltype(GetType(SETTER)), SETTER, ID2D1EffectImpl>, &BlobGetter<decltype(GetType(GETTER)), GETTER, ID2D1EffectImpl> }

#define D2D1_STRING_TYPE_BINDING(NAME, SETTER, GETTER) \
    { NAME, &StringSetter<decltype(GetType(SETTER)), SETTER, ID2D1EffectImpl>, &StringGetter<decltype(GetType(GETTER)), GETTER, ID2D1EffectImpl> }

//
// Read-only variants:
//
#define D2D1_READONLY_VALUE_TYPE_BINDING(NAME, GETTER) \
    { NAME, NULL, &ValueGetter<decltype(GetType(GETTER)), GETTER, ID2D1EffectImpl> }

#define D2D1_READONLY_BLOB_TYPE_BINDING(NAME, GETTER) \
    { NAME, NULL, &BlobGetter<decltype(GetType(GETTER)), GETTER, ID2D1EffectImpl> }

#define D2D1_READONLY_STRING_TYPE_BINDING(NAME, GETTER) \
    { NAME, NULL, &StringGetter<decltype(GetType(GETTER)), GETTER, ID2D1EffectImpl> }

#else // #ifdef D2D1_SIMPLE_BINDING_MACROS

//
// Helper macros for declaring a D2D1_PROPERTY_BINDING for value, blob, or string callbacks.
//
#define D2D1_VALUE_TYPE_BINDING(NAME, TYPE, CLASS, SETTER, GETTER)          \
    {                                                                       \
        NAME,                                                               \
        &ValueSetter<HRESULT (CLASS::*)(TYPE),  SETTER, ID2D1EffectImpl>,   \
        &ValueGetter<TYPE (CLASS::*)() const,   GETTER, ID2D1EffectImpl>    \
    }

#define D2D1_BLOB_TYPE_BINDING(NAME, CLASS, SETTER, GETTER)                                         \
    {                                                                                               \
        NAME,                                                                                       \
        &BlobSetter<HRESULT (CLASS::*)(const BYTE *, UINT32),           SETTER, ID2D1EffectImpl>,   \
        &BlobGetter<HRESULT (CLASS::*)(BYTE *, UINT32, UINT32*) const,  GETTER, ID2D1EffectImpl>    \
    }

#define D2D1_STRING_TYPE_BINDING(NAME, CLASS, SETTER, GETTER)                                       \
    {                                                                                               \
        NAME,                                                                                       \
        &StringSetter<HRESULT (CLASS::*)(PCWSTR string),                SETTER, ID2D1EffectImpl>,   \
        &StringGetter<HRESULT (CLASS::*)(PWSTR, UINT32, UINT32*) const, GETTER, ID2D1EffectImpl>    \
    }

//
// Read-only variants:
//
#define D2D1_READONLY_VALUE_TYPE_BINDING(NAME, TYPE, CLASS, GETTER)         \
    {                                                                       \
        NAME,                                                               \
        NULL,                                                               \
        &ValueGetter<TYPE (CLASS::*)() const,   GETTER, ID2D1EffectImpl>    \
    }

#define D2D1_READONLY_BLOB_TYPE_BINDING(NAME, CLASS, GETTER)                                        \
    {                                                                                               \
        NAME,                                                                                       \
        NULL,                                                                                       \
        &BlobGetter<HRESULT (CLASS::*)(BYTE *, UINT32, UINT32*) const,  GETTER, ID2D1EffectImpl>    \
    }

#define D2D1_READONLY_STRING_TYPE_BINDING(NAME, CLASS, GETTER)                                      \
    {                                                                                               \
        NAME,                                                                                       \
        NULL,                                                                                       \
        &StringGetter<HRESULT (CLASS::*)(PWSTR, UINT32, UINT32*) const, GETTER, ID2D1EffectImpl>    \
    }

#endif // #ifdef D2D1_SIMPLE_BINDING_MACROS

/*#endif*/ /* WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP) */
/*#pragma endregion*/

#endif // #ifndef _D2D1_AUTHOR_H_
