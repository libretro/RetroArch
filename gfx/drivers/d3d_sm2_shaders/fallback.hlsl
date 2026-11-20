/* Native raster fallbacks for devices without geometry/compute stages.
 * CPU-expanded quads feed sprites; downsampling/conversion can use pixel passes. */
#if SHADER_MODEL >= 4
#define VERTEX_POSITION SV_POSITION
#define PIXEL_COLOR SV_TARGET
Texture2D<float4> Source : register(t0);
SamplerState SourceSampler : register(s0);
#define SAMPLE(p) Source.Sample(SourceSampler, p)
#else
#define VERTEX_POSITION POSITION
#define PIXEL_COLOR COLOR0
sampler2D Source : register(s0);
#define SAMPLE(p) tex2D(Source, p)
#endif
float4x4 modelViewProj;
float2 OutputSize;
float time;
float4 p;
float4 views_c;
struct VertexOut { float4 position : VERTEX_POSITION; float2 uv : TEXCOORD0; float4 color : COLOR0; };
VertexOut VSMain(float4 position : POSITION, float2 uv : TEXCOORD0, float4 color : COLOR0)
{
   VertexOut result;
   result.position = mul(modelViewProj, position);
#if EFFECT_KIND == 1
   result.position = float4(position.xy, 0.0, 1.0);
   result.position.y += sin(position.x * 3.0 + time * 0.25) * 0.12;
#endif
   result.uv = uv; result.color = color;
   return result;
}
float4 PSMain(float2 uv : TEXCOORD0, float4 color : COLOR0) : PIXEL_COLOR
{
#if EFFECT_KIND == 1
   float rim = saturate(1.0 - abs(uv.y * 2.0 - 1.0));
   return float4(rim * rim * 0.18, rim * rim * 0.2, rim * rim * 0.25, 0.0);
#elif EFFECT_KIND == 2 || EFFECT_KIND == 3 || EFFECT_KIND == 4
   float2 cell = uv * float2(24.0, 16.0);
   cell.y += time * 0.18;
   float2 grid = floor(cell);
   float2 delta = frac(cell) - 0.5;
   float seed = frac(dot(grid, float2(0.75487766, 0.56984029)));
#if EFFECT_KIND == 3
   float light = saturate((0.22 - dot(delta, delta)) * 5.0);
#elif EFFECT_KIND == 4
   float light = saturate((0.12 - min(abs(delta.x), abs(delta.y))) * 8.0)
      * saturate((0.24 - dot(delta, delta)) * 5.0);
#else
   float light = saturate((0.06 - dot(delta, delta)) * 16.0);
#endif
   light *= seed;
   return float4(light, light, light, light);
#elif EFFECT_KIND == 5
   float3 value = max(SAMPLE(uv).rgb - p.z, 0.0) * 0.4;
   value += max(SAMPLE(uv + p.xy).rgb - p.z, 0.0) * 0.3;
   value += max(SAMPLE(uv - p.xy).rgb - p.z, 0.0) * 0.3;
   return float4(value * p.w, 0.0);
#elif EFFECT_KIND == 6
   float4 value = SAMPLE(uv) * color;
   value.rgb = max(value.rgb, 0.0) / (1.0 + max(value.rgb, 0.0));
   return value;
#elif EFFECT_KIND == 7
   float2 step = 0.5 / max(OutputSize, 1.0);
   return (SAMPLE(uv + step) + SAMPLE(uv - step)
      + SAMPLE(uv + float2(step.x, -step.y)) + SAMPLE(uv + float2(-step.x, step.y))) * 0.25;
#elif EFFECT_KIND == 8
   float y = SAMPLE(float2(uv.x, uv.y * 0.66666667)).r;
   float2 c = float2(uv.x * 0.5, 0.66666667 + uv.y * 0.33333333);
   float cb = SAMPLE(c).r - 0.5;
   float cr = SAMPLE(c + float2(0.5, 0.0)).r - 0.5;
   return float4(saturate(float3(y + 1.402 * cr, y - 0.344136 * cb - 0.714136 * cr, y + 1.772 * cb)), 1.0);
#else
   return SAMPLE(uv) * color;
#endif
}
float4 PSMainA8(float2 uv : TEXCOORD0, float4 color : COLOR0) : PIXEL_COLOR
{ return float4(color.rgb, color.a * SAMPLE(uv).r); }
float4 PSRect(float2 uv : TEXCOORD0) : PIXEL_COLOR
{ return float4(SAMPLE(clamp(uv, views_c.xy, views_c.zw)).rgb, 1.0); }
float4 PSLayer(float2 uv : TEXCOORD0) : PIXEL_COLOR { return SAMPLE(uv); }
float4 PSAnaglyph(float2 uv : TEXCOORD0) : PIXEL_COLOR
{
   float3 l = SAMPLE(float2(uv.x * 0.5, uv.y)).rgb;
   float3 r = SAMPLE(float2(uv.x * 0.5 + 0.5, uv.y)).rgb;
   return float4(saturate(float3(
      dot(l, float3(0.456100,0.500484,0.176381)) + dot(r,float3(-0.0434706,-0.0879388,-0.00155529)),
      dot(l, float3(-0.0400822,-0.0378246,-0.0157589)) + dot(r,float3(0.378476,0.73364,-0.0184503)),
      dot(l, float3(-0.0152161,-0.0205971,-0.00546856)) + dot(r,float3(-0.0721527,-0.112961,1.2264)))),1.0);
}
float4 PSInterlaced(float2 uv : TEXCOORD0) : PIXEL_COLOR
{
   float odd = frac(floor(uv.y * views_c.y) * 0.5) * 2.0;
   return SAMPLE(float2(uv.x * 0.5 + odd * 0.5, uv.y));
}
