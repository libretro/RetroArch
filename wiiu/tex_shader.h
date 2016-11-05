#ifndef TEX_SHADER_H
#define TEX_SHADER_H
#include <gx2.h>
#include "system/memory.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __attribute__((aligned(GX2_VERTEX_BUFFER_ALIGNMENT)))
{
   GX2VertexShader vs;
   GX2PixelShader ps;
   GX2SamplerVar sampler;
   struct
   {
      GX2AttribVar position;
      GX2AttribVar tex_coord;
   } attributes;
   struct
   {
      GX2AttribStream position;
      GX2AttribStream tex_coord;
   } attribute_stream;
   GX2FetchShader fs;
}tex_shader_t;

extern tex_shader_t tex_shader;

#ifdef __cplusplus
}
#endif

#endif // TEX_SHADER_H
