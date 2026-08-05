#version 310 es
precision highp float;
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 1) uniform highp sampler2D uTex;
/* UI layer: menu, overlays, widgets and OSD, rendered into their own
 * cleared-to-transparent RGBA8 offscreen. Only bound when the content
 * layer is PQ (ui_mode != 0); otherwise it is the content texture again
 * and the shader never samples it. */
layout(set = 0, binding = 2) uniform highp sampler2D uUITex;

layout(set = 0, binding = 0, std140) uniform UBO
{
   mat4 MVP;
   /* x = content nits (paper white), y = expand gamut mode,
    * z = content is PQ Rec.2020 (0 = SDR gamma 2.4),
    * w = UI layer nits; <= 0 disables the separate UI composite. */
   vec4 hdr_params;
} global;

/* scRGB encode for the composited frame. The SDR branch mirrors the
 * other drivers' mode-2 composite (gamma 2.4 linearization, gamut
 * round-trip with the same expand-gamut matrix selection, then scale by
 * paper-white / 80 since scRGB 1.0 = 80 nits). The PQ branch is the
 * mode-3 composite: a core-supplied HDR10 frame is already Rec.2020 PQ
 * at absolute luminance, so it is decoded to nits and divided by 80
 * with no gamut rotation and no paper-white scaling - the core applied
 * both when it encoded. Constants match hdr_sm5.hlsl.h / hdr.frag. */

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

/* ST.2084 (PQ) -> normalized linear, the inverse of LinearToST2084 in
 * hdr_common.glsl; kept here so this shader stays self-contained. */
vec3 ST2084ToLinear(vec3 pq)
{
   vec3 p = pow(abs(pq), vec3(1.0 / 78.84375));
   vec3 n = max(p - 0.8359375, vec3(0.0));
   vec3 d = 18.8515625 - 18.6875 * p;
   return pow(abs(n / d), vec3(1.0 / 0.1593017578));
}

/* SDR gamma 2.4 -> linear scRGB at `nits` paper white. */
vec3 SDRToscRGB(vec3 rgb, float nits)
{
   vec3 lin = To2020(pow(abs(rgb), vec3(2.4)));
   lin      = k2020to709 * lin;
   return lin * (nits / 80.0);
}

void main()
{
   vec4 src = texture(uTex, vTexCoord);
   vec3 lin;

   if (global.hdr_params.z > 1.5)
   {
      /* Mode 2: PQ content, but the output is *not* HDR after all (the
       * frontend accepted HDR10 before it could know whether this
       * context would give an scRGB backbuffer). Decode to nits,
       * normalize against paper white, roll the overshoot off with the
       * same Reinhard shoulder the other drivers use, and re-encode to
       * gamma so the picture is merely tonemapped rather than wrong. */
      vec3 nits = ST2084ToLinear(src.rgb) * 10000.0;
      /* M * v: this file stores its matrices column-major for that
       * order (see the SDR path above), unlike hdr_common.glsl, whose
       * row-major storage pairs with v * M. Mixing the idioms applies
       * the transpose, which pushes whites red (row sums 1.52 / 0.44 /
       * 1.04) and crushes greens. */
      vec3 sdr  = (k2020to709 * nits) / max(global.hdr_params.x, 1.0);
      float pk  = max(sdr.r, max(sdr.g, sdr.b));
      if (pk > 1.0)
         sdr  /= pk;
      FragColor = vec4(pow(max(sdr, vec3(0.0)), vec3(1.0 / 2.4)), src.a);
      return;
   }
   else if (global.hdr_params.z > 0.5)
      /* HDR10 PQ Rec.2020 at absolute luminance. 1.0 normalized linear
       * is 10,000 nits and scRGB 1.0 is 80, hence the 125x scalar. */
      lin = (k2020to709 * ST2084ToLinear(src.rgb)) * (10000.0 / 80.0);
   else
      lin = SDRToscRGB(src.rgb, global.hdr_params.x);

   /* Composite the SDR UI over the content, in linear light and at its
    * own brightness. The layer was accumulated over a transparent clear
    * with ordinary src-alpha blending, so it is premultiplied: undo
    * that before the (non-linear) transfer function, then re-apply. */
   if (global.hdr_params.w > 0.0)
   {
      vec4 ui = texture(uUITex, vTexCoord);
      if (ui.a > 0.0)
      {
         vec3 ui_lin = SDRToscRGB(ui.rgb / ui.a, global.hdr_params.w) * ui.a;
         lin         = ui_lin + lin * (1.0 - ui.a);
      }
   }

   FragColor = vec4(lin, src.a);
}
