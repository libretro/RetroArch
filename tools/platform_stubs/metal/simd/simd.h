/* Stand-in for <simd/simd.h>: the vector and matrix types the Metal
 * driver uses, as clang's extended vectors, which is what they are. */
#ifndef STUB_SIMD_H
#define STUB_SIMD_H
typedef float simd_float2 __attribute__((ext_vector_type(2)));
typedef float simd_float3 __attribute__((ext_vector_type(3)));
typedef float simd_float4 __attribute__((ext_vector_type(4)));
typedef simd_float2 vector_float2;
typedef simd_float3 vector_float3;
typedef simd_float4 vector_float4;
typedef struct { simd_float4 columns[4]; } simd_float4x4;
typedef simd_float4x4 matrix_float4x4;
static inline simd_float2 simd_make_float2(float x, float y) { simd_float2 v = { x, y }; return v; }
static inline simd_float3 simd_make_float3(float x, float y, float z) { simd_float3 v = { x, y, z }; return v; }
static inline simd_float4 simd_make_float4(float x, float y, float z, float w) { simd_float4 v = { x, y, z, w }; return v; }
simd_float4x4 simd_mul(simd_float4x4 a, simd_float4x4 b);
#endif
