#version 450
layout(vertices = 3) out;

struct VertexOutput
{
    vec4 pos;
    vec2 uv;
};

struct HSOut
{
    vec4 pos;
    vec2 uv;
};

struct HSConstantOut
{
    float EdgeTess[3];
    float InsideTess;
};

struct VertexOutput_1
{
    vec2 uv;
};

struct HSOut_1
{
    vec2 uv;
};

layout(location = 0) in VertexOutput_1 p[];
layout(location = 0) out HSOut_1 _entryPointOutput[3];

void main()
{
    VertexOutput p_1[3];
    p_1[0].pos = gl_in[0].gl_Position;
    p_1[0].uv = p[0].uv;
    p_1[1].pos = gl_in[1].gl_Position;
    p_1[1].uv = p[1].uv;
    p_1[2].pos = gl_in[2].gl_Position;
    p_1[2].uv = p[2].uv;
    VertexOutput param[3] = p_1;
    HSOut _158;
    HSOut _197 = _158;
    _197.pos = param[gl_InvocationID].pos;
    HSOut _199 = _197;
    _199.uv = param[gl_InvocationID].uv;
    _158 = _199;
    gl_out[gl_InvocationID].gl_Position = param[gl_InvocationID].pos;
    _entryPointOutput[gl_InvocationID].uv = param[gl_InvocationID].uv;
    barrier();
    if (int(gl_InvocationID) == 0)
    {
        VertexOutput param_1[3] = p_1;
        vec2 _174 = vec2(1.0) + param_1[0].uv;
        float _175 = _174.x;
        HSConstantOut _169;
        HSConstantOut _205 = _169;
        _205.EdgeTess[0] = _175;
        vec2 _180 = vec2(1.0) + param_1[0].uv;
        float _181 = _180.x;
        HSConstantOut _207 = _205;
        _207.EdgeTess[1] = _181;
        vec2 _186 = vec2(1.0) + param_1[0].uv;
        float _187 = _186.x;
        HSConstantOut _209 = _207;
        _209.EdgeTess[2] = _187;
        vec2 _192 = vec2(1.0) + param_1[0].uv;
        float _193 = _192.x;
        HSConstantOut _211 = _209;
        _211.InsideTess = _193;
        _169 = _211;
        gl_TessLevelOuter[0] = _175;
        gl_TessLevelOuter[1] = _181;
        gl_TessLevelOuter[2] = _187;
        gl_TessLevelInner[0] = _193;
    }
}

