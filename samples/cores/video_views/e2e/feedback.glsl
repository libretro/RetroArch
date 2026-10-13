/* Its input, or what it drew the frame before if brighter: on a still
 * frame its input, unless the feedback is another view's */
#if defined(VERTEX)
#if __VERSION__ >= 130
#define COMPAT_VARYING out
#define COMPAT_ATTRIBUTE in
#else
#define COMPAT_VARYING varying
#define COMPAT_ATTRIBUTE attribute
#endif
COMPAT_ATTRIBUTE vec4 VertexCoord;
COMPAT_ATTRIBUTE vec4 TexCoord;
COMPAT_ATTRIBUTE vec4 FeedbackTexCoord;
COMPAT_VARYING vec4 TEX0;
COMPAT_VARYING vec4 TEX1;
uniform mat4 MVPMatrix;
void main()
{
   gl_Position = MVPMatrix * VertexCoord;
   TEX0.xy     = TexCoord.xy;
   TEX1.xy     = FeedbackTexCoord.xy;
}
#elif defined(FRAGMENT)
#ifdef GL_ES
precision mediump float;
#endif
#if __VERSION__ >= 130
#define COMPAT_VARYING in
#define COMPAT_TEXTURE texture
out vec4 FragColor;
#else
#define COMPAT_VARYING varying
#define FragColor gl_FragColor
#define COMPAT_TEXTURE texture2D
#endif
uniform sampler2D Texture;
uniform sampler2D FeedbackTexture;
COMPAT_VARYING vec4 TEX0;
COMPAT_VARYING vec4 TEX1;
void main()
{
   FragColor = max(COMPAT_TEXTURE(Texture, TEX0.xy),
         COMPAT_TEXTURE(FeedbackTexture, TEX1.xy));
}
#endif
