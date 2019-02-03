/*=========================================================================*\

    Copyright (c) Microsoft Corporation.  All rights reserved.

\*=========================================================================*/

#pragma once

/*#include <winapifamily.h>*/

/*#pragma region Desktop Family*/
/*#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)*/

/*=========================================================================*\
    D2D Status Codes
\*=========================================================================*/

#define FACILITY_D2D 0x899

#define MAKE_D2DHR( sev, code )\
    MAKE_HRESULT( sev, FACILITY_D2D, (code) )

#define MAKE_D2DHR_ERR( code )\
    MAKE_D2DHR( 1, code )

//+----------------------------------------------------------------------------
//
// D2D error codes
//
//------------------------------------------------------------------------------

//
//  Error codes shared with WINCODECS
//

//
// The pixel format is not supported.
//
#define D2DERR_UNSUPPORTED_PIXEL_FORMAT     WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT

//
// Error codes that were already returned in prior versions and were part of the
// MIL facility.

//
// Error codes mapped from WIN32 where there isn't already another HRESULT based
// define
//

//
// The supplied buffer was too small to accommodate the data.
//
#define D2DERR_INSUFFICIENT_BUFFER          HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)

//
// The file specified was not found.
//
#define D2DERR_FILE_NOT_FOUND               HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)

#ifndef D2DERR_WRONG_STATE

//
// D2D specific codes
//

//
// The object was not in the correct state to process the method.
//
#define D2DERR_WRONG_STATE                  MAKE_D2DHR_ERR(0x001)

//
// The object has not yet been initialized.
//
#define D2DERR_NOT_INITIALIZED              MAKE_D2DHR_ERR(0x002)

//
// The requested opertion is not supported.
//
#define D2DERR_UNSUPPORTED_OPERATION        MAKE_D2DHR_ERR(0x003)

//
// The geomery scanner failed to process the data.
//
#define D2DERR_SCANNER_FAILED               MAKE_D2DHR_ERR(0x004)

//
// D2D could not access the screen.
//
#define D2DERR_SCREEN_ACCESS_DENIED         MAKE_D2DHR_ERR(0x005)

//
// A valid display state could not be determined.
//
#define D2DERR_DISPLAY_STATE_INVALID        MAKE_D2DHR_ERR(0x006)

//
// The supplied vector is vero.
//
#define D2DERR_ZERO_VECTOR                  MAKE_D2DHR_ERR(0x007)

//
// An internal error (D2D bug) occurred. On checked builds, we would assert.
//
// The application should close this instance of D2D and should consider
// restarting its process.
//
#define D2DERR_INTERNAL_ERROR               MAKE_D2DHR_ERR(0x008)

//
// The display format we need to render is not supported by the
// hardware device.
//
#define D2DERR_DISPLAY_FORMAT_NOT_SUPPORTED MAKE_D2DHR_ERR(0x009)

//
// A call to this method is invalid.
//
#define D2DERR_INVALID_CALL                 MAKE_D2DHR_ERR(0x00A)

//
// No HW rendering device is available for this operation.
//
#define D2DERR_NO_HARDWARE_DEVICE           MAKE_D2DHR_ERR(0x00B)

//
// There has been a presentation error that may be recoverable. The caller
// needs to recreate, rerender the entire frame, and reattempt present.
//
#define D2DERR_RECREATE_TARGET              MAKE_D2DHR_ERR(0x00C)

//
// Shader construction failed because it was too complex.
//
#define D2DERR_TOO_MANY_SHADER_ELEMENTS     MAKE_D2DHR_ERR(0x00D)

//
// Shader compilation failed.
//
#define D2DERR_SHADER_COMPILE_FAILED        MAKE_D2DHR_ERR(0x00E)

//
// Requested DX surface size exceeded maximum texture size.
//
#define D2DERR_MAX_TEXTURE_SIZE_EXCEEDED    MAKE_D2DHR_ERR(0x00F)

//
// The requested D2D version is not supported.
//
#define D2DERR_UNSUPPORTED_VERSION          MAKE_D2DHR_ERR(0x010)

//
// Invalid number.
//
#define D2DERR_BAD_NUMBER                   MAKE_D2DHR_ERR(0x0011)

//
// Objects used together must be created from the same factory instance.
//
#define D2DERR_WRONG_FACTORY                MAKE_D2DHR_ERR(0x012)

//
// A layer resource can only be in use once at any point in time.
//
#define D2DERR_LAYER_ALREADY_IN_USE         MAKE_D2DHR_ERR(0x013)

//
// The pop call did not match the corresponding push call
//
#define D2DERR_POP_CALL_DID_NOT_MATCH_PUSH  MAKE_D2DHR_ERR(0x014)

//
// The resource was realized on the wrong render target
//
#define D2DERR_WRONG_RESOURCE_DOMAIN        MAKE_D2DHR_ERR(0x015)

//
// The push and pop calls were unbalanced
//
#define D2DERR_PUSH_POP_UNBALANCED          MAKE_D2DHR_ERR(0x016)

//
// Attempt to copy from a render target while a layer or clip rect is applied
//
#define D2DERR_RENDER_TARGET_HAS_LAYER_OR_CLIPRECT MAKE_D2DHR_ERR(0x017)

//
// The brush types are incompatible for the call.
//
#define D2DERR_INCOMPATIBLE_BRUSH_TYPES     MAKE_D2DHR_ERR(0x018)

//
// An unknown win32 failure occurred.
//
#define D2DERR_WIN32_ERROR                  MAKE_D2DHR_ERR(0x019)

//
// The render target is not compatible with GDI
//
#define D2DERR_TARGET_NOT_GDI_COMPATIBLE    MAKE_D2DHR_ERR(0x01A)

//
// A text client drawing effect object is of the wrong type
//
#define D2DERR_TEXT_EFFECT_IS_WRONG_TYPE    MAKE_D2DHR_ERR(0x01B)

//
// The application is holding a reference to the IDWriteTextRenderer interface
// after the corresponding DrawText or DrawTextLayout call has returned. The
// IDWriteTextRenderer instance will be zombied.
//
#define D2DERR_TEXT_RENDERER_NOT_RELEASED   MAKE_D2DHR_ERR(0x01C)

//
// The requested size is larger than the guaranteed supported texture size.
//
#define D2DERR_EXCEEDS_MAX_BITMAP_SIZE     MAKE_D2DHR_ERR(0x01D)

#else /*D2DERR_WRONG_STATE*/

//
// D2D specific codes now live in winerror.h
//

#endif /*D2DERR_WRONG_STATE*/

/*#endif*/ /* WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) */
/*#pragma endregion*/
