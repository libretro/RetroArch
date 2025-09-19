#ifndef _GX2_SHADER_UTILS_H
#define _GX2_SHADER_UTILS_H

#include <wiiu/types.h>
#include <gx2/ra_shaders.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef union
__attribute__((aligned (16)))
__attribute__((scalar_storage_order ("little-endian")))
{
   struct __attribute__((scalar_storage_order ("little-endian")))
   {
      float x;
      float y;
   };
   struct __attribute__((scalar_storage_order ("little-endian")))
   {
      float width;
      float height;
   };
}GX2_vec2;

typedef struct
__attribute__((aligned (16)))
__attribute__((scalar_storage_order ("little-endian")))
{
   float x;
   float y;
   union __attribute__((scalar_storage_order ("little-endian")))
   {
      struct __attribute__((scalar_storage_order ("little-endian")))
      {
         float z;
         float w;
      };
      struct __attribute__((scalar_storage_order ("little-endian")))
      {
         float width;
         float height;
      };
   };
}GX2_vec4;

typedef union __attribute__((scalar_storage_order ("little-endian")))
{
   struct __attribute__((scalar_storage_order ("little-endian")))
   {
      GX2_vec4 v0;
      GX2_vec4 v1;
      GX2_vec4 v2;
      GX2_vec4 v3;
   };
   struct __attribute__((scalar_storage_order ("little-endian")))
   {
      float data[16];
   };
}GX2_mat4x4;

typedef struct
{
   GX2VertexShader vs;
   GX2PixelShader ps;
   GX2GeometryShader gs;
   GX2FetchShader fs;
   GX2AttribStream* attribute_stream;
}GX2Shader;

void GX2InitShader(GX2Shader* shader);
void GX2DestroyShader(GX2Shader* shader);
void GX2SetShader(GX2Shader* shader);

typedef struct
{
   GX2VertexShader* vs;
   GX2PixelShader* ps;
   u8* data;
} GFDFile;

GFDFile* gfd_open(const char* filename);
void gfd_free(GFDFile* gfd);

#ifdef __cplusplus
}
#endif

#endif
