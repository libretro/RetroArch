#version 450

layout(binding = 0) uniform sampler2D uTextures[2 * 3 * 1];

layout(location = 1) in vec2 vUV;
layout(location = 0) out vec4 FragColor;
layout(location = 0) flat in int vIndex;

int _93;

void main()
{
    vec4 values3[2 * 3 * 1];
    int _96;
    int _97;
    int _94;
    int _95;
    for (int _92 = 0; _92 < 2; _92++, _94 = _96, _95 = _97)
    {
        _96 = 0;
        _97 = _95;
        int _98;
        for (; _96 < 3; _96++, _97 = _98)
        {
            _98 = 0;
            for (; _98 < 1; _98++)
            {
                values3[_92 * 3 * 1 + _96 * 1 + _98] = texture(uTextures[_92 * 3 * 1 + _96 * 1 + _98], vUV);
            }
        }
    }
    FragColor = ((values3[1 * 3 * 1 + 2 * 1 + 0]) + (values3[0 * 3 * 1 + 2 * 1 + 0])) + (values3[(vIndex + 1) * 3 * 1 + 2 * 1 + vIndex]);
}

