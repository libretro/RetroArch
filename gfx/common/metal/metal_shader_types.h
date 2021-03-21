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

#endif
