"#version 310 es\nlayout(location=0)in vec4 Position;layout(location=1)in vec2 TexCoord;layout(location=0)out vec2 vTexCoo"
"rd;layout(set=0,binding=0,std140)uniform UBO{mat4 MVP;vec4 hdr_params;vec4 hdr_out;}global;void main(){gl_Position=globa"
"l.MVP*Position;vTexCoord=TexCoord;}"
