attribute vec2 pos;
attribute vec2 tex;

varying vec2 texCoord;

void main()
{
    gl_Position.x = pos.x;
    gl_Position.y = pos.y;
    gl_Position.z = 0.0;
    gl_Position.w = 1.0;
    
    texCoord = tex;
}
