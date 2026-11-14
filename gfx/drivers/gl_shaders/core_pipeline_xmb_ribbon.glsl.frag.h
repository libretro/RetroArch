#include "shaders_common.h"

static const char *core_stock_fragment_xmb = GLSL(
   in vec4 fragVertexEc;
   out vec4 FragColor;

   uniform float time;

   void main()
   {
      float c;
      if (fragVertexEc.z > 1.5 && fragVertexEc.w > 0.0)
      {
         float layer = fragVertexEc.z - 2.0;
         float middle = 1.0 - abs(layer - 1.0);
         float near = max(0.0, layer - 1.0);
         vec2 grid = vec2(110.0 - 35.0 * layer - 2.0 * near, 38.0 - 10.0 * layer - 3.0 * near);
         vec2 p = fragVertexEc.xy * grid - time * vec2(0.20 + 0.18 * layer, 0.05 - 0.08 * middle + 0.05 * near);
         vec2 cell = floor(p);
         float seed = fract(sin(dot(cell, vec2(1.271, 3.117))) * 43.13);
         c = 0.0;
         if (fract(seed * 17.17) >= 0.996 - 0.004 * middle)
         {
            float depth = fract(sin(dot(cell, vec2(4.113, 7.137))) * 31.73);
            float size = 0.065 + 0.015 * middle + 0.075 * near
                  + (0.055 + 0.115 * middle + 0.105 * near) * depth * depth;
            float phase = time * (0.12 + depth * 0.13) + seed * 42.0;
            vec2 offset = 0.5 + (0.46 - size) * vec2(sin(phase), cos(phase * 0.7));
            vec2 delta = fract(p) - offset;
            float star = max(0.0, 1.0 - dot(delta, delta) / (size * size));
            float pulse = sin(time * (0.45 + depth * 1.2) + seed * 61.0);
            float flutter = sin(time * (1.3 + fract(seed * 7.13) * 1.7) + depth * 45.0);
            float sparkle = max(0.0, pulse * flutter);
            float fade = max(0.0, 1.0 - fragVertexEc.y * fragVertexEc.y);
            sparkle *= sparkle;
            sparkle *= sparkle;
            star *= star;
            c = star * (0.30 + depth * 0.20 + 2.5 * sparkle) * (0.45 + 0.55 * middle - 0.12 * near) * fade * fade * fade;
         }
      }
      else
      {
         float rim = 1.0 - abs(fragVertexEc.x);
         float edge = rim * rim;
         edge *= edge;
         float fade = 1.0 - smoothstep(0.65, 1.0, abs(fragVertexEc.z));
         c = (0.035 + 1.55 * edge) * fade * smoothstep(0.25, 0.45, abs(fragVertexEc.y)) * fragVertexEc.w;
      }
      FragColor = vec4(1.0, 1.0, 1.0, c);
   }
);
