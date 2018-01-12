#version 150

uniform UBO
{
   mat4 MVP;
   vec2 OutputSize;
   float time;
} global;

layout(location = 0) out vec4 FragColor;
void main()
{
   FragColor = vec4(0.05, 0.05, 0.05, 1.0);
}
