#if __VERSION__ >= 130
#define COMPAT_VARYING out
#define COMPAT_ATTRIBUTE in
#define COMPAT_TEXTURE texture
#else
#define COMPAT_VARYING varying
#define COMPAT_ATTRIBUTE attribute
#define COMPAT_TEXTURE texture2D
#endif
#if defined(VERTEX)
COMPAT_ATTRIBUTE vec4 VertexCoord;
COMPAT_ATTRIBUTE vec4 TexCoord;
COMPAT_VARYING vec4 TEX0;
uniform mat4 MVPMatrix;
void main()
{
   gl_Position = MVPMatrix * VertexCoord;
   TEX0 = TexCoord;
}
#elif defined(FRAGMENT)
#if __VERSION__ >= 130
#undef COMPAT_VARYING
#define COMPAT_VARYING in
out vec4 FragColor;
#else
#define FragColor gl_FragColor
#endif
uniform sampler2D Texture;
uniform sampler2D PassFeedback0;
COMPAT_VARYING vec4 TEX0;
void main()
{
   vec3 a = COMPAT_TEXTURE(Texture, TEX0.xy).rgb;
   vec3 f = COMPAT_TEXTURE(PassFeedback0, TEX0.xy).rgb;
   FragColor = vec4(0.5 * a + 0.5 * f, 1.0);
}
#endif
