
#define SRC(...) #__VA_ARGS__
SRC(
struct UBO
{
   float4x4 modelViewProj;
   float2 OutputSize;
   float time;
};
uniform UBO global;

struct PSInput
{
   float4 position : SV_POSITION;
   float2 texcoord : TEXCOORD0;
};

PSInput VSMain(float4 position : POSITION, float2 texcoord : TEXCOORD0)
{
   PSInput result;
   result.position = mul(global.modelViewProj, position);
   result.texcoord = texcoord;
   return result;
}

static const float baseScale = 1.25;  //  [1.0  .. 10.0]
static const float density   = 0.5;  //  [0.01 ..  1.0]
static const float speed     = 0.15; //  [0.1  ..  1.0]

float rand(float2 co)
{
   return frac(sin(dot(co.xy, float2(12.9898, 78.233))) * 43758.5453);
}

float dist_func(float2 distv)
{
   float dist = sqrt((distv.x * distv.x) + (distv.y * distv.y)) * (40.0 / baseScale);
   dist = clamp(dist, 0.0, 1.0);
   return cos(dist * (3.14159265358 * 0.5)) * 0.5;
}

float random_dots(float2 co)
{
   float part = 1.0 / 20.0;
   float2 cd = floor(co / part);
   float p = rand(cd);

   if (p > 0.005 * (density * 40.0))
      return 0.0;

   float2 dpos = (float2(frac(p * 2.0) , p) + float2(2.0, 2.0)) * 0.25;

   float2 cellpos = frac(co / part);
   float2 distv = (cellpos - dpos);

   return dist_func(distv);
}

float snow(float2 pos, float time, float scale)
{
   // add wobble
   pos.x += cos(pos.y * 1.2 + time * 3.14159 * 2.0 + 1.0 / scale) / (8.0 / scale) * 4.0;
   // add gravity
   pos += time * scale * float2(-0.5, 1.0) * 4.0;
   return random_dots(pos / scale) * (scale * 0.5 + 0.5);
}

float4 PSMain(PSInput input) : SV_TARGET
{
   float tim = global.time * 0.4 * speed;
   float2 pos = input.position.xy / global.OutputSize.xx;
   pos.y = 1.0 - pos.y; // Flip Y
   float a = 0.0;
   // Each of these is a layer of snow
   // Remove some for better performance
   // Changing the scale (3rd value) will mess with the looping
   a += snow(pos, tim, 1.0);
   a += snow(pos, tim, 0.7);
   a += snow(pos, tim, 0.6);
   a += snow(pos, tim, 0.5);
   a += snow(pos, tim, 0.4);
   a += snow(pos, tim, 0.3);
   a += snow(pos, tim, 0.25);
   a += snow(pos, tim, 0.125);
   a = a * min(pos.y * 4.0, 1.0);
   return float4(1.0, 1.0, 1.0, a);
};
)
