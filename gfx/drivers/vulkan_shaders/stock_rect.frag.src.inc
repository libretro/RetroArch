"#version 310 es\nprecision highp float;layout(location=0)in vec2 vTexCoord;layout(location=0)out vec4 FragColor;layout(se"
"t=0,binding=1)uniform highp sampler2D uTex;layout(std140,set=0,binding=0)uniform UBO{mat4 MVP;vec4 Clamp;}global;void ma"
"in(){FragColor=vec4(texture(uTex,clamp(vTexCoord,global.Clamp.xy,global.Clamp.zw)).rgb,1.0);}"
