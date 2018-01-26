#version 310 es

precision mediump float;

uniform sampler2D tex;

layout(location = 0) out vec4 FragColor;

void main()
{
	FragColor = textureLod(tex, vec2(0.4, 0.6), 0.0);
}
