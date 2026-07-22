#version 310 es
precision highp float;
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 1) uniform highp sampler2D uTex;

layout(set = 0, binding = 0, std140) uniform UBO
{
   mat4 MVP;
   vec4 hdr_params; /* x = paper white nits, y = expand gamut mode */
} global;

/* scRGB encode for an SDR-composited frame: mirror of the SDR branch of
 * the other drivers' mode-2 composite (gamma 2.4 linearization, gamut
 * round-trip with the same expand-gamut matrix selection, then scale by
 * paper-white / 80 since scRGB 1.0 = 80 nits). Constants match
 * hdr_sm5.hlsl.h / hdr.frag. */

const mat3 k709to2020 = mat3(
   0.6274040, 0.0690970, 0.0163916,
   0.3292820, 0.9195400, 0.0880132,
   0.0433136, 0.0113612, 0.8955950);

const mat3 kExpanded709to2020 = mat3(
   0.6274040, 0.0457456, -0.00121055,
   0.3292820, 0.9417770,  0.0176041,
   0.0433136, 0.0124772,  0.9836070);

const mat3 kP3to2020 = mat3(
   0.753833,  0.045744, -0.001210,
   0.198597,  0.941777,  0.017602,
   0.047570,  0.012479,  0.983609);

const mat3 k2020to709 = mat3(
    1.6604910, -0.1245505, -0.0181508,
   -0.5876411,  1.1328999, -0.1005789,
   -0.0728499, -0.0083494,  1.1187297);

vec3 To2020(vec3 rgb)
{
   float expand = global.hdr_params.y;
   vec3 result;
   if (expand < 0.5)
      result = k709to2020 * rgb;
   else if (expand < 1.5)
      result = kExpanded709to2020 * rgb;
   else if (expand < 2.5)
      result = kP3to2020 * rgb;
   else
      result = rgb;
   return max(result, vec3(0.0));
}

void main()
{
   vec4 sdr = texture(uTex, vTexCoord);
   vec3 lin = To2020(pow(abs(sdr.rgb), vec3(2.4)));
   lin      = k2020to709 * lin;
   lin     *= global.hdr_params.x / 80.0;
   FragColor = vec4(lin, sdr.a);
}
