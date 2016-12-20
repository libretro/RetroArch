#version 310 es
#extension GL_EXT_geometry_shader : require
layout(points) in;
layout(max_vertices = 3, points) out;

out vec3 vNormal;
in VertexData
{
    vec3 normal;
} vin[1];


void main()
{
    gl_Position = gl_in[0].gl_Position;
    vNormal = vin[0].normal;
    EmitVertex();
    gl_Position = gl_in[0].gl_Position;
    vNormal = vin[0].normal;
    EmitVertex();
    gl_Position = gl_in[0].gl_Position;
    vNormal = vin[0].normal;
    EmitVertex();
    EndPrimitive();
}

