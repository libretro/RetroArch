#version 460
/* The quad effects' vertices: the full-screen mesh's clip-space
 * positions, as gfx_display_mesh_fullscreen() keeps them */
layout(location = 0) in vec2 Position;
void main()
{
   gl_Position = vec4(Position, 0.0, 1.0);
}
