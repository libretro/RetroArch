
static const char *hlsl_ribbon_program = CG(
   uniform float time;
   uniform float alpha;

   void main_vertex(
      float2 position : POSITION,
      out float4 oPosition : POSITION,
      out float4 vEC : TEXCOORD0)
   {
      float x = position.x;
      float row = floor((position.y + 1.0) * 31.5 + 0.5);
      float z = min(row, 53.0) * (2.0 / 53.0) - 1.0;
      float a = x * 1.45 + time * 0.08;
      float b = z * 1.75 + x * (1.2 + 0.45 * z) - time * 0.06;
      float c = z * (5.8 + 1.2 * z) + x * 0.75 - time * 0.09;
      float d = z * 10.0 + x * (2.2 - 0.6 * z) + time * 0.075;
      float width = 0.13 + 0.05 * cos(a);
      float y = -0.28 + 0.12 * sin(a) + width * cos(b) * cos(c)
            + 0.04 * sin(d) + 0.06 * z;
      float dx = 0.174 * cos(a) - 0.0725 * sin(a) * cos(b) * cos(c)
            - width * ((1.2 + 0.45 * z) * sin(b) * cos(c) + 0.75 * cos(b) * sin(c))
            + 0.04 * (2.2 - 0.6 * z) * cos(d);
      float dz = -width * ((1.75 + 0.45 * x) * sin(b) * cos(c)
            + (5.8 + 2.4 * z) * cos(b) * sin(c))
            + 0.04 * (10.0 - 0.6 * x) * cos(d) + 0.06;
      float stripes = 0.65 + 0.35 * cos(z * 28.0);
      float w = 1.0 + 0.10 * z;
      float3 normal = float3(dx, -1.12, 1.12 * dz);
      float3 eye = float3(-0.112 * x, -0.10 * y, -w);
      float angle = dot(normal, eye) / sqrt(dot(normal, normal) * dot(eye, eye));
      vEC = float4(angle, (dz * w - 0.10 * y) / (w * w), z,
            stripes * (1.0 - step(52.5, row)));
      oPosition = float4(1.12 * x, y, 0.0, w);
      if (row > 54.5)
      {
         float layer = floor((row - 55.0) / 3.0);
         float t = row - 56.0 - 3.0 * layer;
         vEC = float4(x, t, 2.0 + layer, 1.0 - abs(t));
         oPosition = float4(x, -0.28 + 0.06 * sin(a) + t * (0.70 + 0.12 * layer), 0.0, 1.0);
      }
   }

   struct output
   {
      float4 color : COLOR;
   };

   output main_fragment(float4 vEC : TEXCOORD0)
   {
      output OUT;
      float c;
      if (vEC.z > 1.5 && vEC.w > 0.0)
      {
         float layer = vEC.z - 2.0;
         float middle = 1.0 - abs(layer - 1.0);
         float near = max(0.0, layer - 1.0);
         float2 grid = float2(110.0 - 35.0 * layer - 2.0 * near, 38.0 - 10.0 * layer - 3.0 * near);
         float2 p = vEC.xy * grid - time * float2(0.20 + 0.18 * layer, 0.05 - 0.08 * middle + 0.05 * near);
         float2 cell = floor(p);
         float seed = frac(sin(dot(cell, float2(1.271, 3.117))) * 43.13);
         c = 0.0;
         if (frac(seed * 17.17) >= 0.996 - 0.004 * middle)
         {
            float depth = frac(sin(dot(cell, float2(4.113, 7.137))) * 31.73);
            float size = 0.065 + 0.015 * middle + 0.075 * near
                  + (0.055 + 0.115 * middle + 0.105 * near) * depth * depth;
            float phase = time * (0.12 + depth * 0.13) + seed * 42.0;
            float2 offset = 0.5 + (0.46 - size) * float2(sin(phase), cos(phase * 0.7));
            float2 delta = frac(p) - offset;
            float star = max(0.0, 1.0 - dot(delta, delta) / (size * size));
            float pulse = sin(time * (0.45 + depth * 1.2) + seed * 61.0);
            float flutter = sin(time * (1.3 + frac(seed * 7.13) * 1.7) + depth * 45.0);
            float sparkle = max(0.0, pulse * flutter);
            float fade = max(0.0, 1.0 - vEC.y * vEC.y);
            sparkle *= sparkle;
            sparkle *= sparkle;
            star *= star;
            c = star * (0.30 + depth * 0.20 + 2.5 * sparkle) * (0.45 + 0.55 * middle - 0.12 * near) * fade * fade * fade;
         }
      }
      else
      {
         float rim = 1.0 - abs(vEC.x);
         float edge = rim * rim;
         edge *= edge;
         float fade = 1.0 - smoothstep(0.65, 1.0, abs(vEC.z));
         c = (0.035 + 1.55 * edge) * fade * smoothstep(0.25, 0.45, abs(vEC.y)) * vEC.w;
      }
      OUT.color = float4(1.0, 1.0, 1.0, c * alpha);
      return OUT;
   }
);
