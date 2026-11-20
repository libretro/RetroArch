"#version 310 es\nprecision highp float;layout(location=0)in vec2 vTexCoord;layout(location=0)out vec4 FragColor;layout(se"
"t=0,binding=2)uniform highp sampler2D Source;void main(){FragColor=vec4(texture(Source,vTexCoord).rgb,1.0);}"
