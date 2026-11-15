#version 150
layout(location = 0) in vec4 Position;
layout(location = 1) in vec4 Lighting;
layout(location = 0) out vec4 vEC;
void main()
{
   gl_Position = Position;
   vEC = Lighting;
}
