/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018      - Stuart Carnie
 *  copyright (c) 2011-2021 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 *  Header containing types and enum constants shared between Metal shaders and Swift/ObjC source
 */

#ifndef METAL_SHADER_TYPES_H
#define METAL_SHADER_TYPES_H

#ifdef __METAL_VERSION__
#define NS_ENUM(_type, _name) enum _name : _type _name; enum _name : _type
#define NSInteger metal::int32_t

#define METAL_ATTRIBUTE(x) [[attribute(x)]]
#define METAL_POSITION [[position]]
#else
#import <Foundation/Foundation.h>
#define METAL_ATTRIBUTE(x)
#define METAL_POSITION
#endif

#include <simd/simd.h>

typedef NS_ENUM(NSInteger, BufferIndex)
{
   BufferIndexPositions = 0,
   BufferIndexUniforms = 1
};

typedef NS_ENUM(NSInteger, VertexAttribute)
{
   VertexAttributePosition = 0,
   VertexAttributeTexcoord = 1,
   VertexAttributeColor = 2,
};

typedef NS_ENUM(NSInteger, TextureIndex)
{
   TextureIndexColor = 0,
};

typedef NS_ENUM(NSInteger, SamplerIndex)
{
   SamplerIndexDraw = 0,
};

typedef struct
{
   vector_float3 position METAL_ATTRIBUTE(VertexAttributePosition);
   vector_float2 texCoord METAL_ATTRIBUTE(VertexAttributeTexcoord);
} Vertex;

typedef struct
{
   vector_float4 position;
   vector_float2 texCoord;
} VertexSlang;

typedef struct
{
   vector_float4 position METAL_POSITION;
   vector_float2 texCoord;
} ColorInOut;

typedef struct
{
   matrix_float4x4 projectionMatrix;
   vector_float2 outputSize;
   float time;
} Uniforms;

typedef struct
{
   vector_float2 position  METAL_ATTRIBUTE(VertexAttributePosition);
   vector_float2 texCoord  METAL_ATTRIBUTE(VertexAttributeTexcoord);
   vector_float4 color     METAL_ATTRIBUTE(VertexAttributeColor);
} SpriteVertex;

typedef struct
{
   vector_float4 position METAL_POSITION;
   vector_float2 texCoord;
   vector_float4 color;
} FontFragmentIn;

/* HDR composite pass UBO.
 * Layout must match the MSL fragment shader (hdr_composite_fragment,
 * hdr_tonemap_fragment) and the C++-side HDRUniforms in metal.m.
 * Keep field order and types in sync across all three.
 *
 * HDRMode values:
 *   0 = off (SDR passthrough)
 *   1 = HDR10  (PQ / BT.2020 / 10-bit swapchain)
 *   2 = scRGB  (linear / BT.709 / 16F swapchain, 1.0 = 80 nits)
 *   3 = PQ->scRGB (shader-emitted PQ, convert to scRGB for 16F swapchain)
 */
typedef struct
{
   matrix_float4x4 mvp;
   vector_float4   SourceSize;       /* xy = size, zw = 1/size */
   vector_float4   OutputSize;       /* xy = size, zw = 1/size */
   vector_float4   CoreViewport;     /* xy = origin, zw = size (pixels, drawable-space) */
   float           BrightnessNits;   /* core paper-white in nits             */
   unsigned int    SubpixelLayout;   /* 0=RGB, 1=RBG, 2=BGR                 */
   float           Scanlines;        /* >0 enables CRT scanline/mask pass   */
   unsigned int    ExpandGamut;      /* 0=accurate, 1=expanded709, 2=P3, 3=super */
   float           InverseTonemap;   /* >0 applies SDR->HDR inverse tonemap */
   float           HDR10;            /* >0 applies linear->PQ encode        */
   unsigned int    HDRMode;          /* 0 off, 1 HDR10, 2 scRGB, 3 PQ->scRGB */
   float           PaperWhiteNits;   /* UI paper-white for SDR overlay blend */
} HDRUniforms;

#endif
