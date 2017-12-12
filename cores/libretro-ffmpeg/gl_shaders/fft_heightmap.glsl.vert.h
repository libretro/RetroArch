#include "shaders_common.h"

static const char *fft_vertex_program_heightmap = GLSL_300(
   layout(location = 0) in vec2 aVertex;
   uniform sampler2D sHeight;
   uniform mat4 uMVP;
   uniform ivec2 uOffset;
   uniform vec4 uHeightmapParams;
   uniform float uAngleScale;
   out vec3 vWorldPos;
   out vec3 vHeight;

   void main() {
     vec2 tex_coord = vec2(aVertex.x + float(uOffset.x) + 0.5, -aVertex.y + float(uOffset.y) + 0.5) / vec2(textureSize(sHeight, 0));

     vec3 world_pos = vec3(aVertex.x, 0.0, aVertex.y);
     world_pos.xz += uHeightmapParams.xy;

     float angle = world_pos.x * uAngleScale;
     world_pos.xz *= uHeightmapParams.zw;

     float lod = log2(world_pos.z + 1.0) - 6.0;
     vec4 heights = textureLod(sHeight, tex_coord, lod);

     float cangle = cos(angle);
     float sangle = sin(angle);

     int c = int(-sign(world_pos.x) + 1.0);
     float height = mix(heights[c], heights[1], abs(angle) / 3.141592653);
     height = height * 80.0 - 40.0;

     vec3 up = vec3(-sangle, cangle, 0.0);

     float base_y = 80.0 - 80.0 * cangle;
     float base_x = 80.0 * sangle;
     world_pos.xy = vec2(base_x, base_y);
     world_pos += up * height;

     vWorldPos = world_pos;
     vHeight = vec3(height, heights.yw * 80.0 - 40.0);
     gl_Position = uMVP * vec4(world_pos, 1.0);
   }
);
