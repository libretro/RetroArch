/****************************************************************************
 * Copyright (C) 2015
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/
#ifndef _GX2_TYPES_H_
#define _GX2_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gctypes.h>

//!-----------------------------------------------------------------------------------------------------------------------
//! Constants
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_COMMAND_BUFFER_SIZE                         0x400000
#define GX2_SCAN_BUFFER_ALIGNMENT                       0x1000
#define GX2_SHADER_ALIGNMENT                            0x100
#define GX2_CONTEXT_STATE_ALIGNMENT                     0x100
#define GX2_DISPLAY_LIST_ALIGNMENT                      0x20
#define GX2_VERTEX_BUFFER_ALIGNMENT                     0x40
#define GX2_INDEX_BUFFER_ALIGNMENT                      0x20

#define GX2_CONTEXT_STATE_SIZE                          0xA100

#define GX2_AUX_BUFFER_CLEAR_VALUE                      0xCC

//!-----------------------------------------------------------------------------------------------------------------------
//! Common
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_FALSE                                       0
#define GX2_TRUE                                        1
#define GX2_DISABLE                                     0
#define GX2_ENABLE                                      1

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2InitAttrib
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_INIT_ATTRIB_NULL                            0
#define GX2_INIT_ATTRIB_CB_BASE                         1
#define GX2_INIT_ATTRIB_CB_SIZE                         2
#define GX2_INIT_ATTRIB_ARGC                            7
#define GX2_INIT_ATTRIB_ARGV                            8

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 compare functions
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_COMPARE_NEVER                               0
#define GX2_COMPARE_LESS                                1
#define GX2_COMPARE_EQUAL                               2
#define GX2_COMPARE_LEQUAL                              3
#define GX2_COMPARE_GREATER                             4
#define GX2_COMPARE_NOTEQUAL                            5
#define GX2_COMPARE_GEQUAL                              6
#define GX2_COMPARE_ALWAYS                              7

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 stencil functions
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_STENCIL_KEEP                                0
#define GX2_STENCIL_ZERO                                1
#define GX2_STENCIL_REPLACE                             2
#define GX2_STENCIL_INCR                                3
#define GX2_STENCIL_DECR                                4
#define GX2_STENCIL_INVERT                              5
#define GX2_STENCIL_INCR_WRAP                           6
#define GX2_STENCIL_DECR_WRAP                           7

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 logic op functions
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_LOGIC_OP_CLEAR                              0x00
#define GX2_LOGIC_OP_NOR                                0x11
#define GX2_LOGIC_OP_INVAND                             0x22
#define GX2_LOGIC_OP_INVCOPY                            0x33
#define GX2_LOGIC_OP_REVAND                             0x44
#define GX2_LOGIC_OP_INV                                0x55
#define GX2_LOGIC_OP_XOR                                0x66
#define GX2_LOGIC_OP_NAND                               0x77
#define GX2_LOGIC_OP_AND                                0x88
#define GX2_LOGIC_OP_EQUIV                              0x99
#define GX2_LOGIC_OP_NOOP                               0xAA
#define GX2_LOGIC_OP_INVOR                              0xBB
#define GX2_LOGIC_OP_COPY                               0xCC
#define GX2_LOGIC_OP_REVOR                              0xDD
#define GX2_LOGIC_OP_OR                                 0xEE
#define GX2_LOGIC_OP_SET                                0xFF

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 blend combination functions
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_BLEND_COMBINE_ADD                           0x00
#define GX2_BLEND_COMBINE_SRC_MINUS_DST                 0x01
#define GX2_BLEND_COMBINE_MIN                           0x02
#define GX2_BLEND_COMBINE_MAX                           0x03
#define GX2_BLEND_COMBINE_DST_MINUS_SRC                 0x04

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 blend functions
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_BLEND_ZERO                                  0x00
#define GX2_BLEND_ONE                                   0x01
#define GX2_BLEND_SRC_ALPHA                             0x04
#define GX2_BLEND_ONE_MINUS_SRC_ALPHA                   0x05

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 render targets
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_RENDER_TARGET_0                             0
#define GX2_RENDER_TARGET_1                             1
#define GX2_RENDER_TARGET_2                             2
#define GX2_RENDER_TARGET_3                             3
#define GX2_RENDER_TARGET_4                             4
#define GX2_RENDER_TARGET_5                             5
#define GX2_RENDER_TARGET_6                             6
#define GX2_RENDER_TARGET_7                             7

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 cull modes
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_FRONT_FACE_CCW                              0
#define GX2_FRONT_FACE_CW                               1
//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 polygon modes
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_POLYGON_MODE_POINT                          0
#define GX2_POLYGON_MODE_LINE                           1
#define GX2_POLYGON_MODE_TRIANGLE                       2

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 special states
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_SPECIAL_STATE_CLEAR                         0
#define GX2_SPECIAL_STATE_CLEAR_HIZ                     1
#define GX2_SPECIAL_STATE_COPY                          2
#define GX2_SPECIAL_STATE_EXPAND_COLOR                  3
#define GX2_SPECIAL_STATE_EXPAND_DEPTH                  4
#define GX2_SPECIAL_STATE_CONVERT_DEPTH                 5
#define GX2_SPECIAL_STATE_CONVERT_AADEPTH               6
#define GX2_SPECIAL_STATE_RESOLVE_COLOR                 7
#define GX2_SPECIAL_STATE_CLEAR_COLOR_AS_DEPTH          8

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 attribute formats
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_ATTRIB_FORMAT_8_UNORM                       0x00000000
#define GX2_ATTRIB_FORMAT_4_4_UNORM                     0x00000001
#define GX2_ATTRIB_FORMAT_16_UNORM                      0x00000002
#define GX2_ATTRIB_FORMAT_8_8_UNORM                     0x00000004
#define GX2_ATTRIB_FORMAT_16_16_UNORM                   0x00000007
#define GX2_ATTRIB_FORMAT_8_8_8_8_UNORM                 0x0000000A
#define GX2_ATTRIB_FORMAT_10_10_10_2_UNORM              0x0000000B
#define GX2_ATTRIB_FORMAT_16_16_16_16_UNORM             0x0000000E

#define GX2_ATTRIB_FORMAT_8_UINT                        0x00000100
#define GX2_ATTRIB_FORMAT_16_UINT                       0x00000102
#define GX2_ATTRIB_FORMAT_8_8_UINT                      0x00000104
#define GX2_ATTRIB_FORMAT_32_UINT                       0x00000105
#define GX2_ATTRIB_FORMAT_16_16_UINT                    0x00000107
#define GX2_ATTRIB_FORMAT_8_8_8_8_UINT                  0x0000010A
#define GX2_ATTRIB_FORMAT_10_10_10_2_UINT               0x0000010B
#define GX2_ATTRIB_FORMAT_32_32_UINT                    0x0000010C
#define GX2_ATTRIB_FORMAT_16_16_16_16_UINT              0x0000010E
#define GX2_ATTRIB_FORMAT_32_32_32_UINT                 0x00000110
#define GX2_ATTRIB_FORMAT_32_32_32_32_UINT              0x00000112

#define GX2_ATTRIB_FORMAT_8_SNORM                       0x00000200
#define GX2_ATTRIB_FORMAT_16_SNORM                      0x00000202
#define GX2_ATTRIB_FORMAT_8_8_SNORM                     0x00000204
#define GX2_ATTRIB_FORMAT_16_16_SNORM                   0x00000207
#define GX2_ATTRIB_FORMAT_8_8_8_8_SNORM                 0x0000020A
#define GX2_ATTRIB_FORMAT_10_10_10_2_SNORM              0x0000020B
#define GX2_ATTRIB_FORMAT_16_16_16_16_SNORM             0x0000020E

#define GX2_ATTRIB_FORMAT_8_SINT                        0x00000300
#define GX2_ATTRIB_FORMAT_16_SINT                       0x00000303
#define GX2_ATTRIB_FORMAT_8_8_SINT                      0x00000304
#define GX2_ATTRIB_FORMAT_32_SINT                       0x00000305
#define GX2_ATTRIB_FORMAT_16_16_SINT                    0x00000307
#define GX2_ATTRIB_FORMAT_8_8_8_8_SINT                  0x0000030A
#define GX2_ATTRIB_FORMAT_10_10_10_2_SINT               0x0000030B
#define GX2_ATTRIB_FORMAT_32_32_SINT                    0x0000030C
#define GX2_ATTRIB_FORMAT_16_16_16_16_SINT              0x0000030E
#define GX2_ATTRIB_FORMAT_32_32_32_SINT                 0x00000310
#define GX2_ATTRIB_FORMAT_32_32_32_32_SINT              0x00000312

#define GX2_ATTRIB_FORMAT_8_UINT_TO_FLOAT               0x00000800
#define GX2_ATTRIB_FORMAT_16_UINT_TO_FLOAT              0x00000802
#define GX2_ATTRIB_FORMAT_16_FLOAT                      0x00000803
#define GX2_ATTRIB_FORMAT_8_8_UINT_TO_FLOAT             0x00000804
#define GX2_ATTRIB_FORMAT_32_FLOAT                      0x00000806
#define GX2_ATTRIB_FORMAT_16_16_UINT_TO_FLOAT           0x00000807
#define GX2_ATTRIB_FORMAT_16_16_FLOAT                   0x00000808
#define GX2_ATTRIB_FORMAT_10_11_11_FLOAT                0x00000809
#define GX2_ATTRIB_FORMAT_8_8_8_8_UINT_TO_FLOAT         0x0000080A
#define GX2_ATTRIB_FORMAT_32_32_FLOAT                   0x0000080D
#define GX2_ATTRIB_FORMAT_16_16_16_16_UINT_TO_FLOAT     0x0000080E
#define GX2_ATTRIB_FORMAT_16_16_16_16_FLOAT             0x0000080F
#define GX2_ATTRIB_FORMAT_32_32_32_FLOAT                0x00000811
#define GX2_ATTRIB_FORMAT_32_32_32_32_FLOAT             0x00000813

#define GX2_ATTRIB_FORMAT_8_SINT_TO_FLOAT               0x00000A00
#define GX2_ATTRIB_FORMAT_16_SINT_TO_FLOAT              0x00000A02
#define GX2_ATTRIB_FORMAT_8_8_SINT_TO_FLOAT             0x00000A04
#define GX2_ATTRIB_FORMAT_16_16_SINT_TO_FLOAT           0x00000A07
#define GX2_ATTRIB_FORMAT_8_8_8_8_SINT_TO_FLOAT         0x00000A0A
#define GX2_ATTRIB_FORMAT_16_16_16_16_SINT_TO_FLOAT     0x00000A0E

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 shader modes
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_SHADER_MODE_UNIFORM_REGISTER                0
#define GX2_SHADER_MODE_UNIFORM_BLOCK                   1
#define GX2_SHADER_MODE_GEOMETRY_SHADER                 2
#define GX2_SHADER_MODE_COMPUTE_SHADER                  3

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 shader modes
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_COMP_SEL_NONE                               0x04040405
#define GX2_COMP_SEL_X001                               0x00040405
#define GX2_COMP_SEL_XY01                               0x00010405
#define GX2_COMP_SEL_XYZ1                               0x00010205
#define GX2_COMP_SEL_XYZW                               0x00010203
#define GX2_COMP_SEL_XXXX                               0x00000000
#define GX2_COMP_SEL_YYYY                               0x01010101
#define GX2_COMP_SEL_ZZZZ                               0x02020202
#define GX2_COMP_SEL_WWWW                               0x03030303
#define GX2_COMP_SEL_WZYX                               0x03020100
#define GX2_COMP_SEL_WXYZ                               0x03000102

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 variable types
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_VAR_TYPE_VOID                               0
#define GX2_VAR_TYPE_BOOL                               1
#define GX2_VAR_TYPE_INT                                2
#define GX2_VAR_TYPE_UINT                               3
#define GX2_VAR_TYPE_FLOAT                              4
#define GX2_VAR_TYPE_DOUBLE                             5
#define GX2_VAR_TYPE_VEC2                               9
#define GX2_VAR_TYPE_VEC3                               10
#define GX2_VAR_TYPE_VEC4                               11
#define GX2_VAR_TYPE_MAT2                               21
#define GX2_VAR_TYPE_MAT3                               25
#define GX2_VAR_TYPE_MAT4                               29

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 sample types
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_SAMPLER_TYPE_2D                             1


//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 index formats
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_INDEX_FORMAT_U16                            4
#define GX2_INDEX_FORMAT_U32                            9

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 primitive types
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_PRIMITIVE_POINTS                            0x01
#define GX2_PRIMITIVE_LINES                             0x02
#define GX2_PRIMITIVE_LINE_STRIP                        0x03
#define GX2_PRIMITIVE_TRIANGLES                         0x04
#define GX2_PRIMITIVE_TRIANGLE_FAN                      0x05
#define GX2_PRIMITIVE_TRIANGLE_STRIP                    0x06
#define GX2_PRIMITIVE_RECTS                             0x11
#define GX2_PRIMITIVE_QUADS                             0x13
#define GX2_PRIMITIVE_QUAD_STRIP                        0x14

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 clear modes
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_CLEAR_DEPTH                                 0x01
#define GX2_CLEAR_STENCIL                               0x02
#define GX2_CLEAR_BOTH                                  (GX2_CLEAR_DEPTH | GX2_CLEAR_STENCIL)

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 surface formats
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_SURFACE_FORMAT_TC_R8_UNORM                  0x00000001
#define GX2_SURFACE_FORMAT_T_R4_G4_UNORM                0x00000002
#define GX2_SURFACE_FORMAT_TCD_R16_UNORM                0x00000005
#define GX2_SURFACE_FORMAT_TC_R8_G8_UNORM               0x00000007
#define GX2_SURFACE_FORMAT_TCS_R5_G6_B5_UNORM           0x00000008
#define GX2_SURFACE_FORMAT_TC_R5_G5_B5_A1_UNORM         0x0000000a
#define GX2_SURFACE_FORMAT_TC_R4_G4_B4_A4_UNORM         0x0000000b
#define GX2_SURFACE_FORMAT_TC_A1_B5_G5_R5_UNORM         0x0000000c
#define GX2_SURFACE_FORMAT_TC_R16_G16_UNORM             0x0000000f
#define GX2_SURFACE_FORMAT_D_D24_S8_UNORM               0x00000011
#define GX2_SURFACE_FORMAT_T_R24_UNORM_X8               0x00000011
#define GX2_SURFACE_FORMAT_TCS_R10_G10_B10_A2_UNORM     0x00000019
#define GX2_SURFACE_FORMAT_TCS_R8_G8_B8_A8_UNORM        0x0000001a
#define GX2_SURFACE_FORMAT_TCS_A2_B10_G10_R10_UNORM     0x0000001b
#define GX2_SURFACE_FORMAT_TC_R16_G16_B16_A16_UNORM     0x0000001f
#define GX2_SURFACE_FORMAT_T_BC1_UNORM                  0x00000031
#define GX2_SURFACE_FORMAT_T_BC2_UNORM                  0x00000032
#define GX2_SURFACE_FORMAT_T_BC3_UNORM                  0x00000033
#define GX2_SURFACE_FORMAT_T_BC4_UNORM                  0x00000034
#define GX2_SURFACE_FORMAT_T_BC5_UNORM                  0x00000035
#define GX2_SURFACE_FORMAT_T_NV12_UNORM                 0x00000081

#define GX2_SURFACE_FORMAT_TC_R8_UINT                   0x00000101
#define GX2_SURFACE_FORMAT_TC_R16_UINT                  0x00000105
#define GX2_SURFACE_FORMAT_TC_R8_G8_UINT                0x00000107
#define GX2_SURFACE_FORMAT_TC_R32_UINT                  0x0000010d
#define GX2_SURFACE_FORMAT_TC_R16_G16_UINT              0x0000010f
#define GX2_SURFACE_FORMAT_T_X24_G8_UINT                0x00000111
#define GX2_SURFACE_FORMAT_TC_R10_G10_B10_A2_UINT       0x00000119
#define GX2_SURFACE_FORMAT_TC_R8_G8_B8_A8_UINT          0x0000011a
#define GX2_SURFACE_FORMAT_TC_A2_B10_G10_R10_UINT       0x0000011b
#define GX2_SURFACE_FORMAT_T_X32_G8_UINT_X24            0x0000011c
#define GX2_SURFACE_FORMAT_TC_R32_G32_UINT              0x0000011d
#define GX2_SURFACE_FORMAT_TC_R16_G16_B16_A16_UINT      0x0000011f
#define GX2_SURFACE_FORMAT_TC_R32_G32_B32_A32_UINT      0x00000122

#define GX2_SURFACE_FORMAT_TC_R8_SNORM                  0x00000201
#define GX2_SURFACE_FORMAT_TC_R16_SNORM                 0x00000205
#define GX2_SURFACE_FORMAT_TC_R8_G8_SNORM               0x00000207
#define GX2_SURFACE_FORMAT_TC_R16_G16_SNORM             0x0000020f
#define GX2_SURFACE_FORMAT_T_R10_G10_B10_A2_SNORM       0x00000219
#define GX2_SURFACE_FORMAT_TC_R10_G10_B10_A2_SNORM      0x00000219
#define GX2_SURFACE_FORMAT_TC_R8_G8_B8_A8_SNORM         0x0000021a
#define GX2_SURFACE_FORMAT_TC_R16_G16_B16_A16_SNORM     0x0000021f
#define GX2_SURFACE_FORMAT_T_BC4_SNORM                  0x00000234
#define GX2_SURFACE_FORMAT_T_BC5_SNORM                  0x00000235

#define GX2_SURFACE_FORMAT_TC_R8_SINT                   0x00000301
#define GX2_SURFACE_FORMAT_TC_R16_SINT                  0x00000305
#define GX2_SURFACE_FORMAT_TC_R8_G8_SINT                0x00000307
#define GX2_SURFACE_FORMAT_TC_R32_SINT                  0x0000030d
#define GX2_SURFACE_FORMAT_TC_R16_G16_SINT              0x0000030f
#define GX2_SURFACE_FORMAT_TC_R10_G10_B10_A2_SINT       0x00000319
#define GX2_SURFACE_FORMAT_TC_R8_G8_B8_A8_SINT          0x0000031a
#define GX2_SURFACE_FORMAT_TC_R32_G32_SINT              0x0000031d
#define GX2_SURFACE_FORMAT_TC_R16_G16_B16_A16_SINT      0x0000031f
#define GX2_SURFACE_FORMAT_TC_R32_G32_B32_A32_SINT      0x00000322

#define GX2_SURFACE_FORMAT_TCS_R8_G8_B8_A8_SRGB         0x0000041a
#define GX2_SURFACE_FORMAT_T_BC1_SRGB                   0x00000431
#define GX2_SURFACE_FORMAT_T_BC2_SRGB                   0x00000432
#define GX2_SURFACE_FORMAT_T_BC3_SRGB                   0x00000433

#define GX2_SURFACE_FORMAT_TC_R16_FLOAT                 0x00000806
#define GX2_SURFACE_FORMAT_TCD_R32_FLOAT                0x0000080e
#define GX2_SURFACE_FORMAT_TC_R16_G16_FLOAT             0x00000810
#define GX2_SURFACE_FORMAT_D_D24_S8_FLOAT               0x00000811
#define GX2_SURFACE_FORMAT_TC_R11_G11_B10_FLOAT         0x00000816
#define GX2_SURFACE_FORMAT_D_D32_FLOAT_S8_UINT_X24      0x0000081c
#define GX2_SURFACE_FORMAT_T_R32_FLOAT_X8_X24           0x0000081c
#define GX2_SURFACE_FORMAT_TC_R32_G32_FLOAT             0x0000081e
#define GX2_SURFACE_FORMAT_TC_R16_G16_B16_A16_FLOAT     0x00000820
#define GX2_SURFACE_FORMAT_TC_R32_G32_B32_A32_FLOAT     0x00000823

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 tile modes
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_TILE_MODE_DEFAULT                           0x00000000
#define GX2_TILE_MODE_LINEAR_ALIGNED                    0x00000001

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 surface use
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_SURFACE_USE_TEXTURE                         0x00000001
#define GX2_SURFACE_USE_COLOR_BUFFER                    0x00000002
#define GX2_SURFACE_USE_DEPTH_BUFFER                    0x00000004
#define GX2_SURFACE_USE_SCAN_BUFFER                     0x00000008
#define GX2_SURFACE_USE_FTV                             0x80000000
#define GX2_SURFACE_USE_COLOR_BUFFER_TEXTURE            (GX2_SURFACE_USE_COLOR_BUFFER | GX2_SURFACE_USE_TEXTURE)
#define GX2_SURFACE_USE_DEPTH_BUFFER_TEXTURE            (GX2_SURFACE_USE_DEPTH_BUFFER | GX2_SURFACE_USE_TEXTURE)
#define GX2_SURFACE_USE_COLOR_BUFFER_FTV                (GX2_SURFACE_USE_COLOR_BUFFER | GX2_SURFACE_USE_FTV)
#define GX2_SURFACE_USE_COLOR_BUFFER_TEXTURE_FTV        (GX2_SURFACE_USE_COLOR_BUFFER_TEXTURE | GX2_SURFACE_USE_FTV)

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 surface dim
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_SURFACE_DIM_1D                              0x00000000
#define GX2_SURFACE_DIM_2D                              0x00000001
#define GX2_SURFACE_DIM_3D                              0x00000002
#define GX2_SURFACE_DIM_CUBE                            0x00000003
#define GX2_SURFACE_DIM_1D_ARRAY                        0x00000004
#define GX2_SURFACE_DIM_2D_ARRAY                        0x00000005
#define GX2_SURFACE_DIM_2D_MSAA                         0x00000006
#define GX2_SURFACE_DIM_2D_MSAA_ARRAY                   0x00000007

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 AA modes
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_AA_MODE_1X                                  0x00000000
#define GX2_AA_MODE_2X                                  0x00000001
#define GX2_AA_MODE_4X                                  0x00000002
#define GX2_AA_MODE_8X                                  0x00000003

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 texture clamp
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_TEX_CLAMP_WRAP                              0x00000000
#define GX2_TEX_CLAMP_MIRROR                            0x00000001
#define GX2_TEX_CLAMP_CLAMP                             0x00000002
#define GX2_TEX_CLAMP_MIRROR_ONCE                       0x00000003
#define GX2_TEX_CLAMP_CLAMP_HALF_BORDER                 0x00000004
#define GX2_TEX_CLAMP_MIRROR_ONCE_HALF_BORDER           0x00000005
#define GX2_TEX_CLAMP_CLAMP_BORDER                      0x00000006
#define GX2_TEX_CLAMP_MIRROR_ONCE_BORDER                0x00000007

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 texture filter
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_TEX_XY_FILTER_POINT                         0x00000000
#define GX2_TEX_XY_FILTER_BILINEAR                      0x00000001

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 TV scan modes
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_TV_SCAN_MODE_NONE                           0x00000000
#define GX2_TV_SCAN_MODE_576I                           0x00000001
#define GX2_TV_SCAN_MODE_480I                           0x00000002
#define GX2_TV_SCAN_MODE_480P                           0x00000003
#define GX2_TV_SCAN_MODE_720P                           0x00000004
#define GX2_TV_SCAN_MODE_1080I                          0x00000006
#define GX2_TV_SCAN_MODE_1080P                          0x00000007

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 TV render modes
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_TV_RENDER_480_NARROW                        0x00000001
#define GX2_TV_RENDER_480_WIDE                          0x00000002
#define GX2_TV_RENDER_720                               0x00000003
#define GX2_TV_RENDER_1080                              0x00000005

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 DRC render modes
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_DRC_NONE                                    0x00000000
#define GX2_DRC_SINGLE                                  0x00000001
#define GX2_DRC_DOUBLE                                  0x00000002
#define GX2_DRC_SINGLE_30HZ                             0x00000004

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 buffering mode
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_BUFFERING_SINGLE                            0x00000001
#define GX2_BUFFERING_DOUBLE                            0x00000002
#define GX2_BUFFERING_TRIPLE                            0x00000003
#define GX2_BUFFERING_QUAD
//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 scan targets
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_SCAN_TARGET_TV                              0x00000001
#define GX2_SCAN_TARGET_DRC_FIRST                       0x00000004
#define GX2_SCAN_TARGET_DRC_SECOND                      0x00000008

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 invalidate types
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_INVALIDATE_ATTRIB_BUFFER                    0x00000001
#define GX2_INVALIDATE_TEXTURE                          0x00000002
#define GX2_INVALIDATE_UNIFORM_BLOCK                    0x00000004
#define GX2_INVALIDATE_SHADER                           0x00000008
#define GX2_INVALIDATE_COLOR_BUFFER                     0x00000010
#define GX2_INVALIDATE_DEPTH_BUFFER                     0x00000020
#define GX2_INVALIDATE_CPU                              0x00000040
#define GX2_INVALIDATE_CPU_ATTRIB_BUFFER                (GX2_INVALIDATE_CPU | GX2_INVALIDATE_ATTRIB_BUFFER)
#define GX2_INVALIDATE_CPU_TEXTURE                      (GX2_INVALIDATE_CPU | GX2_INVALIDATE_TEXTURE)
#define GX2_INVALIDATE_CPU_UNIFORM_BLOCK                (GX2_INVALIDATE_CPU | GX2_INVALIDATE_UNIFORM_BLOCK)
#define GX2_INVALIDATE_CPU_SHADER                       (GX2_INVALIDATE_CPU | GX2_INVALIDATE_SHADER)

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 swap modes
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_ENDIANSWAP_DEFAULT                          0x00000003

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 tessellation modes
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_TESSELLATION_MODE_DISCRETE                  0x00000000
#define GX2_TESSELLATION_MODE_CONTINUOUS                0x00000001
#define GX2_TESSELLATION_MODE_ADAPTIVE                  0x00000002

//!-----------------------------------------------------------------------------------------------------------------------
//! GX2 fetch shader types
//!-----------------------------------------------------------------------------------------------------------------------
#define GX2_FETCH_SHADER_TESSELATION_NONE               0x00000000
#define GX2_FETCH_SHADER_TESSELATION_LINES              0x00000001
#define GX2_FETCH_SHADER_TESSELATION_TRIANGLES          0x00000002
#define GX2_FETCH_SHADER_TESSELATION_QUADS              0x00000003


typedef struct _GX2ContextState {
    u8 data[GX2_CONTEXT_STATE_SIZE];
} GX2ContextState;

typedef struct _GX2Surface {
    s32 dimension;
    u32 width;
    u32 height;
    u32 depth;
    u32 num_mips;
    s32 format;
    s32 aa;
    s32 use;
    u32 image_size;
    void *image_data;
    u32 mip_size;
    void *mip_data;
    s32 tile;
    u32 swizzle;
    u32 align;
    u32 pitch;
    u32 mip_offset[13];
} GX2Surface;

typedef struct _GX2ColorBuffer {
    GX2Surface surface;
    u32 view_mip;
    u32 view_first_slice;
    u32 view_slices_count;
    void *aux_data;
    u32  aux_size;
    u32 regs[5];
} GX2ColorBuffer;

typedef struct _GX2DepthBuffer {
    GX2Surface surface;
    u32 view_mip;
    u32 view_first_slice;
    u32 view_slices_count;
    void *hiZ_data;
    u32  hiZ_size;
    f32 clear_depth;
    u32 clear_stencil;
    u32 regs[7];
} GX2DepthBuffer;


typedef struct _GX2Texture {
    GX2Surface surface;
    u32 view_first_mip;
    u32 view_mips_count;
    u32 view_first_slice;
    u32 view_slices_count;
    u32 component_selector;
    u32 regs[5];
} GX2Texture;


typedef struct _GX2Sampler {
    u32 regs[3];
} GX2Sampler;

typedef struct _GX2AttribStream {
    u32 location;
    u32 buffer;
    u32 offset;
    s32 format;
    s32 index_type;
    u32 divisor;
    u32 destination_selector;
    s32 endian_swap;
} GX2AttribStream;

typedef struct _GX2FetchShader {
    s32 type;
    u32 reg;
    u32 shader_size;
    void *shader_program;
    u32 attributes_count;
    u32 divisor[3];
} GX2FetchShader;

typedef struct _GX2AttribVar
{
    const char *name;
    s32 var_type;
    u32 array_count;
    u32 location;
} GX2AttribVar;


typedef struct _GX2UniformBlock {
    const char *name;
    u32 location;
    u32 block_size;
} GX2UniformBlock;

typedef struct _GX2UniformInitialValue {
    f32 value[4];
    u32 offset;
} GX2UniformInitialValue;

typedef struct _GX2SamplerVar
{
    const char *name;
    s32 sampler_type;
    u32 location;
} GX2SamplerVar;

typedef struct _GX2UniformVar
{
    const char *name;
    s32 var_type;
    u32 array_count;
    u32 offset;
    u32 block_index;
} GX2UniformVar;

typedef struct _GX2VertexShader {
    u32 regs[52];
    u32 shader_size;
    void *shader_data;
    s32 shader_mode;
    u32 uniform_blocks_count;
    GX2UniformBlock *uniform_block;
    u32 uniform_vars_count;
    GX2UniformVar *uniform_var;
    u32 initial_values_count;
    GX2UniformInitialValue *initial_value;
    u32 loops_count;
    void *loops_data;
    u32 sampler_vars_count;
    GX2SamplerVar *sampler_var;
    u32 attribute_vars_count;
    GX2AttribVar *attribute_var;
    u32 data[6];
    u32 shader_program_buffer[16];
} GX2VertexShader;

typedef struct _GX2PixelShader {
    u32 regs[41];
    u32 shader_size;
    void *shader_data;
    s32 shader_mode;
    u32 uniform_blocks_count;
    GX2UniformBlock *uniform_block;
    u32 uniform_vars_count;
    GX2UniformVar *uniform_var;
    u32 initial_values_count;
    GX2UniformInitialValue *initial_value;
    u32 loops_count;
    void *loops_data;
    u32 sampler_vars_count;
    GX2SamplerVar *sampler_var;
    u32 shader_program_buffer[16];
} GX2PixelShader;

static const u32 attribute_dest_comp_selector[20] = {
    GX2_COMP_SEL_X001, GX2_COMP_SEL_XY01, GX2_COMP_SEL_X001, GX2_COMP_SEL_X001,  GX2_COMP_SEL_XY01, GX2_COMP_SEL_X001,
    GX2_COMP_SEL_X001, GX2_COMP_SEL_XY01, GX2_COMP_SEL_XY01, GX2_COMP_SEL_XYZ1, GX2_COMP_SEL_XYZW, GX2_COMP_SEL_XYZW,
    GX2_COMP_SEL_XY01, GX2_COMP_SEL_XY01, GX2_COMP_SEL_XYZW, GX2_COMP_SEL_XYZW, GX2_COMP_SEL_XYZ1, GX2_COMP_SEL_XYZ1,
    GX2_COMP_SEL_XYZW, GX2_COMP_SEL_XYZW
};

static const u32 texture_comp_selector[54] = {
    GX2_COMP_SEL_NONE, GX2_COMP_SEL_X001, GX2_COMP_SEL_XY01, GX2_COMP_SEL_NONE, GX2_COMP_SEL_NONE, GX2_COMP_SEL_X001,
    GX2_COMP_SEL_X001, GX2_COMP_SEL_XY01, GX2_COMP_SEL_XYZ1, GX2_COMP_SEL_XYZ1, GX2_COMP_SEL_XYZW, GX2_COMP_SEL_XYZW,
    GX2_COMP_SEL_WZYX, GX2_COMP_SEL_X001, GX2_COMP_SEL_X001, GX2_COMP_SEL_XY01, GX2_COMP_SEL_XY01, GX2_COMP_SEL_NONE,
    GX2_COMP_SEL_NONE, GX2_COMP_SEL_NONE, GX2_COMP_SEL_NONE, GX2_COMP_SEL_NONE, GX2_COMP_SEL_XYZ1, GX2_COMP_SEL_NONE,
    GX2_COMP_SEL_NONE, GX2_COMP_SEL_XYZW, GX2_COMP_SEL_XYZW, GX2_COMP_SEL_WZYX, GX2_COMP_SEL_XY01, GX2_COMP_SEL_XY01,
    GX2_COMP_SEL_XY01, GX2_COMP_SEL_XYZW, GX2_COMP_SEL_XYZW, GX2_COMP_SEL_NONE, GX2_COMP_SEL_XYZW, GX2_COMP_SEL_XYZW,
    GX2_COMP_SEL_NONE, GX2_COMP_SEL_NONE, GX2_COMP_SEL_NONE, GX2_COMP_SEL_XYZ1, GX2_COMP_SEL_XYZ1, GX2_COMP_SEL_X001,
    GX2_COMP_SEL_XY01, GX2_COMP_SEL_XYZ1, GX2_COMP_SEL_NONE, GX2_COMP_SEL_NONE, GX2_COMP_SEL_NONE, GX2_COMP_SEL_XYZ1,
    GX2_COMP_SEL_XYZ1, GX2_COMP_SEL_XYZW, GX2_COMP_SEL_XYZW, GX2_COMP_SEL_XYZW, GX2_COMP_SEL_X001, GX2_COMP_SEL_XY01
};

typedef struct _GX2Color {
    u8 r, g, b, a;
} GX2Color;

typedef struct _GX2ColorF32 {
    f32 r, g, b, a;
} GX2ColorF32;

#ifdef __cplusplus
}
#endif

#endif
