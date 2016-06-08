#include "shaders_common.h"

static const char *stock_cg_gl_program = CG(
      struct input
      {
        float2 tex_coord;
        float4 color;
        float4 vertex_coord;
        uniform float4x4 mvp_matrix;
        uniform sampler2D texture;
      };

      struct vertex_data
      {
        float2 tex;
        float4 color;
      };

      void main_vertex
      (
        out float4 oPosition : POSITION,
        input IN,
        out vertex_data vert
      )
      {
        oPosition = mul(IN.mvp_matrix, IN.vertex_coord);
        vert = vertex_data(IN.tex_coord, IN.color);
      }

      float4 main_fragment(input IN, vertex_data vert, uniform sampler2D s0 : TEXUNIT0) : COLOR
      {
        return vert.color * tex2D(s0, vert.tex);
      }
);
