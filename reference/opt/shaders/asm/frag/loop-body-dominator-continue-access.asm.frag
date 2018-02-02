#version 450

layout(binding = 0, std140) uniform Foo
{
    layout(row_major) mat4 lightVP[64];
    uint shadowCascadesNum;
    int test;
} _11;

layout(location = 0) in vec3 fragWorld;
layout(location = 0) out int _entryPointOutput;

mat4 _152;
uint _155;

int GetCascade(vec3 fragWorldPosition)
{
    mat4 _153;
    _153 = _152;
    uint _156;
    mat4 _157;
    for (uint _151 = 0u; _151 < _11.shadowCascadesNum; _151 = _156 + uint(1), _153 = _157)
    {
        mat4 _154;
        _154 = _153;
        for (;;)
        {
            if (_11.test == 0)
            {
                _156 = _151;
                _157 = mat4(vec4(0.5, 0.0, 0.0, 0.0), vec4(0.0, 0.5, 0.0, 0.0), vec4(0.0, 0.0, 0.5, 0.0), vec4(0.0, 0.0, 0.0, 1.0));
                break;
            }
            _156 = _151;
            _157 = mat4(vec4(1.0, 0.0, 0.0, 0.0), vec4(0.0, 1.0, 0.0, 0.0), vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0));
            break;
        }
        vec4 _92 = (_157 * _11.lightVP[_156]) * vec4(fragWorldPosition, 1.0);
        if ((((_92.z >= 0.0) && (_92.z <= 1.0)) && (max(_92.x, _92.y) <= 1.0)) && (min(_92.x, _92.y) >= 0.0))
        {
            return int(_156);
        }
    }
    return -1;
}

void main()
{
    vec3 _123 = fragWorld;
    _entryPointOutput = GetCascade(_123);
}

