#pragma once

#include <wiiu/gx2/shaders.h>

/* incompatible with elf builds */
/* #define GX2_CAN_ACCESS_DATA_SECTION */

#ifdef __cplusplus
extern "C" {
#endif

typedef union
__attribute__((aligned (16)))
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
   union
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

typedef union
{
   struct
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

void check_shader(const void* shader_, u32 shader_size, const void* org_, u32 org_size, const char* name);
void check_shader_verbose(u32* shader, u32 shader_size, u32* org, u32 org_size, const char* name);

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
