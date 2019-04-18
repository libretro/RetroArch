//////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) Microsoft Corporation.  All Rights Reserved.
//
//  File:       d3dx10.h
//  Content:    D3DX10 utility library
//
//////////////////////////////////////////////////////////////////////////////

#ifdef  __D3DX10_INTERNAL__
#error Incorrect D3DX10 header used
#endif

#ifndef __D3DX10_H__
#define __D3DX10_H__

// Defines
#include <limits.h>
#include <float.h>

#define D3DX10_DEFAULT            ((UINT) -1)
#define D3DX10_FROM_FILE          ((UINT) -3)
#define DXGI_FORMAT_FROM_FILE     ((DXGI_FORMAT) -3)

#ifndef D3DX10INLINE
#ifdef _MSC_VER
  #if (_MSC_VER >= 1200)
  #define D3DX10INLINE __forceinline
  #else
  #define D3DX10INLINE __inline
  #endif
#else
  #ifdef __cplusplus
  #define D3DX10INLINE inline
  #else
  #define D3DX10INLINE
  #endif
#endif
#endif

// Includes
#include "d3d10.h"
#include "d3dx10.h"
#include "d3dx10math.h"
#include "d3dx10core.h"
#include "d3dx10tex.h"
#include "d3dx10mesh.h"
#include "d3dx10async.h"

// Errors
#define _FACDD  0x876
#define MAKE_DDHRESULT( code )  MAKE_HRESULT( 1, _FACDD, code )

enum _D3DX10_ERR {
    D3DX10_ERR_CANNOT_MODIFY_INDEX_BUFFER       = MAKE_DDHRESULT(2900),
    D3DX10_ERR_INVALID_MESH                     = MAKE_DDHRESULT(2901),
    D3DX10_ERR_CANNOT_ATTR_SORT                 = MAKE_DDHRESULT(2902),
    D3DX10_ERR_SKINNING_NOT_SUPPORTED           = MAKE_DDHRESULT(2903),
    D3DX10_ERR_TOO_MANY_INFLUENCES              = MAKE_DDHRESULT(2904),
    D3DX10_ERR_INVALID_DATA                     = MAKE_DDHRESULT(2905),
    D3DX10_ERR_LOADED_MESH_HAS_NO_DATA          = MAKE_DDHRESULT(2906),
    D3DX10_ERR_DUPLICATE_NAMED_FRAGMENT         = MAKE_DDHRESULT(2907),
    D3DX10_ERR_CANNOT_REMOVE_LAST_ITEM		    = MAKE_DDHRESULT(2908),
};

#endif //__D3DX10_H__
