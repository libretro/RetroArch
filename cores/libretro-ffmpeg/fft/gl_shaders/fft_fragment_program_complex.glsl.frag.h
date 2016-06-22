#include "shaders_common.h"

static const char *fft_fragment_program_complex = GLSL_300(
   precision mediump float;
   precision highp int;
   precision highp usampler2D;
   precision highp isampler2D;

   in vec2 vTex;
   uniform usampler2D sTexture;
   uniform usampler2D sParameterTexture;
   uniform int uViewportOffset;
   out uvec2 FragColor;

   vec2 compMul(vec2 a, vec2 b) { return vec2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x); }

   void main() {
      uvec2 params = texture(sParameterTexture, vec2(vTex.x, 0.5)).rg;
      uvec2 coord  = uvec2((params.x >> 16u) & 0xffffu, params.x & 0xffffu);
      int ycoord   = int(gl_FragCoord.y) - uViewportOffset;
      vec2 twiddle = unpackHalf2x16(params.y);

      uvec2 x = texelFetch(sTexture, ivec2(int(coord.x), ycoord), 0).rg;
      uvec2 y = texelFetch(sTexture, ivec2(int(coord.y), ycoord), 0).rg;
      vec4 a = vec4(unpackHalf2x16(x.x), unpackHalf2x16(x.y));
      vec4 b = vec4(unpackHalf2x16(y.x), unpackHalf2x16(y.y));
      b.xy = compMul(b.xy, twiddle);
      b.zw = compMul(b.zw, twiddle);

      vec4 res = a + b;
      FragColor = uvec2(packHalf2x16(res.xy), packHalf2x16(res.zw));
   }
);
