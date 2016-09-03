#include "shaders_common.h"

static const char *fft_fragment_program_real = GLSL_300(
   precision mediump float;
   precision highp int;
   precision highp usampler2D;
   precision highp isampler2D;

   in vec2 vTex;
   uniform isampler2D sTexture;
   uniform usampler2D sParameterTexture;
   uniform usampler2D sWindow;
   uniform int uViewportOffset;
   out uvec2 FragColor;

   vec2 compMul(vec2 a, vec2 b) { return vec2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x); }

   void main() {
      uvec2 params = texture(sParameterTexture, vec2(vTex.x, 0.5)).rg;
      uvec2 coord  = uvec2((params.x >> 16u) & 0xffffu, params.x & 0xffffu);
      int ycoord   = int(gl_FragCoord.y) - uViewportOffset;
      vec2 twiddle = unpackHalf2x16(params.y);

      float window_a = float(texelFetch(sWindow, ivec2(coord.x, 0), 0).r) / float(0x10000);
      float window_b = float(texelFetch(sWindow, ivec2(coord.y, 0), 0).r) / float(0x10000);

      vec2 a = window_a * vec2(texelFetch(sTexture, ivec2(int(coord.x), ycoord), 0).rg) / vec2(0x8000);
      vec2 a_l = vec2(a.x, 0.0);
      vec2 a_r = vec2(a.y, 0.0);
      vec2 b = window_b * vec2(texelFetch(sTexture, ivec2(int(coord.y), ycoord), 0).rg) / vec2(0x8000);
      vec2 b_l = vec2(b.x, 0.0);
      vec2 b_r = vec2(b.y, 0.0);
      b_l = compMul(b_l, twiddle);
      b_r = compMul(b_r, twiddle);

      vec2 res_l = a_l + b_l;
      vec2 res_r = a_r + b_r;
      FragColor = uvec2(packHalf2x16(res_l), packHalf2x16(res_r));
   }
);
