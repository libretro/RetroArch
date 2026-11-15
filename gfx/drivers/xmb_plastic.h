#ifndef XMB_PLASTIC_H
#define XMB_PLASTIC_H

#define XMB_PLASTIC_SAMPLE \
   vec4 base = XMB_SAMPLE(uv); \
   float right = max(0.0, base.a - XMB_SAMPLE(uv + vec2(0.010, 0.0)).a); \
   float left = max(0.0, base.a - XMB_SAMPLE(uv - vec2(0.020, 0.0)).a); \
   float top = max(0.0, base.a - XMB_SAMPLE(uv - vec2(0.0, 0.015)).a); \
   float bottom = max(0.0, base.a - XMB_SAMPLE(uv + vec2(0.0, 0.025)).a); \
   vec2 tangent = vec2(0.035, -0.035); \
   float curve = max(0.0, base.a - max(XMB_SAMPLE(uv + tangent).a, \
         XMB_SAMPLE(uv - tangent).a));

#define XMB_PLASTIC_LIGHT \
   vec2 edge = max(vec2(left - right, top - bottom), vec2(0.0, 0.0)); \
   float facing = 2.0 * edge.x * edge.y / max(dot(edge, edge), 0.001); \
   float glint = smoothstep(0.0, 0.45, curve) * pow(facing, 8.0); \
   float reflection = min(1.0, 0.30 * right + 0.10 * left \
         + 0.12 * glint + 0.85 * glint * glint); \
   vec3 body = base.rgb * tint.rgb * (1.0 - 0.24 * top - 0.07 * bottom);

#define XMB_PLASTIC_COLOR \
   vec4 plastic = vec4(mix(body, vec3(1.0, 1.0, 1.0), \
         reflection), base.a * tint.a * min(1.0, 0.60 + reflection));

#define XMB_FONT_SAMPLE \
   float coverage = XMB_COVERAGE(uv); \
   vec2 bevel = abs(dFdx(uv)) + abs(dFdy(uv)); \
   float right = max(0.0, coverage - XMB_COVERAGE(uv + vec2(bevel.x * 0.55, 0.0))); \
   float left = max(0.0, coverage - XMB_COVERAGE(uv - vec2(bevel.x * 0.85, 0.0))); \
   float top = max(0.0, coverage - XMB_COVERAGE(uv - vec2(0.0, bevel.y * 0.65))); \
   float bottom = max(0.0, coverage - XMB_COVERAGE(uv + vec2(0.0, bevel.y)));

#define XMB_FONT_LIGHT \
   vec2 tangent = bevel * vec2(1.0, -1.0); \
   float curve = max(0.0, coverage - max(XMB_COVERAGE(uv + tangent), \
         XMB_COVERAGE(uv - tangent))); \
   float glint = curve * min(left, top); \
   float light = 0.12 * right + 0.04 * left + 0.08 * glint; \
   float shade = 0.06 * top + 0.02 * bottom; \
   float brightness = clamp(max(tint.r, max(tint.g, tint.b)), 0.0, 1.0);

#define XMB_FONT_COLOR \
   vec4 plastic = vec4(tint.rgb, coverage * tint.a * \
         mix(0.22, min(1.0, 0.88 + light - shade), brightness));

#endif
