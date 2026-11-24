"#version 310 es\nprecision highp float;layout(location=0)in vec2 vTexCoord;layout(location=1)in vec4 vColor;layout(locati"
"on=0)out vec4 FragColor;layout(set=0,binding=1)uniform highp sampler2D uTex;void main(){FragColor=vColor*texture(uTex,vT"
"exCoord);}"
