/* The GL 1.1 entry points gfx/drivers/gl2.c and gl3.c link directly,
 * for the harnesses that include a GL driver against a scripted GL.
 * Windows links them from opengl32 through import pointers, so there
 * the header supplies the pointers, aimed at its own functions.
 *
 * Texture names are handed out from gl11_next_name; glFinish only
 * counts, in gl11_finishes, for a harness that must never see one.
 * Included once, by the test, after the driver source. */

#ifndef GL11_FAKES_H
#define GL11_FAKES_H

static GLuint   gl11_next_name = 1;
static unsigned gl11_finishes;
static void (*gl11_read_pixels)(GLsizei width, GLsizei height);

#ifdef _WIN32
#define GL_FAKE(ret, name, params) \
   static ret APIENTRY fake_##name params; \
   ret (APIENTRY *__imp_##name) params = fake_##name; \
   static ret APIENTRY fake_##name params
#else
#define GL_FAKE(ret, name, params) ret name params
#endif

GL_FAKE(void, glPixelStorei, (GLenum pname, GLint param)) { (void)pname; (void)param; }
GL_FAKE(void, glReadBuffer, (GLenum mode)) { (void)mode; }
GL_FAKE(void, glReadPixels, (GLint x, GLint y, GLsizei width, GLsizei height,
      GLenum format, GLenum type, GLvoid *data))
{ (void)x; (void)y; (void)format; (void)type; (void)data;
  if (gl11_read_pixels) gl11_read_pixels(width, height); }
GL_FAKE(void, glBindTexture, (GLenum t, GLuint i)) { (void)t; (void)i; }
GL_FAKE(void, glBlendFunc, (GLenum a, GLenum b)) { (void)a; (void)b; }
GL_FAKE(void, glClear, (GLbitfield m)) { (void)m; }
GL_FAKE(void, glColorMask, (GLboolean r, GLboolean g, GLboolean b, GLboolean a))
{ (void)r; (void)g; (void)b; (void)a; }
GL_FAKE(void, glClearColor, (GLclampf r, GLclampf g, GLclampf b, GLclampf a))
{ (void)r; (void)g; (void)b; (void)a; }
GL_FAKE(void, glCopyTexSubImage2D, (GLenum a, GLint b, GLint c, GLint d,
      GLint e, GLint f, GLsizei g, GLsizei h))
{ (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h; }
GL_FAKE(void, glDeleteTextures, (GLsizei n, const GLuint *t)) { (void)n; (void)t; }
GL_FAKE(void, glDisable, (GLenum c)) { (void)c; }
GL_FAKE(void, glDrawArrays, (GLenum m, GLint f, GLsizei c)) { (void)m; (void)f; (void)c; }
GL_FAKE(void, glEnable, (GLenum c)) { (void)c; }
GL_FAKE(void, glFinish, (void)) { gl11_finishes++; }
GL_FAKE(void, glFlush, (void)) { }
GL_FAKE(void, glGenTextures, (GLsizei n, GLuint *t))
{
   GLsizei i;
   for (i = 0; i < n; i++)
      t[i] = gl11_next_name++;
}
GL_FAKE(GLenum, glGetError, (void)) { return GL_NO_ERROR; }
GL_FAKE(void, glGetIntegerv, (GLenum p, GLint *v))
{ *v = (p == GL_MAX_TEXTURE_SIZE) ? 4096 : 0; }
GL_FAKE(const GLubyte *, glGetString, (GLenum n)) { (void)n; return (const GLubyte*)""; }
GL_FAKE(GLboolean, glIsEnabled, (GLenum c)) { (void)c; return GL_FALSE; }
GL_FAKE(void, glScissor, (GLint a, GLint b, GLsizei c, GLsizei d))
{ (void)a; (void)b; (void)c; (void)d; }
GL_FAKE(void, glTexImage2D, (GLenum a, GLint b, GLint c, GLsizei d, GLsizei e,
      GLint f, GLenum g, GLenum h, const GLvoid *i))
{ (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h; (void)i; }
GL_FAKE(void, glTexParameteri, (GLenum a, GLenum b, GLint c)) { (void)a; (void)b; (void)c; }
GL_FAKE(void, glTexSubImage2D, (GLenum a, GLint b, GLint c, GLint d, GLsizei e,
      GLsizei f, GLenum g, GLenum h, const GLvoid *i))
{ (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h; (void)i; }
GL_FAKE(void, glViewport, (GLint a, GLint b, GLsizei c, GLsizei d))
{ (void)a; (void)b; (void)c; (void)d; }
GL_FAKE(void, glGetTexLevelParameteriv, (GLenum a, GLint b, GLenum c, GLint *d))
{ (void)a; (void)b; (void)c; *d = 0; }

#ifdef _WIN32
/* The Windows build's context fallback, not reached by the chains. */
void video_driver_force_fallback(const char *driver) { (void)driver; }
#endif

#endif
