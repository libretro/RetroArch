precision mediump float;

varying vec2 texCoord;
uniform sampler2D texUnit;

void main()
{
    gl_FragColor = texture2D(texUnit, texCoord);
}
