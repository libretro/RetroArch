/* The GL 1.1 entry points gfx/drivers/gl2.c and gl3.c link directly,
 * for the harnesses that include a GL driver against a scripted GL.
 * Windows links them from opengl32 through import pointers, so there
 * the header supplies the pointers, aimed at its own functions.
 *
 * Texture names are handed out from gl11_next_name; glFinish only
 * counts, in gl11_finishes, for a harness that must never see one, and
 * glTexParameteri in gl11_tex_params.
 * Included once, by the test, after the driver source. */

#ifndef GL11_FAKES_H
#define GL11_FAKES_H

static GLuint   gl11_next_name = 1;
static unsigned gl11_finishes;
static unsigned gl11_draws;
static unsigned gl11_tex_params;
static GLuint gl11_bound_texture;
static GLint gl11_texture_params[1024][4];
static void (*gl11_copy_texture)(GLuint, GLsizei, GLsizei);
static void (*gl11_delete_texture)(GLuint);
static void (*gl11_texture_info)(GLuint, GLint, GLenum, GLint*);
static void (*gl11_allocate_texture)(GLuint, GLint, GLsizei, GLsizei);
static void (*gl11_upload_texture)(GLuint, const void*);
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
GL_FAKE(void, glBindTexture, (GLenum t, GLuint i)) { (void)t; gl11_bound_texture = i; }
GL_FAKE(void, glBlendFunc, (GLenum a, GLenum b)) { (void)a; (void)b; }
GL_FAKE(void, glClear, (GLbitfield m)) { (void)m; }
GL_FAKE(void, glColorMask, (GLboolean r, GLboolean g, GLboolean b, GLboolean a))
{ (void)r; (void)g; (void)b; (void)a; }
GL_FAKE(void, glClearColor, (GLclampf r, GLclampf g, GLclampf b, GLclampf a))
{ (void)r; (void)g; (void)b; (void)a; }
GL_FAKE(void, glCopyTexSubImage2D, (GLenum a, GLint b, GLint c, GLint d,
      GLint e, GLint f, GLsizei g, GLsizei h))
{ (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
  if (gl11_copy_texture) gl11_copy_texture(gl11_bound_texture, g, h); }
GL_FAKE(void, glDeleteTextures, (GLsizei n, const GLuint *t))
{ GLsizei i; for(i=0;i<n;i++) if(gl11_delete_texture)gl11_delete_texture(t[i]); }
GL_FAKE(void, glDisable, (GLenum c)) { (void)c; }
GL_FAKE(void, glDrawArrays, (GLenum m, GLint f, GLsizei c)) { (void)m; (void)f; (void)c; gl11_draws++; }
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
GL_FAKE(void, glGetFloatv, (GLenum p, GLfloat *v))
{ (void)p; memset(v, 0, 4 * sizeof(*v)); }
GL_FAKE(void, glGetTexParameteriv, (GLenum t, GLenum p, GLint *v))
{ unsigned at = p == GL_TEXTURE_MIN_FILTER ? 0 : p == GL_TEXTURE_MAG_FILTER ? 1 : p == GL_TEXTURE_WRAP_S ? 2 : 3;
  (void)t; *v = gl11_bound_texture < 1024 ? gl11_texture_params[gl11_bound_texture][at] : 0;
  if (!*v) *v = at < 2 ? GL_LINEAR : GL_CLAMP_TO_EDGE; }
GL_FAKE(void, glGetIntegerv, (GLenum p, GLint *v))
{ *v = (p == GL_MAX_TEXTURE_SIZE) ? 4096 : p == GL_TEXTURE_BINDING_2D ? (GLint)gl11_bound_texture : 0; }
GL_FAKE(const GLubyte *, glGetString, (GLenum n)) { (void)n; return (const GLubyte*)""; }
GL_FAKE(GLboolean, glIsEnabled, (GLenum c)) { (void)c; return GL_FALSE; }
GL_FAKE(void, glScissor, (GLint a, GLint b, GLsizei c, GLsizei d))
{ (void)a; (void)b; (void)c; (void)d; }
GL_FAKE(void, glTexImage2D, (GLenum a, GLint b, GLint c, GLsizei d, GLsizei e,
      GLint f, GLenum g, GLenum h, const GLvoid *i))
{ (void)a; (void)b; (void)f; (void)g; (void)h; (void)i;
  if(gl11_allocate_texture)gl11_allocate_texture(gl11_bound_texture,c,d,e); }
GL_FAKE(void, glTexParameteri, (GLenum a, GLenum b, GLint c))
{ unsigned at = b == GL_TEXTURE_MIN_FILTER ? 0 : b == GL_TEXTURE_MAG_FILTER ? 1 : b == GL_TEXTURE_WRAP_S ? 2 : 3;
  (void)a; if(gl11_bound_texture < 1024)gl11_texture_params[gl11_bound_texture][at]=c;gl11_tex_params++; }
GL_FAKE(void, glTexSubImage2D, (GLenum a, GLint b, GLint c, GLint d, GLsizei e,
      GLsizei f, GLenum g, GLenum h, const GLvoid *i))
{ (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h;
  if(gl11_upload_texture)gl11_upload_texture(gl11_bound_texture,i); }
GL_FAKE(void, glViewport, (GLint a, GLint b, GLsizei c, GLsizei d))
{ (void)a; (void)b; (void)c; (void)d; }
GL_FAKE(void, glGetTexLevelParameteriv, (GLenum a, GLint b, GLenum c, GLint *d))
{ (void)a;
  if(gl11_texture_info){gl11_texture_info(gl11_bound_texture,b,c,d);return;}
  (void)b;
#if defined(GL_TEXTURE_INTERNAL_FORMAT) && defined(GL_RGBA8)
  *d = c == GL_TEXTURE_INTERNAL_FORMAT ? GL_RGBA8 : 0;
#else
  (void)c; *d = 0;
#endif
}

#ifdef _WIN32
/* The Windows build's context fallback, not reached by the chains. */
void video_driver_force_fallback(const char *driver) { (void)driver; }
#endif

#endif
